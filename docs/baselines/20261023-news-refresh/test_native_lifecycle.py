"""Actual C policy + actual retained x86 dispatcher/start/poll/parser/consume/publisher.
Only OS HTTP transport, allocation and GFx VM boundaries are explicit doubles.
No game process/service/network is opened. Install requirements.txt in a Linux Python environment.
"""
from pathlib import Path
import sys,ctypes as C,struct,json,hashlib
O=Path(__file__).resolve().parent
from common import load_image
import argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--image')
parser.add_argument('--library',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
parser.add_argument('--select')
options=parser.parse_args()
options.out.mkdir(parents=True,exist_ok=True)

from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
b,pe,mapped=load_image(options.image)
base=pe.OPTIONAL_HEADER.ImageBase

R=C.CFUNCTYPE(C.c_uint32,C.c_void_p,C.c_uint32)
W32=C.CFUNCTYPE(None,C.c_void_p,C.c_uint32,C.c_uint32)
READ=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint32,C.c_uint32)
STEP=C.CFUNCTYPE(None,C.c_void_p)
FREE=C.CFUNCTYPE(None,C.c_void_p,C.c_uint32)
class Port(C.Structure):_fields_=[('ctx',C.c_void_p),('readable',READ),('read32',R),('write32',W32),('read8',R),('step',STEP),('release',FREE)]
class Clock(C.Structure):_fields_=[('manager',C.c_uint32),('due',C.c_uint32),('scheduled',C.c_int)]
class Result(C.Structure):_fields_=[(n,C.c_uint32) for n in ('started','completed','replaced','failed')]
lib=C.CDLL(str(options.library.resolve()))
lib.news_test_frame.argtypes=[C.POINTER(Port),C.POINTER(Clock),C.c_uint32,C.c_uint32]
lib.news_test_frame.restype=Result
NR_KIND=0x4e525458;M=0x10000000;N=0x10001000;STACK=0x300ff000;END=0x100ff000;IFACE=0x100f0000;VTABLE=IFACE+0x100
class Fixture:
    def __init__(self):
        self.uc=Uc(UC_ARCH_X86,UC_MODE_32)
        for a,n in [(0x580000,0x100000),(0xa30000,0x10000),(0xad0000,0x20000),(0xb00000,0x200000),(0x6c1b000,0x1000),(0x10000000,0x200000),(0x20000000,0x4000000),(0x30000000,0x100000)]:self.uc.mem_map(a,n)
        for a,n in [(0x580000,0x100000),(0xa30000,0x10000),(0xad0000,0x20000),(0xb00000,0x200000)]:self.uc.mem_write(a,mapped[a-base:a-base+n])
        self.uc.mem_map(0x76d000,0x1000);self.uc.mem_write(0x76d000,mapped[0x76d000-base:0x76e000-base])
        self.uc.mem_map(0x4b4000,0x1000)
        self.uc.hook_add(UC_HOOK_CODE,self.hook)
        self.stubs={0xadd78c:0,0xadc0b8:0,0xae158d:0,0xa3f2d0:32,0x594210:0,0xae04c0:0,0x4b4880:0,0xada54e:0,0xadcc15:0,0x5c7420:0,0x58e650:0,0x5c7950:0,0xad9f0f:0,0x100f1000:16,0x100f1010:8}
        self.callbacks=(READ(self.readable),R(lambda _,a:self.r(a)),W32(lambda _,a,v:self.w(a,v)),R(lambda _,a:self.uc.mem_read(a,1)[0]),STEP(self.step_cb),FREE(self.free_cb))
        self.port=Port(None,*self.callbacks);self.clock=Clock();self.reset()
    def reset(self,language='zh',same_address=False):
        self.uc.mem_write(M,b'\0'*0x10000);self.heap=0x20000000;self.live={};self.structures={};self.freed=[];self.calls=[];self.events=[];self.rendered='initial';self.errors=[];self.pub=0;self.results=[];self.obj={};self.language=language;self.setup_fail=False
        self.w(0x6c1b29c,M);self.w(M,0xc64364);self.w(M+16,0xbde50c);self.w(M+8,7)
        self.w(IFACE,VTABLE);self.w(VTABLE+20,0x100f1000);self.w(VTABLE+8,0x100f1010)
        urls=[f'http://fixture/feed/news?lang={language}','http://fixture/feed/img/adnews/default.png','http://fixture/promotion.png']
        for i,url in enumerate(urls):
            at=N+i*0x100;u=M+0x2000+i*0x100;self.uc.mem_write(u,url.encode()+b'\0');self.w(at,u);self.w(at+4,at+0x100 if i<2 else 0);self.w(at+20,1 if i<2 else 2);self.w(at+24,i)
            payload=self.alloc(b'initial\0' if i==0 else b'PNG-IMMUTABLE-'+bytes([i]));self.w(at+8,payload);self.w(at+12,len(self.live[payload]));self.w(at+16,2 if i==0 else 1)
            self.structures[at]=0x1c;self.structures[u]=len(url.encode())+1
        self.w(M+0x478,N);self.w(M+0x47c,N+0x200);self.w(M+0x484,M+0x100000);self.w(M+0x488,0x100000)
        self.original_images=[(self.r(N+i*0x100+8),bytes(self.live[self.r(N+i*0x100+8)])) for i in (1,2)]
        self.original_image_nodes=[bytes(self.uc.mem_read(N+i*0x100,0x1c)) for i in (1,2)]
        self.structures[M+0x100000]=0x100000
        if not same_address:self.clock=Clock()
    def r(self,a):return struct.unpack('<I',self.uc.mem_read(a,4))[0]
    def w(self,a,v):self.uc.mem_write(a,struct.pack('<I',v&0xffffffff))
    def string(self,a):
        out=bytearray()
        for i in range(4096):
            x=self.uc.mem_read(a+i,1)[0]
            if not x:return bytes(out)
            out.append(x)
        raise AssertionError('unterminated test string')
    def readable(self,_,a,n):
        try:self.uc.mem_read(a,n);return 1
        except Exception:return 0
    def alloc(self,value):
        if isinstance(value,int):value=b'\xcc'*max(1,value)
        at=self.heap;self.heap+=(len(value)+15)&~15;self.live[at]=value;self.uc.mem_write(at,value);return at
    def free(self,a):
        if not a:return
        if a in self.structures:
            size=self.structures.pop(a);self.uc.mem_write(a,b'\xdd'*size);self.freed.append(a);return
        assert a in self.live,('double/unowned free',hex(a));old=self.live.pop(a);self.uc.mem_write(a,b'\xdd'*len(old));self.freed.append(a);self.events.append(('free',a))
    def free_cb(self,_,a):
        try:self.free(a)
        except Exception as e:self.errors.append((repr(e),hex(self.uc.reg_read(UC_X86_REG_EIP)),hex(self.uc.reg_read(UC_X86_REG_ESP))))
    def ret(self,n=0,value=0):
        sp=self.uc.reg_read(UC_X86_REG_ESP);dest=self.r(sp);self.uc.reg_write(UC_X86_REG_EAX,value);self.uc.reg_write(UC_X86_REG_ESP,sp+4+n);self.uc.reg_write(UC_X86_REG_EIP,dest)
    def hook(self,uc,a,size,_):
        if a not in self.stubs:return
        sp=uc.reg_read(UC_X86_REG_ESP);arg=lambda i:self.r(sp+4+4*i);eax=uc.reg_read(UC_X86_REG_EAX);ecx=uc.reg_read(UC_X86_REG_ECX);edx=uc.reg_read(UC_X86_REG_EDX);value=0
        if a==0xadd78c:value=self.alloc(arg(0))
        elif a==0xadc0b8:self.free(arg(0))
        elif a==0xae158d:value=0 if self.string(arg(0)).lower()==self.string(arg(1)).lower() else 1
        elif a==0xa3f2d0:
            assert not self.r(M+0x2c),'overlapping native HTTP start'
            url=self.string(arg(0)).decode();self.calls.append(url);self.events.append(('http',url));self.w(M+0x28,1);self.w(M+0x2c,2)
            if self.setup_fail:self.w(M+0x28,5);self.w(M+0x2c,0)
        elif a==0xae04c0:uc.mem_write(arg(0),bytes([arg(1)&255])*arg(2))
        elif a==0x4b4880:uc.mem_write(edx,('img://%s'%arg(1)).encode()+b'\0')
        elif a in (0xada54e,0xadcc15):
            dest=arg(0);v=self.string(arg(2));v=(self.string(dest)+v) if a==0xadcc15 else v;uc.mem_write(dest,v+b'\0')
        elif a==0x5c7420:
            dst=uc.reg_read(UC_X86_REG_ESI);self.w(dst,IFACE);self.w(dst+4,0x48);self.w(dst+8,0x100f2000);value=1;self.obj={}
        elif a==0x100f1000:
            name=self.string(arg(1)).decode();v=arg(2);assert self.r(v+4)==6;self.obj[name]=self.string(self.r(v+8)).decode();value=1
        elif a==0x5c7950:
            assert arg(0)==1480;self.pub+=1;self.rendered=self.obj['adnews'];self.events.append(('publish',self.rendered))
        self.ret(self.stubs[a],value)
    def call(self,address,ecx=0,args=()):
        sp=STACK-4*(len(args)+1);self.w(sp,END)
        for i,v in enumerate(args):self.w(sp+4+4*i,v)
        self.uc.reg_write(UC_X86_REG_ESP,sp);self.uc.reg_write(UC_X86_REG_ECX,ecx)
        self.uc.emu_start(address,END,count=200000)
        assert self.uc.reg_read(UC_X86_REG_EIP)==END,'native execution did not return'
    def step_cb(self,_):
        try:self.call(0x5bc9e0)
        except Exception as e:self.errors.append((repr(e),hex(self.uc.reg_read(UC_X86_REG_EIP)),hex(self.uc.reg_read(UC_X86_REG_ESP))))
    def tick(self,now):
        result=lib.news_test_frame(C.byref(self.port),C.byref(self.clock),M,now&0xffffffff);self.results.append([result.started,result.completed,result.replaced,result.failed]);assert not self.errors,self.errors;return result
    def finish(self,body,status=2,pending=0):
        if body:
            a=M+0x180000;self.uc.mem_write(a,body);self.call(0x5bc790,M,(a,len(body)))
        self.w(M+0x28,status);self.w(M+0x2c,pending)
        for _ in range(5):self.tick(self.now)
    def begin(self,now):
        self.now=now;self.tick(now);self.tick(now+60000);self.now=now+60000
    def images_unchanged(self):
        for i,(p,v) in enumerate(self.original_images,1):assert self.r(N+i*0x100+8)==p and bytes(self.uc.mem_read(p,len(v)))==v
        for i,v in enumerate(self.original_image_nodes,1):assert bytes(self.uc.mem_read(N+i*0x100,0x1c))==v
        assert self.r(N+4)==N+0x100 and self.r(N+0x104)==N+0x200

