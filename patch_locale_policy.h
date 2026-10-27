#pragma once
#include <wchar.h>
#include <string.h>
enum CodolPatchLanguage { CODOL_PATCH_EN=0, CODOL_PATCH_ZH=1, CODOL_PATCH_NATIVE=2 };
static int CodolAsciiWideEqual(const wchar_t *a,const wchar_t *b)
{
 while(*a && *b) { wchar_t x=*a++,y=*b++; if(x>=L'A'&&x<=L'Z')x+=32;if(y>=L'A'&&y<=L'Z')y+=32;if(x!=y)return 0; }
 return *a==*b;
}
/* k33_english is the canonical startup-only switch. Keep codol_patchlang for
 * existing launchers. The last valid canonical switch wins over the alias;
 * an absent or malformed canonical value does not erase a valid choice. */
static enum CodolPatchLanguage CodolPatchLanguageFromArgv(int argc,const wchar_t *const *argv)
{
 enum CodolPatchLanguage language=CODOL_PATCH_EN;
 enum CodolPatchLanguage canonical=CODOL_PATCH_EN;
 int canonicalSeen=0;
 int i;
 for(i=1;i<argc;i++) {
  if(!CodolAsciiWideEqual(argv[i],L"+set") || i+1>=argc)continue;
  if(CodolAsciiWideEqual(argv[i+1],L"k33_english")) {
   if(i+2<argc && (CodolAsciiWideEqual(argv[i+2],L"0") || CodolAsciiWideEqual(argv[i+2],L"1"))) {
    canonical=CodolAsciiWideEqual(argv[i+2],L"1") ? CODOL_PATCH_EN : CODOL_PATCH_ZH;
    canonicalSeen=1;
   }
   continue;
  }
  if(!CodolAsciiWideEqual(argv[i+1],L"codol_patchlang"))continue;
  language=CODOL_PATCH_NATIVE;
  if(i+2<argc) {
   if(CodolAsciiWideEqual(argv[i+2],L"en"))language=CODOL_PATCH_EN;
   else if(CodolAsciiWideEqual(argv[i+2],L"zh"))language=CODOL_PATCH_ZH;
  }
 }
 return canonicalSeen ? canonical : language;
}
static int CodolAsciiPrefix(const char *name,const char *prefix)
{
 while(*prefix) { unsigned char a=(unsigned char)*name++,b=(unsigned char)*prefix++;if(a>='A'&&a<='Z')a+=32;if(b>='A'&&b<='Z')b+=32;if(a!=b)return 0; }
 return 1;
}
static int CodolPatchZoneRetired(const char *name)
{
 return strlen(name)==sizeof("codol_zombies_endless")-1 &&
        CodolAsciiPrefix(name,"codol_zombies_endless");
}
/* The patch zones consolidated into one zone per load list and language (build_zones.py, zone_packs/<zone>.csv):
 * {old name, the zone that carries its assets now}. kernel33.c skips an old name while its replacement is in the
 * patch folder, so an install that kept the old files never loads an asset twice (two zones with one
 * stringtable/rawfile replace each other), and an install that has only the old names (codol-classic-zz) still
 * loads them. tests/test_zone_packs.py checks this table against zone_packs/. usermaps_patch's strings went to
 * codol_chinese_locale and its tables to codol_common: it is replaced once codol_common is there. */
static const char *const kCodolPatchZoneMerged[][2]={
 {"000_codol_party_modes","codol_common"},
 {"codol_loading_mode_names","codol_common"},
 {"codol_survival_category","codol_common"},
 {"codol_zm_deathmachine_audio","codol_common"},
 {"controller_ingame","codol_common"},
 {"usermaps_patch","codol_common"},
 {"voice_ui_mp","codol_common_ui_mp"},
 {"usermaps_ui_mp","codol_common_ui_mp"},
 {"codol_field_orders_postgfx_mp","codol_common_postgfx_mp"},
 {"controller_glyphs_postgfx_mp","codol_common_postgfx_mp"},
 {"codol_english","codol_english_locale"},
 {"codol_english_font","codol_english_locale"},
 {"codol_english_ui","codol_english_locale"},
 {"codol_english_ui_extra","codol_english_locale"},
 {"codol_english_usermaps","codol_english_locale"},
 {"codol_english_zombies_base","codol_english_locale"},
 {"codol_chinese_ui","codol_chinese_locale"},
 {"codol_chinese_ui_extra","codol_chinese_locale"},
};
static int CodolAsciiEqual(const char *a,const char *b)
{
 return strlen(a)==strlen(b) && CodolAsciiPrefix(a,b);
}
/* The consolidated zone that replaced `name`, or NULL when `name` is not an old patch zone. */
static const char *CodolPatchZoneMergedInto(const char *name)
{
 unsigned i;
 for(i=0;i<sizeof(kCodolPatchZoneMerged)/sizeof(kCodolPatchZoneMerged[0]);i++)
  if(CodolAsciiEqual(name,kCodolPatchZoneMerged[i][0]))return kCodolPatchZoneMerged[i][1];
 return NULL;
}
/* Shared tables/rules/images/audio and postgfx assets are language-independent.
 * Ignore the retired duplicate table on existing installs in every locale. */
static int CodolPatchZoneLocaleAllowed(const char *name,enum CodolPatchLanguage language)
{
 if(CodolPatchZoneRetired(name))return 0;
 if(CodolAsciiPrefix(name,"codol_english"))return language==CODOL_PATCH_EN;
 /* The UI movies are shared and every text in them is a localize key, so a launch
  * with no valid language (CODOL_PATCH_NATIVE) still needs one set of strings:
  * stock C1 is Chinese, so native loads the Chinese strings. */
 if(CodolAsciiPrefix(name,"codol_chinese"))return language==CODOL_PATCH_ZH || language==CODOL_PATCH_NATIVE;
 return 1;
}
