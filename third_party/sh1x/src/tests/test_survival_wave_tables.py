"""Actual Survival selector and no-missing-table-probe regressions for all advertised maps."""
import importlib.util
import json
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('wave_audit',ROOT/'tools/survival_wave_audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
class SurvivalWaveTables(unittest.TestCase):
    def test_every_authored_map_selects_a_real_packaged_twenty_wave_table(self):
        fixture=json.loads((ROOT/'tests/fixtures/survival_wave_startup_audit.json').read_text())
        results=audit.audit(ROOT/'bin/home/codol_rawfiles',set(fixture['stringtables']))
        self.assertEqual(len(results),14)
        self.assertTrue(all(entry['waves']==20 for entry in results))
        selected={entry['map']:entry['table'] for entry in results}
        self.assertEqual(selected['mp_qadeem_sh'],'mp/survival_waves_tier_1.csv')
        self.assertEqual(selected['mp_strike_sh'],'mp/survival_waves_tier_1.csv')
        for entry in results:
            if entry['map'] not in ('mp_qadeem_sh','mp_strike_sh'):
                self.assertEqual(entry['table'],'mp/survival_waves_'+entry['map'][3:]+'.csv')
    def test_audit_rejects_an_absent_tier_asset_instead_of_claiming_playable(self):
        fixture=json.loads((ROOT/'tests/fixtures/survival_wave_startup_audit.json').read_text())
        names=set(fixture['stringtables'])-{'mp/survival_waves_tier_1.csv'}
        with self.assertRaisesRegex(ValueError,'absent from actual Survival fastfile'):
            audit.audit(ROOT/'bin/home/codol_rawfiles',names)
if __name__=='__main__':unittest.main()
