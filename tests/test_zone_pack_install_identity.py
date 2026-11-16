"""Portable selection and script-string contracts; no game/key/FF/build input."""
import ast
import copy
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import check_zone_packs as checker
from zonekit import build, zoneimage


def actual_selection():
    tree = ast.parse((ROOT / "tests/check_zone_packs.py").read_text(encoding="utf-8"))
    main = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "main")
    value = next(n.value for n in main.body if isinstance(n, ast.Assign)
                 and any(isinstance(t, ast.Name) and t.id == "patch_zones" for t in n.targets))
    known = sorted(p.stem for p in (ROOT / "zone_source").glob("*.csv"))
    return eval(compile(ast.Expression(value), "<actual-checker-selection>", "eval"),
                dict(ROOT=str(ROOT), known=known, build=checker.build,
                     is_patch_install=checker.is_patch_install))


class InstallIdentity(unittest.TestCase):
    def test_actual_m1911_manifest_selected_with_either_valid_separator(self):
        original = build.load
        self.assertEqual(original(str(ROOT), "m1911_arms_diag").get("install"),
                         "client_shipRetail/patch")
        for install in ("client_shipRetail/patch", "client_shipRetail\\patch"):
            def alternate(repo, zone):
                manifest = original(repo, zone)
                if zone == "m1911_arms_diag":
                    manifest = copy.deepcopy(manifest)
                    manifest.settings["install"] = [install]
                return manifest
            with self.subTest(install=install), patch.object(checker.build, "load", side_effect=alternate):
                self.assertIn("m1911_arms_diag", actual_selection())

    def test_only_exact_patch_identity_is_admitted(self):
        for value in (None, "", "client_shipRetail/chinese", "/client_shipRetail/patch",
                      "client_shipRetail/patch/", "client_shipRetail/patch-extra",
                      "client_shipRetail/../patch", "client_shipRetail\\patch\\..\\chinese",
                      "CLIENT_SHIPRETAIL/patch", "other/client_shipRetail/patch"):
            with self.subTest(value=value):
                self.assertFalse(checker.is_patch_install(value))

    def test_actual_m1911_script_strings_and_binding_drift_are_visible(self):
        import json
        strings = json.loads((ROOT / "zones/m1911_arms_diag/script_strings.json").read_bytes())
        image = zoneimage.ZoneImage([0]*9, [0xffffffff if s is not None else 0 for s in strings],
                                   strings, len(strings), 0xffffffff, 0xffffffff)
        baseline = checker.script_string_identity(image)
        self.assertEqual(baseline, checker.script_string_identity(copy.deepcopy(image)))
        changed = copy.deepcopy(image)
        index = next(i for i, s in enumerate(changed.strings) if s is not None)
        changed.strings[index] += "_DRIFT"
        self.assertNotEqual(baseline, checker.script_string_identity(changed))
        changed = copy.deepcopy(image)
        changed.string_ptrs[index] = 0
        self.assertNotEqual(baseline, checker.script_string_identity(changed))
        changed = copy.deepcopy(image)
        changed.string_count_field -= 1
        self.assertNotEqual(baseline, checker.script_string_identity(changed))


if __name__ == "__main__":
    unittest.main()
