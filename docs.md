# Use a Leonardo with USB Host shield

Because we're going to make the Leonardo pretend it's your joystick, which means we need an arduino that knows how to connect over USB and can lie about its vendor and model identifiers.

In order to pretend it's your joystick, we need to...

# ...know what we're going to impersonate.

So we load the following sketch to get the USB identifiers for our joystick

```C
/*
 * Print the device identity of whatever is on the host shield, in a form
 * that drops straight into boards.txt.
 */

#include <SPI.h>
#include <hiduniversal.h>

class StringDumper : public HIDUniversal {
  public: StringDumper(USB *usb) : HIDUniversal(usb) {}
  protected: uint8_t OnInitSuccessful() override;
  private: void printString(uint8_t index);
};

void StringDumper::printString(uint8_t index) {
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
  if (len > sizeof(buf)) len = sizeof(buf);
  for (uint8_t i = 2; i + 1 < len; i += 2) Serial.write(buf[i]);
  Serial.println();
}

uint8_t StringDumper::OnInitSuccessful() {
  USB_DEVICE_DESCRIPTOR d;

  if (pUsb->getDevDescr(bAddress, 0, sizeof(d), (uint8_t *)&d)) {
    Serial.println(F("# could not read device descriptor"));
    return 0;
  }

  Serial.print(F("idVendor      0x"));
  Serial.println(d.idVendor, HEX);
  Serial.print(F("idProduct     0x"));
  Serial.println(d.idProduct, HEX);
  Serial.print(F("bcdDevice     0x"));
  Serial.println(d.bcdDevice, HEX);

  Serial.print(F("iManufacturer "));
  printString(d.iManufacturer);
  Serial.print(F("iProduct      "));
  printString(d.iProduct);
  Serial.print(F("iSerialNumber "));
  printString(d.iSerialNumber);

  return 0;
}

USB          Usb;
StringDumper Hid(&Usb);

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000);
  while (Usb.Init() == -1) {
    Serial.println(F("# shield did not start - retrying"));
    delay(1000);
  }
  delay(200);
  Serial.println(F("# ready - plug in the Gladiator"));
}

void loop() {
  Usb.Task();
}
```

We flash that, then with the serial monitor open (at 115200 baud) we plug in our joystick, and for me that gets me:

```
14:34:09.175 -> # ready - plug in the Gladiator
14:34:16.270 -> idVendor      0x231D
14:34:16.270 -> idProduct     0x201
14:34:16.270 -> bcdDevice     0x2122
14:34:16.270 -> iManufacturer VKB-Sim © Alex Oz 2021
14:34:16.270 -> iProduct       VKBsim Gladiator EVO  L
14:34:16.270 -> iSerialNumber (none)
```

So we create a "custom board" definition that we're going to give to the Arduino IDE so that when it flashes our leonardo, it bakes in my joystick's information. We edit the Arduino boards.txt file (make sure to edit the right one, the Program Files (x86) one may not be the one the IDE reads, I had to edit the one in my AppData\Local\Arduino15 dir) so that it ends in:

```
##############################################################

gladiator.name=Gladiator NXT Passthrough (Leonardo)

gladiator.vid.0=0x2341
gladiator.pid.0=0x0036
gladiator.vid.1=0x2341
gladiator.pid.1=0x8036
gladiator.vid.2=0x2A03
gladiator.pid.2=0x0036
gladiator.vid.3=0x2A03
gladiator.pid.3=0x8036
gladiator.vid.4=0x231D
gladiator.pid.4=0x0201

gladiator.upload.tool=avrdude
gladiator.upload.protocol=avr109
gladiator.upload.maximum_size=28672
gladiator.upload.maximum_data_size=2560
gladiator.upload.speed=57600
gladiator.upload.disable_flushing=true
gladiator.upload.use_1200bps_touch=true
gladiator.upload.wait_for_upload_port=true

gladiator.bootloader.tool=avrdude
gladiator.bootloader.low_fuses=0xff
gladiator.bootloader.high_fuses=0xd8
gladiator.bootloader.extended_fuses=0xcb
gladiator.bootloader.file=caterina/Caterina-Leonardo.hex
gladiator.bootloader.unlock_bits=0x3F
gladiator.bootloader.lock_bits=0x2F

gladiator.build.mcu=atmega32u4
gladiator.build.f_cpu=16000000L
gladiator.build.vid=0x231D
gladiator.build.pid=0x0201
gladiator.build.usb_manufacturer="VKB-Sim (c) Alex Oz 2021"
gladiator.build.usb_product=" VKBsim Gladiator EVO  L  "
gladiator.build.board=AVR_LEONARDO
gladiator.build.core=arduino
gladiator.build.variant=leonardo
gladiator.build.extra_flags={build.usb_flags}
```

