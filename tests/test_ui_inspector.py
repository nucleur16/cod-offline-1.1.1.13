import importlib.util
import json
import os
import struct
import subprocess
import sys
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('ui_inspector',ROOT/'tools/ui_inspector.py')
ui=importlib.util.module_from_spec(spec);spec.loader.exec_module(ui)


class InspectorTests(unittest.TestCase):
    def test_actual_survival_visibility_and_wallet(self):
        path=ROOT/'zones/survival_hud_postgfx_mp/components/survival_status.json'
        for count in (0,1,4,32):
            result=ui.load_menu(path,ui.DEFAULT_DVARS|{'surHUD_enemy':count,'surHUD_credits':9876})
            items=result['menus'][0]['items']
            self.assertEqual(items[1]['visible'],count>1)
            self.assertEqual(items[2]['visible'],count==1)
            self.assertEqual(items[3]['text'],'Credits: 9876')
            self.assertEqual(result['warnings'],[])
        hidden=ui.load_menu(path,ui.DEFAULT_DVARS|{'ui_active':1})
        self.assertFalse(hidden['menus'][0]['visible'])
        self.assertTrue(all(not i['visible'] for i in hidden['menus'][0]['items']))

    def test_plain_menu_geometry_events_and_visibility(self):
        result=ui.parse_menu('''{ menuDef { name "sample" rect 0 0 640 480
          onOpen { exec "do not execute"; }
          itemDef { name "credits" rect 8 -30 0 0 8 10 text "Credits" textscale 0.24
          forecolor .16 .65 1 1 visible ( dvarint "show" == 1 ) }
        } }''',{'show':1})
        item=result['menus'][0]['items'][0]
        self.assertEqual(item['rect'],[8,-30,0,0]);self.assertEqual(item['align'],[8,10])
        self.assertTrue(item['visible']);self.assertEqual(item['text'],'Credits')
        self.assertIn('event/script block is not executed: onOpen',result['warnings'])

    def test_no_eval_and_unsupported_values_reported(self):
        expr=ui.Expression()
        self.assertIsNone(expr.text('__import__("os")'))
        self.assertTrue(expr.unknown)

    def test_binary_sidecar_and_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'x.bin';p.write_bytes(b'\0'*50)
            with self.assertRaisesRegex(ValueError,'sidecar'):ui.load_menu(p)
            p.with_suffix('.json').write_bytes((ROOT/'zones/survival_hud_postgfx_mp/components/survival_status.json').read_bytes())
            self.assertEqual(len(ui.load_menu(p,ui.DEFAULT_DVARS)['menus']),1)

    def test_fullscreen_anchors_keep_status_on_screen(self):
        for width,height in ((640,480),(1280,720),(1920,1080),(2560,1440)):
            x,y,_,_=ui.anchor([8,-30,0,0],[8,10],width,height)
            self.assertAlmostEqual(x,8*height/480)
            self.assertAlmostEqual(y,height-30*height/480)

    def test_compressed_swf_restores_gfx_header(self):
        import struct,zlib
        body=b'hello';header=b'CFX'+bytes([10])+struct.pack('<I',13)
        self.assertEqual(ui.gfx_uncompress(header+zlib.compress(body)),b'GFX'+header[3:]+body)

    def test_offscreen_screenshot_and_overlap_regression(self):
        try:from PIL import Image
        except ImportError:self.skipTest('Pillow needed for rendered-pixel validation')
        with tempfile.TemporaryDirectory() as tmp:
            source=ROOT/'zones/survival_hud_postgfx_mp/components/survival_status.json'
            png=Path(tmp)/'current.png'
            subprocess.run([sys.executable,str(ROOT/'tools/ui_inspector.py'),str(source),'--snapshot',str(png)],check=True,capture_output=True)
            image=Image.open(png).convert('RGB');self.assertEqual(image.size,(1280,720))
            # Actual blue glyph pixels must exist, not just an empty screenshot.
            pixels=image.crop((0,600,400,720)).getdata()
            self.assertGreater(sum(b>r+40 and g>r+30 for r,g,b in pixels),50)
            report=json.loads(png.with_suffix('.report.json').read_text())
            self.assertFalse(any('Text bounds overlap' in w for w in report['preview_warnings']))
            old=json.loads(source.read_text());item=old['p']['8'][0]['p']['284'][3]
            raw=bytearray.fromhex(item['raw']);struct.pack_into('<f',raw,8,-36);struct.pack_into('<f',raw,0xd4,.63);item['raw']=raw.hex()
            legacy=Path(tmp)/'legacy.json';legacy.write_text(json.dumps(old));png=Path(tmp)/'legacy.png'
            subprocess.run([sys.executable,str(ROOT/'tools/ui_inspector.py'),str(legacy),'--snapshot',str(png)],check=True,capture_output=True)
            report=json.loads(png.with_suffix('.report.json').read_text())
            self.assertTrue(any('Text bounds overlap' in w for w in report['preview_warnings']))


if __name__=='__main__':unittest.main()
