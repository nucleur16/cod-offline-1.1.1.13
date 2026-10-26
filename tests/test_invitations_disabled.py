"""Temporary invitation hold and original compiled-class metadata protection."""
from pathlib import Path
import hashlib,importlib.util,json,shutil,struct,unittest
ROOT=Path(__file__).resolve().parents[1]
DOC=ROOT/'docs/baselines/20261024-invitations-disabled'

def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
F=module('invitation_hold_behavior',DOC/'behavior_fixture.py')
A=module('invitation_hold_abc',DOC/'abc_reader.py')

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

    def test_original_metadata_and_every_other_compiled_body_remain_exact(self):
        patch=ROOT/'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch'
        for row in json.loads((DOC/'abi-contract.json').read_text(encoding='utf-8')):
            with self.subTest(cls=row['class_name']):
                tag=(patch/row['tag_relative']).read_bytes()
                header=6 if struct.unpack_from('<H',tag)[0]&63==63 else 2
                self.assertEqual(struct.unpack_from('<H',tag)[0]>>6,82)
                abc=A.read(tag[tag.index(b'\0',header+4)+1:])
                self.assertEqual(hashlib.sha256(abc['tables']).hexdigest(),row['tables_sha256'])
                self.assertEqual(len(abc['bodies']),row['method_count'])
                for index,pin in row['unchanged_bodies'].items():
                    self.assertEqual(hashlib.sha256(abc['bodies'][int(index)]['raw']).hexdigest(),pin)

if __name__=='__main__':unittest.main()
