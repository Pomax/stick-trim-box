#include <Adafruit_seesaw.h>

#define SERIAL_BAUD  115200
#define FIRST_ADDR   0x36
#define LAST_ADDR    0x3D
#define ADDR_COUNT   (LAST_ADDR - FIRST_ADDR + 1)
#define AXIS_COUNT   6
#define TURN_DETENTS 2
#define POLL_MS      20

Adafruit_seesaw enc[ADDR_COUNT];
static bool     present[ADDR_COUNT];
static bool     taken[ADDR_COUNT];
static int32_t  base[ADDR_COUNT];

static const char *PROMPT[AXIS_COUNT] = {
  "coarse pitch",
  "fine pitch",
  "coarse aileron",
  "fine aileron",
  "coarse rudder",
  "fine rudder"
};

static const char *DEFINE_NAME[AXIS_COUNT] = {
  "ENC_PITCH_COARSE",
  "ENC_PITCH_FINE",
  "ENC_AILERON_COARSE",
  "ENC_AILERON_FINE",
  "ENC_RUDDER_COARSE",
  "ENC_RUDDER_FINE"
};

/* the order trim-control.ino lists them in, as indices into PROMPT */
static const uint8_t OUTPUT_ORDER[AXIS_COUNT] = { 0, 2, 4, 1, 3, 5 };

static uint8_t bound[AXIS_COUNT];

static void printAddr(uint8_t addr) {
  Serial.print(F("0x"));
  if (addr < 0x10) Serial.print('0');
  Serial.print(addr, HEX);
}

static uint8_t probe() {
  uint8_t count = 0;

  for (uint8_t i = 0; i < ADDR_COUNT; i++) {
    present[i] = enc[i].begin(FIRST_ADDR + i);
    taken[i]   = false;

    if (!present[i]) continue;

    count++;
    Serial.print(F("found encoder at "));
    printAddr(FIRST_ADDR + i);
    Serial.println();
  }

  Serial.println();

  return count;
}

static uint8_t waitForTurn() {
  for (uint8_t i = 0; i < ADDR_COUNT; i++)
    if (present[i] && !taken[i])
      base[i] = enc[i].getEncoderPosition();

  for (;;) {
    for (uint8_t i = 0; i < ADDR_COUNT; i++) {
      if (!present[i] || taken[i]) continue;

      int32_t moved = enc[i].getEncoderPosition() - base[i];
      if (moved < 0) moved = -moved;

      if (moved >= TURN_DETENTS) return i;
    }

    delay(POLL_MS);
  }
}

static void audit() {
  for (uint8_t step = 0; step < AXIS_COUNT; step++) {
    Serial.print(F("# turn the "));
    Serial.print(PROMPT[step]);
    Serial.println(F(" encoder"));

    uint8_t i = waitForTurn();

    taken[i]    = true;
    bound[step] = FIRST_ADDR + i;

    Serial.print(F("  "));
    Serial.print(PROMPT[step]);
    Serial.print(F(" is "));
    printAddr(bound[step]);
    Serial.println();

    delay(750);
  }
}

static void printDefines() {
  Serial.println();
  Serial.println(F("// ---- stick-trim-box/trim-control.ino: replace these lines ----"));
  Serial.println();
  for (uint8_t n = 0; n < AXIS_COUNT; n++) {
    uint8_t step = OUTPUT_ORDER[n];

    Serial.print(F("#define "));
    Serial.print(DEFINE_NAME[step]);

    for (uint8_t pad = strlen(DEFINE_NAME[step]); pad < 20; pad++)
      Serial.print(' ');

    printAddr(bound[step]);
    Serial.println();
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  while (!Serial)
    ;

  uint8_t count = probe();

  if (count < AXIS_COUNT) {
    Serial.print(F("only "));
    Serial.print(count);
    Serial.print(F(" of "));
    Serial.print(AXIS_COUNT);
    Serial.println();
    Serial.println(F(" encoders responded, cannot run the audit"));
    return;
  }

  audit();
  printDefines();
}

void loop() {
}
