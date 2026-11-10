"""Finite target-body transplant; original pools/tables/other bodies untouched."""
import struct
import verify as V,swf_data as S,symbols as Y
A,L=V.A,V.L
need=S.need

def target(x):
 row=A.method(x,'k33SetLabel');ids=[i for i,b in x['bodies'].items()if b['name']==row['name']];need(len(ids)==1,'unique named target');return ids[0],row

def contract(x):
 i,b=target(x);return dict(name=b['name'],signature=Y.tables(x)['methods'][i],metadata={k:b[k]for k in('max_stack','regs','init_scope','max_scope','exceptions')},instructions=resolved_target(x,b['code']))

def resolved_target(x,code):
 ops,debug=V.decode(code);result=[]
 for op,v in ops:
  if op in('getlex','coerce','getproperty','setproperty'):v=Y.mn(x,v)
  elif op=='string':v=x['values']['string'][v]
  elif op=='callproperty':v=(Y.mn(x,v[0]),v[1])
  result.append((op,v))
 return dict(code=result,debug=[(k,x['values']['string'][n],reg,line)for k,n,reg,line in debug])

def qualify_baseline(original,baseline):
 old,new=V.unique39(original),V.unique39(baseline)
 need(Y.tables(old)==Y.tables(new),'resolved baseline ABI differs')
 need(contract(old)==contract(new),'baseline TARGET full resolved contract differs')
 oi,ob=target(old);ni,nb=target(new);need(oi==ni and ob['raw']==nb['raw'],'baseline TARGET raw reproduction differs')
 return dict(target_raw_exact=True,target_resolved_all_refs_exact=True,full_resolved_ABI_exact=True,whole_class_baseline_still_HOLD=True)

def remap_code(code,candidate,original):
 # Only actual finite target instruction forms are supported. No algebraic or
 # general bytecode conversion; emit same opcode order with symbol-index fixes.
 need(len(code)<=1024,'bounded target code');r=A.Reader(code);rows=[];mapping=[]
 def pool(kind,index):
  values=candidate['values'][kind];need(0<index<len(values),'pool reference extent')
  meaning=Y.mn(candidate,index)if kind=='multiname'else values[index]
  original_values=original['values'][kind]
  def original_meaning(i):return Y.mn(original,i)if kind=='multiname'else original_values[i]
  if index<len(original_values)and original_meaning(index)==meaning:dest=index
  else:
   found=[i for i in range(1,len(original_values))if original_meaning(i)==meaning];need(len(found)==1,'target reference absent or ambiguous in ORIGINAL pools');dest=found[0]
  mapping.append(dict(kind=kind,source_index=index,original_index=dest,meaning=meaning));return dest
 while r.pos<len(code):
  start=r.pos;op=r.u8();args=[]
  if op==0xef:args=[r.u8(),pool('string',r.u30()),r.u8(),r.u30()]
  elif op in(0x60,0x80,0x66,0x61):args=[pool('multiname',r.u30())]
  elif op==0x2c:args=[pool('string',r.u30())]
  elif op==0x46:args=[pool('multiname',r.u30()),r.u30()]
  elif op in(0x62,0x63):args=[r.u30()]
  elif op==0x24:args=[r.u8()]
  elif op in(0x12,0x14):args=[r.pos+3+int.from_bytes(r.take(3),'little',signed=True)]
  else:need(op in {0x30,0x20,0x47,0x87,0xab,0x96,0x2a,0x29,0xa0,0xa1,0xa3,0x75,*range(0xd0,0xd8)},'unsupported target opcode')
  rows.append((start,op,args))
 positions={start:i for i,(start,_,_)in enumerate(rows)}
 for _,op,args in rows:
  if op in(0x12,0x14):need(args[0]in positions,'target branch instruction boundary');args[0]=positions[args[0]]
 def emit(op,args,offsets=None,at=0):
  raw=bytes([op])
  if op==0xef:return raw+bytes([args[0]])+L.u30(args[1])+bytes([args[2]])+L.u30(args[3])
  if op in(0x60,0x80,0x66,0x61,0x2c,0x62,0x63):return raw+L.u30(args[0])
  if op==0x46:return raw+L.u30(args[0])+L.u30(args[1])
  if op==0x24:return raw+bytes(args)
  if op in(0x12,0x14):return raw+(0 if offsets is None else offsets[args[0]]-at-4).to_bytes(3,'little',signed=True)
  return raw
 offsets=[];n=0
 for _,op,args in rows:offsets.append(n);n+=len(emit(op,args))
 output=b''.join(emit(op,args,offsets,offsets[i])for i,(_,op,args)in enumerate(rows));V.decode(output)
 return output,mapping

def patch_tag(original_tag,candidate_tag,expanded):
 original,candidate=S.abc(original_tag),S.abc(candidate_tag);a,b=V.unique39(original),V.unique39(candidate)
 need(Y.tables(a)==Y.tables(b),'candidate full resolved ABI differs')
 oi,ob=target(a);ni,nb=target(b);need(oi==ni,'target method-index changed')
 row=next(z for z in L.parse(candidate)['bodies']if z['method']==ni);oldrow=next(z for z in L.parse(original)['bodies']if z['method']==oi)
 need(row['exceptions']==[] and row['traits']==oldrow['traits'],'target exception/trait drift')
 code,mapping=remap_code(nb['code'],b,a)
 newbody=b''.join(L.u30(v)for v in(oi,nb['max_stack'],nb['regs'],nb['init_scope'],nb['max_scope'],len(code)))+code+L.u30(0)+oldrow['traits']
 patched=original[:oldrow['begin']]+newbody+original[oldrow['end']:]
 head=6 if struct.unpack_from('<H',original_tag)[0]&63==63 else 2;prefix=original_tag[head:len(original_tag)-len(original)];payload=prefix+patched;tag=struct.pack('<HI',(82<<6)|63,len(payload))+payload
 proof=V.compiled(original_tag,tag,expanded)
 pa=V.unique39(patched);need(pa['tables']==a['tables']and pa['pools']==a['pools'],'ORIGINAL pools/tables must remain BYTEEXACT')
 need(all(pa['bodies'][i]['raw']==body['raw']for i,body in a['bodies'].items()if i!=oi),'all38 ORIGINAL bodies must remain BYTEEXACT')
 need(resolved_target(b,nb['code'])==resolved_target(pa,code),'remapping changed resolved target instruction/debug identities')
 return tag,dict(checks=proof,reference_mapping=mapping,original_pools_tables_and_other38_byteexact=True,candidate_other38_NOT_PUBLISHED=True)
