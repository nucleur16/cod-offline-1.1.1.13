#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/patch_locale_policy.h"
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
 assert(!strcmp(CodolPatchZoneMergedInto("codol_loading_mode_names"),"codol_common"));
 assert(!strcmp(CodolPatchZoneMergedInto("CODOL_LOADING_MODE_NAMES"),"codol_common"));
 assert(!CodolPatchZoneMergedInto("codol_loading_mode_names_extra"));
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
 /* ---- languages by name ---- */
 {
  char lang[CODOL_LANG_NAME_MAX];
  char list[CODOL_LANG_MAX][CODOL_LANG_NAME_MAX];
  char joined[64];
  int count=0;
  assert(CodolLanguageCanonical("en",lang) && !strcmp(lang,"en"));
  assert(CodolLanguageCanonical("EN",lang) && !strcmp(lang,"en"));
  assert(CodolLanguageCanonical("English",lang) && !strcmp(lang,"en"));
  assert(CodolLanguageCanonical("chinese",lang) && !strcmp(lang,"zh"));
  assert(CodolLanguageCanonical("zh",lang) && !strcmp(lang,"zh"));
  assert(CodolLanguageCanonical("ptBR",lang) && !strcmp(lang,"ptbr"));
  assert(!CodolLanguageCanonical("",lang));
  assert(!CodolLanguageCanonical(NULL,lang));
  assert(!CodolLanguageCanonical("native",lang));
  assert(!CodolLanguageCanonical("en;quit",lang));
  assert(!CodolLanguageCanonical("en zh",lang));
  assert(!CodolLanguageCanonical("../x",lang));
  assert(!CodolLanguageCanonical("abcdefghijklmnop",lang));        /* 16 characters: one too many */
  assert(CodolLanguageCanonical("abcdefghijklmno",lang));
  assert(CodolLanguageCanonicalWide(L"ZH",lang) && !strcmp(lang,"zh"));
  assert(!CodolLanguageCanonicalWide(L"\x4e2d\x6587",lang));
  assert(!strcmp(CodolLanguageZoneStem("en"),"english"));
  assert(!strcmp(CodolLanguageZoneStem("zh"),"chinese"));
  assert(!strcmp(CodolLanguageZoneStem("fr"),"fr"));
  /* any codol_<name>_locale is a language; the two shipped zones keep their names */
  assert(CodolLocaleZoneLanguage("codol_english_locale",lang) && !strcmp(lang,"en"));
  assert(CodolLocaleZoneLanguage("CODOL_CHINESE_LOCALE",lang) && !strcmp(lang,"zh"));
  assert(CodolLocaleZoneLanguage("codol_fr_locale",lang) && !strcmp(lang,"fr"));
  assert(!CodolLocaleZoneLanguage("codol_common",lang));
  assert(!CodolLocaleZoneLanguage("codol__locale",lang));
  assert(!CodolLocaleZoneLanguage("codol_a_b_locale",lang));
  assert(!CodolLocaleZoneLanguage("codol_fr_locale_extra",lang));
  assert(!CodolLocaleZoneLanguage("xcodol_fr_locale",lang));
  assert(!CodolLocaleZoneLanguage("codol_native_locale",lang));
  assert(CodolPatchZoneLanguage("codol_english_ui_extra",lang) && !strcmp(lang,"en"));
  assert(CodolPatchZoneLanguage("codol_chinese_ui",lang) && !strcmp(lang,"zh"));
  assert(CodolPatchZoneLanguage("codol_de_locale",lang) && !strcmp(lang,"de"));
  assert(!CodolPatchZoneLanguage("codol_common_ui_mp",lang));
  assert(!CodolPatchZoneLanguage("mp_codol_weapons",lang));
  /* the name-based filter agrees with the two-language one and extends it */
  {
   static const char *const zones[]={"codol_english_locale","codol_chinese_locale","codol_english_ui","codol_chinese_ui_extra",
    "codol_common","codol_common_ui_mp","voice_ui_mp","codol_zombies_endless","usermaps_patch","codol_infected_cyborg_postgfx_mp"};
   for(unsigned i=0;i<sizeof(zones)/sizeof(zones[0]);i++) {
    assert(CodolPatchZoneAllowedFor(zones[i],"en")==CodolPatchZoneLocaleAllowed(zones[i],CODOL_PATCH_EN));
    assert(CodolPatchZoneAllowedFor(zones[i],"zh")==CodolPatchZoneLocaleAllowed(zones[i],CODOL_PATCH_ZH));
   }
  }
  assert(CodolPatchZoneAllowedFor("codol_fr_locale","fr"));
  assert(!CodolPatchZoneAllowedFor("codol_fr_locale","en"));
  assert(!CodolPatchZoneAllowedFor("codol_english_locale","fr"));
  assert(CodolPatchZoneAllowedFor("codol_common","fr"));
  /* the command line: k33_language, the last valid one */
  {
   const wchar_t *a[]={L"C1.exe",L"+set",L"k33_language",L"zh"};
   const wchar_t *b[]={L"C1.exe",L"+set",L"k33_language",L"zh",L"+set",L"K33_LANGUAGE",L"English"};
   const wchar_t *c[]={L"C1.exe",L"+set",L"k33_language",L"fr",L"+set",L"k33_language",L"not a name"};
   const wchar_t *d[]={L"C1.exe",L"+set",L"k33_language"};
   const wchar_t *e[]={L"C1.exe",L"+set",L"k33_english",L"0"};
   const wchar_t *f[]={L"C1.exe",L"+set",L"codol_patchlang",L"unsupported"};
   const wchar_t *g[]={L"C1.exe",L"+set",L"k33_english",L"0",L"+set",L"k33_language",L"en"};
   const wchar_t *h[]={L"C1.exe",L"+set",L"k33_english",L"2"};
   const wchar_t *n[]={L"C1.exe"};
   const char cfgZh[]="// generated\nseta 1848 \"0\"\nseta k33_language \"zh\"\nbind W \"+forward\"\n";
   const char cfgTwo[]="seta k33_language \"zh\"\r\nseta k33_language \"fr\"\r\n";
   const char cfgBad[]="seta k33_language \"no;pe\"\nseta k33_languageList \"en;zh\"\n// seta k33_language \"zh\"\nsetak33_language zh\n";
   const char cfgSet[]="  set   k33_language   Chinese\n";
   assert(CodolLanguageFromArgv(4,a,lang) && !strcmp(lang,"zh"));
   assert(CodolLanguageFromArgv(7,b,lang) && !strcmp(lang,"en"));
   assert(CodolLanguageFromArgv(7,c,lang) && !strcmp(lang,"fr"));
   assert(!CodolLanguageFromArgv(3,d,lang));
   assert(!CodolLanguageFromArgv(4,e,lang));
   assert(!CodolPatchLanguageAliasSeen(4,a) && CodolPatchLanguageAliasSeen(4,e) && CodolPatchLanguageAliasSeen(4,f));
   assert(!CodolPatchLanguageAliasSeen(4,h) && !CodolPatchLanguageAliasSeen(1,n));
   assert(CodolLanguageFromConfig(cfgZh,sizeof(cfgZh)-1,lang) && !strcmp(lang,"zh"));
   assert(CodolLanguageFromConfig(cfgTwo,sizeof(cfgTwo)-1,lang) && !strcmp(lang,"fr"));
   assert(!CodolLanguageFromConfig(cfgBad,sizeof(cfgBad)-1,lang));
   assert(CodolLanguageFromConfig(cfgSet,sizeof(cfgSet)-1,lang) && !strcmp(lang,"zh"));
   assert(!CodolLanguageFromConfig("",0,lang));
   /* order: k33_language, the older selectors, the saved choice, English */
   assert(CodolStartupLanguage(4,a,cfgTwo,sizeof(cfgTwo)-1,lang)==CODOL_LANG_ARGV && !strcmp(lang,"zh"));
   assert(CodolStartupLanguage(7,g,cfgTwo,sizeof(cfgTwo)-1,lang)==CODOL_LANG_ARGV && !strcmp(lang,"en"));
   assert(CodolStartupLanguage(4,e,cfgTwo,sizeof(cfgTwo)-1,lang)==CODOL_LANG_ALIAS && !strcmp(lang,"zh"));
   assert(CodolStartupLanguage(4,f,NULL,0,lang)==CODOL_LANG_ALIAS && !strcmp(lang,"zh"));
   assert(CodolStartupLanguage(4,h,cfgZh,sizeof(cfgZh)-1,lang)==CODOL_LANG_CONFIG && !strcmp(lang,"zh"));
   assert(CodolStartupLanguage(1,n,cfgZh,sizeof(cfgZh)-1,lang)==CODOL_LANG_CONFIG && !strcmp(lang,"zh"));
   assert(CodolStartupLanguage(1,n,cfgBad,sizeof(cfgBad)-1,lang)==CODOL_LANG_DEFAULT && !strcmp(lang,"en"));
   assert(CodolStartupLanguage(1,n,NULL,0,lang)==CODOL_LANG_DEFAULT && !strcmp(lang,"en"));
  }
  /* the list: sorted, no duplicates, bounded */
  assert(CodolLanguageListAdd(list,&count,"zh")==0);
  assert(CodolLanguageListAdd(list,&count,"en")==0);
  assert(CodolLanguageListAdd(list,&count,"zh")==1);
  assert(CodolLanguageListAdd(list,&count,"fr")==1 && count==3);
  CodolLanguageListJoin(list,count,joined,sizeof(joined));
  assert(!strcmp(joined,"en;fr;zh"));
  assert(CodolLanguageListFind(list,count,"zh")==2 && CodolLanguageListFind(list,count,"de")==-1);
  CodolLanguageListJoin(list,count,joined,6);
  assert(!strcmp(joined,"en;fr"));
  { char name[4]="l0"; for(int i=0;count<CODOL_LANG_MAX;i++) { name[1]=(char)('0'+i); assert(CodolLanguageListAdd(list,&count,name)>=0); } }
  assert(CodolLanguageListAdd(list,&count,"zz")==-1 && count==CODOL_LANG_MAX);
 }
 puts("canonical language selection and shared-zone policy passed");
 return 0;
}
