"""Decode real zones and execute their actual MapTable/MatchService AVM2 paths.

No native DLL/EXE/GFx runtime is loaded. The bounded interpreter supports only
reviewed instructions and refuses unknown opcodes; native table lookup and
localized labels are explicit fixture boundaries backed by decoded zone data.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,struct,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from zonekit import container,zoneimage,codecs,gfx

# Bounded interpreter of only the actual source movie's selected AVM2 bodies.
# Native StringTableLookupByRow and LocalizationRoom strings are explicit data fixtures.
def vm(code,metadata,globals_,locals_):
 code=bytes.fromhex(code) if isinstance(code,str) else code;pc=0;stack=[];locals_=dict(locals_);steps=0
 def u():
  nonlocal pc
  n=0
  for shift in range(0,35,7):
   b=code[pc];pc+=1;n|=(b&127)<<shift
   if not b&128:return n
  raise ValueError('bad u30')
 def branch():
  nonlocal pc
  n=int.from_bytes(code[pc:pc+3],'little',signed=True);pc+=3;return n
 def key(i):return metadata['multinames'].get(str(i))
 def prop(obj,k):
  if isinstance(obj,list) and k in ('push','splice','concat'):
   if k=='push':return lambda item:obj.append(item) or len(obj)
   if k=='splice':return lambda start:obj.__delitem__(slice(start,None))
   return lambda:list(obj)
  return obj[k]
 while pc<len(code):
  steps+=1;assert steps<5000,'bounded VM step count';op=code[pc];pc+=1
  if op in (0xf0,0xf1):u()
  elif op==0xef:pc+=1;u();pc+=1;u()
  elif 0xd0<=op<=0xd3:stack.append(locals_.get(op-0xd0))
  elif op==0x62:stack.append(locals_.get(u()))
  elif 0xd4<=op<=0xd7:locals_[op-0xd4]=stack.pop()
  elif op==0x63:locals_[u()]=stack.pop()
  elif op in (0x30,0x29):stack.pop()
  elif op==0x20:stack.append(None)
  elif op==0x80:u()
  elif op==0x24:
   value=code[pc];pc+=1;stack.append(value if value<128 else value-256)
  elif op==0x2c:stack.append(metadata['strings'][u()])
  elif op in (0x5d,0x5e):u();stack.append(globals_)
  elif op==0x60:stack.append(globals_[key(u())])
  elif op==0x66:
   k=key(u())
   if k is None:k=stack.pop()
   obj=stack.pop();stack.append(prop(obj,k))
  elif op in (0x61,0x68):
   k=key(u());value=stack.pop();obj=stack.pop();obj[k]=value
  elif op==0x46:
   k=key(u());count=u();args=[stack.pop() for _ in range(count)][::-1];obj=stack.pop();stack.append(prop(obj,k)(*args))
  elif op==0x55:
   count=u();obj={}
   for _ in range(count):value=stack.pop();k=stack.pop();obj[k]=value
   stack.append(obj)
  elif op==0x76:stack.append(bool(stack.pop()))
  elif op==0xab:b=stack.pop();a=stack.pop();stack.append(a==b)
  elif op in (0x10,0x11,0x12,0x13,0x14):
   delta=branch()
   if op==0x10:take=True
   elif op in (0x11,0x12):take=bool(stack.pop())==(op==0x11)
   else:b=stack.pop();a=stack.pop();take=(a==b)==(op==0x13)
   if take:pc+=delta
  elif op==0x48:return stack.pop()
  elif op==0x47:return None
  else:raise ValueError('unimplemented actual opcode '+hex(op)+' at '+str(pc-1))
 return None

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--before',type=Path,required=True)
 parser.add_argument('--after',type=Path,required=True)
 parser.add_argument('--ui',type=Path,required=True)
 parser.add_argument('--game',type=Path,required=True)
 parser.add_argument('--receipt',type=Path,required=True)
 args=parser.parse_args()
 spec=importlib.util.spec_from_file_location('endless_abc_reader',ROOT/'docs/baselines/20261024-invitations-disabled/abc_reader.py')
 reader=importlib.util.module_from_spec(spec);spec.loader.exec_module(reader)
 def load(path):return zoneimage.parse(container.read_any(str(path),str(args.game/'C1.exe')))
 before,after=load(args.before),load(args.after);assert len(before.assets)==len(after.assets)
 changed=[];old_rows=new_rows=None
 for index,((ot,op,ob),(nt,np,nb)) in enumerate(zip(before.assets,after.assets)):
  assert (ot,op)==(nt,np),'asset order/type/pointer changed'
  if ob==nb:continue
  assert ot==37,'non-table asset changed'
  old_name,old_rows,_=codecs.decode_stringtable(ob);new_name,new_rows,_=codecs.decode_stringtable(nb)
  assert old_name==new_name=='mp/comaplist.csv';changed.append(index)
 assert len(changed)==1 and len(old_rows)==len(new_rows)
 cells=[]
 for a,b in zip(old_rows,new_rows):
  assert len(a)==len(b)
  for col,(x,y) in enumerate(zip(a,b)):
   if x!=y:cells.append((int(a[0]),col,x.decode('ascii'),y.decode('ascii')))
 assert cells==[(54,23,'0','1'),(55,23,'0','1'),(72,23,'0','1'),(82,23,'0','1')],cells
 rows=new_rows
 movies=[codecs.decode_rawfile(blob)[1] for tid,ptr,blob in load(args.ui).assets if tid==36 and codecs.decode_rawfile(blob)[0]=='scaleform/default/codo.gfx']
 assert len(movies)==1;movie=movies[0];proof={}
 for code,start,end,name in gfx.tags(movie):
  if code!=82 or name not in ('codo/constants/MapTable','codo/services/MatchService','codo/utils/TableUtil'):continue
  tag=movie[start:end];pos=6 if struct.unpack_from('<H',tag)[0]&63==63 else 2;pos=tag.index(b'\0',pos+4)+1;abc=reader.read(tag[pos:])
  proof[name]={'strings':abc['values']['string'],'multinames':{str(i):reader.qname(abc,i) for i,m in enumerate(abc['values']['multiname']) if i and m[0] in (7,13,9,14,15,16)},'bodies':{str(i):{'name':v['name'],'code':v['code'].hex()} for i,v in abc['bodies'].items()}}
 assert len(proof)==3
 def method(meta,name):
  values=[v for v in meta['bodies'].values() if v['name'].endswith('/'+name)]
  assert len(values)==1,name
  return values[0]['code']
 mt=proof['codo/constants/MapTable'];constants={};vm(mt['bodies']['0']['code'],mt,constants,{0:constants})
 assert constants['DIFFICULTY_PLACEHOLDER']==23 and constants['PATH'].lower()=='mp/comaplist.csv'
 service=proof['codo/services/MatchService'];util=proof['codo/utils/TableUtil'];code=bytes.fromhex(method(service,'listMap'))
 fragment=bytes.fromhex('62095d89016689015d840166840166880162055d840166840166ea01468a01032cc901ab61eb01');assert code.count(fragment)==1
 results=[]
 for rownum,row in enumerate(rows):
  if row[0] not in (b'54',b'55',b'72',b'82'):continue
  for flag in ('0','1'):
   values=list(row);values[23]=flag.encode();calls=[]
   def lookup(path,index,column):
    assert path.lower()=='mp/comaplist.csv' and index==rownum and column==23
    calls.append((path,index,column));return values[column].decode('ascii')
   map_={'id':row[4].decode('ascii'),'difficultyEasy':True,'difficultyNormal':True,'difficultyHard':True}
   native={'CFunctionContainerCommon':{'StringTableLookupByRow':lookup}}
   tableutil={'getValue':lambda *a:vm(method(util,'getValue'),util,native,{0:{},1:a[0],2:a[1],3:a[2]})}
   vm(fragment,service,{'TableUtil':tableutil,'MapTable':constants},{9:map_,5:rownum})
   assert map_['difficultyPlaceholder']==(flag=='1') and len(calls)==1
   state={'_levels':[],'getMapObj':lambda name:map_}
   result=vm(method(service,'listDifficulty'),service,{'LocalizationRoom':{'ROOM_LEVELS':['Easy','Normal','Hard','Endless']}},{0:state,1:map_,2:None})
   actual=[x['id'] for x in result];assert actual==([0,1,2,3] if flag=='1' else [0,1,2])
   results.append({'row_id':int(row[0]),'map':map_['id'],'evaluated_flag':flag,'difficulty_ids':actual,'native_lookup_args':calls})
 assert len(results)==8
 report={'passed':True,'only_changed_cells':cells,'all_other_asset_bytes_identical':True,'same_asset_order_and_count':True,'cases':results,'actual_source_movie_sha256':hashlib.sha256(movie).hexdigest(),'inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.before,args.after,args.ui)},'limits':'Selected actual AVM2 bodies in bounded interpreter; native table lookup/localized strings are explicit fixtures. This is data/menu-consumer evidence, not live GFx/native gameplay or startup difficulty propagation.'}
 args.receipt.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':True,'exact_cells':len(cells),'compiled_consumer_cases':len(results),'all_other_asset_bytes_identical':True}))

if __name__=='__main__':main()
