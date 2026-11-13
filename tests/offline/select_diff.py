#!/usr/bin/env python3
"""Differential test of the room selection: map_handoff.select() of the Python lobby against
src/offline/dw_handoff.cpp HandoffSelect(), descriptor by descriptor.

    PYLOBBY_DIR=<private copy of the Python lobby> python tests/offline/select_diff.py <dwtest.exe> <scratch dir>

The Python lobby copy must be the branch the port follows (fix/hosted-survival-spawn-20261028). Both
sides get the same pristine resources, no zone folder (the shipped zone manifest) and no game install
(every supported Survival level counts, no usermaps); a second pass gives both the same made-up game
install (usermaps, Survival placements). Nothing listens on any port.
"""
import os
import subprocess
import sys
import zlib

# (kind, ...): ("latch", map, gametype|None) or ("select", gametype, map, league, mode, maxplayers, desc[26])
CASES = [
    ("select", "war", "mp_rust", 255, 0, 12, 0),
    ("select", "war", "mp_cargoship_sh", 255, 0, 12, 0),
    ("select", "tdm", "mp_overgrown", 255, 0, 12, 0),
    ("select", "sd", "mp_underpass", 255, 0, 12, 0),
    ("select", "war", "mp_crash_tropical", 255, 0, 12, 0),      # the ctor default: no room map on the wire
    ("select", "war", "mp_not_installed", 255, 0, 12, 0),
    ("select", "combat", "mp_crash_tropical", 1, 0, 6, 0),
    ("select", "combat", "mp_crash_tropical", 3, 0, 12, 0),
    ("select", "playlist", "1", 1, 1, 6, 0),
    ("select", "playlist", "5", 1, 1, 6, 0),
    ("select", "playlist", "30", 1, 1, 6, 0),
    ("select", "playlist", "9999", 1, 1, 6, 0),
    ("select", "coop", "coop_training_mp_killhouse_sh", 255, 2, 4, 0),
    ("select", "coop", "coop_spec_mp_estate_tropical", 255, 2, 4, 0),
    ("select", "coop", "coop_take_mp_fav_tropical", 255, 2, 4, 0),
    # Survival: C1's button (a coop descriptor naming coop_survival_<level>) and a room naming gametype survival
    ("select", "coop", "coop_survival_mp_afghan", 255, 2, 4, 0),
    ("select", "coop", "coop_survival_mp_afghan", 255, 2, 4, 0),  # not a rotation: the same level again
    ("select", "coop", "coop_survival_mp_fav_tropical", 255, 2, 4, 0),
    ("select", "coop", "coop_survival_mp_cross_fire", 255, 2, 4, 0),
    ("select", "coop", "coop_survival_mp_storm_spring", 255, 2, 4, 0),
    ("select", "coop", "coop_survival_mp_rust", 255, 2, 4, 0),
    ("select", "coop", "coop_survival_mp_seatown", 255, 2, 4, 0),
    ("select", "survival", "mp_fav_tropical", 255, 0, 4, 0),
    ("select", "survival", "mp_storm_spring", 255, 0, 4, 0),
    ("select", "survival", "mp_rust", 255, 0, 4, 0),
    ("select", "survival", "mp_seatown", 255, 0, 4, 0),
    ("select", "survival", "mp_crash_tropical", 255, 0, 4, 0),
    # Zombies and its difficulty (desc[26])
    ("select", "zm", "zm_asylum_sh", 255, 0, 4, 0),
    ("select", "zm", "zm_prototype_sh", 255, 0, 4, 1),
    ("select", "zm", "zm_prototype_sh", 255, 0, 4, 2),
    ("select", "zm", "zm_prototype_sh", 255, 0, 4, 3),
    ("select", "zm", "zm_sumpf_sh", 255, 0, 4, 2),
    ("select", "zm", "zm_zombie_training", 1, 0, 4, 0),
    # usermaps and level zones without a _load zone
    ("select", "war", "mp_seatown", 255, 0, 12, 0),
    ("select", "war", "mp_brl_derail", 255, 0, 12, 0),
    ("select", "dom", "mp_abandon_sh_mk", 255, 0, 12, 0),
    # the typeId-15 room latch, and its category
    ("latch", "zm_prototype_sh", "zm"),
    ("select", "war", "mp_crash_tropical", 255, 0, 12, 0),      # a zm latch is not this mp room
    ("select", "war", "mp_not_installed", 255, 0, 12, 0),
    ("select", "zm", "zm_not_installed", 255, 0, 4, 1),
    ("latch", "mp_crash_tropical", "war"),
    ("select", "war", "mp_crash_tropical", 255, 0, 12, 0),      # the same category: the latch names the room
    ("select", "sd", "mp_crash_tropical", 255, 0, 12, 0),
    ("select", "zm", "zm_not_installed", 255, 0, 4, 0),         # an mp latch is not this zm room
    ("latch", "mp_terminal_cls", "survival"),
    ("select", "survival", "mp_crash_tropical", 255, 0, 4, 0),
    ("select", "war", "mp_crash_tropical", 255, 0, 12, 0),
    ("latch", "mp_rust", None),
    ("select", "war", "mp_crash_tropical", 255, 0, 12, 0),
]


