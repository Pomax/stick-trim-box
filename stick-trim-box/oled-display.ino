/*
   OLED - a 128x32 SSD1306 on the same STEMMA QT bus as the encoders.

   Takes the three numbers it is given and draws them. It has no idea where
   they come from, so nothing here depends on the trim code.

   Set USE_OLED to 0 to build without a display; this tab sorts before the
   others, so the rest of the sketch sees the value.
*/

#define USE_OLED 1

#if USE_OLED

#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 oled(128, 32, &Wire, -1);

void oledBegin() {
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
}

void drawTrimValues(int16_t pitch, int16_t aileron, int16_t rudder) {
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.print(F("Pitch   ")); oled.println(pitch);
  oled.print(F("Aileron ")); oled.println(aileron);
  oled.print(F("Rudder  ")); oled.println(rudder);
  oled.display();
}

#endif
