#!/usr/bin/env python3
"""Summarise an RC-XD capture log (tools/t5cap/zz_rccap.gsc), T5 or CoDOL.

    python tools/rccap_analyze.py <log> [more logs...]

Per ride: deploy timings, acceleration (time to 100/200/300/... u/s, speed at
0.25/0.5/1.0/1.5 s), top speed, coast deceleration and stopping distance,
reverse speed, braking, steady turning rate and radius, boost peak and
duration, jump height. Blast logs: damage per distance. Speeds are measured
from the logged origins (horizontal, 0.1 s central differences), so both
games are measured the same way whatever their own speed reporting.
"""
import math
import re
import sys

KV = re.compile(r'(\w+)=(\S+)')


def vec(s):
    return tuple(float(x) for x in s.split(','))


def parse(path):
    rides = []
    cur = None
    blast = []
    head = []
    for line in open(path, encoding='utf-8', errors='replace'):
        line = line.rstrip('\n')
        if not line:
            continue
        tag = line[0]
        parts = line.split(' ')
        if tag == 'H':
            head.append(line)
            continue
        if tag == 'R':
            cur = {'name': dict(KV.findall(line))['ride'], 't0': int(parts[1]), 'v': [], 'm': [], 'e': [], 'd': []}
            rides.append(cur)
            continue
        if cur is None:
            continue
        if tag == 'V':
            d = dict(KV.findall(line))
            cur['v'].append((int(parts[1]), vec(d['o']), vec(d['a']), float(d.get('sp', 0)), d))
        elif tag == 'M':
            cur['m'].append((int(parts[1]), ' '.join(parts[2:])))
        elif tag == 'E':
            cur['e'].append((int(parts[1]), ' '.join(parts[2:])))
        elif tag == 'D':
            cur['d'].append(dict(KV.findall(line)))
        elif tag == 'B':
            blast.append(dict(KV.findall(line)))
    return head, rides, blast


def angdiff(a, b):
    d = (a - b) % 360.0
    if d > 180:
        d -= 360
    return d


def kin(v):
    """(t, speed_xy, forward_speed, yaw_rate, z) per sample, central differences."""
    out = []
    for i in range(1, len(v) - 1):
        t0, o0, a0 = v[i - 1][:3]
        t1, o1, a1 = v[i][:3]
        t2, o2, a2 = v[i + 1][:3]
        dt = (t2 - t0) / 1000.0
        if dt <= 0:
            continue
        dx, dy = (o2[0] - o0[0]) / dt, (o2[1] - o0[1]) / dt
        sp = math.hypot(dx, dy)
        yaw = math.radians(a1[1])
        fwd = dx * math.cos(yaw) + dy * math.sin(yaw)
        yr = angdiff(a2[1], a0[1]) / dt
        # Prefer the engine's own vehicle speed (T5 getspeed, signed; CoDOL
        # vehicle_getspeed, magnitude): T5 logs integer origins, so position
        # differences are noisy there. The sign comes from the motion.
        eng = v[i][3]
        if eng != 0 or sp < 5:
            mag = abs(eng)
            sign = -1 if (eng < 0 or (eng > 0 and fwd < -5)) else 1
            sp = mag
            fwd = sign * mag
        out.append((t1, sp, fwd, yr, o1[2], o1))
    return out


def mark_time(ride, text):
    for t, m in ride['m']:
        if m.startswith(text):
            return t
    return None


def window(k, t0, t1):
    return [s for s in k if t0 <= s[0] < t1]


def at(k, t):
    best = None
    for s in k:
        if best is None or abs(s[0] - t) < abs(best[0] - t):
            best = s
    return best


def report_accel(r, k, thr_on, thr_off, label=''):
    w = window(k, thr_on, thr_off)
    if not w:
        return
    top = max(s[2] for s in w)
    line = f'  accel{label}: top fwd {top:.0f} u/s ({top / 17.6:.1f} mph)'
    for target in (100, 200, 300, 400, 500, 600):
        hit = next((s for s in w if s[2] >= target), None)
        if hit:
            line += f'  t{target}={(hit[0] - thr_on) / 1000:.2f}s'
    print(line)
    line = '         speed at'
    for dt in (0.1, 0.2, 0.3, 0.5, 0.75, 1.0, 1.5):
        s = at(w, thr_on + dt * 1000)
        if s and abs(s[0] - (thr_on + dt * 1000)) < 80:
            line += f'  {dt:.2f}s={s[2]:.0f}'
    print(line)


def report_stop(k, t_from, t_to, label):
    w = window(k, t_from, t_to)
    if len(w) < 3:
        return
    v0 = w[0][2]
    stop = next((s for s in w if abs(s[2]) < 5), None)
    o0 = w[0][5]
    line = f'  {label}: from {v0:.0f} u/s'
    if stop:
        dist = math.hypot(stop[5][0] - o0[0], stop[5][1] - o0[1])
        line += f', stop after {(stop[0] - t_from) / 1000:.2f}s over {dist:.0f} u, mean decel {abs(v0) / max(0.05, (stop[0] - t_from) / 1000):.0f} u/s2'
    s05 = at(w, t_from + 500)
    if s05:
        line += f', at 0.5s {s05[2]:.0f}'
    print(line)


