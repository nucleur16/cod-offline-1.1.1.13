r"""Map-select pictures for the maps ported from CODOL 3.19.26.22 (Create Room carousel, comaplist column 4).

python tools/map_menu_art/build_previews.py --src <folder of source PNGs> [--check]

Writes zones/usermaps_ui_mp/image/sfproto_<map>_usermap.png (200x120, the size of the stock sfproto_* pictures) for
every map in PREVIEWS. The pixels go into the zone itself (usermaps_ui_mp is part of common_ui_mp, a UI zone),
so the pictures need no IWD.

Sources (decoded to PNG with tools/map_menu_art/iwi.py; see docs/MAPS_31926_PORTS.md "Menu art"):
  mw3/preview_mp_<iw5 name>.png     Modern Warfare 3 (Steam) main/iw_04.iwd images/preview_mp_<name>.iwi (324x184)
  orig/preview_mp_brecourt.png      Modern Warfare 2 (Steam) main/iw_02.iwd images/preview_mp_brecourt.iwi (324x184)
  orig/menu_mp_map_select_radiation_big.png   Black Ops (Steam) main/iw_05.iwd (512x512, faded left edge)
  ls/loadscreen_<map>.png           the map's own CODOL 3.19.26.22 loadscreen (1280x720), from its port IWD; used for
                                    the CODOL-only maps, which no other game has a picture of
"""
import argparse
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(REPO, "zones", "usermaps_ui_mp", "image")
W, H = 200, 120

# map -> (source PNG relative to --src, crop box in source pixels or None for a centred 5:3 crop)
PREVIEWS = {
    "mp_arkaden_sh":     ("mw3/preview_mp_plaza2.png", None),
    "mp_bootleg_sh":     ("mw3/preview_mp_bootleg.png", None),
    "mp_carbon_sh":      ("mw3/preview_mp_carbon.png", None),
    "mp_fallen_sh":      ("mw3/preview_mp_lambeth.png", None),
    "mp_foundation_sh":  ("mw3/preview_mp_cement.png", None),
    "mp_interchange_sh": ("mw3/preview_mp_interchange.png", None),
    "mp_italy_sh":       ("mw3/preview_mp_italy.png", None),
    "mp_liberation_sh":  ("mw3/preview_mp_park.png", None),
    "mp_lockdown_sh":    ("mw3/preview_mp_alpha.png", None),
    "mp_mission_sh":     ("mw3/preview_mp_bravo.png", None),
    "mp_mogadishu_sh":   ("mw3/preview_mp_mogadishu.png", None),
    "mp_offshore":       ("mw3/preview_mp_roughneck.png", None),
    "mp_overwatch_sh":   ("mw3/preview_mp_overwatch.png", None),
    "mp_resistance_sh":  ("mw3/preview_mp_paris.png", None),
    "mp_village":        ("mw3/preview_mp_village.png", None),
    "mp_wasteland_sh":   ("orig/preview_mp_brecourt.png", None),
    "mp_radiation_sh":   ("orig/menu_mp_map_select_radiation_big.png", (80, 110, 512, 369)),
    # CODOL-only maps: the right part of their loadscreen (its left third fades to black)
    "mp_awp_arena":      ("ls/loadscreen_mp_awp_arena.png", (380, 120, 1280, 660)),
    "mp_boomtown":       ("ls/loadscreen_mp_boomtown.png", (380, 90, 1280, 630)),
    "mp_tunisia":        ("ls/loadscreen_mp_tunisia.png", (380, 120, 1280, 660)),
}


def centred(im):
    w, h = im.size
    if w * H > h * W:      # too wide
        cw = h * W // H
        return ((w - cw) // 2, 0, (w - cw) // 2 + cw, h)
    ch = w * H // W
    return (0, (h - ch) // 2, w, (h - ch) // 2 + ch)


def build(src, mapname):
    path, box = PREVIEWS[mapname]
    im = Image.open(os.path.join(src, path)).convert("RGB")
    im = im.crop(box or centred(im)).resize((W, H), Image.LANCZOS)
    return im.convert("RGBA")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--check", action="store_true", help="only report which outputs differ")
    a = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    for m in PREVIEWS:
        im = build(a.src, m)
        dst = os.path.join(OUT, "sfproto_%s_usermap.png" % m)
        if a.check:
            same = os.path.isfile(dst) and Image.open(dst).convert("RGBA").tobytes() == im.tobytes()
            print("%-20s %s" % (m, "same" if same else "DIFFERS"))
            continue
        im.save(dst, optimize=True)
        print("wrote", os.path.relpath(dst, REPO))


if __name__ == "__main__":
    sys.exit(main())
