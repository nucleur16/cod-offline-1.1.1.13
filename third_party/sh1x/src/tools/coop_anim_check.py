#!/usr/bin/env python
# -*- coding: utf-8 -*-
r"""
coop_anim_check.py -- check that every animation the server scripts of a
coop (or zm) match name exists, in the same animtree, on the C1 client.

    python tools/coop_anim_check.py --category coop --map mp_estate_tropical \
        --map coop_take_mp_estate_tropical
    python tools/coop_anim_check.py --category coop --map mp_fav_tropical -v

Why this matters (MEASURED, src/cscr_animtree.cpp AnimTreeParseInternal, and
C1 0x408040/0x408310, docs/ACTOR_SPECIES_20260920.md section 3):
an XAnim tree is built from its .atr with every node nobody names PRUNED
(outside a `complete` block).  The server names nodes from its scripts' %anim
references plus the client's animlists (RegisterServerAnimListReferences);
C1 names them only from the animlists it loads:

    common                 always
    common_actor_shared    animlist flags & 0x0d
    common_human           flags & 0x01   (coop soldier species)
    common_dog             flags & 0x02
    common_civilian        flags & 0x04
    common_zombie          flags & 0x08
    <mapname>              always (the level, or coop_<mode>_<level> with
                           coopClientMapName 1)

The actor animation channel sends the SERVER tree's entry index (11 bits,
script_anim.c anim_param).  So a %anim the server scripts name that the
client lists do not name is not merely "not loaded" on C1: it adds a node to
the server tree that C1's tree lacks, and every later entry index on the wire
then points one node further along in C1's tree -- the actor plays the WRONG
animation, not just a missing one.  This tool reports those names, which must
be zero for a category before it ships.

The tree and lists are read out of the shipped zones with alpha0022's
zone_assets.py (CODOL_ALPHA0022, default D:/CoD/alpha0022-field-orders/zones).
"""
from __future__ import print_function

import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
ALPHA = os.environ.get("CODOL_ALPHA0022", r"D:/CoD/alpha0022-field-orders/zones")
sys.path.insert(0, ALPHA)
sys.path.insert(0, os.path.dirname(ALPHA))

# The animlist flag word each category's species set (actor_scripts.cpp
# ActorSpeciesAnimListMask) and the zones that carry each list.
CATEGORY_FLAGS = {"coop": 0x01, "zm": 0x08 | 0x10, "mp": 0x02}
LISTS = [  # (name, flag mask or 0 for always, zones searched in mount order)
    ("common", 0, ["code_post_gfx_mp"]),
    ("common_actor_shared", 0x0d, ["common_gametype_coop", "common_gametype_zm"]),
    ("common_human", 0x01, ["common_gametype_coop"]),
    ("common_dog", 0x02, ["common_gametype_coop", "common_gametype_mp"]),
    ("common_civilian", 0x04, ["coop_take_mp_fav_tropical", "coop_take_mp_estate_tropical",
                               "coop_spec_mp_fav_tropical"]),
    ("common_zombie", 0x08, ["patch_common_gametype_zm", "common_gametype_zm"]),
]

_zone_cache = {}


def zone_rawfile(zone, name):
    import zone_assets as za
    z = _zone_cache.get(zone)
    if z is None:
        try:
            z = za.open_zone(zone)
        except Exception:
            z = False
        _zone_cache[zone] = z
    if not z:
        return None
    data = z.rawfile(name)
    return data.decode("latin-1") if data is not None else None


def parse_animlist(text):
    """{tree: [anim, ...]} in token order (C1 0x4081f6: '=' sets the tree)."""
    out = {}
    tree = ""
    for tok in text.split():
        if tok.startswith("="):
            tree = tok[1:].lower()
            continue
        out.setdefault(tree, []).append(tok.lower())
    return out


class Node(object):
    __slots__ = ("name", "flags", "children")

    def __init__(self, name, flags):
        self.name, self.flags, self.children = name, flags, []


