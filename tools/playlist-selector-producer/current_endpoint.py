"""Additive selector gate; preserves immutable historical and factor contracts.

Root integration adds only the verify_current routing hunk. Prior tools and
contracts are read and reconstructed, never repinned to accept selector bytes.
"""
from pathlib import Path
import hashlib,json,sys,zlib,re
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE))
import selector_producer as P
def need(v,s):
    if not v:raise ValueError(s)
def sha(b):return hashlib.sha256(b).hexdigest()
def exact(path,pin):
    raw=Path(path).read_bytes();need(sha(raw)==pin,'pinned selector developer DATA differs');return raw
def prove_chain(root,tag,source,prior):
    root=Path(root)
    exact(root/'tools/training-factor-producer/reproduce.py','e8ce96f6b005be7b55b1d2a39e68f46cbab1bb8c195a1a4f7a853e2dcb10a099')
    exact(root/'tools/training-factor-producer/recipe.json','7427aba6000245b8c4b7d83c3b22372a986ef5a444248af382582834e1a5228e')
    factor=prior.module('preserved_factor_producer',root/'tools/training-factor-producer/reproduce.py')
    recipe=factor.recipe() # Preserved original tool-hash checks.
    contract=prior.read_contract() # Preserved immutable transition hash/schema.
    need(sha((prior.DOC/'transition.json').read_bytes())==recipe['historical_transition_sha256'],'factor historical transition unchanged')
    original=factor.pinned('original.tag.bin.DATA',recipe['original_tag_sha256'])
    old=factor.M.abc(original)[0]
    oldreviewed=zlib.decompress((prior.DOC/'MatchController.current.abc.zlib').read_bytes())
    need(old==oldreviewed and sha(old)==contract['current_abc_sha256'],'original pre-factor endpoint exact')
    parsed=prior.A.read(old)
    need(len(parsed['bodies'])==contract['current_body_count']==166 and sha(parsed['tables'])==contract['current_tables_sha256'],'historical166 inventory/tables')
    need({str(i):dict(name=b['name'],raw_sha256=sha(b['raw']),code_sha256=sha(b['code']))for i,b in parsed['bodies'].items()}==contract['current_body_pins'],'all166 original body pins')
    historic_source=factor.pinned('Source.before.as.DATA',recipe['source_before_sha256'])
    need(sha(historic_source)==contract['source_sha256'],'historical Source endpoint')
    oldexport=zlib.decompress((prior.DOC/'MatchController.current.as.zlib').read_bytes())
    need(sha(oldexport)==contract['resolved_export_sha256'],'historical readback endpoint')
    before_source=exact(HERE/'Source.before.as.DATA',P.BEFORE_SOURCE_SHA)
    before_tag=exact(HERE/'factor-endpoint.tag.DATA',P.TAG_SHA)
    reconstructed,factor_proof=factor.M.produce(original,before_source)
    need(reconstructed==before_tag and sha(reconstructed)==recipe['output_tag_sha256'],'original factor producer reproduces exact prior f653')
    readback=factor.C.verify(factor.pinned('actual-export.as.DATA',recipe['actual_export_sha256']).decode('utf-8'))
    vertical=json.loads((root/'tools/optionview-method-producer/recipe.json').read_bytes())
    need(vertical['output_tag_sha256']==recipe['vertical_component_tag_sha256']==P.VERTICAL_SHA and
         vertical['output_movie_sha256']==recipe['vertical_component_movie_sha256']==factor.M.MOVIE_SHA,'original vertical component contract')
    need(sha((root/prior.PATCH/'tags/5988_82_codo_views_OptionView.bin').read_bytes())==P.VERTICAL_SHA,'current vertical sibling exact')
    expected,proof=P.produce(before_tag,source,before_source)
    need(tag==expected,'unreviewed selector successor supplied')
    current=P.abc(tag)[0];baseline=prior.historical_parent();abi=prior.verify_abi(baseline,current)
    # Selector does not alter the protected invitation-denial body or Source.
    prior.invitation_contract(prior.A.read(baseline),prior.A.read(current),source,oldexport,contract['invitation_denial_code_hex'])
    need(sha((prior.LEGACY/'abi-contract.json').read_bytes())==contract['historical_abi_contract_sha256'],'historical ABI receipt immutable')
    return dict(**abi,selector_successor=True,protected_invitation_noop=True,current_body_count=166,
        current_abc_sha256=sha(current),other165_exact=True,both_full_rate_factor_bodies_exact=True,
        factor_prior_endpoint_reconstructed=True,historical_factor_proof=factor_proof,prior_factor_readback=readback,
        selector_proof=proof,verified_bytecode_DATA_not_compiled_Source=True)

def verify_successor(root,tag,*,source=None,prior):
    root=Path(root);actual=(root/prior.PATCH/'as/codo/controllers/MatchController.as').read_bytes()if source is None else source
    proof=prove_chain(root,tag,actual,prior)
    recipe=json.loads((HERE/'recipe.json').read_bytes())
    need(recipe['output_tag_sha256']==P.OUTPUT_TAG_SHA and recipe['output_movie_sha256']==P.OUTPUT_MOVIE_SHA and
         recipe['source_after_sha256']==P.SOURCE_SHA,'exact selector recipe endpoint')
    need(recipe['format']=='reviewed-active-ordinal-selector-DATA-successor-v1','exact selector recipe format')
    need(set(recipe['tools'])=={'selector_producer.py','current_endpoint.py','selector_export_contract.py','abc_reader.py','abc_list_reader.py','swf_data.py'},'closed selector tool set')
    for name,pin in recipe['tools'].items():exact(HERE/name,pin)
    lines=(root/prior.PATCH/'patch.txt').read_text(encoding='utf-8').splitlines()
    need(lines.count('as codo.controllers.MatchController as/codo/controllers/MatchController.as '+P.SOURCE_SHA)==1,'exact selector Source recipe row')
    exact(root/'tools/p2p-four-tag-composition/composition.py','636d7a05509499b8afff5e59ef1695f553b4ce3bc8cccd39289760b318b0d2f0')
    composition=prior.module('qualified_four_tag_composition',root/'tools/p2p-four-tag-composition/composition.py')
    proof['movie_composition']=composition.verify(root,P.OUTPUT_MOVIE_SHA)
    # No Source-only candidate may masquerade as an actual decompiler readback.
    pin=recipe['actual_export_sha256']
    need(type(pin)is str and re.fullmatch('[0-9a-f]{64}',pin),'Root-admitted selector readback still UNBOUND')
    checker=prior.module('closed_selector_export',HERE/'selector_export_contract.py')
    exported=exact(HERE/'actual-export.as.DATA',pin).decode('utf-8')
    proof['selector_readback']=checker.verify(exported)
    factor_checker=prior.module('preserved_factor_readback',root/'tools/training-factor-producer/export_contract.py')
    proof['new_export_factors']=factor_checker.verify(exported)
    return proof
