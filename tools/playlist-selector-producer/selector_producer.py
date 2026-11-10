"""Exact one-body DATA successor to reviewed factor endpoint; no compiler/VM."""
import hashlib,struct
import abc_reader as A,abc_list_reader as L,swf_data as S
TAG_SHA='f653ccba172d6a97cfdf65c535e9e94c6e6bd7bca301343759460c0a70703a1c'
MOVIE_SHA='5dfe517c983d2e4ad8fe8254914bf9dfc67e4c040ccafd542f27a0a29520b846'
SOURCE_SHA='03f65de60ecabfb78295023ebc1fa9db373b39d58c72f33e2a64eaffc3435886'
BEFORE_SOURCE_SHA='68997250fac46bf059c8d028e27f3cfcccd5ef4f0f667a53b618679e1edc1615'
BODY_SHA='fddc0fecd255aec6827577262871ab082be786ac99467caaa68fc12b8f5048c7'
VERTICAL_SHA='2a41d1ba645cd58e648c9d397abf73c2b4bb7e7d48edcd849ff825a98b6e9c81'
OUTPUT_TAG_SHA='97a7299d7f54528611ba73da9b1201976fa80cc182c5d9f7c21d9b297a3596e8'
OUTPUT_MOVIE_SHA='38dd0456193d9025f5831de44f9d2deb70d433f42ea7b8d0f76b7236f558ea37'
NAME='listPlayListNew'
BRANCH=(0x10,0x11,0x12)
ONE=(0x08,0x60,0x63,0x66,0x62,0x80,0x56)
TWO=(0x46,0x4f,0x32)
SINGLE=(0x20,0x23,0x26,0x30,0x82,0xab,0x96,0x2a,0x29,0x48,0x09,*range(0xd0,0xd8))
def need(v,s):
    if not v:raise ValueError(s)
def sha(b):return hashlib.sha256(b).hexdigest()
def abc(tag):
    need(type(tag)is bytes and len(tag)>10,'immutable complete tag')
    word=int.from_bytes(tag[:2],'little');h=6 if word&63==63 else 2
    n=struct.unpack_from('<I',tag,2)[0]if h==6 else word&63
    need(word>>6==82 and len(tag)==h+n,'exact DoABC extent')
    at=tag.index(b'\0',h+4)+1
    need(tag[h:h+4]==b'\x01\0\0\0' and tag[h+4:at-1]==b'codo/controllers/MatchController','exact class/flags')
    return tag[at:],tag[h:at]
def decode(code):
    need(type(code)is bytes and len(code)<=256,'bounded exact selector code')
    r=A.Reader(code);rows=[]
    while r.pos<len(code):
        at=r.pos;op=r.u8();args=[]
        if op==0xef:args=[r.u8(),r.u30(),r.u8(),r.u30()]
        elif op in ONE:args=[r.u30()]
        elif op in TWO:args=[r.u30(),r.u30()]
        elif op==0x24:args=[r.u8()]
        elif op in BRANCH:
            delta=int.from_bytes(r.take(3),'little',signed=True);args=[r.pos+delta]
        else:need(op in SINGLE,'unreviewed selector opcode')
        rows.append(dict(label=at,op=op,args=args))
    positions={z['label']for z in rows}
    for z in rows:
        if z['op']in BRANCH:need(z['args'][0]in positions,'branch instruction boundary')
    return rows
def encode(rows):
    def emit(z,positions=None,at=0):
        op=z['op'];a=z['args'];b=bytes([op])
        if op==0xef:return b+bytes([a[0]])+L.u30(a[1])+bytes([a[2]])+L.u30(a[3])
        if op in ONE:return b+L.u30(a[0])
        if op in TWO:return b+L.u30(a[0])+L.u30(a[1])
        if op==0x24:return b+bytes(a)
        if op in BRANCH:
            if positions is None:return b+b'\0\0\0'
            need(a[0]in positions,'no retained branch into removed id clause')
            return b+(positions[a[0]]-at-4).to_bytes(3,'little',signed=True)
        return b
    positions={};at=0
    for z in rows:
        need(z['label']not in positions,'unique instruction label');positions[z['label']]=at;at+=len(emit(z))
    out=b''.join(emit(z,positions,positions[z['label']])for z in rows)
    decode(out);return out
