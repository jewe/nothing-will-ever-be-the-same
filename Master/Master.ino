#include <CmdMessenger.h>
#include <SoftwareSerial.h>
#include <RBD_Timer.h>
#include <RBD_Button.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include <EEPROM.h>
#include <NWEBTS.h>

/*

  NOTHING WILL EVER BE THE SAME
  Master

  Arduino Mega 2560

  2018-2026 Jens Weber

  v0.16
  - all boxes get a new state at the same time (no more 1s offset per lost ack)
  - waits (delay / random delay) keep reading the boxes
  - fixes: RandDelay saved + editable, UP_READY loaded from EEPROM,
    no EEPROM writes at INIT (wrote to address -32), STOP not sent in a loop
  - EEPROM.h instead of EEPROMex (same layout)
  - shared states / commands / defaults in libraries/NWEBTS

  BENCH 1: one box on Serial2, debug on USB (115200), no LCD / knob / buttons.
*/


////////////////////////////////////////////////////////////////////////////////
// CONFIG

#ifndef BENCH
#define BENCH 0
#endif

String version = "0.16";
#define WATCHDOG_TIMEOUT 120000 // ms without phase change in RUNNING -> INIT
#define ACK_TIMEOUT 300         // ms
#define VERBOSE true            // log more details

#if BENCH
#define NUM_BOXES 1
#else
#define NUM_BOXES 4
#endif

// Serials
#if BENCH
Stream *debugSerial = &Serial; // USB
#else
SoftwareSerial softSerial(13, 12); // RX, TX // for debugging
Stream *debugSerial = &softSerial;
#endif
CmdMessenger cmdMessenger1 = CmdMessenger(Serial2);
CmdMessenger cmdMessenger2 = CmdMessenger(Serial3);
CmdMessenger cmdMessenger3 = CmdMessenger(Serial1);
CmdMessenger cmdMessenger4 = CmdMessenger(Serial);

#define STATE_UNKNOWN 255

StateParams stateParams[NWEBTS_NUM_STATES];
bool boxVerbose = BENCH; // sent to boxes as config state 0


////////////////////////////////////////////////////////////////////////////////
// GUI

#if BENCH
// no display on the bench
class NullLcd {
  public:
  void begin() {}
  void backlight() {}
  void clear() {}
  void createChar(uint8_t, uint8_t *) {}
  void setCursor(uint8_t, uint8_t) {}
  void write(uint8_t) {}
  template <class T> void print(T) {}
};
NullLcd lcd;
#else
// Set the LCD address to 0x27 for a 20 chars and 4 line display
LiquidCrystal_I2C lcd(0x27, 20, 4);
#endif
Encoder knob(3, 2);

enum {
  SELECT_STATE,
  SELECT_PROPERTY,
  SELECT_VALUE
};

enum {
  PROPERTY_STEPS,
  PROPERTY_MODE,  // not selectable: box v0.18 always uses half steps
  PROPERTY_V1,
  PROPERTY_V2,
  PROPERTY_DELAY,
  PROPERTY_RANDDELAY
};
int8_t numProperties = PROPERTY_RANDDELAY; // last property

#define GUI_FIRST_STATE CAL_3 // shown as help page
int8_t guiState = GUI_FIRST_STATE;
int16_t value = -1;
int8_t action = 0;
int8_t selected = SELECT_STATE;
int8_t selectedProperty = 0;

// custom chars for liquid cristal
byte customCharArrow1[] = { B10000, B11000, B11100, B11110, B11100, B11000, B10000, B00000 };
byte customCharArrow2[] = { B00001, B00011, B00111, B01111, B00111, B00011, B00001, B00000 };
byte customCharArrow3[] = { B00100, B01110, B10101, B00100, B00100, B10101, B01110, B00100 };

RBD::Button modeBtn(6);
RBD::Button backBtn(4);
RBD::Button okBtn(5);


////////////////////////////////////////////////////////////////////////////////
// BOX

class Box {

  public:

  uint8_t index; // 1..4
  uint8_t state = STATE_UNKNOWN;
  uint8_t lastState;
  float lastStateChange; // sec
  float stateTime;       // sec
  CmdMessenger *cmdMessenger; // CmdMessenger needs to be declared outside of Box

