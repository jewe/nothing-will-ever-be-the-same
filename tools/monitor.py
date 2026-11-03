#!/usr/bin/env python3
"""Show the Master debug output (USB in BENCH builds) with timestamps.

usage: python3 monitor.py <port> [seconds] [keys]
  keys: "t:key,..." sends debug keys after t seconds, e.g. "3:x,60:s"
Opening the port resets the Mega.
"""
import serial, sys, time

port = sys.argv[1]
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 1e9
keys = [(float(a), b) for a, b in (x.split(':') for x in sys.argv[3].split(','))] if len(sys.argv) > 3 else []

s = serial.Serial(); s.port = port; s.baudrate = 115200; s.timeout = 0.1
s.dtr = False; s.open(); time.sleep(0.1); s.dtr = True; time.sleep(0.1); s.dtr = False  # reset
t0 = time.time(); buf = b''
while time.time() - t0 < secs:
    while keys and time.time() - t0 >= keys[0][0]:
        k = keys.pop(0)[1]; s.write(k.encode()); print(f'{time.time()-t0:7.2f} >>> key {k}', flush=True)
    buf += s.read(512)
    while b'\n' in buf:
        line, buf = buf.split(b'\n', 1)
        print(f'{time.time()-t0:7.2f} ' + line.decode(errors='replace').rstrip(), flush=True)
