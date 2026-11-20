import struct,sys,json
# Cast format: header: magic 'cast'(u32 0x74736163), version u32, rootNodes u32, flags u32
# node: identifier u32, nodeSize u32, nodeHash u64, propertyCount u32, childCount u32, props..., children...
# prop: identifier u16 (type code), nameSize u16, arrayLength u32, name, values
TYPES={b'b':('B',1),b'h':('H',2),b'i':('I',4),b'l':('Q',8),b'f':('f',4),b'd':('d',8),b's':None,b'2v':('ff',8),b'3v':('fff',12),b'4v':('ffff',16)}
def rd_node(d,o):
    ident,size,h,pc,cc=struct.unpack_from('<IIQII',d,o); start=o; o+=24
    props={}
    for _ in range(pc):
        code,ns,al=struct.unpack_from('<HHI',d,o); o+=8
        name=d[o:o+ns].decode(); o+=ns
        c=struct.pack('<H',code).rstrip(b'\0')
        if c==b's':
            e=d.index(b'\0',o); val=d[o:e].decode('latin1'); o=e+1
        else:
            fmt,sz=TYPES[c]
            vals=[struct.unpack_from('<'+fmt,d,o+i*sz) for i in range(al)]; o+=al*sz
            val=[v if len(v)>1 else v[0] for v in vals]
        props[name]=val
    kids=[]
    for _ in range(cc):
        k,o=rd_node(d,o); kids.append(k)
    return {'id':struct.pack('<I',ident).rstrip(b'\0').decode('latin1'),'hash':h,'props':props,'kids':kids},start+size
def load(p):
    d=open(p,'rb').read()
    magic,ver,roots,fl=struct.unpack_from('<IIII',d,0); o=16; out=[]
    for _ in range(roots):
        n,o=rd_node(d,o); out.append(n)
    return out
def walk(n,depth=0,fn=None):
    fn(n,depth)
    for k in n['kids']: walk(k,depth+1,fn)
if __name__=='__main__':
    roots=load(sys.argv[1])
    def pr(n,dp):
        if n['id']=='mesh':
            print('mesh',n['props'].get('n'), 'p',n['props'].get('vp'),'u',n['props'].get('u0'),'f',n['props'].get('f'),'m',n['props'].get('m'))
        elif n['id'] in ('mdle','modl','mate','matl'):
            print(n['id'],{k:v for k,v in n['props'].items() if isinstance(v,str)})
    for r in roots: walk(r,0,pr)