  unsigned long setStateTimeout = 0;
  bool isConfigured = false;
  bool displayNewState = false;
  bool acked = false;

  void setup(uint8_t _index, CmdMessenger *_cmdMessenger) { // 1..4
    index = _index;
    cmdMessenger = _cmdMessenger;
    cmdMessenger->printLfCr();
    cmdMessenger->attach(onUnknown);
    requestState();
  }

  void loop() {
    cmdMessenger->feedinSerialData();
  }

  static void onUnknown() {
    debugSerial->println(F("Error: Unknown command received."));
  }

  void printPrefix() {
    debugSerial->print(F("<Box"));
    debugSerial->print(index);
    debugSerial->print(F(" "));
    debugSerial->print(millis() / 1000.0, 1);
    debugSerial->print(F("s> "));
  }

  void receiveStatus() {
    state = cmdMessenger->readInt16Arg();
    lastState = cmdMessenger->readInt16Arg();
    lastStateChange = cmdMessenger->readFloatArg();
    stateTime = cmdMessenger->readFloatArg();

    printPrefix();
    debugSerial->print(state2Name(state));
    if (VERBOSE) {
      debugSerial->print(F(" \tlast "));
      debugSerial->print(state2Name(lastState));
      debugSerial->print(F(" "));
      debugSerial->print(lastStateChange);
      debugSerial->print(F("s"));
    }
    debugSerial->println();
    displayNewState = true;
  }

  void receiveAck() {
    acked = true;
  }

  void requestState() {
    cmdMessenger->sendCmd(kGetStatus);
  }

  // send without waiting (see setAllBoxesToState)
  void sendState(uint8_t _state) {
    acked = false;
    cmdMessenger->sendCmd(kState, _state);
  }

  // blocking retry
  bool sendStateBlocking(uint8_t _state) {
    acked = cmdMessenger->sendCmd(kState, _state, true, kAck, ACK_TIMEOUT);
    return acked;
  }

  void receiveLog() {
    String msg = cmdMessenger->readStringArg();
    printPrefix();
    debugSerial->print(F("LOG  "));
    debugSerial->println(msg);
  }

  void receiveError() {
    String msg = cmdMessenger->readStringArg();
    printPrefix();
    debugSerial->print(F("ERROR  "));
    debugSerial->println(msg);
  }

  void reset() {
    cmdMessenger->sendCmd(kReset);
  }

  void configure(StateParams &p, uint8_t _state) {
    cmdMessenger->sendCmdStart(kConfigure);
    cmdMessenger->sendCmdArg(_state);
    cmdMessenger->sendCmdArg(p.mode);
    cmdMessenger->sendCmdArg(p.steps);
    cmdMessenger->sendCmdArg(p.vel1);
    cmdMessenger->sendCmdArg(p.vel2);
    bool ack = cmdMessenger->sendCmdEnd(true, kAck, 1000);
    printPrefix();
    debugSerial->print(F("conf "));
    debugSerial->print(state2Name(_state));
    debugSerial->println(ack ? F(" ok") : F(" FAILED"));
    isConfigured = ack;
  }
};


////////////////////////////////////////////////////////////////////////////////
// INTERNAL CONFIG

enum Modes {
  STOP,
  INIT,
  CALIBRATION,
  CONFIG,
  RUNNING,
};

uint8_t mode;
Box box[NUM_BOXES];
unsigned long watchdog;


////////////////////////////////////////////////////////////////////////////////
// SETUP

void setup() {
  delay(100);

#if BENCH
  Serial.begin(115200);
#else
  softSerial.begin(115200);
  Serial.begin(38400);
  Serial1.begin(38400);
  Serial3.begin(38400);
#endif
  Serial2.begin(38400);
  debugSerial->println("<MASTER> Setup started. Version " + version + (BENCH ? " BENCH" : ""));

  // initialize the LCD
  lcd.begin();
  lcd.backlight();
  lcd.createChar(0, customCharArrow1);
  lcd.createChar(1, customCharArrow2);
  lcd.createChar(2, customCharArrow3);

  // setup boxes
  CmdMessenger *messengers[4] = { &cmdMessenger1, &cmdMessenger2, &cmdMessenger3, &cmdMessenger4 };
  for (uint8_t i = 0; i < NUM_BOXES; i++) box[i].setup(i + 1, messengers[i]);
  attachBoxCallbacks();

  nwebtsLoadDefaults(stateParams); // defaults for 0..last
  loadStateParamsFromEEPROM();     // EEPROM overrides DOWN_1..UP_READY
  printParams();

  randomSeed(analogRead(0));

  debugSerial->println(F("<MASTER> Setup finished. Press h for help."));

  displayStep01();
  setMode(INIT);
}


