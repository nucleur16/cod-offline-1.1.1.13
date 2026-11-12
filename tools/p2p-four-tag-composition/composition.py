"""Read-only four-tag successor to the immutable reviewed selector movie.

The retained predecessor is a bounded DATA witness, never loaded or compiled.
All original selector/ABI/invitation checks remain in current_endpoint.py.

This endpoint (OUTPUT_SHA) is no longer the tree's movie: three of its four
classes were recompiled later. Its contract, pins and review record are not
repinned. The files of this endpoint that changed since are read from the
successor's preserved DATA witnesses (endpoint() below, each still checked
against this stage's own pins), this exact endpoint is reconstructed as
before, and the tree's movie is then proved by the pinned successor stage.
"""
from pathlib import Path
import hashlib,importlib.util,json,re,struct,sys,types,zlib

HERE=Path(__file__).resolve().parent
PARENT=HERE.parent/'playlist-selector-producer'
sys.path.insert(0,str(PARENT))
import swf_data as S

CONTRACT_SHA='6838fca3f273cf05b1e79a1ee3a85c944a83a177bcf00511a3686e194348ce8b'
PRIOR_SHA='38dd0456193d9025f5831de44f9d2deb70d433f42ea7b8d0f76b7236f558ea37'
OUTPUT_SHA='bc9c8515ca326a31e1006bfeb8338a441cd2d5677d6a56d0b31eb46ef294e9a9'
PATCH=Path('zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch')
MAX=16*1024*1024
NEXT=HERE.parent/'p2p-deferred-host-composition'/'composition.py'
NEXT_SHA='f98e1b5d3bf6c313008c16a30e2a8b4cfcd96ccb53885ad420a5ecdb4f68cacc'

def need(value,label):
    if not value:raise ValueError(label)

def sha(raw):return hashlib.sha256(raw).hexdigest()

def exact(path,pin):
    need('codolpresence'not in str(path).casefold(),'forbidden binary lexical refusal')
    raw=Path(path).read_bytes();need(sha(raw)==pin,'composition DATA identity: '+Path(path).name);return raw

def unpack(raw):
    need(type(raw)is bytes and len(raw)<=MAX,'bounded compressed predecessor')
    d=zlib.decompressobj();out=d.decompress(raw,MAX+1)
    need(len(out)<=MAX and d.eof and not d.unused_data and not d.unconsumed_tail,'exact bounded predecessor stream')
    S.header_len(out);return out

def parse_patch(raw):
    need(type(raw)is bytes and len(raw)<256*1024,'bounded patch metadata')
    meta={};sources={};ops=[]
    for line in raw.decode('utf-8').splitlines():
        if not line or line.startswith('#'):continue
        key,sep,value=line.partition(' ');need(sep and value,'complete patch directive')
        if key=='keep':
            need(re.fullmatch(r'0|[1-9][0-9]*',value),'canonical keep index');ops.append((key,int(value)))
        elif key=='new':
            need(re.fullmatch(r'tags/[A-Za-z0-9_.-]+\.bin',value),'closed new-tag path');ops.append((key,value))
        elif key=='as':
            fields=value.split(' ');need(len(fields)==3,'closed Source metadata')
            cls,path,pin=fields
            need(cls not in sources and path=='as/'+cls.replace('.','/')+'.as' and re.fullmatch('[0-9a-f]{64}',pin),'unique canonical Source row')
            sources[cls]=(path,pin)
        else:
            need(key in {'stock','stock-sha256','sha256','header'}and key not in meta,'closed unique patch metadata')
            meta[key]=value
    need(set(meta)=={'stock','stock-sha256','sha256','header'}and len(ops)==6867,'complete movie recipe')
    return meta,sources,ops

