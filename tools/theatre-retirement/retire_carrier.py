"""Remove the retired announcement credential path from an audited existing menu.

Do not recompile the playlist menu: preserve its native methods, indices and
every unrelated tag. The unreferenced ABC method slot remains a false-returning
body so all other method indices stay stable.
"""
from pathlib import Path
import argparse, hashlib, json, sys
import abc_guard as abc

PREFIX = '_'.join(('CODOL', 'THEATRE', 'SESSION', 'V1:'))
DVAR = '_'.join(('codol', 'theatre', 'session'))
HELPER = 'handle' + 'TheatreSession'

def digest(data): return hashlib.sha256(data).hexdigest()

def body_bytes(body, code, exceptions=None):
    exceptions = body['exceptions'] if exceptions is None else exceptions
    result = b''.join(abc.u30(v) for v in (body['method'], body['stack'], body['locals'],
        body['init_scope'], body['max_scope'], len(code))) + code + abc.u30(len(exceptions))
    return result + b''.join(b''.join(abc.u30(v) for v in row) for row in exceptions) + body['traits']

def erase_strings(data, values):
    old = abc.parse(data); begin, end, count, entries = old['pools'][3]
    r = abc.Reader(data); r.p = entries; records=[]; found=[]
    for i in range(1, count):
        start=r.p; size=r.n(); text=data[r.p:r.p+size].decode('utf8'); r.p+=size
        if text in values: records.append(b'\0'); found.append(text)
        else: records.append(data[start:r.p])
    assert r.p==end
    result=data[:begin]+abc.u30(count)+b''.join(records)+data[end:]
    assert all(abc.parse(result)['strings'][i]==('' if value in values else value)
        for i,value in enumerate(old['strings']))
    return result, found

def trait_ranges(data, parsed):
    r=abc.Reader(data); r.p=parsed['classes_begin']; count=r.n(); groups=[]
    def traits():
        start=r.p; amount=r.n(); rows=[]
        for _ in range(amount):
            begin=r.p; name=r.n(); flags=r.b(); kind=flags&15; method=None
            if kind in (0,6):
                r.n();r.n()
                if r.n():r.b()
            elif kind in (1,2,3):r.n();method=r.n()
            elif kind in (4,5):r.n();r.n()
            else:raise ValueError('Unknown trait')
            if flags&64:
                for _ in range(r.n()):r.n()
            rows.append((begin,r.p,name,kind,method))
        groups.append((start,r.p,rows))
    for _ in range(count):
        r.n();r.n();flags=r.b()
        if flags&8:r.n()
        for _ in range(r.n()):r.n()
        r.n();traits()
    for _ in range(count):r.n();traits()
    assert r.p==parsed['classes_end']
    return groups

