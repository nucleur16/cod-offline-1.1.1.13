from pathlib import Path
import sys
import unittest
from unittest.mock import patch
ROOT=Path(__file__).absolute().parents[1]
sys.path.insert(0,str(ROOT))
from zonekit import ui_baseline

class SharedUIBaseline(unittest.TestCase):
    def test_native_quick_and_date_components_are_pinned_and_mutations_rejected(self):
        import json
        contract=json.loads((ROOT/'ui_baseline.json').read_text())
        names=['kernel33.c','quick_start_gate.c','quick_start_policy.h','quick_start_guards.h',
               'gfx_date_time.c','gfx_date_time_policy.h','gfx_date_time_guards.h']
        read=Path.read_bytes
        for name in names:
            with self.subTest(name=name):
                self.assertIn(name,contract['files'])
                target=ROOT/name
                with patch.object(Path,'read_bytes',lambda p:read(p)+b'\n' if p==target else read(p)):
                    with self.assertRaises(ui_baseline.BaselineError):
                        ui_baseline.validate(ROOT)

    def test_recorded_source_and_tags_match(self):
        self.assertEqual(ui_baseline.validate(ROOT)["retired_playlists"],"hidden even if Quick Match is unavailable")

    def test_reintroduced_old_playlist_fallback_is_rejected(self):
        target=ROOT/"zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/controllers/MatchController.as"
        read=Path.read_bytes
        changed=read(target).replace(b"return visiblePlaylists;",b"return choices;")
        self.assertNotEqual(changed,read(target))
        with patch.object(Path,"read_bytes",lambda p:changed if p==target else read(p)):
            with self.assertRaisesRegex(ui_baseline.BaselineError,"MatchController"):
                ui_baseline.validate(ROOT)

    def test_language_specific_functional_movie_is_rejected(self):
        target=ROOT/"zone_source/codol_chinese_ui.csv"
        read=Path.read_text
        def text(p,*args,**kwargs):
            data=read(p,*args,**kwargs)
            return data+"\nrawfile,scaleform/default/codo.gfx\n" if p==target else data
        with patch.object(Path,"read_text",text):
            with self.assertRaisesRegex(ui_baseline.BaselineError,"split feature versions"):
                ui_baseline.validate(ROOT)

    def test_stale_compiled_source_metadata_is_rejected(self):
        real=ui_baseline.gfx.read_patch
        def stale(path):
            meta,ops=real(path)
            meta["as"]=[(cls,rel,"stale" if cls=="codo.controllers.MatchController" else digest) for cls,rel,digest in meta["as"]]
            return meta,ops
        with patch.object(ui_baseline.gfx,"read_patch",stale):
            with self.assertRaisesRegex(ui_baseline.BaselineError,"compiled source metadata differs"):
                ui_baseline.validate(ROOT)

if __name__=="__main__":
    unittest.main()
