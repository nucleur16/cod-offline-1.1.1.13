#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/patch_locale_policy.h"
#define LOCALE(...) do { const wchar_t *v[]={L"C1.exe",__VA_ARGS__}; \
 assert(PatchLanguageFromArgv(sizeof(v)/sizeof(v[0]),v)==expected); } while(0)
int main(void)
{
 enum PatchLanguage expected=PATCH_EN;
 const wchar_t *empty[]={L"C1.exe"};
 assert(PatchLanguageFromArgv(1,empty)==expected);
 LOCALE(L"+set",L"k33_english",L"1");
 LOCALE(L"+set",L"k33_english",L"0",L"+set",L"k33_english",L"1");
 LOCALE(L"+set",L"k33_english",L"1",L"+set",L"loc_patchLang",L"zh");
 LOCALE(L"+set",L"k33_english",L"2");
 LOCALE(L"+set",L"k33_english");
 expected=PATCH_ZH;
 LOCALE(L"+set",L"k33_english",L"0");
 LOCALE(L"+set",L"K33_ENGLISH",L"0");
 LOCALE(L"+set",L"loc_patchLang",L"zh");
 LOCALE(L"+set",L"loc_patchLang",L"en",L"+set",L"k33_english",L"0");
 LOCALE(L"+set",L"k33_english",L"0",L"+set",L"loc_patchLang",L"en");
 LOCALE(L"+set",L"k33_english",L"0",L"+set",L"k33_english",L"invalid");
 LOCALE(L"+set",L"loc_patchLang",L"zh",L"+set",L"k33_english",L"00");
 expected=PATCH_NATIVE;
 LOCALE(L"+set",L"loc_patchLang",L"unsupported");
 assert(PatchZoneLocaleAllowed("voice_ui_mp",PATCH_ZH));
 assert(PatchZoneLocaleAllowed("controller_ingame",PATCH_EN));
 assert(PatchZoneLocaleAllowed("chinese_ui",PATCH_ZH));
 assert(!PatchZoneLocaleAllowed("english_ui",PATCH_ZH));
 assert(!PatchZoneLocaleAllowed("chinese_ui",PATCH_EN));
 assert(PatchZoneLocaleAllowed("english_usermaps",PATCH_EN));
 assert(PatchZoneLocaleAllowed("chinese_ui",PATCH_NATIVE));
 assert(!PatchZoneLocaleAllowed("english_ui",PATCH_NATIVE));
 assert(PatchZoneLocaleAllowed("voice_ui_mp",PATCH_NATIVE));
 for(int locale=PATCH_EN;locale<=PATCH_NATIVE;locale++) {
  assert(!PatchZoneLocaleAllowed("codol_zombies_endless",locale));
  assert(!PatchZoneLocaleAllowed("CODOL_ZOMBIES_ENDLESS",locale));
  assert(PatchZoneLocaleAllowed("usermaps_patch",locale));
  assert(PatchZoneLocaleAllowed("patch_common_gametype_zm",locale));
  assert(PatchZoneLocaleAllowed("codol_zombies_endless_extra",locale));
 }
 /* consolidated zones: every old name maps to the zone that carried it then, that one to its name without "codol";
  * the current names and unrelated names map to nothing */
 assert(!strcmp(PatchZoneMergedInto("voice_ui_mp"),"codol_common_ui_mp")); /* LEGACY_ZONE */
 assert(!strcmp(PatchZoneMergedInto("VOICE_UI_MP"),"codol_common_ui_mp")); /* LEGACY_ZONE */
 assert(!strcmp(PatchZoneMergedInto("000_codol_party_modes"),"codol_common")); /* LEGACY_ZONE */
 assert(!strcmp(PatchZoneMergedInto("codol_loading_mode_names"),"codol_common")); /* LEGACY_ZONE */
 assert(!strcmp(PatchZoneMergedInto("CODOL_LOADING_MODE_NAMES"),"codol_common")); /* LEGACY_ZONE */
 assert(!PatchZoneMergedInto("codol_loading_mode_names_extra"));
 assert(!strcmp(PatchZoneMergedInto("usermaps_patch"),"codol_common")); /* LEGACY_ZONE */
 assert(!strcmp(PatchZoneMergedInto("controller_glyphs_postgfx_mp"),"codol_common_postgfx_mp")); /* LEGACY_ZONE */
 assert(!strcmp(PatchZoneMergedInto("codol_english"),"codol_english_locale")); /* LEGACY_ZONE */
 assert(!strcmp(PatchZoneMergedInto("codol_chinese_ui_extra"),"codol_chinese_locale")); /* LEGACY_ZONE */
 assert(!strcmp(PatchZoneMergedInto("codol_common"),"common")); /* LEGACY_ZONE */
 assert(!strcmp(PatchZoneMergedInto("codol_english_locale"),"english_locale")); /* LEGACY_ZONE */
 assert(!strcmp(PatchZoneMergedInto("codol_infected_cyborg_postgfx_mp"),"infected_cyborg_postgfx_mp")); /* LEGACY_ZONE */
 assert(!strcmp(PatchZoneMergedInto("mp_codol_weapons_postgfx_mp"),"mp_weapons_postgfx_mp")); /* LEGACY_ZONE */
 assert(!PatchZoneMergedInto("common"));
 assert(!PatchZoneMergedInto("english_locale"));
 assert(!PatchZoneMergedInto("english"));
 assert(!PatchZoneMergedInto("codol_english_ui_extra2"));
 assert(!PatchZoneMergedInto("infected_cyborg_postgfx_mp"));
 assert(!PatchZoneMergedInto("mp_weapons"));
 assert(!PatchZoneMergedInto("patch_common_gametype_zm"));
 {  /* the chain: an older install's part is skipped for its old pack and for the renamed pack */
  const char *chain[PATCH_ZONE_CHAIN_MAX];
  assert(PatchZoneMergedChain("codol_english_ui",chain)==2); /* LEGACY_ZONE */
  assert(!strcmp(chain[0],"codol_english_locale") && !strcmp(chain[1],"english_locale")); /* LEGACY_ZONE */
  assert(PatchZoneMergedChain("voice_ui_mp",chain)==2 && !strcmp(chain[1],"common_ui_mp"));
  assert(PatchZoneMergedChain("codol_zm_m1911_arms_diag",chain)==1 && !strcmp(chain[0],"m1911_arms_diag")); /* LEGACY_ZONE */
  assert(PatchZoneMergedChain("common",chain)==0);
 }
 /* the consolidated zones keep their old zones' languages, under the new names and an older install's */
 assert(PatchZoneLocaleAllowed("codol_english_locale",PATCH_EN) && !PatchZoneLocaleAllowed("codol_english_locale",PATCH_ZH)); /* LEGACY_ZONE */
 assert(PatchZoneLocaleAllowed("codol_chinese_ui",PATCH_ZH) && !PatchZoneLocaleAllowed("codol_chinese_ui",PATCH_EN)); /* LEGACY_ZONE */
 assert(PatchZoneLocaleAllowed("english_locale",PATCH_EN));
 assert(!PatchZoneLocaleAllowed("english_locale",PATCH_ZH));
 assert(!PatchZoneLocaleAllowed("english_locale",PATCH_NATIVE));
 assert(PatchZoneLocaleAllowed("chinese_locale",PATCH_ZH));
 assert(PatchZoneLocaleAllowed("chinese_locale",PATCH_NATIVE));
 assert(!PatchZoneLocaleAllowed("chinese_locale",PATCH_EN));
 for(int locale=PATCH_EN;locale<=PATCH_NATIVE;locale++) {
  assert(PatchZoneLocaleAllowed("common",locale));
  assert(PatchZoneLocaleAllowed("common_ui_mp",locale));
  assert(PatchZoneLocaleAllowed("common_postgfx_mp",locale));
 }
 /* ---- languages by name ---- */
 {
  char lang[LANG_NAME_MAX];
  char list[LANG_MAX][LANG_NAME_MAX];
  char joined[64];
  int count=0;
  assert(LanguageCanonical("en",lang) && !strcmp(lang,"en"));
  assert(LanguageCanonical("EN",lang) && !strcmp(lang,"en"));
  assert(LanguageCanonical("English",lang) && !strcmp(lang,"en"));
  assert(LanguageCanonical("chinese",lang) && !strcmp(lang,"zh"));
  assert(LanguageCanonical("zh",lang) && !strcmp(lang,"zh"));
  assert(LanguageCanonical("ptBR",lang) && !strcmp(lang,"ptbr"));
  assert(!LanguageCanonical("",lang));
  assert(!LanguageCanonical(NULL,lang));
  assert(!LanguageCanonical("native",lang));
  assert(!LanguageCanonical("en;quit",lang));
  assert(!LanguageCanonical("en zh",lang));
  assert(!LanguageCanonical("../x",lang));
  assert(!LanguageCanonical("abcdefghijklmnop",lang));        /* 16 characters: one too many */
  assert(LanguageCanonical("abcdefghijklmno",lang));
  assert(LanguageCanonicalWide(L"ZH",lang) && !strcmp(lang,"zh"));
  assert(!LanguageCanonicalWide(L"\x4e2d\x6587",lang));
  assert(!strcmp(LanguageZoneStem("en"),"english"));
  assert(!strcmp(LanguageZoneStem("zh"),"chinese"));
  assert(!strcmp(LanguageZoneStem("fr"),"fr"));
  /* any <name>_locale is a language; the two shipped zones keep their names */
  assert(LocaleZoneLanguage("english_locale",lang) && !strcmp(lang,"en"));
  assert(LocaleZoneLanguage("CHINESE_LOCALE",lang) && !strcmp(lang,"zh"));
  assert(LocaleZoneLanguage("fr_locale",lang) && !strcmp(lang,"fr"));
  assert(!LocaleZoneLanguage("common",lang));
  assert(!LocaleZoneLanguage("_locale",lang));
  assert(!LocaleZoneLanguage("a_b_locale",lang));
  assert(!LocaleZoneLanguage("fr_locale_extra",lang));
  assert(!LocaleZoneLanguage("native_locale",lang));
  /* an older install's codol_<name>_locale is not a switchable locale zone, but still that language's zone */
  assert(!LocaleZoneLanguage("codol_fr_locale",lang));
  assert(!LocaleZoneLanguage("xcodol_fr_locale",lang));
  assert(!PatchZoneLanguage("xcodol_fr_locale",lang));
  assert(PatchZoneLanguage("CODOL_CHINESE_LOCALE",lang) && !strcmp(lang,"zh"));
  assert(PatchZoneLanguage("codol_english_locale",lang) && !strcmp(lang,"en")); /* LEGACY_ZONE */
  assert(PatchZoneLanguage("english_ui_extra",lang) && !strcmp(lang,"en"));
  assert(PatchZoneLanguage("chinese_ui",lang) && !strcmp(lang,"zh"));
  assert(PatchZoneLanguage("codol_de_locale",lang) && !strcmp(lang,"de"));
  assert(PatchZoneLanguage("de_locale",lang) && !strcmp(lang,"de"));
  assert(!PatchZoneLanguage("common_ui_mp",lang));
  assert(!PatchZoneLanguage("mp_weapons",lang));
  /* the name-based filter agrees with the two-language one and extends it */
  {
   static const char *const zones[]={"english_locale","chinese_locale","english_ui","chinese_ui_extra",
    "common","common_ui_mp","voice_ui_mp","codol_zombies_endless","usermaps_patch","infected_cyborg_postgfx_mp"};
   for(unsigned i=0;i<sizeof(zones)/sizeof(zones[0]);i++) {
    assert(PatchZoneAllowedFor(zones[i],"en")==PatchZoneLocaleAllowed(zones[i],PATCH_EN));
    assert(PatchZoneAllowedFor(zones[i],"zh")==PatchZoneLocaleAllowed(zones[i],PATCH_ZH));
   }
  }
  assert(PatchZoneAllowedFor("codol_fr_locale","fr"));
  assert(!PatchZoneAllowedFor("codol_fr_locale","en"));
  assert(PatchZoneAllowedFor("fr_locale","fr"));
  assert(!PatchZoneAllowedFor("fr_locale","en"));
  assert(!PatchZoneAllowedFor("english_locale","fr"));
  assert(PatchZoneAllowedFor("common","fr"));
  /* the command line: k33_language, the last valid one */
  {
   const wchar_t *a[]={L"C1.exe",L"+set",L"k33_language",L"zh"};
   const wchar_t *b[]={L"C1.exe",L"+set",L"k33_language",L"zh",L"+set",L"K33_LANGUAGE",L"English"};
   const wchar_t *c[]={L"C1.exe",L"+set",L"k33_language",L"fr",L"+set",L"k33_language",L"not a name"};
   const wchar_t *d[]={L"C1.exe",L"+set",L"k33_language"};
   const wchar_t *e[]={L"C1.exe",L"+set",L"k33_english",L"0"};
   const wchar_t *f[]={L"C1.exe",L"+set",L"loc_patchLang",L"unsupported"};
   const wchar_t *g[]={L"C1.exe",L"+set",L"k33_english",L"0",L"+set",L"k33_language",L"en"};
   const wchar_t *h[]={L"C1.exe",L"+set",L"k33_english",L"2"};
   const wchar_t *n[]={L"C1.exe"};
   const char cfgZh[]="// generated\nseta 1848 \"0\"\nseta k33_language \"zh\"\nbind W \"+forward\"\n";
   const char cfgTwo[]="seta k33_language \"zh\"\r\nseta k33_language \"fr\"\r\n";
   const char cfgBad[]="seta k33_language \"no;pe\"\nseta k33_languageList \"en;zh\"\n// seta k33_language \"zh\"\nsetak33_language zh\n";
   const char cfgSet[]="  set   k33_language   Chinese\n";
   assert(LanguageFromArgv(4,a,lang) && !strcmp(lang,"zh"));
   assert(LanguageFromArgv(7,b,lang) && !strcmp(lang,"en"));
   assert(LanguageFromArgv(7,c,lang) && !strcmp(lang,"fr"));
   assert(!LanguageFromArgv(3,d,lang));
   assert(!LanguageFromArgv(4,e,lang));
   assert(!PatchLanguageAliasSeen(4,a) && PatchLanguageAliasSeen(4,e) && PatchLanguageAliasSeen(4,f));
   assert(!PatchLanguageAliasSeen(4,h) && !PatchLanguageAliasSeen(1,n));
   assert(LanguageFromConfig(cfgZh,sizeof(cfgZh)-1,lang) && !strcmp(lang,"zh"));
   assert(LanguageFromConfig(cfgTwo,sizeof(cfgTwo)-1,lang) && !strcmp(lang,"fr"));
   assert(!LanguageFromConfig(cfgBad,sizeof(cfgBad)-1,lang));
   assert(LanguageFromConfig(cfgSet,sizeof(cfgSet)-1,lang) && !strcmp(lang,"zh"));
   assert(!LanguageFromConfig("",0,lang));
   /* order: k33_language, the older selectors, the saved choice, English */
   assert(StartupLanguage(4,a,cfgTwo,sizeof(cfgTwo)-1,lang)==LANG_ARGV && !strcmp(lang,"zh"));
   assert(StartupLanguage(7,g,cfgTwo,sizeof(cfgTwo)-1,lang)==LANG_ARGV && !strcmp(lang,"en"));
   assert(StartupLanguage(4,e,cfgTwo,sizeof(cfgTwo)-1,lang)==LANG_ALIAS && !strcmp(lang,"zh"));
   assert(StartupLanguage(4,f,NULL,0,lang)==LANG_ALIAS && !strcmp(lang,"zh"));
   assert(StartupLanguage(4,h,cfgZh,sizeof(cfgZh)-1,lang)==LANG_CONFIG && !strcmp(lang,"zh"));
   assert(StartupLanguage(1,n,cfgZh,sizeof(cfgZh)-1,lang)==LANG_CONFIG && !strcmp(lang,"zh"));
   assert(StartupLanguage(1,n,cfgBad,sizeof(cfgBad)-1,lang)==LANG_DEFAULT && !strcmp(lang,"en"));
   assert(StartupLanguage(1,n,NULL,0,lang)==LANG_DEFAULT && !strcmp(lang,"en"));
  }
  /* the list: sorted, no duplicates, bounded */
  assert(LanguageListAdd(list,&count,"zh")==0);
  assert(LanguageListAdd(list,&count,"en")==0);
  assert(LanguageListAdd(list,&count,"zh")==1);
  assert(LanguageListAdd(list,&count,"fr")==1 && count==3);
  LanguageListJoin(list,count,joined,sizeof(joined));
  assert(!strcmp(joined,"en;fr;zh"));
  assert(LanguageListFind(list,count,"zh")==2 && LanguageListFind(list,count,"de")==-1);
  LanguageListJoin(list,count,joined,6);
  assert(!strcmp(joined,"en;fr"));
  { char name[4]="l0"; for(int i=0;count<LANG_MAX;i++) { name[1]=(char)('0'+i); assert(LanguageListAdd(list,&count,name)>=0); } }
  assert(LanguageListAdd(list,&count,"zz")==-1 && count==LANG_MAX);
 }
 puts("canonical language selection and shared-zone policy passed");
 return 0;
}
