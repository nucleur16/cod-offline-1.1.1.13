"""Read-only deterministic method transplant from retained compiler DATA.

This never compiles ActionScript, invokes a child, writes an artifact or loads ABC.
The original whole-class baseline is held. A new source/compiler run needs review.
"""
from pathlib import Path
import json,sys
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import swf_data as S,transplant as T,verify as V
ROOT=HERE.parents[1]
PATCH=ROOT/'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch'

def recipe():return json.loads((HERE/'recipe.json').read_bytes())

def pinned(name,expected):
    S.need('codolpresence' not in name.casefold(),'AV lexical refusal')
    raw=(HERE/name).read_bytes();S.need(S.sha(raw)==expected,'dev input identity: '+name);return raw

def expanded_source():
    source=(PATCH/'as/codo/views/OptionView.as').read_bytes()
    include=(PATCH/'as/codo/views/k33_pad_diagrams.as.inc').read_bytes()
    r=recipe()
    S.need(S.sha(source)==r['candidate_source_sha256'],'candidate source identity')
    S.need(source.count(b'// @K33DIAGRAMS@')==1,'exact include marker')
    S.need(S.sha(source+b'\0'+include)==r['candidate_source_include_digest'],'source/include identity')
    return source.decode('utf-8').replace('\r\n','\n').replace('// @K33DIAGRAMS@',include.decode('utf-8').replace('\r\n','\n'))

def reproduce():
    r=recipe()
    for name,digest in r['tools'].items():pinned(name,digest)
    before=pinned('original.tag.bin',r['original_tag_sha256'])
    base=pinned('base.abc',r['base_abc_sha256'])
    S.need(S.abc(before)==base,'original tag/base ABC identity')
    intermediate=pinned('compiled-intermediate.tag.bin',r['compiled_intermediate_sha256'])
    output,proof=T.patch_tag(before,intermediate,expanded_source())
    S.need(S.sha(output)==r['output_tag_sha256'],'reproduced stored tag identity')
    current=(PATCH/'tags/5988_82_codo_views_OptionView.bin').read_bytes()
    S.need(output==current,'reproduced bytes differ from shipped stored tag')
    return output,proof

if __name__=='__main__':
    output,proof=reproduce()
    print(json.dumps(dict(tag_sha256=S.sha(output),proof=proof,whole_class_baseline='HOLD',runtime_or_release_ready=False),sort_keys=True))
