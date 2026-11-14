"""Read-only three-tag successor to the immutable reviewed four-P2P-tag movie.

Commit d319a61 (docs/P2P_DEFERRED_HOST_START.md) recompiled three of the four
P2P classes. The four-tag stage keeps its own pins, contract and review record
untouched: this directory preserves the three sources, three stored tags and
the patch.txt of that endpoint as bounded DATA witnesses (held()), so the
four-tag stage still reconstructs and proves its exact endpoint, and then
hands that reconstructed movie here as the predecessor.

This endpoint (OUTPUT_SHA) is no longer the tree's movie either: the UI style
commits recompiled four classes later (tools/p2p-ui-style-composition). As
the four-tag stage does for this stage, the files of this endpoint that changed
since are read from that successor's preserved DATA witnesses (endpoint()
below, each still checked against this stage's own pins), and the tree's movie
is then proved by that successor over this reconstructed endpoint. held()
hands an earlier stage a file this stage did not change from that successor.

Nothing is compiled or loaded. The review record is an automated review of
the source and stored-tag difference; verify() recomputes its constant-resolved
method delta from the actual tag bytes instead of trusting it.
"""
from pathlib import Path
import hashlib,importlib.util,json,struct

HERE=Path(__file__).resolve().parent
CONTRACT_SHA='a07b3586097f50cefd879df1210ec064032d87b1272d1f77c185654b9bb27d97'
PRIOR_SHA='bc9c8515ca326a31e1006bfeb8338a441cd2d5677d6a56d0b31eb46ef294e9a9'
OUTPUT_SHA='7fedff5bda30dc0bd8a484735e1dcce3d389e7fb68b825f28680aa8bc70ce6a5'
FORMAT='automated-diff-reviewed-three-P2P-tag-composition-v1'
VERDICT='ACCEPT_THREE_P2P_CLASS_TAG_DATA_DELTA_BY_AUTOMATED_REVIEW'
NEXT=HERE.parent/'p2p-ui-style-composition'/'composition.py'
NEXT_SHA='c4833df0cfa12d611fb979a653928ab1214e1b27407206879c239bb16428833c'

def need(value,label):
    if not value:raise ValueError(label)

def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);loaded=importlib.util.module_from_spec(spec);spec.loader.exec_module(loaded);return loaded

def successor():
    raw=NEXT.read_bytes();need(hashlib.sha256(raw).hexdigest()==NEXT_SHA,'composition DATA identity: '+NEXT.name)
    return module('qualified_ui_style_successor',NEXT)

def held(rel):
    """The four-tag endpoint's bytes of a patch file changed since, else None.

    A file this stage changed comes from this stage's witnesses, any other
    from the successor's (this stage's own endpoint). Unpinned here on purpose:
    the four-tag stage checks every returned byte against its own unchanged pins.
    """
    contract=json.loads((HERE/'composition.json').read_bytes());table={'patch.txt':'patch.before.txt.DATA'}
    for row in contract['classes']:table[row['source']]=row['prior_source_data'];table[row['tag']]=row['prior_tag_data']
    name=table.get(rel);return successor().held(rel)if name is None else(HERE/name).read_bytes()

def abc(tag,name):
    head=6 if struct.unpack_from('<H',tag)[0]&63==63 else 2
    need(struct.unpack_from('<H',tag)[0]>>6==82 and tag[head:head+4]==b'\x01\0\0\0','qualified DoABC envelope')
    end=tag.index(b'\0',head+4);need(tag[head+4:end].decode('latin1')==name,'qualified DoABC envelope');return tag[end+1:]

