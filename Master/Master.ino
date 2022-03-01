#include <CmdMessenger.h>
#include <SoftwareSerial.h>
#include <RBD_Timer.h>
#include <RBD_Button.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include <EEPROMex.h>

/*

  NOTHING WILL EVER BE THE SAME
  Master

  Arduino Mega 2560

  2018 Jens Weber

*/




////////////////////////////////////////////////////////////////////////////////
// CONFIG

String version = "0.14";
#define WATCHDOG_TIMEOUT 60000 // ms
#define NUM_BOXES 4
#define VERBOSE true // log more details


// Serials
SoftwareSerial softSerial(13, 12); // RX, TX // for debugging
CmdMessenger cmdMessenger1 = CmdMessenger(Serial2);
CmdMessenger cmdMessenger2 = CmdMessenger(Serial3);
CmdMessenger cmdMessenger3 = CmdMessenger(Serial1);
CmdMessenger cmdMessenger4 = CmdMessenger(Serial);

Stream *debugSerial = &softSerial; // how to debug

// states for boxes
enum States{
  STOPPED,
  INIT_0,   // 1
  INIT_1,
  INIT_2,

  CAL_1,
  CAL_2,
  CAL_3,

  DOWN_1, // 6
  DOWN_2,
  DOWN_3,
  DOWN_4,
  DOWN_5,
  DOWN_READY,

  UP_1,
  UP_2,
  UP_3,
  UP_4,
  UP_5,
  RELAX,
  UP_READY,
};
int8_t numStates = UP_READY; // insert here the last state const

// stepper modes
enum {
  FULL,
  HALF,
  QUARTER,
  QUARTER_S,
  SIXTEENTH,
  SIXTEENTH_S,
};

struct boxState{
  uint8_t mode = SIXTEENTH; // full / half ...
  uint16_t steps; // fullsteps!
  uint16_t vel1; // start velocity
  uint16_t vel2; // end velocity
  uint16_t delay = 0; // only master
  uint8_t randomDelay = 0;
};
boxState stateParams[20];


String state2Name(uint8_t state){
  String str = "?";
  if (state == STOPPED) str =         "STOPPED";
  else if (state == INIT_0) str =     "INIT_0 ";
  else if (state == INIT_1) str =     "INIT_1 ";
  else if (state == INIT_2) str =     "INIT_2 ";
  else if (state == CAL_1) str =      "CAL_1  ";
  else if (state == CAL_2) str =      "CAL_2  ";
  else if (state == CAL_3) str =      "CAL_3  "; 
  else if (state == DOWN_1) str =     "DOWN_1 ";
  else if (state == DOWN_2) str =     "DOWN_2 ";
  else if (state == DOWN_3) str =     "DOWN_3 ";
  else if (state == DOWN_4) str =     "DOWN_4 ";
  else if (state == DOWN_5) str =     "DOWN_5 ";
  else if (state == DOWN_READY) str = "DOWN_RDY";
  else if (state == UP_1) str =       "UP_1   ";
  else if (state == UP_2) str =       "UP_2   ";
  else if (state == UP_3) str =       "UP_3   ";
  else if (state == UP_4) str =       "UP_4   ";
  else if (state == UP_5) str =       "UP_5   ";
  else if (state == RELAX) str =      "RELAX  ";
  else if (state == UP_READY) str =   "UP_READY";
  return str;
}



////////////////////////////////////////////////////////////////////////////////
// GUI

// Set the LCD address to 0x27 for a 20 chars and 4 line display
LiquidCrystal_I2C lcd(0x27, 20, 4);
Encoder knob(3, 2);

long knobValue;

enum {
  SELECT_STATE,
  SELECT_PROPERTY,
  SELECT_VALUE
};

enum {
  PROPERTY_STEPS,
  PROPERTY_MODE,
  PROPERTY_V1,
  PROPERTY_V2,
  PROPERTY_DELAY,
  PROPERTY_RANDDELAY
};
int8_t numProperties = PROPERTY_DELAY; // insert last here

// GUI
int8_t state = 6;
int8_t property = -1;
int16_t value = -1;
int8_t action = 0;
int8_t selected = SELECT_STATE;
int8_t selectedProperty = 0;

// custom chars for liquid cristal
byte customCharArrow1[] = {
  B10000,
  B11000,
  B11100,
  B11110,
  B11100,
  B11000,
  B10000,
  B00000
};
byte customCharArrow2[] = {
  B00001,
  B00011,
  B00111,
  B01111,
  B00111,
  B00011,
  B00001,
  B00000
};
byte customCharArrow3[] = {
  B00100,
  B01110,
  B10101,
  B00100,
  B00100,
  B10101,
  B01110,
  B00100
};

