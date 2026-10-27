"""Exact original x86 plus compiled repair. Windows APIs are named doubles;
run windows_rules.c separately for actual OS rule conversion. No game runs."""
from native_fixture import *
import math,re
BR=pefile.PE(str(args.bridge)); BB=BR.OPTIONAL_HEADER.ImageBase
exports={x.name.decode():BB+x.address for x in BR.DIRECTORY_ENTRY_EXPORT.symbols if x.name}
def exported(prefix):
 return next(v for k,v in exports.items() if k.lstrip('_@').split('@')[0]==prefix)
EPOCH=datetime.datetime(1601,1,1,tzinfo=datetime.timezone.utc)
def unpacktime(data):
 y,m,_,d,h,n,s,ms=struct.unpack('<8H',data)
 return datetime.datetime(y,m,d,h,n,s,ms*1000,tzinfo=datetime.timezone.utc)
def packtime(value):
 return struct.pack('<8H',value.year,value.month,(value.weekday()+1)%7,value.day,value.hour,value.minute,value.second,value.microsecond//1000)
class Fixed(DateFixture):
 def __init__(self,epoch=1792799822,bias=480,daylight=-60,status=2,offset=-25200000):
  self.offset,self.failure,self.missing,self.disabled=offset,0,False,False
  super().__init__(epoch,bias,daylight,status)
  body=BR.get_memory_mapped_image(); self.uc.mem_map(BB,(len(body)+4095)&~4095);self.uc.mem_write(BB,body)
  self.uc.mem_map(0x21000000,0x10000)
  for lib in BR.DIRECTORY_ENTRY_IMPORT:
   for item in lib.imports:
    at=0x21000000+len(self.imports)*16
    self.imports[at]=(item.name or b'ordinal').decode();self.uc.mem_write(item.address,struct.pack('<I',at))
  self.call(exported('BridgeInit'),b'')
 def code(self,machine,at,size,unused):
  sp=machine.reg_read(UC_X86_REG_ESP)
  def arg(i):return struct.unpack('<I',machine.mem_read(sp+4+i*4,4))[0]
  if at==exported('BridgeDynamic'):
   data=bytearray(432);struct.pack_into('<i',data,0,self.bias);struct.pack_into('<i',data,168,self.daylight);data[428]=self.disabled
   machine.mem_write(arg(0),bytes(data));self.ret(4,0xffffffff if self.failure==2 else self.status);return
  if at==exported('BridgeToLocal'):
   utc=unpacktime(machine.mem_read(arg(1),16));local=utc+datetime.timedelta(milliseconds=self.offset)
   machine.mem_write(arg(2),packtime(local));self.ret(12,0 if self.failure==3 else 1);return
  name=self.imports.get(at)
  if name=='FileTimeToSystemTime':
   ticks=struct.unpack('<Q',machine.mem_read(arg(0),8))[0]
   try:machine.mem_write(arg(1),packtime(EPOCH+datetime.timedelta(microseconds=ticks//10)))
   except (OverflowError,ValueError):self.ret(8,0);return
   self.ret(8,0 if self.failure==1 else 1);return
  if name=='SystemTimeToFileTime':
   dt=unpacktime(machine.mem_read(arg(0),16))-EPOCH;ticks=(dt.days*86400+dt.seconds)*10000000+dt.microseconds*10
   machine.mem_write(arg(1),struct.pack('<Q',ticks));self.ret(8,0 if self.failure==4 else 1);return
  if name=='GetModuleHandleA':self.ret(4,0 if self.missing else 1);return
  if name=='GetProcAddress':
   text=bytearray();p=arg(1)
   while machine.mem_read(p,1)!=b'\0':text+=machine.mem_read(p,1);p+=1
   target=exported('BridgeDynamic') if text==b'GetDynamicTimeZoneInformation' else exported('BridgeToLocal')
   self.ret(8,0 if self.missing else target);return
  super().code(machine,at,size,unused)
def settime(obj,value,wrapped):
 obj.call(exported('GfxDateSetTime') if wrapped else 0x88a640,struct.pack('<Id',obj.out,value))
 assert obj.uc.reg_read(UC_X86_REG_ESP)==obj.stack+16,'thiscall stack ABI'
 assert obj.uc.reg_read(UC_X86_REG_EAX)==obj.out,'original return pointer'
 return bytes(obj.uc.mem_read(obj.obj,64)),bytes(obj.uc.mem_read(obj.out,8))
def initialize(obj):obj.call(0x88bcf0,struct.pack('<II',0,0))
checks=[]
for name,bias,daylight,status,offset in [('Pacific-summer',480,-60,2,-25200000),('Pacific-winter',480,-60,1,-28800000),('UTC',0,0,0,0),('Lord-Howe',-630,-30,2,39600000),('India',-330,0,0,19800000),('Nepal',-345,0,0,20700000)]:
 a=Fixed(bias=bias,daylight=daylight,status=status,offset=offset);initialize(a)
 expected=a.hm();settime(a,a.epoch*1000.,True)
 assert a.hm()==expected,(name,a.hm(),expected)
 assert a.state()['epoch_ms']==a.epoch*1000 and a.state()['dynamic_DST_flag']==0
 checks.append(name+'-compiled-wrapper-real-getters')
for value in [1792799822000.,-1.8,1.8,0.,float('nan'),float('inf'),float('-inf'),8.64e15,8.64e15+1,-8.64e15]:
 a,b=Fixed(),Fixed();initialize(a);initialize(b)
 original,out=settime(a,value,False);fixed,fout=settime(b,value,True)
 assert original[:32]+original[36:]==fixed[:32]+fixed[36:] and out==fout,'epoch/NaN/TimeClip/object preservation'
 checks.append('original-epoch-TimeClip-NaN-'+repr(value))
for stage in (1,2,3,4):
 a,b=Fixed(),Fixed();initialize(a);initialize(b);b.failure=stage
 assert settime(a,1792799822000.,False)==settime(b,1792799822000.,True)
 checks.append('failed-API-'+str(stage)+'-retains-entire-original-result')
# Measured by the actual AS method table -> thunk -> member initializer chains.
for name,at in [('getTime',0x867ac0),('getUTCHours',0x88a760),('getUTCMinutes',0x88a7e0),('getUTCSeconds',0x88a860),('getUTCMilliseconds',0x88a8e0),('getUTCDate',0x892cb0),('getUTCMonth',0x8920d0),('getUTCFullYear',0x892ba0),('getUTCDay',0x88a6d0)]:
 a,b=Fixed(),Fixed();initialize(a);initialize(b)
 settime(a,1792799822345.,False);settime(b,1792799822345.,True)
 a.call(at,struct.pack('<I',a.out));b.call(at,struct.pack('<I',b.out))
 assert a.uc.mem_read(a.out,8)==b.uc.mem_read(b.out,8),name
 checks.append('actual-original-'+name+'-unchanged')
a=Fixed();initialize(a)
for epoch,offset in [(1792799822000.,-25200000),(1768478400000.,-28800000),(1792799822000.,-25200000)]:
 a.offset=offset;settime(a,epoch,True)
 assert a.state()['cached_offset_ms']==offset and a.state()['epoch_ms']==epoch
checks.append('repeated-same-object-target-season-changes')
a=Fixed();initialize(a);settime(a,a.epoch*1000.,False)
assert a.hm()==[15.,57.];checks.append('negative-original-summer-reproduces-one-hour-late')
a=Fixed(offset=-28800000);initialize(a);settime(a,1142856000000.,True)
assert a.hm()==[4.,0.]
a.uc.mem_write(a.obj+0x24,b'\1');assert a.hm()==[5.,0.]
checks.append('negative-reset-flag-uses-current-DST-wrong-for-historical-target')
a=Fixed(bias=-630,daylight=-30,offset=39600000);initialize(a);settime(a,a.epoch*1000.,False)
old=struct.unpack('<i',a.uc.mem_read(a.obj+32,4))[0];a.uc.mem_write(a.obj+32,struct.pack('<i',old+3600000));assert a.hm()!=[10.,57.]
checks.append('negative-hardcoded-one-hour-fails-half-hour-DST')
for disabled in (False,True):
 a=Fixed();initialize(a);a.disabled=disabled;settime(a,a.epoch*1000.,True)
 assert a.hm()==([15.,57.] if disabled else [16.,57.]);checks.append('explicit-disabled-DST-'+str(disabled))
guards=Path(__file__).resolve().parents[3]/'gfx_date_time_guards.h'
regions=[(int(v,16),int(n)) for v,n in re.findall(r'\{0x([0-9A-F]+)u,(\d+)u,',guards.read_text())]
def patch(a):
 a.uc.mem_write(0xce7d38,b'\0'*8);a.call(exported('BridgePatch'),b'');return a.uc.reg_read(UC_X86_REG_EAX)
a=Fixed();before=bytes(a.uc.mem_read(0xb36e50,19));assert patch(a)==1
after=bytes(a.uc.mem_read(0xb36e50,19));assert before[:1]+before[5:]==after[:1]+after[5:]
a.call(0xb36e50,b'');assert struct.unpack('<I',a.uc.mem_read(0xce7d38,4))[0]==exported('GfxDateSetTime')
checks.append('actual-original-initializer-installs-only-member-pointer')
for va,n in regions:
 a=Fixed();value=a.uc.mem_read(va,1)[0];a.uc.mem_write(va,bytes([value^1]));assert patch(a)==0,hex(va)
 checks.append('exact-guard-mutation-rejected-'+hex(va))
a=Fixed();a.missing=True;assert patch(a)==0;checks.append('unsupported-API-no-install')
a=Fixed();a.uc.mem_write(0xce7d38,struct.pack('<II',0x88a640,0));a.call(exported('BridgePatch'),b'');assert a.uc.reg_read(UC_X86_REG_EAX)==0
checks.append('late-install-no-write')
proof=dict(passed=True,image_sha256=SHA,bridge_sha256=hashlib.sha256(args.bridge.read_bytes()).hexdigest(),checks=checks,count=len(checks),actual_game=False,
 boundary='Original x86 Date/CRT/getter/initializer and compiled repair execute. Clock output and WinAPI boundaries are doubles; actual Windows timezone rules are checked independently. No process/UI/clock/timezone changes.')
args.output.write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(proof))
