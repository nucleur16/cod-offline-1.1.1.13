"""Fail shipping builds when a merge drops the recorded shared UI baseline.

This source/artifact gate does not replace real-client acceptance. Intentional
changes build with --candidate until their reviewed source and compiled tags
are recorded as the next baseline:

    python -m zonekit.ui_baseline --record [--add-class CLASS ...]

rewrites ui_baseline.json from the tree as it is: every pinned file's sha256 and
every pinned class's compiled source digest (patch.txt's "as" line); --add-class
pins one more codo.gfx class with its as/ source and its stored tag.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
from . import gfx, manifest

CONTRACT = "ui_baseline.json"
PATCH = "zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch"

class BaselineError(ValueError):
    pass

def validate(repo):
    root = Path(repo)
    contract = json.loads((root / CONTRACT).read_text(encoding="utf-8"))
    problems = []
    for name, expected in contract["files"].items():
        path = root / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            problems.append(name)
    for zone in ("codol_english_ui", "codol_english_ui_extra", "codol_chinese_ui", "codol_chinese_ui_extra"):
        data = manifest.read((root / "zone_source" / (zone + ".csv")).read_text(encoding="utf-8"))
        if any(t == "rawfile" and name.endswith(".gfx") for t, name, _ in data.assets):
            problems.append(zone + ": a language-specific movie would split feature versions")
    patch = root / PATCH
    meta, _ = gfx.read_patch(str(patch))
    stored = {cls: digest for cls, _, digest in meta["as"]}
    for cls, row in contract["classes"].items():
        if stored.get(cls) != row["source_sha256"]:
            problems.append(cls + ": compiled source metadata differs")
    if problems:
        raise BaselineError("Shared UI baseline differs; use --candidate for deliberate changes and review the baseline update: " + "; ".join(problems[:12]))
    return contract


def _sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def record(repo, add=(), upstream=None):
    """Rewrite the contract from the tree: file hashes, class digests, and the classes in `add`."""
    root = Path(repo)
    path = root / CONTRACT
    contract = json.loads(path.read_text(encoding="utf-8"))
    meta, ops = gfx.read_patch(str(root / PATCH))
    stored = {cls: (rel, digest) for cls, rel, digest in meta["as"]}
    tags = sorted(op[2] for op in ops if op[0] == "new")
    for cls in add:
        if cls not in stored:
            raise BaselineError("%s has no compiled source in %s/patch.txt" % (cls, PATCH))
        tag = [t for t in tags if t.endswith("_82_" + cls.replace(".", "_") + ".bin")]
        if len(tag) != 1:
            raise BaselineError("%s: expected one stored tag, found %s" % (cls, tag or "none"))
        contract["classes"][cls] = {"source_sha256": "", "source": PATCH + "/" + stored[cls][0]}
        contract["files"][PATCH + "/" + stored[cls][0]] = ""
        contract["files"][PATCH + "/" + tag[0]] = ""
    for cls, row in contract["classes"].items():
        if cls not in stored:
            raise BaselineError("%s is pinned but has no compiled source any more" % cls)
        row["source_sha256"] = stored[cls][1]
    contract["classes"] = dict(sorted(contract["classes"].items()))
    for name in contract["files"]:
        contract["files"][name] = _sha(root / name)
    if upstream:
        contract["client_upstream"] = upstream
    path.write_text(json.dumps(contract, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    validate(root)
    return contract


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser(description="record the shared UI baseline from the tree")
    ap.add_argument("--record", action="store_true", required=True)
    ap.add_argument("--add-class", action="append", default=[])
    ap.add_argument("--upstream", help="the client commit the recorded baseline is based on")
    a = ap.parse_args()
    c = record(Path(__file__).absolute().parents[1], a.add_class, a.upstream)
    print("%s: %d classes, %d files" % (CONTRACT, len(c["classes"]), len(c["files"])))
