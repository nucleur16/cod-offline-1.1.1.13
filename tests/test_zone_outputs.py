"""tests/check_zone_outputs.py's comparison on made-up outputs (portable: no game, no build).

The real run compares two build_zones.py outputs by hand (it needs C1.exe for the zone key)."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).absolute().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tests"))
import check_zone_outputs as cz  # noqa: E402
from zonekit import codecs, zoneimage  # noqa: E402

P = "client_shipRetail/patch/"


def zone(stem, rawfiles, strings=()):
    """A zone image of rawfiles {name: data} (and a script string table), as read_output returns it."""
    blobs = [codecs.encode_rawfile(n, d, {}) for n, d in rawfiles.items()]
    img = zoneimage.ZoneImage(blocks=[0] * 9, string_ptrs=[0xFFFFFFFF] * len(strings), strings=list(strings),
                              string_count_field=len(strings), strings_ptr_field=0xFFFFFFFF if strings else 0,
                              assets_ptr_field=0xFFFFFFFF, assets=[(36, 0xFFFFFFFF, b) for b in blobs])
    items = [("rawfile", n, cz.md5(b), b) for n, b in zip(rawfiles, blobs) if not cz.is_marker(36, b, stem)]
    return img, items


def run(old, new, table):
    return cz.compare(old, {}, new, {}, table)[0]


class CompareOutputs(unittest.TestCase):
    def test_a_folded_file_with_its_marker_left_out(self):
        old = {P + "a.ff": zone("a", {"a": b"", "x": b"1"}), P + "b.ff": zone("b", {"y": b"2"})}
        new = {P + "a.ff": zone("a", {"a": b"", "x": b"1", "y": b"2"})}
        self.assertEqual(run(old, new, {"b": "a"}), [])

    def test_a_chain_of_old_names_reaches_the_pack(self):
        old = {P + "old_b.ff": zone("old_b", {"y": b"2"}), P + "a.ff": zone("a", {"x": b"1"})}
        new = {P + "a.ff": zone("a", {"x": b"1", "y": b"2"})}
        self.assertEqual(run(old, new, {"old_b": "b", "b": "a"}), [])

    def test_a_lost_asset_fails(self):
        old = {P + "a.ff": zone("a", {"x": b"1"}), P + "b.ff": zone("b", {"y": b"2"})}
        new = {P + "a.ff": zone("a", {"x": b"1"})}
        self.assertTrue(run(old, new, {"b": "a"}))

    def test_a_changed_asset_fails(self):
        old = {P + "a.ff": zone("a", {"x": b"1"})}
        new = {P + "a.ff": zone("a", {"x": b"one"})}
        self.assertTrue(run(old, new, {}))

    def test_an_asset_in_two_new_files_fails(self):
        old = {P + "a.ff": zone("a", {"x": b"1"}), P + "b.ff": zone("b", {"y": b"2"})}
        new = {P + "a.ff": zone("a", {"x": b"1", "y": b"2"}), P + "c.ff": zone("c", {"y": b"2"})}
        self.assertTrue(run(old, new, {"b": "a"}))

    def test_a_file_gone_without_a_merge_entry_fails(self):
        old = {P + "a.ff": zone("a", {"x": b"1"}), P + "b.ff": zone("b", {"y": b"2"})}
        new = {P + "a.ff": zone("a", {"x": b"1", "y": b"2"})}
        self.assertTrue(run(old, new, {}))

    def test_a_string_table_must_move_whole(self):
        old = {P + "a.ff": zone("a", {"x": b"1"}), P + "b.ff": zone("b", {"y": b"2"}, ["tag_a", "tag_b"])}
        same = {P + "a.ff": zone("a", {"x": b"1", "y": b"2"}, ["tag_a", "tag_b"])}
        other = {P + "a.ff": zone("a", {"x": b"1", "y": b"2"}, ["tag_b", "tag_a"])}
        self.assertEqual(run(old, same, {"b": "a"}), [])
        self.assertTrue(run(old, other, {"b": "a"}))

    def test_other_files_must_match(self):
        z = {P + "a.ff": zone("a", {"x": b"1"})}
        self.assertEqual(cz.compare(z, {"main/x.iwd": "m"}, z, {"main/x.iwd": "m"}, {})[0], [])
        self.assertTrue(cz.compare(z, {"main/x.iwd": "m"}, z, {"main/x.iwd": "n"}, {})[0])
        self.assertTrue(cz.compare(z, {"main/x.iwd": "m"}, z, {}, {})[0])

    def test_the_real_merge_table_reaches_common_extras_from_both_m1911_names(self):
        table = cz.merged_table()
        self.assertEqual(cz.merged_chain(table, "m1911_arms_diag"), ["common_extras"])
        self.assertEqual(cz.merged_chain(table, "codol_zm_m1911_arms_diag")[-1], "common_extras")


if __name__ == "__main__":
    unittest.main()
