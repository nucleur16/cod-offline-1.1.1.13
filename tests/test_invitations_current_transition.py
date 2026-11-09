"""Pure DATA/source guards; no Node, AVM2, FFDec or game invocation."""
from pathlib import Path
import copy,importlib.util,json,unittest,zlib
ROOT=Path(__file__).resolve().parents[1]
P=ROOT/'docs/baselines/20261105-invitations-current-transition/current_transition.py'
s=importlib.util.spec_from_file_location('current_invitation_gate',P);C=importlib.util.module_from_spec(s);s.loader.exec_module(C)

class CurrentInvitationTransition(unittest.TestCase):
    def setUp(self):
        self.tag=(ROOT/C.PATCH/'tags/6167_82_codo_controllers_MatchController.bin').read_bytes()
        self.old=C.A.read(C.historical_parent());self.new=C.A.read(C.extract(self.tag))
        self.source=(ROOT/C.PATCH/'as/codo/controllers/MatchController.as').read_bytes()
        self.resolved=zlib.decompress((C.DOC/'MatchController.current.as.zlib').read_bytes())
        self.code=json.loads((C.DOC/'transition.json').read_bytes())['invitation_denial_code_hex']
    def test_exact_current_compiler_source_and_immutable_historical_chain(self):
        result=C.verify_current(ROOT,self.tag)
        self.assertTrue(result['protected_invitation_noop']);self.assertTrue(result['existing_public_protected_ABI_exact'])
        self.assertEqual(result['old_methods'],165);self.assertEqual(result['current_methods'],166)
    def test_current_tag_or_source_drift_refuses(self):
        with self.assertRaisesRegex(ValueError,'unreviewed factor successor supplied'):C.verify_current(ROOT,self.tag[:-1]+bytes([self.tag[-1]^1]))
        with self.assertRaisesRegex(ValueError,'factor source producer differs'):C.verify_current(ROOT,self.tag,source=self.source+b'\n// unreviewed source')
    def test_invitation_code_mutation_is_rejected_without_hash_shortcut(self):
        altered=copy.deepcopy(self.new);body=next(b for b in altered['bodies'].values()if b['name'].endswith('/onProposeRoomMembershipHandle'));body['code']=b'\x47'
        with self.assertRaisesRegex(ValueError,'denial bytecode'):C.invitation_contract(self.old,altered,self.source,self.resolved,self.code)
    def test_invitation_source_enable_mutation_is_rejected_without_hash_shortcut(self):
        text=self.source.decode('utf-8');method=C.F.method(text,'onProposeRoomMembershipHandle');changed=text.replace(method['body'],'\n this.broadcast("ENABLED_INVITE");\n',1).encode('utf-8')
        with self.assertRaisesRegex(ValueError,'source/compiled semantics'):C.invitation_contract(self.old,self.new,changed,self.resolved,self.code)
    def test_public_abi_mutation_refuses(self):
        old=C.interface(C.historical_parent());new=C.interface(C.extract(self.tag));key=next(iter(new[0]));mutated=copy.deepcopy(new)
        member=next(k for k,v in mutated[0][key]['traits'].items()if k[1]is not None and k[1][0]!=5 and 'signature'in v)
        mutated[0][key]['traits'][member]['signature']['returns']='UNREVIEWED_TYPE'
        original=C.interface
        try:
            C.interface=lambda raw:old if raw==C.historical_parent()else mutated
            with self.assertRaisesRegex(ValueError,'public/protected'):C.verify_abi(C.historical_parent(),C.extract(self.tag))
        finally:C.interface=original
    def test_named_denial_method_cannot_disappear_or_move_to_unrelated_body(self):
        altered=copy.deepcopy(self.new);body=next(b for b in altered['bodies'].values()if b['name'].endswith('/onProposeRoomMembershipHandle'));body['name']='UNREVIEWED_RENAME'
        with self.assertRaisesRegex(ValueError,'denial bytecode'):C.invitation_contract(self.old,altered,self.source,self.resolved,self.code)

if __name__=='__main__':unittest.main()
