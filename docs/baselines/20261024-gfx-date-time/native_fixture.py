"""Execute original C1 Date ctor/setTime/hour/minute bodies offline.

Only constructor clock output and GetTimeZoneInformation are named doubles.
No instruction patch, process, native app, account or network is used.
"""
from pathlib import Path
import datetime
import hashlib
import json
import os
import struct
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UcError
from unicorn.x86_const import *

import argparse
parser=argparse.ArgumentParser()
parser.add_argument('--image',type=Path,required=True)
parser.add_argument('--bridge',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
raw=args.image.read_bytes()
SHA=hashlib.sha256(raw).hexdigest()
assert SHA in {'a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976',
 'd8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60'}, 'unsupported owned image'
SHIPPING=SHA.startswith('a5a7')
PE=pefile.PE(data=raw);BASE=PE.OPTIONAL_HEADER.ImageBase;MAPPED=PE.get_memory_mapped_image()



class DateFixture:
 def __init__(self,epoch,bias,daylight,status):
  self.epoch,self.bias,self.daylight,self.status=epoch,bias,daylight,status
  self.uc=Uc(UC_ARCH_X86,UC_MODE_32)
  self.uc.mem_map(BASE,(len(MAPPED)+4095)&~4095);self.uc.mem_write(BASE,MAPPED)
  self.uc.mem_map(0x30000000,0x10000);self.obj=0x30001000;self.out=0x30002000
  self.uc.mem_map(0x40000000,0x100000);self.stack=0x40080000
  self.uc.mem_map(0x20000000,0x10000);self.stop=0x2000fff0
  self.uc.mem_write(self.stop,b'\xc3')
  self.imports={};self.calls=[]
  for library in PE.DIRECTORY_ENTRY_IMPORT:
   for item in library.imports:
    at=0x20000000+len(self.imports)*16
    self.imports[at]=(item.name or b'ordinal').decode('ascii','replace')
    self.uc.mem_write(item.address,struct.pack('<I',at))
  if SHIPPING:
   at=0x20000000+len(self.imports)*16;self.imports[at]='GetTimeZoneInformation'
   self.uc.mem_write(0xb4222c,struct.pack('<I',at))
  self.uc.hook_add(UC_HOOK_CODE,self.code)
  self.uc.reg_write(UC_X86_REG_FPCW,0x037f)
 def ret(self,cleanup=0,result=0):
  sp=self.uc.reg_read(UC_X86_REG_ESP);target=struct.unpack('<I',self.uc.mem_read(sp,4))[0]
  self.uc.reg_write(UC_X86_REG_EAX,result);self.uc.reg_write(UC_X86_REG_ESP,sp+4+cleanup);self.uc.reg_write(UC_X86_REG_EIP,target)
 def code(self,machine,at,size,unused):
  if at==self.stop:machine.emu_stop();return
  sp=machine.reg_read(UC_X86_REG_ESP)
  if at==0xae233a:
   pointer=struct.unpack('<I',machine.mem_read(sp+4,4))[0]
   machine.mem_write(pointer,struct.pack('<qHhh',self.epoch,0,self.bias,1 if self.status==2 else 0))
   self.calls.append('constructor-clock-output');self.ret();return
  if at not in self.imports:return
  name=self.imports[at]
  if name=='GetTimeZoneInformation':
   pointer=struct.unpack('<I',machine.mem_read(sp+4,4))[0]
   data=bytearray(172);struct.pack_into('<i',data,0,self.bias);struct.pack_into('<i',data,84,0);struct.pack_into('<i',data,168,self.daylight)
   machine.mem_write(pointer,bytes(data));self.calls.append(name);self.ret(4,self.status);return
  raise RuntimeError('Unmodeled Date API: '+name)
 def call(self,at,args):
  self.uc.reg_write(UC_X86_REG_ESP,self.stack);self.uc.reg_write(UC_X86_REG_EBP,self.stack+0x100)
  self.uc.reg_write(UC_X86_REG_ECX,self.obj)
  self.uc.mem_write(self.stack,struct.pack('<I',self.stop)+args)
  try:self.uc.emu_start(at,self.stop+1,timeout=2_000_000,count=2_000_000)
  except UcError as error:raise RuntimeError('Date stopped PC='+hex(self.uc.reg_read(UC_X86_REG_EIP))+' calls='+repr(self.calls)) from error
  if self.uc.reg_read(UC_X86_REG_EIP)!=self.stop:raise RuntimeError('Date body did not return')
 def hm(self):
  values=[]
  for at in (0x88aa60,0x88ab50):
   self.call(at,struct.pack('<I',self.out));values.append(struct.unpack('<d',self.uc.mem_read(self.out,8))[0])
  return values
 def state(self):
  return dict(cached_offset_ms=struct.unpack('<i',self.uc.mem_read(self.obj+0x20,4))[0],
    dynamic_DST_flag=self.uc.mem_read(self.obj+0x24,1)[0],epoch_ms=struct.unpack('<d',self.uc.mem_read(self.obj+0x28,8))[0])
