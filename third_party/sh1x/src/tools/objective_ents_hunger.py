r"""Write maps/mp/mp_hunger.d3dbsp.ents (mp_hunger, ported from CoDOL 3.5.6.2).

mp_hunger is a DMZ map in 3.5.6.2 (mp/loadingscreensetting.csv), 16000 units
across: a dense town in the west (x -4000..600, y -4100..2700, floor z ~3100)
and open fields to the east.  Its entity string carries the regular mode
prefabs twice, once in the town and once in the fields, plus a third set
parked at (-9000, 9000) under the level, and only 1 tdm spawn, 1 dom spawn,
the start spawns of one insertion point and 18 dm spawns spread over the
whole map.  The gametype scripts need one set (sd.gsc getEnt()s its bomb
pickup trigger; every bombzone/flag becomes an objective), so this override
keeps the town set, which 3.5.6.2's designers placed and is complete except
for a dom B flag:

  * every mode prefab outside the town (and the parked set) is dropped, with
    the brushes, visuals and defuse triggers it targets;
  * dom: the town's A (south) and C (north) flags stay; the parked B flag
    moves to the town centre, the sab bomb's spot, and the three parked
    flag_descriptors move onto A, B and C (dom.gsc needs one near each flag);
  * hq: the town's five radios stay, each gets a capture volume (the zone has
    radiotriggers only for a field radio and the parked one);
  * spawns: every tdm/dm/dom/sd/sab/ctf/dd spawn is rebuilt from the town's
    own path nodes.  Allies hold the south (the ctf allies flag, the sab
    allies target, dom A), axis the north; sd attackers start north next to
    the sd bomb with both bomb sites in the south half, dd defenders
    north with dd's A and B sites.  Start rows are 8
    nodes at the south and north ends, respawns farthest-point samples of
    the town's nodes, each team's from its own half.

Input is the 1.1.1.13 (ported) zone's entity string:
    CodolSoundDumper decrypt --input mp_hunger.ff --exe <1.1.1.13 codoMP exe> --out mp_hunger.zone
    python tools/objective_ents.py extract mp_hunger.zone mp_hunger.txt
    python tools/objective_ents_hunger.py write mp_hunger mp_hunger.txt [--out DIR] [--check]
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import objective_ents as oe  # noqa: E402

TOWN = (-4200.0, 700.0, -4200.0, 2700.0, 1000.0)    # x0, x1, y0, y1, zmin
MID_Y = -560.0                                      # the sab bomb's y: south half below, north above
SOUTH_ROW_AT = (-500.0, -3900.0)
NORTH_ROW_AT = (-1700.0, 2550.0)
DOM_B = (-1241.37, -562.249, 3088.0)                # the town's sab bomb, on the floor
OBJECTIVE_GROUPS = ("sd", "sab", "bombzone", "dd_bombzone", "ctf", "dom onslaught", "hq")
SPAWNS = ("mp_tdm_spawn", "mp_dm_spawn", "mp_dom_spawn", "mp_sd_spawn", "mp_sab_spawn", "mp_ctf_spawn", "mp_dd_spawn")


def in_town(p):
    x0, x1, y0, y1, zmin = TOWN
    return x0 < p[0] < x1 and y0 < p[1] < y1 and p[2] > zmin


def origin(e):
    o = oe.get(e, "origin")
    return oe.vec(o) if o else None


def drop_outside(ents):
    """Every mode prefab entity outside the town, and what it targets."""
    drop = set()
    for e in ents:
        o = origin(e)
        if o is None or in_town(o):
            continue
        if oe.get(e, "script_gameobjectname") in OBJECTIVE_GROUPS or oe.get(e, "targetname") == "radiotrigger":
            drop.add(id(e))
    changed = True
    while changed:
        changed = False
        targets = {oe.get(e, "target") for e in ents if id(e) in drop and oe.get(e, "target")}
        for e in ents:
            if id(e) not in drop and oe.get(e, "targetname") in targets and not in_town(origin(e) or (0, 0, 0)):
                drop.add(id(e))
                changed = True
    return [e for e in ents if id(e) not in drop], len(drop)


def town_nodes(ents):
    return [oe.vec(oe.get(e, "origin")) for e in ents
            if oe.get(e, "classname") == "node_pathnode" and in_town(oe.vec(oe.get(e, "origin")))]


def row(nodes, at, count=8, gap=96.0):
    out = []
    for n in sorted(nodes, key=lambda n: math.dist(n[:2], at)):
        if all(math.dist(n[:2], o[:2]) >= gap for o in out):
            out.append(n)
        if len(out) == count:
            break
    return out


def farthest(nodes, count, avoid):
    """Farthest-point samples of `nodes`, starting away from `avoid`."""
    out = []
    pool = [n for n in nodes if all(math.dist(n[:2], a[:2]) >= 300 for a in avoid)]
    if not pool:
        return out
    out.append(min(pool, key=lambda n: (n[0], n[1])))
    while len(out) < count:
        best = max(pool, key=lambda n: min(math.dist(n, o) for o in out))
        if min(math.dist(best, o) for o in out) < 200:
            break
        out.append(best)
    return out


def plan_hunger(ents):
    notes = []
    flags = {oe.get(f, "script_label"): f for f in oe.select(ents, targetname="flag_primary") if not in_town(origin(f))}
    descs = {oe.get(d, "script_linkname"): d for d in oe.select(ents, targetname="flag_descriptor")}
    town_flags = {oe.get(f, "script_label"): f for f in oe.select(ents, targetname="flag_primary") if in_town(origin(f))}
    # dom: the parked B flag to the town centre; the descriptors onto A, B, C
    b = flags["_b"]
    oe.put(b, "origin", oe.fmt(DOM_B))
    for link, flag in (("1", town_flags["_a"]), ("2", b), ("3", town_flags["_c"])):
        x, y, z = origin(flag)
        oe.put(descs[link], "origin", oe.fmt((x, y, z + 112)))
    ents, dropped = drop_outside([e for e in ents if e is not b])
    ents.append(b)
    notes.append("dropped %d mode prefab entities outside the town (the fields' set and the parked set)" % dropped)

    # hq: a capture volume for each town radio (on its case, which rests on the floor)
    for radio in oe.select(ents, targetname="hq_hardpoint"):
        case = oe.one([p for p in oe.select(ents, targetname=oe.get(radio, "target"))
                       if oe.get(p, "model") == "com_plasticcase_beige_big"], "hq case")
        ents.append(oe.radio_trigger(origin(case)))

    nodes = town_nodes(ents)
    south = row(nodes, SOUTH_ROW_AT)
    north = row(nodes, NORTH_ROW_AT)
    objectives = [origin(e) for e in ents if oe.get(e, "script_gameobjectname") in OBJECTIVE_GROUPS and origin(e)]
    pool = farthest(nodes, 40, south + north + objectives)
    south_pool = [p for p in pool if p[1] < MID_Y - 300]
    north_pool = [p for p in pool if p[1] > MID_Y + 300]
    sab_targets = [origin(e) for e in oe.select(ents, targetname="sab_bomb_allies") + oe.select(ents, targetname="sab_bomb_axis")]
    clear = lambda pts: [p for p in pts if all(math.dist(p[:2], t[:2]) >= 400 for t in sab_targets)]
    centre = (-1241.0, -562.0, 3088.0)

    ents, n = oe.drop_classes(ents, SPAWNS)
    notes.append("dropped %d placeholder spawns; %d town nodes, rows %d/%d, respawn pool %d (%d south, %d north)"
                 % (n, len(nodes), len(south), len(north), len(pool), len(south_pool), len(north_pool)))
    add = lambda cls, pts, yaw=None: ents.extend(
        oe.spawn(cls, p, oe.facing(p, centre) if yaw is None else yaw) for p in pts)
    # team rows: allies south (facing north), axis north
    for cls in ("mp_tdm_spawn_allies_start", "mp_dom_spawn_allies_start", "mp_sab_spawn_allies_start",
                "mp_ctf_spawn_allies_start", "mp_dd_spawn_attacker_start"):
        add(cls, south, 90)
    for cls in ("mp_tdm_spawn_axis_start", "mp_dom_spawn_axis_start", "mp_sab_spawn_axis_start",
                "mp_ctf_spawn_axis_start", "mp_dd_spawn_defender_start"):
        add(cls, north, 270)
    # sd: attackers north with the bomb, defenders south with both sites
    add("mp_sd_spawn_attacker", north, 270)
    add("mp_sd_spawn_defender", south, 90)
    # respawns
    add("mp_tdm_spawn", pool)
    add("mp_dm_spawn", pool)
    add("mp_dom_spawn", pool)
    add("mp_sab_spawn_allies", clear(south_pool))
    add("mp_sab_spawn_axis", clear(north_pool))
    add("mp_ctf_spawn_allies", south_pool)
    add("mp_ctf_spawn_axis", north_pool)
    # dd: its A and B sites stand in the north half (C, the overtime site, is deleted by dd.gsc)
    add("mp_dd_spawn_attacker", south_pool)
    add("mp_dd_spawn_defender", north_pool)
    return ents, notes


oe.PLANS["mp_hunger"] = plan_hunger

if __name__ == "__main__":
    rc = oe.main(sys.argv[1:])
    if rc == 0 and len(sys.argv) > 2 and sys.argv[1] == "write":
        # the shared writer names its own file as the generator; name this one
        out_dir = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else oe.OUT
        path = os.path.join(out_dir, sys.argv[2] + ".d3dbsp.ents")
        with open(path, encoding="latin-1", newline="") as f:
            text = f.read()
        text = text.replace("Generated by tools/objective_ents.py", "Generated by tools/objective_ents_hunger.py", 1)
        with open(path, "w", encoding="latin-1", newline="") as f:
            f.write(text)
    sys.exit(rc)
