#!/usr/bin/env python3
"""Summarise a t5cap actor telemetry log (T5 via Plutonium, or CoDOL).

Both games run the same capture script (maps/_zombiemode_t5cap.gsc here,
scripts/sp/zz_t5cap.gsc in Plutonium's storage), so one parser reads both.

    python tools/t5cap_analyze.py obj/t5capture/t5_proto_r1.txt [more...]

Prints, per log: travel speed per run cycle (straight-line moving frames
only), turn rate, melee timing and range, board-tear timing, the spawn ->
window -> inside timeline, and player damage.
"""
import math
import re
import statistics as st
import sys
from collections import defaultdict

KV = re.compile(r'(\w+)=(\S+)')

# Run-cycle key -> clip, per game (T5 _zombiemode.gsc:1067, CoDOL :1523).
_COMMON = {
    'walk1': 'walk_v1', 'walk2': 'walk_v2', 'walk3': 'walk_v3', 'walk4': 'walk_v4',
    'walk5': 'walk_v6', 'walk6': 'walk_v7', 'walk7': 'walk_v9', 'walk8': 'walk_v9',
    'run1': 'walk_fast_v1', 'run2': 'walk_fast_v2', 'run3': 'walk_fast_v3',
    'run4': 'run_v2', 'run6': 'run_v3',
    'sprint1': 'sprint_v1', 'sprint2': 'sprint_v2', 'sprint3': 'sprint_v1', 'sprint4': 'sprint_v2',
}
CLIPS = {
    't5': dict(_COMMON, run5='run_v4'),
    'codol': dict(_COMMON, run5='run_v1'),
}


def vec(s):
    return tuple(float(x) for x in s.split(','))


def parse(path):
    frames = defaultdict(list)   # id -> [(t, dict)]
    events = []                  # (t, id, kind, rest)
    head = []
    player = []
    for line in open(path, encoding='utf-8', errors='replace'):
        line = line.rstrip('\n')
        if not line:
            continue
        tag = line[0]
        if tag == 'F':
            parts = line.split(' ')
            t, i = int(parts[1]), parts[2]
            d = dict(KV.findall(line))
            d['o'] = vec(d['o'])
            d['y'] = float(d['y'])
            frames[i].append((t, d))
        elif tag == 'E':
            parts = line.split(' ', 4)
            t, i, kind = int(parts[1]), parts[2], parts[3]
            rest = parts[4] if len(parts) > 4 else ''
            events.append((t, i, kind, rest))
        elif tag == 'P':
            parts = line.split(' ')
            player.append((int(parts[1]), vec(parts[2])))
        elif tag == 'H':
            head.append(line)
    return head, frames, events, player


def yawdiff(a, b):
    d = (b - a + 180.0) % 360.0 - 180.0
    return d


def pct(xs, p):
    if not xs:
        return float('nan')
    xs = sorted(xs)
    k = (len(xs) - 1) * p
    f = math.floor(k)
    c = min(f + 1, len(xs) - 1)
    return xs[f] + (xs[c] - xs[f]) * (k - f)


