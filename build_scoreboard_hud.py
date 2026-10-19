# -*- coding: utf-8 -*-
r"""build_scoreboard_hud.py -- client_shipRetail\patch\codol_english_hud_emblems.ff: team emblems on TAB.

    python build_scoreboard_hud.py                      build from <game>\client_shipRetail\patch, install
    python build_scoreboard_hud.py --game <dir>         another install (e.g. codol-classic-zz)
    python build_scoreboard_hud.py --dry-run            build and verify only
    python build_scoreboard_hud.py --out X.ff           build into X.ff

The TAB scoreboard is codo.views.HudScoreboardView in scaleform/default/codohud.gfx. Each team panel
(components.scoreboard_part.ScoreboardTeamPanel) holds a team sprite (myTeamBlue / myTeamRed: header
bar art plus myImg, the 156x154 TF141 / Shadow Company emblem, an external image hud_team_tf141.tga /
hud_team_scar.tga). Stock C1 never hides it in a team mode.

The English UI zone codol_english_ui_extra.ff (launcher release 1.1.1.13-r24 on) ships a codohud.gfx
whose ScoreboardTeamPanel.SetScoreboardData sizes each panel to its player count (rows - 9 rows of
height) and then hides the whole team sprite unless the emblem, moved by half that delta, still fits
between the team name and the panel bottom:

    this.myTeamBlue.visible = style == BLUE && blueImg.y >= name bottom && blueImg.y + blueImg.height <= bkg bottom

With the stock geometry (name bottom 37, emblem y 81 and 154 high, panel 2..316) that fails as soon as
a team has five players or fewer, so in every small match the emblem -- and the header bar art with
it -- is gone. scoreboard_hud/ScoreboardTeamPanel.as is that class with the last step replaced:

  - a team panel (score shown) is at least MIN_TEAM_ROWS (6) rows tall;
  - FitEmblem keeps the team sprite visible for its own style and scales the emblem to the band
    between the team name and the score (or the panel bottom when there is no score), centred on the
    stock position; at nine rows that is exactly the stock 81, and the emblem only hides when it
    would be under 30 % size.

FFDec (ffdec-cli, $CODOL_FFDEC) compiles the class into a copy of that codohud.gfx; only its DoABC
tag is taken from FFDec's output and spliced into the original file, and every other tag is checked
to be byte-identical. The result ships as codol_english_hud_emblems.ff: Kernel33's patch folder loads
zones in name order and C1 keeps the first asset linked under a name, so it is linked ahead of
codol_english_ui_extra's codohud.gfx (the other two movies in that zone are untouched). A client
without codol_english_ui_extra.ff has the stock HUD, which draws the emblems, and needs nothing.

Rebuild this zone whenever codol_english_ui_extra.ff changes: it carries a whole codohud.gfx and
would otherwise shadow the newer one.

Needs: alpha0022's zones/zone_assets.py and D:\CoD\base-wt\tools\codol_english, like build_voice_ui.py.
"""
import argparse
import hashlib
import os
import struct
import subprocess
import sys
import tempfile

import build_voice_ui as v

b, za = v.b, v.za
HERE = os.path.dirname(os.path.abspath(__file__))
ZONE_NAME = "codol_english_hud_emblems"
SOURCE_ZONE = "codol_english_ui_extra"
GFX = "scaleform/default/codohud.gfx"
PANEL_CLASS = "components.scoreboard_part.ScoreboardTeamPanel"
PANEL_SRC = os.path.join(HERE, "scoreboard_hud", "ScoreboardTeamPanel.as")
# what identifies the class this replaces: the r24 panel layout and its emblem test
SOURCE_MARKERS = (b"codolPartyPanelLayout", b"blueY", b"redY")


