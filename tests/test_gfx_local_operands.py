"""Actual C1 generic-local producers, using bytes built by the shipping policy.
Only abstract-type assignment is mocked; original stack accounting, word-array
writers and retained cleanup/refcount instructions execute in Unicorn.
"""
import json,os,pathlib,struct,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
GAME=pathlib.Path(os.environ.get('K33_GAME',r'D:/CoD/CODOL Builds/Call of Duty Online 1.1.1.13'))
DRIVER=r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gfx_local_operands_policy.h"
int main(void){
 unsigned char source[288],out[288],changed[144],sentinel[144];
 assert(fread(source,1,288,stdin)==288);
 const unsigned bases[2]={0x9117a0,0x911830};
 for(unsigned k=0;k<2;k++){
  unsigned char *s=source+k*144,*o=out+k*144;
  assert(GfxBuildLocalOperandProducer(bases[k],s,144,o));
  for(unsigned i=0;i<144;i++){
   memcpy(changed,s,144);changed[i]^=1;memset(sentinel,0xa5,144);
   assert(!GfxBuildLocalOperandProducer(bases[k],changed,144,sentinel));
   for(unsigned j=0;j<144;j++)assert(sentinel[j]==0xa5);
  }
  assert(!GfxBuildLocalOperandProducer(bases[k],o,144,changed));
  assert(!GfxBuildLocalOperandProducer(bases[k],s,143,changed));
  assert(!GfxBuildLocalOperandProducer(bases[k],s,145,changed));
  assert(!GfxBuildLocalOperandProducer(bases[k],s,144,s));
  assert(!GfxBuildLocalOperandProducer(bases[k],NULL,144,changed));
  assert(!GfxBuildLocalOperandProducer(bases[k],s,144,NULL));
  assert(!GfxBuildLocalOperandProducer(0x9117a1,s,144,changed));
 }
 assert(fwrite(out,1,288,stdout)==288);
 return 0;
}
"""
class GfxLocalOperands(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  try:
   import pefile,unicorn
  except ImportError:raise unittest.SkipTest('Needs pefile and Unicorn for primary C1 proof')
  if not (GAME/'C1.exe').exists():raise unittest.SkipTest('C1.exe fixture unavailable')
  cls.pe=pefile.PE(str(GAME/'C1.exe'),fast_load=True)
  cls.original={a:cls.pe.get_data(a-cls.pe.OPTIONAL_HEADER.ImageBase,0x90) for a in (0x9117a0,0x911830)}
  gcc=pathlib.Path('C:/msys64/mingw32/bin/gcc.exe') if os.name=='nt' else pathlib.Path('gcc')
  env=os.environ.copy()
  if os.name=='nt':env['PATH']=str(gcc.parent)+';'+env['PATH']
  with tempfile.TemporaryDirectory() as temp:
   c=pathlib.Path(temp)/'policy.c';exe=pathlib.Path(temp)/('policy.exe' if os.name=='nt' else 'policy')
   c.write_text(DRIVER)
   subprocess.run([str(gcc),'-std=c11','-Wall','-Wextra','-Werror','-I',str(ROOT/'src'/'gfx'),str(c),'-o',str(exe)],env=env,capture_output=True,check=True)
   run=subprocess.run([str(exe)],input=b''.join(cls.original.values()),capture_output=True,env=env,check=True,
                      creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
  if len(run.stdout)!=288:raise AssertionError('Policy driver did not return two complete guarded spans')
  cls.fixed={a:run.stdout[i*144:(i+1)*144] for i,a in enumerate(cls.original)}
  cls.results=[]
 @classmethod
 def tearDownClass(cls):
  out=ROOT/'obj/quit_trace';out.mkdir(parents=True,exist_ok=True)
  (out/'policy_producer_proof.json').write_text(json.dumps(cls.results,indent=2)+'\n')
 def test_captured_abc_operand_and_faulting_wordcode_contract(self):
  fixture=json.loads((ROOT/'docs/baselines/20261029-gfx-local-operands/crash_repro.json').read_text())
  code=bytes.fromhex(fixture['original_code_hex'])
  self.assertIn(bytes.fromhex(fixture['original_increment_bytes']),code)
  self.assertEqual(fixture['local_count'],2)
  self.assertEqual(fixture['captured_runtime_wordcode'][:2],[0x92,0xd1])
  self.assertEqual(fixture['expected_runtime_wordcode'][:3],[0x92,1,0xd1])
  self.assertEqual(fixture['register_base']+16*fixture['misread_register_index'],fixture['misread_value_address'])

 def test_guarded_policy_preserves_native_body_and_rebases_only_three_calls(self):
  for a,old in self.original.items():
   fixed=self.fixed[a]
   self.assertEqual(fixed[:11],old[:11]);self.assertEqual(fixed[11:14],bytes.fromhex('ff7508'))
   expected=old[:11]+bytes.fromhex('ff7508')+old[11:138]+b'\xcc'*3
   changed=set()
   for at,target in [(0x13,0x907870),(0x4a,0x907e30),(0x82,0x9058d0)]:
    self.assertEqual(fixed[at],0xe8)
    self.assertEqual(a+at+5+struct.unpack_from('<i',fixed,at+1)[0],target)
    changed.update(range(at+1,at+5))
   self.assertEqual([i for i in range(144) if fixed[i]!=expected[i] and i not in changed],[])
   self.assertEqual(fixed[-6:],bytes.fromhex('c20400cccccc'))
 def emulate(self,op,index,fixed,cleanup='plain'):
  from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
  from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP
  u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(self.pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
  for sec in self.pe.sections:u.mem_write(0x400000+sec.VirtualAddress,sec.get_data())
  u.mem_map(0x100000,0x20000);u.mem_map(0x200000,0x10000);u.mem_map(0x300000,0x1000)
  def w(a,v):u.mem_write(a,struct.pack('<I',v))
  def r(a):return struct.unpack('<I',u.mem_read(a,4))[0]
  state,tracer,mi=0x100000,0x101000,0x101800
  for a,v in [(state,tracer),(state+12,0x10a000),(tracer+20,0x102000),(0x102014,0x103000),
              (0x103014,0x104000),(0x104110,0x105000),(0x105064,0x107000),(tracer+0x34,0x10c000),
              (tracer+24,mi),(mi+4,0x109000),(mi+12,256),(tracer+0x48,0x108000),(tracer+0x50,256)]:w(a,v)
  node=0x10b000;w(node,1 if cleanup=='ref_free' else 2);w(node+12,2)
  w(0xcde088,0x10b100);w(0x10b100,0x10b200);w(0x10b230,0x300100)
  converted=[];freed=[];release=[]
  def ret(n):
   sp=u.reg_read(UC_X86_REG_ESP);ra=r(sp);u.reg_write(UC_X86_REG_ESP,sp+4+n);u.reg_write(UC_X86_REG_EIP,ra)
  def hook(uc,a,size,user):
   if a==0x907e30:
    converted.append(u.reg_read(UC_X86_REG_ECX));sp=u.reg_read(UC_X86_REG_ESP);value=r(sp+4)
    if cleanup.startswith('ref_'):u.mem_write(value,struct.pack('<IIII',0x20a,node,0,0))
    elif cleanup=='strong_string':u.mem_write(value,struct.pack('<IIII',0xa,0,node,0))
    ret(4)
   elif a==0x9058d0:release.append(a) # Execute actual retained cleanup helper.
   elif a==0x300100:
    freed.append(r(u.reg_read(UC_X86_REG_ESP)+4));ret(4)
  u.hook_add(UC_HOOK_CODE,hook)
  entry=0x9117a0 if op==0x92 else 0x911830
  if fixed:u.mem_write(entry,self.fixed[entry])
  sp=0x208000;w(sp,0x300000);w(sp+4,index)
  saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,UC_X86_REG_EDI:0x34567890,UC_X86_REG_EBP:0x207000}
  for reg,value in saved.items():u.reg_write(reg,value)
  u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,state)
  u.emu_start(entry,0x300000,count=4000)
  words=[r(0x109000+4*i) for i in range(r(mi+8))]
  self.assertEqual(words,[op,index] if fixed else [op]);self.assertEqual(converted,[0x10a000+16*index])
  self.assertEqual(u.reg_read(UC_X86_REG_ESP),sp+8)
  for reg,value in saved.items():self.assertEqual(u.reg_read(reg),value)
  self.assertEqual(freed,[node] if cleanup=='ref_free' else [])
  self.assertEqual(release,[0x9058d0] if cleanup=='strong_string' else [])
  if cleanup=='ref_keep':self.assertEqual(r(node),1)
  if cleanup=='ref_free':self.assertEqual(r(node),0)
  if cleanup=='strong_string':self.assertEqual(r(node+12),1)
  self.results.append(dict(opcode=hex(op),local=index,fixed=fixed,cleanup=cleanup,emitted=words,
                           stack_balanced=True,nonvolatile_preserved=True,freed=freed))
 def test_actual_native_writers_at_16_original_and_fixed_operand_boundaries(self):
  for op in (0x92,0x94):
   for local in (1,127,128,4095):
    for fixed in (False,True):
     with self.subTest(op=op,local=local,fixed=fixed):self.emulate(op,local,fixed)
 def test_native_cleanup_refcounts_and_free_path_unchanged(self):
  for op in (0x92,0x94):
   for cleanup in ('ref_keep','ref_free','strong_string'):
    for fixed in (False,True):
     with self.subTest(op=op,cleanup=cleanup,fixed=fixed):self.emulate(op,1,fixed,cleanup)
if __name__=='__main__':unittest.main()