def summarize(path):
    head, frames, events, player = parse(path)
    game = 't5' if any('zombie_cod5' in h for h in head) else 'codol'
    clips = CLIPS[game]
    print('=' * 72)
    print(path)
    for h in head:
        print('  ' + h)
    # --- speed per run anim
    spd = defaultdict(list)
    spd_ms = defaultdict(list)
    turn = []
    turn_by_species = defaultdict(list)
    for i, fl in frames.items():
        for (t0, a), (t1, b) in zip(fl, fl[1:]):
            dt = (t1 - t0) / 1000.0
            if dt <= 0 or dt > 0.11:
                continue
            if a.get('as') not in ('zombie_move', 'move', '-') or b.get('as') != a.get('as'):
                continue
            if a.get('mv') not in ('run', 'walk'):
                continue
            dx = b['o'][0] - a['o'][0]
            dy = b['o'][1] - a['o'][1]
            dz = b['o'][2] - a['o'][2]
            v = math.hypot(dx, dy) / dt
            dyaw = abs(yawdiff(a['y'], b['y'])) / dt
            key = clips.get(a.get('ra'), a.get('ra', '?')) if a.get('an') == 'zombie' else a.get('an', '?')
            if a.get('legs') == '0':
                key = 'crawl:' + key
            if abs(dz) < 6 and dyaw < 30 and 5 < v < 600:
                spd[key].append(v)
                spd_ms[(a.get('an'), a.get('ms'), a.get('legs'))].append(v)
            if v > 5:
                turn.append(dyaw)
                turn_by_species[a.get('an')].append(dyaw)
    print('  speed (u/s) on straight moving frames, by run cycle:')
    for k in sorted(spd):
        xs = spd[k]
        if len(xs) < 5:
            continue
        print('    %-22s n=%5d  median %6.1f  p10 %6.1f  p90 %6.1f' % (k, len(xs), st.median(xs), pct(xs, .1), pct(xs, .9)))
    print('  speed by (species, bucket, legs):')
    for k in sorted(spd_ms, key=str):
        xs = spd_ms[k]
        if len(xs) < 5:
            continue
        print('    %-30s n=%5d  median %6.1f  p90 %6.1f' % (str(k), len(xs), st.median(xs), pct(xs, .9)))
    for sp, xs in turn_by_species.items():
        if xs:
            print('  turn deg/s (%s): p50 %.0f  p90 %.0f  p99 %.0f  max %.0f' % (sp, pct(xs, .5), pct(xs, .9), pct(xs, .99), max(xs)))
    # --- events
    # CoDOL lands T5-scheduled hits ("swing_hit") instead of the C1 clip's
    # "fire" notes; count those as the fires when the log has them.
    if any(e[2] == 'swing_hit' for e in events):
        events = [e for e in events if not (e[2] == 'melee' and e[3].startswith('fire'))]
        events = [(t, i, 'melee', 'fire ' + r) if k == 'swing_hit' else (t, i, k, r) for t, i, k, r in events]
    by = defaultdict(list)
    for e in events:
        by[e[1]].append(e)
    melee_fire = []
    melee_fire_d = []
    melee_start = []
    melee_cycle = []
    tear_board = []
    tear_cycle = []
    tear_first = []
    win_to_inside = []
    spawn_to_window = []
    spawn_to_inside = []
    trav = []
    pdmg = []
    for i, evs in by.items():
        if i == 'P':
            for t, _, kind, rest in evs:
                if kind == 'pdamage':
                    pdmg.append((t, rest))
            continue
        spawn_t = None
        last_fire = None
        last_mstart = None
        first_tear = None
        last_tear_board = None
        win_t = None
        trav_start = None
        for t, _, kind, rest in evs:
            kv = dict(KV.findall(rest))
            if kind == 'spawn':
                spawn_t = t
            elif kind == 'melee':
                note = rest.split(' ')[0]
                if note == 'fire':
                    if 'ed' in kv:
                        melee_fire_d.append(int(kv['ed']))
                    if last_fire is not None and t - last_fire < 4000:
                        melee_fire.append(t - last_fire)
                    last_fire = t
            elif kind == 'tear':
                note = rest.split(' ')[0]
                if first_tear is None:
                    first_tear = t
                    if spawn_t is not None:
                        spawn_to_window.append((t - spawn_t) / 1000.0)
                if note == 'board':
                    if last_tear_board is not None and t - last_tear_board < 8000:
                        tear_cycle.append(t - last_tear_board)
                    last_tear_board = t
                    tear_board.append(t)
            elif kind == 'zombie_start_traverse':
                trav_start = t
            elif kind == 'zombie_end_traverse':
                if trav_start is not None:
                    trav.append(t - trav_start)
            elif kind in ('completed_emerging_into_playable_area',) or (kind == 'phase' and rest.startswith('hunt')):
                if spawn_t is not None:
                    spawn_to_inside.append((t - spawn_t) / 1000.0)
                if first_tear is not None:
                    win_to_inside.append((t - first_tear) / 1000.0)
    # melee start distance: first frame with as=zombie_melee / melee after not
    for i, fl in frames.items():
        prev = None
        for t, d in fl:
            asn = d.get('as')
            if asn in ('zombie_melee', 'melee') and prev not in ('zombie_melee', 'melee'):
                if d.get('ed') not in (None, '-1'):
                    melee_start.append(int(d['ed']))
            prev = asn
    def show(name, xs, unit=''):
        if xs:
            print('  %-34s n=%4d  median %8.2f  p10 %8.2f  p90 %8.2f %s' % (name, len(xs), st.median(xs), pct(xs, .1), pct(xs, .9), unit))
        else:
            print('  %-34s n=   0' % name)
    show('melee: interval between fires', melee_fire, 'ms')
    show('melee: 3D dist at fire', melee_fire_d, 'u')
    show('melee: 2D dist at melee start', melee_start, 'u')
    show('tear: board-to-board interval', tear_cycle, 'ms')
    show('traverse duration', trav, 'ms')
    show('spawn -> first tear', spawn_to_window, 's')
    show('first tear -> inside', win_to_inside, 's')
    show('spawn -> inside', spawn_to_inside, 's')
    if pdmg:
        amts = [int(r.split(' ')[0]) for _, r in pdmg]
        print('  player damage events: %d  amounts %s' % (len(pdmg), sorted(set(amts))))
    near = 0
    for i, fl in frames.items():
        for t, d in fl:
            if d.get('ed') not in (None, '-1') and int(d['ed']) < 80:
                near += 1
    if near:
        nf = sum(1 for e in events if e[2] == 'melee' and e[3].startswith('fire'))
        print('  melee hits per zombie-second within 80u: %.2f (%d hits, %.0f zombie-s); player damage/s %.2f'
              % (nf / (near * 0.05), nf, near * 0.05, len(pdmg) / (near * 0.05)))
    # anim-script time share
    share = defaultdict(int)
    for i, fl in frames.items():
        for t, d in fl:
            share[d.get('as')] += 1
    tot = sum(share.values()) or 1
    print('  animscript share: ' + ', '.join('%s %.0f%%' % (k, 100.0 * v / tot) for k, v in sorted(share.items(), key=lambda kv: -kv[1])))


if __name__ == '__main__':
    for p in sys.argv[1:]:
        summarize(p)