def verify(root,prior_sha):
    need(prior_sha==PRIOR_SHA,'immutable selector predecessor')
    contract=json.loads(exact(HERE/'composition.json',CONTRACT_SHA))
    need(contract['format']=='reviewed-four-P2P-tag-composition-v1'and contract['prior_movie_sha256']==PRIOR_SHA and contract['output_movie_sha256']==OUTPUT_SHA,'closed composition lineage')
    before=unpack(exact(HERE/'selector-predecessor.gfx.zlib.DATA',contract['predecessor_zlib_sha256']))
    need(sha(before)==PRIOR_SHA,'reconstructed exact selector predecessor')
    prior_patch=exact(HERE/'patch.before.txt.DATA',contract['prior_patch_sha256'])
    oldmeta,oldsrc,oldops=parse_patch(prior_patch)
    exact(NEXT,NEXT_SHA)
    spec=importlib.util.spec_from_file_location('qualified_three_tag_successor',NEXT)
    successor=importlib.util.module_from_spec(spec);spec.loader.exec_module(successor)
    def endpoint(rel):
        # This endpoint's bytes: the successor's witness where the tree has moved on, else the tree.
        raw=successor.held(rel);return(Path(root)/PATCH/rel).read_bytes()if raw is None else raw
    current_patch=endpoint('patch.txt')
    meta,sources,ops=parse_patch(current_patch)
    need(ops==oldops,'all 6867 original patch operations/order exact')
    need(meta['stock']==oldmeta['stock']and meta['stock-sha256']==oldmeta['stock-sha256'],'original stock reference and digest exact')
    header=bytes.fromhex(meta['header']);oldheader=bytes.fromhex(oldmeta['header'])
    # zonekit.apply always rewrites bytes 4:8 to the composed byte length;
    # patch.txt retains its original header template verbatim.
    need(header==oldheader and oldheader[:4]==before[:4]and oldheader[8:]==before[8:S.header_len(before)],'original movie header/version/geometry/fps exact')
    need(oldmeta['sha256']==PRIOR_SHA and meta['sha256']==OUTPUT_SHA,'separate predecessor/current movie identities')
    rows=contract['classes'];allowed={r['class_name'].replace('.','/'):r for r in rows}
    need(len(rows)==len(allowed)==4,'closed four target classes')
    need(set(sources)==set(oldsrc),'original Source inventory exact')
    for cls,row in sources.items():
        if cls.replace('.','/')in allowed:
            expected=allowed[cls.replace('.','/')]
            need(row==(expected['source'],expected['source_sha256']),'qualified four Source metadata exact')
            need(sha(endpoint(row[0]))==row[1],'composition DATA identity: '+Path(row[0]).name)
        else:need(row==oldsrc[cls],'non-target Source metadata unchanged')
    actual_receipt=json.loads(exact(HERE/'actual-semantic-review.json.DATA',contract['actual_semantic_peer_sha256']))
    need(actual_receipt['verdict']=='ACCEPT_FOUR_P2P_CLASS_TAG_DATA_SEMANTICS'and actual_receipt['actual_ABC_checked']is True and actual_receipt['non_target_tags_exact']is True and actual_receipt['source_body_cases_pass']==19,'actual four-tag semantic qualification')
    need(actual_receipt['compiled_manifest_sha256']==contract['compiled_manifest_sha256'],'qualified original manifest lineage')
    tags=S.tags(before);need(len(tags)==len(ops)==6867,'original tag inventory')
    out=[];changed=[];hits=[]
    for op,t in zip(ops,tags):
        old=before[t['begin']:t['end']];name=t['name']
        if op[0]=='keep':
            need(name not in allowed,'qualified class must use an explicit stored tag');out.append(old);continue
        now=endpoint(op[1])
        if name in allowed:
            r=allowed[name]
            need(op[1]==r['tag']and sha(old)==r['prior_tag_sha256']and sha(now)==r['tag_sha256'],'qualified target tag identity')
            # Complete lazy DoABC envelope/name/flags is independently pinned.
            h=6 if struct.unpack_from('<H',now)[0]&63==63 else 2
            need(struct.unpack_from('<H',now)[0]>>6==82 and now[h:h+4]==b'\x01\0\0\0'and now[h+4:now.index(b'\0',h+4)].decode('latin1')==name,'qualified DoABC envelope')
            need(now!=old,'each qualified class changes once');changed.append(name);hits.append((len(out),old))
        else:need(now==old,'non-target tag payload unchanged')
        out.append(now)
    need(set(changed)==set(allowed)and len(changed)==4,'exactly four qualified changes')
    current=bytearray(header+b''.join(out));struct.pack_into('<I',current,4,len(current));current=bytes(current)
    after=S.tags(current)
    need(len(after)==6867 and [(r['code'],r['name'],r['flags'])for r in after]==[(r['code'],r['name'],r['flags'])for r in tags],'whole movie order/types/names/flags exact')
    need(current[:4]==header[:4]and current[8:S.header_len(current)]==header[8:]and sha(current)==OUTPUT_SHA,'whole current movie reconstructed exactly')
    # Reverse the same four changes, preserving every other current tag.
    reverse=[current[t['begin']:t['end']]for t in after]
    for index,old in hits:reverse[index]=old
    restored=bytearray(current[:S.header_len(current)]+b''.join(reverse));struct.pack_into('<I',restored,4,len(restored))
    need(bytes(restored)==before and sha(restored)==PRIOR_SHA,'four-tag inverse restores exact selector endpoint')
    need(sha(current_patch)==contract['current_patch_sha256'],'exact qualified patch metadata bytes')
    proof=dict(prior_movie_sha256=PRIOR_SHA,current_movie_sha256=OUTPUT_SHA,qualified_classes=4,non_target_tags_exact=6863,total_tags=6867,inverse_exact=True,original_selector_contract_preserved=True,actual_ABC_checked=True,compiler_invoked=False,runtime_or_release_ready=False)
    # The tree's movie: proved by the successor over this reconstructed endpoint.
    proof['successor']=successor.verify(root,OUTPUT_SHA,before=current,prior=types.SimpleNamespace(**globals()))
    return proof
