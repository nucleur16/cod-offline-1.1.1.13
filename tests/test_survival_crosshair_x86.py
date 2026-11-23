"""Run the actual C1 scanner and shipping compiled Survival/zombies predicate in Unicorn.
Only CG_LocationalTrace is a boundary double. Positions use C1's own encoded
centity origin representation (decoded by443d90); no flag/timer/math doubles.
"""
import argparse,struct,unittest
import pefile,unicorn
from unicorn.x86_const import *
a=argparse.ArgumentParser();a.add_argument('--fixture',required=True);a.add_argument('--image',required=True)
args,rest=a.parse_known_args()
PE=pefile.PE(args.image,fast_load=True);FP=pefile.PE(args.fixture)
FB=FP.OPTIONAL_HEADER.ImageBase
EX={s.name.decode():FB+s.address for s in FP.DIRECTORY_ENTRY_EXPORT.symbols}

def run(mode,patched,etype=18,team=1,distance=200,slot=0,eflags=0,fx_blocked=False):
 u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
 u.mem_map(0x400000,(PE.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
 for s in PE.sections:u.mem_write(0x400000+s.VirtualAddress,s.get_data())
 u.mem_map(FB,(FP.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(FB,FP.get_memory_mapped_image())
 u.mem_map(0x22000000,0x20000);u.mem_map(0x30000000,0x1000000)
 def w(at,val):u.mem_write(at,struct.pack('<I',val))
 def get(at):return struct.unpack('<I',u.mem_read(at,4))[0]
 w(0x6c3d2ac,0);w(0x16d3b58,0x30000000^0x490b400c)
 w(0x6c3d2b4,1);w(0x16d3b5c,0x30800000^0x189bfead)
 w(0x6c3d2b0,2);w(0x16d3b60,0x30e00000^0x3958bead)
 u.mem_write(0x30e00020,mode.encode()+b'\0')
 cent=0x30800000+821*0x228
 w(cent+0xec,eflags);w(cent+0xe8,etype);w(cent+0x180,821);u.mem_write(cent+0x13c,bytes([slot]))
 # Actual C1 encoded lerpOrigin, inverse of443d90.
 x=struct.unpack('<I',struct.pack('<f',distance))[0]^0x073ff35a
 y=x^0xedb436a0;z=y^0x893fead3
 w(cent+0x18,x);w(cent+0x1c,y);w(cent+0x20,z)
 w(0x1509d58,0x30f00000);w(0x16ec82c,0x30f00020)
 w(0x16d3b24,0x30f00080);w(0x30f0008c,1)
 w(0x30168528,team);w(0x300b0350,1000);w(0x3015ae70,2)
 w(0x3000033c,17);w(0x150d720+17*4,0x30f10000)
 u.mem_write(0x30f1066c,struct.pack('<f',750))
 if fx_blocked:w(0x20c99d8,1)
 def hook(u,pc,size,_):
  if pc==0x486370:
   sp=u.reg_read(UC_X86_REG_ESP);out=get(sp+4)
   u.mem_write(out,b'\0'*44);u.mem_write(out,struct.pack('<f',.01));w(out+0x1c,1)
   u.mem_write(out+0x20,struct.pack('<H',821))
   u.reg_write(UC_X86_REG_EIP,get(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
  if pc==0x22000000:u.emu_stop()
 u.hook_add(unicorn.UC_HOOK_CODE,hook)
 def call(pc):
  u.reg_write(UC_X86_REG_ESP,0x22010000);w(0x22010000,0x22000000)
  u.emu_start(pc,0,count=200000)
  assert u.reg_read(UC_X86_REG_EIP)==0x22000000
 if patched:
  call(EX['STestInstall']);assert get(EX['stest_writes'])==2
 u.reg_write(UC_X86_REG_ESI,0)
 call(0x439940)
 assert bytes(u.mem_read(0x30e00020,len(mode)+1))==mode.encode()+b'\0'
 return tuple(get(0x30000000+x) for x in [0x344,0xbf04c,0xbf050,0xbf054])

class Tests(unittest.TestCase):
 def test_original_modes_unchanged(self):
  for mode in ['coop','mp','war','combat','','survival_extra','z','xzm']:
   for team in [1,2]:
    with self.subTest(mode=mode,team=team):self.assertEqual(run(mode,False,team=team),run(mode,True,team=team))
 def test_survival_actual_enemy_flags_target_and_settle(self):
  self.assertEqual(run('survival',False),(0,0,0,0))
  expected=run('coop',False)
  self.assertEqual(expected,(16,821,1000,1000))
  for mode in ['survival','SURVIVAL']:
   self.assertEqual(run(mode,True),expected)
 def test_zombies_actual_enemy_flags_target_and_settle(self):
  # zombies and hellhounds are axis actors, zombies players allies: C1's own coop arm makes them enemies
  self.assertEqual(run('zm',False),(0,0,0,0))
  expected=run('coop',False)
  for mode in ['zm','ZM','zm_x']:
   self.assertEqual(run(mode,True),expected)
  for kw in [dict(team=2),dict(team=0),dict(distance=1000),dict(etype=0),dict(etype=20),dict(slot=255),dict(eflags=0x20000)]:
   with self.subTest(**kw):
    self.assertEqual(run('zm',True,**kw),run('coop',False,**kw))
 def test_original_actor_filters_preserved(self):
  for kw in [dict(team=2),dict(team=0),dict(distance=1000),dict(etype=0),dict(etype=20),dict(slot=255),dict(eflags=0x20000)]:
   with self.subTest(**kw):
    self.assertEqual(run('survival',True,**kw),run('coop',False,**kw))
if __name__=='__main__':unittest.main(argv=[__file__]+rest)
