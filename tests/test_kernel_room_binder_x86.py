"""Compiled Kernel33 binder against exact private C1 guard bytes, offline only.

Windows APIs, dvar lookup and native room predicate are explicit doubles. The
actual retained Cbuf_AddText instructions execute. No Windows process launches.
"""
from pathlib import Path
import argparse,hashlib,json,re,struct,unittest
if __name__!='__main__':raise unittest.SkipTest('Explicit private-image x86 binder proof')
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
ap=argparse.ArgumentParser();ap.add_argument('--fixture',type=Path,required=True);ap.add_argument('--image',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
args,remaining=ap.parse_known_args();data=args.fixture.read_bytes();pe=pefile.PE(data=data)
raw=args.image.read_bytes();image_sha=hashlib.sha256(raw).hexdigest()
assert image_sha=='a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976'
image=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
exports={s.name.decode():base+s.address for s in pe.DIRECTORY_ENTRY_EXPORT.symbols}
STACK=0x30000000;STUB=0x10000000;MEM=0x6000000;STOP=STACK+0x1fff0;SITE=0x595e9d
ROOT=Path(__file__).resolve().parents[1]
guards=[]
for name,body in re.findall(r'static const unsigned char krc_(a5a7\w+)\[\]=\{([^}]+)\};',(ROOT/'src/kernel/kernel_room_guards.h').read_text()):
 address=0x70e9f0 if name.endswith('IsInRoom') else 0x70e9b0 if name.endswith('Membership') else 0x6232c0 if name.endswith('DvarFind') else 0x58ba60
 guards.append((name,address,bytes(int(v,16) for v in re.findall(r'0x([0-9a-f]+)',body))))
class Machine:
 def __init__(self):
  self.u=Uc(UC_ARCH_X86,UC_MODE_32);self.pages=set();self.api={};self.names={}
  self.map(0,4096);self.map(base,pe.OPTIONAL_HEADER.SizeOfImage);self.u.mem_write(base,pe.get_memory_mapped_image())
  self.map(STACK,0x20000);self.map(STUB,0x10000);self.map(MEM,0x20000)
  self.now=0;self.protects=0;self.flushes=0;self.fail_protect=set();self.fail_flush=set();self.terminated=None;self.deny=None
  self.proxy_path='C:\\Windows\\SysWOW64\\d3d9.dll';self.lock_depth=0;self.max_lock_depth=0;self.lock_busy=False;self.room=True;self.dvar=True;self.endpoint_calls=0
  for library in pe.DIRECTORY_ENTRY_IMPORT:
   for item in library.imports:
    name=item.name.decode();address=self.address(name);self.put(item.address,address)
  for name in ['EnterCriticalSection','LeaveCriticalSection']:self.address(name)
  for name,address,body in guards:
   actual=image.get_data(address-0x400000,len(body));assert actual==body,name
   self.map(address,len(body));self.u.mem_write(address,actual)
  for address,size in [(SITE,5),(0x595cf0,1),(0x25094dc,12),(0x2ab98c4,4),(0x82d5470,0x16000),(0xc7d66c,0x50),(0xb421e8,8)]:self.map(address,size)
  self.u.mem_write(SITE,b'\xe8'+struct.pack('<i',0x595cf0-SITE-5));self.original=bytes(self.u.mem_read(SITE,5));self.u.mem_write(0x595cf0,b'\xc3')
  self.put(0xb421ec,self.api['EnterCriticalSection']);self.put(0xb421e8,self.api['LeaveCriticalSection'])
  self.put(0x25094dc,MEM+0x3000);self.put(0x25094e0,65536);self.put(0x25094e4,0);self.put(0x2ab98c4,100)
  self.put(0x82d5470,0xc7d66c);self.put(0xc7d66c+0x44,0x70e9f0);self.put(0xc7d66c+0x40,0x70e9b0);self.put(0x82d5470+0x10,MEM+0x1000)
  self.write(MEM+0x1001,b'\x01');self.put(MEM+0x1010,0x1234);self.put(MEM+0x1014,0x5678)
  self.write(MEM+0x2008,b'\x07');self.put(MEM+0x200c,MEM+0x2100);self.write(MEM+0x2100,b'\0'*64)
  self.u.hook_add(UC_HOOK_CODE,self.hook)
 def map(self,address,size):
  for page in range(address&~4095,(address+size+4095)&~4095,4096):
   if page not in self.pages:self.u.mem_map(page,4096);self.pages.add(page)
 def address(self,name):
  if name not in self.api:
   address=STUB+len(self.api)*16;self.api[name]=address;self.names[address]=name
  return self.api[name]
 def get(self,address):return struct.unpack('<I',self.u.mem_read(address,4))[0]
 def put(self,address,value):self.write(address,struct.pack('<I',value&0xffffffff))
 def write(self,address,data):self.u.mem_write(address,bytes(data))
 def text(self,address,wide=False):
  out=bytearray();width=2 if wide else 1
  for i in range(4096):
   b=bytes(self.u.mem_read(address+i*width,width))
   if b==b'\0'*width:return out.decode('utf-16le' if wide else 'ascii')
   out.extend(b)
  raise AssertionError('unterminated fixture string')
 def ret(self,result=0,cleanup=0):
  sp=self.u.reg_read(UC_X86_REG_ESP);target=self.get(sp);self.u.reg_write(UC_X86_REG_EAX,result&0xffffffff);self.u.reg_write(UC_X86_REG_ESP,sp+4+cleanup);self.u.reg_write(UC_X86_REG_EIP,target)
 def hook(self,u,at,size,unused):
  if at==STOP:u.emu_stop();return
  if at==0x6232c0:self.ret(MEM+0x2000 if self.dvar else 0);return
  if at==0x70e9f0:self.ret(int(self.room));return
  if at==0x595cf0:self.endpoint_calls+=1;return
  if at not in self.names:return
  name=self.names[at];sp=u.reg_read(UC_X86_REG_ESP);arg=lambda n:self.get(sp+4*n)
  if name=='GetCurrentProcess':self.ret(0xffffffff);return
  if name=='GetTickCount':self.ret(self.now);return
  if name=='GetModuleHandleW':self.ret(0x400000 if not arg(1) else 0x500000 if self.text(arg(1),True).lower()=='d3d9.dll' else 0x700000,4);return
  if name in ('GetSystemDirectoryW','GetSystemWow64DirectoryW'):
   text='C:\\Windows\\'+('SysWOW64' if name=='GetSystemWow64DirectoryW' else 'System32');self.write(arg(1),(text+'\0').encode('utf-16le'));self.ret(len(text),8);return
  if name=='GetModuleFileNameW':
   text=self.proxy_path if arg(1)==0x500000 else 'E:\\AWS Test\\codoMP_client_shipRetail.exe';self.write(arg(2),(text+'\0').encode('utf-16le'));self.ret(len(text),12);return
  if name=='lstrcatW':self.write(arg(1),(self.text(arg(1),True)+self.text(arg(2),True)+'\0').encode('utf-16le'));self.ret(arg(1),8);return
  if name=='GetProcAddress':self.ret(self.address(self.text(arg(2))),8);return
  if name=='ReadProcessMemory':
   address,dest,count,got=arg(2),arg(3),arg(4),arg(5)
   if self.deny is not None and address<=self.deny<address+count:self.ret(0,20);return
   try:content=self.u.mem_read(address,count)
   except Exception:self.ret(0,20);return
   self.write(dest,content)
   if got:self.put(got,count)
   self.ret(1,20);return
  if name=='VirtualProtect':self.protects+=1;self.put(arg(4),0x20);self.ret(int(self.protects not in self.fail_protect),16);return
  if name=='FlushInstructionCache':self.flushes+=1;self.ret(int(self.flushes not in self.fail_flush),12);return
  if name=='TerminateProcess':self.terminated=arg(2);u.emu_stop();return
  if name=='TryEnterCriticalSection':
   if not self.lock_busy:self.lock_depth+=1;self.max_lock_depth=max(self.max_lock_depth,self.lock_depth)
   self.ret(int(not self.lock_busy),4);return
  if name=='EnterCriticalSection':self.lock_depth+=1;self.max_lock_depth=max(self.max_lock_depth,self.lock_depth);self.ret(0,4);return
  if name=='LeaveCriticalSection':self.lock_depth-=1;assert self.lock_depth>=0;self.ret(0,4);return
  if name=='memcpy':self.write(arg(1),self.u.mem_read(arg(2),arg(3)));self.ret(arg(1));return
  if name in ('memcmp','strcmp','_wcsicmp'):
   a=bytes(self.u.mem_read(arg(1),arg(3))) if name=='memcmp' else self.text(arg(1),name=='_wcsicmp')
   b=bytes(self.u.mem_read(arg(2),arg(3))) if name=='memcmp' else self.text(arg(2),name=='_wcsicmp')
   if name=='_wcsicmp':a=a.lower();b=b.lower()
   self.ret((a>b)-(a<b));return
  if name=='_vsnprintf':
   format=self.text(arg(3));values=arg(4)
   if format=='v1|%u|%08x%08x':text='v1|%u|%08x%08x'%(self.get(values),self.get(values+4),self.get(values+8))
   elif format=='set ui_quickNativeRoomContext "%s"\n':text='set ui_quickNativeRoomContext "'+self.text(self.get(values))+'"\n'
   else:raise AssertionError(format)
   encoded=text.encode();assert len(encoded)<arg(2);self.write(arg(1),encoded+b'\0');self.ret(len(encoded));return
  raise AssertionError('Unmodeled API '+name)
 def invoke(self,name):
  sp=STACK+0xf000;self.put(sp,STOP);self.u.reg_write(UC_X86_REG_ESP,sp)
  self.u.emu_start(exports[name] if isinstance(name,str) else name,STOP,count=1000000)
  if self.terminated is not None:return None
  assert self.u.reg_read(UC_X86_REG_EIP)==STOP,(name,hex(self.u.reg_read(UC_X86_REG_EIP)))
  assert self.u.reg_read(UC_X86_REG_ESP)==sp+4
  return self.u.reg_read(UC_X86_REG_EAX)
 def command(self):return self.text(MEM+0x3000)
 def consume(self,ack=True):
  text=self.command();value=text.split('"')[1] if '"' in text else ''
  if ack:self.write(MEM+0x2100,value.encode()+b'\0')
  self.put(0x25094e4,0);self.write(MEM+0x3000,b'\0')

class Binder(unittest.TestCase):
 def test_trusted_d3d_paths_install_once_and_chain_native_tick(self):
  for path in ['C:\\Windows\\System32\\d3d9.dll','C:\\Windows\\SysWOW64\\d3d9.dll']:
   m=Machine();m.proxy_path=path;self.assertEqual(m.invoke('Install'),1);self.assertEqual(m.invoke('Build'),1)
   hook=m.get(exports['hook_address']);expected=b'\xe8'+struct.pack('<i',hook-SITE-5)
   self.assertEqual(bytes(m.u.mem_read(SITE,5)),expected);self.assertEqual(m.invoke('Next'),0x595cf0)
   m.invoke(hook);self.assertEqual(m.endpoint_calls,1);self.assertEqual(m.invoke('Serial'),1);self.assertEqual(m.lock_depth,0)
 def test_tick_wrapper_preserves_registers_flags_and_original_endpoint(self):
  m=Machine();self.assertEqual(m.invoke('Install'),1)
  registers=[UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
  wanted=[0x11223344,0x22334455,0x33445566,0x44556677]
  for reg,value in zip(registers,wanted):m.u.reg_write(reg,value)
  m.u.reg_write(UC_X86_REG_EFLAGS,0x246);flags=m.u.reg_read(UC_X86_REG_EFLAGS)
  m.invoke(m.get(exports['hook_address']))
  self.assertEqual([m.u.reg_read(reg) for reg in registers],wanted)
  self.assertEqual(m.u.reg_read(UC_X86_REG_EFLAGS),flags);self.assertEqual(m.endpoint_calls,1)
 def test_local_proxy_bad_identity_and_foreign_tick_never_patch(self):
  for kind in ['proxy','identity','foreign']:
   m=Machine()
   if kind=='proxy':m.proxy_path='E:\\AWS Test\\d3d9.dll'
   elif kind=='identity':m.put(exports['fixture_image'],0)
   else:m.write(SITE,b'\xe8'+struct.pack('<i',0x12345678-SITE-5))
   before=bytes(m.u.mem_read(SITE,5));self.assertEqual(m.invoke('Install'),0);self.assertEqual(m.protects,0);self.assertEqual(bytes(m.u.mem_read(SITE,5)),before)
 def test_every_startup_guard_byte_mutation_is_rejected(self):
  for name,address,body in guards:
   if name.endswith('CbufLockBody'):continue
   for index in range(len(body)):
    with self.subTest(guard=name,offset=index):
     m=Machine();m.write(address+index,bytes([body[index]^1]));self.assertEqual(m.invoke('Install'),0);self.assertEqual(m.protects,0);self.assertEqual(bytes(m.u.mem_read(SITE,5)),m.original)
 def test_install_faults_restore_original_and_irrecoverable_rollback_aborts(self):
  for protect,flush,aborted in [({1},set(),False),({2},set(),False),(set(),{1},False),({2,3},set(),True),({2,4},set(),True),(set(),{1,2},True)]:
   with self.subTest(protect=protect,flush=flush):
    m=Machine();m.fail_protect=protect;m.fail_flush=flush;result=m.invoke('Install')
    self.assertEqual(m.terminated is not None,aborted)
    if not aborted:self.assertEqual(result,0);self.assertEqual(bytes(m.u.mem_read(SITE,5)),m.original)
 def test_actual_retained_queue_ack_backpressure_and_same_lock_recovery(self):
  m=Machine();self.assertEqual(m.invoke('Install'),1);m.invoke('Tick');self.assertEqual(m.invoke('Serial'),1)
  self.assertEqual(m.command(),'set ui_quickNativeRoomContext "v1|1|0000567800001234"\n')
  used=m.get(0x25094e4);m.now=250;m.invoke('Tick');self.assertEqual(m.get(0x25094e4),used)
  m.now=2000;m.invoke('Tick');self.assertEqual(m.get(0x25094e4),used);self.assertEqual(m.max_lock_depth,1)
  m.consume(False);m.now=2250;m.invoke('Tick');self.assertEqual(m.invoke('Serial'),1);self.assertEqual(m.max_lock_depth,2);self.assertEqual(m.lock_depth,0)
  m.consume(True);m.now=2500;m.invoke('Tick');self.assertEqual(m.invoke('Serial'),2)
 def test_every_recovery_body_byte_and_lock_import_mutation_denies_retry(self):
  for name,address,body in guards:
   if not name.endswith('CbufLockBody'):continue
   for index in range(len(body)):
    with self.subTest(offset=index):
     m=Machine();self.assertEqual(m.invoke('Install'),1);m.invoke('Tick');m.consume(False)
     m.write(address+index,bytes([body[index]^1]));m.now=2000;m.invoke('Tick')
     self.assertEqual(m.get(0x25094e4),0);self.assertEqual(m.lock_depth,0);self.assertEqual(m.invoke('Serial'),1)
  for address in [0xb421ec,0xb421e8]:
   m=Machine();self.assertEqual(m.invoke('Install'),1);m.invoke('Tick');m.consume(False)
   m.put(address,0);m.now=2000;m.invoke('Tick');self.assertEqual(m.get(0x25094e4),0);self.assertEqual(m.lock_depth,0)
 def test_unavailable_room_unreadable_state_and_saturated_queue_fail_closed(self):
  for kind in ['dvar-type','missing-count','room-gone','capacity','busy-lock']:
   with self.subTest(kind=kind):
    m=Machine();self.assertEqual(m.invoke('Install'),1)
    if kind=='dvar-type':m.write(MEM+0x2008,b'\x05')
    elif kind=='missing-count':m.deny=0x2ab98c4
    elif kind=='room-gone':m.room=False
    elif kind=='capacity':m.put(0x25094e4,65535)
    else:
     m.invoke('Tick');m.consume(False);m.lock_busy=True;m.now=2000
    before=m.get(0x25094e4);m.invoke('Tick');self.assertEqual(m.get(0x25094e4),before);self.assertEqual(m.lock_depth,0)

if __name__=='__main__':
 result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Binder))
 report={'passed':result.wasSuccessful(),'methods':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),
         'fixture_sha256':hashlib.sha256(data).hexdigest(),'image_sha256':image_sha,
         'source_sha256':{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in ['src/kernel/kernel_room_context.c','src/kernel/kernel_room_guards.h','src/kernel/kernel_room_policy.h','src/kernel/kernel_call_guard.c']},
         'boundary':'Compiled binder and actual Cbuf_AddText x86; explicit Windows/CRT/hash/room/dvar doubles. No actual game/Windows acceptance.'}
 args.output.write_text(json.dumps(report,indent=2)+'\n');raise SystemExit(not result.wasSuccessful())
