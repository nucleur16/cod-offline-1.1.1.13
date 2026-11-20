r"""Write maps/mp/<map>.d3dbsp.ents for usermaps whose objectives sit outside the level.

mp_mideast and mp_cha_quad came from CoDOL 3.3.3.3 with objective prefabs the
level designer never placed.  mp_mideast parks its sd, sab and hq prefabs at
z -826, under the level, ships no sd, sab or dd spawns, and puts both ctf flags
side by side; mp_cha_quad parks every dom/sd/sab/ctf/dd/hq prefab and its mode
spawns around (-8900, 8800), 11000 units off the map, and ships 2 tdm and 1 dm
spawn points.  No CoDOL build places them: 3.5.2.6 (3.5.6.2) ships both zones
with the identical entity strings, its mp_mideast clash addon carries the same
objective prefabs, 2.6.9.7's mp_cha_quad differs only in 17 decorative models
and 2.6.9.7 has no mp_mideast, so the placements below are designed:
the layouts are mirror-symmetric across y=0 with a spawn row at each end, so
objectives follow the stock IW4/C1 conventions -- bomb sites on the defenders'
(north) half with the attackers starting at the south row, the sab bomb at the
centre with a target in each half, ctf flags in each team's base, dom A/C
mirrored and B on the centre line, hq radios on the centre line and mirrored
pairs.  Every position is a floor point next to the level's own path nodes
(the bot graph); tools/objective_ents.py --check prints each one's
nearest node.  Floors were measured on the probe with traces.

The server reads the file from the script roots in place of the zone's entity
string, for mp matches only, and only while the zone's string has the CRC-32
named on its base_crc32 line (src/online/collision/map_ents_override.c).
A prefab keeps its "?N" trigger models and "*N" brush models and moves as a
unit: trigger hulls and brush models are relative to the entity origin.

Input is the zone's entity string, dumped from the 1.1.1.13 zone:
    CodolSoundDumper decrypt --input <game>\client_shipRetail\chinese\mp_mideast.ff --out mp_mideast.zone
(CODOL-AssetExplorer), then
    python tools/objective_ents.py extract mp_mideast.zone mp_mideast.txt
    python tools/objective_ents.py write mp_mideast mp_mideast.txt [--out DIR] [--check]
DIR defaults to bin/home/codol_rawfiles/maps/mp.
"""
import math
import os
import re
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "bin", "home", "codol_rawfiles", "maps", "mp")
PAIR = re.compile(r'"([^"]*)" "([^"]*)"')


# ---------------------------------------------------------------- entity text
def parse(text):
    ents, cur = [], None
    for line in text.split("\n"):
        if line == "{":
            cur = []
        elif line == "}":
            ents.append(cur)
            cur = None
        elif cur is not None:
            m = PAIR.fullmatch(line)
            if not m:
                raise ValueError("unexpected entity line %r" % line)
            cur.append([m.group(1), m.group(2)])
    return ents


def serialize(ents):
    return "\n".join("{\n" + "".join('"%s" "%s"\n' % (k, v) for k, v in e) + "}" for e in ents)


def get(e, key):
    for k, v in e:
        if k == key:
            return v
    return None


def put(e, key, value):
    for kv in e:
        if kv[0] == key:
            kv[1] = value
            return
    e.append([key, value])


def vec(s):
    return tuple(float(x) for x in s.split())


def fmt(v):
    return " ".join(("%.1f" % x).rstrip("0").rstrip(".") for x in v)


# ---------------------------------------------------------------- operations
def select(ents, **want):
    """Entities whose keys equal every keyword (key_ for keys that are Python words)."""
    out = []
    for e in ents:
        if all(get(e, k.rstrip("_")) == v for k, v in want.items()):
            out.append(e)
    return out


def near(ents, anchor, radius=48.0, dz=64.0):
    """Objective entities around a prefab anchor: the brush models and triggers
    a prefab places with no targetname of their own."""
    ax, ay, az = vec(get(anchor, "origin"))
    out = []
    for e in ents:
        o = get(e, "origin")
        if not o or not get(e, "script_gameobjectname"):
            continue
        x, y, z = vec(o)
        if math.hypot(x - ax, y - ay) <= radius and abs(z - az) <= dz:
            out.append(e)
    return out