(note that we replaced © with (c) because unicode is not worth the effort)



# ...Get the raw HID report descriptor from the host shield

```C
/*
 * Dump the full raw HID report descriptor from the host shield.
 * Bypasses GetReportDescr()'s hardcoded 128-byte limit.
 */

#include <SPI.h>
#include <hiduniversal.h>

#define DESC_MAX 512

class HIDDescDumper : public HIDUniversal {
  public: HIDDescDumper(USB *usb) : HIDUniversal(usb) {}
  protected: uint8_t OnInitSuccessful() override;
};

uint8_t HIDDescDumper::OnInitSuccessful() {
  HexDumper<USBReadParser, uint16_t, uint16_t> hex;
  uint8_t buf[64];
  uint8_t rcode;
  Serial.println(F("# ---- report descriptor ----"));
  rcode = pUsb->ctrlReq(bAddress, 0, bmREQ_HID_REPORT,
                        USB_REQUEST_GET_DESCRIPTOR, 0x00,
                        HID_DESCRIPTOR_REPORT, 0, DESC_MAX,
                        sizeof(buf), buf, (USBReadParser *)&hex);
  Serial.println();
  Serial.print(F("# ---- end, rcode "));
  Serial.print(rcode, HEX);
  Serial.println(F(" ----"));
  return 0;
}

HIDDescDumper Hid(&Usb);
USB Usb;

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000);
  while (Usb.Init() == -1) {
    Serial.println(F("# shield did not start - retrying"));
    delay(1000);
  }
  delay(200);
  Serial.println(F("# ready - plug in the Gladiator"));
}

void loop() {
  Usb.Task();
}
```

For my VKBSim Gladiator NXT joystick, with the serial monitor set to 115200 baud this returns:

```
14:20:32.966 -> # ---- report descriptor ----
14:20:32.966 -> 0000: 05 01 09 04 A1 01 05 01 85 01 05 01 09 30 75 10
14:20:32.966 -> 0010: 95 01 15 00 26 FF 0F 46 FF 0F 81 02 05 01 09 31
14:20:32.966 -> 0020: 75 10 95 01 15 00 26 FF 0F 46 FF 0F 81 02 05 01
14:20:32.966 -> 0030: 09 35 75 10 95 01 15 00 26 FF 07 46 FF 07 81 02
14:20:32.966 -> 0040: 05 01 09 32 75 10 95 01 15 00 26 FF 07 46 FF 07
14:20:32.966 -> 0050: 81 02 05 01 09 33 75 10 95 01 15 00 26 FF 03 46
14:20:32.966 -> 0060: FF 03 81 02 05 01 09 34 75 10 95 01 15 00 26 FF
14:20:32.966 -> 0070: 03 46 FF 03 81 02 05 01 09 36 75 10 95 01 15 00
14:20:32.966 -> 0080: 26 FF 07 46 FF 07 81 02 05 01 09 37 75 10 95 01
14:20:32.966 -> 0090: 15 00 26 FF 07 46 FF 07 81 02 05 09 19 01 2A 80
14:20:32.966 -> 00A0: 00 15 00 25 01 75 01 96 80 00 81 02 05 01 09 39
14:20:32.966 -> 00B0: 15 00 26 07 00 35 00 46 68 01 65 14 55 01 75 04
14:20:32.966 -> 00C0: 95 01 81 42 09 00 65 00 55 00 75 04 95 03 81 01
14:20:32.966 -> 00D0: 05 01 09 00 75 10 95 01 81 01 05 01 09 00 75 10
14:20:32.966 -> 00E0: 95 01 81 01 05 01 09 00 75 10 95 01 81 01 05 01
14:20:32.966 -> 00F0: 09 00 75 08 95 17 81 01 85 0B 05 01 09 00 75 08
14:20:33.014 -> 0100: 95 3F 81 01 85 0C 05 01 09 00 75 08 95 3F 81 01
14:20:33.014 -> 0110: 85 08 05 01 09 00 75 08 95 3F 81 01 15 00 26 FF
14:20:33.014 -> 0120: 00 46 FF 00 85 58 75 08 95 3F 09 00 91 02 85 59
14:20:33.014 -> 0130: 75 08 95 80 09 00 B1 02 C0
14:20:33.014 -> # ---- end, rcode 0 ----
```

