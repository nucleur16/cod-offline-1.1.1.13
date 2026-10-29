"""Validate mounted wave overrides without requiring private game assets."""
from pathlib import Path
import csv,io,sys,unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from zonekit import codecs,manifest
class SurvivalWaveOverrides(unittest.TestCase):
    def test_existing_menus_and_localization_remain_with_full_wave_tables(self):
        data=manifest.read((ROOT/'zone_source/patch_common_gametype_survival.csv').read_text())
        self.assertEqual(sum(t=='menufile' for t,n,p in data.assets),2)
        self.assertEqual(sum(t=='localize' for t,n,p in data.assets),13)
        self.assertTrue({'SO_SURVIVAL_ENEMIES_LEFT','SO_SURVIVAL_ENEMY_LEFT'} <= {n for t,n,p in data.assets if t=='localize'})
        tables=[(n,p) for t,n,p in data.assets if t=='stringtable']
        self.assertEqual(len(tables),11)
        for name,params in tables:
            with self.subTest(table=name):
                text=(ROOT/'zones/patch_common_gametype_survival/stringtable'/name).read_text()
                rows=codecs.read_csv(text,'ascii')
                self.assertEqual(len(rows),20)
                self.assertEqual([int(r[2]) for r in rows],list(range(1,21)))
                self.assertEqual([int(r[2]) for r in rows if r[9]==b'1'],[17,18,19,20])
                self.assertIn(b'chemical',rows[6][5].split())
                self.assertIn(b'hazmat',rows[13][5].split())
                self.assertIn(b'hazmat',rows[17][5].split())
                decoded_name,decoded_rows,_=codecs.decode_stringtable(codecs.encode_stringtable(name,rows,params))
                self.assertEqual((decoded_name,decoded_rows),(name,rows))
if __name__=='__main__':unittest.main()