def move_group(members, anchor, floor):
    """Translate a prefab so its anchor (a model resting on the floor) stands at `floor`."""
    a = vec(get(anchor, "origin"))
    d = tuple(f - b for f, b in zip(floor, a))
    seen = set()
    for e in members:
        if id(e) in seen:
            continue
        seen.add(id(e))
        put(e, "origin", fmt(tuple(p + q for p, q in zip(vec(get(e, "origin")), d))))
        # Baked lighting keys follow the model: the lighting origin moves with
        # it, the ground-lighting sample of the old spot is dropped.
        if get(e, "ltOrigin"):
            put(e, "ltOrigin", fmt(tuple(p + q for p, q in zip(vec(get(e, "ltOrigin")), d))))
        e[:] = [kv for kv in e if kv[0] != "gndLt"]


def one(lst, what):
    if len(lst) != 1:
        raise ValueError("%s: expected one entity, found %d" % (what, len(lst)))
    return lst[0]


def bomb_site(ents, trigger, floor):
    """A bombzone/sab/dd prefab: use trigger -> visual (com_bomb_objective,
    resting on the floor) -> defuse trigger, plus the clip brush beside it."""
    visual = one(select(ents, targetname=get(trigger, "target")), "visual of " + get(trigger, "target"))
    members = [trigger, visual] + near(ents, visual)
    defuse = get(visual, "target")
    if defuse:
        members += select(ents, targetname=defuse)
    move_group(members, visual, floor)


def pickup(ents, model_name, floor):
    """sd_bomb / sab_bomb: the bomb model and its pickup triggers."""
    model = one(select(ents, targetname=model_name), model_name)
    move_group([model] + near(ents, model, 8, 32), model, floor)


def ctf_base(ents, team, floor):
    flag = one(select(ents, targetname="ctf_flag_" + team), "ctf flag " + team)
    move_group([flag, one(select(ents, targetname="ctf_trig_" + team), "ctf trig"),
                one(select(ents, targetname="ctf_zone_" + team), "ctf zone")], flag, floor)


def dom_flag(ents, label, floor):
    flag = one(select(ents, targetname="flag_primary", script_label=label), "dom flag" + label)
    old = vec(get(flag, "origin"))
    descs = select(ents, targetname="flag_descriptor")
    desc = min(descs, key=lambda d: math.dist(vec(get(d, "origin")), old))
    move_group([flag, desc], flag, floor)


def place_hq(ents, points):
    """hq: the shipped radios (laptop on a case, with their target models and
    brushes) moved to the first points, model-only radios for the rest, and a
    trigger_radius capture volume around each (the zone's radiotrigger
    brushes are dropped: koth.gsc needs each radio inside exactly one)."""
    radios = select(ents, targetname="hq_hardpoint")
    ents = [e for e in ents if get(e, "targetname") != "radiotrigger"]
    for i, (point, yaw) in enumerate(points):
        if i < len(radios):
            parts = select(ents, targetname=get(radios[i], "target"))
            case = one([e for e in parts if get(e, "model") == "com_plasticcase_beige_big"], "hq case")
            move_group([radios[i]] + parts, case, point)
        else:
            new_hq(ents, point, yaw, i)
        ents.append(radio_trigger(point))
    return ents


def new_hq(ents, floor, yaw, index):
    """An hq radio built from models alone (case, laptop, phone) for maps
    that ship fewer than the hardpoints they need; trigger_radius below."""
    tn = "codol_hq%d" % index
    x, y, z = floor
    r = math.radians(yaw)
    def at(dx, dy, dz):
        return fmt((x + dx * math.cos(r) - dy * math.sin(r), y + dx * math.sin(r) + dy * math.cos(r), z + dz))
    ents.append([["classname", "script_model"], ["model", "com_plasticcase_beige_big"], ["origin", at(0, 0, 0)],
                 ["angles", "0 %g 0" % yaw], ["script_gameobjectname", "hq"], ["targetname", tn], ["spawnflags", "12"]])
    ents.append([["classname", "script_model"], ["model", "com_laptop_2_open"], ["origin", at(4, -4, 30)],
                 ["angles", "0 %g 0" % yaw], ["targetname", "hq_hardpoint"], ["script_gameobjectname", "hq"],
                 ["target", tn], ["spawnflags", "12"]])
    ents.append([["classname", "script_model"], ["model", "com_cellphone_on"], ["origin", at(18.7, -8.7, 30)],
                 ["angles", "0 %g 0" % yaw], ["script_gameobjectname", "hq"], ["targetname", tn], ["spawnflags", "4"]])


def radio_trigger(floor):
    """Headquarters capture volume (koth.gsc: the radio must touch exactly one)."""
    return [["classname", "trigger_radius"], ["targetname", "radiotrigger"], ["origin", fmt((floor[0], floor[1], floor[2] - 16))],
            ["radius", "160"], ["height", "128"], ["spawnflags", "0"]]


