"""Fail shipping builds when a merge drops the recorded shared UI baseline.

This source/artifact gate does not replace real-client acceptance. Intentional
changes build with --candidate until their reviewed source and compiled tags
are recorded as the next baseline.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
from . import gfx, manifest

CONTRACT = "ui_baseline.json"

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
    patch = root / "zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch"
    meta, _ = gfx.read_patch(str(patch))
    stored = {cls: digest for cls, _, digest in meta["as"]}
    for cls, row in contract["classes"].items():
        if stored.get(cls) != row["source_sha256"]:
            problems.append(cls + ": compiled source metadata differs")
    if problems:
        raise BaselineError("Shared UI baseline differs; use --candidate for deliberate changes and review the baseline update: " + "; ".join(problems[:12]))
    return contract
