"""Temporary invitation hold and original compiled-class metadata protection."""
from pathlib import Path
import hashlib,importlib.util,json,shutil,struct,unittest,sys,zlib
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'docs/baselines/20261024-invitations-disabled'))
ROOT=Path(__file__).resolve().parents[1]
DOC=ROOT/'docs/baselines/20261024-invitations-disabled'

def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
F=module('invitation_hold_behavior',DOC/'behavior_fixture.py')
A=module('invitation_hold_abc',DOC/'abc_reader.py')
R=module('invitation_recompile_review',DOC/'recompile_review.py')
I=module('invitation_integration_review',DOC/'integration_review.py')

class InvitationsDisabled(unittest.TestCase):
    def test_actual_shared_source_behaviors_and_unrelated_social_paths(self):
        self.assertIsNotNone(shutil.which('node'),'Node is required for actual AS behavior checks')
        result=F.execute(ROOT)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout)['checks'],168)

    def test_removing_disable_gate_is_rejected_by_actual_behavior_fixture(self):
        result=F.execute(ROOT,mutation=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('disabled invitation gate removed',result.stderr)

    def test_metadata_and_protected_compiled_bodies_match_reviewed_baseline(self):
        patch=ROOT/'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch'
        integration,_=I.snapshots(DOC)
        for row in json.loads((DOC/'abi-contract.json').read_text(encoding='utf-8')):
            with self.subTest(cls=row['class_name']):
                tag=(patch/row['tag_relative']).read_bytes()
                header=6 if struct.unpack_from('<H',tag)[0]&63==63 else 2
                self.assertEqual(struct.unpack_from('<H',tag)[0]>>6,82)
                abc=A.read(tag[tag.index(b'\0',header+4)+1:])
                self.assertEqual(hashlib.sha256(abc['tables']).hexdigest(),row['tables_sha256'])
                self.assertEqual(len(abc['bodies']),row['method_count'])
                for index,pin in row['unchanged_bodies'].items():
                    if row['class_name']==integration['class_name']:
                        pin=integration['protected_body_updates'].get(index,{}).get('after_sha256',pin)
                    self.assertEqual(hashlib.sha256(abc['bodies'][int(index)]['raw']).hexdigest(),pin)

    def test_reviewed_recompile_preserves_original_pins_and_resolved_semantics(self):
        review,artifacts=R.snapshots(DOC)
        row=next(r for r in json.loads((DOC/'abi-contract.json').read_text()) if r['class_name']==review['class_name'])
        before=artifacts['MatchController.before-20261028.abc.zlib']
        patch=ROOT/'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch'
        tag=(patch/row['tag_relative']).read_bytes()
        header=6 if struct.unpack_from('<H',tag)[0]&63==63 else 2
        # The original review remains immutable; integration is a separate,
        # independently verified transition from this exact historical ABC.
        after=zlib.decompress((DOC/'MatchController.after-20261028.abc.zlib').read_bytes())
        self.assertEqual(hashlib.sha256(before).hexdigest(),review['before_abc_sha256'])
        self.assertEqual(hashlib.sha256(after).hexdigest(),review['after_abc_sha256'])
        old,new=A.read(before),A.read(after)
        self.assertEqual(old['tables'],new['tables'])
        self.assertEqual(old['bodies'].keys(),new['bodies'].keys())
        updates={int(u['index']):u for u in row['reviewed_body_updates_20261028']}
        changed={i for i,b in old['bodies'].items() if b['raw']!=new['bodies'][i]['raw']}
        self.assertEqual(changed,set(updates))
        self.assertEqual(len(updates),review['raw_body_updates'])
        self.assertEqual(len(row['unchanged_bodies']),review['protected_body_count'])
        for index,pin in row['unchanged_bodies'].items():
            i=int(index)
            prior=updates[i]['before_sha256'] if i in updates else pin
            self.assertEqual(hashlib.sha256(old['bodies'][i]['raw']).hexdigest(),prior)
            self.assertEqual(old['bodies'][i]['name'],new['bodies'][i]['name'])
            if i in updates:
                self.assertEqual(updates[i]['method'],old['bodies'][i]['name'])
                self.assertEqual(updates[i]['after_sha256'],pin)
        result=R.execute(DOC)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout)['checks'],99)

    def test_production_main_integration_preserves_invitation_abi_and_review_chain(self):
        review,artifacts=I.snapshots(DOC)
        before=artifacts['MatchController.before-integration-20261029.abc.zlib']
        after=artifacts['MatchController.after-integration-20261029.abc.zlib']
        historical,_=R.snapshots(DOC)
        self.assertEqual(hashlib.sha256(before).hexdigest(),historical['after_abc_sha256'])
        row=next(r for r in json.loads((DOC/'abi-contract.json').read_text()) if r['class_name']==review['class_name'])
        tag=(ROOT/'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch'/row['tag_relative']).read_bytes()
        header=6 if struct.unpack_from('<H',tag)[0]&63==63 else 2
        self.assertEqual(tag[tag.index(b'\0',header+4)+1:],after)
        old,new=A.read(before),A.read(after)
        self.assertEqual(old['tables'],new['tables'])
        self.assertEqual(old['bodies'].keys(),new['bodies'].keys())
        changed={str(i) for i in old['bodies'] if old['bodies'][i]['raw']!=new['bodies'][i]['raw'] and str(i) in row['unchanged_bodies']}
        self.assertEqual(changed,set(review['protected_body_updates']))
        for index,pin in row['unchanged_bodies'].items():
            self.assertEqual(hashlib.sha256(old['bodies'][int(index)]['raw']).hexdigest(),pin)
            if index in changed:
                update=review['protected_body_updates'][index]
                self.assertEqual(update['before_sha256'],pin)
                self.assertEqual(update['method'],old['bodies'][int(index)]['name'])
                self.assertEqual(update['after_sha256'],hashlib.sha256(new['bodies'][int(index)]['raw']).hexdigest())
        result=I.execute(DOC)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout)['checks'],22)

    def test_integration_review_rejects_unrelated_change(self):
        _,a=I.snapshots(DOC)
        before=a['MatchController.before-integration-20261029.as.zlib'].decode()
        after=a['MatchController.after-integration-20261029.as.zlib'].decode()
        with self.assertRaisesRegex(ValueError,'unreviewed resolved'):
            I.resolved_changes(before,after.replace('factor = 1;','factor = 2;',1))

    def test_recompile_review_rejects_an_unrelated_semantic_change(self):
        _,artifacts=R.snapshots(DOC)
        before=artifacts['MatchController.before-20261028.as.zlib'].decode()
        after=artifacts['MatchController.after-20261028.as.zlib'].decode()
        self.assertTrue(R.resolved_equivalence(before,after))
        with self.assertRaisesRegex(ValueError,'unreviewed resolved'):
            R.resolved_equivalence(before,after.replace('factor = 1;','factor = 2;',1))
        with self.assertRaisesRegex(ValueError,'Survival default'):
            R.resolved_equivalence(before,after.replace('coop_survival_mp_fav_tropical','mp_unavailable',1))

if __name__=='__main__':unittest.main()
