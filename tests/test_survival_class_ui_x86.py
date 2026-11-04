"""Execute C1's real menu-name router; only GFx activation/invoke are boundaries."""
import argparse,hashlib,struct,sys,unittest
if __name__!='__main__':raise unittest.SkipTest('invoke explicitly with --image C1.exe')
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
ap=argparse.ArgumentParser();ap.add_argument('--image',required=True);args,rest=ap.parse_known_args()
image=open(args.image,'rb').read()
assert hashlib.sha256(image).hexdigest()=='d990610732258b1d00dd6fe63d6691f294312a2e0562955a01ce16216115390d'
pe=pefile.PE(data=image);BASE=pe.OPTIONAL_HEADER.ImageBase

class Route(unittest.TestCase):
 def run_menu(self,name):
  u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(BASE,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
  for s in pe.sections:u.mem_write(BASE+s.VirtualAddress,s.get_data())
  u.mem_map(0x30000000,0x10000);u.mem_map(0x31000000,0x10000)
  put=lambda a,v:u.mem_write(a,struct.pack('<I',v))
  put(0x28a6688,0x30000200);u.mem_write(0x3000020c,b'\x01')
  for a in (0x810c28c,0x810c290,0x810c294,0x810c298):put(a,0)
  u.mem_write(0x30001000,name.encode()+b'\0');stack=0x31008000;stop=0x30000080
  put(stack,stop);put(stack+4,0x30001000);u.reg_write(UC_X86_REG_ESP,stack)
  calls=[]
  def hook(uc,address,size,_):
   if address==stop:uc.emu_stop();return
   if address not in (0x6b5810,0x5c7b30):return
   if address==0x5c7b30:
    at=uc.reg_read(UC_X86_REG_EDX);value=bytearray()
    while uc.mem_read(at+len(value),1)!=b'\0':value+=uc.mem_read(at+len(value),1)
    calls.append(value.decode())
   esp=uc.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',uc.mem_read(esp,4))[0]
   uc.reg_write(UC_X86_REG_ESP,esp+4);uc.reg_write(UC_X86_REG_EAX,1);uc.reg_write(UC_X86_REG_EIP,ret)
  u.hook_add(UC_HOOK_CODE,hook);u.emu_start(0x6b7940,stop,count=20000)
  return calls
 def test_generic_class_name_does_not_open_pause_selector(self):self.assertEqual(self.run_menu('changeclass'),[])
 def test_allies_uses_original_pause_selector(self):self.assertEqual(self.run_menu('changeclass_marines'),['cpp_openChooseClassMenu'])
 def test_axis_uses_original_pause_selector(self):self.assertEqual(self.run_menu('changeclass_opfor'),['cpp_openChooseClassMenu'])

unittest.main(argv=[sys.argv[0]]+rest)
