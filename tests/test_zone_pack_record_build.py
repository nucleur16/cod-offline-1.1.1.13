"""Exercise the actual recorder against disposable manifests, without assets."""
import ast
import hashlib
import os
from pathlib import Path
import tempfile
import unittest


class RecordBuildTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="codol-record-build-")
        self.root = Path(self.temp.name).resolve()
        (self.root / "zone_source").mkdir()
        self.manifest = self.root / "zone_source" / "authored.csv"
        self.output = self.root / "fixture.ff"
        self.output.write_bytes(b"inert bytes, not a game fastfile")
        self.digest = hashlib.md5(self.output.read_bytes()).hexdigest().encode("ascii")
        source = Path(__file__).resolve().parents[1] / "build_zones.py"
        nodes = [n for n in ast.parse(source.read_bytes()).body
                 if isinstance(n, ast.FunctionDef) and n.name in ("md5", "record_build")]
        self.assertEqual({n.name for n in nodes}, {"md5", "record_build"})
        namespace = {"HERE": str(self.root), "os": os, "hashlib": hashlib}
        exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), "exec"), namespace)
        self.record = namespace["record_build"]
        parser = {}
        parser_source = source.parent / "zonekit" / "manifest.py"
        exec(compile(parser_source.read_bytes(), str(parser_source), "exec"), parser)
        self.parse = parser["read"]

    def tearDown(self):
        self.assertEqual(self.root.parent, Path(tempfile.gettempdir()).resolve())
        self.assertTrue(self.root.name.startswith("codol-record-build-"))
        self.temp.cleanup()

    def check_record(self, before, after):
        self.manifest.write_bytes(before)
        self.record("authored", str(self.output))
        self.assertEqual(self.manifest.read_bytes(), after)
        self.assertEqual(self.parse(after.decode("utf-8")).get("build-md5"), self.digest.decode("ascii"))

    def check_refusal(self, before):
        self.manifest.write_bytes(before)
        with self.assertRaises(ValueError):
            self.record("authored", str(self.output))
        self.assertEqual(self.manifest.read_bytes(), before)

    def test_new_origin_preserves_unrelated_rows(self):
        before = b"// keep\n>zone,authored\n>origin,new\n>build-md5-all,UNTOUCHED\nrawfile,fixture\n"
        after = before.replace(b">origin,new\n", b">origin,new\n>build-md5," + self.digest + b"\n")
        self.check_record(before, after)

    def test_new_origin_preserves_CRLF_and_missing_final_newline(self):
        before = b">zone,authored\r\n>origin,new\r\nrawfile,fixture"
        after = before.replace(b">origin,new\r\n", b">origin,new\r\n>build-md5," + self.digest + b"\r\n")
        self.check_record(before, after)

    def test_existing_origin_hash_remains_unchanged(self):
        old_hash = b"0123456789abcdef0123456789abcdef"
        before = b">origin,G:preserved\n>origin-md5," + old_hash + b"\nrawfile,fixture\n"
        after = before.replace(b">origin-md5," + old_hash + b"\n", b">origin-md5," + old_hash + b"\n>build-md5," + self.digest + b"\n")
        self.check_record(before, after)

    def test_existing_build_hash_replaced_in_place(self):
        before = b">origin,new\r\n>build-md5,OLD\r\n>build-md5-all,UNTOUCHED\r\n"
        self.check_record(before, before.replace(b">build-md5,OLD", b">build-md5," + self.digest))

    def test_unproven_historical_origin_refuses_without_write(self):
        self.check_refusal(b">origin,G:unproven\nrawfile,fixture\n")

    def test_duplicate_authored_origin_refuses_without_write(self):
        self.check_refusal(b">origin,new\n>origin,new\nrawfile,fixture\n")

    def test_missing_origin_refuses_without_write(self):
        self.check_refusal(b">zone,authored\nrawfile,fixture\n")

    def test_repeated_record_is_idempotent(self):
        before = b">origin,new\nrawfile,fixture\n"
        self.manifest.write_bytes(before)
        self.record("authored", str(self.output))
        first = self.manifest.read_bytes()
        self.record("authored", str(self.output))
        self.assertEqual(self.manifest.read_bytes(), first)

    def test_conflicting_plain_origins_refuse_without_write(self):
        before = b">origin,new\n>origin,G:unproven\nrawfile,fixture\n"
        self.assertEqual(self.parse(before.decode()).get("origin"), "G:unproven")
        self.check_refusal(before)

    def test_conflicting_quoted_origin_refuses_without_write(self):
        before = b'>origin,new\n">origin",G:unproven\nrawfile,fixture\n'
        self.assertEqual(self.parse(before.decode()).get("origin"), "G:unproven")
        self.check_refusal(before)

    def test_reverse_origin_order_still_refuses_ambiguity(self):
        before = b">origin,G:unproven\n>origin,new\nrawfile,fixture\n"
        self.assertEqual(self.parse(before.decode()).get("origin"), "new")
        self.check_refusal(before)

    def test_unique_quoted_authored_origin_is_supported(self):
        before = b'">origin","new"\nrawfile,fixture\n'
        after = b'">origin","new"\n>build-md5,' + self.digest + b"\nrawfile,fixture\n"
        self.check_record(before, after)

    def test_multiline_asset_cannot_supply_authored_origin(self):
        before = b'>origin,G:unproven\nrawfile,"fixture\n>origin,new\nsuffix"\n'
        self.assertEqual(self.parse(before.decode()).get("origin"), "G:unproven")
        self.check_refusal(before)

    def test_multiline_asset_cannot_supply_origin_hash(self):
        before = b'>origin,G:unproven\nrawfile,"fixture\n>origin-md5,HISTORICAL\nsuffix"\n'
        self.assertIsNone(self.parse(before.decode()).get("origin-md5"))
        self.check_refusal(before)

    def test_real_origin_after_embedded_literal_is_used(self):
        prefix = b'rawfile,"fixture\n>origin,new\nsuffix"\n'
        before = prefix + b">origin,new\n"
        self.assertEqual(self.parse(before.decode()).get("origin"), "new")
        self.check_record(before, prefix + b">origin,new\n>build-md5," + self.digest + b"\n")

    def test_quoted_existing_build_hash_is_replaced_effectively(self):
        before = b'>origin,new\n">build-md5","OLD"\nrawfile,fixture\n'
        after = b">origin,new\n>build-md5," + self.digest + b"\nrawfile,fixture\n"
        self.check_record(before, after)

    def test_duplicate_build_hash_settings_refuse_without_write(self):
        self.check_refusal(b'>origin,new\n>build-md5,FIRST\n">build-md5",LAST\n')

    def test_duplicate_origin_hash_settings_refuse_without_write(self):
        self.check_refusal(b'>origin,G:preserved\n>origin-md5,FIRST\n">origin-md5",LAST\n')

    def test_mixed_line_endings_refuse_without_write(self):
        self.check_refusal(b">origin,new\r\nrawfile,fixture\n")

    def test_commented_origin_does_not_create_ambiguity(self):
        before = b"// >origin,G:unproven\n>origin,new\nrawfile,fixture\n"
        after = before.replace(b">origin,new\n", b">origin,new\n>build-md5," + self.digest + b"\n")
        self.check_record(before, after)

    def test_multiline_metadata_refuses_without_write(self):
        self.check_refusal(b'>origin,new\n">build-md5","OLD\nVALUE"\n')

    def test_empty_or_malformed_origin_hash_is_not_authority(self):
        for pin in (b">origin-md5", b">origin-md5,", b">origin-md5,NOT_A_HASH",
                    b">origin-md5,0123456789abcdef0123456789abcdef,extra"):
            with self.subTest(pin=pin):
                self.check_refusal(b">origin,G:unproven\n" + pin + b"\nrawfile,fixture\n")

    def test_embedded_valid_hash_is_not_origin_authority(self):
        before = b'>origin,G:unproven\nrawfile,"fixture\n>origin-md5,0123456789abcdef0123456789abcdef\nsuffix"\n'
        self.assertIsNone(self.parse(before.decode()).get("origin-md5"))
        self.check_refusal(before)


if __name__ == "__main__":
    unittest.main()
