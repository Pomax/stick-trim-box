# Dump your joystick's USB signature

Load this sketch onto your Arduino Leonardo with your encoders chained up, and then run the serial monitor at 115200: you will be prompted to connect your joystick, after which it will automatically dump the block of hex codes that you need to copy into the `joystick-profile.ino` file. It does not add any "these codes are these things" comments, because that would require a full HID code parser.
