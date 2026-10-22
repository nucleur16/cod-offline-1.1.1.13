# -*- coding: utf-8 -*-
r"""build_ingame_ui.py -- client_shipRetail\patch\controller_ingame.ff: C1's in-match pause menu with gamepad
focus navigation.

C1 draws the in-match pause menu (Start / Esc during a match: loadouts, options, leave) with
scaleform/default/codoingame.gfx, which ships in common_mp / patch_common_mp, not in the UI zones, so it
cannot go into voice_ui_mp.ff (a *_ui_mp patch zone). It goes into its own patch zone, controller_ingame.ff:
Kernel33 loads non-*_ui_mp patch zones ahead of code_pre_gfx, so this codoingame.gfx is linked before
patch_common_mp's and wins (rawfiles are fine there; only images must not load that early).

voice_ui/MainPause.as is the movie's document class (decompiled, 79 lines) with the focus navigator for the
pad's key codes 240..244 that controller.c hands to the movie; FFDec compiles it and only its DoABC tag is
spliced into patch_common_mp's codoingame.gfx, every other tag checked unchanged (build_voice_ui.splice_view).
(Recompiling the 1168-line PauseMenuView instead crashed C1 when the menu opened.)

    python build_ingame_ui.py              build, verify, install into client_shipRetail\patch
    python build_ingame_ui.py --dry-run    build and verify only
    python build_ingame_ui.py --out X.ff   build into X.ff
"""
import argparse
import os
import sys

import build_voice_ui as v

GFX = "scaleform/default/codoingame.gfx"
ZONE_NAME = "controller_ingame"
CLASSES = [("MainPause", os.path.join(v.HERE, "voice_ui", "MainPause.as"))]


def patched_gfx():
    for zone in ("patch_common_mp", "common_mp"):
        data = v.za.open_zone(zone).rawfile(GFX)
        if data:
            break
    else:
        raise SystemExit("no %s in patch_common_mp or common_mp" % GFX)
    print("%s from %s: %d bytes" % (GFX, zone, len(data)))
    for cls, src_as in CLASSES:
        data = v.splice_view(data, cls, src_as)
    return data


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--out", help="write the .ff here instead of client_shipRetail/patch")
    args = ap.parse_args(argv)
    gfx = patched_gfx()
    zone = v.b.build_zone(ZONE_NAME, [(v.T_RAWFILE, v.b.pack_rawfile_z(GFX, gfx))])
    ff = v.b.wrap(zone, v.C1)
    if v.b.unwrap(ff, v.C1) != zone or v.za.Zone(zone, ZONE_NAME).rawfile(GFX) != gfx:
        raise SystemExit("zone does not read back")
    print("zone %s: %d bytes, file %d bytes" % (ZONE_NAME, len(zone), len(ff)))
    if args.dry_run:
        return 0
    out = args.out or os.path.join(v.PATCH_DIR, ZONE_NAME + ".ff")
    with open(out + ".tmp", "wb") as f:
        f.write(ff)
    os.replace(out + ".tmp", out)
    if v.za.Zone(v.b.unwrap(open(out, "rb").read(), v.C1), ZONE_NAME).rawfile(GFX) != gfx:
        raise SystemExit("written file does not read back")
    print("wrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
