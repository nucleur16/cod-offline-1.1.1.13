"""mp/basemaps.arena fits C1's 128-entry arena table (zonekit/shipping.py ARENA_MAX_ENTRIES).

C1 reads at most 128 arena entries and builds at most 128 maps from them; an entry past that is never read (no
display name on the load screen, no lobby scene). Portable: checks the usermaps_patch source arena (what the
all-maps build installs) and, with K33_C1_EXE set to a 1.1.1.13 C1.exe, the limits in its code.

    python tests/test_arena_cap.py
"""
import os
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).absolute().parents[1]
sys.path.insert(0, str(ROOT))
from zonekit import shipping, zonelist  # noqa: E402

ARENA = ROOT / "zones/usermaps_patch/rawfile/mp/basemaps.arena"
COMAPLIST = ROOT / "zones/usermaps_patch/stringtable/mp/comaplist.csv"


def arena_maps():
    entries, _t = shipping._arena_entries(ARENA.read_bytes())
    return [shipping._arena_map(e) for _g, e in entries]


class ArenaCap(unittest.TestCase):
    def test_source_arena_fits(self):
        n, pool = shipping.arena_check(ARENA.read_bytes(), "usermaps_patch mp/basemaps.arena")
        self.assertLessEqual(n, shipping.ARENA_MAX_ENTRIES)
        self.assertLessEqual(pool, shipping.ARENA_MAX_INFO_BYTES)

    def test_every_listed_usermap_is_read(self):
        maps = arena_maps()
        read = set(maps[:shipping.ARENA_MAX_ENTRIES])
        listed = [l.split()[0] for l in (ROOT / "shipping_maps.txt").read_text(encoding="utf-8").splitlines()
                  if l.strip() and not l.lstrip().startswith("#")]
        rows = {r.split(",")[4] for r in COMAPLIST.read_text(encoding="utf-8", errors="replace").splitlines()
                if r.count(",") > 4 and r.split(",")[5] in ("pvp", "zm")}
        missing = [m for m in listed if m in rows and m not in read]
        self.assertEqual(missing, [], "pvp/zm maps of shipping_maps.txt with no arena entry C1 reads")

    def test_no_name_twice(self):
        maps = arena_maps()
        dup = sorted({m for m in maps if maps.count(m) > 1})
        self.assertEqual(dup, [], "a second entry of a name is never reached (C1 takes the first)")

    def test_check_rejects_129(self):
        entry = b'{\n\tmap\t\t\t"mp_x%d"\n\tlongname\t"X"\n}\n'
        text = b"".join(entry % i for i in range(shipping.ARENA_MAX_ENTRIES + 1))
        with self.assertRaises(shipping.ShippingError):
            shipping.arena_check(text, "test")

    def test_prune_list_is_declared(self):
        spec = zonelist.ZONES["usermaps_patch"]["maps"] if hasattr(zonelist, "ZONES") else None
        if spec is None:
            self.skipTest("zonelist has no ZONES table")
        for m in spec.get("arena_prune", []):
            self.assertNotIn(m, arena_maps(), "%s is pruned from the stock text but still in the source" % m)

    @unittest.skipUnless(os.environ.get("K33_C1_EXE"), "K33_C1_EXE not set")
    def test_c1_limits(self):
        import pefile
        pe = pefile.PE(os.environ["K33_C1_EXE"], fast_load=True)
        img, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase

        def at(va, n):
            return img[va - base:va - base + n]
        # arena loader: mov edx, 0x80 ; sub edx, eax (the parse limit 0x80 - count)
        self.assertEqual(at(0x005EA1E7, 7), bytes.fromhex("ba800000002bd0"))
        # map list: cmp eax, 0x80 after the count increment
        self.assertEqual(at(0x005EA8A7, 5), bytes.fromhex("3d80000000"))
        # info pool: cmp eax, 0x10000
        self.assertEqual(at(0x005EA0BD, 5), bytes.fromhex("3d00000100"))


if __name__ == "__main__":
    unittest.main()
