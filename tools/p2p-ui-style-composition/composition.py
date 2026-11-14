"""Read-only four-tag successor to the deferred-host P2P movie (7fedff5b).

The UI style branch (0f5fb5e, merged in ac1d351; 54dd327; c0a88a2) recompiled
four classes of codo.gfx: OptionView_video (Language / Button icons / HUD
style drop-downs) and, for the removal of the Local Play box,
MatchMiniRoomCreatorView, MatchMPCTRoomCreatorView_mpcreator and
LocalMatchControls. (MainPause, the other class of those commits, is in
codoingame.gfx, which this chain does not cover.)

The deferred-host stage keeps its own pins, contract and review record
untouched: this directory preserves the four sources, four stored tags and the
patch.txt of that endpoint as bounded DATA witnesses (held()), so the earlier
stages still reconstruct and prove their exact endpoints, and the deferred-host
stage then hands its reconstructed movie here as the predecessor. Every other
tag of the movie must be byte-identical to that predecessor.

Nothing is compiled or loaded. The review record is an automated review of
the source and stored-tag difference; verify() recomputes its constant-resolved
method delta from the actual tag bytes (with the deferred-host stage's pinned
resolved_abc.py) instead of trusting it.
"""
from pathlib import Path
import hashlib,importlib.util,json,struct

HERE=Path(__file__).resolve().parent
CONTRACT_SHA='695da8e7128e9b4980e84e9860a7c593ed44d2cf37765f94739c10732250a54e'
PRIOR_SHA='7fedff5bda30dc0bd8a484735e1dcce3d389e7fb68b825f28680aa8bc70ce6a5'
OUTPUT_SHA='2e2f7431a68ed299f1fd4cee2b917456e998975a42e065001d2f9934419d191a'
FORMAT='automated-diff-reviewed-ui-style-four-tag-composition-v1'
VERDICT='ACCEPT_UI_STYLE_FOUR_CLASS_TAG_DATA_DELTA_BY_AUTOMATED_REVIEW'
RESOLVED=HERE.parent/'p2p-deferred-host-composition'/'resolved_abc.py'

def need(value,label):
    if not value:raise ValueError(label)

def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);loaded=importlib.util.module_from_spec(spec);spec.loader.exec_module(loaded);return loaded

def held(rel):
    """The deferred-host endpoint's bytes of a patch file this stage changed, else None.

    Unpinned here on purpose: the earlier stages check every returned byte
    against their own unchanged pins.
    """
    contract=json.loads((HERE/'composition.json').read_bytes());table={'patch.txt':'patch.before.txt.DATA'}
    for row in contract['classes']:table[row['source']]=row['prior_source_data'];table[row['tag']]=row['prior_tag_data']
    name=table.get(rel);return None if name is None else(HERE/name).read_bytes()

def abc(tag,name):
    head=6 if struct.unpack_from('<H',tag)[0]&63==63 else 2
    need(struct.unpack_from('<H',tag)[0]>>6==82 and tag[head:head+4]==b'\x01\0\0\0','qualified DoABC envelope')
    end=tag.index(b'\0',head+4);need(tag[head+4:end].decode('latin1')==name,'qualified DoABC envelope');return tag[end+1:]