def verify(root,prior_sha,*,before=None,prior=None):
    need(prior_sha==PRIOR_SHA,'immutable four-tag predecessor')
    if before is None or prior is None:
        # Standalone: the predecessor movie exists only as the four-tag stage's reconstruction.
        four=module('preserved_four_tag_composition',HERE.parent/'p2p-four-tag-composition/composition.py')
        return four.verify(root,four.PRIOR_SHA)['successor']
    S=prior.S;sha=prior.sha;exact=prior.exact;PATCH=prior.PATCH
    need(type(before)is bytes and sha(before)==PRIOR_SHA,'reconstructed exact four-tag predecessor')
    contract=json.loads(exact(HERE/'composition.json',CONTRACT_SHA))
    need(contract['format']==FORMAT and contract['prior_movie_sha256']==PRIOR_SHA and contract['output_movie_sha256']==OUTPUT_SHA,'closed composition lineage')
    prior_patch=exact(HERE/'patch.before.txt.DATA',contract['prior_patch_sha256'])
    oldmeta,oldsrc,oldops=prior.parse_patch(prior_patch)
    exact(NEXT,NEXT_SHA);after_stage=module('qualified_ui_style_successor',NEXT)
    def endpoint(rel):
        # This endpoint's bytes: the successor's witness where the tree has moved on, else the tree.
        raw=after_stage.held(rel);return(Path(root)/PATCH/rel).read_bytes()if raw is None else raw
    current_patch=endpoint('patch.txt')
    meta,sources,ops=prior.parse_patch(current_patch)
    need(ops==oldops,'all 6867 original patch operations/order exact')
    need(meta['stock']==oldmeta['stock']and meta['stock-sha256']==oldmeta['stock-sha256'],'original stock reference and digest exact')
    header=bytes.fromhex(meta['header'])
    need(meta['header']==oldmeta['header']and header[:4]==before[:4]and header[8:]==before[8:S.header_len(before)],'original movie header/version/geometry/fps exact')
    need(oldmeta['sha256']==PRIOR_SHA and meta['sha256']==OUTPUT_SHA,'separate predecessor/current movie identities')
    rows=contract['classes'];allowed={r['class_name'].replace('.','/'):r for r in rows}
    need(len(rows)==len(allowed)==3,'closed three target classes')
    need(set(sources)==set(oldsrc),'original Source inventory exact')
    for cls,row in sources.items():
        if cls.replace('.','/')in allowed:
            expected=allowed[cls.replace('.','/')]
            need(row==(expected['source'],expected['source_sha256'])and oldsrc[cls]==(expected['source'],expected['prior_source_sha256']),'qualified three Source metadata exact')
            need(sha(endpoint(row[0]))==row[1],'composition DATA identity: '+Path(row[0]).name);exact(HERE/expected['prior_source_data'],expected['prior_source_sha256'])
        else:need(row==oldsrc[cls],'non-target Source metadata unchanged')
    review=json.loads(exact(HERE/'automated-diff-review.json.DATA',contract['review_sha256']))
    need(review['verdict']==VERDICT and review['reviewer']['kind']=='automated'and review['reviewer']['human']is False and
         review['runtime_authority']is False and review['output_movie_sha256']==OUTPUT_SHA and review['prior_movie_sha256']==PRIOR_SHA,'automated three-tag review record')
    reviewed={r['class_name']:r for r in review['classes']}
    need(len(review['classes'])==3 and set(reviewed)=={r['class_name']for r in rows},'review covers exactly the three classes')
    exact(HERE/'resolved_abc.py',contract['resolved_abc_sha256']);resolved=module('bounded_resolved_abc',HERE/'resolved_abc.py')
    tags=S.tags(before);need(len(tags)==len(ops)==6867,'original tag inventory')
    out=[];changed=[];hits=[]
    for op,t in zip(ops,tags):
        old=before[t['begin']:t['end']];name=t['name']
        if op[0]=='keep':
            need(name not in allowed,'qualified class must use an explicit stored tag');out.append(old);continue
        now=endpoint(op[1])
        if name in allowed:
            r=allowed[name]
            need(op[1]==r['tag']and sha(old)==r['prior_tag_sha256']and sha(now)==r['tag_sha256']and
                 exact(HERE/r['prior_tag_data'],r['prior_tag_sha256'])==old,'qualified target tag identity')
            need(now!=old,'each qualified class changes once')
            # The record's method-level statement is recomputed from the two actual ABC blocks.
            need(resolved.delta(abc(old,name),abc(now,name))==reviewed[r['class_name']]['resolved_delta'],'reviewed resolved method delta')
            changed.append(name);hits.append((len(out),old))
        else:need(now==old,'non-target tag payload unchanged')
        out.append(now)
    need(set(changed)==set(allowed)and len(changed)==3,'exactly three qualified changes')
    current=bytearray(header+b''.join(out));struct.pack_into('<I',current,4,len(current));current=bytes(current)
    after=S.tags(current)
    need(len(after)==6867 and[(r['code'],r['name'],r['flags'])for r in after]==[(r['code'],r['name'],r['flags'])for r in tags],'whole movie order/types/names/flags exact')
    need(current[:4]==header[:4]and current[8:S.header_len(current)]==header[8:]and sha(current)==OUTPUT_SHA,'whole current movie reconstructed exactly')
    reverse=[current[t['begin']:t['end']]for t in after]
    for index,old in hits:reverse[index]=old
    restored=bytearray(current[:S.header_len(current)]+b''.join(reverse));struct.pack_into('<I',restored,4,len(restored))
    need(bytes(restored)==before and sha(restored)==PRIOR_SHA,'three-tag inverse restores exact four-tag endpoint')
    need(sha(current_patch)==contract['current_patch_sha256'],'exact qualified patch metadata bytes')
    proof=dict(prior_movie_sha256=PRIOR_SHA,current_movie_sha256=OUTPUT_SHA,qualified_classes=3,non_target_tags_exact=6864,total_tags=6867,
                inverse_exact=True,resolved_method_delta_recomputed=True,review='automated',human_reviewed=False,compiler_invoked=False,runtime_or_release_ready=False)
    # The tree's movie: proved by the successor over this reconstructed endpoint.
    proof['successor']=after_stage.verify(root,OUTPUT_SHA,before=current,prior=prior)
    return proof
