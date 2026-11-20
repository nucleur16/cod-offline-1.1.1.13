#!/usr/bin/env python
"""MP attack-dog behaviour from t5mpdogcap logs (tools/t5cap/zz_t5mpdogcap.gsc).

    python tools/mpdogcap_compare.py t5_frozen.txt codol_frozen.txt ...

Each log is one scenario run, T5 (Plutonium t5mp) or CoDOL (maps/mp/
_codol_dogcap.gsc); the same lines come out of both.  Per log this prints the
numbers docs/MP_DOGS_T5_PARITY.md compares: the spawn waves (gaps,
respawn delay, count), lifetime and leaving, enemy acquisition, speeds by
state, the bite cycle (attack start distance, bite distance, interval, damage)
and the animscript mix while engaged, kills and the owner's credit.
"""
import re
import sys
from collections import defaultdict, Counter


def pct(v, p):
    v = sorted(v)
    return v[min(len(v) - 1, int(len(v) * p))] if v else None


def fmt(v, nd=0):
    if not v:
        return '-'
    m = pct(v, 0.5)
    return ('%.*f (p10 %.*f, p90 %.*f, n %d)' % (nd, m, nd, pct(v, 0.1), nd, pct(v, 0.9), len(v)))


def vec(s):
    return tuple(float(x) for x in s.split(','))


