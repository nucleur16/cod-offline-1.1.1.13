# -*- coding: utf-8 -*-
"""Decode a baseline/candidate three-pack set and verify only the reviewed text migration.

Never launches an executable. --exe is a read-only key input to the existing
container reader. Baseline and candidate MUST share all non-locale source edits:
rebuild both after the P2P owner's separate comaplist correction is merged.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from zonekit import codecs,container,packs,zoneimage
from party_locale_contract import PARTY_LOCALE

PACKS=('codol_common','codol_english_locale','codol_chinese_locale')
LEGACY=('000_codol_party_modes','codol_loading_mode_names')


def named(image):
    rows=[]
    for typ,_ptr,blob in image.assets:
        if typ==27:name=codecs.decode_localize(blob)[0]
        elif typ==36:name=codecs.decode_rawfile(blob)[0]
        elif typ==37:name=codecs.decode_stringtable(blob)[0]
        else:name='@blob:'+hashlib.sha256(blob).hexdigest()
        rows.append((typ,name,blob))
    return rows


def localized(rows):
    result={}
    for typ,name,blob in rows:
        if typ!=27:continue
        assert name not in result, 'duplicate localize key '+name
        result[name]=codecs.decode_localize(blob)[1]
    return result


def validate(before,after):
    assert set(before)==set(after)==set(PACKS), 'all three reviewed packs are required together'
    common_old=localized(before['codol_common'])
    assert common_old=={k:v[0].encode('ascii') for k,v in PARTY_LOCALE.items()}
    assert localized(after['codol_common'])=={}, 'shared common pack still owns localized text'
    for pack in PACKS:
        old=[r for r in before[pack] if r[0]!=27]
        new=[r for r in after[pack] if r[0]!=27]
        assert old==new, 'non-localize assets changed: '+pack
    for pack,index,encoding in [('codol_english_locale',0,'ascii'),('codol_chinese_locale',1,'gbk')]:
        old=localized(before[pack]);new=localized(after[pack])
        assert not set(PARTY_LOCALE)&set(old), 'baseline already has migration keys; use correct predecessor'
        expected=dict(old)
        expected.update({k:v[index].encode(encoding) for k,v in PARTY_LOCALE.items()})
        assert new==expected, 'unexpected locale key/value change: '+pack
        assert {r[1]:r[2] for r in before[pack] if r[0]==27} == {
            r[1]:r[2] for r in after[pack] if r[0]==27 and r[1] not in PARTY_LOCALE}, 'unrelated localization blob changed'
    return True


def merged_table():
    source=(ROOT/'patch_locale_policy.h').read_text(encoding='utf-8')
    source=source.split('kCodolPatchZoneMerged[][2]={',1)[1].split('};',1)[0]
    return {old.lower():new.lower() for old,new in re.findall(r'\{"([^"]+)","([^"]+)"\}',source)}


def effective(zones,language,use_merge=True):
    """Explicit source-policy model, not an execution of the Windows file loader."""
    table=merged_table();names={n.lower() for n in zones};selected=[]
    for name in sorted(zones,key=str.lower):
        if use_merge and table.get(name.lower()) in names:continue
        if language not in packs.locales(name):continue
        selected.append(name)
    result={};duplicates=[]
    for name in selected:
        for typ,key,blob in zones[name]:
            identity=(typ,key.lower())
            if identity in result:duplicates.append(identity)
            else:result[identity]=blob
    return result,duplicates,selected


def validate_complete_folder(candidate,legacy,use_merge=True):
    assert set(candidate)==set(PACKS)
    assert set(legacy)==set(LEGACY)
    for language,index,encoding in [('en',0,'ascii'),('zh',1,'gbk'),('native',1,'gbk')]:
        winners,duplicates,selected=effective({**candidate,**legacy},language,use_merge)
        assert not duplicates, 'duplicate selected assets in '+language
        assert not set(LEGACY)&set(selected), 'retained old shared names were not skipped'
        for key,value in PARTY_LOCALE.items():
            assert codecs.decode_localize(winners[(27,key.lower())])[1]==value[index].encode(encoding), key
    return True


def read_set(directory,names,exe):
    return {name:named(zoneimage.parse(container.read_unsigned((directory/(name+'.ff')).read_bytes(),str(exe)).zone)) for name in names}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--baseline',type=Path,required=True)
    ap.add_argument('--candidate',type=Path,required=True)
    ap.add_argument('--legacy',type=Path,required=True)
    ap.add_argument('--exe',type=Path,required=True)
    ap.add_argument('--receipt',type=Path,required=True)
    a=ap.parse_args()
    before=read_set(a.baseline,PACKS,a.exe);after=read_set(a.candidate,PACKS,a.exe);legacy=read_set(a.legacy,LEGACY,a.exe)
    validate(before,after);validate_complete_folder(after,legacy)
    # Legacy-only data is still a compatibility fallback, not translated r46.
    for language in ('en','zh','native'):
        values,_dups,_selected=effective(legacy,language)
        for key,(english,_chinese) in PARTY_LOCALE.items():
            assert codecs.decode_localize(values[(27,key.lower())])[1]==english.encode('ascii')
    source_files=['patch_locale_policy.h','zone_packs/codol_common.csv','zone_packs/codol_english_locale.csv',
                  'zone_packs/codol_chinese_locale.csv','zone_source/codol_chinese_ui_extra.csv',
                  'zones/codol_chinese_ui_extra/localize.str']
    manifest={'scope':'Unlaunched source/decoded three-pack proof; MUST rebuild from final merged locale+P2P comaplist sources',
       'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in source_files},
       'required_updated_packs_together':[{ 'name':name,'sha256':hashlib.sha256((a.candidate/(name+'.ff')).read_bytes()).hexdigest(),
           'bytes':(a.candidate/(name+'.ff')).stat().st_size} for name in PACKS],
       'English_values_unchanged':12,'Chinese_GBK_values_exact':12,'unrelated_localize_and_nonlocalize_assets_unchanged':True,
       'retained_legacy_shared_files_skipped_with_complete_pack_set':True,
       'legacy_only_English_fallback_is_not_bilingual_qualification':True,
       'Windows_executables_launched':False,'game_or_DLL_runtime_qualified':False,
       'final_merged_comaplist_gates_required':True}
    a.receipt.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(manifest))


if __name__=='__main__':main()
