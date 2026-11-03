# NOTHING WILL EVER BE THE SAME: user guide

This guide is for the people who set up, adjust and look after the installation.
If you want to change the firmware, read [DEVELOPER.md](DEVELOPER.md).

## How the installation works

There are 4 boxes. Each box has a stepper motor that winds a thread onto a spindle, with a load at the end of the thread. A master box controls all 4 boxes through network cables.

The installation runs in a loop:

1. **Wait at the top** for the UP_READY delay.
2. **Fall** in 5 steps, DOWN_1 to DOWN_5. Each step has its own number of steps and its own start and end speed.
3. **Wait on the ground** for the DOWN_READY delay.
4. **Lift** in 5 steps, UP_1 to UP_5. At the top, the limit switch stops the motor.
5. **RELAX:** the box lowers the thread by about 1 cm, so the switch is free again.
6. Start again at 1.

The master waits until **all** boxes have finished a phase before it starts the next one. This keeps the boxes in sync.

## All phases

Each box is always in exactly one of these phases (also called states). The display and the debug log show their names.
"Master" in the *Next* column means the box waits until the master sends the next phase.

### Start-up (INIT)

| Phase | What the box does | Next |
|---|---|---|
| `INIT_0` | Just switched on. It waits for the master, and reports its state every 0.5 s. The master sends all the settings, then INIT_1. | Master |
| `INIT_1` | Drives down ST steps (default: 100 steps ≈ 12 cm, at speed 5). This frees the limit switch if the box was already at the top. | INIT_2 |
| `INIT_2` | Lifts slowly at speed V2 (default 5) until the limit switch triggers. It lifts in chunks of ST steps and repeats them. After 80 chunks without the switch (≈ 9.6 m with the defaults), it gives up: `ERROR INIT_2: no limit switch`, then STOPPED. | RELAX |

### Fall (DOWN)

The master starts the fall in all boxes at the same moment. The box then runs DOWN_1 to DOWN_5 without a break and without waiting for the master.

| Phase | What the box does | Next |
|---|---|---|
| `DOWN_1` | Start of the fall: speeds up from V1 to V2 (default 15 → 80 over 30 steps) | DOWN_2 |
| `DOWN_2` | Speeds up further (80 → 160 over 500 steps) | DOWN_3 |
| `DOWN_3` | The long, fast part of the fall (160 → 180 over 1500 steps) | DOWN_4 |
| `DOWN_4` | Top speed (180 → 199 over 480 steps) | DOWN_5 |
| `DOWN_5` | Brakes down to a slow speed (150 → 7 over 230 steps). At the end, the motor is switched **off**, so the thread can flow freely and the load settles on the ground. | DOWN_READY |
| `DOWN_READY` | On the ground, motor off. When all boxes are here, the master waits DL (+ a random time up to RD), then starts the lift. | Master |

The limit switch is ignored during the fall.

### Lift (UP)

| Phase | What the box does | Next |
|---|---|---|
| `UP_1` | Switches the motor on, waits 80 ms so it holds firmly, then starts the lift very gently (default 4 → 9 over 900 steps). | UP_2 |
| `UP_2` | Lifts a bit faster (9 → 15 over 900 steps) | UP_3 |
| `UP_3` | Faster (16 → 30 over 600 steps) | UP_4 |
| `UP_4` | Fastest part of the lift (32 → 60 over 500 steps) | UP_5 |
| `UP_5` | Slows down towards the top (60 → 21 over 150 steps), then keeps lifting at V2 in chunks of ST steps until the limit switch triggers. After 40 extra chunks without the switch: `ERROR UP_5: no limit switch`, then STOPPED. | RELAX |
| `RELAX` | The limit switch was hit: the box lowers the thread by ST steps (default 10 steps ≈ 1 cm) so the switch is free again. | UP_READY |
| `UP_READY` | At the top, motor on. When all boxes are here, the master waits DL (+ random RD), then starts the next fall. | Master |

**The limit switch during the lift:**

- In **UP_4 and UP_5**, the switch is the normal end of the lift: the box goes to RELAX.
- In **UP_1 to UP_3**, the switch is hit **too early**. The thread may be tangled or wound up unevenly. This is an error: the box stops, reports `ERROR limit early in UP_x`, and the master sends **all** boxes to INIT_2. They lift slowly to their switches, so every box has a safe start point again. When all boxes are in UP_READY, the loop continues.
- A pulse on the switch shorter than 2 ms is treated as noise and ignored (see `ign=` in the log).

### Other phases

