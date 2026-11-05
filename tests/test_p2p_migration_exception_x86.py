"""Offline actual Pi VEH + unchanged migration SEH dispatch ordering proof.

Cross-compiles a PE32 fixture and executes its x86 in Unicorn. Never launches
a Windows executable or the game. ReadProcessMemory/thread/fiber APIs are
explicit doubles. Real unmapped-memory faults enter VEH before thread SEH.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys,unittest

if __name__!='__main__':
    raise unittest.SkipTest('Explicit compiler/Unicorn fixture; no Windows execution')
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True)
ap.add_argument('--image',type=Path,help='existing owned exact native image, read only; never launched')
args=ap.parse_args();ROOT=Path(__file__).resolve().parents[1];out=args.out.resolve();out.mkdir(exist_ok=True)

def linux(path):
    value=str(path.resolve());return '/mnt/'+value[0].lower()+value[2:].replace('\\','/')

source=(ROOT/'p2p_inproc.c').read_text()
start=source.index('#if CODOL_KERNEL_ROOM_CONTEXT\n#include "p2p_exception_policy.h"')
end=source.index('/* A call into the server',start)
body=source[start:end]
prefix='''#define CODOL_KERNEL_ROOM_CONTEXT 1
#include <windows.h>
#include <stdint.h>
#include "%s/tests/kernel_call_guard_fixture.c"
static void *g_piFiber=(void*)0x1234;
static DWORD g_piThread=7;
static DWORD_PTR g_piBase=0x50000000,g_piEnd=0x50001000,g_crtBase=0x100000,g_crtEnd=0x110000;
static volatile DWORD g_piFaultCode,g_piFaultEip;
static BYTE g_piRescueStack[65536];
__declspec(dllexport) DWORD fixture_current_fiber=0x1234;
static void *PiCurrentFiber(void){return (void*)(uintptr_t)fixture_current_fiber;}
static void __cdecl PiRescue(void){}
'''%linux(ROOT)
suffix='''
__declspec(dllexport) int Vectored(EXCEPTION_RECORD *r,CONTEXT *c){EXCEPTION_POINTERS e={r,c};return PiFaultHandler(&e);}
__declspec(dllexport) const uint32_t rescue_address=(uint32_t)(uintptr_t)PiRescue;
'''
fixture=out/'pi-fault-dispatch.dll';c=out/'pi-fault-dispatch.c';c.write_text(prefix+body+suffix,newline='\n')
subprocess.run(['wsl','i686-w64-mingw32-gcc','-shared','-O1','-g','-Wall','-Wextra',
    '-I'+linux(ROOT),'-o',linux(fixture),linux(c)],check=True)

# Reuse the production guard fixture's actual machine/SEH dispatch harness.
# The footer is omitted so only this script controls the test suite and report.
guard=(ROOT/'tests/test_kernel_call_guard_x86.py').read_text().rsplit("if __name__=='__main__':",1)[0]
original_argv=sys.argv;sys.argv=[str(ROOT/'tests/test_kernel_call_guard_x86.py'),
    '--fixture',str(fixture),'--output',str(out/'guard-unused.json')]
if args.image:sys.argv.extend(['--image',str(args.image)])
env={'__name__':'__main__','__file__':str(ROOT/'tests/test_kernel_call_guard_x86.py')}
exec(compile(guard,env['__file__'],'exec'),env);sys.argv=original_argv
UC_HOOK_CODE=env['UC_HOOK_CODE'];UC_X86_REG_ESP=env['UC_X86_REG_ESP'];UC_X86_REG_EIP=env['UC_X86_REG_EIP']
STACK,CODE,ALL=env['STACK'],env['CODE'],env['ALL'];Base=env['Machine']

class Machine(Base):
    def __init__(self):
        self.thread=7;self.seh_reads=0
        super().__init__()
    def hook(self,u,at,size,unused):
        super().hook(u,at,size,unused)
        if at==self.api.get('GetCurrentThreadId'):self.ret(self.thread);return
        if at==self.api.get('ReadProcessMemory'):
            sp=u.reg_read(UC_X86_REG_ESP);self.seh_reads+=1
            try:
                data=bytes(u.mem_read(self.get(sp+8),self.get(sp+16)))
                u.mem_write(self.get(sp+12),data);self.put(self.get(sp+20),len(data));self.ret(1,20)
            except env['UcError']:
                self.ret(0,20)
    def vectored(self,code=0xc0000005,flags=0):
        registers={r:self.u.reg_read(r)for r in ALL}
        record,context=STACK+0x10000,STACK+0x11000
        self.u.mem_write(record,b'\0'*80);self.put(record,code);self.put(record+4,flags)
        self.u.mem_write(context,b'\0'*self.exported('context_size'))
        for field,reg in [('eip',env['UC_X86_REG_EIP']),('esp',env['UC_X86_REG_ESP']),('ebp',env['UC_X86_REG_EBP'])]:
            self.put(context+self.exported('context_'+field),registers[reg])
        result=self.invoke('Vectored',[record,context],STACK+0x1e000)
        after={field:self.get(context+self.exported('context_'+field))for field in ('eip','esp','ebp')}
        for r,value in registers.items():self.u.reg_write(r,value)
        return result,after
    def dispatch(self,code=0xc0000005,flags=0):
        result,_=self.vectored(code,flags)
        assert result==0,'VEH intercepted a fault before migration SEH'
        return super().dispatch(code,flags)

env['Machine']=Machine

class PiOrdering(unittest.TestCase):
    def fault(self,locked=False):
        m=Machine()
        prefix=b''
        if locked:
            import struct
            m.u.mem_write(CODE+0xa000,b'\xc2\x04\x00')
            prefix=b'\x68\x00\x20\x00\x00\xe8'+struct.pack('<i',0xa000-10)
        m.u.mem_write(CODE,prefix+b'\xb8\x00\x00\xad\xde\x8b\x00\xc3')
        m.invoke('Call',[1,CODE,3],expect_fault=True)
        return m
    def test_nested_crt_av_reaches_unchanged_failfast_with_lock_retained(self):
        m=self.fault(True);self.assertEqual(m.depth,1)
        m.dispatch();self.assertEqual(m.terminated,0x4b524301);self.assertEqual(m.depth,1)
    def test_nested_seh_frame_does_not_hide_migration_marker(self):
        m=self.fault();head=m.get(0);m.put(head-32,head);m.put(head-28,0x12345678);m.put(0,head-32)
        result,_=m.vectored();self.assertEqual(result,0);self.assertEqual(m.seh_reads,2)
        m.dispatch();self.assertEqual(m.terminated,0x4b524301)
    def test_ordinary_server_fault_without_migration_frame_still_rescues(self):
        m=self.fault();m.put(0,0xffffffff)
        result,after=m.vectored();self.assertEqual(result,0xffffffff)
        self.assertEqual(after['eip'],m.exported('rescue_address'));self.assertIsNone(m.terminated)
    def test_unreadable_or_malformed_chain_declines_rescue(self):
        for head in (0,1,0xdead0000):
            m=self.fault();m.put(0,head);self.assertEqual(m.vectored()[0],0)
        m=self.fault();head=m.get(0);m.put(head,head);self.assertEqual(m.vectored()[0],0)
    def test_chain_scan_has_a_hard64_read_bound(self):
        m=self.fault();head=STACK+0x4000
        for i in range(65):m.put(head+8*i,head+8*(i+1));m.put(head+8*i+4,0x42)
        m.put(0,head);self.assertEqual(m.vectored()[0],0);self.assertEqual(m.seh_reads,64)
    def test_other_thread_fiber_and_client_eip_are_never_rescued(self):
        m=self.fault();m.thread=8;self.assertEqual(m.vectored()[0],0);self.assertEqual(m.seh_reads,0)
        m=self.fault();m.put(env['exports']['fixture_current_fiber'],0x5678);self.assertEqual(m.vectored()[0],0)
        m=self.fault();m.u.reg_write(UC_X86_REG_EIP,0x58baf2);self.assertEqual(m.vectored()[0],0)

suite=unittest.TestSuite([unittest.defaultTestLoader.loadTestsFromTestCase(env['Guard']),
                         unittest.defaultTestLoader.loadTestsFromTestCase(PiOrdering)])
result=unittest.TextTestRunner(verbosity=2).run(suite)
report={'passed':result.wasSuccessful(),'methods':result.testsRun,'skipped':len(result.skipped),
    'failures':len(result.failures),'errors':len(result.errors),'fixture_sha256':hashlib.sha256(fixture.read_bytes()).hexdigest(),
    'guard_source_sha256':hashlib.sha256((ROOT/'kernel_call_guard.c').read_bytes()).hexdigest(),
    'pi_source_sha256':hashlib.sha256((ROOT/'p2p_inproc.c').read_bytes()).hexdigest(),
    'exact_native_image_supplied':bool(args.image),
    'boundary':'Actual compiled Pi VEH and unchanged migration SEH, real Unicorn AV; Windows dispatch/API/thread/fiber modeled; exact native Cbuf/Dvar image tests require their separate existing proof. No Windows executable launch.'}
(out/'pi-fault-dispatch-receipt.json').write_text(json.dumps(report,indent=2)+'\n')
raise SystemExit(not result.wasSuccessful())
