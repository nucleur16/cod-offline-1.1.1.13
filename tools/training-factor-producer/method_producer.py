"""PROPOSED deterministic two-body AVM2 transformation; no VM/compiler invocation.

Pins the exact previously reviewed current code and reuses its whole Ranked/null
fallback tail. No pools, symbols, signatures, traits, or other bodies are emitted.
Root/independent review required before product artifact integration.
"""
from pathlib import Path
import struct,hashlib
import abc_reader as A,abc_list_reader as L,swf_data as S
D=Path(__file__).parent
NAMES=('getXPLeagueFactor','getGPLeagueFactor')
TAG_SHA='352ca72c0d9110e1be2126467776167402bcf20bce30c65ec5d4a5dcba960c53'
BODY_SHA=('eb098ffcb552a2a6039819a4a657aa901d86ba0feed1ce9e298a21bc90738e2b','b5b33d1631aac997c65f1c4b77cafa24bf206a6fa20e850cbd3d7fc272f7fc42')
SOURCE_SHA='68997250fac46bf059c8d028e27f3cfcccd5ef4f0f667a53b618679e1edc1615'
MOVIE_SHA='dfdac47bcead1047bbd9625f41977a3fe0ed2570c608567e58fcf8c96d1c9836'
PROPOSED_TAG_SHA='f653ccba172d6a97cfdf65c535e9e94c6e6bd7bca301343759460c0a70703a1c'
def need(x,s):
    if not x:raise ValueError(s)
def sha(b):return hashlib.sha256(b).hexdigest()
def abc(tag):
    need(len(tag)>10 and int.from_bytes(tag[:2],'little')>>6==82,'DoABC tag')
    h=6 if int.from_bytes(tag[:2],'little')&63==63 else 2
    size=struct.unpack_from('<I',tag,2)[0] if h==6 else int.from_bytes(tag[:2],'little')&63
    need(len(tag)==h+size,'exact tag extent');at=tag.index(b'\0',h+4)+1
    need(tag[h+4:at-1]==b'codo/controllers/MatchController','exact class tag');return tag[at:],tag[h:at]
def decode(code):
    need(len(code)<=512,'bounded two-factor method');r=A.Reader(code);rows=[]
    single={0x20,0x30,0x75,0xab,0x96,0x2a,0x29,0x48,*range(0xd0,0xd8)}
    while r.pos<len(code):
        at=r.pos;op=r.u8();args=[]
        if op==0xef:args=[r.u8(),r.u30(),r.u8(),r.u30()]
        elif op in (0x60,0x63,0x66,0x2c,0x2f,0x62):args=[r.u30()]
        elif op==0x46:args=[r.u30(),r.u30()]
        elif op==0x24:args=[r.u8()]
        elif op in (0x10,0x11,0x12,0x14):
            delta=int.from_bytes(r.take(3),'little',signed=True);args=[r.pos+delta]
        else:need(op in single,'unsupported exact factor opcode')
        rows.append(dict(label=('old',at),op=op,args=args))
    positions={z['label'][1] for z in rows}
    for z in rows:
        if z['op'] in (0x10,0x11,0x12,0x14):need(z['args'][0] in positions,'branch boundary');z['args']=[('old',z['args'][0])]
    return rows
def encode(rows):
    def emit(z,positions=None,at=0):
        op=z['op'];args=z['args'];b=bytes([op])
        if op==0xef:return b+bytes([args[0]])+L.u30(args[1])+bytes([args[2]])+L.u30(args[3])
        if op in (0x60,0x63,0x66,0x2c,0x2f,0x62):return b+L.u30(args[0])
        if op==0x46:return b+L.u30(args[0])+L.u30(args[1])
        if op==0x24:return b+bytes(args)
        if op in (0x10,0x11,0x12,0x14):
            if positions is None:return b+b'\0\0\0'
            need(args[0] in positions,'retained target label');return b+(positions[args[0]]-at-4).to_bytes(3,'little',signed=True)
        return b
    positions={};at=0
    for z in rows:need(z['label'] not in positions,'duplicate label');positions[z['label']]=at;at+=len(emit(z))
    out=b''.join(emit(z,positions,positions[z['label']])for z in rows);decode(out);return out