Which breaks down as:

```
off    bytes        item                                  effect

0000   05 01        Usage Page (Generic Desktop)
0002   09 04        Usage (Joystick)
0004   A1 01        Collection (Application)              opens the device

--- report 0x01: the joystick ---------------------------------------------
0006   05 01        Usage Page (Generic Desktop)
0008   85 01        Report ID (1)                         byte 0 = 0x01

000A   05 01        Usage Page (Generic Desktop)
000C   09 30        Usage (X)
000E   75 10        Report Size (16)                      16 bits per field
0010   95 01        Report Count (1)                      one field
0012   15 00        Logical Minimum (0)
0014   26 FF 0F     Logical Maximum (4095)
0017   46 FF 0F     Physical Maximum (4095)
001A   81 02        Input (Data,Var,Abs)                  -> bytes 1-2

001C   05 01        Usage Page (Generic Desktop)
001E   09 31        Usage (Y)
0020   75 10        Report Size (16)
0022   95 01        Report Count (1)
0024   15 00        Logical Minimum (0)
0026   26 FF 0F     Logical Maximum (4095)
0029   46 FF 0F     Physical Maximum (4095)
002C   81 02        Input (Data,Var,Abs)                  -> bytes 3-4

002E   05 01        Usage Page (Generic Desktop)
0030   09 35        Usage (Rz)                            the twist
0032   75 10        Report Size (16)
0034   95 01        Report Count (1)
0036   15 00        Logical Minimum (0)
0038   26 FF 07     Logical Maximum (2047)
003B   46 FF 07     Physical Maximum (2047)
003E   81 02        Input (Data,Var,Abs)                  -> bytes 5-6

0040   05 01        Usage Page (Generic Desktop)
0042   09 32        Usage (Z)
0044   75 10        Report Size (16)
0046   95 01        Report Count (1)
0048   15 00        Logical Minimum (0)
004A   26 FF 07     Logical Maximum (2047)
004D   46 FF 07     Physical Maximum (2047)
0050   81 02        Input (Data,Var,Abs)                  -> bytes 7-8

0052   05 01        Usage Page (Generic Desktop)
0054   09 33        Usage (Rx)
0056   75 10        Report Size (16)
0058   95 01        Report Count (1)
005A   15 00        Logical Minimum (0)
005C   26 FF 03     Logical Maximum (1023)
005F   46 FF 03     Physical Maximum (1023)
0062   81 02        Input (Data,Var,Abs)                  -> bytes 9-10

0064   05 01        Usage Page (Generic Desktop)
0066   09 34        Usage (Ry)
0068   75 10        Report Size (16)
006A   95 01        Report Count (1)
006C   15 00        Logical Minimum (0)
006E   26 FF 03     Logical Maximum (1023)
0071   46 FF 03     Physical Maximum (1023)
0074   81 02        Input (Data,Var,Abs)                  -> bytes 11-12

0076   05 01        Usage Page (Generic Desktop)
0078   09 36        Usage (Slider)
007A   75 10        Report Size (16)
007C   95 01        Report Count (1)
007E   15 00        Logical Minimum (0)
0080   26 FF 07     Logical Maximum (2047)
0083   46 FF 07     Physical Maximum (2047)
0086   81 02        Input (Data,Var,Abs)                  -> bytes 13-14

0088   05 01        Usage Page (Generic Desktop)
008A   09 37        Usage (Dial)
008C   75 10        Report Size (16)
008E   95 01        Report Count (1)
0090   15 00        Logical Minimum (0)
0092   26 FF 07     Logical Maximum (2047)
0095   46 FF 07     Physical Maximum (2047)
0098   81 02        Input (Data,Var,Abs)                  -> bytes 15-16

009A   05 09        Usage Page (Button)
009C   19 01        Usage Minimum (Button 1)
009E   2A 80 00     Usage Maximum (Button 128)
00A1   15 00        Logical Minimum (0)
00A3   25 01        Logical Maximum (1)
00A5   75 01        Report Size (1)                       one bit each
00A7   96 80 00     Report Count (128)                    128 of them
00AA   81 02        Input (Data,Var,Abs)                  -> bytes 17-32

00AC   05 01        Usage Page (Generic Desktop)
00AE   09 39        Usage (Hat switch)
00B0   15 00        Logical Minimum (0)
00B2   26 07 00     Logical Maximum (7)                   8 directions
00B5   35 00        Physical Minimum (0)
00B7   46 68 01     Physical Maximum (360)                degrees
00BA   65 14        Unit (English rotation: degrees)
00BC   55 01        Unit Exponent (1)
00BE   75 04        Report Size (4)
00C0   95 01        Report Count (1)
00C2   81 42        Input (Data,Var,Abs,Null State)       -> byte 33, bits 0-3

00C4   09 00        Usage (Undefined)
00C6   65 00        Unit (None)                           undo the degrees
00C8   55 00        Unit Exponent (0)
00CA   75 04        Report Size (4)
00CC   95 03        Report Count (3)
00CE   81 01        Input (Const,Arr,Abs)                 12 bits padding

00D0   05 01        Usage Page (Generic Desktop)
00D2   09 00        Usage (Undefined)
00D4   75 10        Report Size (16)
00D6   95 01        Report Count (1)
00D8   81 01        Input (Const)                         2 bytes padding

00DA   05 01        Usage Page (Generic Desktop)
00DC   09 00        Usage (Undefined)
00DE   75 10        Report Size (16)
00E0   95 01        Report Count (1)
00E2   81 01        Input (Const)                         2 bytes padding

00E4   05 01        Usage Page (Generic Desktop)
00E6   09 00        Usage (Undefined)
00E8   75 10        Report Size (16)
00EA   95 01        Report Count (1)
00EC   81 01        Input (Const)                         2 bytes padding

00EE   05 01        Usage Page (Generic Desktop)
00F0   09 00        Usage (Undefined)
00F2   75 08        Report Size (8)
00F4   95 17        Report Count (23)
00F6   81 01        Input (Const)                         23 bytes padding
                                                          report 0x01 = 64 bytes

--- reports 0x0B, 0x0C, 0x08: VKB's config channel, inbound ----------------
00F8   85 0B        Report ID (0x0B)
00FA   05 01        Usage Page (Generic Desktop)
00FC   09 00        Usage (Undefined)
00FE   75 08        Report Size (8)
0100   95 3F        Report Count (63)
0102   81 01        Input (Const)                         63 bytes, opaque

0104   85 0C        Report ID (0x0C)
0106   05 01        Usage Page (Generic Desktop)
0108   09 00        Usage (Undefined)
010A   75 08        Report Size (8)
010C   95 3F        Report Count (63)
010E   81 01        Input (Const)

0110   85 08        Report ID (0x08)
0112   05 01        Usage Page (Generic Desktop)
0114   09 00        Usage (Undefined)
0116   75 08        Report Size (8)
0118   95 3F        Report Count (63)
011A   81 01        Input (Const)

--- outbound and feature channels ------------------------------------------
011C   15 00        Logical Minimum (0)
011E   26 FF 00     Logical Maximum (255)
0121   46 FF 00     Physical Maximum (255)

0124   85 58        Report ID (0x58)
0126   75 08        Report Size (8)
0128   95 3F        Report Count (63)
012A   09 00        Usage (Undefined)
012C   91 02        Output (Data,Var,Abs)                 host -> stick

012E   85 59        Report ID (0x59)
0130   75 08        Report Size (8)
0132   95 80        Report Count (128)
0134   09 00        Usage (Undefined)
0136   B1 02        Feature (Data,Var,Abs)                128-byte config block

0138   C0           End Collection
```

