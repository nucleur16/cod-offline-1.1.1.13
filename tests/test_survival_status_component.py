import importlib.util,json,pathlib,struct,sys,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from zonekit import menus
spec=importlib.util.spec_from_file_location('status',ROOT/'tools/build_survival_status.py');M=importlib.util.module_from_spec(spec);spec.loader.exec_module(M)
class SurvivalStatus(unittest.TestCase):
 def setUp(self):self.source=json.loads(M.SOURCE.read_text());self.result=M.author(self.source);self.items=self.result['p']['8'][0]['p']['284']
 def test_authored_geometry_fonts_and_colors_preserved(self):
  for i in range(4):
   before=bytes.fromhex(self.source['p']['8'][0]['p']['288'][i]['raw']);after=bytes.fromhex(self.items[i]['raw'])
   self.assertEqual(before[4:8],after[4:8]);self.assertEqual(before[12:0xa4],after[12:0xa4]);self.assertEqual(before[0xd4:0xd8],after[0xd0:0xd4]);self.assertEqual(before[0xdc:0xec],after[0xd8:0xe8])
   self.assertAlmostEqual(struct.unpack_from('<f',after,0xd4)[0],0.24 if i==3 else struct.unpack_from('<f',before,0xd8)[0],places=6)
   self.assertEqual(struct.unpack_from('<f',after,8)[0],-30 if i==3 else struct.unpack_from('<f',before,8)[0])
 def test_enemy_and_credit_rows_have_font_height_clearance(self):
  wallet=bytes.fromhex(self.items[3]['raw']);wy=struct.unpack_from('<f',wallet,8)[0];ws=struct.unpack_from('<f',wallet,0xd4)[0]
  # Conservative64-unit font envelope plus4units for style7 outline/shadow.
  # All three rows share the same vertical screen anchor, so clearance scales
  # with resolution rather than changing at aspect-ratio boundaries.
  for index in (1,2):
   enemy=bytes.fromhex(self.items[index]['raw']);ey=struct.unpack_from('<f',enemy,8)[0];es=struct.unpack_from('<f',enemy,0xd4)[0]
   self.assertAlmostEqual(ws,es,places=6)
   for height in (480,720,768,900,1080,1440,2160):
    factor=height/480
    clearance=((wy-ey)-64*max(ws,es)-4)*factor
    self.assertGreater(clearance,8*factor)
   self.assertLess(wy,0,'credit baseline stays above the bottom screen anchor')
 def test_only_existing_c1_operators_and_real_dvars(self):
  def walk(v):
   if isinstance(v,dict):
    if v.get('kind')=='expressionEntry':
     kind,op=struct.unpack_from('<II',bytes.fromhex(v['raw']))
     if kind==0:self.assertLess(op,len(menus.OPS_1113))
    for x in v.values():walk(x)
   elif isinstance(v,list):
    for x in v:walk(x)
  walk(self.result);text=menus.listing(self.result)
  for s in ['surHUD_enemy','surHUD_credits','survival','SO_SURVIVAL_CREDITS']:self.assertIn(s,text)
  self.assertNotIn('ownerdraw 298',text);self.assertNotIn('cards_deck',text);self.assertEqual(self.items[0]['p']['160']['name'],',white')
 def test_encoded_component_is_reproducible(self):
  self.assertEqual(menus.encode_menulist(self.result),(M.OUT/'survival_status.bin').read_bytes());self.assertEqual(len(self.items),4)
  for i in [1,2]:self.assertIn('SO_SURVIVAL_ENEM',json.dumps(self.items[i]))
if __name__=='__main__':unittest.main()
