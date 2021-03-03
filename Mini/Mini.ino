#include <CmdMessenger.h>

/*

  NOTHING WILL EVER BE THE SAME
  Mini / Box

  Arduino Pro or Pro Mini

  2018 Jens Weber

*/


// stepper modes
enum {
  FULL,
  HALF,
  QUARTER,
  QUARTER_S,
  SIXTEENTH,
  SIXTEENTH_S,
};

////////////////////////////////////////////////////////////////////////////////
// CONFIG

String version = "0.6";
#define WATCHDOG_TIMEOUT 15000 // ms / send every ...ms state to master
//TODO bool verbose true // log more details, as config option


struct boxStates{
  uint8_t mode = SIXTEENTH; // full / half ...
  uint16_t steps; // fullsteps!
  uint16_t vel1; // start velocity
  uint16_t vel2; // end velocity
  //uint16_t delay = 0; // only master
};
boxStates stateParams[20];


// PINS
#define LIMIT_PIN 2 // interrupt pin for limit switch
#define EN_PIN 7    //enable (CFG6)
#define DIR_PIN 8   //direction
#define STEP_PIN 9  //step
#define CFG1_PIN 5  // microsteps / chopping
#define CFG2_PIN 6  // microsteps / chopping
#define LED 13

// VARS
uint8_t state, lastState;
long limitSwitchTimer;
long watchdog, lastWatchdog;
uint16_t stepCounter;

volatile bool limitReached = false; // to break drive cycle by limit switch
volatile bool limitSwitchRaw = false;
volatile bool limitSwitchReset = false;

CmdMessenger cmdMessenger = CmdMessenger(Serial);

// const for drive function
#define UP true
#define DOWN false
#define FREE true
#define HOLD false

// This is the list of recognized commands. These can be commands that can either be sent or received.
enum
{
  // COMMANDS
  // Box to Master
  kAck, // send ok to master
  kError, // send error to master
  kStatus,   // send state to master
  kLog, // send log to master

  // Master to Box
  kReset, // reset box from master
  kState,   // set state from master
  kGetStatus,   // query status from master
  kConfigure, // configure box
};


enum States{
  STOPPED,
  INIT_0,
  INIT_1,
  INIT_2,

  CAL_1,
  CAL_2,
  CAL_3,

