# -*- coding: utf-8 -*-
r"""check_zone_outputs.py -- prove that two build outputs ship the same assets, each in exactly one file.

    python tests/check_zone_outputs.py --old OLD_OUT --new NEW_OUT [--game DIR]

OLD_OUT and NEW_OUT are build_zones.py output roots (client_shipRetail\{patch,chinese}\*.ff, files\ copies), e.g. the
build of the base commit and of a consolidation. Needs C1.exe for the zone key (read only), so it runs by hand;
tests/test_zone_packs.py is the portable half.

Every asset is keyed by (type, name, md5 of its blob); the name is read for rawfiles, localize entries, stringtables
and xanims (other types are keyed by content). A pack's marker (the empty rawfile named after its own file) is not an
asset of anything and is left out on both sides. It requires:
  1. the same multiset of assets in the old and the new output: nothing lost, nothing added, nothing duplicated;
  2. every asset of an old file in exactly one new file: the file of the same name, or, for a file that is gone, the
     file it was folded into (src/patch_locale_policy.h kPatchZoneMerged, followed to its end);
  3. every xanim names the same bones and notetracks through the new file's script strings as through the old one's
     (a pack that took a part's string table must have kept the indices);
  4. every other file (files\ copies: sound_channels.txt, iwds) byte for byte the same, at the same path.
It prints the old -> new file mapping with asset counts and the file counts before and after.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from zonekit import codecs, container, xanim_opposite_arm, zoneimage  # noqa: E402

DEFAULT_GAME = r"D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13"
XANIM = zoneimage.type_id("xanim")


def md5(b):
    return hashlib.md5(b).hexdigest()


def merged_table(root=ROOT):
    with open(os.path.join(root, "src", "patch_locale_policy.h"), encoding="utf-8") as f:
        text = f.read()
    body = text[text.index("kPatchZoneMerged[][2]={"):]
    return dict(re.findall(r'\{"([^"]+)","([^"]+)"\}', body[:body.index("};")]))


def merged_chain(table, name):
    out, seen = [], {name}
    while name in table and table[name] not in seen:
        name = table[name]
        seen.add(name)
        out.append(name)
    return out


def asset_name(tid, blob):
    try:
        if tid == 36:
            return codecs.decode_rawfile(blob)[0]
        if tid == 27:
            return codecs.decode_localize(blob)[0]
        if tid == 37:
            return codecs.decode_stringtable(blob)[0]
    except codecs.NotDecodable:
        return None
    if tid == XANIM and len(blob) > 88 and blob[:4] == b"\xff\xff\xff\xff":
        end = blob.find(b"\0", 88)
        return blob[88:end].decode("latin-1") if end > 0 else None
    return None


def is_marker(tid, blob, stem):
    if tid != 36:
        return False
    try:
        name, data = codecs.decode_rawfile(blob)[:2]
    except codecs.NotDecodable:
        return False
    return name.lower() == stem.lower() and not data


def walk(root):
    """-> {relative path (with /): absolute path} of every file under an output root."""
    out = {}
    for d, _sub, files in os.walk(root):
        for fn in files:
            p = os.path.join(d, fn)
            out[os.path.relpath(p, root).replace(os.sep, "/")] = p
    return out


def read_output(root, exe):
    """-> (zones {rel: (zone image, [(type, name, md5, blob)])}, other files {rel: md5})."""
    zones, other = {}, {}
    for rel, p in sorted(walk(root).items()):
        with open(p, "rb") as f:
            data = f.read()
        if not rel.lower().endswith(".ff"):
            other[rel] = md5(data)
            continue
        img = zoneimage.parse(container.read_any(p, exe))
        stem = os.path.basename(rel)[:-3]
        items = [(zoneimage.type_name(t), asset_name(t, b), md5(b), b)
                 for t, _ptr, b in img.assets if not is_marker(t, b, stem)]
        zones[rel] = (img, items)
    return zones, other


def xanim_names(blob, strings):
    """An xanim's bones and notetracks by name (None when the parser does not take it: compared by bytes only)."""
    try:
        clip = xanim_opposite_arm.parse(blob, strings)
    except (xanim_opposite_arm.Unsupported, ValueError, IndexError):
        return None
    return clip.bones, tuple(s for s, _tail in clip.notifies)


def target_of(rel, new_zones, table):
    """The new file that must carry an old file's assets: the same path, else where its name was folded into."""
    if rel in new_zones:
        return rel
    folder, fn = rel.rsplit("/", 1)
    for into in merged_chain(table, fn[:-3]):
        cand = "%s/%s.ff" % (folder, into)
        if cand in new_zones:
            return cand
    return None


