"""Drift between scripts/gsc (this repository's GSC tree) and the 13-rawfiles commit the sh1x server pins as its
bin/home/codol_rawfiles submodule. Informational: it prints what differs and exits 0 (--strict: exit 1 on drift).

    python -B tests/test_gsc_drift.py [--server server/sh1x] [--fetch] [--strict]

The two trees are compared by git tree/blob ids (line endings as committed). The pinned commit's objects are looked
up in this repository and in the server's rawfiles submodule (when it is initialised); --fetch fetches the pinned
commit from 13-rawfiles into this repository's object store (FETCH_HEAD only, no ref is written) when neither has it.
Drift is expected while one side has moved: scripts/gsc is where GSC edits are made (README.md, "GSC scripts"); the
server's pin catches up when the server repository bumps its submodule.
"""
import argparse
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PREFIX = "scripts/gsc"
SUBMODULE = "bin/home/codol_rawfiles"
UPSTREAM = "https://github.com/CODOL-EMU/13-rawfiles.git"
MARKER = "maps/mp/gametypes/_callbacksetup.gsc"   # a file at the root of every 13-rawfiles commit


def git(cwd, *args, check=True):
    r = subprocess.run(["git", "-C", str(cwd), *args], capture_output=True)
    if check and r.returncode:
        raise RuntimeError(r.stderr.decode(errors="replace").strip())
    return r.stdout.decode("utf-8", errors="replace").strip() if r.returncode == 0 else None


def last_subtree_split():
    """(13-rawfiles commit last merged into scripts/gsc, the commit of this repository that merged it).

    git subtree add and pull (without --squash) are merges whose second parent is a 13-rawfiles commit: a tree with
    the GSC at its root. The newest such merge on the first-parent line is the last sync."""
    out = git(ROOT, "log", "--first-parent", "--merges", "--format=%H %P", "HEAD", check=False) or ""
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and git(ROOT, "cat-file", "-e", "%s:%s" % (parts[2], MARKER), check=False) is not None:
            return parts[2], parts[0]
    out = git(ROOT, "log", "-n1", "--format=%H%n%B", "--grep=^git-subtree-dir: %s$" % PREFIX, "HEAD", check=False)
    if not out:
        return None, None
    m = re.search(r"^git-subtree-split: ([0-9a-f]{40})", out, re.M)
    return (m.group(1) if m else None), out.split("\n", 1)[0]


def tree_entries(repo, treeish):
    """{path: blob id} of a tree, or None when repo does not have it."""
    out = git(repo, "ls-tree", "-r", "-z", treeish, check=False)
    if out is None:
        return None
    entries = {}
    for item in out.split("\0"):
        if item:
            meta, path = item.split("\t", 1)
            entries[path] = meta.split()[2]
    return entries


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--server", default=str(ROOT / "server" / "sh1x"))
    ap.add_argument("--fetch", action="store_true", help="fetch the pinned commit from 13-rawfiles if it is missing")
    ap.add_argument("--strict", action="store_true", help="exit 1 when the trees differ")
    a = ap.parse_args()
    server = pathlib.Path(a.server)

    split, merged_by = last_subtree_split()
    here_tree = git(ROOT, "rev-parse", "HEAD:" + PREFIX, check=False)
    if here_tree is None:
        print("gsc drift: %s is not in HEAD" % PREFIX)
        return 0
    edits = git(ROOT, "rev-list", "--count", "%s..HEAD" % merged_by, "--", PREFIX, check=False) if merged_by else None
    print("gsc drift: %s at 13-rawfiles %s%s" % (PREFIX, split[:9] if split else "?",
                                                   " + %s local commit(s)" % edits if edits and edits != "0" else ""))
    dirty = git(ROOT, "status", "--porcelain", "--", PREFIX, check=False)
    if dirty:
        print("gsc drift: note: %s has uncommitted changes (not compared)" % PREFIX)

    if not (server / ".git").exists():
        print("gsc drift: %s is not initialised; the server's rawfiles pin was not compared" % server)
        return 0
    line = git(server, "ls-tree", "HEAD", SUBMODULE, check=False)
    if not line:
        print("gsc drift: the server at %s pins no %s" % (server, SUBMODULE))
        return 0
    pin = line.split()[2]
    print("gsc drift: server %s pins 13-rawfiles %s" % (git(server, "rev-parse", "--short", "HEAD"), pin[:9]))

    sub = server / SUBMODULE
    repos = [ROOT] + ([sub] if (sub / ".git").exists() else [])
    pinned = None
    for repo in repos:
        pinned = tree_entries(repo, pin)
        if pinned is not None:
            break
    if pinned is None and a.fetch:
        git(ROOT, "fetch", "-q", "--no-tags", UPSTREAM, pin, check=False)
        pinned = tree_entries(ROOT, pin)
    if pinned is None:
        print("gsc drift: commit %s is not available locally (run with --fetch to compare)" % pin[:9])
        return 0

    here = tree_entries(ROOT, "HEAD:" + PREFIX)
    if here == pinned:
        same = "the same commit" if split == pin else "a different commit, the same tree"
        print("gsc drift: none: scripts/gsc and the server's pin are %s (%d files)" % (same, len(here)))
        return 0
    only_here = sorted(set(here) - set(pinned))
    only_pin = sorted(set(pinned) - set(here))
    changed = sorted(p for p in set(here) & set(pinned) if here[p] != pinned[p])
    print("gsc drift: DRIFT: %d changed, %d only in scripts/gsc, %d only in the server's pin" %
          (len(changed), len(only_here), len(only_pin)))
    for tag, paths in (("M", changed), ("+", only_here), ("-", only_pin)):
        for p in paths[:40]:
            print("    %s %s" % (tag, p))
        if len(paths) > 40:
            print("    %s ... %d more" % (tag, len(paths) - 40))
    print("gsc drift: informational; the server repository catches up by bumping its %s submodule" % SUBMODULE)
    return 1 if a.strict else 0


if __name__ == "__main__":
    sys.exit(main())
