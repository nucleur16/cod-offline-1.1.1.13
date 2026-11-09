"""Decoded selector behavior and preservation; no AVM2, compiler or game."""
from pathlib import Path
import importlib.util,sys,unittest
ROOT=Path(__file__).resolve().parents[1]
D=ROOT/'tools/playlist-selector-producer';sys.path.insert(0,str(D))
import selector_producer as P,selector_export_contract as X

def execute(code,abc,choices):
    """Closed interpreter for this selector, injecting listPlayList's two arrays."""
    rows=P.decode(code);positions={z['label']:i for i,z in enumerate(rows)}
    locals_={0:'controller'};stack=[];pc=0;steps=0
    pop=lambda:stack.pop()
    while True:
        steps+=1;P.need(steps<10000,'finite test interpreter')
        z=rows[pc];op=z['op'];a=z['args'];pc+=1
        if op in(0xef,0x09):continue
        if op==0x30:pop()
        elif 0xd0<=op<=0xd3:stack.append(locals_.get(op-0xd0))
        elif 0xd4<=op<=0xd7:locals_[op-0xd4]=pop()
        elif op==0x62:stack.append(locals_.get(a[0]))
        elif op==0x63:locals_[a[0]]=pop()
        elif op==0x08:locals_.pop(a[0],None)
        elif op==0x24:stack.append(a[0])
        elif op==0x20:stack.append(None)
        elif op==0x26:stack.append(True)
        elif op==0x60:
            n=P.A.qname(abc,a[0]);P.need(n in('PlayListType','CombatType'),'closed global')
            stack.append({'COMBAT':2,'RANKED':1}if n=='PlayListType'else{'CT':0})
        elif op==0x66:
            name=P.A.qname(abc,a[0]);obj=pop();stack.append(obj[name])
        elif op in(0x80,0x82):pass
        elif op==0x56:P.need(a==[0],'empty list literal');stack.append([])
        elif op==0xab:right=pop();left=pop();stack.append(left==right)
        elif op==0x96:stack.append(not pop())
        elif op==0x2a:stack.append(stack[-1])
        elif op==0x29:pop()
        elif op in(0x46,0x4f):
            args=[pop()for _ in range(a[1])][::-1];obj=pop();name=P.A.qname(abc,a[0])
            if name=='listPlayList':
                P.need(obj=='controller'and len(args)==3 and args[0]==0 and args[2]is True,'original list call args')
                value=[c for c in choices if c is None or c['type']==args[1]]
                # Null is injected once through COMBAT for explicit null test.
                if args[1]==1:value=[c for c in value if c is not None]
            elif name=='concat':value=obj+args[0]
            elif name=='push':obj.append(args[0]);value=len(obj)
            else:raise ValueError('unreviewed test method')
            if op==0x46:stack.append(value)
        elif op==0x32:
            objreg,indexreg=a;obj=locals_[objreg];index=locals_[indexreg]+1
            locals_[indexreg]=index if index<=len(obj) else 0
            stack.append(index<=len(obj))
        elif op==0x23:index=pop();obj=pop();stack.append(obj[index-1])
        elif op in P.BRANCH:
            jump=op==0x10 or (bool(pop())if op==0x11 else not bool(pop()))
            if jump:pc=positions[a[0]]
        elif op==0x48:
            value=pop();P.need(not stack,'balanced selector return');return value
        else:raise ValueError('unreviewed test opcode')


