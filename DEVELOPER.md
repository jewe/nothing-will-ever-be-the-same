# NOTHING WILL EVER BE THE SAME: developer guide

This guide covers the firmware architecture, the protocol, the step engine and how to test.
For operating the installation, see [USER_GUIDE.md](USER_GUIDE.md).

## Repository layout

| Path | Content |
|---|---|
| `Master/Master.ino` | Master v0.16, Arduino Mega 2560 |
| `Mini/Mini.ino` | Box v0.18, Arduino Pro Mini 328P 5V 16 MHz |
| `Master_init/` | Writes `NWEBTS_DEFAULTS` to the master EEPROM and reads it back |
| `libraries/NWEBTS/NWEBTS.h` | **Shared** states, command ids, `StateParams`, defaults, EEPROM layout, `state2Name()` |
| `libraries/CmdMessenger` | Vendored v4.0.0. **`MAXSTREAMBUFFERSIZE` is changed to 64** (was 512, too much RAM for the Pro Mini). |
| `libraries/…` | Encoder, LiquidCrystal_I2C, RBD_Button / RBD_Timer (master only) |
| `tools/` | Bench tools for the host (Python 3 + pyserial) |
| `archive/` | Master v0.15, Mini v0.12, Exp01–Exp09 (**Exp09 v0.17 was the box firmware until 2026**), Master_init v0.8 |
| `Einstellungen_*.xlsx` | Settings sheets from the installations (source of the defaults) |

## Build

The `arduino-cli` bundled with Arduino IDE 2 works. You need AVR core 1.8.x and no extra libraries, because everything is in `libraries/`.

```sh
ACLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
$ACLI compile --fqbn arduino:avr:mega --libraries ./libraries Master
$ACLI compile --fqbn arduino:avr:mega --libraries ./libraries Master_init
$ACLI compile --fqbn arduino:avr:pro:cpu=16MHzatmega328 --libraries ./libraries Mini

# upload: add  -u -p /dev/cu.wchusbserialXXX
# bench build: add  --build-property "compiler.cpp.extra_flags=-DBENCH=1"
# simulate an early limit switch hit on the box:  "...=-DBENCH=1 -DSIM_EARLY_LIMIT_STATE=UP_2"
```

Current resource use:

| Sketch | Flash | RAM |
|---|---|---|
| Mini | 15 KB | 66% (687 bytes free) |
| Master | 26 KB | 35% |

RAM on the Pro Mini is the tight resource. Watch it when you add Strings, buffers or logs.

## System overview

```
                 Serial2 ─── Box 1
  Master (Mega)  Serial3 ─── Box 2        38400 baud, CmdMessenger (text)
                 Serial1 ─── Box 3
                 Serial  ─── Box 4  (shared with USB!)
                 SoftSerial 13/12 ── debug 115200
```

- **The master owns the phases and the parameters.** Parameters live in EEPROM and can be edited on the LCD.
- **Each box runs a whole phase on its own** (DOWN_1…DOWN_5, or UP_1…UP_5 + RELAX). It reports every state change.
- **Phases are synchronised only at the barriers:** the master waits until *all* boxes report `UP_READY` or `DOWN_READY`.

### State machine (box)

```
INIT_0 ─(master)→ INIT_1 → INIT_2 (repeat until limit) → RELAX → UP_READY
UP_READY ─(master)→ DOWN_1 → … → DOWN_5 (driver off at the end) → DOWN_READY
DOWN_READY ─(master)→ UP_1 → UP_2 → UP_3 → UP_4 → UP_5 (repeat until limit) → RELAX → UP_READY
any ─(master)→ STOPPED / INIT_0 / CAL_1   (aborts a running motion)
CAL_1 (fall slowly, repeat) ─(master)→ CAL_2 (log steps) → CAL_3
```

Rules for the limit switch:
- A **confirmed** hit during any UP state or INIT_2 stops the motion and goes to RELAX.
- A hit in UP_1 to UP_3 also sends `kError "limit early in UP_x"`.
- If the switch never triggers, the box goes to STOPPED and sends `kError` after `MAX_REPEATS_UP5` (40) chunks of UP_5, or `MAX_REPEATS_INIT` (80) chunks of INIT_2.

### Master modes

| Mode | What happens |
|---|---|
| `INIT` | Sends `INIT_0`. As soon as all boxes are in INIT_0, it configures them and sends `INIT_1`. When all boxes are UP_READY, it switches to `RUNNING`. |
| `RUNNING` | All UP_READY → wait DL + random(RD) → `DOWN_1`. All DOWN_READY → wait → `UP_1`. The waits keep reading the serial ports (`waitPolling`). |
| `CONFIG` | LCD editor. Each save writes to EEPROM. **Motion parameters reach the boxes only at the next INIT.** |
| `CALIBRATION` | Only from UP_READY: `CAL_1`, then Back / `C` sends `CAL_2`. |
| `STOP` | Sends `STOPPED` once. |
| Watchdog | No phase change for 120 s in RUNNING → INIT. |

