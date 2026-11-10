"""Pose preservation, remapping and malformed-payload checks; no game launch."""
from dataclasses import replace
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from zonekit.xanim_opposite_arm import Clip, Unsupported, complete, encode, layout, override_zone, parse
from zonekit.zoneimage import parse as parse_zone


def fixture(name, bones, groups, streams, frames=280):
    h = bytearray(88)
    struct.pack_into("<I", h, 0, 0xffffffff)
    struct.pack_into("<H", h, 14, frames)
    h[17:27] = bytes(groups)
    h[27] = 1
    struct.pack_into("<I", h, 48, 0xffffffff)
    struct.pack_into("<I", h, 80, 0xffffffff)
    for i, (offset, width) in enumerate(zip((4, 6, 8, 32, 10, 12), (1, 2, 4, 2, 1, 4))):
        n = len(streams[i])//width
        struct.pack_into("<I" if i == 3 else "<H", h, offset, n)
        struct.pack_into("<I", h, 52+i*4, 0xffffffff if n else 0)
    return Clip(bytes(h), name, tuple(bones), (("end", b"\x00\x00"+struct.pack("<f", 1.0)),), tuple(streams), b"")


def active():
    # One genuinely animated quaternion + translation, one existing static bone.
    return fixture("measured_right_idle_loop", ("j_shoulder_ri", "j_wrist_ri"),
                   (0, 0, 1, 0, 1, 1, 0, 1, 0, 2),
                   (b"\x00\x01", struct.pack("<7H", 1, 0, 280, 20, 30, 40, 32760)+struct.pack("<3H", 1, 0, 280),
                    struct.pack("<9f", 1, 2, 3, 4, 5, 6, 7, 8, 9),
                    struct.pack("<8h", 100, 200, 300, 32000, 200, 300, 400, 31900),
                    b"\x00\x01\x02\x03\x04\x05", b""))


def donor():
    return fixture("original_c1_static_dual", ("j_shoulder_ri", "j_shoulder_le", "j_sleave_reshape_top_le_1"),
                   (0, 0, 0, 0, 3, 0, 0, 3, 0, 3),
                   (b"\x00\x01\x02", struct.pack("<12h", *(list(range(11, 19))+[21, 22, 23, 32700])),
                    struct.pack("<9f", 100, 101, 102, -4, -5, -6, 10, 1.5, -0.75), b"", b"", b""), 0)


def byte_keys_precise_reload():
    clip = fixture("viewmodel_colt1911_dw_right_reload_empty",
                   ("j_shoulder_ri", "j_wrist_ri", "j_gun", "j_bolt"),
                   (0, 0, 1, 0, 3, 1, 1, 2, 0, 4),
                   (bytes((0, 6, 0, 0, 6, 1, 0, 6, 2, 3)),
                    struct.pack("<15H", *([1]+list(range(10, 22))+[1, 1])),
                    struct.pack("<18f", *range(18)),
                    struct.pack("<14h", *range(100, 114)),
                    bytes(range(6)), b""), frames=6)
    h = bytearray(clip.header)
    h[27] = 9
    notes = tuple((f"reload_event_{i}", b"\x00\x00"+struct.pack("<f", i/8)) for i in range(9))
    return replace(clip, header=bytes(h), notifies=notes)


