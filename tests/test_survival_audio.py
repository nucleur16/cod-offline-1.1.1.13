"""Survival's sound alias blobs: what tools/survival_audio writes decodes back, and the committed zone carries it."""
import os
import re
import struct
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path[:0] = [ROOT, os.path.join(ROOT, "tools", "survival_audio")]

import aliases  # noqa: E402

GSC = os.path.join(ROOT, "scripts", "gsc", "maps", "mp", "gametypes")


def decode(b):
    """(name, [(subtitle, mixer, file, flags, speaker map)]) of a blob, every byte consumed."""
    assert struct.unpack_from("<2I", b, 0) == (aliases.INLINE, aliases.INLINE)
    count = struct.unpack_from("<I", b, 8)[0]
    p = 12

    def text():
        nonlocal p
        e = b.index(b"\0", p)
        s = b[p:e].decode("ascii")
        p = e + 1
        return s

    name = text()
    base = p
    p += 108 * count
    out = []
    for i in range(count):
        a = struct.unpack_from("<27I", b, base + 108 * i)
        assert a[0] == aliases.INLINE and a[5] == aliases.INLINE and a[22] == aliases.INLINE and a[26] == aliases.INLINE
        assert text() == name
        subtitle = text() if a[1] else None
        mixer = text() if a[4] else None
        kind, exists, _, d, n = struct.unpack_from("<BBHII", b, p)
        assert (kind, exists, d, n) == (2, 1, aliases.INLINE, aliases.INLINE)
        p += 12
        path = text() + "/" + text()
        p += 8 + 128
        assert text() == "$default"
        p += 8 + 400
        out.append((subtitle, mixer, path, a[14], text()))
    assert p == len(b), (p, len(b))
    return name, out


class SurvivalAudio(unittest.TestCase):
    def test_blobs_decode(self):
        for table in (aliases.BASE, aliases.PACK):
            for name, variants in table.items():
                got, out = decode(aliases.blob(name, variants))
                self.assertEqual(got, name)
                self.assertEqual([o[2] for o in out], [v["file"] for v in variants])
                self.assertEqual([o[0] for o in out], [v.get("subtitle") for v in variants])
                for o in out:
                    self.assertEqual((o[3] >> 7) & 3, 2, "streamed")
                    self.assertFalse(o[3] & 1, "not looping: the script repeats it")

    def test_zone_carries_base(self):
        manifest = open(os.path.join(ROOT, "zone_source", "patch_common_gametype_survival.csv"), encoding="utf-8").read()
        for name, variants in aliases.BASE.items():
            self.assertIn("sound,%s,file=sound/%s.bin\n" % (name, name), manifest)
            with open(os.path.join(ROOT, "zones", "patch_common_gametype_survival", "sound", name + ".bin"), "rb") as f:
                self.assertEqual(f.read(), aliases.blob(name, variants), "run tools/survival_audio/build.py zone")

    def test_script_aliases_exist(self):
        """Every survival alias the script names is one the zone or the pack defines."""
        with open(os.path.join(GSC, "_survival_audio.gsc"), encoding="utf-8") as f:
            script = f.read()
        named = set(re.findall(r'"((?:so_hq_|so_survival_|survival_(?:music_|vo_|wave_\w+_splash))\w*)"', script))
        named -= {"survival_music", "survival_vo"}
        self.assertTrue(named)
        self.assertEqual(named - set(aliases.BASE) - set(aliases.PACK), set())
        self.assertEqual(set(aliases.PACK) - named, set())

    def test_english_subtitles(self):
        with open(os.path.join(ROOT, "zones", "english", "localize.str"), encoding="utf-8") as f:
            have = {line.split("\t", 1)[0] for line in f}
        self.assertEqual([k for k in aliases.subtitles(aliases.PACK) if k not in have], [])


if __name__ == "__main__":
    unittest.main()