## Protocol

CmdMessenger defaults: field separator `,`, command separator `;`, with a newline (`printLfCr`). Ids are in `NWEBTS.h`.

| Id | Command | Direction | Arguments |
|---|---|---|---|
| 0 | `kAck` | box → master | – |
| 1 | `kError` | box → master | text |
| 2 | `kStatus` | box → master | state, lastState, duration of lastState (s, float), time in state (s, float) |
| 3 | `kLog` | box → master | text |
| 4 | `kReset` | master → box | – (software reset) |
| 5 | `kState` | master → box | state; the box replies `kAck`, then `kStatus` |
| 6 | `kGetStatus` | master → box | – |
| 7 | `kConfigure` | master → box | state, mode, steps, vel1, vel2. **State 0** = global options: `mode`=1 means verbose. The box replies `kAck`. |

When the box sends a status:
- on every state change;
- every 500 ms in INIT_0, so the master finds boxes after power-up;
- every 30 s otherwise (heartbeat).

How `kState` is handled:
- **Master side:** `setAllBoxesToState()` writes `kState` to all UARTs back to back, then collects the acks via callbacks for `ACK_TIMEOUT` (300 ms). Missing boxes get one blocking retry.
- **If that also fails,** the box state becomes `STATE_UNKNOWN` and the master requests a status. It does *not* keep the old state, because RUNNING would then send the phase again.
- **Box side:** it ignores a repeated phase start (DOWN_1 during DOWN_x, UP_1 during UP_x, INIT_1 during INIT_x) and logs `ignored repeated …`.

Pitfall: CmdMessenger's blocking `sendCmd(..., true, kAck, timeout)` *discards* every other message that arrives while it waits (`checkForAck`). Use it only where losing a status is acceptable: `configure()`, and the retry.

## Box step engine (Mini v0.18)

### Hardware step generation

- **STEP is on pin 9 = OC1A.** Timer1 runs in CTC mode with prescaler 8, so 1 tick = 0.5 µs.
- **The queue:** `loop()` code pushes intervals `{ticks, flags}` into a 32-entry ring buffer `q[]`. Each compare match triggers `ISR(TIMER1_COMPA_vect)`, which:
  1. forces the pin LOW (`COM1A=clear` + `FOC1A`), which ends the pulse the hardware just started;
  2. counts the step (`stepsDone`, `posHalf += dirSign`) if the last interval had `F_PULSE`;
  3. pops the next entry: `OCR1A = ticks-1`, and `COM1A = set on match` if `F_PULSE`, otherwise disconnected (pin LOW).
- The rising edge is therefore exact to the tick, independent of interrupt latency. The pulse width is the ISR entry latency, a few µs; the TMC2100 needs ≥ 100 ns.
- **Intervals longer than 30 ms** (V=1: 50 ms) are split into pulse-less entries plus a final pulse.
- **Underrun** (queue empty, `streamOpen`): the ISR waits 100 µs without a pulse and counts `underruns`.
- **Stop** (queue empty, `!streamOpen`): the timer stops and `motionRunning` becomes false.

API:

| Function | What it does |
|---|---|
| `drive(up, free, constant)` | Queues one segment. It **returns once the last step is queued, not when it has run**, so the next segment follows without a gap. |
| `motionDrain()` | Lets the queue run out. Needed before a direction change, before `DOWN_READY`/`UP_READY`, and before reports. |
| `motionAbort()` | Drops the queue and stops at the next compare match. |

**Never call `digitalWrite(9, …)`.** The Arduino core turns off the timer output (`turnOffPWM`).

### Profile

The period per half step comes from the configured velocity V (1–199):

```
P(V) = 50000 / V + TRIER_COMP_US   [µs]      TRIER_COMP_US = 190
```

`TRIER_COMP_US` reproduces the extra time per step of Exp09 (160 µs pulse plus loop overhead), so the settings tuned in Trier keep their look. Calibrate it against an Exp09 box if needed.

Within a segment of N half steps, the period follows a smoothstep: `p = P1 + (P2−P1)·(3u² − 2u³)` with `u = i/N`. The slope is zero at both ends, so the acceleration is zero at every segment join. The duration stays close to the old linear-period ramp, and the peak acceleration drops by about 6× (UP) to 20× (DOWN) compared with Exp09 (see `tools/profile_model.py`).

**Blend:** if a segment's P1 differs by more than 2% from the last queued period while the motion is running, the first `min(BLEND_MAX_STEPS=40, N/4)` steps ramp from the current period to P1 first.

**Driver enable:**
- DOWN_5 runs with `FREE`, so the driver is off and the thread can flow.
- The next `drive()` enables the driver, waits `ENABLE_SETTLE_MS` (80 ms), and starts at UP_1's V1.
- The box always uses half steps (`halfSteps()`: CFG1 open, CFG2 GND = 1/2 interpolated, spreadCycle). The `mode` parameter is transferred and stored, but ignored.

