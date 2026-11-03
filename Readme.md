# NOTHING WILL EVER BE THE SAME

This kinetic installation has 4 boxes. Each box lowers and lifts a load on a thread, and a master keeps the boxes in sync.

* **[USER_GUIDE.md](USER_GUIDE.md):** setup, the display menu, the parameters and speeds, the default settings, error messages and firmware updates.
* **[DEVELOPER.md](DEVELOPER.md):** architecture, protocol, step engine, EEPROM layout, how to build, bench tests and known pitfalls.

| Folder | Content |
|---|---|
| `Master/` | Master v0.16 (Arduino Mega 2560) |
| `Mini/` | Box firmware v0.18 (Arduino Pro Mini 328P 5V 16 MHz) |
| `Master_init/` | Writes the default parameters into the master's EEPROM |
| `libraries/` | All libraries needed. `NWEBTS/NWEBTS.h` holds the shared states, commands and defaults. |
| `tools/` | Bench test scripts (Python) |
| `archive/` | All earlier firmware versions |
| `Doku/` | Photos, pin-outs and the driver datasheet |

Video: https://youtu.be/xVbGahsgtTw

----

## Mechanics notes

* Fall height in the Kühlhaus: 2.85 m ≈ 4742 steps
* Spindle circumference ≈ 0.24 m, so 1 m of thread ≈ 4.16 revolutions
* 1 revolution = 200 full steps = 400 half steps, so 1 full step ≈ 1.2 mm
* 1 m ≈ 1664 half steps, 2 m ≈ 3328, 3 m ≈ 4992, 4 m ≈ 6656, 5.4 m ≈ 8985
* 2018-08: 1700 full steps = 2.10 m

More hardware notes (power supply, driver jumpers, protection resistor) are in [Notes.txt](Notes.txt).
