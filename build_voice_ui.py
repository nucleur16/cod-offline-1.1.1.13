# -*- coding: utf-8 -*-
r"""build_voice_ui.py -- client_shipRetail\patch\voice_ui_mp.ff: C1's own voice-chat key row, shown.

    python build_voice_ui.py              build, verify, install
    python build_voice_ui.py --dry-run    build and verify only

C1's options screen (scaleform/default/codo.gfx, codo.views.option_sub.OptionView_control_chat, the
Controls > Chat tab) already has the push-to-talk row: a "Talk" label (myTFTalk, SF_UI_CONTROL_TALK),
a key button (myBtn_1, bound to "toggletalk", or to "+talk" when the "hold to talk" checkbox is on) and
that checkbox (myCBTalk, CONTROL_NEED_LONG_PRESS). Its onInit hides all three:

    debugline 56  getlocal0 getproperty myTFTalk  pushfalse setproperty visible
    debugline 57  getlocal0 getproperty myBtn_1   pushfalse setproperty visible
    debugline 58  getlocal0 getproperty myCBTalk  pushfalse setproperty visible

because C1 had no voice. This flips those three pushfalse (0x27) to pushtrue (0x26) -- same length, so
no ABC or SWF size changes -- in the codo.gfx C1 actually uses (patch_ui_mp's, which loads ahead of
ui_mp's) and ships it as the rawfile of a *_ui_mp patch zone, which Kernel33 loads ahead of the UI
zones; C1 keeps the first asset linked under a name. Binding and reading the row go through C1's own
AS_SetKeyBindingForCommand / AS_GetKeyBindingNamesFromCommand, which work for +talk because voice.c adds
+talk/-talk to C1's bind table.

Needs: alpha0022's zones/zone_assets.py (stock zone reader) and D:\CoD\base-wt\tools\codol_english
(build_zone, wrap, unwrap, pack_rawfile_z), as usermaps\build_usermaps_patch.py does.
"""
import argparse
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
C1 = os.path.join(GAME, "C1.exe")
ALPHA = os.environ.get("CODOL_ALPHA0022", os.path.join(GAME, "CODOL", "1.1.1.13_server", "alpha0022"))
TOOLS = os.environ.get("CODOL_BASE_TOOLS", r"D:\CoD\base-wt\tools")
PATCH_DIR = os.path.join(GAME, "client_shipRetail", "patch")
ZONE_NAME = "voice_ui_mp"
GFX = "scaleform/default/codo.gfx"
T_RAWFILE = 36

for _p in (ALPHA, os.path.join(ALPHA, "zones"), TOOLS, os.path.join(TOOLS, "codol_english")):
    if _p not in sys.path:
        sys.path.insert(0, _p)
import build_font_ff as b              # noqa: E402
import zone_assets as za               # noqa: E402

U30 = rb"(?:[\x80-\xff]{0,4}[\x00-\x7f])"
# three "getlocal0; getproperty X; pushfalse; setproperty visible" on consecutive debuglines, then
# debugline; returnvoid: the end of OptionView_control_chat.onInit (debuglines 56..58 in ui_mp's copy,
# 57..59 in patch_ui_mp's)
ONINIT = re.compile(rb"\xf0([\x00-\x7f])\xd0\x66(" + U30 + rb")(\x27)\x61(" + U30 + rb")"
                    rb"\xf0([\x00-\x7f])\xd0\x66(" + U30 + rb")(\x27)\x61\4"
                    rb"\xf0([\x00-\x7f])\xd0\x66(" + U30 + rb")(\x27)\x61\4\xf0[\x00-\x7f]\x47", re.S)


def abc_tags(data):
    """(start, end, name) of every DoABC (82) tag; the GFX header is a SWF header with another magic."""
    nbits = data[8] >> 3
    pos = 8 + (5 + 4 * nbits + 7) // 8 + 4
    out = []
    while pos + 2 <= len(data):
        code_len = struct.unpack_from("<H", data, pos)[0]
        code, ln = code_len >> 6, code_len & 0x3F
        pos += 2
        if ln == 0x3F:
            ln = struct.unpack_from("<I", data, pos)[0]
            pos += 4
        if code == 82:
            name_end = data.index(b"\0", pos + 4)
            out.append((pos, pos + ln, data[pos + 4:name_end].decode("latin1")))
        if code == 0:
            break
        pos += ln
    return out


def onInit_hits(data):
    """The match inside the DoABC tag of codo.views.option_sub.OptionView_control_chat."""
    tags = [t for t in abc_tags(data) if t[2].endswith("OptionView_control_chat")]
    return [m for m in ONINIT.finditer(data)
            if m.group(5)[0] == m.group(1)[0] + 1 and m.group(8)[0] == m.group(1)[0] + 2 and
            any(a <= m.start() < b for a, b, _ in tags)]


def patched_gfx():
    for zone in ("patch_ui_mp", "ui_mp"):
        data = za.open_zone(zone).rawfile(GFX)
        if data:
            break
    else:
        raise SystemExit("no %s in patch_ui_mp or ui_mp" % GFX)
    hits = onInit_hits(data)
    if len(hits) != 1:
        raise SystemExit("%s (%s): expected one OptionView_control_chat.onInit tail, found %d" % (GFX, zone, len(hits)))
    m = hits[0]
    out = bytearray(data)
    for g in (3, 7, 10):
        out[m.start(g)] = 0x26                                      # pushfalse -> pushtrue
    print("%s from %s: %d bytes; onInit visibility at 0x%X: pushfalse -> pushtrue x3" % (GFX, zone, len(data), m.start()))
    return bytes(out)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args(argv)
    gfx = patched_gfx()
    zone = b.build_zone(ZONE_NAME, [(T_RAWFILE, b.pack_rawfile_z(GFX, gfx))])
    ff = b.wrap(zone, C1)
    if b.unwrap(ff, C1) != zone or za.Zone(zone, ZONE_NAME).rawfile(GFX) != gfx:
        raise SystemExit("zone does not read back")
    print("zone %s: %d bytes, file %d bytes" % (ZONE_NAME, len(zone), len(ff)))
    if args.dry_run:
        return 0
    out = os.path.join(PATCH_DIR, ZONE_NAME + ".ff")
    with open(out + ".tmp", "wb") as f:
        f.write(ff)
    os.replace(out + ".tmp", out)
    if za.Zone(b.unwrap(open(out, "rb").read(), C1), ZONE_NAME).rawfile(GFX) != gfx:
        raise SystemExit("written file does not read back")
    print("wrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