### Limit switch

- The ISR on INT0 (FALLING) only sets `limitPending` and a timestamp.
- `checkLimit()`, called while lifting, confirms a hit only if the pin is **still LOW after 2 ms** (`LIMIT_CONFIRM_US`). Otherwise it counts `ignoredLimits`.
- It also polls the pin level, which catches a switch that is already pressed.
- **At a confirmed hit,** the box logs `posHalf` (0 means no step loss) and sets it to 0.

### Timing report

- The first entry of each segment carries `F_MARK`. The ISR stores `micros()` when it pops a marked entry, which is the moment the previous segment's last pulse fires.
- At the end of a phase, `reportPhase()` logs one line per segment: plan vs real duration, and first/last speed in half steps per second. It only does this when verbose.
- It always logs a summary: `DOWN t=4411ms und=0 lat=8us ign=0`. If `und > 0`, the summary is sent as a `kError`.

### Responsiveness

`poll()` runs between queued steps and while waiting for space in the queue. It does three things:
- calls `cmdMessenger.feedinSerialData()`, so STOP and INIT work during a motion;
- aborts if `state` was changed by the master;
- checks the limit switch while lifting.

Serial output during a segment is limited to the acks and statuses caused by incoming commands. The queue covers this time (≥ 14 ms).

## Master EEPROM

The layout is unchanged since 2018, so old EEPROM contents stay readable. Per state 7…19 (DOWN_1…UP_READY), at `(state − 7) × 32`:

| Offset | Field | Type |
|---|---|---|
| 0 | steps | u16 |
| 4 | mode | u8 |
| 8 | vel1 | u16 |
| 12 | vel2 | u16 |
| 16 | delay | u16 |
| 20 | randomDelay | u16 |

- The values are little endian, which matches EEPROMex `readInt`/`writeInt`.
- **On load,** out-of-range values (for example 0xFFFF on a fresh chip) fall back to the default.
- INIT_1 and INIT_2 are not stored in EEPROM: they come from `NWEBTS_DEFAULTS`.

## Testing without the installation

### Bench builds (`-DBENCH=1`)

| | Master | Mini |
|---|---|---|
| Boxes / sensors | 1 box on Serial2 | Limit switch simulated at the power-up position (`posHalf ≤ 0`); a wire from pin 2 to GND also works |
| Debug | USB at 115200 baud, keys as in the user guide; `x` drops the acks of the next state change | Verbose on; the LED on pin 13 toggles with every step |
| Hardware | No LCD, knob or buttons | No driver or motor |

Wiring: Mega TX2 (16) → Mini RX, Mega RX2 (17) ← Mini TX, 5V, GND. **Unplug the Mini's USB adapter**, otherwise its TX fights the Mega's TX2.

### Tools

| Script | Purpose |
|---|---|
| `tools/monitor.py <port> [s] ["t:key,…"]` | Master debug log with timestamps. It can send keys at set times, for example `"3:x"`. |
| `tools/fakemaster.py <port> [cycles]` | Acts as the master on a box's own USB adapter: INIT + n cycles, printing status, logs and errors. Tests a Mini without a Mega. |
| `tools/profile_model.py` | Models phase durations and peak acceleration for a parameter set (v0.18 vs Exp09). No hardware needed. |

### Reference results (Nov 2026, Trier defaults, bench)

| Check | Result |
|---|---|
| INIT (from the top) | 4.7 s |
| DOWN | 4.41 s; plan vs real within 2 ms per segment |
| UP | 29.79 s |
| Underruns | 0 |
| Max ISR latency | 8–10 µs |
| Early limit (sim. UP_2) | `kError`, then RELAX → UP_READY, next cycle normal |
| Lost ack (key `x`) | Timeout logged, state recovered from the next status, no resend |
| Repeated `DOWN_1` / `UP_1` during a phase | Ignored, motion unchanged |

## Known pitfalls

- **An enum is invisible to `#if`.** Use `#ifdef` for options like `SIM_EARLY_LIMIT_STATE=UP_2`.
- **`delayMicroseconds()` is only accurate up to 16383 µs.** This caused the Exp09 sawtooth in UP_1 (V1=1). It isn't used for stepping anymore.
- **CmdMessenger callbacks must be static functions.** That's why the master has one small wrapper per box ("the ugly part").
- **Box 4 is on the Mega's `Serial`,** the same port as USB. Unplug it before uploading to the Mega.
- **`while(!Serial)` does nothing on these boards.** The box waits 3 s at start-up so the master is ready.

## Ideas / next steps

- Send the parameters to the boxes when CONFIG is left, so no power cycle is needed after changes.
- A shared GO/STOP hardware line through the network cable, for exact sync and an emergency stop (see Notes.txt).
- Show the per-box position error ("limit at pos") on the LCD.
