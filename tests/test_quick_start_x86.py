"""Execute compiled production gate + exact C1 x86 start blocks in Unicorn.

Usage: python tests/test_quick_start_x86.py --fixture DLL --image C1 --output JSON
Requires pefile + unicorn. No process is attached or launched. Timer, Windows
memory/lock APIs, native event delivery and StartGame endpoint are explicit
boundary doubles; this is not C1 gameplay acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, struct, unittest
if __name__ != '__main__':
 raise unittest.SkipTest('Exact private C1 x86 proof is invoked explicitly with --image and --fixture')
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *

P=argparse.ArgumentParser()
P.add_argument('--fixture',required=True)
P.add_argument('--image',required=True)
P.add_argument('--output',required=True)
A,unittest_args=P.parse_known_args()
IMAGE=Path(A.image).read_bytes(); PE=pefile.PE(data=IMAGE,fast_load=True)
assert hashlib.sha256(IMAGE).hexdigest() in {
 'a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976',
 'd8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60'}
FIXTURE=Path(A.fixture).read_bytes(); FP=pefile.PE(data=FIXTURE)
EXPORT={s.name.decode():FP.OPTIONAL_HEADER.ImageBase+s.address for s in FP.DIRECTORY_ENTRY_EXPORT.symbols}
ROOM=0x82d5470; CTRL=0x6000000; RECORD=0x6001000
SLOT=0x6c44f0c; STACK=0x30000000; STOP=STACK+0x10000
ROOT=Path(__file__).resolve().parents[1]
import re
GUARDS=[(int(a,16),int(n)) for a,n in re.findall(r'\{0x([0-9A-F]+)u,(\d+)u,', (ROOT/'quick_start_guards.h').read_text())]

class Machine:
 def __init__(self):
  self.u=Uc(UC_ARCH_X86,UC_MODE_32);self.pages=set();self.searches=0;self.events=0;self.frame_phase=None
  for at,n in GUARDS+[(0x710383,0x1cc),(0x715cd0,64),(0x70e8b0,0x100),(0xa2ce00,32)]:
   self.map(at,n);self.u.mem_write(at,PE.get_data(at-0x400000,n))
  self.map(ROOM,0x16000);self.map(0x82eb000,0x1000);self.map(0x6c44000,0x1000)
  self.map(CTRL,0x4000);self.map(STACK,0x11000)
  base=FP.OPTIONAL_HEADER.ImageBase;self.map(base,FP.OPTIONAL_HEADER.SizeOfImage)
  self.u.mem_write(base,FP.get_memory_mapped_image())
  self.put(SLOT,CTRL);self.put(CTRL,0xc7de84);self.put(CTRL+0xec,1);self.put(CTRL+0x10,5)
  self.put(ROOM,0xc7d66c);self.put(ROOM+0x10,RECORD);self.put(RECORD+0x10,69421,8)
  for offset in [0x44f8,0x4501,0x15aa8]:self.put(ROOM+offset,1,1)
  self.put(ROOM+0x15a70,2)
  for i,name in enumerate([b'playlist\0',b'ctf\0',b'mp_crash_tropical\0']):
   at=CTRL+0x2000+i*0x100;self.u.mem_write(at,name);self.put(ROOM+0x15a80+i*4,at)
  for offset,value in [(0xc,11),(0xd,2),(0xe,12),(0x28,1),(0x2c,0),(0x2d,0xff)]:self.put(ROOM+0x15a80+offset,value,1)
  self.put(ROOM+0x13f20,11,8);self.put(ROOM+0x14090,22,8)
  self.ready(0,1);self.ready(1,1)
  self.put(0x82eb0a8,1)
  self.put(0x6c44d90,CTRL+0x3000);self.put(CTRL+0x300c,1,1)
  self.put(0x6c44da0,CTRL+0x3100);self.u.mem_write(CTRL+0x310c,struct.pack('<f',10))
  self.u.mem_write(CTRL+0x3200,struct.pack('<f',20))
  # Real x87 comparison against a deterministic elapsed-time boundary double.
  self.u.mem_write(0xa2ce00,b'\xd9\x05'+struct.pack('<I',CTRL+0x3200)+b'\xc3')
  self.u.mem_write(0x715cd0,b'\xb8'+struct.pack('<I',ROOM)+b'\xc3')
  self.u.hook_add(UC_HOOK_CODE,self.hook)
 def map(self,at,n):
  for page in range(at&~4095,(at+n+4095)&~4095,4096):
   if page not in self.pages:self.u.mem_map(page,4096);self.pages.add(page)
 def put(self,at,n,size=4):self.u.mem_write(at,int(n).to_bytes(size,'little'))
 def get(self,at,size=4):return int.from_bytes(self.u.mem_read(at,size),'little')
 def var(self,n,v=None):
  at=EXPORT[n]
  if v is not None:self.put(at,v)
  return self.get(at)
 def ready(self,i,value):self.put(ROOM+0x13f20+i*0x170+0x89,value,1)
 def ret(self,extra=0):
  sp=self.u.reg_read(UC_X86_REG_ESP);at=self.get(sp);self.u.reg_write(UC_X86_REG_ESP,sp+4+extra);self.u.reg_write(UC_X86_REG_EIP,at)
 def hook(self,u,at,size,data):
  if at in [STOP,0x71b457]:u.emu_stop()
  elif at==0x70e520:self.searches+=1;self.ret(8)
  elif at==0x70e8b0:self.events+=1;self.ret()
  elif at==0x71ad66:
   # Native frame body is an explicit boundary; execute its real prologue and
   # ret ABI while observing before/after simulated normal phase progression.
   if self.frame_phase is not None:self.put(CTRL+0x10,self.frame_phase)
   bp=u.reg_read(UC_X86_REG_EBP);u.reg_write(UC_X86_REG_ESP,bp);u.reg_write(UC_X86_REG_EBP,self.get(bp));u.reg_write(UC_X86_REG_ESP,bp+4);self.ret()
 def call(self,name,*args):
  sp=STACK+0xf000;self.put(sp,STOP)
  for i,a in enumerate(args):self.put(sp+4+4*i,a)
  self.u.reg_write(UC_X86_REG_ESP,sp);self.u.reg_write(UC_X86_REG_EBP,0x12345678)
  self.u.emu_start(EXPORT[name],STOP,count=3000000)
  assert self.u.reg_read(UC_X86_REG_EIP)==STOP,(name,hex(self.u.reg_read(UC_X86_REG_EIP)))
  assert self.u.reg_read(UC_X86_REG_ESP)==sp+4,(name,'cdecl stack imbalance')
  return self.u.reg_read(UC_X86_REG_EAX)
 def install(self):self.call('QsgTestInstall')
 def bridge(self):self.call('QsgTestBridge',0)
 def tick(self):self.call('QsgTestTick',CTRL)
 def state(self):return self.call('QsgTestState')
 def native(self):
  self.u.reg_write(UC_X86_REG_ESP,STACK+0xe000);self.u.reg_write(UC_X86_REG_EBX,CTRL)
  self.u.reg_write(UC_X86_REG_ESI,0x12345678);self.u.reg_write(UC_X86_REG_EDI,0x23456789)
  self.u.emu_start(0x71b40f,0x71b457,count=3000000)
  assert self.u.reg_read(UC_X86_REG_EIP)==0x71b457
  assert self.u.reg_read(UC_X86_REG_ESP)==STACK+0xe000
  assert self.u.reg_read(UC_X86_REG_EBX)==CTRL
  assert self.u.reg_read(UC_X86_REG_ESI)==0x12345678
  assert self.u.reg_read(UC_X86_REG_EDI)==0x23456789

class NativeQuickGate(unittest.TestCase):
 def test_public_playlist_retains_native_start_without_quick_intent(self):
  # Actual editor C1 capture: mode=playlist, tail discriminator=2, two
  # admitted members, no Quick Ready/AS intent. The stock timer owns start.
  m=Machine();m.install();m.put(ROOM+0x15a8c,2,1)
  m.ready(0,0);m.ready(1,0);m.tick();m.native()
  self.assertEqual(m.searches,1);self.assertEqual(m.get(CTRL+0x10),6)
 def test_unrelated_playlist_discriminators_do_not_enter_quick_policy(self):
  for discriminator in [0,1,2,3,10,12,255]:
   with self.subTest(discriminator=discriminator):
    m=Machine();m.install();m.put(ROOM+0x15a8c,discriminator,1)
    m.tick();m.native();self.assertEqual(m.searches,1)
 def test_old_native_starts_without_any_genuine_bridge(self):
  m=Machine();m.ready(1,0);m.native();self.assertEqual(m.searches,1);self.assertEqual(m.get(CTRL+0x10),6)
 def test_delayed_results_135_seconds_creates_no_search_until_bridge(self):
  m=Machine();m.install();m.ready(1,0)
  for t in [0,10000,60000,120000,135000]:m.var('qtest_now',t);m.tick();m.native()
  self.assertEqual(m.searches,0);self.assertEqual(m.get(CTRL+0x10),5)
  m.ready(1,1);m.bridge();m.native();self.assertEqual(m.searches,1);self.assertEqual(m.get(CTRL+0x10),6)
 def test_ready_snapshot_without_bridge_never_grants(self):
  m=Machine();m.install();m.tick();m.native();self.assertEqual(m.searches,0)
 def test_bridge_phase3_or4_survives_normal_progression(self):
  for phase in [3,4,5]:
   with self.subTest(phase=phase):
    m=Machine();m.install();m.put(CTRL+0x10,phase);m.bridge();self.assertEqual(m.events,1)
    for p in range(phase,6):m.put(CTRL+0x10,p);m.tick()
    m.native();self.assertEqual(m.searches,1)
 def test_real_successor_intent_phase1_survives_enter_room_before_phase5(self):
  m=Machine();m.install();m.put(CTRL+0x10,1);m.bridge()
  self.assertEqual(m.events,1);self.assertEqual(m.state(),1)
  for phase,now in [(1,0),(2,20000),(3,80000),(4,90000),(5,100000)]:
   m.var('qtest_now',now);m.put(CTRL+0x10,phase);m.tick()
  m.native();self.assertEqual(m.searches,1);self.assertEqual(m.get(CTRL+0x10),6)
 def test_phase1_ready_without_bridge_never_grants(self):
  m=Machine();m.install();m.put(CTRL+0x10,1);m.tick()
  for phase in [3,4,5]:m.put(CTRL+0x10,phase);m.tick()
  m.native();self.assertEqual(m.searches,0)
 def test_phase1_intent_real_phase_exit_cannot_be_consumed(self):
  m=Machine();m.install();m.put(CTRL+0x10,1);m.bridge()
  m.put(CTRL+0x10,0);m.tick();m.put(CTRL+0x10,5);m.native()
  self.assertEqual(m.searches,0)
 def test_early_intent_cannot_outlive_collection_bound_or_be_renewed(self):
  m=Machine();m.install();m.put(CTRL+0x10,1);m.bridge()
  m.var('qtest_now',120000);m.put(CTRL+0x10,2);m.tick();m.bridge()
  m.put(CTRL+0x10,5);m.native();self.assertEqual(m.searches,0)
 def test_consumed_generation_returning_to_waiting_needs_new_genuine_bridge(self):
  m=Machine();m.install();m.bridge();m.native();self.assertEqual(m.searches,1)
  m.put(CTRL+0x10,1);m.tick();self.assertEqual(m.state(),4)
  m.put(CTRL+0x10,5);m.native();self.assertEqual(m.searches,1)
  m.bridge();m.native();self.assertEqual(m.searches,2)
 def test_frame_observer_ret_abi_and_rearms_same_cohort_next_round(self):
  m=Machine();m.install();m.put(CTRL+0x10,3);m.bridge();m.frame_phase=4;m.call('QsgTestFrame',CTRL)
  self.assertEqual(m.state(),1);m.put(CTRL+0x10,5);m.native();self.assertEqual(m.searches,1)
  m.frame_phase=None;m.call('QsgTestFrame',CTRL);self.assertEqual(m.state(),0)
  m.put(CTRL+0x10,3);m.bridge();m.put(CTRL+0x10,5);m.native();self.assertEqual(m.searches,2)
 def test_duplicate_bridge_never_renews_or_duplicates_start(self):
  m=Machine();m.install();m.var('qtest_now',100);m.bridge();m.var('qtest_now',200);m.bridge()
  self.assertEqual(m.call('QsgTestBorn'),100);m.native();self.assertEqual(m.searches,1)
  m.bridge();m.put(CTRL+0x10,5);m.native();self.assertEqual(m.searches,1)
 def test_intent_expiry_and_duplicate_cannot_recover(self):
  m=Machine();m.install();m.bridge();m.var('qtest_now',30000);m.native();self.assertEqual(m.searches,0)
  self.assertEqual(m.state(),3);m.bridge();m.native();self.assertEqual(m.searches,0)
 def test_tick_wrap_preserves_finite_lifetime(self):
  m=Machine();m.install();m.var('qtest_now',0xfffffff0);m.bridge();m.var('qtest_now',100);m.native();self.assertEqual(m.searches,1)
 def test_synchronous_reentrant_bridge_does_not_renew_grant(self):
  m=Machine();m.install();m.var('qtest_now',200);m.call('QsgTestReentrant')
  self.assertEqual(m.var('qtest_reentrant_calls'),2);self.assertEqual(m.call('QsgTestBorn'),200)
  m.native();self.assertEqual(m.searches,1);m.put(CTRL+0x10,5);m.native();self.assertEqual(m.searches,1)
 def test_replaced_controller_cannot_consume_original_intent(self):
  m=Machine();m.install();m.bridge();new=CTRL+0x100
  m.u.mem_write(new,bytes(m.u.mem_read(CTRL,0x100)));m.put(SLOT,new)
  self.assertEqual(m.call('QsgTestAllow',new),0)
 def test_unchanged_ready_cohort_after_phase6_needs_new_bridge(self):
  m=Machine();m.install();m.bridge();m.native();m.tick()
  m.put(CTRL+0x10,5);m.native();self.assertEqual(m.searches,1)
  m.bridge();m.native();self.assertEqual(m.searches,2)
 def test_pending_identity_or_ready_mutations_invalidate(self):
  changes=[(ROOM+0x13f20+0x89,0,1),(ROOM+0x44f8,0,1),(RECORD+0x10,69422,8),
   (ROOM+0x13f20,33,8),(ROOM+0x15a70,3,4),(ROOM+0x15aac,1,1),
   (ROOM+0x15aad,1,1),(ROOM+0x15a8e,11,1),(CTRL+0x2200,ord('x'),1),
   (CTRL+0x2100,ord('x'),1),(SLOT,CTRL+0x100,4)]
  for at,value,n in changes:
   with self.subTest(address=hex(at)):
    m=Machine();m.install();m.bridge();m.put(at,value,n);m.tick();m.native();self.assertEqual(m.searches,0)
 def test_managed_cycle_scope_drift_never_bypasses_permission(self):
  for arm in [False,True]:
   m=Machine();m.install();m.tick()
   if arm:m.bridge()
   m.u.mem_write(CTRL+0x2000,b'war\0');m.put(ROOM+0x15a8c,0,1)
   m.tick();m.native();self.assertEqual(m.searches,0)
 def test_unknown_snapshot_never_proves_phase_exit_or_rearms(self):
  for mutation in ['slot','expected','vtable','kind']:
   m=Machine();m.install();m.bridge()
   if mutation=='slot':m.put(SLOT,0);m.tick();m.put(SLOT,CTRL)
   elif mutation=='expected':m.call('QsgTestTick',CTRL+0x100)
   elif mutation=='vtable':m.put(CTRL,0);m.tick();m.put(CTRL,0xc7de84)
   else:m.put(CTRL+0xec,2);m.tick();m.put(CTRL+0xec,1)
   m.bridge();m.native();self.assertEqual(m.searches,0);self.assertEqual(m.state(),3)
 def test_cancel_and_phase_regression_invalidate(self):
  for phase in [0,1,2,4,6,7]:
   with self.subTest(phase=phase):
    m=Machine();m.install();m.bridge();m.put(CTRL+0x10,phase);m.tick();m.put(CTRL+0x10,5);m.native();self.assertEqual(m.searches,0)
 def test_guest_unready_or_duplicate_uid_bridge_has_no_permission(self):
  for at,value,n in [(ROOM+0x44f8,0,1),(ROOM+0x14090+0x89,0,1),(ROOM+0x14090,11,8)]:
   m=Machine();m.install();m.put(at,value,n);m.bridge();m.native();self.assertEqual(m.searches,0)
 def test_cohort_order_is_not_identity_change(self):
  m=Machine();m.install();m.bridge();m.put(ROOM+0x13f20,22,8);m.put(ROOM+0x14090,11,8);m.tick();m.native();self.assertEqual(m.searches,1)
 def test_zero_selection_is_valid_and_consumed_advance_is_not_rearm(self):
  m=Machine();m.install();m.bridge();m.native();m.put(ROOM+0x15aac,1,1);m.put(CTRL+0x10,5);m.bridge();m.native();self.assertEqual(m.searches,1)
 def test_stock_timer_and_dvar_checks_still_control_consume(self):
  for mode in ['bool','time']:
   m=Machine();m.install();m.bridge()
   if mode=='bool':m.put(CTRL+0x300c,0,1)
   else:m.u.mem_write(CTRL+0x3200,struct.pack('<f',2))
   m.native();self.assertEqual(m.searches,0);self.assertEqual(m.state(),1)
 def test_manual_and_other_native_modes_preserved(self):
  for at,value,n in [(ROOM+0x15aa8,0,1),(CTRL+0xec,2,4)]:
   m=Machine();before=bytes(m.u.mem_read(0x710383,0x1cc));m.install();m.put(at,value,n);m.native()
   self.assertEqual(m.searches,1);self.assertEqual(bytes(m.u.mem_read(0x710383,0x1cc)),before)
 def test_each_exact_guard_mutation_keeps_all_three_entries_untouched(self):
  for at,n in GUARDS:
   with self.subTest(address=hex(at)):
    m=Machine();m.put(at,m.get(at,1)^1,1);before=[bytes(m.u.mem_read(a,s)) for a,s in [(0x6aefb0,5),(0x71ad60,6),(0x71b437,7)]]
    m.install();self.assertEqual([bytes(m.u.mem_read(a,s)) for a,s in [(0x6aefb0,5),(0x71ad60,6),(0x71b437,7)]],before)
    self.assertEqual(m.var('qtest_protect_calls'),0)
 def test_prepare_and_postwrite_failures_rollback_entire_transaction(self):
  for var,value in [('qtest_fail_protect',i) for i in range(1,8)]+[('qtest_fail_flush',i) for i in range(1,5)]:
   with self.subTest(variable=var,value=value):
    m=Machine();before=[bytes(m.u.mem_read(a,s)) for a,s in [(0x6aefb0,5),(0x71ad60,6),(0x71b437,7)]]
    m.var(var,value);m.install()
    self.assertEqual([bytes(m.u.mem_read(a,s)) for a,s in [(0x6aefb0,5),(0x71ad60,6),(0x71b437,7)]],before)
    self.assertEqual(m.var('qtest_terminated'),0)

if __name__=='__main__':
 suite=unittest.defaultTestLoader.loadTestsFromTestCase(NativeQuickGate)
 result=unittest.TextTestRunner(verbosity=2).run(suite)
 report=dict(passed=result.wasSuccessful(),methods=result.testsRun,failures=len(result.failures),errors=len(result.errors),
  image_sha256=hashlib.sha256(IMAGE).hexdigest(),fixture_sha256=hashlib.sha256(FIXTURE).hexdigest(),
  source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['quick_start_gate.c','quick_start_policy.h','quick_start_guards.h','tests/quick_start_x86_fixture.c','tests/test_quick_start_x86.py']},
  limits='Offline exact x86; timer/OS/frame-body/event/StartGame boundaries doubled; no live C1 acceptance')
 Path(A.output).write_text(json.dumps(report,indent=2)+'\n')
 raise SystemExit(not result.wasSuccessful())
