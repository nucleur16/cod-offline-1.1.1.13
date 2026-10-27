#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../patch_locale_policy.h"
#define LOCALE(...) do { const wchar_t *v[]={L"C1.exe",__VA_ARGS__}; \
 assert(CodolPatchLanguageFromArgv(sizeof(v)/sizeof(v[0]),v)==expected); } while(0)
int main(void)
{
 enum CodolPatchLanguage expected=CODOL_PATCH_EN;
 const wchar_t *empty[]={L"C1.exe"};
 assert(CodolPatchLanguageFromArgv(1,empty)==expected);
 LOCALE(L"+set",L"k33_english",L"1");
 LOCALE(L"+set",L"k33_english",L"0",L"+set",L"k33_english",L"1");
 LOCALE(L"+set",L"k33_english",L"1",L"+set",L"codol_patchlang",L"zh");
 LOCALE(L"+set",L"k33_english",L"2");
 LOCALE(L"+set",L"k33_english");
 expected=CODOL_PATCH_ZH;
 LOCALE(L"+set",L"k33_english",L"0");
 LOCALE(L"+set",L"K33_ENGLISH",L"0");
 LOCALE(L"+set",L"codol_patchlang",L"zh");
 LOCALE(L"+set",L"codol_patchlang",L"en",L"+set",L"k33_english",L"0");
 LOCALE(L"+set",L"k33_english",L"0",L"+set",L"codol_patchlang",L"en");
 LOCALE(L"+set",L"k33_english",L"0",L"+set",L"k33_english",L"invalid");
 LOCALE(L"+set",L"codol_patchlang",L"zh",L"+set",L"k33_english",L"00");
 expected=CODOL_PATCH_NATIVE;
 LOCALE(L"+set",L"codol_patchlang",L"unsupported");
 assert(CodolPatchZoneLocaleAllowed("voice_ui_mp",CODOL_PATCH_ZH));
 assert(CodolPatchZoneLocaleAllowed("controller_ingame",CODOL_PATCH_EN));
 assert(CodolPatchZoneLocaleAllowed("codol_chinese_ui",CODOL_PATCH_ZH));
 assert(!CodolPatchZoneLocaleAllowed("codol_english_ui",CODOL_PATCH_ZH));
 assert(!CodolPatchZoneLocaleAllowed("codol_chinese_ui",CODOL_PATCH_EN));
 assert(CodolPatchZoneLocaleAllowed("codol_english_usermaps",CODOL_PATCH_EN));
 assert(CodolPatchZoneLocaleAllowed("codol_chinese_ui",CODOL_PATCH_NATIVE));
 assert(!CodolPatchZoneLocaleAllowed("codol_english_ui",CODOL_PATCH_NATIVE));
 assert(CodolPatchZoneLocaleAllowed("voice_ui_mp",CODOL_PATCH_NATIVE));
 for(int locale=CODOL_PATCH_EN;locale<=CODOL_PATCH_NATIVE;locale++) {
  assert(!CodolPatchZoneLocaleAllowed("codol_zombies_endless",locale));
  assert(!CodolPatchZoneLocaleAllowed("CODOL_ZOMBIES_ENDLESS",locale));
  assert(CodolPatchZoneLocaleAllowed("usermaps_patch",locale));
  assert(CodolPatchZoneLocaleAllowed("patch_common_gametype_zm",locale));
  assert(CodolPatchZoneLocaleAllowed("codol_zombies_endless_extra",locale));
 }
 /* consolidated zones: every old name maps to the zone that carries it now; new and unrelated names map to nothing */
 assert(!strcmp(CodolPatchZoneMergedInto("voice_ui_mp"),"codol_common_ui_mp"));
 assert(!strcmp(CodolPatchZoneMergedInto("VOICE_UI_MP"),"codol_common_ui_mp"));
 assert(!strcmp(CodolPatchZoneMergedInto("000_codol_party_modes"),"codol_common"));
 assert(!strcmp(CodolPatchZoneMergedInto("usermaps_patch"),"codol_common"));
 assert(!strcmp(CodolPatchZoneMergedInto("controller_glyphs_postgfx_mp"),"codol_common_postgfx_mp"));
 assert(!strcmp(CodolPatchZoneMergedInto("codol_english"),"codol_english_locale"));
 assert(!strcmp(CodolPatchZoneMergedInto("codol_chinese_ui_extra"),"codol_chinese_locale"));
 assert(!CodolPatchZoneMergedInto("codol_common"));
 assert(!CodolPatchZoneMergedInto("codol_english_locale"));
 assert(!CodolPatchZoneMergedInto("codol_english_ui_extra2"));
 assert(!CodolPatchZoneMergedInto("codol_infected_cyborg_postgfx_mp"));
 assert(!CodolPatchZoneMergedInto("patch_common_gametype_zm"));
 /* the consolidated zones keep their old zones' languages */
 assert(CodolPatchZoneLocaleAllowed("codol_english_locale",CODOL_PATCH_EN));
 assert(!CodolPatchZoneLocaleAllowed("codol_english_locale",CODOL_PATCH_ZH));
 assert(!CodolPatchZoneLocaleAllowed("codol_english_locale",CODOL_PATCH_NATIVE));
 assert(CodolPatchZoneLocaleAllowed("codol_chinese_locale",CODOL_PATCH_ZH));
 assert(CodolPatchZoneLocaleAllowed("codol_chinese_locale",CODOL_PATCH_NATIVE));
 assert(!CodolPatchZoneLocaleAllowed("codol_chinese_locale",CODOL_PATCH_EN));
 for(int locale=CODOL_PATCH_EN;locale<=CODOL_PATCH_NATIVE;locale++) {
  assert(CodolPatchZoneLocaleAllowed("codol_common",locale));
  assert(CodolPatchZoneLocaleAllowed("codol_common_ui_mp",locale));
  assert(CodolPatchZoneLocaleAllowed("codol_common_postgfx_mp",locale));
 }
 puts("canonical language selection and shared-zone policy passed");
 return 0;
}
