#include <SPI.h>
#include <hiduniversal.h>
#include <PluggableUSB.h>

#define REPORT_ID  0x01
#define REPORT_LEN 64
#define OFF_X   1
#define OFF_Y   3
#define OFF_RZ  5


// Our "fake HID" code

#define HID_REPORT_DESCRIPTOR_TYPE 0x22
#define HID_GET_REPORT             0x01
#define HID_GET_IDLE               0x02
#define HID_GET_PROTOCOL           0x03
#define HID_SET_REPORT             0x09
#define HID_SET_IDLE               0x0A
#define HID_SET_PROTOCOL           0x0B

typedef struct {
  uint8_t len, dtype, bcdHIDL, bcdHIDH, country, numDesc, descType, descLenL, descLenH;
} JoystickHIDSub;

typedef struct {
  InterfaceDescriptor iface;
  JoystickHIDSub      hid;
  EndpointDescriptor  in;
} JoystickHIDDesc;

class JoystickHID_ : public PluggableUSBModule {
  public:
    JoystickHID_();
    int send(const void *data, int len);

  protected:
    int  getInterface(uint8_t *interfaceCount) override;
    int  getDescriptor(USBSetup &setup) override;
    bool setup(USBSetup &setup) override;

  private:
    uint8_t epType[1];
    uint8_t protocol = 1;
    uint8_t idle     = 1;
};

JoystickHID_::JoystickHID_() : PluggableUSBModule(1, 1, epType) {
  epType[0] = EP_TYPE_INTERRUPT_IN;
  PluggableUSB().plug(this);
}

int JoystickHID_::getInterface(uint8_t *interfaceCount) {
  *interfaceCount += 1;

  JoystickHIDDesc d = {
    D_INTERFACE(pluggedInterface, 1, USB_DEVICE_CLASS_HUMAN_INTERFACE, 0, 0),
    { 9, 0x21, 0x01, 0x01, 0x00, 0x01, HID_REPORT_DESCRIPTOR_TYPE,
      lowByte(sizeof(JoystickReportDescriptor)), highByte(sizeof(JoystickReportDescriptor))
    },
    D_ENDPOINT(USB_ENDPOINT_IN(pluggedEndpoint), USB_ENDPOINT_TYPE_INTERRUPT, USB_EP_SIZE, 0x01)
  };

  return USB_SendControl(0, &d, sizeof(d));
}

int JoystickHID_::getDescriptor(USBSetup &setup) {
  if (setup.bmRequestType != REQUEST_DEVICETOHOST_STANDARD_INTERFACE) return 0;
  if (setup.wValueH != HID_REPORT_DESCRIPTOR_TYPE)                    return 0;
  if (setup.wIndex  != pluggedInterface)                              return 0;

  protocol = 1;
  return USB_SendControl(TRANSFER_PGM, JoystickReportDescriptor, sizeof(JoystickReportDescriptor));
}

bool JoystickHID_::setup(USBSetup &setup) {
  if (setup.wIndex != pluggedInterface) return false;

  if (setup.bmRequestType == REQUEST_DEVICETOHOST_CLASS_INTERFACE) {
    switch (setup.bRequest) {
      case HID_GET_REPORT:   return true;
      case HID_GET_PROTOCOL: USB_SendControl(TRANSFER_RELEASE, &protocol, 1); return true;
      case HID_GET_IDLE:     USB_SendControl(TRANSFER_RELEASE, &idle, 1);     return true;
    }
  }

  if (setup.bmRequestType == REQUEST_HOSTTODEVICE_CLASS_INTERFACE) {
    switch (setup.bRequest) {
      case HID_SET_PROTOCOL: protocol = setup.wValueL; return true;
      case HID_SET_IDLE:     idle     = setup.wValueL; return true;
      case HID_SET_REPORT:   return true;
    }
  }

  return false;
}

int JoystickHID_::send(const void *data, int len) {
  return USB_Send(pluggedEndpoint | TRANSFER_RELEASE, data, len);
}

JoystickHID_ JoystickHID;
static uint8_t rawReport[REPORT_LEN];
static uint8_t rawLen = 0;

static void patch(uint8_t *p, int16_t offset, int16_t maxv) {
  int32_t v = (int32_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)) + offset;
  if (v < 0)    v = 0;
  if (v > maxv) v = maxv;
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}

void emit() {
  uint8_t out[REPORT_LEN];
  if (!rawLen) return;
  memcpy(out, rawReport, rawLen);
  if (out[0] == REPORT_ID && rawLen >= OFF_RZ + 2) {
    patch(out + OFF_X,  trim[1], 4095);
    patch(out + OFF_Y,  trim[0], 4095);
    patch(out + OFF_RZ, -trim[2], 2047);
  }
  JoystickHID.send(out, rawLen);
}

// The USB host code

USB          Usb;
HIDUniversal Hid(&Usb);

class Relay : public HIDReportParser {
  public:
    void Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf) override {
      (void)hid; (void)is_rpt_id;

      if (len > sizeof(rawReport)) len = sizeof(rawReport);

      memcpy(rawReport, buf, len);
      rawLen = len;
      emit();
    }
};

Relay Parser;

void setupUsbPassthrough() {
  while (Usb.Init() == -1)
    delay(1000);

  delay(200);
  Hid.SetReportParser(0, &Parser);
}

void usbHostTask() {
  Usb.Task();
}
