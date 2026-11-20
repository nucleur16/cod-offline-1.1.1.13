"""Decoder fixtures and an offscreen native-window / PNG-export smoke test."""
import os
os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
import struct
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

from PIL import Image
from iwi import DEFAULT_ROOT, decode, decode_image, scan_images


class DecodeTests(unittest.TestCase):
    def fixture(self, fmt, payload):
        data = struct.pack('<3sBI4H4I', b'IWi', 9, 3, fmt, 1, 1, 1, *([32 + len(payload)] * 4)) + payload
        p = Path(self.temp.name) / 'fixture.iwi'
        p.write_bytes(data)
        return p

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.temp.cleanup()

    def test_channel_order_and_alpha(self):
        for fmt, raw, expected in [(1, bytes([10, 20, 30, 40]), (30, 20, 10, 40)),
                (2, bytes([10, 20, 30]), (30, 20, 10, 255)),
                (3, bytes([70, 90]), (70, 70, 70, 90)),
                (4, bytes([70]), (70, 70, 70, 255)),
                (5, bytes([90]), (255, 255, 255, 90))]:
            with self.subTest(format=fmt):
                img, _ = decode(self.fixture(fmt, raw))
                self.assertEqual(img.getpixel((0, 0)), expected)

    def test_dxt_red(self):
        color = struct.pack('<HHI', 0xf800, 0, 0)
        for fmt, raw in [(11, color), (12, b'\xff' * 8 + color), (13, b'\xff\x00' + b'\x00' * 6 + color)]:
            with self.subTest(format=fmt):
                img, _ = decode(self.fixture(fmt, raw))
                self.assertEqual(img.getpixel((0, 0)), (255, 0, 0, 255))

    def test_truncated(self):
        with self.assertRaises(ValueError):
            decode(self.fixture(1, b'\0'))

    def test_recursive_mixed_images(self):
        root = Path(self.temp.name)
        nested = root / 'one' / 'two'
        nested.mkdir(parents=True)
        self.fixture(1, bytes([10, 20, 30, 40]))
        Image.new('RGBA', (3, 2), (10, 20, 30, 40)).save(nested / 'alpha.PNG')
        Image.new('RGB', (2, 3), 'red').save(root / 'one' / 'color.jpg')
        (nested / 'ignore.txt').write_text('not an image')
        items = scan_images(root)
        self.assertEqual(len(items), 3)
        image, meta = decode_image(nested / 'alpha.PNG')
        self.assertEqual(meta['format'], 'PNG')
        self.assertEqual(image.getpixel((0, 0)), (10, 20, 30, 40))

    def test_full_resolution_from_last_mip(self):
        p = self.fixture(1, b'\x99' * 4 + bytes([10, 20, 30, 40]))
        img, _ = decode(p)
        self.assertEqual(img.getpixel((0, 0)), (30, 20, 10, 40))

    def test_native_window_and_png_export(self):
        from PySide6.QtWidgets import QApplication, QFileDialog
        from desktop import Window, configure_app
        app = QApplication.instance() or QApplication([])
        configure_app(app)
        source = Path(DEFAULT_ROOT) / 'iw_image_00/images/cardboard_box_color.iwi'
        if not source.exists():
            source = self.fixture(1, bytes([10, 20, 30, 40]))
        window = Window(str(source))
        window.show()
        deadline = time.monotonic() + 20
        while window.image is None and time.monotonic() < deadline:
            app.processEvents()
            time.sleep(0.01)
        self.assertIsNotNone(window.image, window.meta.text())
        self.assertTrue(window.export.isEnabled())
        window.search.setText(source.stem.upper())
        self.assertEqual(window.proxy.rowCount(), 1)
        window.search.setText('no-such-image-name-123456')
        self.assertEqual(window.proxy.rowCount(), 0)
        window.search.clear()
        self.assertEqual(window.proxy.rowCount(), 1)
        target = Path(self.temp.name) / 'export.png'
        with patch.object(QFileDialog, 'getSaveFileName', return_value=(str(target), 'PNG image (*.png)')):
            window.export.click()
        with Image.open(target) as exported:
            self.assertEqual(exported.size, window.image.size)
            self.assertEqual(exported.convert('RGBA').tobytes(), window.image.tobytes())
        window.channel.setCurrentIndex(2)
        window.zoom.setCurrentIndex(2)
        app.processEvents()
        self.assertEqual(window.preview.transform().m11(), 2)
        window.channel.setCurrentIndex(0)
        window.zoom.setCurrentIndex(0)
        out = Path(__file__).parent / 'test-output'
        out.mkdir(exist_ok=True)
        window.grab().save(str(out / 'native-viewer.png'))
        window.close()
        window.pool.waitForDone()


if __name__ == '__main__':
    unittest.main()
