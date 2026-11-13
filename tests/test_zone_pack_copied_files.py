"""Actual copy/CLI functions on disposable files; no game assets or builds."""
import argparse
import ast
import contextlib
import hashlib
import io
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest


class CopiedFileVerificationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="codol-copy-check-")
        self.root = Path(self.temp.name).resolve()
        self.source = self.root / "files"
        self.install = self.root / "install"
        self.output = self.root / "output"
        self.source.mkdir()
        self.install.mkdir()
        (self.root / "zone_source").mkdir()
        self.filtered = False
        maps = SimpleNamespace(by_state=lambda state: [])
        shipping = SimpleNamespace(
            MODES=("ship", "all"), ShippingError=ValueError,
            ShippingMaps=SimpleNamespace(load=lambda *args: maps),
            filter_file=self.filter_file,
        )
        source = Path(__file__).resolve().parents[1] / "scripts/build_zones.py"
        tree = ast.parse(source.read_bytes())
        nodes = [n for n in tree.body if isinstance(n, ast.FunctionDef)
                 and n.name in ("md5", "copy_files", "main")]
        self.assertEqual({n.name for n in nodes}, {"md5", "copy_files", "main"})
        self.namespace = dict(
            __doc__="disposable CLI fixture", HERE=str(self.root), os=os,
            hashlib=hashlib, argparse=argparse, tempfile=tempfile,
            DEFAULT_GAME=str(self.install), DEFAULT_ZZ=str(self.root / "unused"),
            DEFAULT_FFDEC="", shipping=shipping,
            packs=SimpleNamespace(part_index=lambda _: {}, names=lambda _: [], PackError=ValueError),
            zonelist=SimpleNamespace(ZONES={}),
            stock=SimpleNamespace(ZoneSource=lambda *args: None),
            ui_baseline=SimpleNamespace(validate=lambda _: None, BaselineError=ValueError),
            build_zones=lambda *args: 0,
        )
        exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), "exec"), self.namespace)

    def tearDown(self):
        self.assertEqual(self.root.parent, Path(tempfile.gettempdir()).resolve())
        self.assertTrue(self.root.name.startswith("codol-copy-check-"))
        self.temp.cleanup()

    def filter_file(self, rel, data, maps):
        return (b"filtered output", {"fixture_map"}) if self.filtered else (data, set())

    def fixture(self, name, source=b"original", installed=None):
        for root, data in ((self.source, source), (self.install, installed)):
            if data is not None:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)

    def copy(self, verify=True):
        with contextlib.redirect_stdout(io.StringIO()) as log:
            failed = self.namespace["copy_files"](str(self.output), str(self.install), verify, object())
        return failed, log.getvalue()

    def cli(self, verify=True):
        args = ["--game", str(self.install), "--out", str(self.output), "--maps", "ship"]
        if verify:
            args.append("--verify")
        with contextlib.redirect_stdout(io.StringIO()) as log:
            result = self.namespace["main"](args)
        return result, log.getvalue()

    def test_missing_install_counts_failure_but_preserves_output(self):
        self.fixture("main/fixture.iwd")
        failed, log = self.copy()
        self.assertEqual(failed, 1)
        self.assertIn("differs from / missing in the install", log)
        self.assertEqual((self.output / "main/fixture.iwd").read_bytes(), b"original")

    def test_different_install_counts_failure_without_modifying_install(self):
        self.fixture("fixture", installed=b"different")
        self.assertEqual(self.copy()[0], 1)
        self.assertEqual((self.install / "fixture").read_bytes(), b"different")

    def test_matching_install_is_zero(self):
        self.fixture("fixture", installed=b"original")
        failed, log = self.copy()
        self.assertEqual(failed, 0)
        self.assertIn("IDENTICAL", log)

    def test_multiple_mismatches_accumulate(self):
        self.fixture("matched", installed=b"original")
        self.fixture("different", installed=b"changed")
        self.fixture("nested/missing")
        self.assertEqual(self.copy()[0], 2)

    def test_nonverify_copy_needs_no_install(self):
        self.fixture("fixture")
        self.assertEqual(self.copy(False)[0], 0)

    def test_filtered_copy_verifies_unfiltered_source_contract(self):
        self.filtered = True
        self.fixture("fixture", installed=b"original")
        failed, log = self.copy()
        self.assertEqual(failed, 0)
        self.assertIn("before the filter", log)
        self.assertEqual((self.output / "fixture").read_bytes(), b"filtered output")

    def test_filtered_output_is_not_install_reference(self):
        self.filtered = True
        self.fixture("fixture", installed=b"filtered output")
        self.assertEqual(self.copy()[0], 1)

    def test_cli_returns_failure_for_missing_copied_file(self):
        self.fixture("fixture")
        result, log = self.cli()
        self.assertEqual(result, 1)
        self.assertIn("1 failed", log)

    def test_cli_returns_failure_for_different_copied_file(self):
        self.fixture("fixture", installed=b"changed")
        self.assertEqual(self.cli()[0], 1)

    def test_cli_returns_success_for_matching_copied_file(self):
        self.fixture("fixture", installed=b"original")
        self.assertEqual(self.cli()[0], 0)

    def test_cli_nonverify_copy_remains_successful(self):
        self.fixture("fixture")
        self.assertEqual(self.cli(False)[0], 0)


if __name__ == "__main__":
    unittest.main()