////////////////////////////////////////////////////////////////////////////////
// THE UGLY PART: COMMAND HANDLER FOR EACH BOX
// (CmdMessenger callbacks have to be static functions)

void receiveStatus1() { box[0].receiveStatus(); }
void receiveLog1()    { box[0].receiveLog(); }
void receiveError1()  { box[0].receiveError(); }
void receiveAck1()    { box[0].receiveAck(); }
#if NUM_BOXES > 1
void receiveStatus2() { box[1].receiveStatus(); }
void receiveLog2()    { box[1].receiveLog(); }
void receiveError2()  { box[1].receiveError(); }
void receiveAck2()    { box[1].receiveAck(); }
void receiveStatus3() { box[2].receiveStatus(); }
void receiveLog3()    { box[2].receiveLog(); }
void receiveError3()  { box[2].receiveError(); }
void receiveAck3()    { box[2].receiveAck(); }
void receiveStatus4() { box[3].receiveStatus(); }
void receiveLog4()    { box[3].receiveLog(); }
void receiveError4()  { box[3].receiveError(); }
void receiveAck4()    { box[3].receiveAck(); }
#endif

void attachBoxCallbacks() {
  cmdMessenger1.attach(kLog, receiveLog1);
  cmdMessenger1.attach(kStatus, receiveStatus1);
  cmdMessenger1.attach(kError, receiveError1);
  cmdMessenger1.attach(kAck, receiveAck1);
#if NUM_BOXES > 1
  cmdMessenger2.attach(kLog, receiveLog2);
  cmdMessenger2.attach(kStatus, receiveStatus2);
  cmdMessenger2.attach(kError, receiveError2);
  cmdMessenger2.attach(kAck, receiveAck2);
  cmdMessenger3.attach(kLog, receiveLog3);
  cmdMessenger3.attach(kStatus, receiveStatus3);
  cmdMessenger3.attach(kError, receiveError3);
  cmdMessenger3.attach(kAck, receiveAck3);
  cmdMessenger4.attach(kLog, receiveLog4);
  cmdMessenger4.attach(kStatus, receiveStatus4);
  cmdMessenger4.attach(kError, receiveError4);
  cmdMessenger4.attach(kAck, receiveAck4);
#endif
}


////////////////////////////////////////////////////////////////////////////////
// LOOP

void pollBoxes() {
  for (uint8_t i = 0; i < NUM_BOXES; i++) box[i].loop();
}

// like delay(), but keeps reading the boxes
void waitPolling(unsigned long ms) {
  unsigned long t = millis();
  while (millis() - t < ms) pollBoxes();
}