def drop_classes(ents, prefixes):
    keep = [e for e in ents if not any((get(e, "classname") or "").startswith(p) for p in prefixes)]
    return keep, len(ents) - len(keep)


def spawn(cls, origin, yaw):
    return [["classname", cls], ["origin", fmt(origin)], ["angles", "0 %g 0" % (round(yaw) % 360)]]


def facing(origin, target):
    return math.degrees(math.atan2(target[1] - origin[1], target[0] - origin[0]))


# ---------------------------------------------------------------- the maps
# Team halves: axis (game["defenders"] in sd/dd) holds the north (+y) row,
# allies (attackers) the south (-y) row, as both maps' tdm/dom starts do.
def mirror(points):
    """(x, y, z) points of the north half and their south mirrors."""
    return [p for p in points] + [(x, -y, z) for x, y, z in points if y > 0]


def mode_spawns(north_row, south_row, pool, centre, sab_targets):
    """The sd/sab/ctf/dd spawn sets: starts on the team rows, respawns from
    the pool points of the team's own half (300+ off the centre line, so the
    two teams never share a spawn area), sab respawns also 400+ from either
    bomb target, so neither team is born on a target."""
    out = []
    north = [p for p in pool if p[1] >= 300]
    south = [p for p in pool if p[1] <= -300]
    def clear(pts):
        return [p for p in pts if all(math.dist(p[:2], t[:2]) >= 400 for t in sab_targets)]
    def row(cls, pts, yaw):
        out.extend(spawn(cls, p, yaw) for p in pts)
    def respawn(cls, pts):
        out.extend(spawn(cls, p, facing(p, centre)) for p in pts)
    row("mp_sd_spawn_attacker", south_row, 90)
    row("mp_sd_spawn_defender", north_row, 270)
    row("mp_sab_spawn_allies_start", south_row, 90)
    row("mp_sab_spawn_axis_start", north_row, 270)
    respawn("mp_sab_spawn_allies", clear(south))
    respawn("mp_sab_spawn_axis", clear(north))
    row("mp_ctf_spawn_allies_start", south_row, 90)
    row("mp_ctf_spawn_axis_start", north_row, 270)
    respawn("mp_ctf_spawn_allies", south)
    respawn("mp_ctf_spawn_axis", north)
    row("mp_dd_spawn_attacker_start", south_row, 90)
    row("mp_dd_spawn_defender_start", north_row, 270)
    respawn("mp_dd_spawn_attacker", south)
    respawn("mp_dd_spawn_defender", north)
    return out


MODE_SPAWNS = ("mp_sd_spawn", "mp_sab_spawn", "mp_ctf_spawn", "mp_dd_spawn")


def plan_mideast(ents):
    notes = []
    # Spawn rows: the tdm and cha start rows at y +-2304 (11 distinct points per side).
    rows = {}
    for cls in ("mp_tdm_spawn_axis_start", "mp_cha_spawn_axis_start", "mp_tdm_spawn_allies_start", "mp_cha_spawn_allies_start"):
        side = "north" if "axis" in cls else "south"
        rows.setdefault(side, []).extend(vec(get(e, "origin")) for e in select(ents, classname=cls))
    north_row = sorted(set(rows["north"]))
    south_row = sorted(set(rows["south"]))
    # Respawn pool: the level's own 16 tdm spawn points.
    tdm = [vec(get(e, "origin")) for e in select(ents, classname="mp_tdm_spawn")]
    centre = (-300.0, 0.0, 0.0)

    ents, dropped = drop_classes(ents, MODE_SPAWNS)
    notes.append("dropped %d placeholder sd/sab/ctf/dd spawns" % dropped)
    F = FLOORS["mp_mideast"]
    ents += mode_spawns(north_row, south_row, tdm, centre, (F["sab_allies"], F["sab_axis"]))

    # sd (dd falls back to these: the map has no dd_bombzone): sites in the
    # north half, the bomb at the attackers' row.
    bomb_site(ents, one(select(ents, targetname="bombzone", script_label="_a"), "sd A"), FLOORS["mp_mideast"]["sd_a"])
    bomb_site(ents, one(select(ents, targetname="bombzone", script_label="_b"), "sd B"), FLOORS["mp_mideast"]["sd_b"])
    pickup(ents, "sd_bomb", FLOORS["mp_mideast"]["sd_bomb"])
    # sab: bomb on the centre line, each team's target in its own half.
    pickup(ents, "sab_bomb", FLOORS["mp_mideast"]["sab_bomb"])
    bomb_site(ents, one(select(ents, targetname="sab_bomb_allies"), "sab allies"), FLOORS["mp_mideast"]["sab_allies"])
    bomb_site(ents, one(select(ents, targetname="sab_bomb_axis"), "sab axis"), FLOORS["mp_mideast"]["sab_axis"])
    # ctf: one flag in each base (the zone put both beside each other at x -1470).
    ctf_base(ents, "allies", FLOORS["mp_mideast"]["ctf_allies"])
    ctf_base(ents, "axis", FLOORS["mp_mideast"]["ctf_axis"])
    # hq: the three shipped radios plus model-only ones.
    ents = place_hq(ents, FLOORS["mp_mideast"]["hq"])
    return ents, notes


