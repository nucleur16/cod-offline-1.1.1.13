"""Check the authored native kiosk graphs and their original purchase contracts."""
from pathlib import Path
import copy
import importlib.util
import json
import struct
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('kiosk_builder',ROOT/'tools/build_survival_kiosks.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)

class SurvivalKioskMenus(unittest.TestCase):
    def test_wallet_prices_and_existing_purchase_handlers(self):
        for name,rows in [('kiosk_locker',[0,1,2,3,4]),('kiosk_killstreak',[10,11,13,14])]:
            with self.subTest(menu=name):
                original=b.original(name)
                authored=b.author(original)
                old=original['p']['8'][0];new=authored['p']['8'][0]
                self.assertEqual(len(new['p']['284']),len(old['p']['284'])+1)
                self.assertEqual(struct.unpack_from('<I',bytes.fromhex(new['raw']),0xb0)[0],len(new['p']['284']))
                balance=new['p']['284'][-1]
                self.assertEqual(balance['p']['0'],'survival_credit_balance')
                self.assertIn('surHUD_credits',json.dumps(balance))
                self.assertIn('@SO_SURVIVAL_CREDITS',json.dumps(balance))
                for index in rows:self.assertEqual(json.dumps(authored).count('surHUD_cost_'+str(index)+'"'),2)
                # All original actions/mouse-focus events/visibility and owned/perk gates remain intact.
                for before,after in zip(old['p']['284'],new['p']['284']):
                    expected=copy.deepcopy(before);actual=copy.deepcopy(after)
                    if 'surHUD_cost_' in json.dumps(after):
                        expected['p'].pop('332');actual['p'].pop('332')
                    for graph in (expected,actual):
                        for node in b.traverse(graph):
                            if node.get('asset')=='material' and node.get('name')==',navbar_selection_bar_focus':node['name']=',gradient_fadein'
                    self.assertEqual(expected,actual)
                blob=b.codec.encode_menulist(authored)
                written=b.menu_source.load(b.ZONE/'menufile'/(name+'.menu'),b.asset(name))
                self.assertEqual(blob,b.codec.encode_menulist(written))
                self.assertNotIn('SO_SURVIVAL_NUM_CARDS',json.dumps(authored))
                self.assertTrue((b.ZONE/'localize/SO_SURVIVAL_CREDITS.bin').is_file())
if __name__=='__main__':unittest.main()
