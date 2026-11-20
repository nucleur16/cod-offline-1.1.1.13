"""Keep authored wave budgets/bosses while adding requested late gas encounters."""
from pathlib import Path
import csv,io,json,zlib,unittest
ROOT=Path(__file__).resolve().parents[1]
RAW=ROOT/'bin/home/codol_rawfiles'
class SurvivalProgression(unittest.TestCase):
    def test_all_authored_rows_and_counts_survive_conservative_type_swaps(self):
        original=json.loads(zlib.decompress((ROOT/'tests/fixtures/survival_authored_waves_20261029.json.zlib').read_bytes()))
        changed=0
        for name,text in original.items():
            before=list(csv.reader(io.StringIO(text)))
            after=list(csv.reader(io.StringIO((RAW/'mp'/name).read_text())))
            with self.subTest(table=name):
                self.assertEqual(len(after),20)
                self.assertEqual([int(r[2]) for r in after],list(range(1,21)))
                gas_was_authored=any({'chemical','hazmat'}&set(r[5].split()) for r in before)
                for old,new in zip(before,after):
                    wave=int(old[2])
                    # Two source exports lost the repeat-metadata column;
                    # retain their original nine cells and repair only that.
                    if name in ('survival_waves_hardhat_sh.csv','survival_waves_terminal_cls.csv'):
                        self.assertEqual(len(old),9)
                        old=old+['1' if wave>=17 else '']
                    if gas_was_authored or wave not in (7,14,18):
                        self.assertEqual(old,new)
                    else:
                        self.assertEqual(old[:5]+old[6:],new[:5]+new[6:])
                        a,b=old[5].split(),new[5].split()
                        self.assertEqual(len(a),len(b))
                        self.assertEqual(sum(x!=y for x,y in zip(a,b)),1)
                        self.assertIn('chemical' if wave==7 else 'hazmat',b)
                        self.assertEqual([x for x in a if 'dog' in x],[x for x in b if 'dog' in x])
                        changed+=1
                self.assertEqual(after[0][3],'easy')
                self.assertEqual(after[-1][3],'elite')
                self.assertEqual([int(r[2]) for r in after if len(r)>9 and r[9]=='1'],[17,18,19,20])
                self.assertTrue(any('martyrdom' in r[5].split() for r in after))
                self.assertTrue(any('dog_reg' in r[5].split() for r in after))
                self.assertTrue(any('dog_splode' in r[5].split() for r in after))
                for r in after:self.assertEqual(len(r[5].split()),len(r[6].split()))
        self.assertEqual(changed,33)

    def test_real_ai_type_weapons_and_stronger_health_are_authored(self):
        rows={r[1]:r for r in csv.reader(io.StringIO((RAW/'mp/survival_waves.csv').read_text())) if r and r[0].isdigit() and 100<=int(r[0])<200}
        order=['easy','regular','hardened','veteran','elite']
        self.assertEqual([rows[n][5] for n in order],['model1887_mp','mp5k_mp','scar_mp','ak104_mp','fn2000_mp'])
        self.assertEqual([int(rows[n][7]) for n in order],[25,50,75,100,125])
        self.assertEqual(rows['easy'][6],'')
        self.assertEqual(rows['chemical'][6],'tabun_gas_mp')
        self.assertEqual(rows['hazmat'][6],'tabun_gas_mp')
        self.assertEqual(rows['dog_reg'][4],'') # native dog species, not a disguised soldier

if __name__=='__main__':unittest.main()
