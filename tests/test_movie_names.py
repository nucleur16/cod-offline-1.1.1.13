"""The Scaleform movies spell no old k33_ / codol_ console name (tools/rename_movie_strings.py): portable, no game files.

Every stored DoABC tag of codo.gfx, codologin.gfx and codoingame.gfx is free of the names IW_TABLE renamed, every
.as digest in their patch.txt is current, and the C side registers or sets the names those movies now read."""
import contextlib
import io
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import rename_movie_strings as R  # noqa: E402


class MovieNames(unittest.TestCase):
    def test_tags_and_digests_current(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = R.run(True, None)
        self.assertEqual(rc, 0, out.getvalue())

    def test_c_side_spells_the_movie_names(self):
        src = "\n".join(p.read_text(encoding="utf-8", errors="replace") for p in (ROOT / "src").rglob("*.[ch]"))
        for name in ("cg_hudStyle", "cg_hudStyleList", "loc_languageName", "loc_languageList", "loc_languageState",
                     "loc_languageSerial", "gpad_info", "gpad_binds", "gpad_bindCapture", "gpad_bindClear",
                     "gpad_navLog", "gpad_snapScan", "gpad_snapHeader", "gpad_snapError", "gpad_snapProbe",
                     "gpad_snapAt", "gpad_snap0", "gpad_snapClass7", "gpad_snapHidden3", "cl_voiceVolume",
                     "cl_voiceMode", "cl_voiceThreshold", "cl_voiceMicMute", "cl_voiceLevel", "cl_voiceMeter",
                     "cl_voiceInputList", "cl_voiceInputPick", "cl_voiceInputIndex", "r_noborder",
                     "ui_quickNativeRoomContext", "ui_theatreRequest"):
            with self.subTest(name=name):
                self.assertIn('"%s"' % name, src)
        theatre = (ROOT / "src" / "theatre.c").read_text(encoding="utf-8")
        self.assertIn('"set ui_theatre%s', theatre)          # ThSet's fields complete the name: ui_theatreId0
        self.assertIn('"Id%d"', theatre)


if __name__ == "__main__":
    unittest.main()
