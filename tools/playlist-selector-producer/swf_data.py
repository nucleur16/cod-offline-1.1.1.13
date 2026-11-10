"""Bounded uncompressed movie DATA operations, never a compiler or loader."""
import hashlib,struct
CLASS='codo/views/OptionView'
def need(ok,msg):
    if not ok:raise ValueError(msg)
def sha(raw):return hashlib.sha256(raw).hexdigest()
def header_len(raw):
    need(type(raw)is bytes and 14<=len(raw)<=16*1024*1024,'bounded immutable movie bytes')
    need(raw[:3]in(b'GFX',b'FWS')and struct.unpack_from('<I',raw,4)[0]==len(raw),'uncompressed movie signature/length')
    n=8+(5+4*(raw[8]>>3)+7)//8+4;need(n<=len(raw),'complete movie header');return n
def tags(raw):
    pos=header_len(raw);out=[]
    while pos<len(raw):
        need(len(out)<10000 and pos+2<=len(raw),'bounded complete tag header')
        start=pos;word=struct.unpack_from('<H',raw,pos)[0];pos+=2;code=word>>6;size=word&63
        if size==63:need(pos+4<=len(raw),'tag length header');size=struct.unpack_from('<I',raw,pos)[0];pos+=4
        end=pos+size;need(end<=len(raw),'tag extent');name='';flags=None
        if code==82:
            need(pos+5<=end,'DoABC envelope');flags=struct.unpack_from('<I',raw,pos)[0];zero=raw.find(b'\0',pos+4,end);need(zero!=-1,'DoABC name termination');name=raw[pos+4:zero].decode('latin1')
        out.append(dict(code=code,begin=start,end=end,name=name,flags=flags))
        pos=end
        if code==0:need(size==0 and pos==len(raw),'exact final end tag');break
    need(out and out[-1]['code']==0 and pos==len(raw),'terminal end tag required');return out
def class_tag(raw):
    found=[r for r in tags(raw)if r['code']==82 and r['name']==CLASS]
    need(len(found)==1,'exactly one target DoABC required');r=found[0];need(r['flags']==1,'target DoABC flags');return raw[r['begin']:r['end']]
def abc(tag):
    word=struct.unpack_from('<H',tag)[0];n=6 if word&63==63 else 2
    need(word>>6==82,'DoABC tag');size=struct.unpack_from('<I',tag,2)[0]if n==6 else word&63
    need(n+size==len(tag)and tag[n:n+4]==b'\x01\0\0\0','complete exact DoABC envelope')
    end=tag.index(b'\0',n+4);need(tag[n+4:end]==CLASS.encode(),'exact OptionView name');return tag[end+1:]
def splice(original,new_tag):
    rows=tags(original);target=[r for r in rows if r['code']==82 and r['name']==CLASS];need(len(target)==1,'unique target before splice');abc(new_tag);r=target[0]
    out=bytearray(original[:r['begin']]+new_tag+original[r['end']:]);struct.pack_into('<I',out,4,len(out));return bytes(out)
def restore(current,stock):return splice(current,class_tag(stock))
def fws(movie):header_len(movie);return b'FWS'+movie[3:]
def normalize_output(raw):
    if raw[:3]==b'CWS':
        import zlib
        need(len(raw)>=8,'compressed header');size=struct.unpack_from('<I',raw,4)[0];need(14<=size<=16*1024*1024,'bounded decompression size')
        decoder=zlib.decompressobj();body=decoder.decompress(raw[8:],size-8+1)
        need(len(body)==size-8 and decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail,'exact compressed stream')
        raw=b'FWS'+raw[3:8]+body
    need(raw[:3]==b'FWS','compiler output must be FWS/CWS');header_len(raw);return raw
def verify_output(current,compiled):
    before,after=tags(current),tags(compiled);need(len(before)==len(after)==6867,'exact current tag count')
    need(current[3:4]==compiled[3:4]and current[8:header_len(current)]==compiled[8:header_len(compiled)],'movie header version/rectangle/rate/frame changed')
    hits=0
    for a,b in zip(before,after):
        need((a['code'],a['name'],a['flags'])==(b['code'],b['name'],b['flags']),'tag order/name/type/flags changed')
        if a['code']==82 and a['name']==CLASS:hits+=1
        else:need(current[a['begin']:a['end']]==compiled[b['begin']:b['end']],'non-target tag changed')
    need(hits==1,'unique output target');return class_tag(compiled)