def changed_code(code,parsed):
    rows=decode(code);by={z['label']:z for z in rows}
    # Closed preimage: precisely the dup/short-circuit AND of (id==0 || id==2).
    expected=[(104,0x2a,[]),(105,0x12,[130]),(109,0x29,[]),
       (110,0xd3,[]),(111,0x66,[683]),(114,0x24,[0]),(116,0xab,[]),
       (117,0x2a,[]),(118,0x11,[130]),(122,0x29,[]),(123,0xd3,[]),
       (124,0x66,[683]),(127,0x24,[2]),(129,0xab,[])]
    removed=[z for z in rows if 104<=z['label']<130]
    need([(z['label'],z['op'],z['args'])for z in removed]==expected,'exact old id0/id2 conjunction')
    need(A.qname(parsed,683)=='id' and A.qname(parsed,667)=='type' and
         A.qname(parsed,689)=='combatTrainingType','exact semantic fields')
    need(by[103]['op']==0xab and by[130]['op']==0x2a and by[131]['op']==0x12 and by[131]['args']==[147],
         'type result feeds original CT short circuit')
    retained=[z for z in rows if not 104<=z['label']<130]
    out=encode(retained)
    # Encoder reflow is structural only. Every retained opcode/operand remains;
    # compare branch destinations by retained instruction index, not byte offset.
    new=decode(out);oldindex={z['label']:i for i,z in enumerate(retained)};newindex={z['label']:i for i,z in enumerate(new)}
    norm=lambda rs,ix:[(z['op'],[ix[z['args'][0]]]if z['op']in BRANCH else z['args'])for z in rs]
    need(norm(retained,oldindex)==norm(new,newindex),'all retained instructions and CFG edges exact')
    return out,dict(removed_old_offsets=[104,130],removed_bytes=26,retained_instructions_and_targets_exact=True,
        null_type_CT_iterator_append_return_preserved=True,new_instructions=0,new_constants=0)
def produce(tag,source,before_source):
    need(sha(tag)==TAG_SHA,'exact current factor endpoint')
    need(sha(source)==SOURCE_SHA and sha(before_source)==BEFORE_SOURCE_SHA,'exact selector Source endpoints')
    clause=b' && (choice.id == 0 || choice.id == 2)'
    need(before_source.count(clause)==1 and before_source.replace(clause,b'',1)==source,'one Source conjunct only')
    data,prefix=abc(tag);old=A.read(data);listing=L.parse(data)
    need(len(old['bodies'])==166 and len(listing['bodies'])==166 and len({r['method']for r in listing['bodies']})==166,'unique166 bodies')
    body=A.method(old,NAME);need(sha(body['raw'])==BODY_SHA and not body['exceptions'],'exact selector body/no exceptions')
    index=next(i for i,b in old['bodies'].items()if b is body);row=next(r for r in listing['bodies']if r['method']==index)
    code,proof=changed_code(body['code'],old)
    newbody=b''.join(L.u30(v)for v in(index,body['max_stack'],body['regs'],body['init_scope'],body['max_scope'],len(code)))+code+L.u30(0)+row['traits']
    after=data[:row['begin']]+newbody+data[row['end']:];new=A.read(after)
    need(old['pools']==new['pools'] and old['tables']==new['tables'],'all pools/tables exact')
    strict=A.compare(data,after,[NAME]);newlist=L.parse(after)
    need(len(newlist['bodies'])==166 and len({r['method']for r in newlist['bodies']})==166,'outputunique166')
    conserved=[i for i in old['bodies']if i!=index]
    need(len(conserved)==165 and all(old['bodies'][i]['raw']==new['bodies'][i]['raw']for i in conserved),'all other165 bodies exact')
    for n in('getXPLeagueFactor','getGPLeagueFactor'):
        need(A.method(old,n)['raw']==A.method(new,n)['raw'],'100percent factor body unchanged')
    payload=prefix+after;out=struct.pack('<HI',(82<<6)|63,len(payload))+payload
    need(sha(out)==OUTPUT_TAG_SHA,'exact derived selector endpoint')
    return out,dict(**proof,strict=strict,other165_raw_exact=True,both_full_rate_factor_bodies_raw_exact=True,
                    pools_tables_raw_exact=True,compiler_invoked=False,AVM2_executed=False)
def splice_movie(movie,prior_tag,new_tag):
    need(sha(movie)==MOVIE_SHA and sha(prior_tag)==TAG_SHA,'exact current factor movie/tag')
    before=S.tags(movie);need(len(before)==6867,'exact tag inventory')
    hits=[r for r in before if r['code']==82 and r['name']=='codo/controllers/MatchController']
    need(len(hits)==1,'unique selector class');r=hits[0]
    need(movie[r['begin']:r['end']]==prior_tag,'current embedded factor tag exact')
    need(sha(new_tag)==OUTPUT_TAG_SHA,'only exact selector tag splice');abc(new_tag);out=bytearray(movie[:r['begin']]+new_tag+movie[r['end']:]);struct.pack_into('<I',out,4,len(out));out=bytes(out)
    after=S.tags(out);need(len(after)==len(before),'same tag count')
    for a,b in zip(before,after):
        need((a['code'],a['name'],a['flags'])==(b['code'],b['name'],b['flags']),'same tag order/metadata')
        if a is not r:need(movie[a['begin']:a['end']]==out[b['begin']:b['end']],'all6866 sibling tags exact')
        if a['name']=='codo/views/OptionView':need(sha(out[b['begin']:b['end']])==VERTICAL_SHA,'vertical2a41 exact')
    need(movie[:4]==out[:4]and movie[8:S.header_len(movie)]==out[8:S.header_len(out)],'movie header exact except length')
    need(sha(out)==OUTPUT_MOVIE_SHA,'exact selector movie endpoint')
    return out,dict(other6866_tags_raw_exact=True,vertical2a41_raw_exact=True,original_header_retained=True)
