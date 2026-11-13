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
        names=['src/kernel33.c','src/rooms/quick_start_gate.c','src/rooms/quick_start_policy.h','src/rooms/quick_start_guards.h',
               'src/gfx/gfx_date_time.c','src/gfx/gfx_date_time_policy.h','src/gfx/gfx_date_time_guards.h',
               'src/kernel/kernel_call_guard.c','src/kernel/kernel_room_context.c','src/kernel/kernel_room_guards.h',
               'src/kernel/kernel_room_policy.h','src/production_profile.h','scripts/build-profile.sh',
               'src/kernel/kernel_name_policy.h','src/kernel/kernel_name_guards.h',
               'src/kernel/kernel_name_render.c','src/kernel/kernel_name_display.c','src/borderless.c','src/kernel/kernel_video_mode.c']
        read=Path.read_bytes
        for name in names:
            with self.subTest(name=name):
                self.assertIn(name,contract['files'])
                target=ROOT/name
                with patch.object(Path,'read_bytes',lambda p:read(p)+b'\n' if p==target else read(p)):
                    with self.assertRaises(ui_baseline.BaselineError):
                        ui_baseline.validate(ROOT)

    def test_reviewed_center_and_factor_files_remain_individually_pinned(self):
        # Each accepted successor pin remains a refusal boundary, not a global refresh.
        import json,hashlib
        contract=json.loads((ROOT/'ui_baseline.json').read_text(encoding='utf-8'))
        base='zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/'
        names=('as/codo/controllers/MatchController.as','tags/6167_82_codo_controllers_MatchController.bin',
               'as/codo/views/OptionView.as','tags/5988_82_codo_views_OptionView.bin','patch.txt')
        read=Path.read_bytes
        for name in names:
            target=ROOT/(base+name)
            self.assertEqual(contract['files'][base+name],hashlib.sha256(read(target)).hexdigest())
            with self.subTest(name=name),patch.object(Path,'read_bytes',lambda p:read(p)+b'\n' if p==target else read(p)):
                with self.assertRaises(ui_baseline.BaselineError):ui_baseline.validate(ROOT)

    def test_selector_producer_inputs_and_actual_export_are_individually_pinned(self):
        import json,hashlib
        contract=json.loads((ROOT/'ui_baseline.json').read_text(encoding='utf-8'))
        names=('selector_producer.py','current_endpoint.py','selector_export_contract.py','abc_reader.py',
               'abc_list_reader.py','swf_data.py','recipe.json','Source.before.as.DATA',
               'factor-endpoint.tag.DATA','actual-export.as.DATA')
        read=Path.read_bytes
        for name in names:
            rel='tools/playlist-selector-producer/'+name;target=ROOT/rel
            self.assertEqual(contract['files'][rel],hashlib.sha256(read(target)).hexdigest())
            with self.subTest(name=name),patch.object(Path,'read_bytes',lambda p:read(p)+b'\n' if p==target else read(p)):
                with self.assertRaises(ui_baseline.BaselineError):ui_baseline.validate(ROOT)

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