void loop() {

  pollBoxes();
  checkWatchdog();
  readDebugSerial();

  switch (mode) {

    case STOP:
      break;

    case INIT:
      if (allBoxesInState(INIT_0)) {
        configureAllBoxes(); // and start INIT_1
      } else if (allBoxesInState(UP_READY)) {
        setMode(RUNNING);
      }
      break;

    case RUNNING:
      if (allBoxesInState(UP_READY)) {
        debugSerial->println(F("\n<MASTER> ------- DOWN ------"));
        waitPolling(stateParams[UP_READY].delay);
        if (stateParams[UP_READY].randomDelay > 0) waitPolling(random(stateParams[UP_READY].randomDelay));
        setAllBoxesToState(DOWN_1);
      } else if (allBoxesInState(DOWN_READY)) {
        debugSerial->println(F("\n<MASTER> -------  UP  ------"));
        waitPolling(stateParams[DOWN_READY].delay);
        if (stateParams[DOWN_READY].randomDelay > 0) waitPolling(random(stateParams[DOWN_READY].randomDelay));
        setAllBoxesToState(UP_1);
      }
      break;

    case CALIBRATION:
    case CONFIG:
      break;
  }

#if !BENCH
  // read knob
  action = readKnob();
  if (action != 0 && mode == CONFIG) {
    configUpdateDisplay();
  }

  // buttons
  if (okBtn.onPressed()) {
    if (mode == CONFIG) {
      // STATE -> PROPERTY -> VALUE -> PROPERTY
      if (selected == SELECT_STATE) {
        selected = SELECT_PROPERTY;
        selectedProperty = 0;
        action = 0;
      } else if (selected == SELECT_PROPERTY) {
        selected = SELECT_VALUE;
      } else if (selected == SELECT_VALUE) {
        selected = SELECT_PROPERTY;
        saveValue();
      }
      configUpdateDisplay();
    }
  }

  if (backBtn.onPressed()) {
    if (mode == CONFIG) {
      // VALUE -> PROPERTY -> STATE
      if (selected == SELECT_PROPERTY) {
        selected = SELECT_STATE;
      } else if (selected == SELECT_VALUE) {
        selected = SELECT_PROPERTY;
      }
      action = 0;
      configUpdateDisplay();
    } else if (mode == CALIBRATION) {
      setAllBoxesToState(CAL_2);
    }
  }

  if (modeBtn.onPressed()) {
    if (mode == CONFIG) {
      setMode(RUNNING);
    } else {
      setMode(CONFIG);
    }
    action = 0;
    configUpdateDisplay();
  }
#endif

  if (mode == RUNNING) updateDisplay();
}


////////////////////////////////////////////////////////////////////////////////
// HELPER

boolean allBoxesInState(uint8_t state) {
  for (uint8_t i = 0; i < NUM_BOXES; i++) {
    if (box[i].state != state) return false;
  }
  return true;
}

void setMode(uint8_t _mode) {
  mode = _mode;
  watchdog = millis();

  lcd.clear();
  debugSerial->print(F("\n<MASTER> mode: "));
  debugSerial->println(mode2Name(mode));
  if (mode != CONFIG) displayModeUpdate();

  if (mode == STOP) {
    setAllBoxesToState(STOPPED);
  } else if (mode == INIT) {
    setAllBoxesToState(INIT_0);
    lcd.setCursor(0, 3);
    lcd.print("V " + version);
  } else if (mode == CALIBRATION) {
    if (allBoxesInState(UP_READY)) {
      setAllBoxesToState(CAL_1);
      lcd.setCursor(0, 3);
      lcd.print(F("Press Back to stop "));
      debugSerial->println(F("send C to stop"));
    } else {
      debugSerial->println(F("<MASTER> calibration needs all boxes in UP_READY (run INIT first)"));
      lcd.setCursor(0, 3);
      lcd.print(F("Needs UP_READY"));
    }
  }
}

// Send the new state to all boxes at once, then collect the acks.
// The UARTs transmit in parallel, so all boxes start within ~1ms.
void setAllBoxesToState(uint8_t state) {
  debugSerial->print(F("<MASTER> all to "));
  debugSerial->println(state2Name(state));
  watchdog = millis();

  for (uint8_t i = 0; i < NUM_BOXES; i++) box[i].sendState(state);

  unsigned long t = millis();
  bool all = false;
  while (!all && millis() - t < ACK_TIMEOUT) {
    pollBoxes();
    all = true;
    for (uint8_t i = 0; i < NUM_BOXES; i++) all = all && box[i].acked;
  }

  for (uint8_t i = 0; i < NUM_BOXES; i++) {
    if (!box[i].acked) {
      box[i].printPrefix();
      debugSerial->println(F("no ack -> retry"));
      box[i].sendStateBlocking(state);
    }
    if (box[i].acked) {
      box[i].state = state; // the box reports its real state right after the ack
      box[i].setStateTimeout = 0;
    } else {
      box[i].printPrefix();
      debugSerial->print(F("Timeout setState "));
      debugSerial->println(state2Name(state));
      box[i].setStateTimeout = millis();
      box[i].displayNewState = true;
    }
  }
}

