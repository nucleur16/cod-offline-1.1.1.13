"""Execute C1's own range consumer in Unicorn; no live process is opened.
The boundary supplies a WeaponDef pointer and target-difference vector only.
Actual C1 instructions read +66c, square it, compare distance and select the
branch leading to the enemy-crosshair bit. No T5 layout offsets are used.
"""
import argparse,struct
import pefile,unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EDX,UC_X86_REG_EAX,UC_X86_REG_EFLAGS
def main():
    p=argparse.ArgumentParser();p.add_argument('--image',default='D:/CoD/CODOL Builds/Call of Duty Online 1.1.1.13/C1.exe');a=p.parse_args()
    image=pefile.PE(a.image,fast_load=True)
    def run(distance,range_value):
        uc=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
        base=image.OPTIONAL_HEADER.ImageBase
        uc.mem_map(base,(image.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
        for section in image.sections:uc.mem_write(base+section.VirtualAddress,section.get_data())
        # These are synthetic isolated machine regions, never a client address.
        uc.mem_map(0x20000000,0x20000);uc.mem_map(0x30000000,0x2000)
        uc.reg_write(UC_X86_REG_ESP,0x20008000);uc.reg_write(UC_X86_REG_EBP,0x20010000)
        uc.reg_write(UC_X86_REG_EDX,0x30000000)
        uc.mem_write(0x3000066c,struct.pack('<f',range_value))
        assert bytes(uc.mem_read(0x8356f21,6))==bytes.fromhex('d9826c060000')
        assert bytes(uc.mem_read(0x83d3cb8,2))==bytes.fromhex('dcc8')
        assert bytes(uc.mem_read(0x842eeba,6))==bytes.fromhex('810b10000000')
        seen={}
        def step(machine,pc,size,_):
            if pc==0x402466:
                # C1's real vector-length-squared routine receives this vector.
                machine.mem_write(machine.reg_read(UC_X86_REG_EAX),struct.pack('<fff',distance,0,0))
            elif pc==0x845f9b7:
                seen['squared_range']=struct.unpack('<f',machine.mem_read(0x20010000-0x74,4))[0]
            elif pc==0x837c2d9:
                seen['reject']=not bool(machine.reg_read(UC_X86_REG_EFLAGS)&4)
                if seen['reject']:machine.emu_stop()
            elif pc==0x842eeba:
                seen['enemy_bit']=True;machine.emu_stop()
        uc.hook_add(unicorn.UC_HOOK_CODE,step)
        uc.emu_start(0x8356f21,0,count=1500)
        assert seen['squared_range']==range_value*range_value
        assert seen['reject']==(distance*distance>range_value*range_value)
        assert bool(seen.get('enemy_bit'))==(not seen['reject'])
        return seen
    for distance,range_value in [(200.,512.),(1024.,512.),(200.,0.),(1024.,2048.)]:
        print(distance,range_value,run(distance,range_value))
    print('PASS: actual C1 WeaponDef+66c range consumer controls squared-distance rejection before enemy bit')
if __name__=='__main__':main()