def analyze(path):
    call = None
    spawns = []           # (t, ent, pos)
    dog_died = []         # level "dog died" times
    leaving = None
    done = None
    allspawned = None
    gone = []
    enemy_first = {}
    frames = defaultdict(list)  # ent -> [(t, x, y, z, en, ed, as)]
    player = []           # (t, x, y)
    states = defaultdict(list)  # ent -> [(t, text)]
    pdmg = []             # (t, amount, from, ai, d)
    vdmg = []
    dogdmg = []           # (t, ent, amount, from)
    died = []             # (t, ent, by)
    vdied = []
    owner_k = []
    owner = None
    victims = set()
    host = None
    for line in open(path, errors='replace'):
        p = line.split()
        if not p:
            continue
        k = p[0]
        if k == 'H':
            if p[1] == 'owner':
                owner = int(p[2])
            elif p[1] == 'victim':
                victims.add(int(p[2]))
            elif p[1] == 'player':
                host = int(p[2])
            continue
        if k == 'P' and len(p) > 2:
            x, y, z = vec(p[2])
            player.append((int(p[1]), x, y))
            continue
        if k == 'F':
            t, ent = int(p[1]), int(p[2])
            d = dict(kv.split('=', 1) for kv in p[3:] if '=' in kv)
            m = re.search(r' as=(.*)$', line.rstrip())
            asn = m.group(1) if m else '-'
            x, y, z = vec(d['o'])
            frames[ent].append((t, x, y, z, int(d.get('en', -1)), int(d.get('ed', -1)), asn.split()[0]))
            continue
        if k == 'S':
            states[int(p[2])].append((int(p[1]), ' '.join(p[3:])))
            continue
        if k != 'E':
            continue
        t = int(p[1])
        if p[2] == 'call':
            call = t
        elif p[2] == 'L':
            ev = ' '.join(p[3:])
            if ev == 'dog died':
                dog_died.append(t)
            elif ev == 'dogs leaving':
                leaving = t
            elif ev == 'dogs done':
                done = t
            elif ev == 'all dogs spawned':
                allspawned = t
            elif ev.startswith('gone'):
                gone.extend([t] * int(p[4]))
        elif p[2] == 'P' and p[3] == 'pdamage':
            d = dict(kv.split('=', 1) for kv in p[6:] if '=' in kv)
            pdmg.append((t, int(p[4]), int(d['from']), int(d['ai']), int(d['d'])))
        elif p[2] == 'V' and p[3] == 'pdamage':
            d = dict(kv.split('=', 1) for kv in p[6:] if '=' in kv)
            vdmg.append((t, int(p[4]), int(d['from']), int(d['ai']), int(d['d'])))
        elif p[2] == 'V' and p[3] == 'died':
            vdied.append(t)
        elif p[2] == 'O':
            d = dict(kv.split('=', 1) for kv in p[4:] if '=' in kv)
            owner_k.append((t, int(d['kills']), int(d['score'])))
        else:
            try:
                ent = int(p[2])
            except ValueError:
                continue
            what = p[3]
            d = dict(kv.split('=', 1) for kv in p[4:] if '=' in kv)
            if what == 'spawn':
                spawns.append((t, ent, vec(d['o']), float(d.get('apr', '1'))))
            elif what == 'enemy' and ent not in enemy_first and 'ed' in d:
                enemy_first[ent] = (t, int(d['ed']))
            elif what == 'damage':
                dogdmg.append((t, ent, int(p[4]), int(d.get('from', -1))))
            elif what == 'died':
                died.append((t, ent, int(d.get('by', -1))))
    out = []
    o = out.append
    o('== %s' % path)
    if call is None:
        o('  no call')
        return out
    sp = sorted(spawns)
    o('  dogs spawned %d; first %+d ms after the call; initial gaps %s' % (
        len(sp), sp[0][0] - call if sp else -1,
        ', '.join('%.2f' % ((sp[i][0] - sp[i - 1][0]) / 1000.0) for i in range(1, min(4, len(sp))))))
    if dog_died:
        later = [s for s in sp[4:]]
        delays = []
        for s in later:
            prior = [t for t in dog_died if t <= s[0]]
            if prior:
                delays.append((s[0] - prior[-1]) / 1000.0)
        o('  respawn after a death: %s s' % fmt(delays, 2))
    if allspawned:
        o('  all dogs spawned %+.1f s' % ((allspawned - call) / 1000.0))
    if leaving:
        o('  dogs leaving %+.1f s after the call' % ((leaving - call) / 1000.0))
        lg = [(g - leaving) / 1000.0 for g in gone if g >= leaving]
        o('  leaving -> deleted: %s s' % fmt(lg, 1))
    if done:
        o('  dogs done %+.1f s after the call' % ((done - call) / 1000.0))
    o('  spawn points: %s' % ', '.join(sorted(set('%.0f,%.0f' % (s[2][0], s[2][1]) for s in sp))))
    o('  animplaybackrate %s' % fmt([s[3] for s in sp], 2))
    # acquisition
    acq = []
    never = 0
    for t, ent, pos, apr in sp:
        if ent in enemy_first:
            acq.append(((enemy_first[ent][0] - t) / 1000.0, enemy_first[ent][1]))
        else:
            never += 1
    o('  enemy acquired by %d of %d dogs; after %s s, at %s units' % (
        len(acq), len(sp), fmt([a[0] for a in acq], 1), fmt([a[1] for a in acq])))
    # speeds
    spd = defaultdict(list)
    for ent, fr in frames.items():
        for a, b in zip(fr, fr[1:]):
            dt = (b[0] - a[0]) / 1000.0
            if dt <= 0 or dt > 0.11:
                continue
            v = ((b[1] - a[1]) ** 2 + (b[2] - a[2]) ** 2) ** 0.5 / dt
            if v > 1000:
                continue
            key = b[6]
            if key == 'move':
                key = 'move/chase' if b[4] >= 0 else 'move/patrol'
            spd[key].append(v)
    for key in sorted(spd):
        vs = spd[key]
        moving = [v for v in vs if v > 20]
        o('  speed %-12s %s u/s (moving frames %d of %d)' % (key, fmt(moving), len(moving), len(vs)))
    # state mix while engaged (enemy within 200)
    mix = Counter()
    for ent, fr in frames.items():
        for f in fr:
            if f[4] >= 0 and 0 <= f[5] < 200:
                mix[f[6]] += 1
    tot = sum(mix.values())
    if tot:
        o('  engaged (enemy < 200) frame share: ' + ', '.join('%s %.0f%%' % (k, 100.0 * v / tot) for k, v in mix.most_common()))
    # bite cycle
    att = []
    first_att = []
    melee_d = []
    notes = Counter()
    bite_iv = []
    per = Counter()
    for ent, st in states.items():
        seen_attack = False
        last_bite = None
        for t, s in st:
            w = s.split()
            per[w[0]] += 1
            if w[0] == 'attack':
                d = int(w[1].split('=')[1])
                att.append(d)
                if not seen_attack:
                    first_att.append(d)
                seen_attack = True
            elif w[0] == 'note':
                notes[w[1]] += 1
                if w[1] == 'dog_melee':
                    melee_d.append(int(w[2].split('=')[1]))
                    if last_bite is not None and t - last_bite < 5000:
                        bite_iv.append(t - last_bite)
                    last_bite = t
            elif w[0] in ('move', 'stop', 'turn', 'death'):
                seen_attack = seen_attack and w[0] not in ('move',)
    o('  animscript entries: ' + ', '.join('%s %d' % kv for kv in per.most_common()))
    o('  attack started at %s units (first of an approach %s)' % (fmt(att), fmt(first_att)))
    o('  dog_melee note at %s units; notes %s' % (fmt(melee_d), dict(notes)))
    o('  bite to bite (same dog) %s ms' % fmt(bite_iv))
    # A bite is target damage at a dog_melee note (T5 credits it to the
    # owner, whose own frozen bot may also be shooting; CoDOL to the dog).
    melee_t = sorted(t for ent in states for t, s in states[ent] if s.startswith('note dog_melee') or s.startswith('lowbite'))
    import bisect

    def at_melee(t):
        i = bisect.bisect_left(melee_t, t - 150)
        return i < len(melee_t) and melee_t[i] <= t + 150
    hostbites = [x for x in pdmg if (x[3] == 1 or x[2] == owner) and at_melee(x[0])]
    if melee_t:
        o('  bite attempts (dog_melee notes) %d, landed %d (%.0f%%)' % (len(melee_t), len(hostbites) + len([x for x in vdmg if (x[3] == 1 or x[2] == owner) and at_melee(x[0])]),
            100.0 * (len(hostbites) + len([x for x in vdmg if (x[3] == 1 or x[2] == owner) and at_melee(x[0])])) / len(melee_t)))
    if hostbites:
        o('  bites on the target: %d, damage %s, attacker %s' % (
            len(hostbites), fmt([x[1] for x in hostbites]),
            'owner' if all(x[2] == owner for x in hostbites) else 'dog' if all(x[3] for x in hostbites) else 'mixed'))
        engaged_ms = 0
        bt = sorted(x[0] for x in hostbites)
        o('  target bitten %.1f times a minute between first and last bite' % (
            60000.0 * (len(bt) - 1) / max(1, bt[-1] - bt[0])))
    if vdmg:
        vb = [x for x in vdmg if (x[3] == 1 or x[2] == owner) and at_melee(x[0])]
        o('  victim hits from dogs %d, damage %s' % (len(vb), fmt([x[1] for x in vb])))
    if vdied:
        o('  victims died %d times' % len(vdied))
    if owner_k:
        o('  owner kills %d -> %d, score %d -> %d' % (owner_k[0][1], owner_k[-1][1], owner_k[0][2], owner_k[-1][2]))
    if dogdmg:
        o('  dog damage events %d: amounts %s' % (len(dogdmg), Counter(x[2] for x in dogdmg).most_common(8)))
    if died:
        o('  dog deaths %d (by: %s)' % (len(died), Counter(x[2] for x in died).most_common()))
    deaths = [s for ent in states for t, s in states[ent] if s.startswith('death')]
    if deaths:
        o('  death animscripts: %s' % deaths[:12])
    return out


def main():
    for path in sys.argv[1:]:
        print('\n'.join(analyze(path)))


if __name__ == '__main__':
    main()
