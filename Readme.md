# NOTHING WILL EVER BE THE SAME


## Debugging
Use debugging port (micro USB) with 115200bps

### Options
Send char via serial monitor
* v/V - Box: verbose on/off
* r/R - Box: responsive while drive loop on/off

see Master.ino:770

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