def summarize(path):
    head, rides, blast = parse(path)
    print(f'== {path}')
    for h in head:
        print('  ' + h)
    for r in rides:
        k = kin(r['v'])
        ev = ' '.join(f'{e[1].split()[0]}@{e[0] - r["t0"]}' for e in r['e'])
        print(f'-- ride {r["name"]}: {ev}')
        if r['name'] == 'accel':
            on, off, rev, end = mark_time(r, 'throttle 1'), None, mark_time(r, 'throttle -1'), None
            ts = [t for t, m in r['m'] if m == 'throttle 0']
            off = ts[0] if ts else None
            end = ts[1] if len(ts) > 1 else None
            report_accel(r, k, on, off)
            report_stop(k, off, rev, 'coast')
            w = window(k, rev, end or rev + 3000)
            if w:
                mn = min(s[2] for s in w)
                t100 = next((s for s in w if s[2] <= -100), None)
                print(f'  reverse from rest: max {mn:.0f} u/s ({mn / 17.6:.1f} mph)' + (f', -100 at {(t100[0] - rev) / 1000:.2f}s' if t100 else ''))
        elif r['name'].startswith('turn'):
            on = mark_time(r, 'throttle 1')
            off = mark_time(r, 'throttle 0')
            report_accel(r, k, on, off)
            w = window(k, on + 2000, off)
            if w:
                sp = sum(s[1] for s in w) / len(w)
                yr = sum(s[3] for s in w) / len(w)
                rad = sp / max(1e-3, abs(math.radians(yr)))
                print(f'  steady turn (2s..end): speed {sp:.0f} u/s, yaw rate {yr:.0f} deg/s, radius {rad:.0f} u')
            w = window(k, on, on + 1000)
            if w:
                print(f'  first second: yaw change {sum(s[3] for s in w) * 0.05:.0f} deg, peak yaw rate {max(abs(s[3]) for s in w):.0f}')
            report_stop(k, off, off + 1500, 'coast out of turn')
        elif r['name'] == 'rev':
            on = mark_time(r, 'throttle -1')
            off = mark_time(r, 'throttle 0')
            w = window(k, on, off)
            if w:
                mn = min(s[2] for s in w)
                line = f'  reverse: max {mn:.0f} u/s ({mn / 17.6:.1f} mph)'
                for target in (-100, -200, -300, -400):
                    hit = next((s for s in w if s[2] <= target), None)
                    if hit:
                        line += f'  t{target}={(hit[0] - on) / 1000:.2f}s'
                print(line)
            report_stop(k, off, off + 1500, 'coast out of reverse')
        elif r['name'] == 'brake':
            on = mark_time(r, 'throttle 1')
            br = mark_time(r, 'throttle -1')
            report_accel(r, k, on, br)
            w = window(k, br, br + 3000)
            if w:
                v0 = w[0][2]
                z = next((s for s in w if s[2] <= 5), None)
                mn = min(s[2] for s in w)
                line = f'  brake from {v0:.0f}'
                if z:
                    dist = math.hypot(z[5][0] - w[0][5][0], z[5][1] - w[0][5][1])
                    line += f': zero after {(z[0] - br) / 1000:.2f}s over {dist:.0f} u'
                line += f', then reverse max {mn:.0f}'
                print(line)
        elif r['name'] in ('boost', 'jump', 'ads', 'sprint', 'melee') or r['name'].startswith('cmd_'):
            on = mark_time(r, 'throttle 1')
            b = mark_time(r, 'button')
            off = mark_time(r, 'throttle 0')
            pre = window(k, b - 300, b)
            post = window(k, b, off)
            if pre and post:
                v0 = sum(s[2] for s in pre) / len(pre)
                peak = max(post, key=lambda s: s[2])
                fast = [s for s in post if s[2] > v0 * 1.15]
                dur = (fast[-1][0] - fast[0][0]) / 1000 if fast else 0
                z0 = sum(s[4] for s in pre) / len(pre)
                zmax = max(s[4] for s in post)
                print(f'  {r["name"]}: before {v0:.0f} u/s, peak {peak[2]:.0f} at +{(peak[0] - b) / 1000:.2f}s, above +15% for {dur:.2f}s; z rise {zmax - z0:.1f}')
        elif r['name'] == 'timeout':
            ctl = next((e[0] for e in r['e'] if e[1].startswith('control')), None)
            dth = next((e[0] for e in r['e'] if e[1].startswith('cardeath')), None)
            if ctl and dth:
                print(f'  control -> car death {(dth - ctl) / 1000:.2f}s')
        for d in r['d']:
            print('  dmg ' + ' '.join(f'{k}={v}' for k, v in d.items()))
    if blast:
        print('-- blast ring:')
        for b in blast:
            print('  ' + ' '.join(f'{k}={v}' for k, v in b.items()))


if __name__ == '__main__':
    for p in sys.argv[1:]:
        summarize(p)
