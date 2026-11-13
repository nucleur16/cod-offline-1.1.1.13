# -*- coding: utf-8 -*-
"""Portable source/data contracts; compiled pack checks are a separate artifact gate."""
from pathlib import Path
import hashlib
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from zonekit import build, codecs, packs
from party_locale_contract import PARTY_LOCALE, expected_effective


def strings(zone):
    result = {}
    for line in (ROOT/'zones'/zone/'localize.str').read_text(encoding='utf-8').splitlines():
        if line.startswith('#') or '\t' not in line:
            continue
        key, value = line.split('\t', 1)
        if key in result:
            raise ValueError('duplicate localization key: '+key)
        result[key] = value
    return result


def old_winners(language='en', baseline='source-parts'):
    # Independently model source alphabetical first-wins, including extra's
    # four loading translations that precede codol_loading_mode_names.
    source = {'000_codol_party_modes': strings('000_codol_party_modes'),
              'codol_loading_mode_names': strings('codol_loading_mode_names')}
    if language != 'en' and baseline == 'source-parts':
        source['codol_chinese_ui_extra'] = strings('codol_chinese_ui_extra')
    result = {}
    for zone in sorted(source):
        for key, value in source[zone].items():
            if key not in PARTY_LOCALE:continue
            blob = codecs.encode_localize(key,value.encode('ascii' if zone != 'codol_chinese_ui_extra' else 'gbk'))
            result.setdefault((27,key.lower()),('pre_gfx',zone,hashlib.md5(blob).hexdigest()))
    return result


class PartyLocales(unittest.TestCase):
    def test_legacy_english_values_remain_exact(self):
        old = {**strings('000_codol_party_modes'), **strings('codol_loading_mode_names')}
        self.assertEqual(set(old), set(PARTY_LOCALE))
        self.assertEqual(old, {k:v[0] for k,v in PARTY_LOCALE.items()})

    def test_chinese_values_and_registration_are_exact_lossless_gbk(self):
        chinese = strings('codol_chinese_ui_extra')
        registered = [n for t,n,_ in build.load(str(ROOT),'codol_chinese_ui_extra').assets if t=='localize']
        for key, (_english,value) in PARTY_LOCALE.items():
            self.assertEqual(chinese[key], value)
            self.assertEqual(registered.count(key), 1)
            self.assertEqual(value.encode('gbk').decode('gbk'), value)

    def test_shared_pack_keeps_tables_and_markers_but_not_localized_text(self):
        for part,keep,_ in packs.parts(packs.load(str(ROOT),'codol_common')):
            if part in ('000_codol_party_modes','codol_loading_mode_names'):
                self.assertFalse(keep('localize'))
                self.assertTrue(keep('rawfile'))
                self.assertTrue(keep('stringtable'))
        actual = {p for p,_,_ in packs.parts(packs.load(str(ROOT),'codol_common'))}
        self.assertTrue({'000_codol_party_modes','codol_loading_mode_names'} <= actual)

    def test_english_pack_takes_only_the_exact_legacy_text_parts(self):
        found = set()
        for part,keep,_ in packs.parts(packs.load(str(ROOT),'codol_english_locale')):
            if part in ('000_codol_party_modes','codol_loading_mode_names'):
                found.add(part)
                self.assertTrue(keep('localize'))
                self.assertFalse(keep('rawfile'))
                self.assertFalse(keep('stringtable'))
        self.assertEqual(found, {'000_codol_party_modes','codol_loading_mode_names'})
        self.assertIn('codol_chinese_ui_extra', {p for p,_,_ in packs.parts(packs.load(str(ROOT),'codol_chinese_locale'))})

    def test_legacy_shared_names_still_merge_into_the_shared_table_pack(self):
        header=(ROOT/'src/patch_locale_policy.h').read_text(encoding='utf-8')
        body=header.split('kCodolPatchZoneMerged[][2]={',1)[1].split('};',1)[0]
        merged=dict(re.findall(r'\{"([^"]+)","([^"]+)"\}',body))
        for name in ('000_codol_party_modes','codol_loading_mode_names'):
            self.assertEqual(merged[name],'codol_common')

    def test_checker_migrates_only_twelve_chinese_winners_and_keeps_english(self):
        before=old_winners();before[(37,'mp/comaplist.csv')]=('pre_gfx','shared','unchanged-table')
        self.assertEqual(expected_effective(before,'en',codecs.encode_localize),before)
        for locale in ('zh','native'):
            before=old_winners(locale);before[(37,'mp/comaplist.csv')]=('pre_gfx','shared','unchanged-table')
            after=expected_effective(before,locale,codecs.encode_localize)
            self.assertEqual(after[(37,'mp/comaplist.csv')],before[(37,'mp/comaplist.csv')])
            changed={k for k in before if before[k]!=after[k]}
            self.assertEqual(changed,{(27,k.lower()) for k in PARTY_LOCALE})
            for name,(_english,chinese) in PARTY_LOCALE.items():
                want=hashlib.md5(codecs.encode_localize(name,chinese.encode('gbk'))).hexdigest()
                self.assertEqual(after[(27,name.lower())][2],want)

    def test_checker_refuses_missing_changed_or_wrong_stage_legacy_values(self):
        for key in PARTY_LOCALE:
            k=(27,key.lower())
            for mode in ('missing','changed','stage'):
                before=old_winners('zh')
                if mode=='missing':del before[k]
                elif mode=='changed':before[k]=('pre_gfx','old','wrong-blob')
                else:before[k]=('ui','old',before[k][2])
                with self.subTest(key=key,mode=mode), self.assertRaises(ValueError):
                    expected_effective(before,'zh',codecs.encode_localize)

    def test_source_loading_keys_have_exact_chinese_extra_winners(self):
        for language in ('zh','native'):
            before=old_winners(language)
            for name in PARTY_LOCALE:
                zone=before[(27,name.lower())][1]
                self.assertEqual(zone,'codol_chinese_ui_extra' if name.startswith('CODOL_LOADING_MODE_') else '000_codol_party_modes')
            expected_effective(before,language,codecs.encode_localize)

    def test_pre_migration_profile_is_separate_and_not_auto_accepted(self):
        for language in ('en','zh','native'):
            before=old_winners(language,'pre-migration')
            expected_effective(before,language,codecs.encode_localize,'pre-migration')
            if language != 'en':
                with self.assertRaises(ValueError):expected_effective(before,language,codecs.encode_localize)
                with self.assertRaises(ValueError):expected_effective(old_winners(language),language,codecs.encode_localize,'pre-migration')

    def test_wrong_winning_zone_and_unknown_profiles_are_refused(self):
        for language in ('en','zh','native'):
            for name in PARTY_LOCALE:
                before=old_winners(language);key=(27,name.lower());phase,_zone,digest=before[key]
                before[key]=(phase,'unexpected-zone',digest)
                with self.subTest(language=language,key=name), self.assertRaises(ValueError):
                    expected_effective(before,language,codecs.encode_localize)
        with self.assertRaises(ValueError):expected_effective(old_winners(),'en',codecs.encode_localize,'unknown')
        with self.assertRaises(ValueError):expected_effective(old_winners(),'unknown',codecs.encode_localize)


if __name__=='__main__':unittest.main()
