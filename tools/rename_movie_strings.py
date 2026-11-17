#!/usr/bin/env python3
"""Renames the console names the shipped Scaleform movies spell (codo.gfx, codologin.gfx, codoingame.gfx) to their
IW engine names (tools/drop_codol_prefix.py IW_TABLE), in the movies' stored DoABC tags, without recompiling a class.

A DoABC tag keeps every string constant once, in its constant pool; the code refers to a string by its pool index.
zonekit/abcstr.replace() rewrites a pool string in place, so the class's methods, its traits and every other tag
stay byte for byte what they were: the only change is the text of the dvar names (and of a constant that holds one,
"seta codol_borderless 0"). A pool string that is also a property, class or package name is refused (none is).
This is the method-preserving route the OptionView / MatchController tags need (zonekit/gfx.py forbids recompiling
those two whole classes), and it is used for every tag the same way.

Each name is matched as a whole word inside a pool string (the same rule as drop_codol_prefix.py), so a prefix the
movie completes at run time ("codol_theatre_id_" + i, "k33_snap" + i) is renamed with it (IW_TABLE kind "prefix",
and the PadSnap chunk prefixes below). The .as sources beside the tags are the reading copy: rename them with the
same table (drop_codol_prefix.py does not touch zones/) and run this, which then records each changed .as file's
digest ("as" line) and the movie's new sha256 in patch.txt.

    python tools/rename_movie_strings.py            rename, rewrite the changed tags and patch.txt
    python tools/rename_movie_strings.py --check    change nothing; exit 1 when a tag still holds an old name or a
                                                    patch.txt digest is stale
Needs the stock movies (--game, default as scripts/build_zones.py) only to compute the movie sha256 lines.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools"))

from zonekit import abcstr, build, gfx, stock  # noqa: E402
import drop_codol_prefix  # noqa: E402

DEFAULT_GAME = r"D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13"
MOVIES = [  # (zone, rawfile)
    ("voice_ui_mp", "scaleform/default/codo.gfx"),
    ("voice_ui_mp", "scaleform/default/codologin.gfx"),
    ("controller_ingame", "scaleform/default/codoingame.gfx"),
]
# the names the movies spell: the IW_TABLE rows, plus the prefixes PadSnap publishes its chunks under
NAMES = dict(drop_codol_prefix.IW_NAMES)
NAMES.update({"k33_snap": "gpad_snap", "k33_snapcls": "gpad_snapClass", "k33_snaphid": "gpad_snapHidden"})
PATTERN = re.compile(r'(?<![A-Za-z0-9_])(' + '|'.join(re.escape(k) for k in sorted(NAMES, key=lambda s: (-len(s), s)))
                     + r')(?![A-Za-z0-9_])')


def patch_dir(zone, rawfile):
    return os.path.join(ROOT, "zones", zone, "rawfile", rawfile.replace("/", os.sep) + ".gfxpatch")


def tag_mapping(tag):
    """{pool string: renamed string} for a DoABC tag's strings that spell an old name."""
    out = {}
    for s in abcstr.strings(tag):
        new = PATTERN.sub(lambda m: NAMES[m.group(1)], s)
        if new != s:
            out[s] = new
    return out


def stock_movie(game, zone, rawfile):
    m = build.load(ROOT, zone)
    ref = [q for t, n, q in m.assets if t == "rawfile" and n == rawfile]
    if not ref or "gfx" not in ref[0]:
        raise SystemExit("%s has no gfx rawfile %s" % (zone, rawfile))
    exe = os.path.join(game, "C1.exe")
    return stock.ZoneSource(exe, [os.path.join(game, "client_shipRetail", "chinese")]).rawfile(ref[0]["gfx"])


def run(check, game):
    stale = 0
    for zone, rawfile in MOVIES:
        pdir = patch_dir(zone, rawfile)
        meta, ops = gfx.read_patch(pdir)
        for op in ops:
            if op[0] != "new" or "_82_" not in op[2]:
                continue
            tag, rel = op[1], op[2]
            mapping = tag_mapping(tag)
            if not mapping:
                continue
            stale += 1
            for old, new in sorted(mapping.items()):
                print("%s %s: %r -> %r" % ("stale" if check else "renamed", rel, old, new))
            if not check:
                with open(os.path.join(pdir, rel), "wb") as f:
                    f.write(abcstr.replace(tag, mapping))
        # patch.txt: the "as" digests and the movie's sha256
        path = os.path.join(pdir, "patch.txt")
        with open(path, encoding="utf-8") as f:
            lines = f.read().splitlines()
        out = []
        for line in lines:
            k, _, v = line.partition(" ")
            if k == "as":
                cls, rel, digest = v.split(" ")
                now = gfx.source_sha(os.path.join(pdir, rel))
                if now != digest:
                    stale += 1
                    print("%s %s: as digest %s -> %s" % ("stale" if check else "recorded", rel, digest, now))
                    line = "as %s %s %s" % (cls, rel, now)
            out.append(line)
        if not check:
            with open(path, "w", encoding="utf-8", newline="\n") as f:
                f.write("\n".join(out) + "\n")
        if game:
            movie = gfx.build_patch(pdir, stock_movie(game, zone, rawfile))
            digest = hashlib.sha256(movie).hexdigest()
            want = "sha256 " + digest
            if want not in out:
                stale += 1
                print("%s %s: movie sha256 %s" % ("stale" if check else "recorded", rawfile, digest))
                if not check:
                    out = [want if l.startswith("sha256 ") else l for l in out]
                    with open(path, "w", encoding="utf-8", newline="\n") as f:
                        f.write("\n".join(out) + "\n")
            print("%s: %s" % (rawfile, digest))
    return 1 if check and stale else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--game", default=os.environ.get("K33_GAME", DEFAULT_GAME),
                    help="1.1.1.13 install holding the stock movies (read only); '' skips the sha256 lines")
    a = ap.parse_args(argv)
    game = a.game if a.game and os.path.isfile(os.path.join(a.game, "C1.exe")) else None
    if a.game and not game:
        print("note: no C1.exe under %s; the movie sha256 lines are not checked" % a.game)
    return run(a.check, game)


if __name__ == "__main__":
    sys.exit(main())
