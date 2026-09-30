#include <SPI.h>
#include <hiduniversal.h>

#define SERIAL_BAUD     115200
#define DESC_MAX        512
#define MAX_REPORT_LEN  64

// 1 = print every report, even ones identical to the previous one. Sticks that
// report continuously will swamp the serial link, hence the default.
#define SEND_UNCHANGED  0

#define AUDIT_AXES      3
#define AUDIT_CHANGES   50
#define AUDIT_SETTLE_MS 2000

static const char *AUDIT_PROMPT[AUDIT_AXES] = { "pitch", "aileron", "rudder" };

// 16-bit input fields on the generic desktop page with an axis usage, as
// declared by the report descriptor
#define MAX_FIELDS 8

struct AxisField {
  uint8_t  reportId;
  uint8_t  offset;
  uint16_t max;
  uint8_t  usage;
};

static AxisField fields[MAX_FIELDS];
static uint8_t   fieldCount = 0;

static uint16_t fieldLast[MAX_FIELDS];
static uint16_t fieldChanges[MAX_FIELDS];
static bool     fieldSeen[MAX_FIELDS];
static bool     auditing      = false;
static uint8_t  auditStep     = 0;
static uint8_t  leader        = 0;
static uint16_t leaderChanges = 0;
static uint32_t settledSince  = 0;
static uint8_t  auditField[AUDIT_AXES];
static uint8_t  auditLen     = 0;

static void auditStart();
static void auditFeed(uint8_t len, const uint8_t *buf);
static void auditPoll();

// Walks the report descriptor as it streams in from the control transfer and
// records every 16-bit axis input field it declares.
class AxisFinder : public USBReadParser {
  public:
    void Parse(const uint16_t len, const uint8_t *pbuf, const uint16_t &offset) override;

  private:
    uint8_t  item[5];
    uint8_t  itemLen    = 0;
    uint8_t  itemNeed   = 0;
    uint8_t  usagePage  = 0;
    uint8_t  reportSize = 0;
    uint16_t reportCount = 0;
    int32_t  logMax     = 0;
    uint8_t  reportId   = 0;
    uint16_t bitCursor  = 0;
    uint8_t  usages[MAX_FIELDS];
    uint8_t  usageCount = 0;

    void handle();
};

void AxisFinder::Parse(const uint16_t len, const uint8_t *pbuf, const uint16_t &offset) {
  (void)offset;

  for (uint16_t i = 0; i < len; i++) {
    uint8_t b = pbuf[i];

    if (itemLen == 0) {
      uint8_t size = b & 0x03;
      if (size == 3) size = 4;

      itemNeed = 1 + size;
      item[0]  = b;
      itemLen  = 1;
    } else {
      item[itemLen++] = b;
    }

    if (itemLen == itemNeed) {
      handle();
      itemLen = 0;
    }
  }
}

void AxisFinder::handle() {
  uint8_t tag  = item[0] >> 4;
  uint8_t type = (item[0] >> 2) & 0x03;
  uint8_t size = itemNeed - 1;

  int32_t data = 0;
  for (uint8_t i = size; i > 0; i--)
    data = (data << 8) | item[i];

  if (size == 1 && (data & 0x80))     data |= 0xFFFFFF00;
  if (size == 2 && (data & 0x8000))   data |= 0xFFFF0000;

  if (type == 1) {
    switch (tag) {
      case 0x0: usagePage   = (uint8_t)data;  break;
      case 0x2: logMax      = data;           break;
      case 0x7: reportSize  = (uint8_t)data;  break;
      case 0x8: reportId    = (uint8_t)data;  bitCursor = 8; break;
      case 0x9: reportCount = (uint16_t)data; break;
    }
    return;
  }

  if (type == 2) {
    if (tag == 0x0 && usageCount < MAX_FIELDS)
      usages[usageCount++] = (uint8_t)data;
    return;
  }

  if (tag == 0x8) {
    bool constant = data & 0x01;

    for (uint16_t f = 0; f < reportCount; f++) {
      uint8_t usage = f < usageCount ? usages[f] : (usageCount ? usages[usageCount - 1] : 0);

      if (!constant && reportSize == 16 && usagePage == 0x01 &&
          usage >= 0x30 && usage <= 0x37 && fieldCount < MAX_FIELDS) {
        fields[fieldCount].reportId = reportId;
        fields[fieldCount].offset   = (bitCursor + f * 16) / 8;
        fields[fieldCount].max      = (uint16_t)logMax;
        fields[fieldCount].usage    = usage;
        fieldCount++;
      }
    }

    bitCursor += reportSize * reportCount;
  }

  usageCount = 0;
}

// Prints the descriptor as the body of a C array, 16 bytes per line.
class CArrayPrinter : public USBReadParser {
  public:
    void Parse(const uint16_t len, const uint8_t *pbuf, const uint16_t &offset) override {
      for (uint16_t i = 0; i < len; i++) {
        uint16_t n = offset + i;

        if (n % 16 == 0) Serial.print(F("\n  "));

        Serial.print(F("0x"));
        if (pbuf[i] < 0x10) Serial.print('0');
        Serial.print(pbuf[i], HEX);
        Serial.print(',');
      }
    }
};