def parse_atr(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    toks = re.findall(r"[A-Za-z0-9_]+|[{}:]", text)
    pos = [0]

    def block():
        nodes = []
        while pos[0] < len(toks):
            t = toks[pos[0]]
            if t == "}":
                pos[0] += 1
                return nodes
            if t == "{":
                pos[0] += 1
                nodes[-1].children = block()
                continue
            pos[0] += 1
            flags = set()
            if pos[0] < len(toks) and toks[pos[0]] == ":":
                pos[0] += 1
                while pos[0] < len(toks) and toks[pos[0]] not in ("{",):
                    flags.add(toks[pos[0]].lower())
                    pos[0] += 1
            nodes.append(Node(t.lower(), flags))
        return nodes

    return Node("root", set()), block()


def surviving(nodes, names, complete=False):
    """AnimTreeParseInternal's pruning: the set of node names kept."""
    kept = []
    for n in nodes:
        ignore = not complete and n.name not in names
        if n.children:
            sub = surviving(n.children, names,
                            complete or ("complete" in n.flags and not ignore))
            if sub or not ignore:
                kept.append(n.name)
                kept.extend(sub)
        elif not ignore:
            kept.append(n.name)
    return kept


def all_names(nodes, out=None):
    out = set() if out is None else out
    for n in nodes:
        out.add(n.name)
        all_names(n.children, out)
    return out


ANIM_REF = re.compile(r"%\s*([A-Za-z_][A-Za-z0-9_]*)")
USING = re.compile(r'#using_animtree\s*\(\s*"([^"]+)"\s*\)')


def script_refs(path):
    """[(tree, anim, line)] for every %anim in a script, by the #using_animtree
    in force at that point of the source."""
    text = open(path, encoding="latin-1").read()
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r'"(?:\\.|[^"\\])*"', lambda m: m.group(0) if "#using_animtree" in
                  text[max(0, m.start() - 20):m.start()] else '""', text)
    out = []
    events = [(m.start(), "u", m.group(1).lower()) for m in USING.finditer(text)]
    for m in ANIM_REF.finditer(text):
        # `a % b` is the modulo operator: an animation reference starts an
        # operand, so whatever precedes it cannot end one.
        head = text[:m.start()].rstrip()
        before = head[-1:]
        word = re.search(r"[A-Za-z_][A-Za-z0-9_]*$", head)
        if before and (before.isalnum() or before in "_)]") and not (
                word and word.group(0) in ("return", "case", "else")):
            continue
        events.append((m.start(), "a", m.group(1).lower()))
    tree = None
    for start, kind, value in sorted(events):
        if kind == "u":
            tree = value
        elif tree:
            out.append((tree, value, text.count("\n", 0, start) + 1))
    return out


# Scene clips named by STRING (maps/_coopanim.gsc: add_anim / scene_anim /
# has_scene_anim, and codol_getAnim( tree, name )).  They add no node to the
# server tree (codol_getAnim only finds what the client's lists put there), so
# they never shift an index; but a name no shipped animlist carries can never
# resolve and silently falls back.  Those are reported.
COOP_PLAYLISTS = [
    "coop_training_mp_killhouse_sh", "coop_spec1_mp_killhouse_sh",
    "coop_prolog_mp_crash_intro", "coop_spec_mp_crash_tropical",
    "coop_defense_mp_crash_tropical", "coop_spec_mp_estate_tropical",
    "coop_take_mp_estate_tropical", "coop_spec_mp_fav_tropical",
    "coop_take_mp_fav_tropical",
]
SCENE_REFS = [
    re.compile(r'\badd_anim\s*\(\s*[^,]+,\s*[^,]+,\s*"([A-Za-z0-9_]+)"\s*[,)]'),
    re.compile(r'\b(?:scene_anim|has_scene_anim)\s*\(\s*"([A-Za-z0-9_]+)"\s*[,)]'),
]
GETANIM_REF = re.compile(r'\bcodol_getAnim\s*\(\s*"([A-Za-z0-9_]+)"\s*,\s*"([A-Za-z0-9_]+)"\s*\)', re.I)


def scene_string_refs(path):
    """[(tree, anim, line)] for every scene clip a script names by string."""
    text = open(path, encoding="latin-1").read()
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    out = []
    for rx in SCENE_REFS:
        for m in rx.finditer(text):
            out.append(("generic_human", m.group(1).lower(), text.count("\n", 0, m.start()) + 1))
    for m in GETANIM_REF.finditer(text):
        out.append((m.group(1).lower(), m.group(2).lower(), text.count("\n", 0, m.start()) + 1))
    return out


def shipped_list_names():
    """{tree: set(anim)} over every list any coop client can load."""
    names = {}
    for name, _mask, zones in LISTS:
        for z in zones:
            body = zone_rawfile(z, "animlists/%s.animlist" % name)
            if body is not None:
                for t, anims in parse_animlist(body).items():
                    names.setdefault(t, set()).update(anims)
                break
    for pl in COOP_PLAYLISTS:
        for z in ("patch_" + pl, pl):
            body = zone_rawfile(z, "animlists/%s.animlist" % pl)
            if body is not None:
                for t, anims in parse_animlist(body).items():
                    names.setdefault(t, set()).update(anims)
                break
    return names


