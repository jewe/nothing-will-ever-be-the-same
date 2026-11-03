#include <EEPROM.h>
#include <NWEBTS.h>

/*
  NOTHING WILL EVER BE THE SAME
  Master_init

  Writes the default parameters (NWEBTS_DEFAULTS in libraries/NWEBTS/NWEBTS.h,
  settings Trier Sept 2026) into the EEPROM of the Master (Arduino Mega 2560)
  and reads them back.

  1. Upload this sketch, open the serial monitor (115200), check the table
  2. Upload Master
*/

StateParams p[NWEBTS_NUM_STATES];

uint16_t eeReadU16(int addr) {
  uint16_t v;
  EEPROM.get(addr, v);
  return v;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\nMaster_init: writing defaults to EEPROM"));

  nwebtsLoadDefaults(p);
  for (uint8_t s = NWEBTS_EEPROM_FIRST_STATE; s <= NWEBTS_LAST_STATE; s++) {
    int a = NWEBTS_EEPROM_ADDR(s);
    EEPROM.put(a + NWEBTS_EE_STEPS, p[s].steps);
    EEPROM.update(a + NWEBTS_EE_MODE, p[s].mode);
    EEPROM.put(a + NWEBTS_EE_VEL1, p[s].vel1);
    EEPROM.put(a + NWEBTS_EE_VEL2, p[s].vel2);
    EEPROM.put(a + NWEBTS_EE_DELAY, p[s].delay);
    EEPROM.put(a + NWEBTS_EE_RAND, p[s].randomDelay);
  }

  Serial.println(F("read back:"));
  Serial.println(F("  state      steps v1  v2  delay rand  check"));
  bool ok = true;
  for (uint8_t s = NWEBTS_EEPROM_FIRST_STATE; s <= NWEBTS_LAST_STATE; s++) {
    int a = NWEBTS_EEPROM_ADDR(s);
    uint16_t st = eeReadU16(a + NWEBTS_EE_STEPS);
    uint16_t v1 = eeReadU16(a + NWEBTS_EE_VEL1);
    uint16_t v2 = eeReadU16(a + NWEBTS_EE_VEL2);
    uint16_t dl = eeReadU16(a + NWEBTS_EE_DELAY);
    uint16_t rd = eeReadU16(a + NWEBTS_EE_RAND);
    bool same = st == p[s].steps && v1 == p[s].vel1 && v2 == p[s].vel2 && dl == p[s].delay && rd == p[s].randomDelay;
    ok = ok && same;
    char line[64];
    snprintf(line, sizeof(line), "  %-9s %5u %3u %3u %5u %4u  %s",
             state2Name(s), st, v1, v2, dl, rd, same ? "ok" : "MISMATCH");
    Serial.println(line);
  }
  Serial.println(ok ? F("DONE. Now upload Master.") : F("ERROR: EEPROM mismatch"));
}

void loop() {
}
