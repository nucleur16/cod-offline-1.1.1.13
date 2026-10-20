# -*- coding: utf-8 -*-
r"""build_voice_ui.py -- client_shipRetail\patch\voice_ui_mp.ff: C1's options screens with Kernel33's settings.

Every *_ui_mp patch zone that carries scaleform/default/codo.gfx carries the whole movie, and the first
one loaded wins, so all of Kernel33's options-screen changes go into this one codo.gfx: the voice-chat
rows (Controls > Chat, Audio > Voice) and the frame-rate cap (Graphics > Max FPS).

    python build_voice_ui.py              build, verify, install
    python build_voice_ui.py --dry-run    build and verify only
    python build_voice_ui.py --out X.ff   build into X.ff (a test client's own patch folder)

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
(build_zone, wrap, unwrap, pack_rawfile_z), as usermaps\build_usermaps_patch.py does, and FFDec.

Options > Audio: C1's codo.views.option_sub.OptionView_audio has a whole Voice section (voice chat
volume, a microphone switch and level slider, an input-level indicator) that its onInit hides, and an
unused "Sound Presets" combo box. voice_ui/OptionView_audio.as is that class (decompiled) with the
section shown and wired to Kernel33's dvars: Voice Chat -> k33_voiceVolume, Microphone checkbox ->
k33_voiceMicMute, Microphone slider -> k33_voiceThreshold, the presets box (moved into the section as
"Voice Mode") -> k33_voiceMode (push to talk / voice activation), Input Level -> a live meter of
k33_voiceLevel. That is not a same-length edit, so JPEXS FFDec (ffdec-cli, $CODOL_FFDEC) compiles the
class into a copy of codo.gfx, and only that class's DoABC tag is taken from FFDec's output and spliced
into the original file; every other tag is checked to be byte-identical.

Options > Graphics: voice_ui/OptionView_video.as is codo.views.option_sub.OptionView_video with C1's
hidden Aspect Ratio row turned into "Max FPS" (Unlimited / 30 ... 500 -> "1848", com_maxfps, whose
domain Kernel33 raises to 0..1000), set when Apply is pressed. It is compiled and spliced the same way.
"""
import argparse
import os
import re
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
C1 = os.path.join(GAME, "C1.exe")
ALPHA = os.environ.get("CODOL_ALPHA0022", os.path.join(GAME, "CODOL", "1.1.1.13_server", "alpha0022"))
TOOLS = os.environ.get("CODOL_BASE_TOOLS", r"D:\CoD\base-wt\tools")
PATCH_DIR = os.path.join(GAME, "client_shipRetail", "patch")
ZONE_NAME = "voice_ui_mp"
GFX = "scaleform/default/codo.gfx"
T_RAWFILE = 36
FFDEC = os.environ.get("CODOL_FFDEC", r"C:\Program Files (x86)\FFDec\ffdec-cli.exe")
# classes compiled from voice_ui/*.as by FFDec, in order
VIEW_CLASSES = [
    ("codo.views.option_sub.OptionView_audio", os.path.join(HERE, "voice_ui", "OptionView_audio.as")),
    ("codo.views.option_sub.OptionView_video", os.path.join(HERE, "voice_ui", "OptionView_video.as")),
    ("codo.controllers.StageKeyController", os.path.join(HERE, "voice_ui", "StageKeyController.as")),
]

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


def all_tags(data):
    """(code, header start, body start, end, DoABC name or "") of every tag; the GFX header is a SWF
    header with another magic."""
    nbits = data[8] >> 3
    pos = 8 + (5 + 4 * nbits + 7) // 8 + 4
    out = []
    while pos + 2 <= len(data):
        start = pos
        code_len = struct.unpack_from("<H", data, pos)[0]
        code, ln = code_len >> 6, code_len & 0x3F
        pos += 2
        if ln == 0x3F:
            ln = struct.unpack_from("<I", data, pos)[0]
            pos += 4
        name = ""
        if code == 82:
            name_end = data.index(b"\0", pos + 4)
            name = data[pos + 4:name_end].decode("latin1")
        out.append((code, start, pos, pos + ln, name))
        if code == 0:
            break
        pos += ln
    return out


def abc_tags(data):
    """(start, end, name) of every DoABC (82) tag."""
    return [(b, e, n) for code, _, b, e, n in all_tags(data) if code == 82]


def splice_view(data, cls, src_as):
    """codo.gfx with the DoABC tag of class cls compiled from src_as."""
    want = cls.replace(".", "/")
    if not os.path.isfile(FFDEC):
        raise SystemExit("FFDec not found at %s (set CODOL_FFDEC)" % FFDEC)
    with tempfile.TemporaryDirectory() as tmp:
        src, dst = os.path.join(tmp, "in.gfx"), os.path.join(tmp, "out.gfx")
        with open(src, "wb") as f:
            f.write(data)
        r = subprocess.run([FFDEC, "-replace", src, dst, cls, src_as], capture_output=True, text=True)
        if r.returncode or not os.path.isfile(dst):
            raise SystemExit("FFDec could not compile %s:\n%s%s" % (src_as, r.stdout, r.stderr))
        with open(dst, "rb") as f:
            compiled = f.read()
    old, new = all_tags(data), all_tags(compiled)
    if len(old) != len(new):
        raise SystemExit("FFDec changed the tag count (%d -> %d)" % (len(old), len(new)))
    hit = None
    for i, (a, b) in enumerate(zip(old, new)):
        if a[0] == 82 and a[4] == want:
            hit = i
        elif a[0] != b[0] or data[a[2]:a[3]] != compiled[b[2]:b[3]]:
            raise SystemExit("FFDec changed tag %d (code %d %s), not only %s" % (i, a[0], a[4], want))
    if hit is None:
        raise SystemExit("no DoABC tag %s" % want)
    _, hstart, _, hend, _ = old[hit]
    _, nstart, _, nend, _ = new[hit]
    out = bytearray(data[:hstart] + compiled[nstart:nend] + data[hend:])
    struct.pack_into("<I", out, 4, len(out))
    print("%s: DoABC tag %d -> %d bytes (FFDec), file %d bytes" % (want, hend - hstart, nend - nstart, len(out)))
    return bytes(out)


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
    out = bytes(out)
    for cls, src_as in VIEW_CLASSES:
        out = splice_view(out, cls, src_as)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--out", help="write the .ff here instead of client_shipRetail/patch (for a test copy)")
    args = ap.parse_args(argv)
    gfx = patched_gfx()
    zone = b.build_zone(ZONE_NAME, [(T_RAWFILE, b.pack_rawfile_z(GFX, gfx))])
    ff = b.wrap(zone, C1)
    if b.unwrap(ff, C1) != zone or za.Zone(zone, ZONE_NAME).rawfile(GFX) != gfx:
        raise SystemExit("zone does not read back")
    print("zone %s: %d bytes, file %d bytes" % (ZONE_NAME, len(zone), len(ff)))
    if args.dry_run:
        return 0
    out = args.out or os.path.join(PATCH_DIR, ZONE_NAME + ".ff")
    with open(out + ".tmp", "wb") as f:
        f.write(ff)
    os.replace(out + ".tmp", out)
    if za.Zone(b.unwrap(open(out, "rb").read(), C1), ZONE_NAME).rawfile(GFX) != gfx:
        raise SystemExit("written file does not read back")
    print("wrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
