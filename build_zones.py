# -*- coding: utf-8 -*-
r"""build_zones.py -- the one build entry point for every zone of this repository.

    python build_zones.py                              build every zone in zone_source/ into output\ (shipping build)
    python build_zones.py --maps all                   the same with every usermap's UI, into output_dev\
    python build_zones.py voice_ui_mp controller_ingame
                                                       build only these zones
    python build_zones.py --list                       list the zones, where each installs and where it came from
    python build_zones.py --verify                     build, then compare each output with the recorded original
    python build_zones.py --decompile [zone ...]       refresh zones\ + zone_source\ from the shipped files

Usermaps: shipping_maps.txt lists every usermap as "ship" or "dev" (zonekit/shipping.py). The default build is the
shipping build: the usermap zones (usermaps_patch, usermaps_ui_mp) and files\...\sound_channels.txt carry only the
"ship" maps' data (create-room rows, arena entries, names, map-select images, vision, sound channels). --maps all
(or CODOL_MAPS=all) keeps every listed map, for local testing of maps in development, and writes output_dev\ unless
--out is given, so a dev build is never the folder that ships. A usermap that shipping_maps.txt does not list fails
the build either way.

Options:
  --maps ship|all  ship (default): only the maps shipping_maps.txt marks "ship"; all: every listed map.
  --game DIR       the 1.1.1.13 install (default D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13): C1.exe holds the
                   zone key and client_shipRetail\chinese the stock zones (the movies the .gfx patches apply to, the
                   PaP zone's unchanged blobs). Read only.
  --zz DIR         codol-classic-zz, for --verify / --decompile of the zones that came from it. Read only.
  --out DIR        output root (default output\, output_dev\ with --maps all); zones land in
                   <out>\client_shipRetail\{patch,chinese}, files\ is copied (sound_channels.txt filtered like the
                   usermap zones, main\iw_image_16.iwd as it is).
  --ffdec PATH     JPEXS ffdec-cli.exe. Needed only when a .gfxpatch\as\*.as file changed since its tag was stored
                   (that class is compiled into the movie again) or with --compile-as.
  --store-class ZONE:RAWFILE:CLASS
                   compile a class from <rawfile>.gfxpatch\as\<package>\<Class>.as (FFDec) and store its DoABC tag in
                   tags\ and patch.txt, so builds need no FFDec; a class the stock movie does not have is added.
  --record         with --verify: a zone that does not reproduce its origin gets its build's md5 recorded in its
                   zone_source csv (">build-md5"); --verify then reports it IDENTICAL to the recorded build. Use it
                   after reviewing an intentional change, like --candidate's baseline record.
  --compile-as     also compile every .gfxpatch\as\ file whose text is not FFDec's decompile of its stored tag (the
                   hand-written sources of voice_ui_mp and controller_ingame); reproduces the stored tags.
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
from zonekit import build, container, decompile, gfx, packs, shipping, stock, ui_baseline, zoneimage, zonelist  # noqa: E402

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
    if md5(out) == m.get("build-md5"):
        return "IDENTICAL to the recorded build (md5 %s; its sources changed on purpose since %s)" % (
            md5(out), m.get("origin"))
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


def record_build(name, out):
    """zone_source/<name>.csv: ">build-md5" = this build's md5, the reviewed output of the sources as they are."""
    path = os.path.join(HERE, "zone_source", name + ".csv")
    with open(path, "rb") as f:
        data = f.read()
    nl = b"\r\n" if b"\r\n" in data else b"\n"
    line = b">build-md5," + md5(out).encode()
    lines = data.split(nl)
    at = [i for i, l in enumerate(lines) if l.startswith(b">build-md5,")]
    if at:
        lines[at[0]] = line
    else:
        lines.insert([i for i, l in enumerate(lines) if l.startswith(b">origin-md5,")][0] + 1, line)
    with open(path, "wb") as f:
        f.write(nl.join(lines))


def run_decompile(names, exe, roots, chinese, ffdec, tmp):
    bad = 0
    for name in names:
        cfg = zonelist.ZONES[name]
        if cfg["origin"] == "new":
            print("%s: authored here (no shipped original), its sources are the originals" % name)
            continue
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


