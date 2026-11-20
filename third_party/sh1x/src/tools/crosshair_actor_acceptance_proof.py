"""Execute C1 1.1.1.13 crosshair scanner with the same actor trace for each mode.
Run with `survival`, `coop` or `war`. Actual C1 code performs its hardcoded
coop comparison, actor classification and range dispatch. Only the locational
trace result is supplied at the engine boundary; synthetic memory holds the
same actor/weapon/team inputs. No process is opened or modified.
The test proves mode eligibility, not complete live tracing/timing behavior.
"""
import pefile,unicorn,struct,sys
from unicorn.x86_const import *
gametype=sys.argv[1] if len(sys.argv)>1 else 'survival'
assert gametype in ('survival','coop','war')
p=pefile.PE('D:/CoD/CODOL Builds/Call of Duty Online 1.1.1.13/C1.exe',fast_load=True);u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
u.mem_map(0x400000,(p.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
for s in p.sections:u.mem_write(0x400000+s.VirtualAddress,s.get_data())
u.mem_map(0x20000000,0x20000);u.mem_map(0x30000000,0x1000000)
def w(a,v):u.mem_write(a,struct.pack('<I',v))
w(0x6c3d2ac,0);w(0x16d3b58,0x30000000^0x490b400c);w(0x300035e0,0x30400000);w(0x6c3d2b4,1);w(0x16d3b5c,0x30800000^0x189bfead)
w(0x30800000+821*0x228+0xe8,18);w(0x30800000+821*0x228+0x180,821)
w(0x1509d58,0x30f00000);w(0x16ec82c,0x30f00020)
u.reg_write(UC_X86_REG_ESP,0x20010000);w(0x20010000,0x20000000);u.reg_write(UC_X86_REG_ESI,0)
w(0x6c3d2b0,2);w(0x16d3b60,(0x30000000^0x490b400c)^0x4053fec1^0x30e00000)
u.mem_write(0x30e00080,gametype.encode()+b'\0')
w(0x16d3b24,0x30f00080);w(0x30f0008c,1)
w(0x30168528,1);w(0x300b0350,1000);w(0x3015ae70,2);w(0x3000033c,17);w(0x150d720+17*4,0x30f10000);u.mem_write(0x30f1066c,struct.pack('<f',750))
last=[];count=0; reads=[];range_seen=False;actor_seen=False
def hook(u,pc,size,x):
 global count,range_seen,actor_seen
 count+=1;last.append(pc)
 if len(last)>20:last.pop(0)
 if pc==0x8356f21:range_seen=True
 if pc==0x83c5768:actor_seen=True

 if pc==0x486370:
  sp=u.reg_read(UC_X86_REG_ESP);out=struct.unpack('<I',u.mem_read(sp+4,4))[0];u.mem_write(out,b'\0'*44);u.mem_write(out,struct.pack('<f',.1));w(out+0x1c,1);u.mem_write(out+0x20,struct.pack('<H',821));u.reg_write(UC_X86_REG_EIP,struct.unpack('<I',u.mem_read(sp,4))[0]);u.reg_write(UC_X86_REG_ESP,sp+4)
 if pc==0x20000000:u.emu_stop()
def invalid(u,access,a,size,val,x):
 print('READS',[r for r in reads if not (0x20000000<=int(r[1],16)<0x20020000)][-35:]);print('INVALID',hex(a),'size',size,'EIP',hex(u.reg_read(UC_X86_REG_EIP)), 'last',*[hex(i) for i in last]);return False
def onread(u,access,a,size,val,x):
 reads.append((hex(u.reg_read(UC_X86_REG_EIP)),hex(a),size,bytes(u.mem_read(a,size)).hex()))
u.hook_add(unicorn.UC_HOOK_MEM_READ,onread);u.hook_add(unicorn.UC_HOOK_CODE,hook);u.hook_add(unicorn.UC_HOOK_MEM_INVALID,invalid)
try:u.emu_start(0x439940,0,count=200000)
except Exception as e:print(e)
assert u.reg_read(UC_X86_REG_EIP)==0x20000000
assert range_seen==(gametype=='coop')
assert actor_seen==(gametype=='coop')
print({'gametype':gametype,'actor_classification':actor_seen,'range_consumer':range_seen,'instructions':count})
