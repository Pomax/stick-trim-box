/*
   The Gladiator's own HID report descriptor, byte for byte.

   A report descriptor is a flat stream of items, not a struct. Every item
   starts with a prefix byte:

     bits 7-4  tag    what the item means
     bits 3-2  type   0 = main, 1 = global, 2 = local
     bits 1-0  size   how many data bytes follow (0, 1, 2 or 4)

   Global items - usage page, report size, report count, logical range - stay
   in force until changed. Local items, such as usage, apply only to the next
   main item. Main items, meaning input, output, feature and collection,
   consume whatever state is current and emit the actual bits. So the stream
   reads as "set up, emit, set up, emit".

   Each input item appends report size x report count bits to the report being
   described, and nothing else moves that cursor. That is what fixes the
   layout of report 0x01, which comes out as:

     byte  0       report id, always 0x01
     bytes 1-16    eight 16-bit little-endian axes, in the order they are
                   declared below: X, Y, Rz, Z, Rx, Ry, slider, dial
     bytes 17-32   128 buttons, one bit each, LSB of byte 17 being button 1
     byte  33      hat switch in the low nibble, filler in the high nibble
     bytes 34-63   filler, padding the report out to 64 bytes

   Reports 0x08, 0x0B, 0x0C, 0x58 and 0x59 are VKB's configuration channel and
   carry no joystick state.
*/