  DOWN_1,
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


void attachCommandCallbacks() {
  // Attach callback methods
  cmdMessenger.attach(OnUnknownCommand);
  cmdMessenger.attach(kReset, reset);
  cmdMessenger.attach(kState, receiveState);
  cmdMessenger.attach(kGetStatus, sendStatus);
  cmdMessenger.attach(kConfigure, configureState);
}

void OnUnknownCommand() {
  cmdMessenger.sendCmd(kError,"Command without attached callback");
}

// software reset
void(* resetFunc) (void) = 0; //declare reset function @ address 0

void reset() {
  sendLog("Version " + version);
  sendLog("RESET");
  delay(1000);
  resetFunc();
}

void sendStatus(){
  cmdMessenger.sendCmdStart(kStatus);
  cmdMessenger.sendCmdArg(state);
  cmdMessenger.sendCmdArg(lastState);
  float t = (millis() - lastWatchdog) / 1000.0;
  cmdMessenger.sendCmdArg( t );
  t = (millis() - limitSwitchTimer) / 1000.0;
  cmdMessenger.sendCmdArg( t );
  cmdMessenger.sendCmdEnd();
}

// receive state params from master
void configureState(){
  uint8_t _state = cmdMessenger.readInt16Arg();
  uint8_t  mode = cmdMessenger.readInt16Arg();
  uint16_t steps = cmdMessenger.readInt16Arg();
  uint16_t vel1 = cmdMessenger.readInt16Arg();
  uint16_t vel2 = cmdMessenger.readInt16Arg();

  if (_state >= 0 && _state < 100) {
    stateParams[_state].mode = mode;
    stateParams[_state].steps = steps;
    sendLog("steps");
    sendLog( (String) steps);
    stateParams[_state].vel1 = vel1 == 0 ? 1 : vel1;
    stateParams[_state].vel2 = vel2 == 0 ? 1 : vel2;
    cmdMessenger.sendCmd(kAck);
  } else sendError("Unknown state " + _state);
}

void receiveState(){
  uint8_t _state = cmdMessenger.readInt16Arg();
  cmdMessenger.sendCmd(kAck);
  if (_state >= 0 && _state < 100) setState(_state);
  else sendLog("Unknown state " + _state);
}

void setState(uint8_t _state){
  if (state != _state) {
    lastState = state;
    lastWatchdog = watchdog;
    state = _state;
    watchdog = millis();

    // init states
    switch(state){
      case CAL_1:
        stepCounter = 0;
        sendLog("Calibration started");
      break;
    }

  }
  sendStatus();
}

void sendLog(String msg) {
  cmdMessenger.sendCmd(kLog, msg);
}

void sendError(String msg) {
  cmdMessenger.sendCmd(kError, msg);
}


void setup() {
  // wait for serial
  while (!Serial) {
      ;
  }

  // default values - config overrides all!
  // init
  stateParams[INIT_1].mode = SIXTEENTH;
  stateParams[INIT_1].steps = 100; // 87 = 10cm
  stateParams[INIT_1].vel1 = 4;
  stateParams[INIT_1].vel2 = 4;

  stateParams[INIT_2].mode = SIXTEENTH;
  stateParams[INIT_2].steps = 20000;
  stateParams[INIT_2].vel1 = 4;
  stateParams[INIT_2].vel2 = 4;

  // down
  stateParams[DOWN_1].mode = HALF;
  stateParams[DOWN_1].steps = 100;
  stateParams[DOWN_1].vel1 = 400;
  stateParams[DOWN_1].vel2 = 400;

  stateParams[DOWN_2].mode = SIXTEENTH;
  stateParams[DOWN_2].steps = 100;
  stateParams[DOWN_2].vel1 = 400;
  stateParams[DOWN_2].vel2 = 400;

  stateParams[DOWN_3].mode = SIXTEENTH;
  stateParams[DOWN_3].steps = 100;
  stateParams[DOWN_3].vel1 = 12;
  stateParams[DOWN_3].vel2 = 12;

  stateParams[DOWN_4].mode = SIXTEENTH;
  stateParams[DOWN_4].steps = 100;
  stateParams[DOWN_4].vel1 = 12;
  stateParams[DOWN_4].vel2 = 12;

  stateParams[DOWN_5].mode = SIXTEENTH;
  stateParams[DOWN_5].steps = 100;
  stateParams[DOWN_5].vel1 = 12;
  stateParams[DOWN_5].vel2 = 12;

  // up
  stateParams[UP_1].mode = SIXTEENTH;
  stateParams[UP_1].steps = 100;
  stateParams[UP_1].vel1 = 4;
  stateParams[UP_1].vel2 = 4;

  stateParams[UP_2].mode = SIXTEENTH;
  stateParams[UP_2].steps = 100;
  stateParams[UP_2].vel1 = 4;
  stateParams[UP_2].vel2 = 4;

  stateParams[UP_3].mode = SIXTEENTH;
  stateParams[UP_3].steps = 100;
  stateParams[UP_3].vel1 = 4;
  stateParams[UP_3].vel2 = 4;

  stateParams[UP_4].mode = SIXTEENTH;
  stateParams[UP_4].steps = 100;
  stateParams[UP_4].vel1 = 4;
  stateParams[UP_4].vel2 = 4;

  stateParams[UP_5].mode = SIXTEENTH;
  stateParams[UP_5].steps = 100;
  stateParams[UP_5].vel1 = 4;
  stateParams[UP_5].vel2 = 4;

  stateParams[RELAX].mode = SIXTEENTH;
  stateParams[RELAX].steps = 20;
  stateParams[RELAX].vel1 = 2;
  stateParams[RELAX].vel2 = 2;


  pinMode(LED, OUTPUT); // indicator LED for debugging
  pinMode(LIMIT_PIN, INPUT_PULLUP);  // limit switch
  attachInterrupt(digitalPinToInterrupt(LIMIT_PIN), limitSwitch, FALLING);
  attachInterrupt(digitalPinToInterrupt(LIMIT_PIN), limitSwitchBack, RISING);

  // initialize stepper driver
  delay(200);  // wait a bit for stepper driver
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH); // deactivate driver (LOW active)
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(DIR_PIN, LOW); // set direction to down (LOW)
  pinMode(STEP_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);