class ActiveOrdinalSelector(unittest.TestCase):
    def setUp(self):
        self.prior=(D/'factor-endpoint.tag.DATA').read_bytes()
        self.before=(D/'Source.before.as.DATA').read_bytes()
        patch=ROOT/'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch'
        self.source=(patch/'as/codo/controllers/MatchController.as').read_bytes()
        self.tag=(patch/'tags/6167_82_codo_controllers_MatchController.bin').read_bytes()
        self.old=P.A.read(P.abc(self.prior)[0]);self.new=P.A.read(P.abc(self.tag)[0])
        self.oldcode=P.A.method(self.old,P.NAME)['code'];self.code=P.A.method(self.new,P.NAME)['code']
    def values(self,inputs,old,new):
        self.assertEqual(execute(self.oldcode,self.old,inputs),old)
        got=execute(self.code,self.new,inputs);self.assertEqual(got,new)
        self.assertTrue(all(any(x is y for y in inputs) for x in got))
    def test_active_ordinal_one_no_longer_hides_bot_training(self):
        boot=dict(id=0,type=2,combatTrainingType=0);bot=dict(id=1,type=2,combatTrainingType=0)
        self.values([boot,bot],[boot],[boot,bot])
    def test_ordinal_two_remains_visible(self):
        boot=dict(id=0,type=2,combatTrainingType=0);bot=dict(id=2,type=2,combatTrainingType=0)
        self.values([boot,bot],[boot,bot],[boot,bot])
    def test_null_ranked_and_versus_stay_hidden(self):
        self.values([None,dict(id=1,type=1,combatTrainingType=0),dict(id=1,type=2,combatTrainingType=1)],[],[])
    def test_order_and_object_identity_preserved(self):
        two=dict(id=2,type=2,combatTrainingType=0);boot=dict(id=0,type=2,combatTrainingType=0);one=dict(id=1,type=2,combatTrainingType=0)
        self.values([two,boot,one],[two,boot],[two,boot,one])
    def test_duplicates_remain_duplicates(self):
        one=dict(id=1,type=2,combatTrainingType=0);self.values([one,one],[],[one,one])
    def test_future_semantic_training_entry_is_visible(self):
        other=dict(id=9,type=2,combatTrainingType=0);self.values([other],[],[other])
    def test_selector_does_not_read_transient_ordinal(self):
        class NoOrdinal(dict):
            def __getitem__(self,key):
                if key=='id':raise RuntimeError('ordinal read')
                return super().__getitem__(key)
        row=NoOrdinal(type=2,combatTrainingType=0)
        with self.assertRaises(RuntimeError):execute(self.oldcode,self.old,[row])
        self.assertIs(execute(self.code,self.new,[row])[0],row)
    def test_exact_transform_preserves_other165_and_full_rate_factors(self):
        tag,proof=P.produce(self.prior,self.source,self.before)
        self.assertEqual(tag,self.tag)
        self.assertTrue(proof['other165_raw_exact']);self.assertTrue(proof['pools_tables_raw_exact'])
        self.assertTrue(proof['both_full_rate_factor_bodies_raw_exact'])
        self.assertEqual(proof['removed_bytes'],26);self.assertEqual(proof['new_instructions'],0)
    def test_source_and_preimage_drift_refused(self):
        for old,source,before in [(self.prior[:-1],self.source,self.before),(self.prior,self.source+b'\n',self.before),(self.prior,self.source,self.before+b'\n')]:
            with self.subTest(source=len(source)),self.assertRaises(ValueError):P.produce(old,source,before)
    def test_branch_to_removed_clause_and_mid_operand_refused(self):
        rows=P.decode(self.oldcode)
        retained=[dict(z,args=list(z['args'])) for z in rows if not 104<=z['label']<130]
        next(z for z in retained if z['op'] in P.BRANCH)['args']=[110]
        with self.assertRaisesRegex(ValueError,'removed id clause'):P.encode(retained)
        bad=bytearray(self.oldcode);bad[89:92]=(3).to_bytes(3,'little',signed=True)
        with self.assertRaisesRegex(ValueError,'boundary'):P.decode(bytes(bad))
    def test_actual_export_keeps_only_semantic_training_filter(self):
        exported=(D/'actual-export.as.DATA').read_text(encoding='utf-8')
        self.assertTrue(X.verify(exported)['no_id_or_name_filter'])
        for bad in [self.before.decode(),exported.replace('choice.type == PlayListType.COMBAT','choice.type == PlayListType.RANKED',1),exported.replace('choice.combatTrainingType == CombatType.CT','true',1)]:
            with self.assertRaises(ValueError):X.verify(bad)

if __name__=='__main__':unittest.main()
