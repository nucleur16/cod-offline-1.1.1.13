#!/usr/bin/env python3
"""Stage the data a hosting client needs for the embedded sh1x match server, with the GSC from scripts/gsc.

    python scripts/stage_server_data.py --server server/sh1x --stage ../../k33_p2p_staging

Writes, under --stage (which mirrors the client install):
    p2p/home/main/sh1x_svr_scripts.iwd   the server pack: scripts/ from this repository's scripts/gsc, waypoints/
                                         and mp/ from the server repository (its tools/package.py layout)
    main/sh1x_rawfiles/                  the GSC tree, the tracked files of scripts/gsc (src/p2p/p2p.c's scripts=)

The GSC tree lives in this repository (scripts/gsc, a git subtree of 13-rawfiles; see README.md, "GSC scripts").
The server's own bin/home/codol_rawfiles submodule is never read here: it is what the standalone server build
uses. Only git-tracked files are staged (an untracked scratch file in scripts/gsc never reaches an install).
"""
import argparse
import importlib.util
import os
import pathlib
import shutil
import subprocess
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
GSC = "scripts/gsc"
SERVER_GSC = "bin/home/codol_rawfiles/"          # the server's tree; its pack entry is replaced by GSC
# The server's tools/package.py layout, used when that module cannot be loaded (its values win when it can).
PACK = "sh1x_svr_scripts.iwd"
PACK_TREES = ((SERVER_GSC, "scripts/"), ("bin/home/main/waypoints/", "waypoints/"), ("bin/home/main/mp/", "mp/"))
PACK_SKIP = {"COLLATION_CONFLICTS.txt", "COLLATION_MANIFEST.json"}
STAMP = (2026, 1, 1, 0, 0, 0)                   # fixed zip times: the pack is the same bytes for the same files


def git_files(root, subpath):
    out = subprocess.run(["git", "-C", str(root), "ls-files", "-z", "--", subpath],
                         check=True, capture_output=True).stdout
    return [p for p in out.decode("utf-8").split("\0") if p]


def server_layout(server):
    """(PACK, PACK_TREES, PACK_SKIP) of the server's tools/package.py, or this file's copy of them."""
    path = pathlib.Path(server) / "tools" / "package.py"
    if path.is_file():
        try:
            sys.path.insert(0, str(path.parent))
            spec = importlib.util.spec_from_file_location("_sh1x_package", path)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod.PACK, tuple(mod.PACK_TREES), set(mod.PACK_SKIP)
        except Exception as e:  # noqa: BLE001  (a server without the module's imports: same layout, our copy)
            print("note: %s not loaded (%s); using this script's copy of its pack layout" % (path, e))
        finally:
            sys.path.pop(0)
    return PACK, PACK_TREES, PACK_SKIP


def gsc_files():
    """[(path inside the GSC tree, absolute source)] for every tracked file of scripts/gsc."""
    files = []
    for rel in git_files(ROOT, GSC):
        src = ROOT / rel
        if src.is_file():
            files.append((rel[len(GSC) + 1:], src))
    if not files:
        raise SystemExit("%s has no tracked files" % GSC)
    return files


def build_pack(path, server):
    pack, trees, skip = server_layout(server)
    target = pathlib.Path(path) / pack
    target.parent.mkdir(parents=True, exist_ok=True)
    tmp = str(target) + ".tmp"
    count = gsc = 0
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for tree, prefix in trees:
            if tree == SERVER_GSC:
                entries = gsc_files()
            else:
                entries = [(rel[len(tree):], pathlib.Path(server) / rel) for rel in git_files(server, tree.rstrip("/"))]
            for rel, src in entries:
                if not src.is_file() or os.path.basename(rel) in skip:
                    continue
                info = zipfile.ZipInfo(prefix + rel, date_time=STAMP)
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, src.read_bytes())
                count += 1
                gsc += tree == SERVER_GSC
    os.replace(tmp, target)
    return target, count, gsc


def stage_rawfiles(stage):
    dest = pathlib.Path(stage) / "main" / "sh1x_rawfiles"
    if dest.exists():
        shutil.rmtree(dest)
    files = gsc_files()
    for rel, src in files:
        out = dest / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, out)
    return dest, len(files)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--server", required=True, help="the sh1x server checkout (server/sh1x or K33_SERVER_DIR)")
    ap.add_argument("--stage", required=True, help="the staging folder (mirrors the client install)")
    a = ap.parse_args()
    server = pathlib.Path(a.server).resolve()
    if not (server / "bin/home/main/waypoints").is_dir():
        ap.error("%s has no bin/home/main/waypoints (not a sh1x server checkout?)" % server)
    target, count, gsc = build_pack(pathlib.Path(a.stage) / "p2p" / "home" / "main", server)
    print("Server pack: %s (%d files, %d of them GSC from %s)" % (target, count, gsc, GSC))
    dest, n = stage_rawfiles(a.stage)
    print("GSC tree: %s (%d files from %s)" % (dest, n, GSC))


if __name__ == "__main__":
    main()