RBD::Button modeBtn(6);
RBD::Button backBtn(4);
RBD::Button okBtn(5);


////////////////////////////////////////////////////////////////////////////////
// BOX

class Box {

  public:

  uint8_t index; // 1..4
  uint8_t state;
  uint8_t lastState;
  float lastStateChange; // sec
  float limitSwitchTimer; // sec / last activated
  CmdMessenger *cmdMessenger; // Pointer: CmdMessenger needs to be declared outside of Box

  #define VERBOSE true // log more details
  uint16_t setStateTimeout = 0;
  bool isConfigured = false;
  bool displayNewState = false;

  // This is the list of recognized commands. These can be commands that can either be sent or received.
  enum
  {
    // COMMANDS
    // Box to Master
    kAck, // receive ok from box
    kError, // receive error
    kStatus,   // receive state
    kLog, // receive log

    // Master to Box
    kReset, // reset box from master
    kState,   // set state from master
    kGetStatus,   // query status from master
    kConfigure, // configure box
  };

  Box(){
    state = 1000;
  }

  void setup(uint8_t _index, CmdMessenger *_cmdMessenger){ // 1..4
    index = _index;
    cmdMessenger = _cmdMessenger;
    cmdMessenger->printLfCr();
    cmdMessenger->attach(onError);
    requestState();
  }

  void loop(){
    cmdMessenger->feedinSerialData();
  }


  // command handler
  static void onError(){
    debugSerial->println("Error: Unknown command received.");
  }

  // Set received status in master
  void receiveStatus(){

    state = cmdMessenger->readInt16Arg();
    lastState = cmdMessenger->readInt16Arg();
    lastStateChange = cmdMessenger->readFloatArg();
    limitSwitchTimer = cmdMessenger->readFloatArg();

    debugSerial->print("<Box");
    debugSerial->print(index);
    debugSerial->print(">  ");
    debugSerial->print( state2Name(state) );
    
    if (VERBOSE) {
      debugSerial->print(" \tlast ");
      debugSerial->print( state2Name(lastState) );
      debugSerial->print(" \tlastChange ");
      debugSerial->print(lastStateChange);
      debugSerial->print("s \tlimitSwitch ");
      debugSerial->print(limitSwitchTimer);
      debugSerial->print("s");
    }
    debugSerial->println(" ");
    displayNewState = true;
  }

  void setState(uint8_t _state){
    state = _state;
    debugSerial->print("<Box");
    debugSerial->print(index);
    debugSerial->print("> setState ");
    debugSerial->println(_state);
  }

  void requestState(){
    cmdMessenger->sendCmd(kGetStatus); // kGetStatus
  }

  void setRemoteState(uint8_t _state){

    // debugSerial->print(kState);
    // debugSerial->print(" setRemoteState ");
    // debugSerial->println(_state);

    int ack = cmdMessenger->sendCmd(kState, _state, true, kAck, 1000);
    if (ack == 0) {
      debugSerial->print("<Box");
      debugSerial->print(index);
      debugSerial->print(">  Timeout  ");
      debugSerial->println("setRemoteState");
      setStateTimeout = millis();
    } else {
      setStateTimeout = 0;
    }
  }

  void receiveLog(){
    String msg = cmdMessenger->readStringArg();
    debugSerial->print("<Box");
    debugSerial->print(index);
    debugSerial->print(">  LOG  ");
    debugSerial->println(msg);
  }

  void receiveError(){
    String msg = cmdMessenger->readStringArg();
    debugSerial->print("<Box");
    debugSerial->print(index);
    debugSerial->print(">  ERROR  ");
    debugSerial->println(msg);
  }

  void reset(){
    cmdMessenger->sendCmd(kReset);
  }


  void configure(boxState _cstate, uint8_t _state){
    cmdMessenger->sendCmdStart(kConfigure);
    cmdMessenger->sendCmdArg(_state);
    cmdMessenger->sendCmdArg(_cstate.mode);
    cmdMessenger->sendCmdArg(_cstate.steps);
    cmdMessenger->sendCmdArg(_cstate.vel1);
    cmdMessenger->sendCmdArg(_cstate.vel2);
    int ack = cmdMessenger->sendCmdEnd(true, kAck, 1000);
    debugSerial->print("<Box");
    debugSerial->print(index);
    debugSerial->println("> conf state " + (String)_state + ": " + (ack==1 ? "ok" : "failed") );
    isConfigured = ack==1;
    //delay(20);
  }


};



