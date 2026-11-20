#!/usr/bin/env python
"""Hellhound clip timelines from t5cap logs (tools/t5cap/zz_t5cap.gsc "D" lines).

    python tools/t5cap_dogclips.py t5_dogclip_frozen.txt codol_dogclip_frozen.txt [-v]

Each "D <ms> <ent> clip:time ..." line names the zombie_dog clips whose
getanimtime moved since the previous frame.  This joins them into segments
(one continuous play of one clip; a non-looping clip restarts when its time
goes back) and reports, per clip: how often it played, for how long, at what
rate (normalized time advanced x clip length / wall time), and where in the
clip it stopped; then what started after what, and event-relative timings
(visible -> first move, dog_combat -> run_attack, death -> death clip end).
Clip lengths are C1's (common_gametype_zm), which equal T5's for the clips
the T5 dump measured (trot 1.8 s / 245 u, run 2.0 s / 724 u, run_stop 0.6 s).
"""
import re
import sys
from collections import defaultdict, Counter

# name: (seconds, looping) -- C1 xanims (tools: xanim header numframes / 30)
CLIPS = {'trot': (1.8, 1), 'run': (2.0, 1), 'run_lean_l': (2.0, 1), 'run_lean_r': (2.0, 1),
         'run_stop': (0.6, 0), 'run_attack': (1.5, 0), 'run_attack_low': (2.8333, 0),
         'run_attack_miss': (0.9667, 0), 'attack_player_miss_turnl': (1.3333, 0),
         'attack_player_miss_turnr': (1.2, 0), 'idle': (3.0667, 0), 'attackidle': (1.1333, 0),
         'attackidle_bark': (1.2, 0), 'attackidle_growl': (1.5667, 0), 'turn_90_left': (0.8667, 0),
         'turn_90_right': (0.7, 0), 'turn_180_left': (0.9333, 0), 'turn_180_right': (0.8, 0),
         'run_turn_90_left': (0.8667, 0), 'run_turn_90_right': (0.7, 0),
         'run_turn_180_left': (0.9, 0), 'run_turn_180_right': (0.9333, 0),
         'death_front': (1.1667, 0), 'death_hit_back': (1.1667, 0), 'death_hit_left': (1.3333, 0),
         'death_hit_right': (1.1667, 0), 'tesla_death_a': (2.0, 0), 'tesla_death_b': (1.2667, 0),
         'tesla_death_c': (1.8667, 0), 'tesla_death_d': (1.4333, 0), 'tesla_death_e': (1.6667, 0),
         'pain_hit_front': (1.1667, 0), 'pain_hit_back': (0.8333, 0), 'pain_hit_left': (1.0, 0),
         'pain_hit_right': (1.1667, 0), 'run_pain_front': (1.1667, 0),
         'traverse_up_40': (0.6333, 0), 'traverse_up_80': (0.6, 0), 'traverse_down_40': (0.3, 0),
         'traverse_down_126': (0.9333, 0), 'traverse_down_190': (0.9333, 0),
         'run_jump_40': (0.9333, 0), 'run_jump_window_40': (1.1667, 0)}


def pct(v, p):
    v = sorted(v)
    return v[min(len(v) - 1, int(len(v) * p))] if v else None


def load(path):
    dogs = set()
    frames = defaultdict(list)   # ent -> [(ms, {clip: t})]
    events = defaultdict(list)   # ent -> [(ms, name, rest)]
    bites = []
    eds = defaultdict(dict)       # ent -> {ms: 2D distance to its enemy}
    for line in open(path, errors='replace'):
        p = line.split()
        if not p:
            continue
        if p[0] == 'F' and 'an=zombie_dog' in line:
            m = re.search(r' ed=(-?[0-9]+)', line)
            if m:
                eds[p[2]][int(p[1])] = int(m.group(1))
            continue
        if p[0] == 'E' and len(p) > 3:
            if p[3] == 'spawn' and 'an=zombie_dog' in line:
                dogs.add(p[2])
            if p[2] == 'P' and p[3] == 'pdamage':
                bites.append((int(p[1]), line))
            events[p[2]].append((int(p[1]), p[3], ' '.join(p[4:])))
        elif p[0] == 'D':
            d = {}
            for kv in p[3:]:
                if kv.startswith('eye='):
                    continue
                k, _, v = kv.rpartition(':')
                d[k] = float(v)
            frames[p[2]].append((int(p[1]), d))
    load.eds = eds
    return dogs, frames, events, bites