f=Fixture();checks=[]
def check(name,fn):
    if options.select and options.select not in name:return
    fn();checks.append(name)
def repeated():
    for locale,text in [('zh','中文更新一'),('en','English update one')]:
        f.reset(locale);f.begin(100)
        assert len(f.calls)==1,'same-process news never started a real HTTP request'
        f.finish(b'\xef\xbb\xbf'+text.encode())
        assert f.rendered==text and f.pub==1 and len(f.calls)==1
        assert f.calls[0].endswith('lang='+locale);assert len(f.live)==3;f.images_unchanged()
        assert f.events.index(('publish',text))<next(i for i,e in enumerate(f.events) if e[0]=='free')
        for i in range(2,22):
            f.tick(100+i*60000);f.now=100+i*60000;f.finish(b'\xef\xbb\xbf'+f'{locale}-{i}'.encode());assert f.rendered==f'{locale}-{i}' and len(f.live)==3;f.images_unchanged()
        assert len(f.calls)==21 and f.pub==21
check('repeated actual native HTTP/parser/publication, EN/ZH, bounded live payloads and immutable images',repeated)
def bad_responses():
    for body,status in [(b'',3),(b'',2),(b'\xef\xbb\xbf',2),(b'not-BOM',2),(b'x',2)]:
        f.reset();f.begin(0);f.finish(body,status);assert f.rendered=='initial';assert len(f.calls)==1 and len(f.live)==3;f.images_unchanged();assert f.r(M+8)==7