////////////////////////////////////////////////////////////////////////////////
// INTERNAL CONFIG


enum Modes{
  STOP,
  INIT,
  CALIBRATION,
  CONFIG,
  RUNNING,
};

uint8_t mode;
Box box[NUM_BOXES];
long watchdog;



////////////////////////////////////////////////////////////////////////////////
// SETUP

void setup() {
  // initialize the LCD
	lcd.begin();
  lcd.backlight();
  lcd.createChar(0, customCharArrow1);
  lcd.createChar(1, customCharArrow2);
  lcd.createChar(2, customCharArrow3);

  // initialize serial ports:
  Serial.begin(38400);
  Serial1.begin(38400);
  Serial2.begin(38400);
  Serial3.begin(38400);
  softSerial.begin(115200);

  if (VERBOSE) debugSerial->println("<MASTER> Setup started. Version " + version);

  // setup boxes
  for(uint8_t i = 0; i < NUM_BOXES; i++){
    if (i == 0) box[i].setup(i+1, &cmdMessenger1);
    else if (i == 1) box[i].setup(i+1, &cmdMessenger2);
    else if (i == 2) box[i].setup(i+1, &cmdMessenger3);
    else if (i == 3) box[i].setup(i+1, &cmdMessenger4);
  }

  cmdMessenger1.attach(Box::kLog, receiveLog1);
  cmdMessenger1.attach(Box::kStatus, receiveStatus1);
  cmdMessenger1.attach(Box::kError, receiveError1);

  cmdMessenger2.attach(Box::kLog, receiveLog2);
  cmdMessenger2.attach(Box::kStatus, receiveStatus2);
  cmdMessenger2.attach(Box::kError, receiveError2);

  cmdMessenger3.attach(Box::kLog, receiveLog3);
  cmdMessenger3.attach(Box::kStatus, receiveStatus3);
  cmdMessenger3.attach(Box::kError, receiveError3);

  cmdMessenger4.attach(Box::kLog, receiveLog4);
  cmdMessenger4.attach(Box::kStatus, receiveStatus4);
  cmdMessenger4.attach(Box::kError, receiveError4);


  loadDefaultStateValues(); // values from code for 0..last
  // override 7..last
  loadStateParamsFromEEPROM(); // values from EEPROM from 7..last
  //saveAllParamsToEEPROM(); // 7..last

  randomSeed(analogRead(0));

  debugSerial->println("<MASTER> Setup finished");

  displayStep01();
  setMode(INIT);

}


////////////////////////////////////////////////////////////////////////////////
// THE UGLY PART: COMMAND HANDLER FOR EACH BOX

void receiveStatus1(){
  box[0].receiveStatus();
}

void receiveLog1(){
  box[0].receiveLog();
}

void receiveError1(){
  box[0].receiveError();
}

// -----

void receiveStatus2(){
  box[1].receiveStatus();
}

void receiveLog2(){
  box[1].receiveLog();
}

void receiveError2(){
  box[1].receiveError();
}

// -----

void receiveStatus3(){
  box[2].receiveStatus();
}

void receiveLog3(){
  box[2].receiveLog();
}

void receiveError3(){
  box[2].receiveError();
}

// -----

void receiveStatus4(){
  box[3].receiveStatus();
}

void receiveLog4(){
  box[3].receiveLog();
}

void receiveError4(){
  box[3].receiveError();
}


////////////////////////////////////////////////////////////////////////////////
// LOOP

