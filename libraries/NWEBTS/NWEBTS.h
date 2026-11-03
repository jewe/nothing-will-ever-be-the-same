/*
  NOTHING WILL EVER BE THE SAME
  Shared protocol, states and default parameters for Master, Master_init and Mini.

  Compile the sketches with:  --libraries ./libraries
*/
#ifndef NWEBTS_H
#define NWEBTS_H

#include <Arduino.h>
#include <avr/pgmspace.h>

////////////////////////////////////////////////////////////////////////////////
// Box states (the numeric values are part of the protocol and the EEPROM layout!)

enum States {
  STOPPED,    // 0
  INIT_0,     // 1  wait for master
  INIT_1,     // 2  drive down a bit
  INIT_2,     // 3  drive up until limit switch
  CAL_1,      // 4  calibration: fall slowly and count steps
  CAL_2,      // 5  calibration: report steps
  CAL_3,      // 6  calibration: wait (GUI uses 6 as help page)
  DOWN_1,     // 7
  DOWN_2,
  DOWN_3,
  DOWN_4,
  DOWN_5,     // 11
  DOWN_READY, // 12 wait for master
  UP_1,       // 13
  UP_2,
  UP_3,
  UP_4,
  UP_5,       // 17 repeated until limit switch
  RELAX,      // 18 drive down a few steps after the limit switch
  UP_READY,   // 19 wait for master
};
#define NWEBTS_NUM_STATES 20
#define NWEBTS_LAST_STATE UP_READY

////////////////////////////////////////////////////////////////////////////////
// CmdMessenger command ids

enum Commands {
  // Box to Master
  kAck,       // 0 ok
  kError,     // 1 error message
  kStatus,    // 2 state, lastState, sec since last state change, sec since limit switch
  kLog,       // 3 log message
  // Master to Box
  kReset,     // 4 software reset
  kState,     // 5 set state
  kGetStatus, // 6 query status
  kConfigure, // 7 state, mode, steps, vel1, vel2  (state 0 = global flags)
};

////////////////////////////////////////////////////////////////////////////////
// Step modes. Since v0.18 the box always runs half steps; the value is still
// transferred and stored for compatibility, but ignored.

enum StepModes {
  FULL,
  HALF,
  QUARTER,
  QUARTER_S,
  SIXTEENTH,
  SIXTEENTH_S,
};

////////////////////////////////////////////////////////////////////////////////
// Parameters per state

struct StateParams {
  uint8_t  mode;         // step mode (ignored by box v0.18+)
  uint16_t steps;        // full steps
  uint16_t vel1;         // start velocity (1..199)
  uint16_t vel2;         // end velocity   (1..199)
  uint16_t delay;        // ms, master only (DOWN_READY / UP_READY)
  uint16_t randomDelay;  // ms, master only (DOWN_READY / UP_READY)
};

// Defaults: settings "Einstellungen_NWEBTS_Sept_2026_Trier.xlsx", column Trier
// (column "Dresden" where Trier is empty).
//
// Exception UP_1 vel1: the sheet says 1. With Exp09 a velocity <= 3 overflowed
// delayMicroseconds() and produced a sawtooth speed (the stutter at the start
// of each lift). Executed correctly, V1=1 would make UP_1 take ~50 s.
// V1=4 reproduces the ~16 s UP_1 duration that was seen in Trier.
const StateParams NWEBTS_DEFAULTS[NWEBTS_NUM_STATES] PROGMEM = {
  //  mode       steps vel1 vel2 delay rand
  { HALF,          0,   0,   0,    0,  0 }, // STOPPED
  { HALF,          0,   0,   0,    0,  0 }, // INIT_0
  { HALF,        100,   5,   5,    0,  0 }, // INIT_1
  { HALF,        100,   5,   5,    0,  0 }, // INIT_2 (repeated until limit)
  { HALF,          0,   0,   0,    0,  0 }, // CAL_1
  { HALF,          0,   0,   0,    0,  0 }, // CAL_2
  { HALF,          0,   0,   0,    0,  0 }, // CAL_3
  { HALF,         30,  15,  80,    0,  0 }, // DOWN_1
  { HALF,        500,  80, 160,    0,  0 }, // DOWN_2
  { HALF,       1500, 160, 180,    0,  0 }, // DOWN_3
  { HALF,        480, 180, 199,    0,  0 }, // DOWN_4
  { HALF,        230, 150,   7,    0,  0 }, // DOWN_5  (199 -> 150 is blended by the box)
  { HALF,          0,   0,   0, 5000,  0 }, // DOWN_READY: pause on the ground
  { HALF,        900,   4,   9,    0,  0 }, // UP_1    (sheet: V1=1, see above)
  { HALF,        900,   9,  15,    0,  0 }, // UP_2
  { HALF,        600,  16,  30,    0,  0 }, // UP_3
  { HALF,        500,  32,  60,    0,  0 }, // UP_4
  { HALF,        150,  60,  21,    0,  0 }, // UP_5 (repeated until limit)
  { HALF,         10,   2,   2,    0,  0 }, // RELAX
  { HALF,          0,   0,   0, 1000,  0 }, // UP_READY
};

inline void nwebtsLoadDefaults(StateParams *p) {
  memcpy_P(p, NWEBTS_DEFAULTS, sizeof(NWEBTS_DEFAULTS));
}

////////////////////////////////////////////////////////////////////////////////
// EEPROM layout (Master), unchanged since 2018: states 7..19, 32 bytes each.
// Values are 16 bit little endian (compatible with EEPROMex readInt/writeInt).

#define NWEBTS_EEPROM_FIRST_STATE DOWN_1
#define NWEBTS_EEPROM_ADDR(state) (((state) - NWEBTS_EEPROM_FIRST_STATE) * 32)
#define NWEBTS_EE_STEPS 0
#define NWEBTS_EE_MODE  4
#define NWEBTS_EE_VEL1  8
#define NWEBTS_EE_VEL2  12
#define NWEBTS_EE_DELAY 16
#define NWEBTS_EE_RAND  20

////////////////////////////////////////////////////////////////////////////////
// Names (8 chars max for the LCD)

inline const char *state2Name(uint8_t s) {
  switch (s) {
    case STOPPED:    return "STOPPED";
    case INIT_0:     return "INIT_0";
    case INIT_1:     return "INIT_1";
    case INIT_2:     return "INIT_2";
    case CAL_1:      return "CAL_1";
    case CAL_2:      return "CAL_2";
    case CAL_3:      return "CAL_3";
    case DOWN_1:     return "DOWN_1";
    case DOWN_2:     return "DOWN_2";
    case DOWN_3:     return "DOWN_3";
    case DOWN_4:     return "DOWN_4";
    case DOWN_5:     return "DOWN_5";
    case DOWN_READY: return "DOWN_RDY";
    case UP_1:       return "UP_1";
    case UP_2:       return "UP_2";
    case UP_3:       return "UP_3";
    case UP_4:       return "UP_4";
    case UP_5:       return "UP_5";
    case RELAX:      return "RELAX";
    case UP_READY:   return "UP_READY";
  }
  return "?";
}

#endif
