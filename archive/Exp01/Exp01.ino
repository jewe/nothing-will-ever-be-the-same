
/*

  Experiment 01
  NOTHING WILL EVER BE THE SAME
  Mini / Box

  Arduino Pro or Pro Mini
  328P 5V 16MHz
  
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
long limitSwitchCounter = 0;
long watchdog, lastWatchdog;
uint16_t stepCounter;
bool limitReached = false; // to break drive cycle by limit switch
bool limitSwitchRaw = false;

// const for drive function
#define UP true
#define DOWN false
#define FREE true
#define HOLD false


  uint16_t stps = 1000;
  float vel1 = 10;
  float vel2 = 10;
  bool down = true;
  String debug;

void setup() {
  // wait for serial
  while (!Serial) {
      ;
  }

  pinMode(LED, OUTPUT); // indicator LED for debugging
  pinMode(LIMIT_PIN, INPUT_PULLUP);  // limit switch
  attachInterrupt(digitalPinToInterrupt(LIMIT_PIN), limitSwitch, FALLING);

  // initilaize stepper driver
  delay(200);  // wait a bit for stepper driver
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH); // deactivate driver (LOW active)
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(DIR_PIN, LOW); // set direction to down (LOW)
  pinMode(STEP_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);

  Serial.begin(115200);

Serial.print("start");
 }


void loop() {
 
  if (down) drive(DOWN, HOLD);
  else drive(UP, HOLD);
     
  if (limitReached) {
    limitReached = false;
  }

  if (limitSwitchRaw) {
    limitSwitchRaw = false;
  }
  
  
  Serial.print("limitSwitchTimer");
  Serial.println( (String) limitSwitchTimer);
  Serial.print("limitSwitchCounter");
  Serial.println( (String) limitSwitchCounter);

  Serial.print("debug");
  Serial.println(debug);

  delay(1000);
}

// interrupt (don't use Serial)
void limitSwitch() {
  if (millis() < limitSwitchTimer + 500) return; // debounce switch with 500m
  limitSwitchTimer = millis();
  limitSwitchCounter++;
  limitReached = true;
  limitSwitchRaw = true;
  digitalWrite(LED, HIGH);

  down = !down;
}


//
void drive(bool up, bool free) {
  int8_t f;
  if (!true)         {
    fullSteps();
    f = 1;
  }
  else if (!true)    {
    halfSteps();
    f = 2;
  }
  else if (!true) {
    quarterSteps();
    f = 4;
  }
  else if (!true)   {
    quarterStepsSpreadCycle();
    f = 4;
  }
  else if (!true)   {
    sixteenthSteps();
    f = 16;
  }
  else if (true)   {
    sixteenthStepsSpreadCycle();
    f = 16   ;
  }


  uint16_t steps = 1000 * f;
  float v1 = 100000.0 / vel1 / f;
  float v2 = 100000.0 / vel2 / f;

  float v = v1;
  float a = (v2 - v1) / steps;
  
  debug = v1;

  // see https://github.com/watterott/SilentStepStick/blob/master/software/TMC2100.ino

  digitalWrite(EN_PIN, LOW);    // enable driver
  digitalWrite(DIR_PIN, up);   // direction

  for (uint16_t i = 0; i < steps; i++) {
    if (!limitReached) { 
      digitalWrite(STEP_PIN, HIGH);
      delayMicroseconds(200); 
      digitalWrite(STEP_PIN, LOW);
      v += a;
      delayMicroseconds( (unsigned int) (v) );
    }
  }
   
  
  
  if (!free){
    // digitalWrite(EN_PIN, LOW);   // enable driver
  } else {
    digitalWrite(EN_PIN, HIGH);   // disable driver, treiber ausschalten (dann dreht er frei)
  }

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