We can then set up a sketch that registers everything in exactly the same way as my joystick:

# our hijacking sketch

```C
/*
 * VKB Gladiator clone - Leonardo + USB Host Shield 2.0
 * Presents the stick's own 313-byte report descriptor and forwards reports verbatim.
 * Build with the "Gladiator NXT Passthrough (Leonardo)" board entry.
 */

#include <SPI.h>
#include <hiduniversal.h>
#include <PluggableUSB.h>

/* --- the Gladiator's report descriptor, byte for byte -------------------- */

static const uint8_t VKB_ReportDescriptor[] PROGMEM = {
  0x05,0x01,0x09,0x04,0xA1,0x01,0x05,0x01,0x85,0x01,0x05,0x01,0x09,0x30,0x75,0x10,
  0x95,0x01,0x15,0x00,0x26,0xFF,0x0F,0x46,0xFF,0x0F,0x81,0x02,0x05,0x01,0x09,0x31,
  0x75,0x10,0x95,0x01,0x15,0x00,0x26,0xFF,0x0F,0x46,0xFF,0x0F,0x81,0x02,0x05,0x01,
  0x09,0x35,0x75,0x10,0x95,0x01,0x15,0x00,0x26,0xFF,0x07,0x46,0xFF,0x07,0x81,0x02,
  0x05,0x01,0x09,0x32,0x75,0x10,0x95,0x01,0x15,0x00,0x26,0xFF,0x07,0x46,0xFF,0x07,
  0x81,0x02,0x05,0x01,0x09,0x33,0x75,0x10,0x95,0x01,0x15,0x00,0x26,0xFF,0x03,0x46,
  0xFF,0x03,0x81,0x02,0x05,0x01,0x09,0x34,0x75,0x10,0x95,0x01,0x15,0x00,0x26,0xFF,
  0x03,0x46,0xFF,0x03,0x81,0x02,0x05,0x01,0x09,0x36,0x75,0x10,0x95,0x01,0x15,0x00,
  0x26,0xFF,0x07,0x46,0xFF,0x07,0x81,0x02,0x05,0x01,0x09,0x37,0x75,0x10,0x95,0x01,
  0x15,0x00,0x26,0xFF,0x07,0x46,0xFF,0x07,0x81,0x02,0x05,0x09,0x19,0x01,0x2A,0x80,
  0x00,0x15,0x00,0x25,0x01,0x75,0x01,0x96,0x80,0x00,0x81,0x02,0x05,0x01,0x09,0x39,
  0x15,0x00,0x26,0x07,0x00,0x35,0x00,0x46,0x68,0x01,0x65,0x14,0x55,0x01,0x75,0x04,
  0x95,0x01,0x81,0x42,0x09,0x00,0x65,0x00,0x55,0x00,0x75,0x04,0x95,0x03,0x81,0x01,
  0x05,0x01,0x09,0x00,0x75,0x10,0x95,0x01,0x81,0x01,0x05,0x01,0x09,0x00,0x75,0x10,
  0x95,0x01,0x81,0x01,0x05,0x01,0x09,0x00,0x75,0x10,0x95,0x01,0x81,0x01,0x05,0x01,
  0x09,0x00,0x75,0x08,0x95,0x17,0x81,0x01,0x85,0x0B,0x05,0x01,0x09,0x00,0x75,0x08,
  0x95,0x3F,0x81,0x01,0x85,0x0C,0x05,0x01,0x09,0x00,0x75,0x08,0x95,0x3F,0x81,0x01,
  0x85,0x08,0x05,0x01,0x09,0x00,0x75,0x08,0x95,0x3F,0x81,0x01,0x15,0x00,0x26,0xFF,
  0x00,0x46,0xFF,0x00,0x85,0x58,0x75,0x08,0x95,0x3F,0x09,0x00,0x91,0x02,0x85,0x59,
  0x75,0x08,0x95,0x80,0x09,0x00,0xB1,0x02,0xC0
};

/* --- USB device side ----------------------------------------------------- */

#define HID_REPORT_DESCRIPTOR_TYPE 0x22
#define HID_GET_REPORT             0x01
#define HID_GET_IDLE               0x02
#define HID_GET_PROTOCOL           0x03
#define HID_SET_REPORT             0x09
#define HID_SET_IDLE               0x0A
#define HID_SET_PROTOCOL           0x0B

typedef struct {
  uint8_t len, dtype, bcdHIDL, bcdHIDH, country, numDesc, descType, descLenL, descLenH;
} VKBHIDSub;

typedef struct {
  InterfaceDescriptor iface;
  VKBHIDSub           hid;
  EndpointDescriptor  in;
} VKBHIDDesc;

class VKBHID_ : public PluggableUSBModule {
  public:
    VKBHID_();
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

VKBHID_::VKBHID_() : PluggableUSBModule(1, 1, epType) {
  epType[0] = EP_TYPE_INTERRUPT_IN;
  PluggableUSB().plug(this);
}

int VKBHID_::getInterface(uint8_t *interfaceCount) {
  *interfaceCount += 1;

  VKBHIDDesc d = {
    D_INTERFACE(pluggedInterface, 1, USB_DEVICE_CLASS_HUMAN_INTERFACE, 0, 0),
    { 9, 0x21, 0x01, 0x01, 0x00, 0x01, HID_REPORT_DESCRIPTOR_TYPE,
      lowByte(sizeof(VKB_ReportDescriptor)), highByte(sizeof(VKB_ReportDescriptor)) },
    D_ENDPOINT(USB_ENDPOINT_IN(pluggedEndpoint), USB_ENDPOINT_TYPE_INTERRUPT, USB_EP_SIZE, 0x01)
  };

  return USB_SendControl(0, &d, sizeof(d));
}

int VKBHID_::getDescriptor(USBSetup &setup) {
  if (setup.bmRequestType != REQUEST_DEVICETOHOST_STANDARD_INTERFACE) return 0;
  if (setup.wValueH != HID_REPORT_DESCRIPTOR_TYPE)                    return 0;
  if (setup.wIndex  != pluggedInterface)                              return 0;

  protocol = 1;
  return USB_SendControl(TRANSFER_PGM, VKB_ReportDescriptor, sizeof(VKB_ReportDescriptor));
}

bool VKBHID_::setup(USBSetup &setup) {
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

int VKBHID_::send(const void *data, int len) {
  return USB_Send(pluggedEndpoint | TRANSFER_RELEASE, data, len);
}

VKBHID_ VKBHID;

/* --- USB host side ------------------------------------------------------- */

USB          Usb;
HIDUniversal Hid(&Usb);

class Relay : public HIDReportParser {
  public:
    void Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf) override {
      (void)hid; (void)is_rpt_id;
      VKBHID.send(buf, len);
    }
};

Relay Parser;

void setup() {
  while (Usb.Init() == -1)
    delay(1000);

  delay(200);
  Hid.SetReportParser(0, &Parser);
}

void loop() {
  Usb.Task();
}
```