def crc(name):
    return zlib.crc32(name.encode("latin1")) & 0xFFFFFFFF


def python_side(lobby, scratch, game):
    """Runs in a child process: the module reads its environment at import."""
    code = r'''
import os, sys, json
sys.path.insert(0, sys.argv[1])
from handoff import map_handoff as M
out = []
for c in json.loads(sys.argv[2]):
    if c[0] == "latch":
        M.latch_room(c[1], c[2], 12, "test")
        continue
    _, gt, m, league, mode, maxp, diff = c
    d = [0] * 28
    d[2], d[5], d[11], d[26] = maxp, mode, league, diff
    d[23] = 3615625517 if gt == "playlist" else M.zlib.crc32(gt.encode("latin1")) & 0xFFFFFFFF
    d[24] = int(m) if m.isdigit() else M.zlib.crc32(m.encode("latin1")) & 0xFFFFFFFF
    d = [(7, v) for v in d]          # (tag, value) pairs, as dw-capture keeps a descriptor
    try:
        s = M.select(d, latch=True)
        out.append("%s %s %s %s %d %s %d -" % (s["map"], s["session_map"], s["gametype"], s["type"],
                   -1 if s["difficulty"] is None else s["difficulty"], s["source"], 1 if s["resolved"] else 0))
    except ValueError:
        out.append("ERROR")
print("\n".join(out))
'''
    import json
    env = dict(os.environ)
    for k in list(env):
        if k.startswith("CODOL_"):
            del env[k]
    state = os.path.join(scratch, "py")
    os.makedirs(state, exist_ok=True)
    env.update(CODOL_MAP_POLICY="room", CODOL_MAP_PREFER="mp_killhouse_sh", CODOL_MAP_TYPES="mp,coop,zm,survival",
               CODOL_SELECTION_STATE=os.path.join(state, "selection.json"),
               CODOL_ROTATION_STATE=os.path.join(state, "rotation.json"),
               CODOL_HANDOFF_FILE=os.path.join(state, "startgame.txt"),
               CODOL_LOBBY_LOG=os.path.join(state, "no-such.log"),
               CODOL_STATE_DIR=state, PYTHONDONTWRITEBYTECODE="1")
    if game:
        env.update(CODOL_GAME_DIR=game, CODOL_ZONE_DIR=os.path.join(game, "client_shipRetail", "chinese"))
    else:
        env.update(CODOL_ZONE_DIR=os.path.join(scratch, "no-such-zones"), CODOL_USERMAPS_DIR=os.path.join(scratch, "no-such-usermaps"))
    for name in ("selection.json", "rotation.json"):
        try:
            os.remove(os.path.join(state, name))
        except OSError:
            pass
    r = subprocess.run([sys.executable, "-B", "-c", code, lobby, json.dumps(CASES)], env=env, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, cwd=lobby)
    if r.returncode:
        sys.exit("the Python side failed:\n" + r.stderr.decode("utf-8", "replace"))
    return r.stdout.decode().splitlines()[:sum(1 for c in CASES if c[0] == "select")]