def changed_code(code):
    rows=decode(code)
    # Both exact original methods have two occurrences of p != null. The second
    # begins the Ranked branch, preserved byte/semantic instruction for instruction.
    starts=[i for i in range(len(rows)-3) if [z['op']for z in rows[i:i+4]]==[0xd3,0x20,0xab,0x96]]
    need(len(starts)==2,'exact two original null guards')
    ranked=starts[1]
    # Initial COMBAT comparison ends just before the extra name-dependent AND.
    end=next(i for i in range(starts[0]+4,ranked)if rows[i]['op']==0xab)
    need([z['op']for z in rows[end-5:end+1]]==[0x29,0xd3,0x66,0x60,0x66,0xab],'COMBAT type comparison')
    prefix=[dict(z,args=list(z['args']))for z in rows[:end+1]]
    branches=[z for z in prefix if z['op'] in (0x10,0x11,0x12,0x14)]
    need(len(branches)==1 and branches[0]['op']==0x12,'initial short-circuit null guard')
    branches[0]['args']=[('new','combat-if-false')]
    returns=[z for z in rows if z['op']==0x62 and z['args']==[4]]
    need(len(returns)==1,'unique original factor return')
    added=[dict(label=('new','combat-if-false'),op=0x12,args=[rows[ranked]['label']]),dict(label=('new','one'),op=0x24,args=[1]),dict(label=('new','number'),op=0x75,args=[]),dict(label=('new','factor'),op=0x63,args=[4]),dict(label=('new','end'),op=0x10,args=[returns[0]['label']])]
    suffix=rows[ranked:]
    out=encode(prefix+added+suffix)
    return out,dict(initial_guard_prefix_retained=True,removed_only_name_AND_and_half_ternary=True,full_ranked_and_service_fallback_tail_retained=True,new_factor=1,no_new_constants_or_locals=True)
def produce(tag,source):
    need(sha(tag)==TAG_SHA,'exact original reviewed MatchController tag');need(sha(source)==SOURCE_SHA,'exact reviewed two-factor source')
    before,prefix=abc(tag);listing=L.parse(before);old=A.read(before)
    need(len(listing['bodies'])==len({z['method']for z in listing['bodies']})==166,'166 declared and unique bodies')
    edits=[];proof=[]
    for name,pin in zip(NAMES,BODY_SHA):
        body=A.method(old,name);need(sha(body['raw'])==pin,'exact original factor body')
        index=next(i for i,b in old['bodies'].items()if b['name']==body['name']);row=next(z for z in listing['bodies']if z['method']==index)
        need(body['exceptions']==0 and body['max_stack']==3 and body['regs']==5,'original scope/local/stack contract')
        code,p=changed_code(body['code'])
        newbody=b''.join(L.u30(v)for v in(index,body['max_stack'],body['regs'],body['init_scope'],body['max_scope'],len(code)))+code+L.u30(0)+row['traits']
        edits.append((row['begin'],row['end'],newbody));proof.append(dict(name=name,**p))
    after=before
    for start,end,data in sorted(edits,reverse=True):after=after[:start]+data+after[end:]
    new=A.read(after);strict=A.compare(before,after,list(NAMES))
    need(old['tables']==new['tables']and old['pools']==new['pools'],'all original tables and pools byteexact')
    newlist=L.parse(after);need(len(newlist['bodies'])==len({z['method']for z in newlist['bodies']})==166,'output unique166')
    payload=prefix+after;out=struct.pack('<HI',(82<<6)|63,len(payload))+payload
    return out,dict(strict=strict,methods=proof,original_tables_pools_and_other164_exact=True,compiler_invoked=False,AVM2_executed=False,runtime_or_release_ready=False)

def splice_movie(movie,tag):
    need(sha(movie)==MOVIE_SHA,'exact vertical-reviewed current movie DATA')
    need(sha(tag)==PROPOSED_TAG_SHA,'exact proposed two-body transformed tag')
    abc(tag);before=S.tags(movie);need(len(before)==6867,'exact movie inventory')
    hits=[r for r in before if r['code']==82 and r['name']=='codo/controllers/MatchController']
    need(len(hits)==1,'one exact factor class');r=hits[0]
    need(sha(movie[r['begin']:r['end']])==TAG_SHA,'exact original class in movie')
    out=bytearray(movie[:r['begin']]+tag+movie[r['end']:]);struct.pack_into('<I',out,4,len(out));out=bytes(out)
    after=S.tags(out);need(len(after)==len(before),'same movie tag count')
    for a,b in zip(before,after):
        need((a['code'],a['flags'],a['name'])==(b['code'],b['flags'],b['name']),'movie class order/flags unchanged')
        if a is not r:need(movie[a['begin']:a['end']]==out[b['begin']:b['end']],'all6866 sibling tags exact')
    need(movie[:4]==out[:4] and movie[8:S.header_len(movie)]==out[8:S.header_len(out)],'original movie header retained')
    return out,dict(other6866_tags_byteexact=True,original_header_retained=True,original_vertical_OptionView_retained=True)

if __name__=='__main__':print('PROPOSED pure DATA producer; no CLI write/integration entry')
