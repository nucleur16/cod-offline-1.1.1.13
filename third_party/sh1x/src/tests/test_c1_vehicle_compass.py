"""Original C1 vehicle icon choice, without replacing engine functions.

Execute only the bounded original selector. Stop before actual material registration
for the explicit case, inspecting its exact original lookup/name arguments.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import unittest
import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EBP, UC_X86_REG_ESP
IMAGE=None

def choose(owner_team=2, override=0):
    p=pefile.PE(IMAGE)
    u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    u.mem_map(0x400000,(p.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
    for section in p.sections:u.mem_write(0x400000+section.VirtualAddress,section.get_data())
    u.mem_map(0x20000000,0x20000);u.mem_map(0x30000000,0x1000000)
    def w(a,v):u.mem_write(a,struct.pack('<I',v))
    def get(a):return struct.unpack('<I',u.mem_read(a,4))[0]
    cg=0x30000000;cent=0x30800000;instance=0x30a00000;definition=0x30b00000;frame=0x20010000
    w(instance+4,4);w(0x2bc3018+4*4,definition)
    w(cg+0x35c0,0);w(cg+0x15ae70,2)
    w(instance+0x38,1);w(cg+0x15ae70+0xb50,owner_team)
    w(definition+0x1d0,0x1111);w(definition+0x1d4,0x2222)
    w(cent+0x15c,override)
    name=b'hud_minimap_pavelow_red\0'
    if override:
        w(0x6d13040+override*4,0x100)
        u.mem_write(0x6d1370c+0x100,name)
    for register,value in ((UC_X86_REG_EAX,cent),(UC_X86_REG_EBX,cg),(UC_X86_REG_ESI,instance+8),(UC_X86_REG_EBP,frame),(UC_X86_REG_ESP,frame-0x100)):
        u.reg_write(register,value)
    result={}
    def stop(uc,pc,size,user):
        if pc==0x68124b:
            result['material']=get(frame-0x30);uc.emu_stop()
        if pc==0xa78170:
            esp=uc.reg_read(UC_X86_REG_ESP)
            result['name']=bytes(uc.mem_read(get(esp+4),len(name)))
            result['registration_type']=get(esp+8)
            uc.emu_stop()
    u.hook_add(unicorn.UC_HOOK_CODE,stop)
    u.emu_start(0x6811ac,0,count=10000)
    assert result,'Original C1 selector failed to reach selection/registration'
    return result

class OriginalVehicleCompass(unittest.TestCase):
    def test_normal_friendly_owner(self):self.assertEqual(choose(2),{'material':0x1111})
    def test_normal_hostile_owner(self):self.assertEqual(choose(1),{'material':0x2222})
    def test_explicit_material_precedes_friendly_owner(self):
        self.assertEqual(choose(2,7),{'name':b'hud_minimap_pavelow_red\0','registration_type':7})

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--image',required=True)
    args,remaining=parser.parse_known_args();IMAGE=args.image
    if hashlib.sha256(Path(IMAGE).read_bytes()).hexdigest().upper()!='D990610732258B1D00DD6FE63D6691F294312A2E0562955A01CE16216115390D':parser.error('Expected unchanged original C1.exe')
    unittest.main(argv=[__file__]+remaining)
