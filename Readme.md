# NOTHING WILL EVER BE THE SAME

## Folders
* `Master/` Master v0.16 (Arduino Mega 2560)
* `Mini/` Box firmware v0.18 (Arduino Pro Mini 328P 5V 16MHz)
* `Master_init/` writes the default parameters into the Master EEPROM
* `libraries/NWEBTS/NWEBTS.h` shared states, commands and **default parameters** (Trier, Sept 2026)
* `archive/` all previous versions (Master v0.15, Mini v0.12, Exp01..Exp09 = box firmware used until 2026)

## Build
Compile with the libraries of this repo, e.g. with the arduino-cli of the Arduino IDE:

    ACLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
    $ACLI compile --fqbn arduino:avr:mega --libraries ./libraries Master
    $ACLI compile --fqbn arduino:avr:pro:cpu=16MHzatmega328 --libraries ./libraries Mini

Bench build (one box, no display / driver / motor / switch): add
`--build-property "compiler.cpp.extra_flags=-DBENCH=1"` to both.
Master BENCH: box 1 on Serial2 (TX2 pin 16 -> Mini RX, RX2 pin 17 <- Mini TX, GND), debug on USB 115200.
Mini BENCH: limit switch simulated at the power-up position, LED toggles with every step.
`-DSIM_EARLY_LIMIT_STATE=UP_2` simulates a tangled thread (early limit switch).

## Motion (Mini v0.18)
* Step pulses by Timer1 hardware (pin 9 = OC1A), continuous across DOWN_1..5 / UP_1..5
* Each segment ramps V1 -> V2 with a smoothstep of the step period: no acceleration jumps at the joins
* Speed mismatch between segments (e.g. DOWN_4 V2=199, DOWN_5 V1=150) is blended over max. 40 half steps
* Period per half step = 50000/V + TRIER_COMP_US (190us): same speeds as the Exp09 firmware used in Trier.
  Calibrate TRIER_COMP_US if an old box runs faster/slower.
* Velocity 1..3 now works (Exp09 overflowed delayMicroseconds -> sawtooth speed in UP_1)
* Limit switch: confirmed after 2ms, stops every UP segment (UP_1..3 -> kError "limit early", tangled thread?),
  no limit after 40 x UP_5 / 80 x INIT_2 -> STOPPED
* Per phase report in the master debug log: duration, underruns, ISR latency, ignored limit switch pulses,
  position at the limit switch (0 = no step loss)



## Debugging
Use debugging port (micro USB) with 115200bps

### Options
Send char via serial monitor
* v/V - Box: verbose on/off (verbose: timing report per segment)
* i INIT, g RUNNING, s STOP, c/C calibration start/stop
* p print params, d load defaults (RAM), w write params to EEPROM, ? request status
* any other key: help

## Update / Sketch Upload
### Master
Use USB B connector
Disconnect all boxes before upload!
(Serial port is used for box communication)


### Boxes
See photo in Doku folder
Update in box with connected Arduino is possible
Use FTDI adapter


## States

### Initialisation
First steps after power on to ensure that all boxes are in the same state

INIT_0:   
  wait for boxes

INIT_1:   
  down for 20cm

INIT_2:
  up until limit switch triggered
    go to state "XX"


### Down

DOWN_1:

DOWN_2:

DOWN_3:

DOWN_4:

DOWN_5:

DOWN_READY:
  stop and wait for Master

### Up

UP_1:

UP_2:

UP_3:

UP_4:

UP_5:

RELAX:
  down

UP_READY:
  stop
  wait for master


----


  Fallhöhe im Kühlhaus 2,85m = ca.4742 Schritte


  Spindel Umfang ca. 0,24m
  1m Faden entspricht 4,16 Umdrehungen
  1 Umdrehung sind 200 Schritte, 400 Halbe, 800 Viertelschritte

  1m = 4,16Umdr = 1664 halbe Schritte // 2m = 3328 Schritte   // 3m = 4992 //  4m = 6656Schritte
  5,4m = 22,46 Umdr = 8985 halbe Schritte //



----

it is not possible to include CmdMessenger into Box class, because function to attach needs to be static


# Video 2026
https://youtu.be/xVbGahsgtTw


----
Monitoring Connection for Arduino Pro Mini
38400baud