class OppositeArm(unittest.TestCase):
    def test_preserves_active_motion_and_copies_nonzero_opposite_pose(self):
        old, src = active(), donor()
        new, names = complete(old, src, "le")
        self.assertEqual(names, ("j_shoulder_le", "j_sleave_reshape_top_le_1"))
        self.assertEqual(new.bones[:2], old.bones)
        self.assertEqual(new.streams[1][:14], old.streams[1][:14])  # Existing time keys and static quaternion.
        self.assertEqual(new.streams[1][-6:], old.streams[1][-6:])  # Existing translation time keys.
        self.assertEqual(new.streams[3:], old.streams[3:])          # All animated quaternion/translation samples.
        self.assertEqual(new.streams[2][:36], old.streams[2])
        self.assertEqual(struct.unpack("<6f", new.streams[2][36:]), (-4, -5, -6, 10, 1.5, -0.75))
        self.assertEqual(new.notifies, old.notifies)
        self.assertEqual(new.header[14:17], old.header[14:17])
        self.assertEqual(new.header[28:32], old.header[28:32])
        self.assertEqual(new.header[40:48], old.header[40:48])

    def test_compact_script_strings_round_trip_and_remap_notify(self):
        clip, _ = complete(active(), donor(), "le")
        strings = [None, "end", "j_sleave_reshape_top_le_1", "j_wrist_ri", "j_shoulder_le", "j_shoulder_ri"]
        self.assertEqual(parse(encode(clip, strings), strings), clip)

    def test_no_opposite_side_never_changes_an_active_side(self):
        with self.assertRaises(Unsupported):
            complete(active(), donor(), "ri")

    def test_rejects_unmeasured_fire_and_delta_shape(self):
        with self.assertRaises(Unsupported):
            complete(replace(active(), name="right_fire"), donor(), "le")
        h = bytearray(active().header)
        struct.pack_into("<I", h, 84, 0xffffffff)
        bad = replace(active(), header=bytes(h))
        with self.assertRaises(Unsupported):
            parse(encode(bad, [None, *bad.bones, "end"]), [None, *bad.bones, "end"])

    def test_rejects_bad_string_and_truncated_payload(self):
        clip = active()
        strings = [None, *clip.bones, "end"]
        raw = encode(clip, strings)
        for broken in (raw[:-1], raw+b"extra"):
            with self.assertRaises(Unsupported):
                parse(broken, strings)
        with self.assertRaises(Unsupported):
            parse(raw, [None])

    def test_rejects_unexpected_dynamic_quaternion_payload(self):
        clip = replace(active(), streams=active().streams[:3]+(b"\x00\x00",)+active().streams[4:])
        with self.assertRaises(Unsupported):
            complete(clip, donor(), "le")

    def test_rejects_invalid_translation_bone_indices(self):
        clip = replace(active(), streams=(b"\x00\x00",)+active().streams[1:])
        with self.assertRaises(Unsupported):
            complete(clip, donor(), "le")

    def test_standalone_zone_remaps_scripts_and_keeps_only_two_named_assets(self):
        right, _ = complete(active(), donor(), "le")
        left = replace(right, name="different_left_idle_loop")
        image = parse_zone(override_zone((right, left)))
        self.assertEqual(image.blocks[0], 0)  # No external resources.
        self.assertEqual(image.blocks[1], 4096)
        self.assertEqual([i for i, n in enumerate(image.blocks) if n], [1, 4])
        self.assertEqual(len(image.assets), 2)
        for (kind, pointer, raw), expected in zip(image.assets, (right, left)):
            self.assertEqual((kind, pointer), (2, 0xffffffff))
            self.assertEqual(parse(raw, image.strings), expected)
        self.assertEqual(override_zone((right, left)), override_zone((right, left)))

    def test_opt_in_byte_key_and_precise_translation_preserves_entire_motion(self):
        old = byte_keys_precise_reload()
        before = layout(old)
        new, added = complete(old, donor(), "le", all_states=True)
        after = layout(new)
        self.assertEqual(before.rotation_static_end, 26)
        self.assertEqual(before.translation_static_start, 8)
        self.assertEqual(before.translation_static_end, 10)
        self.assertEqual(new.streams[0][:10], old.streams[0])
        self.assertEqual(new.streams[1][:26], old.streams[1][:26])
        self.assertEqual(new.streams[1][26+8*len(added):], old.streams[1][26:])
        self.assertEqual(new.streams[2][:72], old.streams[2])
        self.assertEqual(new.streams[3:], old.streams[3:])
        self.assertEqual(after.rotation_key_records, before.rotation_key_records)
        self.assertEqual(new.notifies, old.notifies)
        self.assertEqual(new.header[23], 1)
        image = parse_zone(override_zone((new,)))
        self.assertEqual(parse(image.assets[0][2], image.strings), new)

    def test_opt_in_rejects_any_unmeasured_state_name(self):
        with self.assertRaises(Unsupported):
            complete(active(), donor(), "le", all_states=True)

    def test_byte_key_malformed_time_and_precise_sample_count_rejected(self):
        clip = byte_keys_precise_reload()
        malformed = []
        for time in (0, 7):
            b = bytearray(clip.streams[0])
            b[1] = time
            malformed.append(replace(clip, streams=(bytes(b),)+clip.streams[1:]))
        malformed.append(replace(clip, streams=clip.streams[:3]+(clip.streams[3][:-2],)+clip.streams[4:]))
        for bad in malformed:
            with self.assertRaises(Unsupported):
                complete(bad, donor(), "le", all_states=True)


if __name__ == "__main__":
    unittest.main()
