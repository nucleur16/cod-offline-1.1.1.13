"""Portable language-scoped ownership; actual checker block, no game/FF inputs."""
import ast
import collections
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tests"))
import check_zone_packs as checker
from zonekit import codecs


def ownership_block(old, new, source=None):
    """Execute actual rule3 statements with owned asset tuples and real locale selection."""
    if source is None:
        source = (ROOT / 'tests/check_zone_packs.py').read_bytes()
    main = next(n for n in ast.parse(source).body if isinstance(n, ast.FunctionDef) and n.name == 'main')
    start = next(i for i, n in enumerate(main.body) if isinstance(n, ast.Assign)
                 and any(isinstance(t, ast.Name) and t.id == 'where' for t in n.targets))
    stop = next(i for i in range(start, len(main.body)) if isinstance(main.body[i], ast.Expr)
                and isinstance(main.body[i].value, ast.Call) and isinstance(main.body[i].value.func, ast.Name)
                and main.body[i].value.func.id == 'print')
    used = {lang: {v[2] for v in checker.effective(checker.load_order(list(old), lang), old).values()}
            for lang in checker.LANGS}
    env = dict(checker.__dict__, old=old, new=new, used_by_language=used,
               used_somewhere=set().union(*used.values()), problems=[])
    exec(compile(ast.Module(body=main.body[start:stop], type_ignores=[]),
                 '<actual-rule3-owned-assets>', 'exec'), env)
    return env['problems'], env['shadowed']


def asset(key='K33UI_CODO_3905', value=b''):
    return (27, key, codecs.encode_localize(key, value))


class LocaleOwnership(unittest.TestCase):
    def test_present_empty_keys_have_one_owner_per_selected_language(self):
        for key in ('K33UI_CODO_3905', 'K33UI_CODO_4288'):
            entry = asset(key)
            self.assertEqual(codecs.decode_localize(entry[2]), (key, b''))
            self.assertEqual(len(entry[2]), 25)
            old = {'englishui': [entry], 'chineseui': [entry]}
            new = {'english_locale': [entry], 'chinese_locale': [entry]}
            self.assertEqual(ownership_block(old, new), ([], []))

    def test_nonempty_bytes_also_obey_language_selection(self):
        entry = asset('NORMAL_SHARED_TEXT', b'identical nonempty')
        old = {'englishui': [entry], 'chineseui': [entry]}
        new = {'english_locale': [entry], 'chinese_locale': [entry]}
        self.assertEqual(ownership_block(old, new), ([], []))

    def test_same_locale_duplicate_is_refused_for_each_effective_language(self):
        entry = asset()
        errors, _ = ownership_block({'chineseui': [entry]},
                                   {'chinese_locale': [entry], 'chinese_extra': [entry]})
        self.assertEqual(len(errors), 2)
        self.assertTrue(any(e.startswith('zh:') and '2 eligible' in e for e in errors))
        self.assertTrue(any(e.startswith('native:') and '2 eligible' in e for e in errors))

    def test_missing_owner_is_refused(self):
        errors, _ = ownership_block({'englishui': [asset()]}, {})
        self.assertEqual(len(errors), 1)
        self.assertIn('en:', errors[0])
        self.assertIn('0 eligible', errors[0])

    def test_wrong_language_owner_is_not_substitute(self):
        errors, _ = ownership_block({'englishui': [asset()]}, {'chinese_locale': [asset()]})
        self.assertEqual(len(errors), 1)
        self.assertIn('en:', errors[0])
        self.assertIn('0 eligible', errors[0])

    def test_empty_key_cannot_be_dropped_or_replaced_with_different_key(self):
        errors, _ = ownership_block({'englishui': [asset()]},
                                   {'english_locale': [asset('K33UI_CODO_4288')]})
        self.assertEqual(len(errors), 1)
        self.assertIn('K33UI_CODO_3905', errors[0])

    def test_shared_pack_duplicate_with_language_pack_still_refused(self):
        errors, _ = ownership_block({'shared_old': [asset()]},
                                   {'shared_new': [asset()], 'english_locale': [asset()]})
        self.assertEqual(len(errors), 1)
        self.assertIn('en:', errors[0])
        self.assertIn('2 eligible', errors[0])

    def test_never_used_blob_reintroduction_still_refused(self):
        first = asset(value=b'first winner')
        shadowed = asset(value=b'never selected')
        errors, hidden = ownership_block({'a_old': [first], 'z_old': [shadowed]},
                                         {'new': [first, shadowed]})
        self.assertEqual(len(errors), 1)
        self.assertIn('was never used', errors[0])
        self.assertEqual(hidden, [('z_old', 'localize', 'K33UI_CODO_3905')])


if __name__ == '__main__':
    unittest.main()