  Serial.begin(19200);
  cmdMessenger.printLfCr();
  attachCommandCallbacks();

  sendLog("Setup finished. Version " + version);

  setState(INIT_0);
}


void loop() {
  cmdMessenger.feedinSerialData();
  checkWatchdog();

  switch(state){

    case STOPPED:
      //sendLog("STOPPED");
      digitalWrite(EN_PIN, HIGH); // driver free
      delay(100);
    break;

    // INIT ////////////////
    case INIT_0:
      // nothing to do: wait for master
    break;

    case INIT_1:
      drive(DOWN, HOLD);
      setState(INIT_2);
    break;

    case INIT_2:
      drive(UP, HOLD);
      if (limitReached) {
        limitReached = false;
        setState(RELAX);
      }
    break;

    // DOWN ////////////////
    case DOWN_1:
      drive(DOWN, HOLD);
      setState(DOWN_2);
    break;

    case DOWN_2:
      drive(DOWN, HOLD);
      setState(DOWN_3);
    break;

    case DOWN_3:
      drive(DOWN, HOLD);
      setState(DOWN_4);
    break;

    case DOWN_4:
      drive(DOWN, HOLD);
      setState(DOWN_5);
    break;

    case DOWN_5:
      drive(DOWN, HOLD);
      setState(DOWN_READY);
    break;

    case DOWN_READY:
      // wait for master cammand to lift
      // TODO RELAX - DISABLE DRIVER
    break;

    // UP ////////////////
    case UP_1:
      drive(UP, HOLD);
      setState(UP_2);
    break;

    case UP_2:
      drive(UP, HOLD);
      setState(UP_3);
    break;

    case UP_3:
      drive(UP, HOLD);
      setState(UP_4);
    break;

    case UP_4:
      drive(UP, HOLD);
      if (limitReached) {
        limitReached = false;
        setState(RELAX);
        sendLog("Error: limit switch in UP4. Reduce steps in UP4");
      } else {
        setState(UP_5);
      }
    break;

    case UP_5:
      // wait for limit switch
      drive(UP, HOLD);
      if (limitReached) {
        limitReached = false;
        setState(RELAX);
      }
    break;

    case RELAX:
      drive(DOWN, HOLD);
      setState(UP_READY);

      delay(300);
      digitalWrite(LED, LOW);
      sendLog("relax");
    break;

    case UP_READY:
      // nothing to do // wait for master cammand to fall
    break;


    // CALIBRATION ////////////////
    case CAL_1:
      //sendLog("cal 1");
      calibrateFall();
      // wait for master to stop
    break;

    case CAL_2:
      sendLog("steps /16:");
      sendLog( (String) stepCounter);
      setState(CAL_3);
    break;

    case CAL_3:
      // wait for master
    break;
  }

  // handling exception
  // state is not UP_5 || INIT_2
  if (limitReached) {
    limitReached = false;
    if (state == INIT_1) {
      sendLog("Error: Limit switch while INIT_1 -> INIT");
      setState(INIT_1);
    } 
    sendLog("ERROR: limit switch activated ");
  }

  if (limitSwitchRaw) {
    limitSwitchRaw = false;
    sendLog("limit switch activated ");
  }

  if (limitSwitchReset) {
    limitSwitchReset = false;
    sendLog("limit switch reset ");
    sendLog( (String) limitSwitchTimer);
  }
  
}

// interrupt (don't use Serial)
void limitSwitch() {
  if (millis() < limitSwitchTimer + 500) {
    //sendLog("limit switch activated, but ignored (<500ms)");
    return; // debounce switch with 500ms
  }
  //digitalWrite(EN_PIN, HIGH);   // disable driver
  limitSwitchTimer = millis();
  // sendLog("limit switch activated");
  // if (state == INIT_1 || state == INIT_2 || state == UP_5 || state == UP_4) {
    limitReached = true;
  // } 
  limitSwitchRaw = true;
  digitalWrite(LED, HIGH);
}

// interrupt 
void limitSwitchBack() {
  limitSwitchReset = true;
}

void checkWatchdog(){
  if (millis() > watchdog + WATCHDOG_TIMEOUT) {
    //sendLog("Watchdog");
    sendStatus();
    watchdog = millis();
  }
}

