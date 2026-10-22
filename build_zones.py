# -*- coding: utf-8 -*-
r"""build_zones.py -- the one build entry point for every zone of this repository.

    python build_zones.py                              build every zone in zone_source/ into output\
    python build_zones.py voice_ui_mp controller_ingame
                                                       build only these zones
    python build_zones.py --list                       list the zones, where each installs and where it came from
    python build_zones.py --verify                     build, then compare each output with the recorded original
    python build_zones.py --decompile [zone ...]       refresh zones\ + zone_source\ from the shipped files

Options:
  --game DIR       the 1.1.1.13 install (default D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13): C1.exe holds the
                   zone key and client_shipRetail\chinese the stock zones (the movies the .gfx patches apply to, the
                   PaP zone's unchanged blobs). Read only.
  --zz DIR         codol-classic-zz, for --verify / --decompile of the zones that came from it. Read only.
  --out DIR        output root (default output\); zones land in <out>\client_shipRetail\{patch,chinese}, files\ is
                   copied as it is (sound_channels.txt, main\iw_image_16.iwd).
  --ffdec PATH     JPEXS ffdec-cli.exe. Needed only when an actionscript\*.as file changed (its class is compiled into
                   the movie again) or with --compile-as.
  --compile-as     compile every actionscript\ class and every edited .gfxpatch\as\ file, changed or not.
  --fresh-key      a new AES key and timestamp instead of the recorded ones (same zone; different file bytes).
  --assetexplorer  CodolSoundDumper.exe: --verify also loads outputs that are not md5-identical and compares asset tables.

Every zone is described in zone_source\<zone>.csv (zonekit/manifest.py) and its contents are under zones\<zone>\;
zonekit/ does the work (container, zone image, codecs, gfx patches, the zone list in zonekit/zonelist.py). An untouched
tree rebuilds every zone byte for byte (the C#-written PaP zone: the same zone image, Python's zlib stream).
"""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from zonekit import build, container, decompile, stock, zoneimage, zonelist  # noqa: E402

DEFAULT_GAME = r"D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13"
DEFAULT_ZZ = r"D:\CoD\CODOL Builds\codol-classic-zz"
DEFAULT_FFDEC = r"C:\Program Files (x86)\FFDec\ffdec-cli.exe"


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def origin_path(origin, name, roots, tmp):
    """The shipped file a zone came from: G:/Z: are install folders, R:<commit>:<dir> a file this repo committed."""
    root, _, rel = origin.partition(":")
    if root == "R":
        rev, _, d = rel.partition(":")
        out = os.path.join(tmp, name + ".ff")
        with open(out, "wb") as f:
            r = subprocess.run(["git", "-C", HERE, "show", "%s:%s/%s.ff" % (rev, d, name)], stdout=f)
        return out if r.returncode == 0 else None
    if not roots.get(root):
        return None
    p = os.path.join(roots[root], rel.replace("/", os.sep), name + ".ff")
    return p if os.path.isfile(p) else None


def loader_summary(cli, path, exe):
    r = subprocess.run([cli, "info", "--input", path, "--exe", exe], capture_output=True, text=True)
    return [l for l in r.stdout.splitlines() if l.startswith("  ") or l.startswith(("Sound", "Loaded"))], r.returncode


def verify(out, zone, m, roots, exe, tmp, cli=None):
    if md5(out) == m.get("origin-md5"):
        return "IDENTICAL (md5 %s, as recorded from %s)" % (md5(out), m.get("origin"))
    ship = origin_path(m.get("origin"), m.get("zone"), roots, tmp)
    if ship is None:
        return "DIFFERS from the recorded md5 %s (original not available to compare zone images)" % m.get("origin-md5")
    shipped = container.read_any(ship, exe)
    if shipped == zone:
        note = "zone image identical (%d bytes); file differs only in the zlib stream" % len(zone)
        if cli:
            a, ra = loader_summary(cli, ship, exe)
            b, rb = loader_summary(cli, out, exe)
            note += "; AssetExplorer loader: %s" % ("same asset table (%d lines)" % len(a) if a == b and not ra and not rb
                                                   else "DIFFERENT")
        return note
    a, b = zoneimage.parse(shipped), zoneimage.parse(zone)
    diffs = [i for i, (x, y) in enumerate(zip(a.assets, b.assets)) if x != y]
    return "DIFFERS: %d vs %d assets, %d differing (first %s)" % (len(a.assets), len(b.assets), len(diffs), diffs[:5])


