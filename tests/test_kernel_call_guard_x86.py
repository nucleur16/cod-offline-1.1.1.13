"""Offline compiled x86 call/SEH boundary proof; Windows dispatch is modeled.

No process is attached or started. Invalid native memory reads are real Unicorn
faults; the OS exception dispatcher is replaced by an explicit CONTEXT/record
call to the compiled handler. Recognized native faults must terminate, never
resume normal execution with potentially held locks or partial native state.
"""
from pathlib import Path
import argparse,hashlib,json,struct,unittest
if __name__!='__main__':
 raise unittest.SkipTest('Explicit x86 fixture/Unicorn proof; no Windows EXE execution')
import pefile
from unicorn import Uc,UcError,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_INTR
from unicorn.x86_const import *
ap=argparse.ArgumentParser();ap.add_argument('--fixture',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--image',type=Path)
args,remaining=ap.parse_known_args()
data=args.fixture.read_bytes();pe=pefile.PE(data=data);base=pe.OPTIONAL_HEADER.ImageBase
exports={s.name.decode():base+s.address for s in pe.DIRECTORY_ENTRY_EXPORT.symbols}
STACK=0x30000000;CODE=0x100000;STOP=STACK+0x1fff0
CALLEE=[UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
SAVED=[0x11223344,0x22334455,0x33445566,0x44556677]
ALL=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_EFLAGS]
class Machine:
 def __init__(self):
  self.u=Uc(UC_ARCH_X86,UC_MODE_32)
  for at,size in [(0,4096),(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095),(STACK,0x20000),(CODE,0x10000)]:self.u.mem_map(at,size)
  self.u.mem_write(base,pe.get_memory_mapped_image());self.put(0,CODE+0xf000)
  for reg,value in zip(CALLEE,SAVED):self.u.reg_write(reg,value)
  self.target_registers=None
  self.terminated=None;self.terminate_fail=False;self.failfast=None;self.api={};self.depth=0;self.enters=0;self.leaves=0
  for lib in getattr(pe,'DIRECTORY_ENTRY_IMPORT',[]):
   for item in lib.imports:
    name=item.name.decode();at=CODE+0xd000+16*len(self.api);self.api[name]=at;self.put(item.address,at)
  self.u.hook_add(UC_HOOK_CODE,self.hook)
  self.u.hook_add(UC_HOOK_INTR,self.interrupt)
 def ret(self,value=0,cleanup=0):
  sp=self.u.reg_read(UC_X86_REG_ESP);target=self.get(sp);self.u.reg_write(UC_X86_REG_EAX,value&0xffffffff);self.u.reg_write(UC_X86_REG_ESP,sp+4+cleanup);self.u.reg_write(UC_X86_REG_EIP,target)
 def interrupt(self,u,number,unused):
  assert number==0x29;self.failfast=u.reg_read(UC_X86_REG_ECX);u.emu_stop()
 def hook(self,u,at,size,unused):
  if at==STOP:u.emu_stop()
  if at==CODE:self.target_registers=(u.reg_read(UC_X86_REG_EAX),u.reg_read(UC_X86_REG_ECX),self.get(u.reg_read(UC_X86_REG_ESP)+4))
  if at==self.api.get('GetCurrentProcess'):self.ret(0xffffffff);return
  if at==self.api.get('TerminateProcess'):
   self.terminated=self.get(u.reg_read(UC_X86_REG_ESP)+8)
   if self.terminate_fail:self.ret(0,8)
   else:u.emu_stop()
  if at==CODE+0xa000:self.depth+=1;self.enters+=1
  if at==CODE+0xa100:self.depth-=1;self.leaves+=1
 def get(self,at):return struct.unpack('<I',self.u.mem_read(at,4))[0]
 def put(self,at,value):self.u.mem_write(at,struct.pack('<I',value&0xffffffff))
 def exported(self,name):return self.get(exports[name])
 def invoke(self,name,values,sp=STACK+0xf000,expect_fault=False):
  self.put(sp,STOP)
  for i,value in enumerate(values):self.put(sp+4+i*4,value)
  self.u.reg_write(UC_X86_REG_ESP,sp)
  try:self.u.emu_start(exports[name],STOP,count=100000)
  except UcError:
   if not expect_fault:raise
   return None
  if self.terminated is not None or self.failfast is not None:return None
  if expect_fault:raise AssertionError('expected a real unmapped-memory fault')
  assert self.u.reg_read(UC_X86_REG_EIP)==STOP
  assert self.u.reg_read(UC_X86_REG_ESP)==sp+4
  return self.u.reg_read(UC_X86_REG_EAX)
 def dispatch(self,code=0xc0000005,flags=0):
  registers={r:self.u.reg_read(r) for r in ALL}
  frame=self.get(0);record=STACK+0x10000;context=STACK+0x11000
  self.u.mem_write(record,b'\0'*80);self.put(record,code);self.put(record+4,flags)
  self.u.mem_write(context,b'\0'*self.exported('context_size'))
  for field,reg in [('eip',UC_X86_REG_EIP),('esp',UC_X86_REG_ESP),('ebp',UC_X86_REG_EBP)]:self.put(context+self.exported('context_'+field),registers[reg])
  result=self.invoke('Dispatch',[record,frame,context],STACK+0x1e000)
  updated={field:self.get(context+self.exported('context_'+field)) for field in ('eip','esp','ebp')}
  if self.terminated is not None or self.failfast is not None:return result,frame,updated
  for r,value in registers.items():self.u.reg_write(r,value)
  return result,frame,updated
 def resume(self,updated):
  for field,reg in [('eip',UC_X86_REG_EIP),('esp',UC_X86_REG_ESP),('ebp',UC_X86_REG_EBP)]:self.u.reg_write(reg,updated[field])
  self.u.emu_start(updated['eip'],STOP,count=100000)
  assert self.u.reg_read(UC_X86_REG_EIP)==STOP
  return self.u.reg_read(UC_X86_REG_EAX)

class Guard(unittest.TestCase):
 def restored(self,m):
  self.assertEqual(m.get(0),CODE+0xf000)
  self.assertEqual(m.u.reg_read(UC_X86_REG_ESP),STACK+0xf004)
  self.assertEqual([m.u.reg_read(r) for r in CALLEE],SAVED)
 def test_each_supported_calling_convention_and_callee_state(self):
  variants=[(0,b'\xb8\x01\xcd\xab\xff\xc3',1),(1,b'\x8b\x44\x24\x04\xc2\x04\x00',0x12345678),
            (2,b'\x31\xc0\xc2\x04\x00',1),(3,b'\x8b\x44\x24\x04\xc3',0x12345678),(4,b'\xc3',1)]
  for convention,body,wanted in variants:
   with self.subTest(convention=convention):
    m=Machine();m.u.mem_write(CODE,body)
    self.assertEqual(m.invoke('Call',[0x12345678,CODE,convention]),wanted)
    if convention==0:self.assertEqual(m.target_registers[1],0x12345678)
    if convention in (1,2,3):self.assertEqual(m.target_registers[2],0x12345678)
    if convention==4:self.assertEqual(m.target_registers[:2],(0x12345678,0))
    self.restored(m)
 def test_unknown_convention_cannot_call_native(self):
  m=Machine();m.u.mem_write(CODE,b'\xcc')
  self.assertEqual(m.invoke('Call',[1,CODE,5]),0);self.assertIsNone(m.target_registers);self.restored(m)
 def test_real_fault_each_convention_terminates_without_context_rewrite(self):
  for convention in range(5):
   for altered in (False,True):
    with self.subTest(convention=convention,altered_stack=altered):
     m=Machine();body=(b'\x55\x53\x83\xec\x20\xbd\x00\x90\x99\x99' if altered else b'')+b'\xb8\x00\x00\xad\xde\x8b\x00\xc3'
     m.u.mem_write(CODE,body);m.invoke('Call',[0x12345678,CODE,convention],expect_fault=True)
     before={field:m.u.reg_read(reg) for field,reg in [('eip',UC_X86_REG_EIP),('esp',UC_X86_REG_ESP),('ebp',UC_X86_REG_EBP)]}
     result,frame,context=m.dispatch();self.assertIsNone(result);self.assertEqual(m.terminated,0x4b524301);self.assertEqual(context,before)
 def test_unwind_noncontinuable_and_unrelated_errors_are_not_consumed(self):
  for code,flags in [(0xc0000005,1),(0xc0000005,2),(0xc0000005,4),(0xc000001d,0),(0x80000003,0)]:
   with self.subTest(code=hex(code),flags=flags):
    m=Machine();m.u.mem_write(CODE,b'\xb8\x00\x00\xad\xde\x8b\x00\xc3');m.invoke('Call',[1,CODE,0],expect_fault=True)
    before={r:m.u.reg_read(r) for r in (UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EBP)}
    result,frame,context=m.dispatch(code,flags);self.assertEqual(result,1)
    self.assertEqual(list(context.values()),list(before.values()))
 def test_in_page_error_uses_same_fail_closed_path(self):
  m=Machine();m.u.mem_write(CODE,b'\xb8\x00\x00\xad\xde\x8b\x00\xc3');m.invoke('Call',[1,CODE,0],expect_fault=True)
  result,frame,context=m.dispatch(0xc0000006);self.assertIsNone(result);self.assertEqual(m.terminated,0x4b524301)
 def test_termination_failure_uses_failfast_trap_not_continuation(self):
  m=Machine();m.terminate_fail=True;m.u.mem_write(CODE,b'\xb8\x00\x00\xad\xde\x8b\x00\xc3');m.invoke('Call',[1,CODE,0],expect_fault=True)
  result,frame,context=m.dispatch();self.assertIsNone(result);self.assertEqual(m.failfast,7);self.assertNotEqual(m.u.reg_read(UC_X86_REG_EIP),STOP)
 def test_false_null_native_returns_do_not_terminate(self):
  for convention,body in [(0,b'\x31\xc0\xc3'),(1,b'\x31\xc0\xc2\x04\x00'),(3,b'\x31\xc0\xc3')]:
   m=Machine();m.u.mem_write(CODE,body);self.assertEqual(m.invoke('Call',[1,CODE,convention]),0);self.assertIsNone(m.terminated);self.restored(m)
 def actual(self,kind,nested=0,fault=True):
  if not args.image:self.skipTest('Exact owned native image explicitly required')
  raw=args.image.read_bytes();self.assertEqual(hashlib.sha256(raw).hexdigest(),'a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976');native=pefile.PE(data=raw);m=Machine();m.depth=nested
  def map(at,size):m.u.mem_map(at&~4095,((at%4096+size+4095)//4096)*4096)
  enter,leave=CODE+0xa000,CODE+0xa100;m.u.mem_write(enter,b'\xc2\x04\x00');m.u.mem_write(leave,b'\xc2\x04\x00');map(0xb42000,4096)
  if kind=='cbuf':
   function=0x58ba60;map(function,0xab);m.u.mem_write(function,native.get_data(function-0x400000,0xab));map(0x25094dc,12)
   m.put(0xb421ec,enter);m.put(0xb421e8,leave);m.put(0x25094dc,0xdead0000 if fault else CODE+0x4000);m.put(0x25094e0,65536);m.put(0x25094e4,0)
   text=CODE+0x2000;m.u.mem_write(text,b'set ui_quickNativeRoomContext "v1|1|0000000000000001"\n\0');convention=4;fault_site=0x58baf2
  else:
   function=0x6232c0;map(function,0xc7);m.u.mem_write(function,native.get_data(function-0x400000,0xc7));map(0x6217e0,0x4d);m.u.mem_write(0x6217e0,native.get_data(0x6217e0-0x400000,0x4d))
   map(0x2ab98d0,8);map(0x2abd8e0,4);m.put(0x2ab98d4,0);m.put(0x2abd8e0,0xdead0000 if fault else 0);m.put(0xb42120,enter);m.put(0xb42124,leave)
   text=CODE+0x2000;m.u.mem_write(text,b'\0');convention=3;fault_site=0x623305
  result=m.invoke('Call',[text,function,convention],expect_fault=fault)
  if fault:
   self.assertEqual(m.u.reg_read(UC_X86_REG_EIP),fault_site);self.assertEqual((m.depth,m.enters,m.leaves),(nested+1,1,0));result,frame,context=m.dispatch();self.assertIsNone(result);self.assertEqual(m.terminated,0x4b524301);self.assertEqual((m.depth,m.enters,m.leaves),(nested+1,1,0));self.assertNotEqual(m.u.reg_read(UC_X86_REG_EIP),STOP)
  else:
   self.assertEqual(result,1 if kind=='cbuf' else 0);self.assertIsNone(m.terminated);self.assertEqual((m.depth,m.enters,m.leaves),(nested,1,1));self.restored(m)
 def test_actual_native_cbuf_fault_and_nested_recovery_lock_terminate(self):
  for nested in (0,1):self.actual('cbuf',nested)
 def test_actual_native_dvar_fault_terminates(self):self.actual('dvar')
 def test_actual_native_success_and_null_returns_balance_locks(self):
  self.actual('cbuf',0,False);self.actual('cbuf',1,False);self.actual('dvar',0,False)

if __name__=='__main__':
 result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Guard))
 report={'passed':result.wasSuccessful(),'methods':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),
         'fixture_sha256':hashlib.sha256(data).hexdigest(),'source_sha256':hashlib.sha256((Path(__file__).resolve().parents[1]/'src/kernel/kernel_call_guard.c').read_bytes()).hexdigest(),
         'native_fault_policy':'Terminate client without lock cleanup or continuation; does not repair underlying fault','exact_native_image_supplied':bool(args.image),
         'boundary':'Compiled x86 call/handler and real Unicorn memory faults; Windows exception dispatch/CONTEXT application modeled. No actual Windows/game execution.'}
 args.output.write_text(json.dumps(report,indent=2)+'\n');raise SystemExit(not result.wasSuccessful())