check('zero-body failure/success, BOM-only, malformed and short responses retain last good text',bad_responses)
def setup_fail():
    f.reset();f.setup_fail=True;f.begin(0);assert f.r(M+8)==5
    f.finish(b'',status=5);assert f.r(M+8)==7 and f.rendered=='initial' and len(f.calls)==1 and len(f.live)==3
check('real native HTTP setup-incomplete status retries only at the next bounded interval',setup_fail)
def fallback():
    for loc,text in [('zh','暂无新闻。'),('en','No current news.')]:
        f.reset(loc);f.begin(0);f.finish(b'\xef\xbb\xbf'+text.encode());assert f.rendered==text and f.pub==1 and len(f.live)==3
check('actual empty-feed localized nonempty fallback is published',fallback)
def in_flight():
    f.reset();f.begin(0)
    for i in range(100):f.tick(60000+i*60000)
    assert len(f.calls)==1 and f.r(M+8)==3
    f.finish(b'\xef\xbb\xbfarrived',pending=2);assert f.pub==0 and not f.freed and len(f.calls)==1
    f.w(M+0x2c,0);f.finish(b'');assert f.rendered=='arrived';f.images_unchanged()
check('no overlap across elapsed intervals; final status waits for native pending-task retirement',in_flight)
def teardown():
    f.reset();f.begin(0);assert f.r(M+4)==NR_KIND
    # Same-address replacement matches native constructor: request kind resets.
    f.reset('en',same_address=True);f.w(M+8,2);f.w(M+0x480,N)
    f.tick(60001);assert f.r(M+4)==2 and f.r(M+0x480)==N
    assert not f.freed and f.r(N+4)==N+0x100
