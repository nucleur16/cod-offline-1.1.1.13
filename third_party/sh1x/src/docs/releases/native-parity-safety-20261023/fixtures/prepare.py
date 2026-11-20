"""Prepare the exact offline archive/animation regression in a NEW directory.

No compiler, container, server or client is launched. Donors remain read-only.
Supply preserved e43 fastfile and c8 animation source materializations.
"""
from pathlib import Path
import argparse,hashlib,json,re,shutil

FUNCTIONS=[('_scr_vm.asm',['Scr_GetAnim']),('_scr_animtree.asm',['Scr_GetAnims']),('_xanim.asm',['XAnimGetAnims','XAnimGetAnimTreeDebugName','XAnimGetAnimDebugName','XAnimGetTime','XAnimGetInfoIndex_r'])]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def extract(source):
    texts={name:(source/'src/asmsource'/name).read_text() for name,_ in FUNCTIONS}
    bodies={}
    for filename,names in FUNCTIONS:
        text=texts[filename]
        for name in names:
            start=text.index('\n'+name+':')+1;end=text.index('\n\n;',start)
            bodies[name]=text[start:end]
    external=['gScrVmPub','gScrVarPub','gScrCompilePub','gScrVmGlob','gScrAnimPub','g_xAnimInfo','var_typename','va','RemoveRefToValue','Sys_Error','Scr_ErrorJumpOut','Q_strncpyz','DB_IsXAssetDefault']
    parts=['BITS 32\nSECTION .text\n']+['extern '+name+'\n' for name in external]
    parts+=['global '+name+'\n'+body+'\n' for name,body in bodies.items()]
    labels=sorted(set(re.findall(r'\b_cstring_\w+','\n'.join(bodies.values()))))
    parts.append('SECTION .rodata\n');pool=texts['_scr_vm.asm']+'\n'+texts['_xanim.asm']
    for label in labels:
        match=re.search(r'^'+label+r':[^\n]*(?:\n\s*db[^\n]*)?',pool,re.M)
        assert match,('missing retained string',label)
        parts.append(match.group(0)+'\n')
    parts.append('SECTION .bss\nerror_message: resb 1024\n')
    return ''.join(parts),{name:hashlib.sha256(body.encode()).hexdigest() for name,body in bodies.items()}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',required=True,choices=['normal','zombies'])
    parser.add_argument('--source',required=True,type=Path)
    parser.add_argument('--animation-donor',required=True,type=Path)
    parser.add_argument('--unsigned-donor',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args();here=Path(__file__).resolve().parent
    q=json.loads((here.parent/'qualification.json').read_text())
    source=args.source.resolve();animation=args.animation_donor.resolve();unsigned=args.unsigned_donor.resolve();output=args.output.resolve()
    assert not output.exists(),'output must be new'
    assert all(not output.is_relative_to(p) for p in [source,animation,unsigned,here]),'output must be separate from inputs'
    assert sha(source/'src/codol/script/codol_script_anim.c')==q['profiles'][args.profile]['animation_leaf_sha256']
    assert sha(source/'src/codol/assets/codol_fastfile.c')==q['unsigned_archive']['corrected_loader_sha256']
    assert sha(animation/'src/codol/script/codol_script_anim.c')==q['animation_reference']['donor_leaf_sha256']
    assert sha(unsigned/'src/codol/assets/codol_fastfile.c')==q['unsigned_archive']['donor_loader_sha256']
    for name,digest in q['native_libraries'].items():assert sha(source/'lib-native'/name)==digest
    assembly,bodies=extract(source);assert bodies==q['animation_reference']['retained_function_body_sha256']
    output.mkdir(parents=True)
    for name in ['archive_probe.c','test_anim_receiver.c','run.sh']:shutil.copyfile(here/name,output/name)
    (output/'retained.asm').write_text(assembly,encoding='utf-8',newline='\n')
    (output/'qa.sha256').write_text(q['unsigned_archive']['archive_sha256']+'  /qa/archive.ff\n'+q['unsigned_archive']['key_input_sha256']+'  /key/C1.exe\n',encoding='utf-8',newline='\n')
    receipt={'schema':'codol-offline-parity-regression-preparation-v1','profile':args.profile,'qualification_sha256':sha(here.parent/'qualification.json'),'retained_function_body_sha256':bodies,'prepared_files':{p.name:sha(p) for p in output.iterdir()},'runtime_or_compiler_launched':False,'key_material_read_or_logged':False}
    (output/'preparation.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(receipt,indent=2))
if __name__=='__main__':main()
