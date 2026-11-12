"""Bounded constant-resolved ABC inventory: a DATA reader, never a compiler, loader or VM.

A whole-class recompilation renumbers the constant pool, so unchanged methods
differ in raw bytes. Every operand is therefore resolved to its text (names,
namespaces, strings, numbers), branches to instruction ordinals, and debug
line records are dropped. Two methods with one resolved digest have the same
instructions, operands, signature, exception table and activation traits.
Unknown opcodes, constant kinds or trailing bytes refuse.
"""
import hashlib,struct

def need(ok,msg):
    if not ok:raise ValueError(msg)

class Reader:
    def __init__(self,data,pos=0):self.d=data;self.p=pos
    def u8(self):
        need(self.p<len(self.d),'truncated ABC');self.p+=1;return self.d[self.p-1]
    def u30(self):
        value=0
        for shift in range(0,35,7):
            byte=self.u8();value|=(byte&127)<<shift
            if not byte&128:return value&0xffffffff
        raise ValueError('invalid ABC variable integer')
    def s24(self):
        return int.from_bytes(self.take(3),'little',signed=True)
    def take(self,count):
        need(count>=0 and self.p+count<=len(self.d),'truncated ABC');self.p+=count;return self.d[self.p-count:self.p]

NAME={0x04,0x05,0x59,0x5d,0x5e,0x5f,0x60,0x61,0x66,0x68,0x6a,0x80,0x86,0xb2}
NAME_ARGS={0x45,0x46,0x4a,0x4c,0x4e,0x4f}
PLAIN={0x08,0x25,0x41,0x42,0x49,0x53,0x55,0x56,0x58,0x5a,0x62,0x63,0x6c,0x6d,0x6e,0x6f,0x92,0x94,0xc2,0xc3}
BYTE={0x24,0x65}
NONE=({0x01,0x02,0x03,0x07,0x09,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x23,0x26,0x27,0x28,0x29,0x2a,0x2b,0x30,0x47,0x48,0x57,0x64,
       0x81,0x82,0x83,0x84,0x85,0x87,0x88,0x89,0x90,0x91,0x93,0x95,0x96,0x97,0xb3,0xb4,0xc0,0xc1}
      |set(range(0x70,0x79))|set(range(0xa0,0xb2))|set(range(0xc4,0xc8))|set(range(0xd0,0xd8)))

AS3='8:http://adobe.com/AS3/2006/builtin'
CONVERT={0x70:'String',0x85:'String',0x73:'int',0x74:'uint',0x75:'Number',0x76:'Boolean'}

def fold(listing):
    """One spelling for an explicit builtin conversion.

    `Boolean(x)` is either `x; convert_b` or `findpropstrict Boolean; x;
    callproperty Boolean,1` depending on the compiler build (likewise String,
    int, uint, Number). Both become one ('convert',T) row; an adjacent repeat
    of the same conversion is dropped. convert_s and coerce_s are folded
    together, so this view cannot tell String(null) "null" from a null kept
    by coerce_s: it is a comparison aid, not an equivalence proof.

    A builtin member such as `splice` or `indexOf` is either the QName in the
    AS3 namespace (the receiver's type was inferred) or a late-bound multiname
    whose open namespace set contains that namespace. Both become one
    ('as3-or-open',name) operand; the open set itself is then not compared in
    this view (the strict digest still compares it).
    """
    def late(value):
        if type(value)is tuple and(value[:3]==('qname',7,AS3)or value[:2]==('multiname',9)and AS3 in value[3]):
            return('as3-or-open',value[3]if value[0]=='qname'else value[2])
        return value
    rows=[tuple(late(x)for x in row)for row in listing];gone=set();open_={}
    def builtin(value):
        return value[3]if type(value)is tuple and value[:3]==('qname',7,'22:')and value[3]in CONVERT.values()else None
    for at,row in enumerate(rows):
        kind=builtin(row[1])if len(row)>1 else None
        if row[0]==0x5d and kind:open_.setdefault(kind,[]).append(at)
        elif row[0]==0x46 and kind and row[2]==1 and open_.get(kind):gone.add(open_[kind].pop());rows[at]=('convert',kind)
        elif row[0]in CONVERT:rows[at]=('convert',CONVERT[row[0]])
    last=None
    for at,row in enumerate(rows):
        if at in gone:continue
        if row[0]=='convert'and row==last:gone.add(at)
        last=row
    order=[];count=0
    for at in range(len(rows)+1):
        order.append(count)
        if at<len(rows)and at not in gone:count+=1
    move=lambda ordinal:order[ordinal]
    return tuple(tuple(('to',move(x[1]))if type(x)is tuple and x[:1]==('to',)else x for x in row)for at,row in enumerate(rows)if at not in gone),move