def port_side(dwtest, lobby, scratch, game):
    path = os.path.join(scratch, "cases.txt")
    with open(path, "w", newline="\n") as fh:
        for c in CASES:
            if c[0] == "latch":
                fh.write("latch %s %s\n" % (c[1], c[2] or "-"))
            else:
                fh.write("select %s %s %d %d %d %d\n" % c[1:])
    args = [dwtest, "--differential", "--resources", os.path.join(lobby, "resources")]
    if game:
        args += ["--zone-dir", os.path.join(game, "client_shipRetail", "chinese"), "--game-dir", game]
    r = subprocess.run(args + ["--select-file", path], stdout=subprocess.PIPE, check=True)
    out = []
    for line in r.stdout.decode().splitlines():
        out.append("ERROR" if line.endswith(" ERROR") else line)
    return out


def make_game(scratch):
    """A made-up install: stock zones, a usermaps folder, a map-pack zone without a _load zone,
    Survival placements for some levels."""
    game = os.path.join(scratch, "game")
    zones = os.path.join(game, "client_shipRetail", "chinese")
    def touch(*parts):
        p = os.path.join(*parts)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, "wb").close()
    for z in ("mp_rust", "mp_rust_load", "mp_fav_tropical", "mp_fav_tropical_load", "mp_cross_fire", "mp_killhouse_sh",
              "mp_crash_tropical", "mp_cargoship_sh", "mp_overgrown", "mp_underpass", "mp_estate_tropical",
              "coop_training_mp_killhouse_sh", "coop_spec_mp_estate_tropical", "coop_take_mp_fav_tropical",
              "zm_asylum_sh", "zm_prototype_sh", "zm_zombie_training", "zm_sumpf_sh", "mp_brl_derail", "mp_playlists",
              "patch_mp_rust", "common_mp", "mp_terminal_cls"):
        touch(zones, z + ".ff")
    touch(game, "usermaps", "mp_seatown", "mp_seatown.ff")
    touch(game, "usermaps", "mp_abandon_sh_mk", "mp_abandon_sh_mk.ff")
    os.makedirs(os.path.join(game, "usermaps", "not_a_map"), exist_ok=True)
    touch(game, "main", "sh1x_rawfiles", "maps", "mp", "gametypes", "survival.gsc")
    for level in ("mp_cross_fire", "mp_seatown", "mp_storm_spring", "mp_terminal_cls"):
        touch(game, "main", "sh1x_rawfiles", "maps", "mp", "survival", level + ".gsc")
    return game


def main():
    dwtest, scratch = sys.argv[1], os.path.abspath(sys.argv[2])
    lobby = os.environ.get("PYLOBBY_DIR")
    if not lobby:
        sys.exit("set PYLOBBY_DIR to a private copy of the Python lobby")
    os.makedirs(scratch, exist_ok=True)
    fails = 0
    for label, game in (("no game install", None), ("made-up game install", make_game(scratch))):
        py, cc = python_side(lobby, scratch, game), port_side(dwtest, lobby, scratch, game)
        selects = [c for c in CASES if c[0] == "select"]
        differ = 0
        for i, c in enumerate(selects):
            a, b = (py[i] if i < len(py) else "<missing>"), (cc[i] if i < len(cc) else "<missing>")
            if a != b:
                differ += 1
                print("DIFF [%s] %s\n    python: %s\n    port:   %s" % (label, " ".join(str(x) for x in c[1:]), a, b))
        print("%s: %d selections, %d differ" % (label, len(selects), differ))
        fails += differ
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
