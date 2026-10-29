import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('survival_hud', ROOT/'tools/build_survival_hud.py')
hud = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hud)

class SurvivalHud(unittest.TestCase):
    def test_only_mp_score_and_killstreak_visibility_changes(self):
        original = json.loads((hud.DIR/'survival_hud_visibility.original.json').read_text())
        result = hud.author(original)
        changed = [i for i,(a,b) in enumerate(zip(original['p']['8'],result['p']['8'])) if a != b]
        self.assertEqual(len(changed), 2)
        self.assertEqual({result['p']['8'][i]['p']['0'] for i in changed}, {'killstreaks_hd','scorebar_hd'})
        restored = copy.deepcopy(result)
        for i in changed:
            menu = result['p']['8'][i]
            stock = original['p']['8'][i]
            if menu['p']['0'] == 'killstreaks_hd':
                replacement = menu['p']['228']
                before = stock['p']['228']
                self.assertEqual(replacement['p']['4'][8:-1], before['p']['4'])
                self.assertEqual(replacement['p']['4'][2]['p']['8'], 'g_gametype')
                self.assertEqual(replacement['p']['4'][5]['p']['8'], 'survival')
                restored['p']['8'][i]['p']['228'] = before
            else:
                self.assertEqual(menu['p']['228'], stock['p']['228'])
                count = len(stock['p']['284'])
                component = json.loads((hud.DIR.parent/'components/survival_status.json').read_text())
                self.assertEqual(menu['p']['284'][count:], component['p']['8'][0]['p']['284'])
                for j, before in enumerate(stock['p']['284']):
                    replacement = menu['p']['284'][j]['p']['324']
                    self.assertEqual(replacement['p']['4'][2]['p']['8'],'g_gametype')
                    self.assertEqual(replacement['p']['4'][5]['p']['8'],'survival')
                    if '324' in before['p']:
                        self.assertEqual(replacement['p']['4'][8:-1],before['p']['324']['p']['4'])
                        restored['p']['8'][i]['p']['284'][j]['p']['324'] = before['p']['324']
                    else:
                        restored['p']['8'][i]['p']['284'][j]['p'].pop('324')
                restored['p']['8'][i]['p']['284'] = restored['p']['8'][i]['p']['284'][:count]
                restored['p']['8'][i]['raw'] = stock['raw']
        self.assertEqual(restored, original)
        self.assertEqual((hud.DIR/'survival_hud_visibility.bin').read_bytes(), hud.menus.encode_menulist(result))

if __name__ == '__main__':
    unittest.main()
