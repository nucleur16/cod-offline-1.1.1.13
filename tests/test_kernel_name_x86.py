"""Execute the compiled C migration and legacy thunks in Unicorn, not Windows.

Native guard bytes come from the owned exact C1 image. Windows, HTTP, TLS,
Unicode conversion and GFx are explicit doubles; actual Windows/gameplay and
remote-service qualification are separate release gates.
"""
from pathlib import Path
import argparse,hashlib,json,re,struct,time,unittest
if __name__!='__main__':raise unittest.SkipTest('Explicit private-image name migration proof')
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
ap=argparse.ArgumentParser();ap.add_argument('--fixture',type=Path,required=True);ap.add_argument('--image',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
args,remaining=ap.parse_known_args();raw=args.image.read_bytes();image_sha=hashlib.sha256(raw).hexdigest()
assert image_sha=='a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976'
image=pefile.PE(data=raw);pe=pefile.PE(str(args.fixture));base=pe.OPTIONAL_HEADER.ImageBase
exports={s.name.decode():base+s.address for s in pe.DIRECTORY_ENTRY_EXPORT.symbols}
STACK=0x30000000;STOP=STACK+0x1fff0;STUB=0x10000000;MEM=0x6000000
ROOT=Path(__file__).resolve().parents[1]
header=(ROOT/'src/kernel/kernel_name_guards.h').read_text();arrays={name:bytes(int(v,16) for v in re.findall(r'0x([0-9a-f]+)',body)) for name,body in re.findall(r'static const unsigned char (\w+)\[\]=\{([^}]+)\};',header)}
guards=[(int(address,16),arrays[name]) for address,name in re.findall(r'\{(0x[0-9a-f]+)u,(\w+),sizeof',header)]
assert len(guards)==16
patch_guards=[guards[i] for i in (4,5,6,7,8,10,11,12,13)]
REGS=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
class Machine:
 def __init__(self):
  self.u=Uc(UC_ARCH_X86,UC_MODE_32);self.pages=set();self.names={};self.api={}
  self.map(base,pe.OPTIONAL_HEADER.SizeOfImage);self.write(base,pe.get_memory_mapped_image());self.map(STACK,0x20000);self.map(STUB,0x10000);self.map(MEM,0x20000)
  self.now=0;self.protects=0;self.protections={};self.flushes=0;self.fail_protect=set();self.fail_flush=set();self.fail_thread=False;self.terminated=None;self.fls=0;self.allocated=MEM+0x10000;self.worker=0
  self.fls_callback=0;self.fail_fls_alloc=False;self.fail_fls_set=False;self.fail_alloc=False;self.allocations={};self.frees=[];self.slot_frees=0;self.fail_pin=False;self.pinned=False
  self.body=b'{"identity":"Alpha","uid":"42","name":"\\u4e2d\\u6587"}';self.status=200;self.response_at=0;self.request_path='';self.http_calls=[];self.http_fail=None;self.http_advance=0;self.closed=[]
  self.native_calls=[];self.lock=0;self.deny=None;self.setevent=True;self.events=0;self.waits=0
  self.files={};self.file_opens=0;self.file_fail=None;self.file_handles={};self.renames=[];self.deleted=[]
  for lib in pe.DIRECTORY_ENTRY_IMPORT:
   for item in lib.imports:self.put(item.address,self.address(item.name.decode()))
  for at,body in guards:
   assert image.get_data(at-0x400000,len(body))==body,hex(at)
   self.map(at,len(body));self.write(at,body)
  for at,size in [(0xcea9f8,8),(0x6c44f27,1),(0x6c44f58,8),(0x6c26318,4),(0x2800a80,4),(0x6c44d6c,1),(0x810c2a4,4),(0x810c50c,4),(0x448e10,1),(0x488af0,1)]:self.map(at,size)
  self.write(0xcea9f8,struct.pack('<Q',42));self.put(0x6c26318,MEM+0x100);self.put(MEM+0x10c,MEM+0x200);self.write(MEM+0x200,b'Alpha\0')
  self.put(0x2800a80,MEM+0x300);self.put(MEM+0x30c,11);self.put(0x810c2a4,0x1234);self.put(0x810c50c,0x5678)
  self.u.hook_add(UC_HOOK_CODE,self.hook)
 def map(self,at,size):
  for p in range(at&~4095,(at+size+4095)&~4095,4096):
   if p not in self.pages:self.u.mem_map(p,4096);self.pages.add(p)
 def write(self,at,data):self.u.mem_write(at,bytes(data))
 def put(self,at,n):self.write(at,struct.pack('<I',n&0xffffffff))
 def get(self,at):return struct.unpack('<I',self.u.mem_read(at,4))[0]
 def text(self,at,wide=False):
  out=bytearray();width=2 if wide else 1
  for i in range(8192):
   b=bytes(self.u.mem_read(at+i*width,width))
   if b==b'\0'*width:return out.decode('utf-16le') if wide else bytes(out)
   out.extend(b)
  raise AssertionError('unterminated string')
 def string(self,text,at=MEM+0x1000):self.write(at,text.encode() +b'\0' if isinstance(text,str) else text+b'\0');return at
 def address(self,name):
  if name not in self.api:
   at=STUB+16*len(self.api);self.api[name]=at;self.names[at]=name
  return self.api[name]
 def ret(self,n=0,cleanup=0):
  sp=self.u.reg_read(UC_X86_REG_ESP);target=self.get(sp);self.u.reg_write(UC_X86_REG_EAX,n&0xffffffff);self.u.reg_write(UC_X86_REG_ESP,sp+4+cleanup);self.u.reg_write(UC_X86_REG_EIP,target)
 def hook(self,u,at,size,unused):
  if at==STOP:u.emu_stop();return
  if at in (0x5aa786,0x695990,0x60ad9a,0x60ade7):u.emu_stop();return
  if at in (0x5c7340,0x744aa0,0x448e10,0x488af0):
   self.native_calls.append((at,{r:u.reg_read(r) for r in REGS},u.reg_read(UC_X86_REG_ESP)))
   if at==0x5c7340:self.ret(u.reg_read(UC_X86_REG_ESI));return
   if at==0x744aa0:self.ret(0,8);return
   if at==0x488af0:self.ret(1);return
   self.ret(0);return
  if at not in self.names:return
  name=self.names[at];sp=u.reg_read(UC_X86_REG_ESP);arg=lambda n:self.get(sp+4*n)
  if name=='GetCurrentProcess':self.ret(0xffffffff);return
  if name=='GetCurrentProcessId':self.ret(0x12345678);return
  if name=='GetTickCount':self.ret(self.now);return
  if name=='GetModuleFileNameW':t='E:\\AWS Test\\codoMP_client_shipRetail.exe';self.write(arg(2),(t+'\0').encode('utf-16le'));self.ret(len(t),12);return
  if name=='GetModuleHandleW':self.ret(0x400000,4);return
  if name=='GetModuleHandleExW':
   assert arg(1)==5 and base<=arg(2)<base+pe.OPTIONAL_HEADER.SizeOfImage
   if not self.fail_pin:self.put(arg(3),base);self.pinned=True
   self.ret(int(not self.fail_pin),12);return
  if name=='ReadProcessMemory':
   try:
    if self.deny is not None and arg(2)<=self.deny<arg(2)+arg(4):raise ValueError()
    self.write(arg(3),u.mem_read(arg(2),arg(4)))
   except Exception:self.ret(0,20);return
   if arg(5):self.put(arg(5),arg(4))
   self.ret(1,20);return
  if name=='VirtualQuery':
   page=arg(1)&~4095
   if page not in self.pages:self.ret(0,12);return
   self.write(arg(2),struct.pack('<7I',page,page,4,4096,0x1000,4,0x20000));self.ret(28,12);return
  if name=='VirtualProtect':
   self.protects+=1
   if self.protects in self.fail_protect:self.ret(0,16);return
   page=arg(1)&~4095;self.put(arg(4),self.protections.get(page,0x20))
   for p in range(page,(arg(1)+arg(2)+4095)&~4095,4096):self.protections[p]=arg(3)
   self.ret(1,16);return
  if name=='FlushInstructionCache':self.flushes+=1;self.ret(int(self.flushes not in self.fail_flush),12);return
  if name=='TerminateProcess':self.terminated=arg(2);u.emu_stop();return
  if name=='FlsAlloc':assert self.pinned;self.fls_callback=arg(1);self.ret(0xffffffff if self.fail_fls_alloc else 7,4);return
  if name=='FlsFree':assert not self.fls;self.slot_frees+=1;self.fls_callback=0;self.ret(1,4);return
  if name=='FlsGetValue':self.ret(self.fls,4);return
  if name=='FlsSetValue':
   if not self.fail_fls_set:self.fls=arg(2)
   self.ret(int(not self.fail_fls_set),8);return
  if name=='GetProcessHeap':self.ret(1);return
  if name=='HeapAlloc':
   if self.fail_alloc:self.ret(0,12);return
   out=self.allocated;self.allocations[out]=arg(3);self.allocated+=(arg(3)+15)&~15;self.ret(out,12);return
  if name=='HeapFree':self.frees.append(arg(3));assert arg(3) in self.allocations;del self.allocations[arg(3)];self.ret(1,12);return
  if name=='CreateEventW':self.ret(33,16);return
  if name=='CreateThread':self.worker=arg(3);self.ret(0 if self.fail_thread else 34,24);return
  if name=='CloseHandle':self.ret(1,4);return
  if name in ('lstrcpyW','lstrcatW'):
   text=(self.text(arg(1),True) if name=='lstrcatW' else '')+self.text(arg(2),True);self.write(arg(1),(text+'\0').encode('utf-16le'));self.ret(arg(1),8);return
  if name=='lstrlenW':self.ret(len(self.text(arg(1),True)),4);return
  if name=='wcsrchr':
   text=self.text(arg(1),True);offset=text.rfind(chr(arg(2)));self.ret(arg(1)+offset*2 if offset>=0 else 0);return
  if name=='CreateFileW':
   self.file_opens+=1;path=self.text(arg(1),True);read=arg(5)==3
   if self.file_fail=='open' or read and path not in self.files:self.ret(0xffffffff,28);return
   handle=44 if read else 45;self.file_handles[handle]=path
   if not read:self.files[path]=b''
   self.ret(handle,28);return
  if name=='ReadFile':
   content=self.files[self.file_handles[arg(1)]][:arg(3)];self.write(arg(2),content);self.put(arg(4),len(content));self.ret(int(self.file_fail!='read'),20);return
  if name=='WriteFile':
   content=bytes(u.mem_read(arg(2),arg(3)));self.files[self.file_handles[arg(1)]]=content;self.put(arg(4),len(content)-(self.file_fail=='short-write'));self.ret(int(self.file_fail!='write'),20);return
  if name=='FlushFileBuffers':self.ret(int(self.file_fail!='flush'),4);return
  if name=='MoveFileExW':
   source,target=self.text(arg(1),True),self.text(arg(2),True);self.renames.append((source,target,arg(3)))
   if self.file_fail=='rename':self.ret(0,12);return
   self.files[target]=self.files.pop(source);self.ret(1,12);return
  if name=='DeleteFileW':path=self.text(arg(1),True);self.deleted.append(path);self.files.pop(path,None);self.ret(1,4);return
  if name=='SetEvent':self.events+=1;self.ret(int(self.setevent),4);return
  if name=='WaitForSingleObject':self.waits+=1;self.ret(0 if self.waits==1 else 0xffffffff,8);return
  if name.startswith('AcquireSRWLock'):self.lock+=1;self.ret(0,4);return
  if name.startswith('ReleaseSRWLock'):self.lock-=1;assert self.lock>=0;self.ret(0,4);return
  if name in ('memcpy','memset'):
   self.write(arg(1),u.mem_read(arg(2),arg(3)) if name=='memcpy' else bytes([arg(2)&255])*arg(3));self.ret(arg(1));return
  if name=='memchr':
   b=bytes(u.mem_read(arg(1),arg(3)));offset=b.find(bytes([arg(2)&255]));self.ret(arg(1)+offset if offset>=0 else 0);return
  if name=='strlen':self.ret(len(self.text(arg(1))));return
  if name=='strstr':offset=self.text(arg(1)).find(self.text(arg(2)));self.ret(arg(1)+offset if offset>=0 else 0);return
  if name in ('memcmp','strcmp'):
   a=bytes(u.mem_read(arg(1),arg(3))) if name=='memcmp' else self.text(arg(1));b=bytes(u.mem_read(arg(2),arg(3))) if name=='memcmp' else self.text(arg(2));self.ret((a>b)-(a<b));return
  if name=='_vsnprintf':
   assert self.text(arg(3))==b'%llu';n=struct.unpack('<Q',u.mem_read(arg(4),8))[0];s=str(n).encode();self.write(arg(1),s+b'\0');self.ret(len(s));return
  if name in ('MultiByteToWideChar','WideCharToMultiByte'):
   try:
    source,count=arg(3),arg(4);wide=name=='WideCharToMultiByte'
    content=self.text(source,wide) if count==0xffffffff else bytes(u.mem_read(source,count*(2 if wide else 1))).decode('utf-16le') if wide else bytes(u.mem_read(source,count))
    if not wide:content=content.decode('utf8')
    result=content.encode('cp936' if wide else 'utf-16le')+(b'\0' if wide else b'\0\0')*(count==0xffffffff)
    out_length=len(result)//(1 if wide else 2)
    if out_length>arg(6):raise ValueError()
    self.write(arg(5),result)
    if wide and arg(8):self.put(arg(8),0)
    self.ret(out_length,32 if wide else 24)
   except (UnicodeError,ValueError):self.ret(0,32 if wide else 24)
   return
  if name.startswith('WinHttp'):
   self.http_calls.append(name);self.now=(self.now+self.http_advance)&0xffffffff
   if name=='WinHttpCloseHandle':self.closed.append(arg(1));self.ret(1,4);return
   widths={'WinHttpOpen':20,'WinHttpSetTimeouts':20,'WinHttpConnect':16,'WinHttpOpenRequest':28,'WinHttpSetOption':16,'WinHttpSendRequest':28,'WinHttpReceiveResponse':8,'WinHttpQueryHeaders':24,'WinHttpReadData':16}
   if name==self.http_fail:self.ret(0,widths[name]);return
   if name=='WinHttpOpen':self.ret(1,20);return
   if name=='WinHttpConnect':assert self.text(arg(2),True)=='codol.kewkew.org' and arg(3)==443;self.ret(2,16);return
   if name=='WinHttpOpenRequest':self.request_path=self.text(arg(3),True);assert arg(7)&0x800000;self.response_at=0;self.ret(3,28);return
   if name=='WinHttpSetOption':assert arg(2)==88 and self.get(arg(3))==0;self.ret(1,16);return
   if name=='WinHttpQueryHeaders':self.put(arg(4),self.status);self.ret(1,24);return
   if name=='WinHttpReadData':
    b=self.body[self.response_at:self.response_at+arg(3)];self.response_at+=len(b);self.write(arg(2),b);self.put(arg(4),len(b));self.ret(1,16);return
   self.ret(1,widths[name]);return
  raise AssertionError('Unmodeled API '+name)
 def invoke(self,name,*values,callee_cleanup=0):
  sp=STACK+0xf000;self.put(sp,STOP)
  for i,value in enumerate(values):self.put(sp+4+4*i,value)
  self.u.reg_write(UC_X86_REG_ESP,sp);self.u.emu_start(exports[name] if isinstance(name,str) else name,STOP,count=1000000)
  if self.terminated is not None:return None
  assert self.u.reg_read(UC_X86_REG_EIP)==STOP,(name,hex(self.u.reg_read(UC_X86_REG_EIP)))
  assert self.u.reg_read(UC_X86_REG_ESP)==sp+4+callee_cleanup
  assert self.lock==0
  return self.u.reg_read(UC_X86_REG_EAX)
 def seed(self,alias='中文'):
  self.invoke('Seed',self.string('Alpha',MEM+0x1000),42,0,self.string(alias,MEM+0x1100),self.now)
 def thread_exit(self):
  value=self.fls;self.fls=0
  if value:self.invoke(self.fls_callback,value,callee_cleanup=4)
 def original(self):return [bytes(self.u.mem_read(at,len(body))) for at,body in patch_guards]

class Tests(unittest.TestCase):
 def test_install_exact_guard_and_every_guard_byte_mutation(self):
  m=Machine();self.assertEqual(m.invoke('Install'),1);self.assertTrue(m.worker);self.assertEqual(m.protects,18);self.assertEqual(set(m.protections.values()),{0x20})
  for index,(at,body) in enumerate(patch_guards):
   replacement=bytes(m.u.mem_read(at,len(body)));target=(at+5+struct.unpack('<i',replacement[1:5])[0])&0xffffffff
   self.assertEqual(target,m.get(exports['hooks']+index*4));self.assertEqual(replacement[0],0xe9 if index in (0,3,4) else 0xe8)
  for at,body in guards:
   for offset,byte in enumerate(body):
    m=Machine();m.write(at+offset,bytes([byte^1]));self.assertEqual(m.invoke('Install'),0);self.assertEqual(m.protects,0)
 def test_install_transaction_faults_restore_every_original_site(self):
  for fail in range(1,19):
   m=Machine();before=m.original();m.fail_protect={fail};self.assertEqual(m.invoke('Install'),0);self.assertEqual(m.original(),before);self.assertTrue(all(p==0x20 for p in m.protections.values()))
  for mode in ('flush','worker'):
   m=Machine();before=m.original();m.fail_flush={1} if mode=='flush' else set();m.fail_thread=mode=='worker'
   self.assertEqual(m.invoke('Install'),0);self.assertEqual(m.original(),before);self.assertEqual(set(m.protections.values()),{0x20})
  m=Machine();m.fail_flush={1,2};self.assertIsNone(m.invoke('Install'));self.assertEqual(m.terminated,0x4b4e4402)
 def test_exact_local_identity_ttl_clan_and_uid_fallback(self):
  m=Machine();self.assertEqual(m.invoke('Install'),1);m.seed();source=m.string('Alpha');self.assertEqual(m.text(m.invoke('Alias',source)),'中文'.encode())
  self.assertEqual(m.invoke('Alias',m.string('Other')),MEM+0x1000)
  m.write(MEM+0x400,struct.pack('<Q',42));self.assertEqual(m.text(m.invoke('Member',m.string('[TAG] Alpha'),MEM+0x400)),'[TAG] 中文'.encode())
  m.write(MEM+0x400,struct.pack('<Q',43));self.assertEqual(m.invoke('Member',MEM+0x1000,MEM+0x400),MEM+0x1000)
  m.write(0xcea9f8,b'\0'*8);m.write(0x6c44f27,b'\1');m.write(0x6c44f58,struct.pack('<Q',42));self.assertEqual(m.text(m.invoke('Alias',m.string('Alpha'))),'中文'.encode())
  m.now=60000;self.assertEqual(m.invoke('Alias',MEM+0x1000),MEM+0x1000)
 def test_fls_exit_destructor_churn_and_failed_allocation_assignment(self):
  m=Machine();m.map(MEM,0x100000);self.assertEqual(m.invoke('Install'),1);m.seed();self.assertTrue(m.fls_callback)
  for thread in range(128):
   source=m.string('Alpha');self.assertEqual(m.text(m.invoke('Alias',source)),'中文'.encode());self.assertEqual(sum(m.allocations.values()),1088)
   self.assertEqual(m.text(m.invoke('Alias',source)),'中文'.encode());self.assertEqual(len(m.allocations),1)
   m.thread_exit();self.assertEqual(m.allocations,{});self.assertEqual(len(m.frees),thread+1)
  m.invoke(m.fls_callback,0,callee_cleanup=4);self.assertEqual(len(m.frees),128)
  for failure in ('allocation','assignment'):
   m=Machine();m.invoke('Install');m.seed();m.fail_alloc=failure=='allocation';m.fail_fls_set=failure=='assignment';source=m.string('Alpha')
   self.assertEqual(m.invoke('Alias',source),source);self.assertEqual(m.allocations,{});self.assertEqual(len(m.frees),int(failure=='assignment'));self.assertEqual(m.fls,0);m.thread_exit();self.assertEqual(len(m.frees),int(failure=='assignment'))
  m=Machine();m.fail_fls_alloc=True;self.assertEqual(m.invoke('Install'),0);self.assertEqual(m.protects,0);self.assertEqual(m.slot_frees,0)
  m=Machine();m.fail_thread=True;self.assertEqual(m.invoke('Install'),0);self.assertEqual(m.slot_frees,1);self.assertEqual(m.fls_callback,0);self.assertEqual(m.allocations,{})
  self.assertEqual(m.invoke('Install'),0);self.assertEqual(m.slot_frees,2);self.assertEqual(m.fls_callback,0);self.assertEqual(m.frees,[])
  m=Machine();m.fail_pin=True;self.assertEqual(m.invoke('Install'),0);self.assertEqual(m.fls_callback,0);self.assertEqual(m.protects,0)
 def test_chinese_native_buffers_and_english_unchanged(self):
  m=Machine();m.invoke('Install');m.seed();text=m.string(b'\x14MP_CONNECTED\x15'+ '中文'.encode());expected=b'\x14MP_CONNECTED\x15'+'中文'.encode('cp936')
  self.assertEqual(m.text(m.invoke('HUD',text)),expected);self.assertEqual(m.text(text),b'\x14MP_CONNECTED\x15'+'中文'.encode())
  for text in (b'\x14OTHER\x15'+'中文'.encode(),b'\x14MP_CONNECTED\x16'+'中文'.encode(),b'\x14MP_CONNECTED\x15ASCII'):
   source=m.string(text);self.assertEqual(m.invoke('HUD',source),source)
  source=m.string('中文');m.invoke('Obituary',source);self.assertEqual(m.text(source),'中文'.encode('cp936'))
  m.put(MEM+0x30c,0);source=m.string('中文');m.invoke('Obituary',source);self.assertEqual(m.text(source),'中文'.encode())
  m.put(MEM+0x30c,11);row=MEM+0x400;m.write(row,struct.pack('<Q',42));m.string('Alpha',row+8)
  self.assertEqual(m.invoke('Lobby',row+8,MEM+0x600),1);self.assertEqual(m.text(MEM+0x600),'中文'.encode('cp936'));self.assertEqual(m.text(row+8),b'Alpha')
 def test_main_thread_sampling_worker_backpressure_refresh_and_identity_invalidation(self):
  m=Machine();m.invoke('Install');m.invoke('Tick');self.assertEqual(m.events,1);self.assertEqual(m.invoke('Busy'),1);self.assertEqual(m.invoke('Pending'),1)
  for now in (1,249,250,10000,30000):m.now=now;m.invoke('Tick')
  self.assertEqual(m.events,1);self.assertEqual(m.invoke('WorkerOnce'),0);self.assertEqual(m.invoke('Busy'),0);self.assertEqual(m.invoke('Pending'),0)
  self.assertEqual(m.text(m.invoke('Alias',m.string('Alpha'))),'中文'.encode())
  m.now=30250;m.invoke('Tick');self.assertEqual(m.events,2)
  m.string('Other',MEM+0x200);m.now=30500;m.invoke('Tick');self.assertEqual(m.invoke('Alias',m.string('Alpha')),MEM+0x1000)
  m.waits=0;m.invoke('WorkerOnce');self.assertEqual(m.invoke('Alias',MEM+0x1000),MEM+0x1000)
  m=Machine();m.invoke('Install');m.setevent=False;m.invoke('Tick');self.assertEqual(m.invoke('Busy'),0)
  m.now=999;m.invoke('Tick');self.assertEqual(m.events,1);m.now=1249;m.setevent=True;m.invoke('Tick');self.assertEqual(m.events,2)
 def test_theatre_generation_transition_zero_wrap_rejects_pending_name_reply(self):
  # The worker keeps its original request identity. A newer Theatre generation
  # invalidates that reply even when canonical name and UID are unchanged.
  for previous,current in ((0,1),(1,0),(0xffffffff,0),(0,0xffffffff)):
   with self.subTest(previous=previous,current=current):
    m=Machine();self.assertEqual(m.invoke('Install'),1)
    m.invoke('TheatreSession',previous);m.invoke('Tick');self.assertEqual(m.events,1)
    m.invoke('TheatreSession',current);m.now=250;m.invoke('Tick')
    m.waits=0;self.assertEqual(m.invoke('WorkerOnce'),0)
    source=m.string('Alpha');self.assertEqual(m.invoke('Alias',source),source)
    m.now=500;m.invoke('Tick');self.assertEqual(m.events,2)
    m.waits=0;self.assertEqual(m.invoke('WorkerOnce'),0)
    self.assertEqual(m.text(m.invoke('Alias',m.string('Alpha'))),'中文'.encode())

 def test_unchanged_zero_and_max_theatre_generation_keep_pending_name_reply(self):
  # Zero is a real initial/wrapped generation, not an unavailable sentinel.
  for generation in (0,1,0xffffffff):
   with self.subTest(generation=generation):
    m=Machine();self.assertEqual(m.invoke('Install'),1)
    m.invoke('TheatreSession',generation);m.invoke('Tick')
    m.invoke('TheatreSession',generation);m.now=250;m.invoke('Tick')
    self.assertEqual(m.events,1);self.assertEqual(m.invoke('WorkerOnce'),0)
    self.assertEqual(m.text(m.invoke('Alias',m.string('Alpha'))),'中文'.encode())

 def test_utf8_conversion_native_custom_abi_and_gfx_thiscall(self):
  m=Machine();source=m.string('ASCII');out=MEM+0x500;m.u.reg_write(UC_X86_REG_ESI,0xabcdef);self.assertEqual(m.invoke('Convert',out,source),out)
  at,registers,sp=m.native_calls[-1];self.assertEqual(at,0x5c7340);self.assertEqual(registers[UC_X86_REG_ESI],out);self.assertEqual(m.get(sp+4),source);self.assertEqual(m.u.reg_read(UC_X86_REG_ESI),0xabcdef)
  source=m.string('中文');m.write(out,b'\xff'*8);self.assertEqual(m.invoke('Convert',out,source),out);at,registers,sp=m.native_calls[-1]
  self.assertEqual(at,0x744aa0);self.assertEqual(registers[UC_X86_REG_ECX],0x1234);self.assertEqual(m.get(sp+4),out);self.assertEqual(m.get(sp+8),source);self.assertEqual(bytes(m.u.mem_read(out,8)),b'\0'*8)
 def test_all_legacy_thunks_preserve_stack_registers_flags_and_continuations(self):
  for index in (0,1,3,4,5,7):
   m=Machine();m.invoke('Install');m.seed();sp=STACK+0xf000;frame=STACK+0xd000
   source=m.string('Alpha',MEM+0x1000);row=MEM+0x400;m.write(row,struct.pack('<Q',42));m.string('Alpha',row+8)
   initial=[MEM+0x100,row+8,source,0x11110000,MEM+0x600,row+8,frame]
   for register,value in zip(REGS,initial):m.u.reg_write(register,value)
   m.u.reg_write(UC_X86_REG_EFLAGS,0x246);m.u.reg_write(UC_X86_REG_ESP,sp);m.put(sp,STOP);m.put(sp+4,source)
   m.put(frame-100,source);m.put(frame-32,0x87654321);m.put(frame-36,0x12345678)
   if index==5:m.u.reg_write(UC_X86_REG_ECX,m.string(b'\x14MP_CONNECTED\x15'+'中文'.encode(),MEM+0x1200))
   if index==7:m.string('中文',MEM+0x600)
   m.u.emu_start(m.get(exports['hooks']+index*4),STOP,count=1000000)
   end=m.u.reg_read(UC_X86_REG_EIP);after=[m.u.reg_read(r) for r in REGS]
   if index!=1:self.assertEqual(m.u.reg_read(UC_X86_REG_EFLAGS)&0xcd5,0x246&0xcd5,index)
   if index==0:
    self.assertEqual(end,0x5aa786);self.assertEqual(m.u.reg_read(UC_X86_REG_ESP),sp-8);self.assertEqual(m.get(sp-4),63);self.assertEqual(m.text(m.get(sp-8)),'中文'.encode())
    expected=initial[:];expected[2]=m.get(sp-8);self.assertEqual(after,expected)
   elif index==3:
    self.assertEqual(end,0x695990);self.assertEqual(m.u.reg_read(UC_X86_REG_ESP),sp);self.assertEqual(m.text(m.get(frame-100)),'中文'.encode());expected=initial[:];expected[2:4]=[0x12345678,0x87654321];self.assertEqual(after,expected)
   elif index==4:
    self.assertEqual(end,0x60ade7);self.assertEqual(m.u.reg_read(UC_X86_REG_ESP),sp);self.assertEqual(m.text(frame-80),'中文'.encode('cp936'));expected=initial[:];expected[3]=MEM+0x300;self.assertEqual(after,expected)
   elif index==5:
    self.assertEqual(end,STOP);self.assertEqual(m.u.reg_read(UC_X86_REG_ESP),sp+4);native=m.native_calls[-1];self.assertEqual(native[0],0x448e10);self.assertEqual(m.text(native[1][UC_X86_REG_ECX]),b'\x14MP_CONNECTED\x15'+'中文'.encode('cp936'))
   elif index==7:
    self.assertEqual(end,STOP);self.assertEqual(m.u.reg_read(UC_X86_REG_ESP),sp+4);expected=initial[:];expected[0]=1;self.assertEqual(after,expected);self.assertEqual(m.text(MEM+0x600),'中文'.encode('cp936'))
   else:
    self.assertEqual(end,STOP);self.assertEqual(m.u.reg_read(UC_X86_REG_ESP),sp+4);self.assertEqual(after[4:],initial[4:]);self.assertEqual(after[0],MEM+0x600)
 def test_http_exact_identity_body_budget_failures_and_cleanup(self):
  for failure in (None,'WinHttpOpen','WinHttpSetTimeouts','WinHttpConnect','WinHttpOpenRequest','WinHttpSetOption','WinHttpSendRequest','WinHttpReceiveResponse','WinHttpQueryHeaders','WinHttpReadData'):
   m=Machine();m.http_fail=failure;key=MEM+0x400;m.write(key,b'Alpha\0'+b'\0'*58+struct.pack('<QQI',42,1,0));out=MEM+0x600
   self.assertEqual(m.invoke('Fetch',key,out),int(failure is None));self.assertEqual(m.text(out),'中文'.encode() if failure is None else b'')
   if failure is None:self.assertEqual(m.request_path,'/api/community/display-name?identity=Alpha');self.assertEqual(m.closed,[3,2,1])
  for body in (b'x'*4097,b'{"identity":"Other","uid":"42","name":"Alias"}'):
   m=Machine();m.body=body;m.write(MEM+0x400,b'Alpha\0'+b'\0'*58+struct.pack('<QQI',42,1,0));self.assertEqual(m.invoke('Fetch',MEM+0x400,MEM+0x600),0);self.assertEqual(m.closed,[3,2,1])
  m=Machine();m.http_advance=2000;m.write(MEM+0x400,b'Alpha\0'+b'\0'*58+struct.pack('<QQI',42,1,0));self.assertEqual(m.invoke('Fetch',MEM+0x400,MEM+0x600),0)
 def test_legacy_video_mode_fallback_atomic_write_failures_and_no_frame_io(self):
  path='E:\\AWS Test\\codol_video_mode.cfg'
  for content,expected in [(b'0\n',0),(b'1\n',1),(b'x',1),(b'',1)]:
   m=Machine();m.files[path]=content;self.assertEqual(m.invoke('VideoLoad',1),expected)
   for _ in range(10):self.assertEqual(m.invoke('VideoLoad',1),expected)
   self.assertEqual(m.file_opens,1)
  m=Machine();self.assertEqual(m.invoke('VideoLoad',0),0);m.invoke('VideoSave',1);self.assertEqual(m.files[path],b'1\n');self.assertEqual(m.renames,[(path+'.tmp.12345678',path,9)])
  opens=m.file_opens
  for _ in range(100):m.invoke('VideoSave',1)
  self.assertEqual(m.file_opens,opens);m.invoke('VideoSave',0);self.assertEqual(m.files[path],b'0\n')
  for failure in ('open','write','short-write','flush','rename'):
   m=Machine();m.files[path]=b'0\n';m.invoke('VideoLoad',0);m.file_fail=failure;m.invoke('VideoSave',1);self.assertEqual(m.files[path],b'0\n')
   if failure!='open':self.assertEqual(m.deleted,[path+'.tmp.12345678'])
   opens=m.file_opens
   for now in (1,249,999):m.now=now;m.invoke('VideoSave',1)
   self.assertEqual(m.file_opens,opens);m.now=1000;m.file_fail=None;m.invoke('VideoSave',1);self.assertEqual(m.files[path],b'1\n')

if __name__=='__main__':
 started=time.perf_counter();suite=unittest.defaultTestLoader.loadTestsFromTestCase(Tests);result=unittest.TextTestRunner(verbosity=2).run(suite)
 args.output.write_text(json.dumps({'passed':result.wasSuccessful(),'test_methods':result.testsRun,'elapsed_seconds':time.perf_counter()-started,'fixture_sha256':hashlib.sha256(args.fixture.read_bytes()).hexdigest(),'image_sha256':image_sha,'guard_regions':len(guards),'guard_bytes':sum(len(b) for _,b in guards),'native_instructions_executed_in_emulator':True,'Windows_API_HTTP_GFx_doubles':True,'Windows_binary_launched':False,'Windows_gameplay_qualified':False},indent=2)+'\n')
 raise SystemExit(not result.wasSuccessful())