| Phase | What the box does | Next |
|---|---|---|
| `STOPPED` | Motor off, no motion. After the STOP key (`s`), or after an error where the box gave up. Only INIT starts it again. | Master |
| `CAL_1` | Calibration: falls slowly (speed 5) and counts its steps, until you stop it. Starts only from UP_READY. | Master |
| `CAL_2` | Calibration stopped: the box reports the number of full steps it fell (`calibration full steps: …`). | CAL_3 |
| `CAL_3` | Waits after the calibration. Start INIT to bring the box back to the top. | Master |

## Hardware at a glance

| Part | What it is |
|---|---|
| Master | Arduino Mega 2560, 20x4 display, rotary knob, 3 buttons (Mode, Back, OK) |
| Box | Arduino Pro Mini (5V, 16 MHz), SilentStepStick TMC2100 driver, NEMA 23 motor, limit switch |
| Power | One 9 V power supply in the master, which also powers all boxes through the network cables |
| Cables | Master → box: network cable (serial at 38400 baud, 9 V, GND) |

The photos in [Doku/](Doku/) show the wiring and the driver settings.

## Switching on

1. Connect all 4 boxes, then switch on the power.
2. The display shows `Mode INIT`. Each box drives down about 12 cm, then up until it hits the limit switch.
3. When all boxes are at the top, the master switches to `Mode RUNNING` on its own, and the loop starts.

During RUNNING, the display shows the current state of each box, for example `DOWN_3`. It shows `Timeout` if a box doesn't answer.

## Changing the settings on the display

1. Press **Mode** to switch to CONFIG.
2. Turn the knob to choose a state, for example `DOWN_3`. The page with the number 1 is a help page that lists the full names of all parameters.
3. Press **OK**. Now the knob chooses a parameter, marked with ▶.
4. Press **OK** again. Now the knob changes the value, marked with ↕.
5. Press **OK** to save the value. It is stored permanently and survives a power cut.
6. **Back** takes you one level up.
7. Press **Mode** to return to RUNNING.

> **Important:** the master sends new motion settings (ST, V1, V2) to the boxes only during INIT. After you change them, switch the power off and on again.
> DL and RD (the waiting times) take effect straight away.

### Parameters

| Short | Name | Meaning | Range |
|---|---|---|---|
| ST | Steps | Distance of this step, in full motor steps. 1 full step ≈ 1.2 mm of thread (200 steps = 24 cm). | 0–9999 |
| V1 | Vel Start | Speed at the start of this step | 1–199 |
| V2 | Vel End | Speed at the end of this step. The box changes smoothly from V1 to V2. | 1–199 |
| DL | Delay | Waiting time in ms. Only for DOWN_READY (on the ground) and UP_READY (at the top). | 0–9999 |
| RD | RandDelay | Extra random waiting time, from 0 up to this value in ms. Only for DOWN_READY and UP_READY. | 0–9999 |
| MO | StepMode | Always 1/2 (half steps). Can't be changed. | – |

### What the speed numbers mean

| V | cm/s | | V | cm/s |
|---|---|---|---|---|
| 1 | 1.2 | | 30 | 32 |
| 2 | 2.4 | | 60 | 59 |
| 4 | 4.7 | | 100 | 87 |
| 5 | 5.9 | | 150 | 115 |
| 10 | 12 | | 199 | 136 |
| 20 | 22 | | | |

Doubling V does **not** double the speed at high values: 100 → 199 is only about 1.6× faster.

### Tips for good motion

- **Make each V1 the same as the V2 of the step before.** The box smooths out differences over the first 40 half-steps, but large jumps still feel abrupt. Example: DOWN_4 ends at 199 and DOWN_5 starts at 150.
- **Change speeds gradually.** A big change over only a few steps (for example 15 → 80 in 30 steps) is a hard push for the motor. If a box loses steps, give that phase more steps or a smaller change.
- **A low V1 at the start of UP_1 makes a gentle lift.** V1 = 4 matches what Trier looked like. V1 = 1 makes UP_1 very slow: about 50 s for 900 steps.
- **Add up the down distance:** the sum of ST for DOWN_1 to DOWN_5, × 1.2 mm, must be less than the fall height. Example: 2740 steps ≈ 3.3 m.
- **Up must cover more distance than down.** UP_5 repeats until the limit switch is reached.

## Default settings (Trier, September 2026)

These values are in the firmware and are written to the master by `Master_init` (see below). They come from `Einstellungen_NWEBTS_Sept_2026_Trier.xlsx`.

