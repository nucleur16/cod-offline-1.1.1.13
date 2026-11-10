"""Finite strict compiled/roundtrip checks; pure DATA, no AVM2 execution."""
import copy,hashlib,json,re
import abc_reader as A
import abc_list_reader as L
from swf_data import need,abc,sha

def unique39(raw):
    parsed=L.parse(raw);rows=parsed['bodies'];need(len(rows)==39 and len({r['method']for r in rows})==39,'require39 declared/list bodies and39 unique method IDs')
    bounded=A.read(raw);need(len(bounded['bodies'])==39,'bounded body map differs');return bounded
def decode(code):
    pos=0;all_rows=[];debug=[]
    def u30():
        nonlocal pos
        value=0
        for shift in range(0,35,7):
            need(pos<len(code),'u30 extent');b=code[pos];pos+=1;value|=(b&127)<<shift
            if not b&128:need(value<1<<30,'u30 value');return value
        raise ValueError('invalid u30')
    singles={0x30:'pushscope',0x20:'null',0x47:'return',0x87:'astypelate',0xab:'equals',0x96:'not',0x2a:'dup',0x29:'pop',0xa0:'add',0xa1:'subtract',0xa3:'divide',0x75:'convert_d'}
    while pos<len(code):
        start=pos;op=code[pos];pos+=1
        if op==0xef:
            need(pos<len(code),'debug type');kind=code[pos];pos+=1;index=u30();need(pos<len(code),'debug reg');reg=code[pos];pos+=1;extra=u30();debug.append((kind,index,reg,extra));continue
        if 0xd0<=op<=0xd3:name='getlocal';value=op-0xd0
        elif 0xd4<=op<=0xd7:name='setlocal';value=op-0xd4
        elif op in(0x62,0x63):name='getlocal'if op==0x62 else'setlocal';value=u30()
        elif op in(0x2c,0x60,0x80,0x66,0x61):name={0x2c:'string',0x60:'getlex',0x80:'coerce',0x66:'getproperty',0x61:'setproperty'}[op];value=u30()
        elif op==0x46:name='callproperty';value=(u30(),u30())
        elif op==0x24:need(pos<len(code),'pushbyte');name='byte';value=code[pos]if code[pos]<128 else code[pos]-256;pos+=1
        elif op in(0x12,0x14):
            need(pos+3<=len(code),'branch extent');offset=int.from_bytes(code[pos:pos+3],'little');offset=offset-(1<<24)if offset&(1<<23)else offset;pos+=3;name='iffalse'if op==0x12 else'ifne';value=pos+offset
        else:need(op in singles,'unexpected target opcode '+hex(op));name=singles[op];value=None
        all_rows.append(dict(start=start,end=pos,op=name,value=value))
        need(len(all_rows)<=128,'bounded target instruction count')
    positions={r['start']:i for i,r in enumerate(all_rows)}
    for row in all_rows:
        if row['op']in('ifne','iffalse'):need(row['value']in positions,'branch target is not a semantic instruction');row['value']=positions[row['value']]
    return [(r['op'],r['value'])for r in all_rows],debug

def center_semantics(before,after,candidate_expanded):
    import sys,vertical_contract
    return vertical_contract.center_semantics(sys.modules[__name__],before,after,candidate_expanded)

def compiled(before_tag,after_tag,expanded):
    before,after=abc(before_tag),abc(after_tag);unique39(before);unique39(after)
    strict=A.compare(before,after,['k33SetLabel']);semantic=center_semantics(before,after,expanded)
    return dict(strict=strict,semantic=semantic)

def function_source(text):
    match=re.search(r'private\s+function\s+k33SetLabel\s*\(\s*(\w+)\s*:\s*Sprite\s*,\s*(\w+)\s*:\s*String\s*\)\s*:\s*void\s*\{',text)
    need(match is not None,'exact private setter signature');pos=match.end();start=pos;depth=1;quote=None;escape=False
    while depth:
        need(pos<len(text),'complete function source');c=text[pos];pos+=1
        if quote:
            if escape:escape=False
            elif c=='\\':escape=True
            elif c==quote:quote=None
        elif c in('"',"'"):quote=c
        elif c=='{':depth+=1
        elif c=='}':depth-=1
    return match.groups(),text[start:pos-1]
def tokens(text):
    # Strings are retained; comments are removed lexically rather than by a
    # wildcard replacement that could discard literal content.
    pattern=r'"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|//[^\n]*|/\*.*?\*/|[A-Za-z_$][\w$]*|\d+|==|!=|&&|[{}():;=+\-/.,]'
    found=[];end=0
    for m in re.finditer(pattern,text,re.S):
        need(not text[end:m.start()].strip(),'unrecognized source token');end=m.end()
        if not m[0].startswith(('//','/*')):found.append(m[0])
    need(not text[end:].strip(),'unrecognized source suffix');return found
def exported(text):
    import sys,vertical_contract
    return vertical_contract.exported(sys.modules[__name__],text)
