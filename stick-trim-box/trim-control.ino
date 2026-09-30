#include <Adafruit_seesaw.h>


#define ENC_PITCH_COARSE    0x39
#define ENC_AILERON_COARSE  0x37
#define ENC_RUDDER_COARSE   0x3B
#define ENC_PITCH_FINE      0x36
#define ENC_AILERON_FINE    0x3A
#define ENC_RUDDER_FINE     0x38

#define ENCODER_COUNT          6
#define SS_SWITCH             24
#define COARSE_STEP           25
#define FINE_STEP              1
#define AXIS_COUNT             3

static const int16_t TRIM_MAX[AXIS_COUNT]    = { 4095, 4095, 2047 };
static const int16_t TRIM_CENTRE[AXIS_COUNT] = { 2047, 2047, 1023 };

static const uint8_t ENC_ADDR[ENCODER_COUNT] = {
  ENC_PITCH_COARSE,
  ENC_AILERON_COARSE,
  ENC_RUDDER_COARSE,
  ENC_PITCH_FINE,
  ENC_AILERON_FINE,
  ENC_RUDDER_FINE
};

Adafruit_seesaw enc[ENCODER_COUNT];
static int32_t  encBase[ENCODER_COUNT];
static bool     encOk[ENCODER_COUNT];
static bool     swWas[ENCODER_COUNT];
static int16_t  trim[AXIS_COUNT];        /* -1000 .. +1000 */

void setupTrimEncoders() {
  for (uint8_t i = 0; i < ENCODER_COUNT; i++) {
    encOk[i] = enc[i].begin(ENC_ADDR[i]);
    if (!encOk[i]) continue;

    enc[i].pinMode(SS_SWITCH, INPUT_PULLUP);
    encBase[i] = enc[i].getEncoderPosition();
    swWas[i]   = true;
  }

  #if USE_OLED
  oledBegin();
  drawTrimValues(trim[0], trim[1], trim[2]);
  #endif
}

// Returns true if any trim value changed
bool readTrim() {
  bool changed = false;

  for (uint8_t a = 0; a < AXIS_COUNT; a++) {
    uint8_t c = a;              /* coarse encoder */
    uint8_t f = a + AXIS_COUNT; /* fine encoder   */

    if (!encOk[c] || !encOk[f]) continue;

    // switch on either encoder of the pair re-centres that axis
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
    int32_t v      = TRIM_CENTRE[a] + coarse * COARSE_STEP + fine * FINE_STEP;

    if (v < 0)           v = 0;
    if (v > TRIM_MAX[a]) v = TRIM_MAX[a];
    int16_t t = (int16_t)(v - TRIM_CENTRE[a]);

    if (t != trim[a]) {
      trim[a] = t;
      changed = true;
    }
  }

  #if USE_OLED
  if (changed) drawTrimValues(trim[0], trim[1], trim[2]);
  #endif

  return changed;
}