static const uint8_t JoystickReportDescriptor[] PROGMEM = {

  0x05, 0x01,      // usage page: generic desktop
  0x09, 0x04,      // usage: joystick
  0xA1, 0x01,      // collection: application

  // report 0x01, the joystick state
  0x05, 0x01,      // usage page: generic desktop
  0x85, 0x01,      // report id: 1

  // X axis
  0x05, 0x01,      // usage page: generic desktop
  0x09, 0x30,      // usage: X
  0x75, 0x10,      // report size: 16 bits
  0x95, 0x01,      // report count: 1 field
  0x15, 0x00,      // logical minimum: 0
  0x26, 0xFF, 0x0F, // logical maximum: 4095
  0x46, 0xFF, 0x0F, // physical maximum: 4095
  0x81, 0x02,      // input: data, variable, absolute

  // Y axis
  0x05, 0x01,      // usage page: generic desktop
  0x09, 0x31,      // usage: Y
  0x75, 0x10,      // report size: 16 bits
  0x95, 0x01,      // report count: 1 field
  0x15, 0x00,      // logical minimum: 0
  0x26, 0xFF, 0x0F, // logical maximum: 4095
  0x46, 0xFF, 0x0F, // physical maximum: 4095
  0x81, 0x02,      // input: data, variable, absolute

  // Rz, the stick twist
  0x05, 0x01,      // usage page: generic desktop
  0x09, 0x35,      // usage: Rz
  0x75, 0x10,      // report size: 16 bits
  0x95, 0x01,      // report count: 1 field
  0x15, 0x00,      // logical minimum: 0
  0x26, 0xFF, 0x07, // logical maximum: 2047
  0x46, 0xFF, 0x07, // physical maximum: 2047
  0x81, 0x02,      // input: data, variable, absolute

  // Z axis
  0x05, 0x01,      // usage page: generic desktop
  0x09, 0x32,      // usage: Z
  0x75, 0x10,      // report size: 16 bits
  0x95, 0x01,      // report count: 1 field
  0x15, 0x00,      // logical minimum: 0
  0x26, 0xFF, 0x07, // logical maximum: 2047
  0x46, 0xFF, 0x07, // physical maximum: 2047
  0x81, 0x02,      // input: data, variable, absolute

  // Rx
  0x05, 0x01,      // usage page: generic desktop
  0x09, 0x33,      // usage: Rx
  0x75, 0x10,      // report size: 16 bits
  0x95, 0x01,      // report count: 1 field
  0x15, 0x00,      // logical minimum: 0
  0x26, 0xFF, 0x03, // logical maximum: 1023
  0x46, 0xFF, 0x03, // physical maximum: 1023
  0x81, 0x02,      // input: data, variable, absolute

  // Ry
  0x05, 0x01,      // usage page: generic desktop
  0x09, 0x34,      // usage: Ry
  0x75, 0x10,      // report size: 16 bits
  0x95, 0x01,      // report count: 1 field
  0x15, 0x00,      // logical minimum: 0
  0x26, 0xFF, 0x03, // logical maximum: 1023
  0x46, 0xFF, 0x03, // physical maximum: 1023
  0x81, 0x02,      // input: data, variable, absolute

  // slider
  0x05, 0x01,      // usage page: generic desktop
  0x09, 0x36,      // usage: slider
  0x75, 0x10,      // report size: 16 bits
  0x95, 0x01,      // report count: 1 field
  0x15, 0x00,      // logical minimum: 0
  0x26, 0xFF, 0x07, // logical maximum: 2047
  0x46, 0xFF, 0x07, // physical maximum: 2047
  0x81, 0x02,      // input: data, variable, absolute

  // dial
  0x05, 0x01,      // usage page: generic desktop
  0x09, 0x37,      // usage: dial
  0x75, 0x10,      // report size: 16 bits
  0x95, 0x01,      // report count: 1 field
  0x15, 0x00,      // logical minimum: 0
  0x26, 0xFF, 0x07, // logical maximum: 2047
  0x46, 0xFF, 0x07, // physical maximum: 2047
  0x81, 0x02,      // input: data, variable, absolute

  // 128 buttons, one bit each, declared as a usage range rather than
  // 128 separate usage items, so bit order is button order
  0x05, 0x09,      // usage page: button
  0x19, 0x01,      // usage minimum: button 1
  0x2A, 0x80, 0x00, // usage maximum: button 128
  0x15, 0x00,      // logical minimum: 0
  0x25, 0x01,      // logical maximum: 1
  0x75, 0x01,      // report size: 1 bit
  0x96, 0x80, 0x00, // report count: 128 fields
  0x81, 0x02,      // input: data, variable, absolute

  // the hat switch, four bits holding eight directions
  0x05, 0x01,      // usage page: generic desktop
  0x09, 0x39,      // usage: hat switch
  0x15, 0x00,      // logical minimum: 0
  0x26, 0x07, 0x00, // logical maximum: 7
  0x35, 0x00,      // physical minimum: 0
  0x46, 0x68, 0x01, // physical maximum: 360
  0x65, 0x14,      // unit: english rotation, degrees
  0x55, 0x01,      // unit exponent: 1
  0x75, 0x04,      // report size: 4 bits
  0x95, 0x01,      // report count: 1 field
  0x81, 0x42,      // input: data, var, abs, null state (8-15 = centred)

  // twelve constant bits finishing the hat's byte and the one after it
  0x09, 0x00,      // usage: undefined
  0x65, 0x00,      // unit: none
  0x55, 0x00,      // unit exponent: 0
  0x75, 0x04,      // report size: 4 bits
  0x95, 0x03,      // report count: 3 fields
  0x81, 0x01,      // input: constant

  // Internal VKB protocol data
  0x05, 0x01, 0x09, 0x00, 0x75, 0x10, 0x95, 0x01, 0x81, 0x01,
  0x05, 0x01, 0x09, 0x00, 0x75, 0x10, 0x95, 0x01, 0x81, 0x01,
  0x05, 0x01, 0x09, 0x00, 0x75, 0x10, 0x95, 0x01, 0x81, 0x01,
  0x05, 0x01, 0x09, 0x00, 0x75, 0x08, 0x95, 0x17, 0x81, 0x01,
  0x85, 0x0B, 0x05, 0x01, 0x09, 0x00, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x01,
  0x85, 0x0C, 0x05, 0x01, 0x09, 0x00, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x01,
  0x85, 0x08, 0x05, 0x01, 0x09, 0x00, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x01,
  0x15, 0x00, 0x26, 0xFF, 0x00, 0x46, 0xFF, 0x00,
  0x85, 0x58, 0x75, 0x08, 0x95, 0x3F, 0x09, 0x00, 0x91, 0x02,
  0x85, 0x59, 0x75, 0x08, 0x95, 0x80, 0x09, 0x00, 0xB1, 0x02,

  0xC0             // end collection
};