def plan_cha_quad(ents):
    notes = []
    north_row = sorted(set(vec(get(e, "origin")) for e in select(ents, classname="mp_cha_spawn_axis_start")))
    south_row = sorted(set(vec(get(e, "origin")) for e in select(ents, classname="mp_cha_spawn_allies_start")))
    F = FLOORS["mp_cha_quad"]
    pool = mirror(F["pool"])
    centre = (450.0, 0.0, 0.0)

    # Every mode's spawns are rebuilt: the zone has 2 tdm, 1 dm and 1 dom
    # spawn point in the level and parks the rest at (-9500, 8700).
    ents, dropped = drop_classes(ents, MODE_SPAWNS + ("mp_tdm_spawn", "mp_dm_spawn", "mp_dom_spawn"))
    notes.append("dropped %d placeholder spawns" % dropped)
    ents += mode_spawns(north_row, south_row, pool, centre, (F["sab_allies"], F["sab_axis"]))
    for cls, pts, yaw in (("mp_tdm_spawn_axis_start", north_row, 270), ("mp_tdm_spawn_allies_start", south_row, 90),
                          ("mp_dom_spawn_axis_start", north_row, 270), ("mp_dom_spawn_allies_start", south_row, 90)):
        ents += [spawn(cls, p, yaw) for p in pts]
    for cls in ("mp_tdm_spawn", "mp_dm_spawn", "mp_dom_spawn"):
        ents += [spawn(cls, p, facing(p, centre)) for p in pool]

    dom_flag(ents, "_a", F["dom_a"])
    dom_flag(ents, "_b", F["dom_b"])
    dom_flag(ents, "_c", F["dom_c"])
    bomb_site(ents, one(select(ents, targetname="bombzone", script_label="_a"), "sd A"), F["sd_a"])
    bomb_site(ents, one(select(ents, targetname="bombzone", script_label="_b"), "sd B"), F["sd_b"])
    pickup(ents, "sd_bomb", F["sd_bomb"])
    bomb_site(ents, one(select(ents, targetname="dd_bombzone", script_label="_a"), "dd A"), F["sd_a"])
    bomb_site(ents, one(select(ents, targetname="dd_bombzone", script_label="_b"), "dd B"), F["sd_b"])
    pickup(ents, "sab_bomb", F["sab_bomb"])
    bomb_site(ents, one(select(ents, targetname="sab_bomb_allies"), "sab allies"), F["sab_allies"])
    bomb_site(ents, one(select(ents, targetname="sab_bomb_axis"), "sab axis"), F["sab_axis"])
    ctf_base(ents, "allies", F["ctf_allies"])
    ctf_base(ents, "axis", F["ctf_axis"])
    # dd.gsc deletes its "_c" site (IW5 overtime only); it stands at the centre.
    bomb_site(ents, one(select(ents, targetname="dd_bombzone", script_label="_c"), "dd C"), F["dom_b"])
    ents = place_hq(ents, F["hq"])
    return ents, notes