def retire_abc(data, name):
    old=abc.parse(data); replacements=[]; target=None; delta=0
    def named(index):
        record=old['names'][index]
        return old['strings'][record[2]] if record and record[2] is not None else None
    if name=='codo/common/DataManager':
        methods=[method for _,traits in old['instances'] for n,k,method in traits if k==1 and named(n)=='setData']
        assert len(methods)==1; target=methods[0]
        main=[i for i in range(1,len(old['names'])) if named(i)=='Main']
        guard=[i for i in range(1,len(old['names'])) if named(i)==HELPER]
        assert len(main)==len(guard)==1
        prefix=b'\x60'+abc.u30(main[0])+b'\xd1\x46'+abc.u30(guard[0])+b'\x01\x12\x01\x00\x00\x47'
        body=next(b for b in old['bodies'] if b['method']==target)
        assert body['code'].startswith(prefix),'Expected exact measured conditional receiver prefix'
        delta=len(prefix); code=body['code'][delta:]
        prologue=0
        while code[prologue] in (0xf0,0xf1):
            prologue+=1
            while code[prologue]&128:prologue+=1
            prologue+=1
        assert code[prologue:prologue+2]==b'\xd0\x30','Expected original native method prologue'
        exceptions=[(a-delta,b-delta,c-delta,d,e) for a,b,c,d,e in body['exceptions']]
        assert all(min(row[:3])>=0 for row in exceptions)
        replacements.append((body['begin'],body['end'],body_bytes(body,code,exceptions)))
    elif name=='Main':
        matches=[]
        for start,end,rows in trait_ranges(data,old):
            for row in rows:
                if row[3]==1 and named(row[2])==HELPER:matches.append((start,end,rows,row))
        assert len(matches)==1; start,end,rows,row=matches[0];target=row[4]
        replacements.append((start,end,abc.u30(len(rows)-1)+b''.join(data[a:b] for a,b,*_ in rows if a!=row[0])))
        body=next(b for b in old['bodies'] if b['method']==target)
        replacements.append((body['begin'],body['end'],body_bytes(body,b'\x27\x48',[])))
    else:raise ValueError('Unexpected ABC')
    result=data
    for begin,end,value in sorted(replacements,reverse=True):result=result[:begin]+value+result[end:]
    result,cleared=erase_strings(result,{PREFIX,DVAR,HELPER,'Main/'+HELPER})
    new=abc.parse(result)
    assert len(old['bodies'])==len(new['bodies'])
    for a,b in zip(old['bodies'],new['bodies']):
        assert a['method']==b['method']
        if a['method']!=target:
            assert data[a['begin']:a['end']]==result[b['begin']:b['end']],'Unrelated method changed'
        elif name=='codo/common/DataManager':assert b['code']==a['code'][delta:]
        else:assert b['code']==b'\x27\x48'
    for value in (PREFIX,DVAR,HELPER):assert value.encode() not in result
    return result,{'abc':name,'method':target,'removed_prefix_bytes':delta,'cleared_strings':len(cleared),
        'unrelated_method_bodies_byte_identical':True,'method_indices_preserved':True}

def transform_movie(movie,gfx):
    head,tags=gfx.read_movie(movie); updated=list(tags);changes=[]
    for i,(code,body,long) in enumerate(tags):
        if code!=82:continue
        offset=body.index(0,4)+1;name=body[4:offset-1].decode()
        if name not in ('Main','codo/common/DataManager'):continue
        replacement,proof=retire_abc(body[offset:],name)
        updated[i]=(code,body[:offset]+replacement,long);changes.append((i,proof))
    assert len(changes)==2
    result=gfx.write_movie(head,updated);h2,t2=gfx.read_movie(result)
    assert h2==head and len(t2)==len(tags)
    assert [i for i,(a,b) in enumerate(zip(tags,t2)) if a!=b]==[i for i,_ in changes]
    assert all(value.encode() not in result for value in (PREFIX,DVAR,HELPER))
    return result,{'changed_abcs':[p for _,p in changes],'other_tags_byte_identical':True}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('input','output','exe','toolchain'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--input-sha256',required=True);a=p.parse_args()
    assert a.input.resolve()!=a.output.resolve()
    raw=a.input.read_bytes();assert digest(raw)==a.input_sha256
    sys.path.insert(0,str(a.toolchain.resolve()));import build_font_ff as ff,gfx_font as gfx
    files=ff.rawfiles(ff.unwrap(raw,str(a.exe)));original=dict(files);name='scaleform/default/codo.gfx'
    files[name],proof=transform_movie(files[name],gfx)
    marker=a.input.stem;assert files[marker]==b''
    zone=ff.build_zone(marker,[(ff.bf.T_RAWFILE,ff.pack_rawfile_z(k,v)) for k,v in files.items() if k!=marker])
    body=ff.wrap(zone,str(a.exe));decoded=ff.rawfiles(ff.unwrap(body,str(a.exe)))
    assert files==decoded and all(decoded[k]==v for k,v in original.items() if k!=name)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(body)
    receipt=dict(input_sha256=digest(raw),output_sha256=digest(body),exe_sha256=digest(a.exe.read_bytes()),
        other_rawfiles_byte_identical=True,rawfiles=len(files),retired_transport_absent=True,**proof)
    a.output.with_suffix('.receipt.json').write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt))
if __name__=='__main__':main()