def reachable(category, maps):
    cmd = [sys.executable, os.path.join(HERE, "script_resolve.py"),
           "--category", category, "--list-reachable"]
    for m in maps:
        cmd += ["--map", m]
    res = subprocess.run(cmd, cwd=REPO, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         universal_newlines=True)
    out = []
    for line in res.stdout.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0].endswith(".gsc"):
            out.append(os.path.join(REPO, parts[1], parts[0]))
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[1],
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--category", default="coop", choices=sorted(CATEGORY_FLAGS))
    p.add_argument("--map", action="append", default=[],
                   help="level and playlist spellings (the first is the level)")
    p.add_argument("--client-map", help="mapname C1 is told (default: the first --map; "
                   "pass the coop_<mode>_<level> spelling for coopClientMapName 1)")
    p.add_argument("--flags", type=lambda s: int(s, 0), help="animlist flag word override")
    p.add_argument("--tree", default="generic_human")
    p.add_argument("-v", "--verbose", action="store_true")
    a = p.parse_args()

    flags = CATEGORY_FLAGS[a.category] if a.flags is None else a.flags
    client_map = a.client_map or (a.map[0] if a.map else "")
    tree = a.tree.lower()

    lists = []
    for name, mask, zones in LISTS:
        if mask and not (flags & mask):
            continue
        for z in zones:
            body = zone_rawfile(z, "animlists/%s.animlist" % name)
            if body is not None:
                lists.append((name, parse_animlist(body)))
                break
    map_zones = [client_map, "patch_" + client_map]
    if client_map.startswith("coop_"):
        map_zones = ["patch_" + client_map, client_map]
    for z in map_zones:
        body = zone_rawfile(z, "animlists/%s.animlist" % client_map)
        if body is not None:
            lists.append((client_map, parse_animlist(body)))
            break

    atr = None
    for z in ("patch_common_mp", "common_mp", "common_gametype_zm", "common_gametype_coop"):
        atr = zone_rawfile(z, "animtrees/%s.atr" % tree)
        if atr:
            break
    if not atr:
        print("no animtrees/%s.atr in the mounted zones" % tree)
        return 2
    _, top = parse_atr(atr)
    atr_names = all_names(top)

    client = set()
    for name, groups in lists:
        client.update(groups.get(tree, []))
    refs = {}
    scene_refs = {}
    for path in reachable(a.category, a.map):
        for t, anim, line in script_refs(path):
            if t == tree:
                refs.setdefault(anim, []).append("%s:%d" % (os.path.relpath(path, REPO), line))
        if a.category == "coop":
            for t, anim, line in scene_string_refs(path):
                scene_refs.setdefault((t, anim), []).append("%s:%d" % (os.path.relpath(path, REPO), line))

    server = client | set(refs)
    ckept, skept = surviving(top, client), surviving(top, server)
    print("animlists C1 loads for %s (flags 0x%x, mapname %s): %s"
          % (a.category, flags, client_map, ", ".join(n for n, _ in lists)))
    print("%s: %d atr nodes; client names %d -> %d entries; server scripts name %d"
          % (tree, len(atr_names), len(client), len(ckept), len(refs)))

    undefined = sorted(n for n in refs if n not in atr_names)
    extra = sorted(n for n in refs if n in atr_names and n not in client)
    if undefined:
        print("\nNOT IN %s.atr (compile error \"not defined in anim tree\"):" % tree)
        for n in undefined:
            print("  %-48s %s" % (n, " ".join(refs[n][:3])))
    if extra:
        print("\nNamed by server scripts but by no animlist C1 loads (the server tree "
              "gains these nodes, C1's does not: every later entry index is shifted):")
        for n in extra:
            print("  %-48s %s" % (n, " ".join(refs[n][:3])))
    if a.verbose:
        print("\nreferenced and client-loaded:")
        for n in sorted(set(refs) & client):
            print("  %-48s %s" % (n, refs[n][0]))
    unknown_scene = []
    if scene_refs:
        shipped = shipped_list_names()
        unknown_scene = sorted(k for k in scene_refs if k[1] not in shipped.get(k[0], ()))
        here = sum(1 for k in scene_refs if k[0] == tree and k[1] in client)
        print("scene clips named by string: %d (%d load for this client map)" % (len(scene_refs), here))
        if unknown_scene:
            print("\nSCENE CLIPS IN NO SHIPPED ANIMLIST (codol_getAnim can never find them):")
            for t, n in unknown_scene:
                print("  %s/%-40s %s" % (t, n, " ".join(scene_refs[(t, n)][:3])))
    if ckept != skept:
        first = next((i for i, (x, y) in enumerate(zip(ckept, skept)) if x != y),
                     min(len(ckept), len(skept)))
        print("\nTREES DIFFER: server %d entries, client %d; first divergence at entry %d"
              % (len(skept), len(ckept), first))
        return 1
    print("\nserver and client %s trees are identical (%d entries)" % (tree, len(skept)))
    return 1 if undefined or unknown_scene else 0


if __name__ == "__main__":
    sys.exit(main())