# Floor points (x, y, floor z), traced on the probe: a bullet trace down
# through each point and four points 48 units around it, with headroom checked
# above (2026-10-22).  mp_mideast sd B sits 50 north of a wooden prop and its
# first hq on dom B, clear of the planter 48 east of the plaza centre.
FLOORS = {
    "mp_mideast": {
        "sd_a": (-610, 1330, -480.5),
        "sd_b": (650, 1350, -448),
        "sd_bomb": (40, -2130, -448),
        "sab_bomb": (-608, 0, -448),
        "sab_allies": (-250, -820, -448),
        "sab_axis": (-250, 820, -448),
        "ctf_allies": (0, -1560, -448),
        "ctf_axis": (0, 1560, -448),
        "hq": [((352, 0, -454.2), 0), ((-620, 0, -448), 0), ((-896, 1280, -479.9), 90), ((-896, -1280, -479.9), 90)],
    },
    "mp_cha_quad": {
        "dom_a": (-180, 700, 252.2),
        "dom_b": (1200, 0, 256),
        "dom_c": (-180, -700, 252.2),
        "sd_a": (-200, 640, 253.3),
        "sd_b": (846, 1000, 252.8),
        "sd_bomb": (32, -1750, 250.2),
        "sab_bomb": (1200, 0, 256),
        "sab_allies": (-200, -640, 253.3),
        "sab_axis": (-200, 640, 253.3),
        "ctf_allies": (192, -1536, 249.8),
        "ctf_axis": (192, 1536, 249.8),
        "hq": [((1200, 0, 256), 0), ((-180, 700, 252.2), 0), ((-180, -700, 252.2), 0), ((850, 950, 254.8), 90),
               ((850, -950, 256.6), 90)],
        # Respawn pool, north half (mirrored to the south): farthest-point
        # samples of the level's own path nodes, away from the spawn rows.
        "pool": [(1840, 408, 257), (-896, 176, 289), (496, 448, 256), (992, 1376, 257), (-768, 1072, 289),
                 (64, 1152, 254), (-160, 128, 289), (1232, 672, 257), (-384, 1504, 291), (-480, 608, 289),
                 (448, 1536, 252), (1376, 160, 257)],
    },
}

PLANS = {"mp_mideast": plan_mideast, "mp_cha_quad": plan_cha_quad}


def nodes(ents):
    return [vec(get(e, "origin")) for e in ents if (get(e, "classname") or "").startswith("node_")]


def check(ents):
    ns = nodes(ents)
    for e in ents:
        cls = get(e, "classname") or ""
        tn = get(e, "targetname") or ""
        if cls.startswith("node_") or not get(e, "origin"):
            continue
        objective = tn in ("bombzone", "dd_bombzone", "sd_bomb", "sab_bomb", "sab_bomb_allies", "sab_bomb_axis",
                           "ctf_zone_allies", "ctf_zone_axis", "flag_primary", "hq_hardpoint", "radiotrigger")
        if not (objective or (cls.startswith("mp_") and "spawn" in cls)):
            continue
        o = vec(get(e, "origin"))
        n = min(ns, key=lambda p: math.dist(p, o))
        flag = "  FAR" if math.dist(n, o) > 256 else ""
        print("%-28s %-20s %-24s nearest node %s d=%.0f%s" % (cls, tn + (get(e, "script_label") or ""), fmt(o), fmt(n), math.dist(n, o), flag))


def extract(zone, out):
    data = open(zone, "rb").read()
    i = data.find(b'"classname" "worldspawn"')
    s, e = data.rfind(b"{", 0, i), data.find(b"\0", i)
    open(out, "wb").write(data[s:e])
    print("%s: %d bytes of entity text -> %s" % (zone, e - s, out))


def write(map_name, dump, out_dir, do_check):
    text = open(dump, encoding="latin-1", newline="").read()
    ents = parse(text)
    if serialize(ents) != text:
        raise SystemExit("%s does not round-trip; refusing to build an override from it" % dump)
    crc = zlib.crc32(text.encode("latin-1")) & 0xFFFFFFFF
    ents, notes = PLANS[map_name](ents)
    body = serialize(ents)
    header = ["// %s: map entity override (sh1x server, mp matches only)." % map_name,
              "// Generated by tools/objective_ents.py in CoD4x_Server from the 1.1.1.13 zone's",
              "// entity string; edit the plan there and regenerate rather than editing this file.",
              "// The server uses it in place of the zone's entity string while the zone's string",
              "// has this CRC-32 (src/online/collision/map_ents_override.c).",
              "// base_crc32 %08x" % crc] + ["// " + n for n in notes]
    path = os.path.join(out_dir, map_name + ".d3dbsp.ents")
    with open(path, "w", encoding="latin-1", newline="\n") as f:
        f.write("\n".join(header) + "\n" + body + "\n")
    print("%s: %d entities, base_crc32 %08x -> %s" % (map_name, len(ents), crc, path))
    if do_check:
        check(ents)


def main(argv):
    if len(argv) >= 3 and argv[0] == "extract":
        extract(argv[1], argv[2])
        return 0
    if len(argv) >= 3 and argv[0] == "write":
        out_dir = OUT
        if "--out" in argv:
            out_dir = argv[argv.index("--out") + 1]
        write(argv[1], argv[2], out_dir, "--check" in argv)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
