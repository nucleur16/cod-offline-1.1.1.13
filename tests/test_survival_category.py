"""The survival category's table and zone registration (portable; no game files).

    K33_SURVIVAL_TEST_EXE=<survival_category_test.exe> python -B tests/test_survival_category.py

mp/constconfigstrings_survival.csv must be a well-formed two-column constant table in C1 1.1.1.13's configstring
layout (it is a copy of code_post_gfx_mp's mp/constconfigstrings_coop.csv), ship in codol_common, and checksum the
same in C (survival_category_policy.h, what Kernel33 stores) as with zlib (what the server computes).
"""
from pathlib import Path
import os
import subprocess
import sys
import unittest
import zlib

ROOT = Path(__file__).absolute().parents[1]
sys.path.insert(0, str(ROOT))
from zonekit import build, codecs, packs, zonelist  # noqa: E402

CSV = ROOT / "zones/codol_survival_category/stringtable/mp/constconfigstrings_survival.csv"
ZONE = "codol_survival_category"


def rows():
    return [l.split(",", 1) for l in CSV.read_text(encoding="ascii").split("\n") if l]


def checksum(table):
    c = 0
    for r in table:
        for cell in r:
            c = zlib.crc32(cell.encode("ascii") + b"\0", c)
    return c


class SurvivalTable(unittest.TestCase):
    def test_table_is_a_two_column_constant_table(self):
        raw = CSV.read_bytes()
        self.assertTrue(raw.endswith(b"\n"))
        self.assertNotIn(b"\r", raw)
        self.assertNotIn(b'"', raw)
        self.assertTrue(all(b < 0x80 for b in raw))
        t = rows()
        self.assertEqual(len(t), 1803)
        idx = [int(r[0]) for r in t]
        self.assertEqual(idx, sorted(set(idx)), "configstring indices ascend without repeats")
        self.assertTrue(all("," not in r[1] for r in t))
        self.assertEqual(t[0], ["2", "cod"])
        self.assertLess(idx[-1], 6600)

    def test_table_is_in_c1_layout(self):
        """1.1.1.13's sections (code_post_gfx_mp): 2.6.9.7's start 72 slots later."""
        t = dict((int(a), b) for a, b in rows())
        self.assertEqual(t[1316], "defaultweapon")          # models
        self.assertEqual(t[4243], "quickmessages")          # menus
        self.assertEqual(t[5363], "defaultweapon_mp")       # weapons
        self.assertTrue(t[6134].startswith("footstep_right_large;"))
        self.assertEqual(t[6132], "0 0 0 1")

    def test_zone_carries_the_table_and_ships_in_codol_common(self):
        m = build.load(str(ROOT), ZONE)
        self.assertEqual(m.get("install"), r"client_shipRetail\patch")
        self.assertIn(("stringtable", "mp/constconfigstrings_survival.csv", {"columns": "2"}), m.assets)
        self.assertEqual([t for t, _n, _q in m.assets], ["rawfile", "stringtable"])
        self.assertEqual(packs.part_index(str(ROOT)).get(ZONE), ["codol_common"])
        self.assertEqual(packs.stage(ZONE), "pre_gfx")       # stringtables only: no image, model or menu
        self.assertEqual(zonelist.ZONES[ZONE]["origin"], "new")
        encoded = codecs.encode_stringtable("mp/constconfigstrings_survival.csv",
                                            [[c.encode("ascii") for c in r] for r in rows()], {"columns": "2"})
        name, decoded, _p = codecs.decode_stringtable(encoded)
        self.assertEqual(name, "mp/constconfigstrings_survival.csv")
        self.assertEqual([[c.decode() for c in r] for r in decoded], rows())

    def test_c_and_zlib_checksums_agree(self):
        exe = os.environ.get("K33_SURVIVAL_TEST_EXE")
        if not exe:
            self.skipTest("K33_SURVIVAL_TEST_EXE not set (tests/run_tests.sh sets it)")
        out = subprocess.run([exe, str(CSV)], check=True, capture_output=True, text=True).stdout.split()
        self.assertEqual(out[:2], ["rows", str(len(rows()))])
        self.assertEqual(int(out[3], 16), checksum(rows()))
        print("mp/constconfigstrings_survival.csv: %d rows, checksum 0x%08X" % (len(rows()), checksum(rows())))


if __name__ == "__main__":
    unittest.main()
