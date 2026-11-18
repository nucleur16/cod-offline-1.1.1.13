#!/usr/bin/env python3
"""Write the match server's pack, main/sh1x_svr_scripts.iwd, from this repository.

    python scripts/stage_server_data.py --out dist/main                  the pack only (what build.sh ships)
    python scripts/stage_server_data.py --out dist/main --rawfiles dist/main/sh1x_rawfiles
                                                                         also the GSC as loose files

The pack holds
    scripts/     the GSC, scripts/gsc (the host reads it through its "fs:scripts" root, src/p2p/p2p.c)
    waypoints/   bot waypoints, third_party/sh1x/data/waypoints
    mp/          bot class table, third_party/sh1x/data/mp

In a git checkout only tracked files are packed (an untracked scratch file in scripts/gsc never ships); without git
(a source download) every file is. Fixed zip times: the same files make the same bytes.
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
PACK = "sh1x_svr_scripts.iwd"
TREES = (("scripts/gsc", "scripts/"),
         ("third_party/sh1x/data/waypoints", "waypoints/"),
         ("third_party/sh1x/data/mp", "mp/"))
STAMP = (2026, 1, 1, 0, 0, 0)
SKIP = {"COLLATION_CONFLICTS.txt", "COLLATION_MANIFEST.json"}   # the GSC tree's merge bookkeeping, not scripts


def tree_files(sub):
    """[(path inside the tree, absolute source)], sorted."""
    base = ROOT / sub
    try:
        out = subprocess.run(["git", "-C", str(ROOT), "ls-files", "-z", "--", sub],
                             check=True, capture_output=True).stdout
        rels = [p[len(sub) + 1:] for p in out.decode("utf-8").split("\0") if p]
    except (OSError, subprocess.CalledProcessError):
        rels = []
    if not rels:  # not a git checkout
        rels = [p.relative_to(base).as_posix() for p in base.rglob("*") if p.is_file()]
    files = sorted((r, base / r) for r in rels if (base / r).is_file() and os.path.basename(r) not in SKIP)
    if not files:
        raise SystemExit("%s has no files" % sub)
    return files


def build_pack(out_dir):
    target = pathlib.Path(out_dir) / PACK
    target.parent.mkdir(parents=True, exist_ok=True)
    tmp = str(target) + ".tmp"
    count = 0
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for sub, prefix in TREES:
            for rel, src in tree_files(sub):
                info = zipfile.ZipInfo(prefix + rel, date_time=STAMP)
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, src.read_bytes())
                count += 1
    os.replace(tmp, target)
    return target, count


def stage_rawfiles(dest):
    dest = pathlib.Path(dest)
    if dest.exists():
        shutil.rmtree(dest)
    files = tree_files("scripts/gsc")
    for rel, src in files:
        out = dest / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, out)
    return len(files)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", required=True, help="the folder the pack is written to (an install's main\\)")
    ap.add_argument("--rawfiles", help="also copy the GSC as loose files to this folder (a script developer's overlay)")
    a = ap.parse_args()
    target, count = build_pack(a.out)
    print("Server pack: %s (%d files)" % (target, count))
    if a.rawfiles:
        print("GSC tree: %s (%d files)" % (a.rawfiles, stage_rawfiles(a.rawfiles)))


if __name__ == "__main__":
    main()
