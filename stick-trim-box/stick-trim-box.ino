/*
   VKB Gladiator clone + trim offsets - Leonardo + USB Host Shield 2.0

   Forwards the stick's reports verbatim, except X, Y and Rz which get the
   aileron, pitch and rudder trim added before sending.

   Build with the "Gladiator NXT Passthrough (Leonardo)" board entry.

   oled-display.ino - the SSD1306, drawing whatever numbers it is handed
   trim-control.ino - the six STEMMA encoders and the trim values
   usb-host.ino     - host shield, the cloned HID device, report patching
*/

#define TRIM_POLL_MS  20

void setup() {
  setupUsbPassthrough();
  setupTrimEncoders();
}

void loop() {
  static unsigned long next = 0;
  usbHostTask();
  if (millis() - next >= 0) {
    next = millis() + TRIM_POLL_MS;
    if (readTrim()) emit();
  }
}