def segments(frames):
    """[(clip, start_ms, end_ms, t0, t1, advanced_norm)] per ent."""
    out = []
    open_ = {}
    last_ms = None
    for ms, d in frames:
        if last_ms is not None and ms - last_ms > 120:
            for c, s in open_.items():
                out.append(s)
            open_ = {}
        for c in list(open_):
            if c not in d:
                out.append(open_.pop(c))
        for c, t in d.items():
            length, loop = CLIPS.get(c, (1.0, 0))
            s = open_.get(c)
            if s is not None:
                prev = s[4]
                if t == 0.0 and prev > 0.0:
                    # cleared: getanimtime reads 0 once the clip is gone
                    out.append(open_.pop(c))
                    continue
                if t < prev - 1e-6:
                    if loop:
                        s[5] += t + 1.0 - prev
                    else:
                        out.append(open_.pop(c))
                        s = None
                else:
                    s[5] += t - prev
                if s is not None:
                    s[2] = ms
                    s[4] = t
            if s is None and t == 0.0:
                continue
            if s is None:
                # started between the previous frame and this one; the rate
                # is measured from this first sample on (s[5] = advance)
                open_[c] = [c, ms - 50, ms, t, t, 0.0]
        last_ms = ms
    out.extend(open_.values())
    out.sort(key=lambda s: s[1])
    return out