void configureAllBoxes() {
  debugSerial->println(F("<MASTER> configureAllBoxes start"));

  // state 0: global options (RAM only)
  stateParams[0].mode = boxVerbose ? 1 : 0;
  stateParams[0].steps = 0;
  stateParams[0].vel1 = 0;
  stateParams[0].vel2 = 0;

  for (uint8_t i = 0; i < NUM_BOXES; i++) {
    for (uint8_t s = 0; s < NWEBTS_NUM_STATES; s++) {
      if (s == 0 || stateParams[s].steps > 0) box[i].configure(stateParams[s], s);
    }
  }
  debugSerial->println(F("<MASTER> configureAllBoxes end"));
  setAllBoxesToState(INIT_1);
}

void configureAllBoxesOptions() {
  stateParams[0].mode = boxVerbose ? 1 : 0;
  for (uint8_t i = 0; i < NUM_BOXES; i++) box[i].configure(stateParams[0], 0);
}

void checkWatchdog() {
  if (millis() - watchdog > WATCHDOG_TIMEOUT) {
    watchdog = millis();
    if (mode == RUNNING) {
      debugSerial->println(F("<MASTER> Watchdog: no progress -> INIT"));
      setMode(INIT);
    }
  }
}

void printHelp() {
  debugSerial->println(F("Keys: v/V box verbose on/off, i INIT, g RUNNING, s STOP,"));
  debugSerial->println(F("      c calibration start, C calibration stop, p print params,"));
  debugSerial->println(F("      d load Trier defaults into RAM, w write RAM params to EEPROM, ? status"));
}

void readDebugSerial() {
  if (!debugSerial->available()) return;
  char inByte = debugSerial->read();
  if (inByte == '\r' || inByte == '\n') return;
  debugSerial->print(F("CMD "));
  debugSerial->println(inByte);

  if (inByte == 'v') {
    boxVerbose = true;
    configureAllBoxesOptions();
  } else if (inByte == 'V') {
    boxVerbose = false;
    configureAllBoxesOptions();
  } else if (inByte == 'c') {
    setMode(CALIBRATION);
  } else if (inByte == 'C') {
    setAllBoxesToState(CAL_2);
  } else if (inByte == 's') {
    setMode(STOP);
  } else if (inByte == 'i') {
    setMode(INIT);
  } else if (inByte == 'g') {
    setMode(RUNNING);
  } else if (inByte == 'p') {
    printParams();
  } else if (inByte == 'd') {
    nwebtsLoadDefaults(stateParams);
    debugSerial->println(F("defaults loaded (RAM). w = save, i = send to boxes"));
  } else if (inByte == 'w') {
    saveAllParamsToEEPROM();
    debugSerial->println(F("saved to EEPROM"));
  } else if (inByte == '?') {
    for (uint8_t i = 0; i < NUM_BOXES; i++) box[i].requestState();
  } else {
    printHelp();
  }
}

void printParams() {
  debugSerial->println(F("<MASTER> state      steps v1  v2  delay rand"));
  for (uint8_t s = INIT_1; s < NWEBTS_NUM_STATES; s++) {
    if (s == INIT_0 || s == CAL_1 || s == CAL_2 || s == CAL_3) continue;
    StateParams &p = stateParams[s];
    char line[60];
    snprintf(line, sizeof(line), "  %-9s %5u %3u %3u %5u %4u",
             state2Name(s), p.steps, p.vel1, p.vel2, p.delay, p.randomDelay);
    debugSerial->println(line);
  }
}

const char *mode2Name(uint8_t mode) {
  if (mode == STOP) return "STOP";
  if (mode == INIT) return "INIT";
  if (mode == RUNNING) return "RUNNING";
  if (mode == CALIBRATION) return "CALIBRATION";
  if (mode == CONFIG) return "CONFIGURATION";
  return "unknown";
}


////////////////////////////////////////////////////////////////////////////////
// GUI

int8_t readKnob() {
  long knobValue = knob.read();
  int8_t action = 0;
  if (knobValue > 1) {
    action = -1;
    knob.write(0);
  } else if (knobValue < -1) {
    action = 1;
    knob.write(0);
  }
  return action;
}