class HIDDumper : public HIDUniversal {
  public:
    HIDDumper(USB *usb) : HIDUniversal(usb) {}
    void printProfileArray();

  protected:
    uint8_t OnInitSuccessful() override;

  private:
    void printIdentity();
    void printReportDescriptor();
    void printString(uint8_t index);
};

static void printHex16(uint16_t v) {
  for (int8_t shift = 12; shift >= 0; shift -= 4)
    Serial.print((uint8_t)((v >> shift) & 0x0F), HEX);
}

void HIDDumper::printString(uint8_t index) {
  uint8_t buf[64];

  if (index == 0) {
    Serial.println(F("(none)"));
    return;
  }

  uint8_t rcode = pUsb->getStrDescr(bAddress, 0, sizeof(buf), index, 0x0409, buf);

  if (rcode) {
    Serial.print(F("(error 0x"));
    Serial.print(rcode, HEX);
    Serial.println(')');
    return;
  }

  uint8_t len = buf[0];
  if (len > sizeof(buf))
    len = sizeof(buf);

  // UTF-16LE; printing the low byte only, so anything above 0x7F comes out
  // as a raw byte the terminal will mangle
  for (uint8_t i = 2; i + 1 < len; i += 2)
    Serial.write(buf[i]);

  Serial.println();
}

void HIDDumper::printIdentity() {
  USB_DEVICE_DESCRIPTOR d;

  Serial.println();

  if (pUsb->getDevDescr(bAddress, 0, sizeof(d), (uint8_t *)&d)) {
    Serial.println(F("Could not read device descriptor!"));
    return;
  }

  Serial.print(F("idVendor      0x"));
  printHex16(d.idVendor);
  Serial.println();

  Serial.print(F("idProduct     0x"));
  printHex16(d.idProduct);
  Serial.println();

  Serial.print(F("bcdDevice     0x"));
  printHex16(d.bcdDevice);
  Serial.println();

  Serial.print(F("iManufacturer "));
  printString(d.iManufacturer);
  Serial.print(F("iProduct      "));
  printString(d.iProduct);
  Serial.print(F("iSerialNumber "));
  printString(d.iSerialNumber);
}

void HIDDumper::printReportDescriptor() {
  AxisFinder finder;
  uint8_t    buf[64];

  fieldCount = 0;

  pUsb->ctrlReq(bAddress, 0, bmREQ_HID_REPORT,
                USB_REQUEST_GET_DESCRIPTOR, 0x00,
                HID_DESCRIPTOR_REPORT, 0, DESC_MAX,
                sizeof(buf), buf, (USBReadParser *)&finder);
}

void HIDDumper::printProfileArray() {
  CArrayPrinter printer;
  uint8_t       buf[64];
  Serial.print(F("static const uint8_t JoystickReportDescriptor[] PROGMEM = {"));
  pUsb->ctrlReq(bAddress, 0, bmREQ_HID_REPORT,
                USB_REQUEST_GET_DESCRIPTOR, 0x00,
                HID_DESCRIPTOR_REPORT, 0, DESC_MAX,
                sizeof(buf), buf, (USBReadParser *)&printer);
  Serial.println();
  Serial.println(F("};"));
}

uint8_t HIDDumper::OnInitSuccessful() {
  printIdentity();
  printReportDescriptor();
  auditStart();
  return 0;
}

class ReportDumper : public HIDReportParser {
  public:
    void Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf) override;
    void reset() { lastLen = 0; }

  private:
    uint8_t lastLen = 0;
    uint8_t last[MAX_REPORT_LEN];
};

void ReportDumper::Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf) {
  (void)hid;
  (void)is_rpt_id;

  if (len > MAX_REPORT_LEN)
    len = MAX_REPORT_LEN;

#if !SEND_UNCHANGED
  if (len == lastLen && memcmp(last, buf, len) == 0)
    return;
#endif

  if (auditing) {
    memcpy(last, buf, len);
    lastLen = len;
    auditFeed(len, buf);
    return;
  }

  if (lastLen == 0) {
    Serial.print(F("# report length "));
    Serial.print(len);
    Serial.println(F(" bytes"));
  }

  memcpy(last, buf, len);
  lastLen = len;

  for (uint8_t i = 0; i < len; i++) {
    if (buf[i] < 0x10)
      Serial.write('0');
    Serial.print(buf[i], HEX);
    Serial.write(i + 1 < len ? ' ' : '\n');
  }
}

extern HIDDumper Hid;

static void auditPrompt() {
  Serial.println();
  Serial.print(F("# move the "));
  Serial.print(AUDIT_PROMPT[auditStep]);
  Serial.println(F(" control through its full travel, then let go..."));
}

