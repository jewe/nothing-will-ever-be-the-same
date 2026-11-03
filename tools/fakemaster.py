#!/usr/bin/env python3
"""Act as the Master on a box's serial port (38400): test a Mini without a Mega.

usage: python3 fakemaster.py <port> [cycles]
Runs INIT, then <cycles> x DOWN / UP, printing status, logs and errors.
Opening the port resets the Mini (it waits 3 s before it talks).
"""
import serial, sys, time

NAMES = ("STOPPED INIT_0 INIT_1 INIT_2 CAL_1 CAL_2 CAL_3 DOWN_1 DOWN_2 DOWN_3 DOWN_4 DOWN_5 "
         "DOWN_RDY UP_1 UP_2 UP_3 UP_4 UP_5 RELAX UP_READY").split()
INIT_0, INIT_1, DOWN_1, DOWN_READY, UP_1, UP_READY = 1, 2, 7, 12, 13, 19
kError, kStatus, kLog, kState, kConfigure = 1, 2, 3, 5, 7

s = serial.Serial(); s.port = sys.argv[1]; s.baudrate = 38400; s.timeout = 0.05
s.dtr = False; s.open(); time.sleep(0.1); s.dtr = True; time.sleep(0.1); s.dtr = False
t0 = time.time(); buf = b''; cur = None

def pump(until_state=None, timeout=60):
    """read messages; return True when the box reports until_state"""
    global buf, cur
    end = time.time() + timeout
    while time.time() < end:
        buf += s.read(256)
        while b';' in buf:
            msg, buf = buf.split(b';', 1)
            f = msg.decode(errors='replace').strip().split(',')
            try:
                cmd = int(f[0])
                if cmd == kStatus:
                    st, last = int(f[1]), int(f[2])
                    if st != cur:
                        print(f'{time.time()-t0:7.2f} STATUS {NAMES[st]:9s} (last {NAMES[last]} {f[3]}s)', flush=True)
                    cur = st
                elif cmd in (kError, kLog):
                    print(f'{time.time()-t0:7.2f} {"ERROR" if cmd == kError else "LOG  "} {",".join(f[1:])}', flush=True)
            except (ValueError, IndexError):
                continue  # garbage after reset
            if until_state is not None and cur == until_state:
                return True
    return False

def send(*args):
    cmd = ','.join(str(a) for a in args)
    print(f'{time.time()-t0:7.2f} >>> {cmd}', flush=True)
    s.write((cmd + ';\r\n').encode())

pump(INIT_0, 8)
send(kConfigure, 0, 1, 0, 0, 0)          # state 0: verbose on
send(kState, INIT_1); print('INIT', 'ok' if pump(UP_READY, 120) else 'TIMEOUT')
for _ in range(int(sys.argv[2]) if len(sys.argv) > 2 else 1):
    send(kState, DOWN_1); print('DOWN', 'ok' if pump(DOWN_READY, 30) else 'TIMEOUT')
    send(kState, UP_1); print('UP', 'ok' if pump(UP_READY, 120) else 'TIMEOUT')
