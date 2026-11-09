"""Read-only two-factor DATA successor to the immutable invitation endpoint."""
from pathlib import Path
import hashlib,json,sys,zlib
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sys.path.insert(0,str(HERE))
import method_producer as M,export_contract as C
PATCH=ROOT/'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch'
TOOLS={'abc_reader.py','abc_list_reader.py','swf_data.py','method_producer.py','export_contract.py','reproduce.py'}
def need(ok,message):
    if not ok:raise ValueError(message)
def sha(raw):return hashlib.sha256(raw).hexdigest()
def pinned(name,digest):
    need(type(name)is str and '/'not in name and '\\'not in name and 'codolpresence'not in name.casefold(),'fixed developer DATA basename')
    raw=(HERE/name).read_bytes();need(sha(raw)==digest,'developer input/tool differs: '+name);return raw
def recipe():
    r=json.loads((HERE/'recipe.json').read_bytes())
    need(r['format']=='reviewed-two-training-factor-DATA-successor-v1','fixed factor successor format')
    need(set(r['tools'])==TOOLS,'exact protected producer tools')
    for name,digest in r['tools'].items():pinned(name,digest)
    return r
def reproduce(source=None):
    r=recipe();original=pinned('original.tag.bin.DATA',r['original_tag_sha256'])
    vertical=json.loads((ROOT/'tools/optionview-method-producer/recipe.json').read_bytes())
    need(vertical['output_tag_sha256']==r['vertical_component_tag_sha256'] and vertical['output_movie_sha256']==r['vertical_component_movie_sha256']==M.MOVIE_SHA,'reviewed vertical component retained')
    need(sha((PATCH/'tags/5988_82_codo_views_OptionView.bin').read_bytes())==r['vertical_component_tag_sha256'],'actual vertical sibling tag retained')
    source=(PATCH/'as/codo/controllers/MatchController.as').read_bytes()if source is None else source
    need(sha(source)==r['source_after_sha256'],'factor source producer differs')
    result,proof=M.produce(original,source);need(sha(result)==r['output_tag_sha256'],'reviewed two-factor tag differs')
    current=(PATCH/'tags/6167_82_codo_controllers_MatchController.bin').read_bytes()
    need(current==result,'stored factor tag does not reproduce')
    resolved=pinned('actual-export.as.DATA',r['actual_export_sha256']).decode('utf-8');readback=C.verify(resolved)
    return result,dict(transform=proof,readback=readback,compiler_invoked=False,rendered=False)
def verify_successor(root,tag,*,source=None,prior):
    root=Path(root);need(root.resolve()==ROOT.resolve(),'exact current producer root')
    r=recipe();contract=prior.read_contract()
    need(sha((prior.DOC/'transition.json').read_bytes())==r['historical_transition_sha256'],'immutable prior transition changed')
    original=pinned('original.tag.bin.DATA',r['original_tag_sha256']);old=prior.extract(original)
    oldreviewed=zlib.decompress((prior.DOC/'MatchController.current.abc.zlib').read_bytes())
    need(old==oldreviewed and sha(old)==contract['current_abc_sha256'],'original exact protected transition endpoint')
    parsed=prior.A.read(old)
    need(len(parsed['bodies'])==contract['current_body_count']==166 and sha(parsed['tables'])==contract['current_tables_sha256'],'prior endpoint tables/inventory')
    need({str(i):dict(name=b['name'],raw_sha256=sha(b['raw']),code_sha256=sha(b['code']))for i,b in parsed['bodies'].items()}==contract['current_body_pins'],'all166 original body pins protected')
    before=pinned('Source.before.as.DATA',r['source_before_sha256']);need(sha(before)==contract['source_sha256'],'prior producer source retained')
    oldexport=zlib.decompress((prior.DOC/'MatchController.current.as.zlib').read_bytes());need(sha(oldexport)==contract['resolved_export_sha256'],'prior resolved Source retained')
    actual=(PATCH/'as/codo/controllers/MatchController.as').read_bytes()if source is None else source
    expected,proof=reproduce(actual);need(tag==expected,'unreviewed factor successor supplied')
    baseline=prior.historical_parent();abi=prior.verify_abi(baseline,prior.extract(tag))
    exported=pinned('actual-export.as.DATA',r['actual_export_sha256'])
    prior.invitation_contract(prior.A.read(baseline),prior.A.read(prior.extract(tag)),actual,exported,contract['invitation_denial_code_hex'])
    need(sha((prior.LEGACY/'abi-contract.json').read_bytes())==contract['historical_abi_contract_sha256'],'historical ABI receipt immutable')
    lines=(PATCH/'patch.txt').read_text(encoding='utf-8').splitlines()
    need(lines.count('as codo.controllers.MatchController as/codo/controllers/MatchController.as '+r['source_after_sha256'])==1,'exact factor Source recipe row')
    need(lines.count('sha256 '+r['output_movie_sha256'])==1,'exact composed movie digest')
    return dict(**abi,current_body_count=166,current_abc_sha256=sha(prior.extract(tag)),protected_invitation_noop=True,historical_parent_sha256=sha(baseline),factor_successor=True,unchanged_other164=True,verified_bytecode_DATA_not_compiled_Source=True)
if __name__=='__main__':
    tag,proof=reproduce();print(json.dumps(dict(tag_sha256=sha(tag),proof=proof,FF_rebuilt=False,rendered=False),sort_keys=True))
