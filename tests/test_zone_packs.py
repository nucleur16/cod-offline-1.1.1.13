"""The consolidated patch zones (zone_packs/) agree with Kernel33's old-name table and with their source zones.

Portable (no game files): tests/check_zone_packs.py is the half that loads the built zones.
"""
import json
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).absolute().parents[1]
sys.path.insert(0, str(ROOT))
from zonekit import build, packs, zonelist  # noqa: E402

PATCH = r"client_shipRetail\patch"
# source zones that cannot join a pack and still install under their own name
STANDALONE = {"infected_cyborg_postgfx_mp": "C#-written: its assets point into its own runtime blocks",
              # the sh1x server mounts mp_*.ff of the patch folder with every MP map: a pack name would hide them
              "mp_weapons": "mp_*: the sh1x server mounts it too (the MP weapon tables)",
              "mp_weapons_postgfx_mp": "mp_*: the sh1x server mounts it too; C#-written (the MP weapon art)"}


def merged_table():
    text = (ROOT / "src/patch_locale_policy.h").read_text(encoding="utf-8")
    body = text[text.index("kPatchZoneMerged[][2]={"):]
    body = body[:body.index("};")]
    return dict(re.findall(r'\{"([^"]+)","([^"]+)"\}', body))


def merged_final(table, name):
    """Where the chain of kPatchZoneMerged ends for name (kernel33.c follows it, PatchZoneMergedChain)."""
    seen = []
    while name in table and name not in seen:
        seen.append(name)
        name = table[name]
    return name


def source_zones():
    return sorted(p.stem for p in (ROOT / "zone_source").glob("*.csv"))


class ZonePacks(unittest.TestCase):
    def test_every_patch_folder_zone_is_packed_or_listed_standalone(self):
        idx = packs.part_index(str(ROOT))
        for z in source_zones():
            if build.load(str(ROOT), z).get("install").replace("/", "\\") != PATCH:
                continue
            with self.subTest(zone=z):
                self.assertTrue(z in idx or z in STANDALONE, "%s is neither in a pack nor standalone" % z)
                self.assertFalse(z in idx and z in STANDALONE)

    def test_kernel33_skips_every_old_name_for_its_pack(self):
        table = merged_table()
        idx = packs.part_index(str(ROOT))
        legacy = zonelist.LEGACY_NAME
        # every part, under the name an install from before the packs carries, and every renamed pack or standalone
        # zone, under its old name; nothing else
        parts = {legacy.get(part, part): part for part in idx}
        renamed = {legacy[z]: z for z in legacy if z not in idx}
        # zones that installed under their own new name before they were packed: that file is an old name too
        later = zonelist.PACKED_LATER
        self.assertEqual(set(table), set(parts) | set(renamed) | set(later),
                         "patch_locale_policy.h kPatchZoneMerged vs zone_packs")
        for name, pack in later.items():
            with self.subTest(zone=name):
                self.assertEqual(idx.get(name), [pack])
                self.assertEqual(table[name], pack)
        for old, part in parts.items():
            with self.subTest(zone=old):
                # a zone split by type is replaced by the pack that keeps its non-string assets (the shared one)
                into = merged_final(table, old)
                self.assertIn(into, idx[part])
                if len(idx[part]) > 1:
                    self.assertEqual(packs.locales(into), packs.locales(part))
        for old, new in renamed.items():
            with self.subTest(zone=old):
                self.assertEqual(table[old], new)

    def test_packs_keep_the_load_list_and_language_of_their_parts(self):
        for pack in packs.names(str(ROOT)):
            m = packs.load(str(ROOT), pack)
            self.assertEqual(m.get("install"), PATCH)
            for part, _keep, q in packs.parts(m):
                with self.subTest(pack=pack, part=part):
                    self.assertIn(part, source_zones())
                    self.assertEqual(packs.stage(part), packs.stage(pack))
                    if "only" not in q and "exclude" not in q:
                        self.assertEqual(packs.locales(part), packs.locales(pack))

    def test_split_parts_cover_every_type_once(self):
        idx = packs.part_index(str(ROOT))
        for part, ps in idx.items():
            if len(ps) < 2:
                continue
            types = {t for t, _n, _q in build.load(str(ROOT), part).assets}
            keeps = [k for p in ps for name, k, _q in packs.parts(packs.load(str(ROOT), p)) if name == part]
            for t in types:
                with self.subTest(part=part, type=t):
                    self.assertEqual(sum(1 for k in keeps if k(t)), 1)

    def test_retired_and_new_names_never_collide(self):
        table = merged_table()
        for pack in packs.names(str(ROOT)):
            self.assertNotIn(pack, table)
            self.assertNotEqual(pack.lower(), "codol_zombies_endless")

    def test_a_pack_takes_at_most_one_part_with_script_strings(self):
        for pack in packs.names(str(ROOT)):
            with self.subTest(pack=pack):
                m = packs.load(str(ROOT), pack)
                withs = [part for part, _k, q in packs.parts(m) if "strings" in build.load(str(ROOT), part).settings]
                self.assertLessEqual(len(withs), 1, withs)
                for part, _k, q in packs.parts(m):
                    if part in withs:
                        self.assertFalse("only" in q or "exclude" in q)
                        types = {t for t, _n, _q in build.load(str(ROOT), part).assets}
                        self.assertLessEqual(types, set(packs.STRINGS_PART_TYPES))

    def test_m1911_xanims_move_with_their_string_table(self):
        m = build.load(str(ROOT), "m1911_arms_diag")
        zdir = ROOT / "zones" / "m1911_arms_diag"
        strings = json.loads((zdir / m.settings["strings"][0]).read_text(encoding="utf-8"))
        items = [(t, n, (0, 0xFFFFFFFF, (zdir / q["file"]).read_bytes())) for t, n, q in m.assets]
        self.assertEqual(len(items), 8)
        packs.strings_part_check("m1911_arms_diag", items, strings)     # all inline, every index a string
        with self.assertRaises(packs.PackError):
            packs.strings_part_check("m1911_arms_diag", items, strings[:5])
        bad = items[:1] + [("material", "x", (0, 0xFFFFFFFF, bytes(4 * [0xFF])))]
        with self.assertRaises(packs.PackError):
            packs.strings_part_check("m1911_arms_diag", bad, strings)
        self.assertIn("m1911_arms_diag", [p for p, _k, _q in packs.parts(packs.load(str(ROOT), "common_extras"))])

    def test_standalone_zones_are_only_the_listed_ones(self):
        idx = packs.part_index(str(ROOT))
        alone = {z for z in source_zones() if z not in idx and not zonelist.ZONES.get(z, {}).get("staging")
                 and build.load(str(ROOT), z).get("install").replace("/", "\\") == PATCH}
        self.assertEqual(alone, set(STANDALONE))

    def test_stage_and_locale_rules_match_kernel33(self):
        self.assertEqual(packs.stage("common_postgfx_mp"), "post_gfx")
        self.assertEqual(packs.stage("common_ui_mp"), "ui")
        self.assertEqual(packs.stage("common_extras"), "pre_gfx")
        self.assertEqual(packs.locales("english_locale"), {"en"})
        self.assertEqual(packs.locales("chinese_locale"), {"zh", "native"})


if __name__ == "__main__":
    unittest.main()
