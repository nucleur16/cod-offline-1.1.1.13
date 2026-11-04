import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ui_assets', ROOT / 'tools/ui_inspector_assets.py')
M = importlib.util.module_from_spec(spec)
spec.loader.exec_module(M)


def tag(code, body):
    return (struct.pack('<H', code << 6 | min(len(body), 63)) +
            (struct.pack('<I', len(body)) if len(body) >= 63 else b'') + body)


def movie(filename='hudChattingInputBG.tga', code=1009):
    name = filename.rsplit('.', 1)[0].encode()
    filename = filename.encode()
    body = struct.pack('<H' if code == 1001 else '<I', 6553) + struct.pack('<HHH', 13, 32, 16)
    body += bytes([len(name)]) + name + bytes([len(filename)]) + filename
    data = b'GFX\x0a' + b'\0' * 4 + b'\x08\0' + b'\0' * 4 + tag(code, body) + tag(0, b'')
    return data[:4] + struct.pack('<I', len(data)) + data[8:]


class UiAssets(unittest.TestCase):
    def test_actual_external_tag_layouts_and_compression(self):
        for code in (1001, 1009):
            plain = movie(code=code)
            compressed = b'CFX' + plain[3:8] + zlib.compress(plain[8:])
            for value in (plain, compressed):
                result = M.external_image_refs(value)
                self.assertEqual(len(result), 1)
                self.assertEqual(result[0], dict(tag=code, id=6553, format=13, width=32,
                                                height=16, export='hudChattingInputBG',
                                                filename='hudChattingInputBG.tga'))

    def test_truncated_movie_never_reports_a_resolved_asset(self):
        with tempfile.TemporaryDirectory() as temp:
            result = M.prepare_movie_assets(movie()[:-1], Path(temp) / 'preview', Path(temp) / 'game')
            self.assertTrue(result['errors'])
            self.assertEqual(result['resolved'], [])

    def test_local_tga_copy_preserves_actual_pixels_and_provenance(self):
        with tempfile.TemporaryDirectory() as temp:
            game, preview = Path(temp) / 'game', Path(temp) / 'preview'
            source = game / 'main/images/hudChattingInputBG.tga'
            source.parent.mkdir(parents=True)
            # A real two-pixel red/blue32bit top-left TGA test fixture.
            data = bytes([0, 0, 2]) + b'\0' * 9 + struct.pack('<HHBB', 2, 1, 32, 0x28)
            data += bytes([0, 0, 255, 255, 255, 0, 0, 128])
            source.write_bytes(data)
            with patch.object(M, '_runtime', side_effect=AssertionError('No decoder needed')):
                result = M.prepare_movie_assets(movie(), preview, game)
            self.assertEqual((preview / source.name).read_bytes(), data)
            self.assertEqual(result['resolved'][0]['source'], str(source))
            self.assertFalse(result['unresolved'])

    def test_traversal_is_reported_without_creating_outputs(self):
        with tempfile.TemporaryDirectory() as temp:
            preview = Path(temp) / 'preview'
            result = M.prepare_movie_assets(movie('../outside.tga'), preview, Path(temp) / 'game')
            self.assertEqual(result['resolved'], [])
            self.assertIn('unsafe', result['unresolved'][0]['error'])
            self.assertFalse(preview.exists())

    def test_output_inside_game_is_refused(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError):
                M.prepare_movie_assets(movie(), Path(temp) / 'preview', temp)

    def test_missing_decoder_is_actionable_and_never_claims_rendering(self):
        with tempfile.TemporaryDirectory() as temp:
            with patch.object(M, '_runtime', side_effect=FileNotFoundError('set CODOL_ASSETEXPLORER_BIN')):
                result = M.prepare_movie_assets(movie(), Path(temp) / 'preview', Path(temp) / 'game')
            self.assertEqual(result['resolved'], [])
            self.assertEqual(len(result['unresolved']), 1)
            self.assertIn('CODOL_ASSETEXPLORER_BIN', result['errors'][0])


if __name__ == '__main__':
    unittest.main()