def run_decompile(names, exe, roots, chinese, ffdec, tmp):
    bad = 0
    for name in names:
        cfg = zonelist.ZONES[name]
        path = origin_path(cfg["origin"], name, roots, tmp)
        if path is None:
            print("%s: origin %s not available" % (name, cfg["origin"]))
            bad += 1
            continue
        src = stock.ZoneSource(exe, [chinese, os.path.dirname(path)])
        print("%s <- %s" % (name, path), flush=True)
        stats, img = decompile.decompile(name, cfg, path, exe, HERE, src, ffdec=ffdec)
        print("  %d assets: %s" % (len(img.assets), ", ".join("%s %d" % kv for kv in stats.items() if kv[1])))
        zone = build.assemble(HERE, name, build.load(HERE, name), src, ffdec=ffdec, log=print)
        same = zone == zoneimage.assemble(img)
        print("  rebuild of the zone image: %s" % ("identical" if same else "DIFFERS"), flush=True)
        bad += not same
    return bad


def copy_files(out_root, game, check):
    fdir = os.path.join(HERE, "files")
    for root, _d, files in os.walk(fdir):
        for fn in files:
            rel = os.path.relpath(os.path.join(root, fn), fdir)
            dst = os.path.join(out_root, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(os.path.join(root, fn), dst)
            line = "%s (copied)" % rel
            if check:
                ship = os.path.join(game, rel)
                line += ": " + ("IDENTICAL (md5 %s)" % md5(dst) if os.path.isfile(ship) and md5(ship) == md5(dst)
                                else "differs from / missing in the install")
            print(line)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("zones", nargs="*", help="zone names (default: every zone_source/*.csv)")
    ap.add_argument("--game", default=os.environ.get("CODOL_GAME", DEFAULT_GAME))
    ap.add_argument("--zz", default=os.environ.get("CODOL_ZZ", DEFAULT_ZZ))
    ap.add_argument("--exe", help="C1.exe of 1.1.1.13 (default: <game>\\C1.exe)")
    ap.add_argument("--out", default=os.path.join(HERE, "output"))
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("--decompile", action="store_true")
    ap.add_argument("--assetexplorer")
    ap.add_argument("--fresh-key", action="store_true")
    ap.add_argument("--compile-as", action="store_true")
    ap.add_argument("--ffdec", default=os.environ.get("CODOL_FFDEC", DEFAULT_FFDEC))
    a = ap.parse_args(argv)
    exe = a.exe or os.path.join(a.game, "C1.exe")
    chinese = os.path.join(a.game, "client_shipRetail", "chinese")
    ffdec = a.ffdec if a.ffdec and os.path.isfile(a.ffdec) else None
    roots = {"G": a.game, "Z": a.zz}
    known = sorted(f[:-4] for f in os.listdir(os.path.join(HERE, "zone_source")) if f.endswith(".csv"))
    if a.list:
        for name in known:
            m = build.load(HERE, name)
            print("%-36s %-26s from %s" % (name, m.get("install"), m.get("origin")))
        return 0
    with tempfile.TemporaryDirectory() as tmp:
        if a.decompile:
            names = a.zones or list(zonelist.ZONES)
            return 1 if run_decompile(names, exe, roots, chinese, ffdec, tmp) else 0
        names = a.zones or known
        unknown = [n for n in names if n not in known]
        if unknown:
            ap.error("no zone_source for %s (python build_zones.py --list)" % ", ".join(unknown))
        failed = 0
        src = stock.ZoneSource(exe, [chinese])
        for name in names:
            t0 = time.time()
            try:
                out, zone, m = build.build(HERE, name, exe, src, a.out, fresh_key=a.fresh_key, ffdec=ffdec,
                                           compile_as=a.compile_as, log=print)
            except build.BuildError as e:
                failed += 1
                print("%s: FAILED: %s" % (name, e), flush=True)
                continue
            line = "%s (%d bytes, %.1fs)" % (os.path.relpath(out, a.out), os.path.getsize(out), time.time() - t0)
            if a.verify:
                v = verify(out, zone, m, roots, exe, tmp, a.assetexplorer)
                failed += v.startswith("DIFFERS")
                line += ": " + v
            print("%s: %s" % (name, line), flush=True)
        if not a.zones:
            copy_files(a.out, a.game, a.verify)
    print("\n%d zones, %d failed" % (len(names), failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
