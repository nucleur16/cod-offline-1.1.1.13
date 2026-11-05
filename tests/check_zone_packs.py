# -*- coding: utf-8 -*-
r"""check_zone_packs.py -- prove the consolidated patch zones (zone_packs/) load exactly what the old zones loaded.

    python tests/check_zone_packs.py                  shipping build: output\ against the source zones
    python tests/check_zone_packs.py --maps all       all-maps build: output_dev\
    python tests/check_zone_packs.py --old DIR        compare with these old .ff files instead of a fresh --parts build
                                                      (e.g. an install's client_shipRetail\patch)

Needs the game (C1.exe for the zone key, client_shipRetail\chinese for the stock movies), so it is not part of CI;
tests/test_zone_packs.py is the portable half. Run build_zones.py first: this reads the built .ff files themselves.

For each language Kernel33 can select (en, zh, native: patch_locale_policy.h) it loads the old and the new patch
folder the way C1 does -- the code_pre_gfx list's patch zones, then the code_post_gfx list's, then the UI list's,
each in Kernel33's alphabetical order, the first asset linked under a (type, name) kept -- and requires:
  1. the same assets reach the game, byte for byte, in the same list, except the
     twelve explicitly reviewed Chinese party/loading values in party_locale_contract.py;
  2. no new zone of a list carries an asset another zone of that list (or itself) also carries;
  3. every asset of every old zone is in exactly one new zone, unless no language ever used it (an earlier zone
     carried the same name: reported as shadowed);
plus the two features this consolidation must keep: the endless difficulty button in codo.gfx's LevelChooser (Cam's
#18) and comaplist column 23 (endless Zombies) on zm_asylum_sh and zm_prototype_sh.
It also prints which old winners depend on C1's first-wins rule (the old zones' own duplicates).
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import os
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from zonekit import build, codecs, container, gfx, packs, shipping, stock, zoneimage  # noqa: E402
from party_locale_contract import expected_effective  # noqa: E402

DEFAULT_GAME = r"D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13"
PHASES = ("pre_gfx", "post_gfx", "ui")          # the order C1 loads the three lists in
LANGS = ("en", "zh", "native")
LEVELCHOOSER = "zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/tags/5148_82_components_LevelChooser.bin"


def h(b):
    return hashlib.md5(b).hexdigest()


def read_zone(path, exe):
    return zoneimage.parse(container.read_any(path, exe))


def named(img):
    """[(tid, name or None, blob)] of a zone image (names read from the blobs)."""
    out = []
    for tid, _ptr, blob in img.assets:
        out.append((tid, asset_name(tid, blob), blob))
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
        pass
    return None     # other types are keyed by content (zonekit/packs.py already refuses a repeated manifest name)


def load_order(zones, lang):
    """Kernel33's view of a patch folder: [(phase, zone)] in load order, for one language."""
    keep = [z for z in zones if lang in packs.locales(z)]
    return [(ph, z) for ph in PHASES for z in sorted(keep, key=str.lower) if packs.stage(z) == ph]