void loop() {

  for(uint8_t i = 0; i < NUM_BOXES; i++){
    box[i].loop();
  }

  checkWatchdog();
  readDebugSerial();
    
  switch(mode){

    case STOP:
      setAllBoxesToState(STOPPED);
    break;

    case INIT:
      // wait for all boxes
      if ( allBoxesInState(INIT_0) ) {
        // start init
        configureAllBoxes();

      } else if ( allBoxesInState(UP_READY) ) {
        setMode(RUNNING);

      } else if (millis() > watchdog + 3000) {
        // no response after ...s
        //setAllBoxesToState(INIT_0);
      }

    break;

    case RUNNING:
      // wait for all boxes
      if ( allBoxesInState(UP_READY) ) {
        // start falling
        debugSerial->println("\n<MASTER> ------- DOWN ------" );
        delay(stateParams[UP_READY].delay);
        if (stateParams[UP_READY].randomDelay > 0) {
          uint16_t d = random(stateParams[UP_READY].randomDelay);
          delay(d);
        }
        setAllBoxesToState(DOWN_1);
      } else if ( allBoxesInState(DOWN_READY) ) {
        // start lifting
        debugSerial->println("\n<MASTER> -------  UP  ------" );
        delay(stateParams[DOWN_READY].delay);
        // random sync
        if (stateParams[DOWN_READY].randomDelay > 0) {
          uint16_t d = random(stateParams[DOWN_READY].randomDelay);
          delay(d);
        }
        // TODO
        // random async
        // if (stateParams[DOWN_READY].randomDelay > 0) {
        //   uint16_t d = random(stateParams[DOWN_READY].randomDelay);
        //   delay(d);
        // }
        setAllBoxesToState(UP_1);
      }

    break;

    case CALIBRATION:
      if ( allBoxesInState(UP_READY) ) {
        setAllBoxesToState(CAL_1);
      }
    break;


    case CONFIG:


    break;

  }


  // read knob
  action = readKnob();
  if (action != 0  && mode == CONFIG) {
    configUpdateDisplay();
  }


  // buttons
  if (okBtn.onPressed()) {
    if (mode == CONFIG) {
      // STATE -> PROPERTY -> VALUE -> PROPERTY
      if (selected == SELECT_STATE) {
        selected = SELECT_PROPERTY;
        selectedProperty = 0;
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
      // VALUE -> PROPERTY -> STATE -> EXIT
      if (selected == SELECT_STATE) {
        // mode running
      } else if (selected == SELECT_PROPERTY) {
        selected = SELECT_STATE;
      } else if (selected == SELECT_VALUE) {
        selected = SELECT_PROPERTY;
      }
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
    configUpdateDisplay();
  }


//   STOP,
  // INIT,
  // CALIBRATION,
  // CONFIG,
  // RUNNING,


  // if(calibrationBtn.onPressed()) {
  //   if (mode != CALIBRATION) {
  //     // 1. start calibration
  //     setMode(CALIBRATION);
  //   } else {
  //
  //     if ( allBoxesInState(CAL_1) ) {
  //       // 2. stop calibration
  //       setAllBoxesToState(CAL_2);
  //     } else {
  //       // 3. RUNNING
  //       setMode(RUNNING);
  //     }
  //
  //   }
  //   if (VERBOSE) debugSerial->println("calibrationBtn pressed. mode: " + (String)mode2Name(mode) );
  // }
  //
  // if(stopBtn.onPressed()) {
  //   if (mode != STOP) {
  //     setMode(STOP);
  //
  //     delay(1000);
  //
  //     debugSerial->println("RESET BOXES");
  //     // reset all boxes
  //     for(uint8_t i = 0; i < NUM_BOXES; i++){
  //       box[i].reset();
  //     }
  //
  //
  //   } else {
  //     setMode(INIT);
  //   }
  //   if (VERBOSE) debugSerial->println("stopBtn pressed. mode: " + (String)mode2Name(mode) );
  // }

  if (mode == RUNNING) updateDisplay();
}




////////////////////////////////////////////////////////////////////////////////
// HELPER

boolean allBoxesInState(uint8_t state){
  uint8_t counter = 0;
  for(uint8_t i = 0; i < NUM_BOXES; i++){
    if (box[i].state == state) counter++;
  }
  return counter == NUM_BOXES;
}

void setMode(uint8_t _mode){
  mode = _mode;
  watchdog = millis();

  lcd.clear();
  debugSerial->print("\n<MASTER> mode: ");
  debugSerial->println( mode2Name(mode) );
  if (mode != CONFIG) displayModeUpdate();

  if (mode == INIT) {
    setAllBoxesToState(INIT_0);
    lcd.setCursor(0, 3);
    lcd.print("V " + version);
    //delay(1000);
  } else if (mode == CALIBRATION) {
    setAllBoxesToState(INIT_0);
    lcd.setCursor(0, 3);
    lcd.print("Press Back to stop ");
  }
}

void setAllBoxesToState(uint8_t state){
  debugSerial->print("<MASTER> all to ");
  debugSerial->println( state2Name(state) );
  watchdog = millis();
  for(uint8_t i = 0; i < NUM_BOXES; i++){
    box[i].setRemoteState(state);
  }
  //delay(300);
}

void configureAllBoxes(){

  debugSerial->println("<MASTER> configureAllBoxes start");

// TODO reomove this 
    stateParams[0].mode = 0;
    saveParamToEEPROM(state, PROPERTY_MODE);
    
    stateParams[0].steps = 0;
    saveParamToEEPROM(state, PROPERTY_STEPS);
    
    stateParams[0].vel1 = 5;
    saveParamToEEPROM(state, PROPERTY_V1);
    stateParams[0].vel2 = 0;
    saveParamToEEPROM(state, PROPERTY_V2);


  debugSerial->println("verbose " + (String)stateParams[0].mode );
  debugSerial->println("responsive " + (String)stateParams[0].steps );
  debugSerial->println("driveDelay1 " + (String)stateParams[0].vel1 );
  debugSerial->println("silent " + (String)stateParams[0].vel2 );

  for(uint8_t i = 0; i < NUM_BOXES; i++){
    for(uint8_t s = 0; s < 20; s++){
      if (s == 0 || stateParams[s].steps > 0) box[i].configure(stateParams[s], s);
      //else debugSerial->println("exit " + (String)s + " " + (String)stateParams[s].steps );
    }
  }
  debugSerial->println("<MASTER> configureAllBoxes end");
  setAllBoxesToState(INIT_1);
  // TODO:
  // reset box states on master and wait for refresh
  for(uint8_t i = 0; i < NUM_BOXES; i++){
    box[i].state = 1000;
  }
}

void checkWatchdog(){
  if (millis() > watchdog + WATCHDOG_TIMEOUT) {
    //debugSerial->println("<MASTER> Watchdog " );
    //debugSerial->print( (millis() - watchdog - WATCHDOG_TIMEOUT)/1000.0 );
    //debugSerial->println("s (reset now)");


    // action for mode RUNNING
    if (mode == RUNNING) {
      setMode(INIT);
    }


    watchdog = millis();
    //requestStates();
  }
}

void readDebugSerial(){
  // receive commands from debug serial 
  if (softSerial.available()) {
    char inByte = softSerial.read();
    debugSerial->print("CMD ");
    debugSerial->println(inByte);

    if (inByte == 'v') {
      debugSerial->println("enable verbose");
      stateParams[0].mode = 1;
    } else if (inByte == 'V') {
      debugSerial->println("disable verbose");
      stateParams[0].mode = 0;
    
    } else if (inByte == 'r') {
      debugSerial->println("disable responsive");
      stateParams[0].steps = 1; 
    } else if (inByte == 'R') {
      debugSerial->println("enable responsive");
      stateParams[0].steps = 0; 
      
    } else if (inByte == 'q') {
      debugSerial->println("disable silent");
      stateParams[0].vel2 = 0; 
    } else if (inByte == 'Q') {
      debugSerial->println("enable silent");
      stateParams[0].vel2 = 1; 
      
    } else if (inByte == '1') {
      debugSerial->println("driveDelay1 1");
      stateParams[0].vel1 = 1;
    } else if (inByte == '2') {
      debugSerial->println("driveDelay1 5");
      stateParams[0].vel1 = 5;
    } else if (inByte == '3') {
      debugSerial->println("driveDelay1 20");
      stateParams[0].vel1 = 20;
    
    } else if (inByte == 'c') {
      setMode(CALIBRATION);
      debugSerial->println(mode);
      debugSerial->println("send C to stop");
    } else if (inByte == 'C') {
      setAllBoxesToState(CAL_2);
      debugSerial->println('STOP: CAL_2');
    } else if (inByte == 's') {
      setMode(STOP);
      debugSerial->println(mode);
    } else if (inByte == 'i') {
      setMode(INIT);
      debugSerial->println(mode);

    } else if (inByte == 'x') {
      // debugSerial->println("enable unresponsive");
      // stateParams[0].steps = 1; 
      // stateParams[0].vel1 = 5;
      // stateParams[0].vel2 = 5;
    }  

    // send config params
    if (inByte != 'c' && inByte != 'C' && inByte != 's' && inByte != 'i') {
      for(uint8_t i = 0; i < NUM_BOXES; i++){
        box[i].configure(stateParams[0], 0);
      }
    }
  }
}

void requestStates(){
  for(uint8_t i = 0; i < NUM_BOXES; i++){
    box[i].requestState();
  }
}

String mode2Name(uint8_t mode){
  String str = "unknown";
  if (mode == STOP) str = "STOP";
  else if (mode == INIT) str = "INIT";
  else if (mode == RUNNING) str = "RUNNING";
  else if (mode == CALIBRATION) str = "CALIBRATION";
  else if (mode == CONFIG) str = "CONFIGURATION";
  return str;
}

////////////////////////////////////////////////////////////////////////////////
// GUI


int8_t readKnob(){
  knobValue = knob.read();
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

void configUpdateDisplay(){

  if (selected == SELECT_STATE) {
    state += action;
    if (state < 6) state = numStates;
    else if (state > numStates) state = 6;
  }

  if (selected == SELECT_PROPERTY) {
    selectedProperty += action;
    if (selectedProperty < 0) selectedProperty = numStates;
    else if (selectedProperty > numProperties) selectedProperty = 0;

    // load property into "value"
    if (selectedProperty == PROPERTY_STEPS) {
      value = stateParams[state].steps;
    }
    else if (selectedProperty == PROPERTY_MODE) {
      value = stateParams[state].mode;
    }
    else if (selectedProperty == PROPERTY_V1) {
      value = stateParams[state].vel1;
    }
    else if (selectedProperty == PROPERTY_V2) {
      value = stateParams[state].vel2;
    }
    else if (selectedProperty == PROPERTY_DELAY) {
      value = stateParams[state].delay;
    }
    else if (selectedProperty == PROPERTY_RANDDELAY) {
      value = stateParams[state].randomDelay;
    }
  }

  if (selected == SELECT_VALUE) {
    if (selectedProperty == PROPERTY_STEPS) {
      value += action * 5;
      if (value < 0) value = 0;
      else if (value > 9999) value = 9999;
    }
    else if (selectedProperty == PROPERTY_MODE) {
      value += action;
      if (value < 0) value = 0;
      else if (value > 5) value = 0;
    }
    // TODO: find fastest velocity
    else if (selectedProperty == PROPERTY_V1 || selectedProperty == PROPERTY_V2) {
      value += action * 2;
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
  lcd.setCursor(0,0);
  if (selected == SELECT_STATE) lcd.write(0);
  else lcd.print(" ");
  lcdPrint( state2Name(state), 1,0,9 );
  lcdPrint( (String) (state-5), 18,0,2 ); // state ID

  writeProperty(0,1, PROPERTY_STEPS, "ST", "Steps", (String) stateParams[state].steps );

  writeProperty(10,1, PROPERTY_MODE, "MO", "StepMode", (String) stepMode2Name(stateParams[state].mode) );
  if (state != 6 && selected == SELECT_VALUE && selectedProperty == PROPERTY_MODE) lcdPrint( (String) stepMode2Name(value), 14,1,6 );

  writeProperty(0,2, PROPERTY_V1, "V1", "Vel Start", (String) stateParams[state].vel1 );
  writeProperty(10,2, PROPERTY_V2, "V2", "Vel End", (String) stateParams[state].vel2 );
  if (state == DOWN_READY || state == UP_READY || state == 6){
    writeProperty(0,3, PROPERTY_DELAY, "DL", "Delay", (String) stateParams[state].delay );
    writeProperty(10,3, PROPERTY_RANDDELAY, "RD", "RandDelay", (String) stateParams[state].randomDelay );
  } else {
    lcdPrint("", 0,3, 10);
    lcdPrint("", 10,3, 10);
  }


}

void writeProperty(uint8_t x, uint8_t y, uint8_t prop, String labelShort, String label, String _value){ // 0,2, PROPERTY_V1
  if (state != 6) {
    // display short label + value
    lcd.setCursor(x,y);
    if (selected == SELECT_PROPERTY && selectedProperty == prop) lcd.write(0);
    else lcd.print(" ");

    lcdPrint( labelShort, x+1,y, 2 );
    if (selected == SELECT_VALUE && selectedProperty == prop) lcdPrint( (String) value , x+4,y, 6 ); // display temp value
    else lcdPrint( (String) _value , x+4,y, 6 );

    lcd.setCursor(x+3,y);
    if (selected == SELECT_VALUE && selectedProperty == prop) lcd.write(2);
    else lcd.print(" ");

  } else {

    // help: display long labels
    lcdPrint(label, x+1,y, 9);
  }

}


void lcdPrint(String s, uint8_t x, uint8_t y, uint8_t l){
  lcd.setCursor(x,y);
  for (uint8_t i = 0; i < l; i++) lcd.print(" ");
  lcd.setCursor(x,y);
  lcd.print( s.substring(0,l) );
}




String stepMode2Name(uint8_t mode){
  String str = "?";
  if (mode == FULL) str = "1/1";
  else if (mode == HALF) str = "1/2";
  else if (mode == QUARTER) str = "1/4";
  else if (mode == QUARTER_S) str = "1/4 S";
  else if (mode == SIXTEENTH) str = "1/16";
  else if (mode == SIXTEENTH_S) str = "1/16 S";
  return str;
}


void saveValue() {
  if (selectedProperty == PROPERTY_STEPS) {
    stateParams[state].steps = value;
    saveParamToEEPROM(state, PROPERTY_STEPS);
  }
  else if (selectedProperty == PROPERTY_MODE) {
    stateParams[state].mode = value;
    saveParamToEEPROM(state, PROPERTY_MODE);
  }
  else if (selectedProperty == PROPERTY_V1) {
    stateParams[state].vel1 = value;
    saveParamToEEPROM(state, PROPERTY_V1);
  }
  else if (selectedProperty == PROPERTY_V2) {
    stateParams[state].vel2 = value;
    saveParamToEEPROM(state, PROPERTY_V2);
  }
  else if (selectedProperty == PROPERTY_DELAY) {
    stateParams[state].delay = value;
    saveParamToEEPROM(state, PROPERTY_DELAY);
  }
  else if (selectedProperty == PROPERTY_DELAY) {
    stateParams[state].randomDelay = value;
    saveParamToEEPROM(state, PROPERTY_DELAY);
  }

}


void displayModeUpdate(){
  lcd.setCursor(0, 0);
	lcd.print("                    ");
  lcd.setCursor(0, 0);
	lcd.print("Mode " + mode2Name(mode) );
}

void updateDisplay(){
  // BOXES
  for(uint8_t i = 0; i < NUM_BOXES; i++){
    if (box[i].displayNewState) {
      _displayUpdateState(i);
      box[i].displayNewState = false;
    }

    if (box[i].setStateTimeout != 0) {
      _displayUpdateError(i);
    }
  }
}

void _displayUpdateState(uint8_t index){
    String s = state2Name(box[index].state);
    // clear
    if (index==0) lcd.setCursor(0,1);
    else if (index==1) lcd.setCursor(10,1);
    else if (index==2) lcd.setCursor(0,2);
    else if (index==3) lcd.setCursor(10,2);
    lcd.print( "          " );
    // write
    if (index==0) lcd.setCursor(0,1);
    else if (index==1) lcd.setCursor(10,1);
    else if (index==2) lcd.setCursor(0,2);
    else if (index==3) lcd.setCursor(10,2);
    lcd.print( s.substring(0,8) );
}

void _displayUpdateError(uint8_t index){
    String s = state2Name(box[index].state);
    // clear
    if (index==0) lcd.setCursor(0,1);
    else if (index==1) lcd.setCursor(10,1);
    else if (index==2) lcd.setCursor(0,2);
    else if (index==3) lcd.setCursor(10,2);
    lcd.print( "          " );
    // write
    if (index==0) lcd.setCursor(0,1);
    else if (index==1) lcd.setCursor(10,1);
    else if (index==2) lcd.setCursor(0,2);
    else if (index==3) lcd.setCursor(10,2);
    lcd.print( "Timeout" );
}


////////////////////////////////////////////////////////////////////////////////
// HELPER


void displayStep01(){
  lcd.clear();
  // lcd.setCursor(0, 0);
	// lcd.print("NOTHING WILL EVER ");
  // lcd.setCursor(0, 1);
	// lcd.print("BE THE SAME");

  lcd.setCursor(0, 3);
  lcd.print("V " + version);
}

void loadStateParamsFromEEPROM(){
  int address = 0;
  for (uint8_t i = 7; i < numStates; i++){
    address = (i-7) * 32;
    stateParams[i].steps = EEPROM.readInt(address);
    stateParams[i].mode = EEPROM.readByte(address+4);
    stateParams[i].vel1 = EEPROM.readInt(address+8);
    stateParams[i].vel2 = EEPROM.readInt(address+12);
    stateParams[i].delay = EEPROM.readInt(address+16);
    stateParams[i].randomDelay = EEPROM.readInt(address+20);
  }
}

void saveAllParamsToEEPROM(){
  int address = 0;
  for (uint8_t i = 7; i < numStates; i++){
    for (uint8_t j = 0; j < 6; j++) saveParamToEEPROM(i, j);
  }
}

void saveParamToEEPROM(uint8_t state, uint8_t param){
  int address = 0;
  address = (state-7) * 32;
  if (param == PROPERTY_STEPS) EEPROM.writeInt(address, stateParams[state].steps);
  else if (param == PROPERTY_MODE) EEPROM.writeByte(address+4, stateParams[state].mode);
  else if (param == PROPERTY_V1) EEPROM.writeInt(address+8, stateParams[state].vel1);
  else if (param == PROPERTY_V2) EEPROM.writeInt(address+12, stateParams[state].vel2);
  else if (param == PROPERTY_DELAY) EEPROM.writeInt(address+16, stateParams[state].delay);
  else if (param == PROPERTY_RANDDELAY) EEPROM.writeInt(address+20, stateParams[state].randomDelay);
}


void loadDefaultStateValues() {
  // init
  stateParams[INIT_1].mode = SIXTEENTH;
  stateParams[INIT_1].steps = 100; // 87 = 10cm
  stateParams[INIT_1].vel1 = 5;
  stateParams[INIT_1].vel2 = 5;

  stateParams[INIT_2].mode = SIXTEENTH;
  stateParams[INIT_2].steps = 20000;
  stateParams[INIT_2].vel1 = 20;
  stateParams[INIT_2].vel2 = 20;

  // down
  stateParams[UP_READY].delay = 1000; // ms
  stateParams[UP_READY].randomDelay = 0; // ms

  stateParams[DOWN_1].mode = SIXTEENTH;
  stateParams[DOWN_1].steps = 20;
  stateParams[DOWN_1].vel1 = 1;
  stateParams[DOWN_1].vel2 = 30;

  stateParams[DOWN_2].mode = QUARTER;
  stateParams[DOWN_2].steps = 300;
  stateParams[DOWN_2].vel1 = 30;
  stateParams[DOWN_2].vel2 = 50;

  stateParams[DOWN_3].mode = HALF;
  stateParams[DOWN_3].steps = 700;
  stateParams[DOWN_3].vel1 = 50;
  stateParams[DOWN_3].vel2 = 90;

  stateParams[DOWN_4].mode = HALF;
  stateParams[DOWN_4].steps = 400;
  stateParams[DOWN_4].vel1 = 90;
  stateParams[DOWN_4].vel2 = 50;

  stateParams[DOWN_5].mode = SIXTEENTH;
  stateParams[DOWN_5].steps = 150;
  stateParams[DOWN_5].vel1 = 50;
  stateParams[DOWN_5].vel2 = 1;


  stateParams[UP_1].mode = SIXTEENTH;
  stateParams[UP_1].steps = 20;
  stateParams[UP_1].vel1 = 1;
  stateParams[UP_1].vel2 = 10;

  stateParams[UP_2].mode = SIXTEENTH;
  stateParams[UP_2].steps = 300;
  stateParams[UP_2].vel1 = 10;
  stateParams[UP_2].vel2 = 20;

  stateParams[UP_3].mode = SIXTEENTH;
  stateParams[UP_3].steps = 700;
  stateParams[UP_3].vel1 = 20;
  stateParams[UP_3].vel2 = 20;

  stateParams[UP_4].mode = SIXTEENTH;
  stateParams[UP_4].steps = 400;
  stateParams[UP_4].vel1 = 20;
  stateParams[UP_4].vel2 = 10;

  stateParams[UP_5].mode = SIXTEENTH;
  stateParams[UP_5].steps = 150;
  stateParams[UP_5].vel1 = 10;
  stateParams[UP_5].vel2 = 1;


  stateParams[DOWN_READY].delay = 2000; // ms
  stateParams[DOWN_READY].randomDelay = 0; // ms

  stateParams[RELAX].mode = SIXTEENTH;
  stateParams[RELAX].steps = 20;
  stateParams[RELAX].vel1 = 2;
  stateParams[RELAX].vel2 = 2;
}


//   lcd.clear();
//   // BOXES
//   for(uint8_t i = 0; i < NUM_BOXES; i++){
//     if (i==0) lcd.setCursor(0,1);
//     else if (i==1) lcd.setCursor(10,1);
//     else if (i==2) lcd.setCursor(0,2);
//     else if (i==3) lcd.setCursor(10,2);
//     String s = box[i].setStateTimeout == 0 ? "OK" : "?";
//     s = ": " + s;
//     lcd.print("Box" + (i+1) + s);
//   }
// }

// isConfigured