def verify(root,prior_sha,*,before=None,prior=None):
    need(prior_sha==PRIOR_SHA,'immutable deferred-host predecessor')
    if before is None or prior is None:
        # Standalone: the predecessor movie exists only as the earlier stages' reconstruction.
        four=module('preserved_four_tag_composition',HERE.parent/'p2p-four-tag-composition/composition.py')
        return four.verify(root,four.PRIOR_SHA)['successor']['successor']
    S=prior.S;sha=prior.sha;exact=prior.exact;PATCH=prior.PATCH
    need(type(before)is bytes and sha(before)==PRIOR_SHA,'reconstructed exact deferred-host predecessor')
    contract=json.loads(exact(HERE/'composition.json',CONTRACT_SHA))
    need(contract['format']==FORMAT and contract['prior_movie_sha256']==PRIOR_SHA and contract['output_movie_sha256']==OUTPUT_SHA,'closed composition lineage')
    prior_patch=exact(HERE/'patch.before.txt.DATA',contract['prior_patch_sha256'])
    oldmeta,oldsrc,oldops=prior.parse_patch(prior_patch)
    current_patch=(Path(root)/PATCH/'patch.txt').read_bytes()
    meta,sources,ops=prior.parse_patch(current_patch)
    need(ops==oldops,'all 6867 original patch operations/order exact')
    need(meta['stock']==oldmeta['stock']and meta['stock-sha256']==oldmeta['stock-sha256'],'original stock reference and digest exact')
    header=bytes.fromhex(meta['header'])
    need(meta['header']==oldmeta['header']and header[:4]==before[:4]and header[8:]==before[8:S.header_len(before)],'original movie header/version/geometry/fps exact')
    need(oldmeta['sha256']==PRIOR_SHA and meta['sha256']==OUTPUT_SHA,'separate predecessor/current movie identities')
    rows=contract['classes'];allowed={r['class_name'].replace('.','/'):r for r in rows}
    need(len(rows)==len(allowed)==4,'closed four target classes')
    need(set(sources)==set(oldsrc),'original Source inventory exact')
    for cls,row in sources.items():
        if cls.replace('.','/')in allowed:
            expected=allowed[cls.replace('.','/')]
            need(row==(expected['source'],expected['source_sha256'])and oldsrc[cls]==(expected['source'],expected['prior_source_sha256']),'qualified four Source metadata exact')
            exact(Path(root)/PATCH/row[0],row[1]);exact(HERE/expected['prior_source_data'],expected['prior_source_sha256'])
        else:need(row==oldsrc[cls],'non-target Source metadata unchanged')
    review=json.loads(exact(HERE/'automated-diff-review.json.DATA',contract['review_sha256']))
    need(review['verdict']==VERDICT and review['reviewer']['kind']=='automated'and review['reviewer']['human']is False and
         review['runtime_authority']is False and review['output_movie_sha256']==OUTPUT_SHA and review['prior_movie_sha256']==PRIOR_SHA,'automated four-tag review record')
    reviewed={r['class_name']:r for r in review['classes']}
    need(len(review['classes'])==4 and set(reviewed)=={r['class_name']for r in rows},'review covers exactly the four classes')
    exact(RESOLVED,contract['resolved_abc_sha256']);resolved=module('bounded_resolved_abc',RESOLVED)
    tags=S.tags(before);need(len(tags)==len(ops)==6867,'original tag inventory')
    out=[];changed=[];hits=[]
    for op,t in zip(ops,tags):
        old=before[t['begin']:t['end']];name=t['name']
        if op[0]=='keep':
            need(name not in allowed,'qualified class must use an explicit stored tag');out.append(old);continue
        now=(Path(root)/PATCH/op[1]).read_bytes()
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
    need(set(changed)==set(allowed)and len(changed)==4,'exactly four qualified changes')
    current=bytearray(header+b''.join(out));struct.pack_into('<I',current,4,len(current));current=bytes(current)
    after=S.tags(current)
    need(len(after)==6867 and[(r['code'],r['name'],r['flags'])for r in after]==[(r['code'],r['name'],r['flags'])for r in tags],'whole movie order/types/names/flags exact')
    need(current[:4]==header[:4]and current[8:S.header_len(current)]==header[8:]and sha(current)==OUTPUT_SHA,'whole current movie reconstructed exactly')
    reverse=[current[t['begin']:t['end']]for t in after]
    for index,old in hits:reverse[index]=old
    restored=bytearray(current[:S.header_len(current)]+b''.join(reverse));struct.pack_into('<I',restored,4,len(restored))
    need(bytes(restored)==before and sha(restored)==PRIOR_SHA,'four-tag inverse restores exact deferred-host endpoint')
    need(sha(current_patch)==contract['current_patch_sha256'],'exact qualified patch metadata bytes')
    return dict(prior_movie_sha256=PRIOR_SHA,current_movie_sha256=OUTPUT_SHA,qualified_classes=4,non_target_tags_exact=6863,total_tags=6867,
                inverse_exact=True,resolved_method_delta_recomputed=True,review='automated',human_reviewed=False,compiler_invoked=False,runtime_or_release_ready=False)
