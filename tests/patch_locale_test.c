#include <assert.h>
#include <stdio.h>
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
 puts("canonical language selection and shared-zone policy passed");
 return 0;
}