And now Windows sees "the same joystick" as before!

# Adding trim

Which mean all that's left is to add trim controls that literally change the joystick inputs

```C
/*
 * VKB Gladiator clone + trim offsets - Leonardo + USB Host Shield 2.0
 *
 * Forwards the stick's reports verbatim, except X, Y and Rz which get the
 * aileron, pitch and rudder trim added before sending.
 *
 * Build with the "Gladiator NXT Passthrough (Leonardo)" board entry.
 */

#include <SPI.h>
#include <hiduniversal.h>
#include <PluggableUSB.h>
#include <Adafruit_seesaw.h>

#define USE_OLED 1          /* see the flash note below */

#if USE_OLED
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 oled(128, 32, &Wire, -1);
#endif

/* --- trim ---------------------------------------------------------------- */

#define ENCODER_COUNT 6
#define AXIS_COUNT    3
#define SS_SWITCH     24
#define TRIM_CENTRE   1000
#define TRIM_MAX      2000
#define COARSE_STEP   5
#define FINE_STEP     1
#define TRIM_POLL_MS  20

/* index 0 = pitch (Y), 1 = aileron (X), 2 = rudder (Rz) */
static const uint8_t ENC_ADDR[ENCODER_COUNT] = { 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B };

Adafruit_seesaw enc[ENCODER_COUNT];
static int32_t  encBase[ENCODER_COUNT];
static bool     encOk[ENCODER_COUNT];
static bool     swWas[ENCODER_COUNT];
static int16_t  trim[AXIS_COUNT];        /* -1000 .. +1000 */

/* --- the Gladiator's report descriptor, byte for byte -------------------- */

static const uint8_t VKB_ReportDescriptor[] PROGMEM = {
  0x05,0x01,0x09,0x04,0xA1,0x01,0x05,0x01,0x85,0x01,0x05,0x01,0x09,0x30,0x75,0x10,
  0x95,0x01,0x15,0x00,0x26,0xFF,0x0F,0x46,0xFF,0x0F,0x81,0x02,0x05,0x01,0x09,0x31,
  0x75,0x10,0x95,0x01,0x15,0x00,0x26,0xFF,0x0F,0x46,0xFF,0x0F,0x81,0x02,0x05,0x01,
  0x09,0x35,0x75,0x10,0x95,0x01,0x15,0x00,0x26,0xFF,0x07,0x46,0xFF,0x07,0x81,0x02,
  0x05,0x01,0x09,0x32,0x75,0x10,0x95,0x01,0x15,0x00,0x26,0xFF,0x07,0x46,0xFF,0x07,
  0x81,0x02,0x05,0x01,0x09,0x33,0x75,0x10,0x95,0x01,0x15,0x00,0x26,0xFF,0x03,0x46,
  0xFF,0x03,0x81,0x02,0x05,0x01,0x09,0x34,0x75,0x10,0x95,0x01,0x15,0x00,0x26,0xFF,
  0x03,0x46,0xFF,0x03,0x81,0x02,0x05,0x01,0x09,0x36,0x75,0x10,0x95,0x01,0x15,0x00,
  0x26,0xFF,0x07,0x46,0xFF,0x07,0x81,0x02,0x05,0x01,0x09,0x37,0x75,0x10,0x95,0x01,
  0x15,0x00,0x26,0xFF,0x07,0x46,0xFF,0x07,0x81,0x02,0x05,0x09,0x19,0x01,0x2A,0x80,
  0x00,0x15,0x00,0x25,0x01,0x75,0x01,0x96,0x80,0x00,0x81,0x02,0x05,0x01,0x09,0x39,
  0x15,0x00,0x26,0x07,0x00,0x35,0x00,0x46,0x68,0x01,0x65,0x14,0x55,0x01,0x75,0x04,
  0x95,0x01,0x81,0x42,0x09,0x00,0x65,0x00,0x55,0x00,0x75,0x04,0x95,0x03,0x81,0x01,
  0x05,0x01,0x09,0x00,0x75,0x10,0x95,0x01,0x81,0x01,0x05,0x01,0x09,0x00,0x75,0x10,
  0x95,0x01,0x81,0x01,0x05,0x01,0x09,0x00,0x75,0x10,0x95,0x01,0x81,0x01,0x05,0x01,
  0x09,0x00,0x75,0x08,0x95,0x17,0x81,0x01,0x85,0x0B,0x05,0x01,0x09,0x00,0x75,0x08,
  0x95,0x3F,0x81,0x01,0x85,0x0C,0x05,0x01,0x09,0x00,0x75,0x08,0x95,0x3F,0x81,0x01,
  0x85,0x08,0x05,0x01,0x09,0x00,0x75,0x08,0x95,0x3F,0x81,0x01,0x15,0x00,0x26,0xFF,
  0x00,0x46,0xFF,0x00,0x85,0x58,0x75,0x08,0x95,0x3F,0x09,0x00,0x91,0x02,0x85,0x59,
  0x75,0x08,0x95,0x80,0x09,0x00,0xB1,0x02,0xC0
};

/* --- USB device side ----------------------------------------------------- */

#define HID_REPORT_DESCRIPTOR_TYPE 0x22
#define HID_GET_REPORT             0x01
#define HID_GET_IDLE               0x02
#define HID_GET_PROTOCOL           0x03
#define HID_SET_REPORT             0x09
#define HID_SET_IDLE               0x0A
#define HID_SET_PROTOCOL           0x0B

typedef struct {
  uint8_t len, dtype, bcdHIDL, bcdHIDH, country, numDesc, descType, descLenL, descLenH;
} VKBHIDSub;

typedef struct {
  InterfaceDescriptor iface;
  VKBHIDSub           hid;
  EndpointDescriptor  in;
} VKBHIDDesc;

class VKBHID_ : public PluggableUSBModule {
  public:
    VKBHID_();
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

VKBHID_::VKBHID_() : PluggableUSBModule(1, 1, epType) {
  epType[0] = EP_TYPE_INTERRUPT_IN;
  PluggableUSB().plug(this);
}

int VKBHID_::getInterface(uint8_t *interfaceCount) {
  *interfaceCount += 1;

  VKBHIDDesc d = {
    D_INTERFACE(pluggedInterface, 1, USB_DEVICE_CLASS_HUMAN_INTERFACE, 0, 0),
    { 9, 0x21, 0x01, 0x01, 0x00, 0x01, HID_REPORT_DESCRIPTOR_TYPE,
      lowByte(sizeof(VKB_ReportDescriptor)), highByte(sizeof(VKB_ReportDescriptor)) },
    D_ENDPOINT(USB_ENDPOINT_IN(pluggedEndpoint), USB_ENDPOINT_TYPE_INTERRUPT, USB_EP_SIZE, 0x01)
  };

  return USB_SendControl(0, &d, sizeof(d));
}

int VKBHID_::getDescriptor(USBSetup &setup) {
  if (setup.bmRequestType != REQUEST_DEVICETOHOST_STANDARD_INTERFACE) return 0;
  if (setup.wValueH != HID_REPORT_DESCRIPTOR_TYPE)                    return 0;
  if (setup.wIndex  != pluggedInterface)                              return 0;

  protocol = 1;
  return USB_SendControl(TRANSFER_PGM, VKB_ReportDescriptor, sizeof(VKB_ReportDescriptor));
}

bool VKBHID_::setup(USBSetup &setup) {
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

int VKBHID_::send(const void *data, int len) {
  return USB_Send(pluggedEndpoint | TRANSFER_RELEASE, data, len);
}

VKBHID_ VKBHID;

/* --- report patching ----------------------------------------------------- */

#define OFF_X   1       /* aileron trim, 0..4095 */
#define OFF_Y   3       /* pitch trim,   0..4095 */
#define OFF_RZ  5       /* rudder trim,  0..2047 */

static uint8_t rawReport[64];
static uint8_t rawLen = 0;

static void patch(uint8_t *p, int16_t offset, int16_t maxv) {
  int32_t v = (int32_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)) + offset;

  if (v < 0)    v = 0;
  if (v > maxv) v = maxv;

  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}

static void emit() {
  uint8_t out[64];

  if (!rawLen) return;

  memcpy(out, rawReport, rawLen);

  if (out[0] == 0x01 && rawLen >= OFF_RZ + 2) {
    patch(out + OFF_X,  trim[1], 4095);   /* aileron */
    patch(out + OFF_Y,  trim[0], 4095);   /* pitch   */
    patch(out + OFF_RZ, -trim[2], 2047);  /* rudder  */
  }

  VKBHID.send(out, rawLen);
}

/* --- USB host side ------------------------------------------------------- */

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

/* --- encoders ------------------------------------------------------------ */

static void bindEncoders() {
  for (uint8_t i = 0; i < ENCODER_COUNT; i++) {
    encOk[i] = enc[i].begin(ENC_ADDR[i]);

    if (!encOk[i]) continue;

    enc[i].pinMode(SS_SWITCH, INPUT_PULLUP);
    encBase[i] = enc[i].getEncoderPosition();
    swWas[i]   = true;
  }
}

/* Returns true if any trim value changed. */
static bool readTrim() {
  bool changed = false;

  for (uint8_t a = 0; a < AXIS_COUNT; a++) {
    uint8_t c = a;              /* coarse encoder */
    uint8_t f = a + AXIS_COUNT; /* fine encoder   */

    if (!encOk[c] || !encOk[f]) continue;

    /* switch on either encoder of the pair re-centres that axis */
    for (uint8_t k = 0; k < 2; k++) {
      uint8_t i  = k ? f : c;
      bool    sw = enc[i].digitalRead(SS_SWITCH);

      if (!sw && swWas[i]) {
        encBase[c] = enc[c].getEncoderPosition();
        encBase[f] = enc[f].getEncoderPosition();
      }
      swWas[i] = sw;
    }

    int32_t coarse = enc[c].getEncoderPosition() - encBase[c];
    int32_t fine   = enc[f].getEncoderPosition() - encBase[f];
    int32_t v      = TRIM_CENTRE + coarse * COARSE_STEP + fine * FINE_STEP;

    if (v < 0)        v = 0;
    if (v > TRIM_MAX) v = TRIM_MAX;

    int16_t t = (int16_t)(v - TRIM_CENTRE);

    if (t != trim[a]) {
      trim[a] = t;
      changed = true;
    }
  }

  return changed;
}

#if USE_OLED
static void drawValuesToScreen() {
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.print(F("Pitch   ")); oled.println(trim[0]);
  oled.print(F("Aileron ")); oled.println(trim[1]);
  oled.print(F("Rudder  ")); oled.println(trim[2]);
  oled.display();
}
#endif

/* --- main ---------------------------------------------------------------- */

void setup() {
  bindEncoders();

#if USE_OLED
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  drawValuesToScreen();
#endif

  while (Usb.Init() == -1)
    delay(1000);

  delay(200);
  Hid.SetReportParser(0, &Parser);
}

void loop() {
  static uint32_t next = 0;

  Usb.Task();

  if ((int32_t)(millis() - next) >= 0) {
    next = millis() + TRIM_POLL_MS;

    if (readTrim()) {
      emit();               /* push trim change even if the stick is idle */
#if USE_OLED
      drawValuesToScreen();
#endif
    }
  }
}
```

And that still comfortably fits: 28614 bytes of program means we have still have 4 kb free to add things later if we want to!

# Does it work?

Very much so! Although some applications will go "okay but the way you show up means I think you're still a different device" so Microsoft Flight Simulator, for example, goes "sure, this is the joystick you say it is, it's just not the SAME one as you had plugged in before" so you need to bind some axes again. No biggy if I'm honest: just load a profile.