// in state CAL_1 fall slowly and count steps
void calibrateFall() {
  sixteenthSteps();
  digitalWrite(EN_PIN, LOW);    // enable driver
  digitalWrite(DIR_PIN, LOW);   // direction down
  for (uint16_t i = 0; i < 100; i++) { // limit this to receive command via Serial
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(5);   //was 10
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(1500);
    stepCounter++;
  }
}

//
void drive(bool up, bool free) {
  int8_t f;
  if (stateParams[state].mode == FULL)         {
    fullSteps();
    f = 1;
  }
  else if (stateParams[state].mode == HALF)    {
    halfSteps();
    f = 2;
  }
  else if (stateParams[state].mode == QUARTER) {
    quarterSteps();
    f = 4;
  }
  else if (stateParams[state].mode == QUARTER_S)   {
    quarterStepsSpreadCycle();
    f = 4;
  }
  else if (stateParams[state].mode == SIXTEENTH)   {
    sixteenthSteps();
    f = 16;
  }
  else if (stateParams[state].mode == SIXTEENTH_S)   {
    sixteenthStepsSpreadCycle();
    f = 16   ;
  }

  uint16_t steps = stateParams[state].steps * f;
  float v1 = 100000.0 / stateParams[state].vel1 / f;
  float v2 = 100000.0 / stateParams[state].vel2 / f;

  float v = v1;
  float a = (v2 - v1) / steps;

  //sendLog( (String)v );

  // see https://github.com/watterott/SilentStepStick/blob/master/software/TMC2100.ino

  digitalWrite(EN_PIN, LOW);    // enable driver
  digitalWrite(DIR_PIN, up);   // direction

  for (uint16_t i = 0; i < steps; i++) {
    cmdMessenger.feedinSerialData(); // make box responsive
    if (!limitReached && !cmdMessenger.available() && state != STOPPED) {
      digitalWrite(STEP_PIN, HIGH);
      delayMicroseconds(5);
      digitalWrite(STEP_PIN, LOW);

      v += a;
      delayMicroseconds( (unsigned int) v);
    }
  }

  if (!free){
    digitalWrite(EN_PIN, LOW);   // enable driver
  } else {
    digitalWrite(EN_PIN, HIGH);   // disable driver, treiber ausschalten (dann dreht er frei)
  }



  // if ( cmdMessenger.available() ) {
  //   sendLog("exit drive loop");
  // }

  //if (stateParams[state].delay > 0) delay(stateParams[state].delay);
}















/////////////////////////////////////////////////////////
/////spreadCycle / stealthChop///////////////////////////
/////////////////////////////////////////////////////////


void fullSteps() {
  pinMode(CFG1_PIN, OUTPUT);
  digitalWrite(CFG1_PIN, LOW);
  pinMode(CFG2_PIN, OUTPUT);
  digitalWrite(CFG2_PIN, LOW);
}

// half-steps with interpolation
void halfSteps() {
  pinMode(CFG1_PIN, INPUT);
  digitalWrite(CFG1_PIN, LOW);
  pinMode(CFG2_PIN, OUTPUT);
  digitalWrite(CFG2_PIN, LOW);
}

// quarter-steps with interpolation (stealthChop)
void quarterSteps() {
  pinMode(CFG1_PIN, OUTPUT);
  digitalWrite(CFG1_PIN, HIGH);
  pinMode(CFG2_PIN, INPUT);
  digitalWrite(CFG2_PIN, LOW);
}

// quarter-steps with interpolation (spreadCycle)
void quarterStepsSpreadCycle() {
  pinMode(CFG1_PIN, INPUT);
  digitalWrite(CFG1_PIN, LOW);
  pinMode(CFG2_PIN, OUTPUT);
  digitalWrite(CFG2_PIN, HIGH);
}

// sixteenth-steps with interpolation (stealthChop)
void sixteenthSteps() {
  pinMode(CFG1_PIN, INPUT);
  digitalWrite(CFG1_PIN, LOW);
  pinMode(CFG2_PIN, INPUT);
  digitalWrite(CFG2_PIN, LOW);
}

// sixteenth-steps with interpolation (spreadCycle)
void sixteenthStepsSpreadCycle() {
  pinMode(CFG1_PIN, OUTPUT);
  digitalWrite(CFG1_PIN, LOW);
  pinMode(CFG2_PIN, INPUT);
  digitalWrite(CFG2_PIN, LOW);
}