def report(path, verbose):
    dogs, frames, events, bites = load(path)
    print('== %s: %d hounds, %d clip frames' % (path, len(dogs), sum(len(frames[d]) for d in dogs)))
    per = defaultdict(list)
    trans = Counter()
    for d in dogs:
        segs = segments(frames[d])
        prev = None
        for s in segs:
            c, a, b, t0, t1, adv = s
            length, loop = CLIPS.get(c, (1.0, 0))
            wall = (b - a) / 1000.0
            rate = adv * length / ((b - a - 50) / 1000.0) if b - a >= 200 else None
            per[c].append((wall, rate, t1))
            if c.startswith('look') or c.startswith('attack_look') or c.startswith('run_lean'):
                continue
            if prev is not None:
                trans[(prev, c)] += 1
            prev = c
    print('  %-26s %5s %9s %9s %11s' % ('clip', 'plays', 'wall s', 'rate', 'ends at'))
    for c in sorted(per, key=lambda k: -len(per[k])):
        v = per[c]
        walls = [x[0] for x in v]
        rates = [x[1] for x in v if x[1] is not None]
        ends = [x[2] for x in v]
        print('  %-26s %5d %4.2f-%4.2f %4s %4s %5.2f/%5.2f' % (
            c, len(v), pct(walls, .1), pct(walls, .9),
            '%.2f' % pct(rates, .5) if rates else '-', '(%.2f-%.2f)' % (pct(rates, .1), pct(rates, .9)) if rates else '',
            pct(ends, .5), pct(ends, .9)))
    print('  transitions:')
    for (a, b), n in trans.most_common(25 if not verbose else 200):
        print('    %4d %s -> %s' % (n, a, b))
    # event-relative timings
    vis_move, combat_attack, bite_iv = [], [], []
    for d in dogs:
        ev = events[d]
        segs = segments(frames[d])
        vis = [t for t, n, _ in ev if n == 'visible']
        if vis:
            mv = [s[1] for s in segs if s[0] in ('run', 'trot') and s[1] >= vis[0] - 100]
            if mv:
                vis_move.append(mv[0] - vis[0])
        for t, n, _ in ev:
            if n == 'dog_combat':
                at = [s[1] for s in segs if s[0] == 'run_attack' and s[1] >= t - 100]
                if at:
                    combat_attack.append(at[0] - t)
        atk = [s[1] for s in segs if s[0] == 'run_attack']
        bite_iv += [b - a for a, b in zip(atk, atk[1:]) if b - a < 3000]
    if vis_move:
        print('  visible -> first trot/run clip: median %d ms (n %d)' % (pct(vis_move, .5), len(vis_move)))
    if combat_attack:
        print('  dog_combat -> run_attack start: median %d ms p90 %d (n %d)' % (
            pct(combat_attack, .5), pct(combat_attack, .9), len(combat_attack)))
    if bite_iv:
        print('  run_attack start -> next run_attack: median %d p10 %d p90 %d ms (n %d)' % (
            pct(bite_iv, .5), pct(bite_iv, .1), pct(bite_iv, .9), len(bite_iv)))
    # time share: the main clip each hound frame plays
    share = Counter()
    for d in dogs:
        for ms, dd in frames[d]:
            main = [c for c in dd if not (c.startswith('look') or c.startswith('attack_look') or c.startswith('run_lean'))]
            cat = 'other'
            for c in main:
                if c.startswith('run_attack'):
                    cat = 'attack'
                elif c.startswith('attackidle') and cat != 'attack':
                    cat = 'attackidle'
                elif 'turn' in c and cat not in ('attack', 'attackidle'):
                    cat = 'turn'
                elif c in ('run', 'trot', 'run_stop') and cat == 'other':
                    cat = c
                elif c == 'idle' and cat == 'other':
                    cat = 'idle'
                elif c.startswith('death') or c.startswith('tesla'):
                    cat = 'death'
            share[cat] += 1
    tot = float(sum(share.values())) or 1.0
    print('  frame share: ' + '  '.join('%s %.0f%%' % (k, 100 * v / tot) for k, v in share.most_common()))
    dogbites = [b for b in bites if any(' from=%s ' % d in b[1] + ' ' for d in dogs)]
    alive = sum(len(frames[d]) for d in dogs) * 0.05 / 60.0
    if dogbites:
        dist = [int(re.search(r' d=(-?[0-9]+)', b[1]).group(1)) for b in dogbites]
        print('  bites on the player: %d (%.1f per hound-minute of clip frames); distance median %d p10 %d p90 %d' % (
            len(dogbites), len(dogbites) / alive if alive else 0, pct(dist, .5), pct(dist, .1), pct(dist, .9)))
    starts = []
    for d in dogs:
        ed = load.eds.get(d, {})
        for sg in segments(frames[d]):
            if sg[0] == 'run_attack':
                near = [ed[t] for t in (sg[1] + 50, sg[1], sg[1] + 33, sg[1] + 16, sg[1] + 66) if t in ed and ed[t] >= 0]
                if near:
                    starts.append(near[0])
    if starts:
        print('  enemy distance as run_attack starts: median %d p10 %d p90 %d (n %d)' % (
            pct(starts, .5), pct(starts, .1), pct(starts, .9), len(starts)))
    look = sum(1 for d in dogs for ms, dd in frames[d] if any(c.startswith('look') for c in dd))
    print('  frames with a normal look additive moving: %d' % look)
    # deaths
    for d in sorted(dogs):
        ev = events[d]
        died = [t for t, n, _ in ev if n == 'died']
        if not died:
            continue
        info = [r for t, n, r in ev if n == 'doginfo']
        kill = [r for t, n, r in ev if n == 'cap_kill']
        gone = [(t, r) for t, n, r in ev if n == 'deathlog_end']
        segs = [s for s in segments(frames[d]) if s[2] >= died[0] - 50]
        ds = ['%s %d-%dms t%.2f' % (s[0], s[1] - died[0], s[2] - died[0], s[4]) for s in segs]
        print('  death %s %s %s: %s; log end %s' % (d, info[0] if info else '', kill[0] if kill else '',
                                                    ', '.join(ds) or 'no clip',
                                                    '%d ms %s' % (gone[0][0] - died[0], gone[0][1]) if gone else '-'))


def main():
    verbose = '-v' in sys.argv
    for p in sys.argv[1:]:
        if p != '-v':
            report(p, verbose)


if __name__ == '__main__':
    main()
