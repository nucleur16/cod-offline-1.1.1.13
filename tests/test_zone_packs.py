"""The consolidated patch zones (zone_packs/) agree with Kernel33's old-name table and with their source zones.

Portable (no game files): tests/check_zone_packs.py is the half that loads the built zones.
"""
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).absolute().parents[1]
sys.path.insert(0, str(ROOT))
from zonekit import build, packs  # noqa: E402

PATCH = r"client_shipRetail\patch"
# source zones that cannot join a pack and still install under their own name
STANDALONE = {"codol_infected_cyborg_postgfx_mp": "C#-written: its assets point into its own runtime blocks",
              # the sh1x server mounts mp_*.ff of the patch folder with every MP map: a pack name would hide them
              "mp_codol_weapons": "mp_*: the sh1x server mounts it too (the MP weapon tables)",
              "mp_codol_weapons_postgfx_mp": "mp_*: the sh1x server mounts it too; C#-written (the MP weapon art)"}


def merged_table():
    text = (ROOT / "src/patch_locale_policy.h").read_text(encoding="utf-8")
    body = text[text.index("kPatchZoneMerged[][2]={"):]
    body = body[:body.index("};")]
    return dict(re.findall(r'\{"([^"]+)","([^"]+)"\}', body))


def source_zones():
    return sorted(p.stem for p in (ROOT / "zone_source").glob("*.csv"))


class ZonePacks(unittest.TestCase):
    def test_every_patch_folder_zone_is_packed_or_listed_standalone(self):
        idx = packs.part_index(str(ROOT))
        for z in source_zones():
            if build.load(str(ROOT), z).get("install") != PATCH:
                continue
            with self.subTest(zone=z):
                self.assertTrue(z in idx or z in STANDALONE, "%s is neither in a pack nor standalone" % z)
                self.assertFalse(z in idx and z in STANDALONE)

    def test_kernel33_skips_every_old_name_for_its_pack(self):
        table = merged_table()
        idx = packs.part_index(str(ROOT))
        self.assertEqual(set(table), set(idx), "patch_locale_policy.h kPatchZoneMerged vs zone_packs parts")
        for old, into in table.items():
            with self.subTest(zone=old):
                # a zone split by type is replaced by the pack that keeps its non-string assets (the shared one)
                self.assertIn(into, idx[old])
                if len(idx[old]) > 1:
                    self.assertEqual(packs.locales(into), packs.locales(old))

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

    def test_stage_and_locale_rules_match_kernel33(self):
        self.assertEqual(packs.stage("codol_common_postgfx_mp"), "post_gfx")
        self.assertEqual(packs.stage("codol_common_ui_mp"), "ui")
        self.assertEqual(packs.stage("codol_common"), "pre_gfx")
        self.assertEqual(packs.locales("codol_english_locale"), {"en"})
        self.assertEqual(packs.locales("codol_chinese_locale"), {"zh", "native"})


if __name__ == "__main__":
    unittest.main()