def copy_files(out_root, game, check, maps):
    """files/ -> the output as they are, except the per-map ones (shipping.FILES), which keep only the maps `maps`
    ships. --verify compares the unfiltered file with the install."""
    fdir = os.path.join(HERE, "files")
    for root, _d, files in os.walk(fdir):
        for fn in files:
            src = os.path.join(root, fn)
            rel = os.path.relpath(src, fdir)
            dst = os.path.join(out_root, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(src, "rb") as f:
                data = f.read()
            out, dropped = shipping.filter_file(rel, data, maps)
            with open(dst, "wb") as f:
                f.write(out)
            line = "%s (%s)" % (rel, "copied" if not dropped else "without " + ", ".join(sorted(dropped)))
            if check:
                ship = os.path.join(game, rel)
                line += ": " + ("IDENTICAL (md5 %s)%s" % (md5(src), " before the filter" if dropped else "")
                                if os.path.isfile(ship) and md5(ship) == md5(src)
                                else "differs from / missing in the install")
            print(line)


def build_zones(names, out_root, a, exe, src, maps, roots, ffdec, tmp):
    """Build source zones as zones of their own (the standalone ones, or every one with --parts) -> failures."""
    failed = 0
    for name in names:
        t0 = time.time()
        try:
            out, zone, m, plan = build.build(HERE, name, exe, src, out_root, fresh_key=a.fresh_key, ffdec=ffdec,
                                             compile_as=a.compile_as, log=print, maps=maps)
            if a.verify and plan:
                # the recorded original carries every map: compare the unfiltered build with it
                vdir = os.path.join(tmp, "unfiltered")
                vout, vzone, _m, _p = build.build(HERE, name, exe, src, vdir, fresh_key=a.fresh_key, ffdec=ffdec,
                                                  compile_as=a.compile_as, log=print)
        except build.BuildError as e:
            failed += 1
            print("%s: FAILED: %s" % (name, e), flush=True)
            continue
        line = "%s (%d bytes, %.1fs)" % (os.path.relpath(out, out_root), os.path.getsize(out), time.time() - t0)
        if plan:
            line += "; without %s (%s)" % (", ".join(sorted(plan.dropped_maps)), "; ".join(plan.notes))
        if a.verify:
            v = verify(vout, vzone, m, roots, exe, tmp, a.assetexplorer) if plan else \
                verify(out, zone, m, roots, exe, tmp, a.assetexplorer)
            if a.record and v.startswith("DIFFERS"):
                record_build(name, vout if plan else out)
                v = "recorded as the reviewed build (was: %s)" % v
            failed += v.startswith("DIFFERS")
            line += ": " + ("unfiltered build " if plan else "") + v
        print("%s: %s" % (name, line), flush=True)
    return failed


def build_one_pack(pack, a, exe, src, maps, ffdec):
    """zone_packs/<pack>.csv -> <out>/<install>/<pack>.ff (zonekit/packs.py) -> 1 on failure."""
    t0 = time.time()
    try:
        out, zone, m, rep = packs.build_pack(HERE, pack, exe, src, a.out, maps, fresh_key=a.fresh_key, ffdec=ffdec,
                                             compile_as=a.compile_as, log=print)
    except (build.BuildError, packs.PackError) as e:
        print("%s: FAILED: %s" % (pack, e), flush=True)
        return 1
    line = "%s (%d bytes, %.1fs; %s list, %s): %s" % (
        os.path.relpath(out, a.out), os.path.getsize(out), time.time() - t0, packs.stage(pack),
        "/".join(sorted(packs.locales(pack))), ", ".join("%s %d%s" % (p, n, "" if n == t else " of %d" % t)
                                                        for p, n, t in rep["parts"]))
    if rep["dropped_maps"]:
        line += "; without %s" % ", ".join(sorted(rep["dropped_maps"]))
    for t, name, part, first in rep["shadowed"]:
        line += "\n    shadowed: %s %s of %s (%s, loaded first, carries it; the old install never used this one)" % (
            t, name, part, first)
    if a.verify:
        key = packs.md5_key(a.maps)
        got, want = md5(out), packs.load(HERE, pack).get(key)
        if got == want:
            line += "\n    IDENTICAL to the recorded build (md5 %s)" % got
        elif a.record:
            packs.set_settings(HERE, pack, {key: [got]})
            line += "\n    recorded as the reviewed build (%s %s, was %s)" % (key, got, want)
        else:
            print("%s: %s\n    DIFFERS from the recorded %s %s (md5 %s)" % (pack, line, key, want, got), flush=True)
            return 1
    print("%s: %s" % (pack, line), flush=True)
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("zones", nargs="*", help="zone names (default: every zone_source/*.csv)")
    ap.add_argument("--game", default=os.environ.get("CODOL_GAME", DEFAULT_GAME))
    ap.add_argument("--zz", default=os.environ.get("CODOL_ZZ", DEFAULT_ZZ))
    ap.add_argument("--exe", help="C1.exe of 1.1.1.13 (default: <game>\\C1.exe)")
    ap.add_argument("--out")
    ap.add_argument("--maps", default=os.environ.get("CODOL_MAPS", "ship"), choices=shipping.MODES)
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("--record", action="store_true",
                    help="with --verify: record each zone that differs from its origin as the reviewed build (>build-md5)")
    ap.add_argument("--decompile", action="store_true")
    ap.add_argument("--assetexplorer")
    ap.add_argument("--fresh-key", action="store_true")
    ap.add_argument("--compile-as", action="store_true")
    ap.add_argument("--store-class", action="append", default=[])
    ap.add_argument("--parts", action="store_true",
                    help="build every source zone on its own (the layout before zone_packs/) into output_parts\\")
    ap.add_argument("--candidate", action="store_true", help="build unqualified intentional changes into output_candidate instead of shipping output")
    ap.add_argument("--ffdec", default=os.environ.get("CODOL_FFDEC", DEFAULT_FFDEC))
    a = ap.parse_args(argv)
    exe = a.exe or os.path.join(a.game, "C1.exe")
    chinese = os.path.join(a.game, "client_shipRetail", "chinese")
    ffdec = a.ffdec if a.ffdec and os.path.isfile(a.ffdec) else None
    roots = {"G": a.game, "Z": a.zz}
    a.out = a.out or os.path.join(HERE, ("output_candidate" if a.candidate else "output" if a.maps == "ship" else "output_dev")
                                  + ("_parts" if a.parts else ""))
    if not a.candidate and not a.decompile and not a.list and not a.store_class and a.maps == "ship":
        try:
            ui_baseline.validate(HERE)
        except (ui_baseline.BaselineError, FileNotFoundError) as e:
            ap.error(str(e))
    try:
        maps = shipping.ShippingMaps.load(HERE, a.maps)
    except shipping.ShippingError as e:
        ap.error(str(e))
    known = sorted(f[:-4] for f in os.listdir(os.path.join(HERE, "zone_source")) if f.endswith(".csv"))
    try:
        part_of = packs.part_index(HERE)
    except packs.PackError as e:
        ap.error(str(e))
    all_packs = packs.names(HERE)
    if a.list:
        for pack in all_packs:
            m = packs.load(HERE, pack)
            print("%-36s %-26s %s list, %s: %s" % (pack, m.get("install"), packs.stage(pack),
                                                   "/".join(sorted(packs.locales(pack))),
                                                   " + ".join(p for p, _f, _q in packs.parts(m))))
        for name in known:
            m = build.load(HERE, name)
            print("%-36s %-26s from %s%s" % (name, m.get("install"), m.get("origin"),
                                             " (part of %s)" % " + ".join(part_of[name]) if name in part_of else ""))
        for state in shipping.STATES:
            print("%s maps (%s): %s" % (state, shipping.MANIFEST, " ".join(maps.by_state(state)) or "-"))
        return 0
    for spec in a.store_class:
        zone, rawfile, cls = spec.split(":")
        if not ffdec:
            ap.error("--store-class needs FFDec (--ffdec)")
        m = build.load(HERE, zone)
        ref = [q for t, n, q in m.assets if t == "rawfile" and n == rawfile]
        if not ref or "gfx" not in ref[0]:
            ap.error("%s has no gfx rawfile %s" % (zone, rawfile))
        pdir = os.path.join(HERE, "zones", zone, "rawfile", rawfile.replace("/", os.sep) + ".gfxpatch")
        stock_movie = stock.ZoneSource(exe, [chinese]).rawfile(ref[0]["gfx"])
        fn = gfx.store_class(pdir, stock_movie, cls, "as/%s.as" % cls.replace(".", "/"), ffdec)
        print("%s: %s stored as %s" % (zone, cls, fn))
    if a.store_class:
        return 0
    with tempfile.TemporaryDirectory() as tmp:
        if a.decompile:
            names = a.zones or list(zonelist.ZONES)
            return 1 if run_decompile(names, exe, roots, chinese, ffdec, tmp) else 0
        unknown = [n for n in a.zones if n not in known and n not in all_packs]
        if unknown:
            ap.error("no zone_source or zone_packs for %s (python build_zones.py --list)" % ", ".join(unknown))
        if a.parts:
            names, pack_list = ([n for n in a.zones if n in known] if a.zones else known), []
        elif a.zones:
            names = [n for n in a.zones if n in known and n not in part_of]
            pack_list = sorted(set([n for n in a.zones if n in all_packs] + [q for n in a.zones if n in part_of for q in part_of[n]]))
            for n in a.zones:
                if n in part_of:
                    print("%s is a part of %s: building that (--parts builds it on its own)" % (n, " + ".join(part_of[n])))
        else:
            names, pack_list = [n for n in known if n not in part_of], all_packs
        failed = 0
        src = stock.ZoneSource(exe, [chinese])
        print("usermaps: %s build -> %s (ship: %d maps%s)" % (
            "shipping" if a.maps == "ship" else "ALL-MAPS (dev, not for players)", a.out, len(maps.by_state("ship")),
            "" if a.maps == "ship" else ", dev: %d maps" % len(maps.by_state("dev"))), flush=True)
        if a.verify and not a.parts and not a.zones:
            # every source zone that went into a pack still verifies against the file it came from
            print("source zones of the packs (built into a temporary folder, not installed):", flush=True)
            failed += build_zones(sorted(part_of), os.path.join(tmp, "parts"), a, exe, src, maps, roots, ffdec, tmp)
            print("installed zones:", flush=True)
        failed += build_zones(names, a.out, a, exe, src, maps, roots, ffdec, tmp)
        for pack in pack_list:
            failed += build_one_pack(pack, a, exe, src, maps, ffdec)
        if not a.zones:
            copy_files(a.out, a.game, a.verify, maps)
        names = names + pack_list
    print("\n%d zones, %d failed" % (len(names), failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