def parse(data):
    need(type(data)is bytes and 4<len(data)<=4*1024*1024,'bounded ABC');r=Reader(data)
    need(struct.unpack('<HH',r.take(4))==(16,46),'reviewed ABC version')
    def pool(read,zero):return [zero]+[read()for _ in range(max(0,r.u30()-1))]
    def signed():
        value=r.u30();return value-(1<<32)if value>>31 else value
    ints=pool(signed,0);uints=pool(r.u30,0);doubles=pool(lambda:r.take(8).hex(),'nan')
    strings=pool(lambda:r.take(r.u30()).decode('utf-8'),'')
    spaces=pool(lambda:(r.u8(),r.u30()),None)
    sets=pool(lambda:[r.u30()for _ in range(r.u30())],None)
    def multiname():
        kind=r.u8()
        if kind in(7,13,9,14):return(kind,r.u30(),r.u30())
        if kind in(15,16,27,28):return(kind,r.u30())
        if kind in(17,18):return(kind,)
        need(kind==29,'reviewed multiname kind');return(kind,r.u30(),[r.u30()for _ in range(r.u30())])
    names=pool(multiname,None)
    def text(index):
        need(index<len(strings),'string index');return strings[index]
    def space(index):
        if index==0:return'*'
        need(index<len(spaces),'namespace index');kind,label=spaces[index];return'%d:%s'%(kind,text(label))
    def spaceset(index):
        need(0<index<len(sets),'namespace set index');return tuple(space(x)for x in sets[index])
    def name(index):
        if index==0:return'*'
        need(index<len(names),'multiname index');row=names[index];kind=row[0]
        if kind in(7,13):return('qname',kind,space(row[1]),text(row[2])if row[2]else'*')
        if kind in(9,14):return('multiname',kind,text(row[1])if row[1]else'*',spaceset(row[2]))
        if kind in(15,16):return('rtqname',kind,text(row[1]))
        if kind in(17,18):return('rtqnamel',kind)
        if kind in(27,28):return('multinamel',kind,spaceset(row[1]))
        return('typename',name(row[1]),tuple(name(x)for x in row[2]))
    def constant(index,kind):
        if kind==3:need(index<len(ints),'int index');return('int',ints[index])
        if kind==4:need(index<len(uints),'uint index');return('uint',uints[index])
        if kind==6:need(index<len(doubles),'double index');return('double',doubles[index])
        if kind==1:return('string',text(index))
        if kind in(0,0x0a,0x0b,0x0c):return('literal',kind)
        need(kind in(5,8,0x16,0x17,0x18,0x19,0x1a),'reviewed constant kind');return('namespace',space(index))
    methods=[]
    for _ in range(r.u30()):
        count=r.u30();returns=name(r.u30());params=tuple(name(r.u30())for _ in range(count));label=text(r.u30());flags=r.u8();defaults=()
        if flags&8:defaults=tuple(constant(r.u30(),r.u8())for _ in range(r.u30()))
        if flags&128:
            for _ in range(count):r.u30()
        methods.append(dict(name=label,signature=(returns,params,flags&127,defaults)))
    def method(index):
        need(index<len(methods),'method index');return methods[index]['name']
    for _ in range(r.u30()):
        r.u30()
        for _ in range(r.u30()*2):r.u30()
    def traits():
        rows=[]
        for _ in range(r.u30()):
            label=name(r.u30());kind=r.u8();role=kind&15;slot=r.u30();target=r.u30()
            if role in(0,6):
                value=r.u30();detail=(name(target),constant(value,r.u8())if value else None)
            elif role in(1,2,3,5):detail=('method',method(target))
            else:need(role==4,'reviewed trait kind');detail=('class',target)
            if kind&64:
                for _ in range(r.u30()):r.u30()
            rows.append((label,role,(kind>>4)&3,slot,detail))
        return rows
    classes=r.u30();owners=[]
    for _ in range(classes):
        label=name(r.u30());parent=name(r.u30());flags=r.u8();protected=space(r.u30())if flags&8 else None
        interfaces=tuple(name(r.u30())for _ in range(r.u30()));init=method(r.u30())
        owners.append(('instance',label,parent,flags,protected,interfaces,init,traits()))
    for _ in range(classes):owners.append(('class',method(r.u30()),traits()))
    for _ in range(r.u30()):owners.append(('script',method(r.u30()),traits()))
    bodies={}
    for _ in range(r.u30()):
        index=r.u30();need(index<len(methods)and index not in bodies,'unique method body')
        limits=tuple(r.u30()for _ in range(4));code=r.take(r.u30())
        handlers=[tuple(r.u30()for _ in range(5))for _ in range(r.u30())]
        bodies[index]=(limits,code,handlers,traits())
    need(r.p==len(data),'unconsumed ABC data')

    def instructions(code):
        c=Reader(code);rows=[]
        while c.p<len(code):
            at=c.p;op=c.u8()
            if op in NONE:row=(op,)
            elif op in NAME:row=(op,name(c.u30()))
            elif op in NAME_ARGS:row=(op,name(c.u30()),c.u30())
            elif op in PLAIN:row=(op,c.u30())
            elif op in BYTE:row=(op,c.u8())
            elif op in(0x2c,0x06):row=(op,text(c.u30()))
            elif op==0x2d:
                index=c.u30();need(index<len(ints),'int index');row=(op,ints[index])
            elif op==0x2e:
                index=c.u30();need(index<len(uints),'uint index');row=(op,uints[index])
            elif op==0x2f:
                index=c.u30();need(index<len(doubles),'double index');row=(op,doubles[index])
            elif op==0x31:row=(op,space(c.u30()))
            elif op in(0x32,0x43):row=(op,c.u30(),c.u30())
            elif op==0x40:row=(op,method(c.u30()))
            elif op==0x44:row=(op,method(c.u30()),c.u30())
            elif 0x0c<=op<=0x1a:
                offset=c.s24();row=(op,('to',c.p+offset))
            elif op==0x1b:
                default=c.s24();count=c.u30();row=(op,('to',at+default))+tuple(('to',at+c.s24())for _ in range(count+1))
            elif op==0xef:c.u8();c.u30();c.u8();c.u30();row=None
            elif op in(0xf0,0xf1):c.u30();row=None
            else:raise ValueError('unreviewed opcode '+hex(op))
            rows.append((at,row))
        kept=[];ordinal={}
        for at,row in rows:
            ordinal[at]=len(kept)
            if row is not None:kept.append(row)
        ordinal[len(code)]=len(kept)
        def place(offset):
            need(offset in ordinal,'branch lands on an instruction');return ordinal[offset]
        return tuple(tuple(('to',place(x[1]))if type(x)is tuple and x[:1]==('to',)else x for x in row)for row in kept),place

    out={};folded={}
    for index,row in enumerate(methods):
        resolved=loose=None
        if index in bodies:
            limits,code,handlers,slots=bodies[index];listing,place=instructions(code)
            ranges=tuple((place(a),place(b),place(c),name(d),name(e))for a,b,c,d,e in handlers)
            resolved=(limits,listing,ranges,tuple(slots))
            listing,move=fold(listing)
            loose=(limits[1:],listing,tuple((move(a),move(b),move(c),d,e)for a,b,c,d,e in ranges),tuple(slots))
        for target,value in((out,resolved),(folded,loose)):
            target.setdefault(row['name'],[]).append(hashlib.sha256(repr((row['signature'],value)).encode('utf-8')).hexdigest())
    return dict(methods={k:sorted(v)for k,v in out.items()},folded={k:sorted(v)for k,v in folded.items()},bodies=len(bodies),
                owners=sorted(repr(x)for x in owners),
                traits=sorted(repr((owner[0],t))for owner in owners for t in owner[-1]))

def delta(before,after):
    """Resolved difference of two ABC blocks of one class."""
    old,new=parse(before),parse(after)
    names=sorted(set(old['methods'])|set(new['methods']))
    return dict(
        bodies_before=old['bodies'],bodies_after=new['bodies'],
        methods_added=[n for n in names if n not in old['methods']],
        methods_removed=[n for n in names if n not in new['methods']],
        methods_changed=[n for n in names if n in old['methods']and n in new['methods']and old['methods'][n]!=new['methods'][n]],
        methods_identical=sum(1 for n in names if old['methods'].get(n)==new['methods'].get(n)),
        methods_changed_beyond_compiler_spelling=[n for n in names if n in old['folded']and n in new['folded']and old['folded'][n]!=new['folded'][n]],
        traits_added=sorted(set(new['traits'])-set(old['traits'])),
        traits_removed=sorted(set(old['traits'])-set(new['traits'])))