void configUpdateDisplay() {

  if (selected == SELECT_STATE) {
    guiState += action;
    if (guiState < GUI_FIRST_STATE) guiState = NWEBTS_LAST_STATE;
    else if (guiState > NWEBTS_LAST_STATE) guiState = GUI_FIRST_STATE;
  }

  if (selected == SELECT_PROPERTY) {
    selectedProperty += action;
    if (selectedProperty == PROPERTY_MODE) selectedProperty += action != 0 ? action : 1; // skip MO
    if (selectedProperty < 0) selectedProperty = numProperties;
    else if (selectedProperty > numProperties) selectedProperty = 0;

    // load property into "value"
    if (selectedProperty == PROPERTY_STEPS) value = stateParams[guiState].steps;
    else if (selectedProperty == PROPERTY_V1) value = stateParams[guiState].vel1;
    else if (selectedProperty == PROPERTY_V2) value = stateParams[guiState].vel2;
    else if (selectedProperty == PROPERTY_DELAY) value = stateParams[guiState].delay;
    else if (selectedProperty == PROPERTY_RANDDELAY) value = stateParams[guiState].randomDelay;
  }

  if (selected == SELECT_VALUE) {
    if (selectedProperty == PROPERTY_STEPS) {
      value += action * 5;
      if (value < 0) value = 0;
      else if (value > 9999) value = 9999;
    }
    else if (selectedProperty == PROPERTY_V1 || selectedProperty == PROPERTY_V2) {
      value += action;
      if (value < 1) value = 1;
      else if (value > 199) value = 199;
    }
    else if (selectedProperty == PROPERTY_DELAY || selectedProperty == PROPERTY_RANDDELAY) {
      value += action * 100;
      if (value < 0) value = 0;
      else if (value > 9999) value = 9999;
    }
  }

  // STATE
  lcd.setCursor(0, 0);
  if (selected == SELECT_STATE) lcd.write(0);
  else lcd.print(" ");
  lcdPrint(state2Name(guiState), 1, 0, 9);
  lcdPrint((String)(guiState - 5), 18, 0, 2); // state ID

  writeProperty(0, 1, PROPERTY_STEPS, "ST", "Steps", (String)stateParams[guiState].steps);
  writeProperty(10, 1, PROPERTY_MODE, "MO", "StepMode", "1/2");
  writeProperty(0, 2, PROPERTY_V1, "V1", "Vel Start", (String)stateParams[guiState].vel1);
  writeProperty(10, 2, PROPERTY_V2, "V2", "Vel End", (String)stateParams[guiState].vel2);
  if (guiState == DOWN_READY || guiState == UP_READY || guiState == GUI_FIRST_STATE) {
    writeProperty(0, 3, PROPERTY_DELAY, "DL", "Delay", (String)stateParams[guiState].delay);
    writeProperty(10, 3, PROPERTY_RANDDELAY, "RD", "RandDelay", (String)stateParams[guiState].randomDelay);
  } else {
    lcdPrint("", 0, 3, 10);
    lcdPrint("", 10, 3, 10);
  }
}

void writeProperty(uint8_t x, uint8_t y, uint8_t prop, String labelShort, String label, String _value) {
  if (guiState != GUI_FIRST_STATE) {
    // display short label + value
    lcd.setCursor(x, y);
    if (selected == SELECT_PROPERTY && selectedProperty == prop) lcd.write(0);
    else lcd.print(" ");

    lcdPrint(labelShort, x + 1, y, 2);
    if (selected == SELECT_VALUE && selectedProperty == prop) lcdPrint((String)value, x + 4, y, 6); // temp value
    else lcdPrint(_value, x + 4, y, 6);

    lcd.setCursor(x + 3, y);
    if (selected == SELECT_VALUE && selectedProperty == prop) lcd.write(2);
    else lcd.print(" ");
  } else {
    // help: display long labels
    lcdPrint(label, x + 1, y, 9);
  }
}

void lcdPrint(String s, uint8_t x, uint8_t y, uint8_t l) {
  lcd.setCursor(x, y);
  for (uint8_t i = 0; i < l; i++) lcd.print(" ");
  lcd.setCursor(x, y);
  lcd.print(s.substring(0, l));
}