static void auditReset() {
  for (uint8_t i = 0; i < MAX_FIELDS; i++) {
    fieldChanges[i] = 0;
    fieldSeen[i]    = false;
  }

  leaderChanges = 0;
  leader        = 0;
  settledSince  = millis();
}

static void auditStart() {
  if (fieldCount == 0) {
    Serial.println(F("# the descriptor declares no 16-bit axes, skipping the audit"));
    return;
  }

  auditing  = true;
  auditStep = 0;
  auditReset();
  auditPrompt();
}

static void auditFeed(uint8_t len, const uint8_t *buf) {
  for (uint8_t i = 0; i < fieldCount; i++) {
    if (fields[i].reportId != buf[0]) continue;
    if (fields[i].offset + 1 >= len)  continue;

    uint16_t v = (uint16_t)buf[fields[i].offset] | ((uint16_t)buf[fields[i].offset + 1] << 8);

    if (fieldSeen[i] && v != fieldLast[i]) fieldChanges[i]++;

    fieldLast[i] = v;
    fieldSeen[i] = true;
    auditLen     = len;
  }

  uint16_t best    = 0;
  uint8_t  bestIdx = 0;

  for (uint8_t i = 0; i < fieldCount; i++) {
    if (fieldChanges[i] > best) {
      best    = fieldChanges[i];
      bestIdx = i;
    }
  }

  if (best != leaderChanges || bestIdx != leader) {
    leaderChanges = best;
    leader        = bestIdx;
    settledSince  = millis();
  }
}

static void auditPrintValues() {
  const AxisField &pitch   = fields[auditField[0]];
  const AxisField &aileron = fields[auditField[1]];
  const AxisField &rudder  = fields[auditField[2]];

  Serial.println();
  Serial.println(F("// ---- stick-trim-box/joystick-profile.ino: replace the array ----"));
  Serial.println();
  Hid.printProfileArray();
  Serial.println();
  Serial.println(F("// ---- stick-trim-box/usb-host.ino: replace these lines ----"));
  Serial.println();
  Serial.print(F("#define REPORT_ID  0x"));
  if (pitch.reportId < 0x10) Serial.print('0');
  Serial.println(pitch.reportId, HEX);
  Serial.print(F("#define REPORT_LEN "));
  Serial.println(auditLen);
  Serial.print(F("#define OFF_X   "));
  Serial.println(aileron.offset);
  Serial.print(F("#define OFF_Y   "));
  Serial.println(pitch.offset);
  Serial.print(F("#define OFF_RZ  "));
  Serial.println(rudder.offset);
  Serial.println();
  Serial.println(F("// ---- stick-trim-box/trim-control.ino: replace these lines ----"));
  Serial.println();
  Serial.print(F("static const int16_t TRIM_MAX[AXIS_COUNT]    = { "));
  Serial.print(pitch.max);   Serial.print(F(", "));
  Serial.print(aileron.max); Serial.print(F(", "));
  Serial.print(rudder.max);  Serial.println(F(" };"));

  Serial.print(F("static const int16_t TRIM_CENTRE[AXIS_COUNT] = { "));
  Serial.print(pitch.max / 2);   Serial.print(F(", "));
  Serial.print(aileron.max / 2); Serial.print(F(", "));
  Serial.print(rudder.max / 2);  Serial.println(F(" };"));
}

static void auditPoll() {
  if (!auditing) return;
  if (leaderChanges < AUDIT_CHANGES) return;
  if (millis() - settledSince < AUDIT_SETTLE_MS) return;

  auditField[auditStep] = leader;

  Serial.print(F("  "));
  Serial.print(AUDIT_PROMPT[auditStep]);
  Serial.print(F(" is at offset "));
  Serial.print(fields[leader].offset);
  Serial.print(F(", "));
  Serial.print(leaderChanges);
  Serial.println(F(" changes"));

  auditStep++;

  if (auditStep < AUDIT_AXES) {
    auditReset();
    auditPrompt();
    return;
  }

  auditing = false;
  auditPrintValues();
}


USB          Usb;
HIDDumper    Hid(&Usb);
ReportDumper Parser;

static uint8_t lastAddress = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);

  // block until the serial monitor attaches, otherwise everything setup() and
  // OnInitSuccessful() print happens before anything is listening
  while (!Serial)
    ;

  while (Usb.Init() != 0) {
    Serial.println(F("# shield did not start - retrying"));
    delay(1000);
  }

  delay(200);

  if (!Hid.SetReportParser(0, &Parser))
    Serial.println(F("# could not install the report parser"));

  Serial.println(F("# ready - plug in a joystick"));
}

void loop() {
  Usb.Task();
  auditPoll();

  uint8_t addr = Hid.GetAddress();

  if (addr != lastAddress) {
    lastAddress = addr;
    Parser.reset();

    if (!addr)
      Serial.println(F("# joystick disconnected"));
  }
}
