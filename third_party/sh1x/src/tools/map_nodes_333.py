"""Write waypoints/<map>_nodes.map from CoDOL 3.3.3.3's map entity strings.

CoDOL 3.3.3.3 (the experience build, IIPS packages ff_init_mp_<map>_V*.ifs /
ff_dlc*_mp_<map>_V*.ifs) ships the stock mp levels with a reworked AI node set.
The server replaces a level's own node_* records with waypoints/<map>_nodes.map
when that file exists (script_path_nodes.c, mp only), and builds the bot
graph from it (bot_nav.c, T5 Path_ConnectPaths rule, drop-to-floor and
the component and spawn-coverage gates).

Input: a directory of entity-string dumps, <map>-333.txt (3.3.3.3) and
<map>-111.txt (1.1.1.13, the zone the server mounts), each the raw text from the
worldspawn '{' to the NUL.  The 3.3.3.3 lumps were read with the AssetExplorer
Codol.FastFiles library (IfsArchive + FastFileReader with the 3.3.3.3
codoMP_client_shipRetail.exe key); the 1.1.1.13 ones with alpha0022's
zones/zone_assets.py.

Geometry gate: a map is written only when at least 95% of the 1.1.1.13 spawn
points occur at the same origin in 3.3.3.3 (the levels were recompiled, not
redesigned); the builder's drop-to-floor pass then discards any node that is in
solid or floating on the 1.1.1.13 collision.

usage: python tools/map_nodes_333.py <lump dir> <map> [<map> ...]
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "bin", "home", "main", "waypoints")
BLOCK = re.compile(r"\{(.*?)\}", re.S)
PAIR = re.compile(r'"([^"]*)"\s+"([^"]*)"')


def entities(path):
    text = open(path, encoding="latin-1").read()
    return [(b, dict(PAIR.findall(b))) for b in BLOCK.findall(text)]


def spawn_set(ents):
    out = set()
    for _, kv in ents:
        cls = kv.get("classname", "")
        if cls.startswith("mp_") and "_spawn" in cls and "origin" in kv:
            out.add((cls, tuple(round(float(v)) for v in kv["origin"].split())))
    return out


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    lumps, maps, status = sys.argv[1], sys.argv[2:], 0
    for m in maps:
        new = entities(os.path.join(lumps, m + "-333.txt"))
        old = entities(os.path.join(lumps, m + "-111.txt"))
        s_old, s_new = spawn_set(old), spawn_set(new)
        share = len(s_old & s_new) / max(1, len(s_old))
        nodes = [(b, kv) for b, kv in new if kv.get("classname", "").startswith("node_")]
        if share < 0.95:
            print("%-20s SKIP: only %.0f%% of the 1.1.1.13 spawns match 3.3.3.3" % (m, share * 100))
            status = 1
            continue
        path = os.path.join(OUT, m + "_nodes.map")
        with open(path, "w", encoding="latin-1", newline="\n") as f:
            f.write("// %s: %d node_* records from CoDOL 3.3.3.3 (tools/map_nodes_333.py)\n" % (m, len(nodes)))
            for _, kv in nodes:
                f.write("{\n")
                for k, v in kv.items():
                    f.write('"%s" "%s"\n' % (k, v))
                f.write("}\n")
        old_nodes = sum(1 for _, kv in old if kv.get("classname", "").startswith("node_"))
        print("%-20s %5d nodes (1.1.1.13 zone: %5d); spawns matched %.0f%% -> %s" % (m, len(nodes), old_nodes, share * 100, path))
    return status


if __name__ == "__main__":
    sys.exit(main())
