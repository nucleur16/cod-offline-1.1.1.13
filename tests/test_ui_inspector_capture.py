import contextlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from unittest.mock import patch
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
spec = importlib.util.spec_from_file_location('capture_tested', ROOT / 'tools/ui_inspector_capture.py')
M = importlib.util.module_from_spec(spec)
spec.loader.exec_module(M)


class Capture(unittest.TestCase):
    def test_scenario_is_sdk_movie_input_and_capture_protocol(self):
        root = ET.fromstring(M.scenario_xml([dict(op='advance', seconds=.5), dict(op='click', x=23, y=42),
                                              dict(op='key', key='Enter'), dict(op='capture')]))
        self.assertEqual([x.get('name') for x in root], ['AutoAdvance', 'MouseMove', 'MouseButton',
                         'MouseButton', 'KeyPress', 'KeyPress', 'TakeScreenshot', 'Quit'])
        self.assertEqual(root[2].get('keyDown'), '1')
        self.assertEqual(root[3].get('keyDown'), '0')
        self.assertEqual(root[2].get('x'), '23')

    def test_static_backend_selects_actual_frames_and_bounds(self):
        self.assertEqual(M.static_frames([dict(op='advance', seconds=.5), dict(op='capture'),
                                          dict(op='frame', frame=2), dict(op='capture')], 30, 10), [2, 10])

    def test_static_never_claims_interactive_navigation(self):
        for op in ('click', 'key', 'move'):
            with self.assertRaisesRegex(ValueError, 'cannot execute'):
                M.static_frames([dict(op=op)], 30, 10)

    def test_unsafe_advance_rejected_before_process(self):
        for number in (-1, float('nan'), float('inf')):
            with self.assertRaises(ValueError): M.scenario_xml([dict(op='advance', seconds=number)])

    def test_frame_navigation_uses_internal_sdk_commands(self):
        root = ET.fromstring(M.scenario_xml([dict(op='frame', frame=12), dict(op='capture')]))
        self.assertEqual([x.get('name') for x in root], ['Restart', 'StepForward_10', 'StepForward_1', 'TakeScreenshot', 'Quit'])

    def test_old_capture_files_do_not_make_a_failed_sdk_attempt_succeed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp);out = root / 'preview'
            stale = out / 'captures/scenario.xml/old.png'
            stale.parent.mkdir(parents=True);stale.write_bytes(b'old capture')
            movie = b'GFX\x0a' + struct.pack('<I', 16) + b'\x08\0' + struct.pack('<HH', 30 * 256, 1) + b'\0\0'
            with patch('ui_inspector.movie_bytes', return_value=movie), \
                 patch('ui_inspector_assets.prepare_movie_assets', return_value={}), \
                 patch.object(M, '_private_desktop', return_value=contextlib.nullcontext(None)), \
                 patch.object(M.subprocess, 'CREATE_NO_WINDOW', 0, create=True), \
                 patch.object(M.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, b'', b'')):
                with self.assertRaises(RuntimeError):
                    M.capture_movie(root / 'movie.gfx', out, root / 'game', backend='sdk')
            report = json.loads((out / 'capture-report.json').read_text())
            self.assertEqual(report['captures'], [])
            self.assertEqual(report['attempts'][0]['exit_code'], 0)
            self.assertIn('no completed capture', report['attempts'][0]['error'])

    def test_static_reports_rejected_input_instead_of_running_player(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp);out = root / 'preview'
            movie = b'GFX\x0a' + struct.pack('<I', 16) + b'\x08\0' + struct.pack('<HH', 30 * 256, 1) + b'\0\0'
            with patch('ui_inspector.movie_bytes', return_value=movie), \
                 patch('ui_inspector_assets.prepare_movie_assets', return_value={}), \
                 patch.object(M.subprocess, 'run', side_effect=AssertionError('must not launch')):
                with self.assertRaises(RuntimeError):
                    M.capture_movie(root / 'movie.gfx', out, root / 'game',
                                    [dict(op='click', x=1, y=2), dict(op='capture')], backend='ffdec')
            report = json.loads((out / 'capture-report.json').read_text())
            self.assertIn('cannot execute', report['attempts'][0]['error'])
            self.assertEqual(report['captures'], [])

    @staticmethod
    def sprite_movie():
        def tag(code, payload):
            return struct.pack('<H', code << 6 | len(payload)) + payload
        body = b'\x08\0' + struct.pack('<HH', 30 * 256, 1)
        body += tag(39, struct.pack('<HH', 7, 2) + b'\0\0')
        body += tag(76, struct.pack('<HH', 1, 7) + b'pauseMenuView\0') + b'\0\0'
        return b'GFX\x0a' + struct.pack('<I', len(body) + 8) + body

    def test_character_resolves_symbol_and_numeric_id_in_compressed_movie(self):
        data = self.sprite_movie()
        compressed = b'CFX' + data[3:8] + zlib.compress(data[8:])
        for movie in (data, compressed):
            for selector in ('pauseMenuView', '7', 7):
                self.assertEqual(M.resolve_character(movie, selector), dict(id=7, name='pauseMenuView', frames=2))
            with self.assertRaisesRegex(ValueError, 'No exact SymbolClass'):
                M.resolve_character(movie, 'missing')
            with self.assertRaisesRegex(ValueError, 'not a DefineSprite'):
                M.resolve_character(movie, 0)

    def test_character_requires_explicit_static_backend(self):
        with self.assertRaisesRegex(ValueError, 'backend=ffdec'):
            M.capture_movie('missing.gfx', 'unused', 'game', character='pauseMenuView')

    def test_character_exports_actual_sprite_selected_frame_only(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp);out = root / 'preview'
            def render(command, **kwargs):
                self.assertEqual(command[command.index('-select') + 1], '7:2')
                self.assertEqual(command[command.index('-selectid') + 1], '7')
                self.assertEqual(command[command.index('-format') + 1], 'sprite:png')
                target = Path(command[command.index('-export') + 2]) / 'DefineSprite_7'
                target.mkdir();Image.new('RGBA', (2, 3), 'red').save(target / '2.png')
                return subprocess.CompletedProcess(command, 0, b'', b'')
            with patch('ui_inspector.movie_bytes', return_value=self.sprite_movie()), \
                 patch('ui_inspector_assets.prepare_movie_assets', return_value={}), \
                 patch.object(M.subprocess, 'run', side_effect=render):
                report = M.capture_movie(root / 'movie.gfx', out, root / 'game',
                                         [dict(op='frame', frame=2), dict(op='capture')],
                                         backend='ffdec', character='pauseMenuView')
            self.assertEqual(report['character']['id'], 7)
            self.assertEqual(report['capabilities'], dict(actionscript=False, movie_input=False, static_only=True))
            self.assertEqual(len(report['captures']), 1)

    def test_capture_cannot_write_into_game(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaisesRegex(ValueError, 'outside the game'):
                M.capture_movie('x.gfx', Path(temp) / 'preview', temp)


if __name__ == '__main__':
    unittest.main()