check('frontend teardown and same-address reconstruction do not inherit refresh ownership',teardown)
def initial():
    f.reset();f.w(M+8,2);f.w(M+0x480,N);f.tick(0)
    assert f.r(M+4)==2 and f.r(N+4)==N+0x100 and not f.freed
check('ordinary initial native queue is unchanged',initial)
def native_cleanup():
    for pending in (False,True):
        f.reset();f.begin(0)
        if not pending:f.finish(b'\xef\xbb\xbflifetime')
        f.uc.reg_write(UC_X86_REG_ESI,M);f.call(0x5bc4e0)
        assert not f.live and not f.structures and f.r(M+0x478)==0 and f.r(M+0x480)==0
check('actual native cleanup frees every retained allocation once, including pending refresh teardown',native_cleanup)
def crlf():
    f.reset();f.begin(0);f.finish(b'\xef\xbb\xbf'+ '第一行\r\n第二行'.encode());assert f.rendered=='第一行\n第二行'
check('actual BOM parser retains Chinese and normalizes CRLF',crlf)
def queue_guard():
    f.reset();f.w(N+0x204,N);f.begin(0);assert not f.calls
    f.reset();f.w(N+16,1);f.begin(0);assert not f.calls
check('cyclic queues and binary-kind nodes are not rearmed',queue_guard)
def wraparound():
    f.reset();f.begin(0xffff9000);f.finish(b'\xef\xbb\xbfwrapped');assert f.rendered=='wrapped' and len(f.calls)==1
check('GetTickCount wraparound',wraparound)
result={'passed':len(checks),'checks':checks,'policy_library':Path(lib._name).name,'native_image_sha256':hashlib.sha256(b).hexdigest(),'native_functions_executed':['5BC9E0 dispatcher','5BC8B0 real native request setup','5BC790/5BC640 response body callback/buffering','5BC990 status poll','5BCD90 consume','5BC700 BOM/CRLF text parser','5BCA50 complete native category publisher'],'explicit_doubles':['HTTP transport only; no network','CRT allocator with poisoning/double-free tracking','GFx CreateObject/SetMember/refreshTask and string copy boundary','CRT string helpers, logger, stack cookie'],'boundary':'No game process, UI, account, service or network used. GFx AVM string-copy boundary is a double; native stack-CSV lifetime and source SDK contract substantiate it separately.'}
(options.out/(Path(lib._name).stem+'-result.json')).write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n')
print(json.dumps(result,ensure_ascii=False))