void saveValue() {
  StateParams &p = stateParams[guiState];
  if (selectedProperty == PROPERTY_STEPS) p.steps = value;
  else if (selectedProperty == PROPERTY_V1) p.vel1 = value;
  else if (selectedProperty == PROPERTY_V2) p.vel2 = value;
  else if (selectedProperty == PROPERTY_DELAY) p.delay = value;
  else if (selectedProperty == PROPERTY_RANDDELAY) p.randomDelay = value;
  else return;
  saveParamToEEPROM(guiState, selectedProperty);
}

void displayModeUpdate() {
  lcd.setCursor(0, 0);
  lcd.print("                    ");
  lcd.setCursor(0, 0);
  lcd.print(String("Mode ") + mode2Name(mode));
}

void updateDisplay() {
  for (uint8_t i = 0; i < NUM_BOXES; i++) {
    if (box[i].displayNewState) {
      box[i].displayNewState = false;
      if (box[i].setStateTimeout != 0) _displayBox(i, "Timeout");
      else _displayBox(i, state2Name(box[i].state));
    }
  }
}

void _displayBox(uint8_t index, const char *s) {
  uint8_t x = (index % 2) * 10;
  uint8_t y = 1 + index / 2;
  lcdPrint(s, x, y, 10);
}


////////////////////////////////////////////////////////////////////////////////
// EEPROM (layout see NWEBTS.h, compatible with v0.15 / EEPROMex)

void displayStep01() {
  lcd.clear();
  lcd.setCursor(0, 3);
  lcd.print("V " + version);
}

uint16_t eeReadU16(int addr) {
  uint16_t v;
  EEPROM.get(addr, v);
  return v;
}

void loadStateParamsFromEEPROM() {
  for (uint8_t i = NWEBTS_EEPROM_FIRST_STATE; i <= NWEBTS_LAST_STATE; i++) {
    int a = NWEBTS_EEPROM_ADDR(i);
    StateParams p;
    p.steps = eeReadU16(a + NWEBTS_EE_STEPS);
    p.mode = EEPROM.read(a + NWEBTS_EE_MODE);
    p.vel1 = eeReadU16(a + NWEBTS_EE_VEL1);
    p.vel2 = eeReadU16(a + NWEBTS_EE_VEL2);
    p.delay = eeReadU16(a + NWEBTS_EE_DELAY);
    p.randomDelay = eeReadU16(a + NWEBTS_EE_RAND);
    // empty / foreign EEPROM (0xFFFF): keep the defaults
    if (p.steps > 9999 || p.vel1 > 199 || p.vel2 > 199 || p.delay > 9999 || p.randomDelay > 9999) {
      debugSerial->print(F("<MASTER> EEPROM invalid for "));
      debugSerial->print(state2Name(i));
      debugSerial->println(F(" -> default"));
      continue;
    }
    stateParams[i] = p;
  }
}

void saveAllParamsToEEPROM() {
  for (uint8_t i = NWEBTS_EEPROM_FIRST_STATE; i <= NWEBTS_LAST_STATE; i++) {
    for (uint8_t j = PROPERTY_STEPS; j <= PROPERTY_RANDDELAY; j++) saveParamToEEPROM(i, j);
  }
}

void saveParamToEEPROM(uint8_t state, uint8_t param) {
  if (state < NWEBTS_EEPROM_FIRST_STATE || state > NWEBTS_LAST_STATE) return;
  int a = NWEBTS_EEPROM_ADDR(state);
  StateParams &p = stateParams[state];
  if (param == PROPERTY_STEPS) EEPROM.put(a + NWEBTS_EE_STEPS, p.steps);
  else if (param == PROPERTY_MODE) EEPROM.update(a + NWEBTS_EE_MODE, p.mode);
  else if (param == PROPERTY_V1) EEPROM.put(a + NWEBTS_EE_VEL1, p.vel1);
  else if (param == PROPERTY_V2) EEPROM.put(a + NWEBTS_EE_VEL2, p.vel2);
  else if (param == PROPERTY_DELAY) EEPROM.put(a + NWEBTS_EE_DELAY, p.delay);
  else if (param == PROPERTY_RANDDELAY) EEPROM.put(a + NWEBTS_EE_RAND, p.randomDelay);
}
