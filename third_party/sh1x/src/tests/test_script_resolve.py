"""Focused checks for the production and developer GSC resolution modes."""

import importlib.util
import re
from pathlib import Path
import subprocess
import sys
import unittest


REPO = Path(__file__).resolve().parents[1]
TOOL = REPO / "tools" / "script_resolve.py"
spec = importlib.util.spec_from_file_location("script_resolve", TOOL)
resolver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(resolver)


class ScriptResolverTests(unittest.TestCase):
    def test_escaped_quotes_stay_inside_gsc_strings(self):
        source = r'''maperrors[0] = "flag_descriptor \"flag_descriptor (or)\""; fileprint_launcher( "\tVisionSetNaked( \"" + value + "\", 0 );" ); realCall();'''
        masked = resolver.strip_strings(source)
        calls = [match.group(1).lower() for match in resolver.CALL.finditer(masked)]
        self.assertEqual(calls, ['fileprint_launcher', 'realcall'])

    def test_developer_delimiters(self):
        source = 'main()\n{\n  live();\n  /# hidden();\n  hidden_again(); #/\n}\n'
        masked, errors = resolver.scan_developer_blocks(source)
        self.assertEqual(errors, [])
        self.assertIn('live();', masked)
        self.assertNotIn('hidden', masked)
        self.assertEqual(source.count('\n'), masked.count('\n'))
        enabled, errors = resolver.scan_developer_blocks(source, enabled=True)
        self.assertEqual(errors, [])
        self.assertIn('hidden_again();', enabled)

        for malformed, expected in (
            ('/# unfinished', '/# has no matching #/'),
            ('#/ orphan', '#/ has no matching /#'),
            ('/# /# nested #/ #/', 'cannot recurse /#'),
        ):
            with self.subTest(malformed=malformed):
                self.assertTrue(any(expected in error for error in
                                    resolver.scan_developer_blocks(malformed)[1]))

    def test_killhouse_production_and_developer_modes(self):
        command = [sys.executable, str(TOOL), '--quiet',
                   '--seed', 'maps/mp/gametypes/war.gsc',
                   '--seed', 'maps/mp/mp_killhouse_sh.gsc']
        production = subprocess.run(command, cwd=REPO, text=True,
                                    capture_output=True, check=False)
        self.assertEqual(production.returncode, 0, production.stdout)

        developer = subprocess.run(command + ['--developer-script'], cwd=REPO,
                                   text=True, capture_output=True, check=False)
        self.assertEqual(developer.returncode, 1, developer.stdout)
        self.assertIn('shouldrunserversideeffects', developer.stdout)

    def test_all_mp_gametypes_and_map_entry_scripts(self):
        modes = sorted(path for path in
                       (REPO / 'bin/home/codol_rawfiles/maps/mp/gametypes').glob('*.gsc')
                       if not path.stem.startswith('_') and
                       path.stem not in ('coop', 'zm'))
        maps = sorted(path for path in
                      (REPO / 'bin/home/codol_rawfiles/maps/mp').glob('mp_*.gsc')
                      if not path.stem.endswith(('_fx', '_precache')))
        self.assertGreaterEqual(len(modes), 14)
        self.assertGreaterEqual(len(maps), 37)
        command = [sys.executable, str(TOOL), '--quiet']
        for path in modes:
            command.extend(('--seed', 'maps/mp/gametypes/' + path.name))
        for path in maps:
            command.extend(('--seed', 'maps/mp/' + path.name))
        result = subprocess.run(command, cwd=REPO, text=True,
                                capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_coop_and_zombies_map_entry_scripts(self):
        rawfiles = REPO / 'bin/home/codol_rawfiles'
        for category, pattern, minimum in (('coop', ('coop_*.gsc', 'mp_*.gsc'), 14),
                                           ('zm', ('zm_*.gsc',), 3)):
            with self.subTest(category=category):
                maps = sorted(path for glob in pattern
                              for path in (rawfiles / 'maps').glob(glob))
                self.assertGreaterEqual(len(maps), minimum)
                command = [sys.executable, str(TOOL), '--quiet', '--list-reachable',
                           '--category', category]
                for path in maps:
                    command.extend(('--map', path.stem))
                result = subprocess.run(command, cwd=REPO, text=True,
                                        capture_output=True, check=False)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertNotIn('aitype/enemy_dog_mp.gsc', result.stdout)
                self.assertNotIn('maps/mp/gametypes/_callbacksetup.gsc', result.stdout)
                if category == 'zm':
                    self.assertIn('aitype/enemy_zombie_zm.gsc', result.stdout)
                    self.assertIn('animscripts/zombie_jump.gsc', result.stdout)
                    self.assertIn('maps/_zombiemode_gametype.gsc', result.stdout)
                else:
                    self.assertNotIn('aitype/enemy_zombie_zm.gsc', result.stdout)
                    self.assertIn('animscripts/soldier_jump.gsc', result.stdout)
                    self.assertIn('maps/_coop_gametype.gsc', result.stdout)

    def test_retired_category_overlays_stay_empty(self):
        rawfiles = REPO / 'bin/home/codol_rawfiles'
        for overlay in resolver.RETIRED_OVERLAYS:
            self.assertEqual(sorted((rawfiles / overlay).rglob('*.gsc')), [],
                             '%s/ is a retired overlay; see docs/GSC_TREE_LAYOUT.md'
                             % overlay)

    def test_entry_points_mirror_the_native_table(self):
        source = (REPO / 'src/online/script/script_source.c').read_text()
        rows = re.findall(r'\{\s*"(\w+)",\s*(NULL|"[^"]*"),\s*(NULL|"[^"]*"),\s*'
                          r'(NULL|"[^"]*"),\s*(NULL|"[^"]*"),\s*"([^"]*)",\s*"([^"]*)",\s*'
                          r'(NULL|"[^"]*")\s*\}',
                          source)
        native = {row[0]: {'rules': None if row[2] == 'NULL' else row[2].strip('"'),
                           'callbacks': row[5], 'level': row[6],
                           'suffix': '' if row[7] == 'NULL' else row[7].strip('"')} for row in rows}
        self.assertEqual(native, resolver.ENTRY_POINTS)


if __name__ == '__main__':
    unittest.main()