def compare(old_zones, old_other, new_zones, new_other, table):
    """-> (problems, report lines)."""
    problems, lines = [], []
    key = lambda it: (it[0], (it[1] or "").lower(), it[2])  # noqa: E731
    old_count = collections.Counter(key(it) for _img, items in old_zones.values() for it in items)
    new_count = collections.Counter(key(it) for _img, items in new_zones.values() for it in items)
    for k in sorted(set(old_count) | set(new_count)):
        if old_count[k] != new_count[k]:
            problems.append("%s %s (md5 %s): %d in the old output, %d in the new" % (k[0], k[1] or "-", k[2],
                                                                                    old_count[k], new_count[k]))
    new_where = collections.defaultdict(set)
    for rel, (_img, items) in new_zones.items():
        for it in items:
            new_where[key(it)].add(rel)
    mapping = collections.Counter()
    for rel, (oimg, items) in sorted(old_zones.items()):
        tgt = target_of(rel, new_zones, table)
        if tgt is None:
            problems.append("%s: not in the new output and not folded into a file that is" % rel)
            continue
        nimg = new_zones[tgt][0]
        new_items = {key(it): it for it in new_zones[tgt][1]}
        # the same table (pointers, strings, order) keeps every index of every asset; else each xanim by name
        same_strings = (oimg.string_ptrs, oimg.strings) == (nimg.string_ptrs, nimg.strings)
        if oimg.strings:
            lines.append("%s: %d script strings, %s in %s" % (rel, len(oimg.strings),
                                                              "the same table" if same_strings else "a different table",
                                                              tgt))
        for it in items:
            k = key(it)
            if k not in new_items:
                problems.append("%s: %s %s (md5 %s) is not in %s (it is in %s)" % (
                    rel, it[0], it[1] or "-", it[2], tgt, ", ".join(sorted(new_where.get(k, ()))) or "no new file"))
                continue
            mapping[(rel, tgt)] += 1
            if old_count[k] == 1 and new_where[k] != {tgt}:
                problems.append("%s: %s %s is in %s, expected only %s" % (rel, it[0], it[1], sorted(new_where[k]), tgt))
            if oimg.strings and not same_strings and it[0] != "xanim":
                problems.append("%s: %s %s moved into %s, whose script string table is not this file's" % (
                    rel, it[0], it[1] or "-", tgt))
            elif oimg.strings and not same_strings:
                before = xanim_names(it[3], oimg.strings)
                after = xanim_names(new_items[k][3], nimg.strings)
                if before is None or before != after:
                    problems.append("%s: xanim %s names %s through %s's script strings, %s before" % (
                        rel, it[1], after and "different bones/notetracks" or "invalid strings", tgt,
                        "unreadable" if before is None else "these"))
    for rel, digest in sorted(old_other.items()):
        if new_other.get(rel) != digest:
            problems.append("%s: %s in the new output" % (rel, "missing" if rel not in new_other else "different"))
    for rel in sorted(set(new_other) - set(old_other)):
        problems.append("%s: new file that the old output did not have" % rel)
    lines.append("old file -> new file (assets):")
    for (rel, tgt), n in sorted(mapping.items()):
        lines.append("  %-52s -> %-52s %5d%s" % (rel, tgt, n, "" if rel == tgt else "  (folded)"))
    for rel in sorted(old_zones):
        if not any(r == rel for r, _t in mapping) and not old_zones[rel][1]:
            lines.append("  %-52s    (no assets besides its marker)" % rel)
    lines.append("fastfiles: %d old -> %d new; other files: %d old -> %d new" % (
        len(old_zones), len(new_zones), len(old_other), len(new_other)))
    lines.append("assets: %d old, %d new (pack markers left out)" % (sum(old_count.values()), sum(new_count.values())))
    return problems, lines


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--old", required=True, help="the base build's output root")
    ap.add_argument("--new", required=True, help="the changed build's output root")
    ap.add_argument("--game", default=os.environ.get("K33_GAME", DEFAULT_GAME))
    a = ap.parse_args(argv)
    exe = os.path.join(a.game, "C1.exe")
    old_zones, old_other = read_output(a.old, exe)
    new_zones, new_other = read_output(a.new, exe)
    problems, lines = compare(old_zones, old_other, new_zones, new_other, merged_table())
    print("\n".join(lines))
    if problems:
        print("\nFAILED:\n  " + "\n  ".join(problems[:50]) + ("\n  ... %d more" % (len(problems) - 50)
                                                            if len(problems) > 50 else ""))
        return 1
    print("\nevery old asset is in exactly one new file, byte for byte")
    return 0


if __name__ == "__main__":
    sys.exit(main())
