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
    old,new=unique39(before),unique39(after);a,b=A.method(old,'k33SetLabel'),A.method(new,'k33SetLabel')
    need(a['exceptions']==b['exceptions']==0 and a['init_scope']==b['init_scope']and a['max_scope']==b['max_scope'],'target exception/scope drift')
    need(a['regs']==4 and b['regs']==5 and a['max_stack']==3 and b['max_stack']==4,'exact target local/stack budget')
    oi,od=decode(a['code']);ni,nd=decode(b['code'])
    prop={}
    for op,i in oi:
        if op in('getproperty','setproperty'):
            name=A.qname(old,i)
            if name in('text','x','width'):prop[name]=i
    need(set(prop)=={'text','x','width'},'exact old target property identities')
    text,x,width=(prop[k]for k in('text','x','width'))
    first=next(i for i,r in enumerate(oi)if r==('setproperty',text));guard=first-2
    need(oi[guard:first]==[('getlocal',3),('getlocal',2)],'old setter boundary')
    need(len(ni)>guard and len([1 for op,_ in ni if op in('ifne','iffalse')])==3,'exact null/equality branch count')
    for i,(op,val)in enumerate(oi[:guard]):
        expected=(op,len(ni)-1 if op in('ifne','iffalse')and val==len(oi)-1 else val)
        need(ni[i]==expected,'null/text-equality guard prefix changed')
    body=[('getlocal',3),('getproperty',x),('getlocal',3),('getproperty',width),('byte',2),('divide',None),('add',None)]
    tail=[('setlocal',4),('getlocal',3),('getlocal',2),('setproperty',text),('getlocal',3),('getlocal',4),('getlocal',3),('getproperty',width),('byte',2),('divide',None),('subtract',None),('setproperty',x),('return',None)]
    conversions=[[],[('convert_d',None)]]
    for i in range(1,len(old['values']['multiname'])):
        try:
            if A.qname(old,i)=='Number':conversions.append([('coerce',i)])
        except ValueError:pass
    need(any(ni[guard:]==body+c+tail for c in conversions),'expected saved tf.x+tf.width/2 BEFORE text then center-tf.width/2 required')
    need(nd[:len(od)]==od and len(nd)==len(od)+1,'existing target debug declarations changed')
    kind,index,reg,line=nd[-1];expected_line=next(i for i,s in enumerate(candidate_expanded.splitlines(),1)if re.search(r'\bvar w:Number\s*=',s))
    need(kind==1 and reg==3 and new['values']['string'][index]=='w'and line==expected_line,'exact new center debug local')
    # No arbitrary unused pool growth can hide in the allowed target delta.
    for name,pool in old['pools'].items():
        need(new['pools'][name]==pool,'line-neutral candidate requires every pool exact')
    return dict(target='k33SetLabel',guard_prefix_exact=True,old_bodies=39,new_bodies=39,other_bodies_exact=38,locals4_to5=True,stack3_to4=True,exceptions0=True,center_before_setter=True,center_after_setter=True)

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
    need(type(text)is str and len(text.encode())<=262144,'bounded exported source')
    need(re.search(r'package\s+codo\.views\s*\{',text)and re.search(r'\bclass\s+OptionView\b',text),'exact exported class')
    need(len(re.findall(r'\bfunction\s+k33SetLabel\s*\(',text))==1,'unique exported setter')
    params,body=function_source(text);decls=re.findall(r'\bvar\s+(\w+)\s*:\s*(TextField|Number)\b',body)
    tf=[n for n,t in decls if t=='TextField'];center=[n for n,t in decls if t=='Number'];need(len(tf)==len(center)==1,'exact two setter locals')
    names={params[0]:'b',params[1]:'s',tf[0]:'tf',center[0]:'center'};actual=[names.get(t,t)for t in tokens(body)]
    # Finite decompiler layouts: typed locals may be hoisted to null/NaN.
    variants=[]
    for hoist_tf in(False,True):
        for hoist_center in(False,True):
            prefix=('var tf:TextField=null;'if hoist_tf else'')+('var center:Number=NaN;'if hoist_center else'')
            template=prefix+'if(b==null){return;}'+('tf='if hoist_tf else'var tf:TextField=')+'b.getChildByName("label") as TextField;if(tf!=null&&tf.text!=s){'+('center='if hoist_center else'var center:Number=')+'tf.x+tf.width/2;tf.text=s;tf.x=center-tf.width/2;}'
            variants.extend([tokens(template),tokens(template+'return;')])
    need(actual in variants,'exported setter guard/call/order/arithmetic differs; no generic cast/control-flow waiver')
    return dict(class_name='codo.views.OptionView',setter_finite_roundtrip=True,other_methods_protected_by_ABC=True)
