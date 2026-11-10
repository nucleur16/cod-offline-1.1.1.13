"""Append a balanced AVM2 call prefix without recompiling native class metadata."""
import hashlib

def u30(value):
    out=bytearray()
    while value>127: out.append((value&127)|128); value>>=7
    out.append(value);return bytes(out)

class Reader:
    def __init__(self,data): self.d=data;self.p=4
    def n(self):
        value=0
        for shift in range(0,35,7):
            b=self.d[self.p];self.p+=1;value|=(b&127)<<shift
            if not b&128:return value
        raise ValueError('invalid U30')
    def b(self):v=self.d[self.p];self.p+=1;return v
    def traits(self):
        result=[]
        for _ in range(self.n()):
            name=self.n();kind=self.b();low=kind&15;method=None
            if low in (0,6):
                self.n();self.n()
                if self.n():self.b()
            elif low in (1,2,3):self.n();method=self.n()
            elif low in (4,5):self.n();self.n()
            else:raise ValueError('unknown trait')
            if kind&64:
                for _ in range(self.n()):self.n()
            result.append((name,low,method))
        return result

def parse(data):
    r=Reader(data);pools=[];strings=[''];spaces=[None];names=[None]
    for kind in ('int','uint','double','string','namespace','nsset','multiname'):
        begin=r.p;count=r.n();entries_start=r.p
        for index in range(1,count):
            if kind in ('int','uint'):r.n()
            elif kind=='double':r.p+=8
            elif kind=='string':
                size=r.n();strings.append(data[r.p:r.p+size].decode('utf8'));r.p+=size
            elif kind=='namespace':spaces.append((r.b(),r.n()))
            elif kind=='nsset':
                for _ in range(r.n()):r.n()
            else:
                tag=r.b();ns=None;name=None
                if tag in (7,13):ns=r.n();name=r.n()
                elif tag in (15,16):name=r.n()
                elif tag in (17,18):pass
                elif tag in (9,14):name=r.n();r.n()
                elif tag in (27,28):r.n()
                elif tag==29:
                    r.n()
                    for _ in range(r.n()):r.n()
                else:raise ValueError('unknown multiname '+str(tag))
                names.append((tag,ns,name))
        pools.append((begin,r.p,count,entries_start))
    pools_end=r.p;methods_begin=r.p
    for _ in range(r.n()):
        params=r.n();r.n()
        for _ in range(params):r.n()
        r.n();flags=r.b()
        if flags&8:
            for _ in range(r.n()):r.n();r.b()
        if flags&128:
            for _ in range(params):r.n()
    methods_end=r.p
    for _ in range(r.n()):
        r.n();items=r.n()
        for _ in range(items*2):r.n()
    classes_begin=r.p;count=r.n();instances=[]
    for _ in range(count):
        name=r.n();r.n();flags=r.b()
        if flags&8:r.n()
        for _ in range(r.n()):r.n()
        r.n();instances.append((name,r.traits()))
    for _ in range(count):r.n();r.traits()
    classes_end=r.p;scripts_begin=r.p
    for _ in range(r.n()):r.n();r.traits()
    scripts_end=r.p;bodies=[]
    for _ in range(r.n()):
        begin=r.p;method=r.n();stack=r.n();locals_=r.n();init_scope=r.n();max_scope=r.n();length=r.n();code=data[r.p:r.p+length];r.p+=length
        exceptions=[tuple(r.n() for _ in range(5)) for _ in range(r.n())]
        traits_begin=r.p;r.traits()
        bodies.append({'begin':begin,'end':r.p,'method':method,'stack':stack,'locals':locals_,'init_scope':init_scope,'max_scope':max_scope,'code':code,'exceptions':exceptions,'traits':data[traits_begin:r.p]})
    assert r.p==len(data),'ABC trailing data'
    return locals()

def add_guard(data,method_name,root_class,helper_name,conditional=False):
    old=parse(data);strings=old['strings'];spaces=old['spaces'];names=old['names'];pools=old['pools'];extra={3:[],4:[],6:[]}
    def string(value):
        if value in strings:return strings.index(value)
        index=len(strings);strings.append(value);raw=value.encode();extra[3].append(u30(len(raw))+raw);return index
    package=next((i for i,value in enumerate(spaces) if value==(22,0)),None)
    if package is None:
        package=len(spaces);spaces.append((22,0));extra[4].append(b'\x16\x00')
    def qname(value):
        value=string(value);record=(7,package,value)
        if record in names:return names.index(record)
        index=len(names);names.append(record);extra[6].append(b'\x07'+u30(package)+u30(value));return index
    main=qname(root_class);guard=qname(helper_name)
    prefix=(b'\x60'+u30(main)+b'\xd1\x46'+u30(guard)+b'\x01\x12\x01\x00\x00\x47') if conditional else (b'\x60'+u30(main)+b'\x4f'+u30(guard)+b'\x00')
    target=[]
    for _,traits in old['instances']:
        for name,kind,method in traits:
            record=old['names'][name]
            if kind==1 and record[2] is not None and old['strings'][record[2]]==method_name:target.append(method)
    assert len(target)==1,'expected one native '+method_name
    body=next(b for b in old['bodies'] if b['method']==target[0]);code=body['code']
    prologue=0
    while code[prologue] in (0xf0,0xf1):
        prologue+=1
        while code[prologue]&128:prologue+=1
        prologue+=1
    assert code[prologue:prologue+2]==b'\xd0\x30','unexpected native '+method_name+' prologue'
    delta=len(prefix)
    patched=b''.join(u30(n) for n in (body['method'],max(2 if conditional else 1,body['stack']),body['locals'],body['init_scope'],body['max_scope'],len(code)+delta))+prefix+code+u30(len(body['exceptions']))
    for start,end,target_,type_,var_ in body['exceptions']:
        patched+=b''.join(u30(n) for n in (start+delta,end+delta,target_+delta,type_,var_))
    patched+=body['traits']
    pool_bytes=b''
    for i,(begin,end,count,entries) in enumerate(pools):
        additions=extra.get(i,[])
        pool_bytes+=u30(max(1,count)+len(additions))+data[entries:end]+b''.join(additions) if additions else data[begin:end]
    result=data[:4]+pool_bytes+data[old['pools_end']:body['begin']]+patched+data[body['end']:]
    new=parse(result)
    def section(obj,a,b):return obj['data'][obj[a]:obj[b]]
    for a,b in [('methods_begin','methods_end'),('classes_begin','classes_end'),('scripts_begin','scripts_end')]:assert section(old,a,b)==section(new,a,b)
    for a,b in zip(old['bodies'],new['bodies']):
        if a['method']!=body['method']:assert data[a['begin']:a['end']]==result[b['begin']:b['end']]
        else:assert b['code']==prefix+a['code']
    return result,{'method_name':method_name,'root_class':root_class,'helper':helper_name,'conditional_return':conditional,'method':body['method'],'prefix_hex':prefix.hex(),'original_code_sha256':hashlib.sha256(code).hexdigest(),'native_class_method_traits_unchanged':True,'other_method_bodies_unchanged':True}

def add_runtime_guard(data):
    return add_guard(data,'initAllViews','MainPause','prepareTheatrePauseViews')