def effective(order, assets, first_wins=True):
    """-> {(tid, name): (phase, zone, md5)}: what C1 ends up with. assets[zone] = [(tid, name, blob)]."""
    eff = {}
    for ph, z in order:
        for tid, name, blob in assets[z]:
            key = (tid, (name or h(blob)).lower())
            if key in eff and first_wins:
                continue
            eff[key] = (ph, z, h(blob))
    return eff


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--maps", default="ship", choices=shipping.MODES)
    ap.add_argument("--game", default=os.environ.get("CODOL_GAME", DEFAULT_GAME))
    ap.add_argument("--new", help="the built patch folder (default output[_dev]\\client_shipRetail\\patch)")
    ap.add_argument("--old", help="old .ff files to compare with (default: the source zones built as before)")
    a = ap.parse_args(argv)
    exe = os.path.join(a.game, "C1.exe")
    new_dir = a.new or os.path.join(ROOT, "output" if a.maps == "ship" else "output_dev", "client_shipRetail", "patch")
    part_of = packs.part_index(ROOT)
    pack_names = packs.names(ROOT)
    known = sorted(f[:-4] for f in os.listdir(os.path.join(ROOT, "zone_source")) if f.endswith(".csv"))
    patch_zones = [z for z in known if build.load(ROOT, z).get("install") == r"client_shipRetail\patch"]
    standalone = [z for z in patch_zones if z not in part_of]
    problems = []

    # --- the old patch folder: every patch-folder source zone as its own zone
    old = {}
    if a.old:
        for z in patch_zones:
            p = os.path.join(a.old, z + ".ff")
            if not os.path.isfile(p):
                problems.append("old folder has no %s.ff" % z)
                continue
            old[z] = named(read_zone(p, exe))
    else:
        maps = shipping.ShippingMaps.load(ROOT, a.maps)
        src = stock.ZoneSource(exe, [os.path.join(a.game, "client_shipRetail", "chinese")])
        for z in patch_zones:
            m = build.load(ROOT, z)
            img, _plan = build.assemble_filtered(ROOT, z, m, src, maps=maps, log=lambda *x: None)
            old[z] = named(zoneimage.parse(img))
    # --- the new patch folder: the packs and the zones that still ship on their own
    new = {}
    for z in pack_names + standalone:
        p = os.path.join(new_dir, z + ".ff")
        if not os.path.isfile(p):
            problems.append("%s has no %s.ff (run build_zones.py%s)" % (new_dir, z, "" if a.maps == "ship" else
                                                                        " --maps all"))
            continue
        new[z] = named(read_zone(p, exe))
    if problems:
        print("\n".join(problems))
        return 1
    markers = {h(codecs.encode_rawfile(p, b"", {})) for p in pack_names}
    unnamed = sum(1 for zs in (old, new) for items in zs.values() for _t, n, _b in items if n is None)
    print("rawfiles, localize entries and stringtables are keyed by name, the other %d assets by content" % unnamed)

    # 1 + 2: per language, exact winners (including the bounded reviewed translation) and no duplicates.
    used_somewhere = set()
    for lang in LANGS:
        o_order, n_order = load_order(list(old), lang), load_order(list(new), lang)
        o_eff, n_eff = effective(o_order, old), effective(n_order, new)
        try:
            o_eff = expected_effective(o_eff, lang, codecs.encode_localize)
        except ValueError as error:
            problems.append(str(error))
        if lang in ('zh', 'native'):
            print('%s: expected twelve reviewed Chinese party/loading values; all other identities unchanged' % lang)
        used_somewhere |= {v[2] for v in o_eff.values()}
        dup = collections.Counter()
        for ph, z in n_order:
            for tid, name, blob in new[z]:
                dup[(ph, tid, (name or h(blob)).lower())] += 1
        dups = [k for k, c in dup.items() if c > 1]
        o_set = {(ph, md) for ph, _z, md in o_eff.values()}
        n_set = {(ph, md) for ph, _z, md in n_eff.values() if md not in markers}
        missing, extra = o_set - n_set, n_set - o_set
        print("%-6s old: %2d zones, %5d assets reach the game | new: %d zones, %5d (+%d zone markers) | %s" % (
            lang, len(o_order), len(o_set), len(n_order), len(n_set), len(n_eff) - len(n_set),
            "SAME" if not missing and not extra and not dups else "DIFFERENT"))
        by_md5 = {md: (ph, z, k) for k, (ph, z, md) in o_eff.items()}
        for ph, md in sorted(missing)[:10]:
            problems.append("%s: %s asset %s of old %s is not loaded by the new zones" % (lang, ph, by_md5[md][2],
                                                                                        by_md5[md][1]))
        nb = {md: (ph, z, k) for k, (ph, z, md) in n_eff.items()}
        for ph, md in sorted(extra)[:10]:
            problems.append("%s: new %s loads %s, which the old zones did not" % (lang, nb[md][1], nb[md][2]))
        for k in dups[:10]:
            problems.append("%s: %s list carries %s %s more than once" % (lang, k[0], zoneimage.type_name(k[1]), k[2]))

    # 3: every old asset in exactly one new zone (or shadowed everywhere)
    where = collections.defaultdict(set)
    for z, items in new.items():
        for _tid, _n, blob in items:
            where[h(blob)].add(z)
    counts = collections.Counter()
    shadowed = []
    for z, items in old.items():
        for tid, name, blob in items:
            md = h(blob)
            places = where.get(md, set())
            if md not in used_somewhere:
                shadowed.append((z, zoneimage.type_name(tid), name))
                if places:
                    problems.append("%s %s of %s was never used, yet new %s carries it" % (
                        zoneimage.type_name(tid), name, z, ", ".join(sorted(places))))
                continue
            if len(places) != 1:
                problems.append("%s %s of %s is in %d new zones (%s)" % (zoneimage.type_name(tid), name, z,
                                                                       len(places), ", ".join(sorted(places))))
            counts[(z, next(iter(places)) if places else "-")] += 1
    print("\nold zone -> new zone (assets):")
    for (z, n), c in sorted(counts.items()):
        print("  %-34s -> %-34s %5d" % (z, n, c))
    for z, t, n in shadowed:
        print("  %-34s -> (shadowed, used by no language: an earlier old zone carried %s %s)" % (z, t, n))
    print("new zones: " + ", ".join("%s %d" % (z, len(new[z])) for z in sorted(new)))

    # rule dependence: winners that would change if C1 kept the last asset instead of the first
    for lang in LANGS:
        o_order = load_order(list(old), lang)
        f, l = effective(o_order, old, True), effective(o_order, old, False)
        diff = sorted(k for k in f if f[k][2] != l[k][2])
        if diff:
            print("%s: %d old winner(s) rely on first-wins: %s" % (
                lang, len(diff), ", ".join("%s %s (%s over %s)" % (zoneimage.type_name(k[0]), k[1], f[k][1], l[k][1])
                                          for k in diff)))

    # the features this consolidation must keep
    ui = [z for z in new if packs.stage(z) == "ui"]
    movie = [b for z in ui for tid, n, b in new[z] if tid == 36 and n == "scaleform/default/codo.gfx"]
    with open(os.path.join(ROOT, LEVELCHOOSER), "rb") as f:
        tag = f.read()
    if len(movie) != 1:
        problems.append("codo.gfx is in %d UI zones" % len(movie))
    else:
        data = codecs.decode_rawfile(movie[0])[1]
        lc = [data[s:e] for code, s, e, nm in gfx.tags(data) if code == gfx.T_DOABC and nm.endswith("LevelChooser")]
        ok = lc == [tag] and b"endlessLayout" in tag
        print("codo.gfx (%s): LevelChooser is the #18 endless tag: %s" % (", ".join(ui), "yes" if ok else "NO"))
        if not ok:
            problems.append("codo.gfx's LevelChooser is not the endless-button tag")
    tables = [(z, b) for z in new for tid, n, b in new[z] if tid == 37 and n == "mp/comaplist.csv"]
    if len(tables) != 1:
        problems.append("mp/comaplist.csv is in %d new zones" % len(tables))
    else:
        rows = codecs.decode_stringtable(tables[0][1])[1]
        flags = {r[4].decode("latin-1"): r[23].decode("latin-1") for r in rows if len(r) > 23}
        ok = flags.get("zm_asylum_sh") == "1" and flags.get("zm_prototype_sh") == "1"
        print("mp/comaplist.csv (%s, %d rows): column 23 endless: zm_asylum_sh=%r zm_prototype_sh=%r" % (
            tables[0][0], len(rows), flags.get("zm_asylum_sh"), flags.get("zm_prototype_sh")))
        if not ok:
            problems.append("comaplist column 23 lost the endless flags")
    if problems:
        print("\nFAILED:\n  " + "\n  ".join(problems))
        return 1
    print("\nall checks passed (%s maps)" % a.maps)
    return 0


if __name__ == "__main__":
    sys.exit(main())
