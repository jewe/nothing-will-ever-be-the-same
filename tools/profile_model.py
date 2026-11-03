#!/usr/bin/env python3
"""Model the motion profile of Mini v0.18 (and the old Exp09) for a parameter set.

Prints the duration of each phase and the peak acceleration, without hardware.
Edit DOWN / UP below (full steps, V1, V2) to try new settings.
"""
COMP = 190            # TRIER_COMP_US
BLEND = 40            # BLEND_MAX_STEPS
DOWN = [(30, 15, 80), (500, 80, 160), (1500, 160, 180), (480, 180, 199), (230, 150, 7)]
UP = [(900, 4, 9), (900, 9, 15), (600, 16, 30), (500, 32, 60), (150, 60, 21)]

def period(v): return 50000 / max(v, 1) + COMP          # us per half step
def smooth(u): return u * u * (3 - 2 * u)

def v018(segs):
    t = 0; last = None; per = []
    for steps, v1, v2 in segs:
        n = steps * 2; p1, p2 = period(v1), period(v2)
        pin = last or p1
        b = min(BLEND, n // 4) if abs(pin - p1) > 0.02 * p1 else 0
        for i in range(n):
            if i < b: a, c, u = pin, p1, (i + 1) / b
            else:     a, c, u = p1, p2, (i - b + 1) / (n - b)
            p = a + (c - a) * smooth(u); per.append(p); t += p; last = p
    return t / 1e6, per

def exp09(segs):  # linear period ramp, delayMicroseconds wraps above 16383us
    t = 0; per = []
    for steps, v1, v2 in segs:
        n = steps * 2; p1, p2 = 50000 / v1, 50000 / v2; a = (p2 - p1) / n; p = p1
        for i in range(n):
            p += a; d = int(p) % 16384 + COMP; per.append(d); t += d
    return t / 1e6, per

def peak_acc(per):  # half steps / s^2
    return max(abs(1e6 / q - 1e6 / p) / (q / 1e6) for p, q in zip(per, per[1:]))

for name, segs in (('DOWN', DOWN), ('UP', UP)):
    tn, pn = v018(segs); to, po = exp09(segs)
    print(f'{name}: v0.18 {tn:5.1f} s (peak acc {peak_acc(pn):7.0f} hs/s2)   '
          f'Exp09 {to:5.1f} s (peak acc {peak_acc(po):7.0f} hs/s2)')