| State | ST | V1 | V2 | DL ms |
|---|---|---|---|---|
| DOWN_1 | 30 | 15 | 80 | |
| DOWN_2 | 500 | 80 | 160 | |
| DOWN_3 | 1500 | 160 | 180 | |
| DOWN_4 | 480 | 180 | 199 | |
| DOWN_5 | 230 | 150 | 7 | |
| DOWN_READY | | | | 5000 |
| UP_1 | 900 | 4 | 9 | |
| UP_2 | 900 | 9 | 15 | |
| UP_3 | 600 | 16 | 30 | |
| UP_4 | 500 | 32 | 60 | |
| UP_5 | 150 | 60 | 21 | |
| RELAX | 10 | 2 | 2 | |
| UP_READY | | | | 1000 |

One cycle takes about 4.4 s down, 5 s on the ground, 30 s up and 1 s at the top.

## Error messages and what to do

You see these messages in the debug log (see [Debug connection](#debug-connection)).

| Message | Meaning | What to do |
|---|---|---|
| `Timeout` on the display, or `Timeout setState …` | A box didn't answer. | Check the network cable and the box's power. The system carries on with the next status the box sends. |
| `ERROR limit early in UP_2` (or UP_1, UP_3) | The limit switch triggered too early. The thread may be tangled or wound up unevenly. The box stops, and the master sends all boxes to INIT_2: they lift slowly to their switches. This can take up to about a minute for a box near the ground. Then the loop continues. | Check the thread on that box. If the error comes back, switch off and fix it. |
| `ERROR UP_5: no limit switch` / `ERROR INIT_2: no limit switch` | The box lifted far beyond the expected distance without hitting the switch. It stopped, to protect the mechanics. | Check the switch and its cable, then switch off and on again. |
| `LOG limit at pos 35 full steps (0 = no loss)` | At the top, the box was 35 steps away from where it expected to be. 0 means it lost no steps. | If the value keeps growing in one direction, the motor is losing steps: lower the speeds or soften the speed changes. |
| `ign=3` in `UP t=… und=0 lat=8us ign=3` | During this phase, the box ignored 3 short pulses on the limit switch cable (shorter than 2 ms), which were noise and not a real hit. | A few are fine. Many of them point to a bad cable or a loose contact. |
| `ERROR … und=5` | The box could not keep up with planning the motion. This should never happen. | Tell the developer. |
| `<MASTER> Watchdog: no progress -> INIT` | For 2 minutes, not all boxes reported the end of a phase. The master restarts INIT. | Check which box is stuck in the log. |

## Debug connection

- **Master:** the debug port runs at 115200 baud on SoftwareSerial pins 13 (RX) and 12 (TX). Connect it with a USB serial adapter and use any serial monitor.
- **Bench build:** the debug output goes to the Mega's own USB port instead.

Type a key in the serial monitor to send a command:

| Key | Action |
|---|---|
| `i` | INIT: all boxes go to the top again |
| `g` | Start RUNNING |
| `s` | STOP: all motors off |
| `c` / `C` | Start / stop calibration. A box falls slowly and counts its steps; it must be in UP_READY first. |
| `v` / `V` | Detailed logs from the boxes on / off. With logs on, you get a timing line per step. |
| `p` | Print all parameters |
| `d` | Load the default settings into the master (not saved yet) |
| `w` | Save the current parameters permanently |
| `?` | Ask all boxes for their status |
| any other key | Help |

To watch one box directly, connect a USB serial adapter to the box at 38400 baud. Connect only RX and GND, so you only listen.

## Updating the firmware

**Master (Arduino Mega):**

1. **Unplug the boxes of branch A first** (box 4 is on the Mega's Serial0, which is shared with USB; see Notes.txt). If in doubt, unplug all boxes.
2. Connect USB-B.
3. Upload `Master/Master.ino`.

**Resetting all settings to the defaults** (or setting up a new Mega):

1. Upload `Master_init/Master_init.ino`.
2. Open the serial monitor at 115200 baud and check that the table ends with `DONE`.
3. Upload `Master/Master.ino` again.

**Box (Arduino Pro Mini):**

- Use an FTDI or USB serial adapter. The photos in [Doku/](Doku/) show how to connect it, and you can update the box while it is installed.
- Board: *Arduino Pro or Pro Mini*, *ATmega328P (5V, 16 MHz)*.
- Upload `Mini/Mini.ino`.

**Build settings:** in the Arduino IDE, the sketchbook folder must contain the libraries from [libraries/](libraries/), or use the build commands in [DEVELOPER.md](DEVELOPER.md). Build without the `BENCH` option: bench builds are only for testing on the desk.
