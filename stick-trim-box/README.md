# Add trim to your stick

Load this sketch onto your Arduino Leonardo (after running through the code setup steps) and enjoy having a pure hardware trim box rather than needing to use any sort of in-game bindings for setting trim.

## How do I make it show up "as" my joystick?

By default, your joystick will now show up as an Arduino Leonardo, because that's literally what it is, but we can change the name and vendor/product ids that it registers with by defining a new board in the `boards.txt` that the Arduino IDE uses. Find out which one your IDE uses (too many options so I'll leave the rest of the internet to explain how to do that) and then add a new board at the end.

In my case, I added the following to the boards.txt in the AVR folder:

```ini
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

The important values for this file are reported by the `joystick-dump` sketch, right before the hex block.

With this in place, you can pick this new board rather than the plain Leonardo profile, and flashing your Arduino will now make it register "as if it's your joystick". Neat!