def source_gfx(patch_dir):
    path = os.path.join(patch_dir, SOURCE_ZONE + ".ff")
    if not os.path.isfile(path):
        raise SystemExit("%s not found: a client without it has the stock HUD and needs no fix" % path)
    raw = open(path, "rb").read()
    data = za.Zone(b.unwrap(raw, v.C1), SOURCE_ZONE).rawfile(GFX)
    if not data:
        raise SystemExit("%s has no %s" % (path, GFX))
    tags = [t for t in v.abc_tags(data) if t[2] == PANEL_CLASS.replace(".", "/")]
    if len(tags) != 1:
        raise SystemExit("%s: expected one DoABC tag %s, found %d" % (GFX, PANEL_CLASS, len(tags)))
    body = data[tags[0][0]:tags[0][1]]
    missing = [m.decode() for m in SOURCE_MARKERS if m not in body]
    if missing:
        raise SystemExit("%s in %s is not the panel-layout class this fixes (no %s); re-check it"
                         % (PANEL_CLASS, path, ", ".join(missing)))
    print("%s from %s (sha256 %s): %d bytes" % (GFX, path, hashlib.sha256(raw).hexdigest()[:16], len(data)))
    return data


def splice_panel(data):
    """codohud.gfx with ScoreboardTeamPanel's DoABC tag compiled from scoreboard_hud/ScoreboardTeamPanel.as."""
    want = PANEL_CLASS.replace(".", "/")
    if not os.path.isfile(v.FFDEC):
        raise SystemExit("FFDec not found at %s (set CODOL_FFDEC)" % v.FFDEC)
    with tempfile.TemporaryDirectory() as tmp:
        src, dst = os.path.join(tmp, "in.gfx"), os.path.join(tmp, "out.gfx")
        with open(src, "wb") as f:
            f.write(data)
        r = subprocess.run([v.FFDEC, "-replace", src, dst, PANEL_CLASS, PANEL_SRC], capture_output=True, text=True)
        if r.returncode or not os.path.isfile(dst):
            raise SystemExit("FFDec could not compile %s:\n%s%s" % (PANEL_SRC, r.stdout, r.stderr))
        with open(dst, "rb") as f:
            compiled = f.read()
    old, new = v.all_tags(data), v.all_tags(compiled)
    if len(old) != len(new):
        raise SystemExit("FFDec changed the tag count (%d -> %d)" % (len(old), len(new)))
    hit = None
    for i, (a, c) in enumerate(zip(old, new)):
        if a[0] == 82 and a[4] == want:
            hit = i
        elif a[0] != c[0] or data[a[2]:a[3]] != compiled[c[2]:c[3]]:
            raise SystemExit("FFDec changed tag %d (code %d %s), not only %s" % (i, a[0], a[4], want))
    if hit is None:
        raise SystemExit("no DoABC tag %s" % want)
    _, hstart, _, hend, _ = old[hit]
    _, nstart, _, nend, _ = new[hit]
    out = bytearray(data[:hstart] + compiled[nstart:nend] + data[hend:])
    struct.pack_into("<I", out, 4, len(out))
    if b"FitEmblem" not in out[hstart:hstart + (nend - nstart)]:
        raise SystemExit("compiled %s has no FitEmblem" % want)
    print("%s: DoABC tag %d -> %d bytes (FFDec), file %d bytes" % (want, hend - hstart, nend - nstart, len(out)))
    return bytes(out)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--game", default=v.GAME, help="install whose client_shipRetail\\patch holds %s.ff" % SOURCE_ZONE)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--out", help="write the .ff here instead of <game>\\client_shipRetail\\patch")
    args = ap.parse_args(argv)
    patch_dir = os.path.join(args.game, "client_shipRetail", "patch")
    gfx = splice_panel(source_gfx(patch_dir))
    zone = b.build_zone(ZONE_NAME, [(v.T_RAWFILE, b.pack_rawfile_z(GFX, gfx))])
    ff = b.wrap(zone, v.C1)
    if b.unwrap(ff, v.C1) != zone or za.Zone(zone, ZONE_NAME).rawfile(GFX) != gfx:
        raise SystemExit("zone does not read back")
    print("zone %s: %d bytes, file %d bytes" % (ZONE_NAME, len(zone), len(ff)))
    if args.dry_run:
        return 0
    out = args.out or os.path.join(patch_dir, ZONE_NAME + ".ff")
    with open(out + ".tmp", "wb") as f:
        f.write(ff)
    os.replace(out + ".tmp", out)
    if za.Zone(b.unwrap(open(out, "rb").read(), v.C1), ZONE_NAME).rawfile(GFX) != gfx:
        raise SystemExit("written file does not read back")
    print("wrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
