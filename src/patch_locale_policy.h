#pragma once
#include <wchar.h>
#include <string.h>
enum PatchLanguage { PATCH_EN=0, PATCH_ZH=1, PATCH_NATIVE=2 };
static int AsciiWideEqual(const wchar_t *a,const wchar_t *b)
{
 while(*a && *b) { wchar_t x=*a++,y=*b++; if(x>=L'A'&&x<=L'Z')x+=32;if(y>=L'A'&&y<=L'Z')y+=32;if(x!=y)return 0; }
 return *a==*b;
}
/* LEGACY_NAME: the names launchers may still pass from before the IW-style rename (k33_english, k33_language). */
#define LANG_OLD_ENGLISH L"k33_english"     /* LEGACY_NAME */
#define LANG_OLD_LANGUAGE "k33_language"    /* LEGACY_NAME */
#define LANG_OLD_LANGUAGE_W L"k33_language" /* LEGACY_NAME */
static int EnglishSwitchName(const wchar_t *name)
{
 return AsciiWideEqual(name,L"loc_english") || AsciiWideEqual(name,LANG_OLD_ENGLISH);
}
/* loc_english (LEGACY_NAME: or its old spelling k33_english) is the canonical startup-only switch. Keep loc_patchLang for
 * existing launchers. The last valid canonical switch wins over the alias;
 * an absent or malformed canonical value does not erase a valid choice. */
static enum PatchLanguage PatchLanguageFromArgv(int argc,const wchar_t *const *argv)
{
 enum PatchLanguage language=PATCH_EN;
 enum PatchLanguage canonical=PATCH_EN;
 int canonicalSeen=0;
 int i;
 for(i=1;i<argc;i++) {
  if(!AsciiWideEqual(argv[i],L"+set") || i+1>=argc)continue;
  if(EnglishSwitchName(argv[i+1])) {
   if(i+2<argc && (AsciiWideEqual(argv[i+2],L"0") || AsciiWideEqual(argv[i+2],L"1"))) {
    canonical=AsciiWideEqual(argv[i+2],L"1") ? PATCH_EN : PATCH_ZH;
    canonicalSeen=1;
   }
   continue;
  }
  if(!AsciiWideEqual(argv[i+1],L"loc_patchLang"))continue;
  language=PATCH_NATIVE;
  if(i+2<argc) {
   if(AsciiWideEqual(argv[i+2],L"en"))language=PATCH_EN;
   else if(AsciiWideEqual(argv[i+2],L"zh"))language=PATCH_ZH;
  }
 }
 return canonicalSeen ? canonical : language;
}
static int AsciiPrefix(const char *name,const char *prefix)
{
 while(*prefix) { unsigned char a=(unsigned char)*name++,b=(unsigned char)*prefix++;if(a>='A'&&a<='Z')a+=32;if(b>='A'&&b<='Z')b+=32;if(a!=b)return 0; }
 return 1;
}
static int PatchZoneRetired(const char *name)
{
 return strlen(name)==sizeof("codol_zombies_endless")-1 &&
        AsciiPrefix(name,"codol_zombies_endless");
}
/* The patch zones consolidated into one zone per load list and language (build_zones.py, zone_packs/<zone>.csv):
 * {old name, the zone that carries its assets now}. kernel33.c skips an old name while its replacement, or the
 * replacement's own replacement (PatchZoneMergedChain), is in the patch folder, so an install that kept the old files never loads an asset twice (two zones with one
 * stringtable/rawfile replace each other), and an install that has only the old names (codol-classic-zz) still
 * loads them. tests/test_zone_packs.py checks this table against zone_packs/. usermaps_patch's strings went to
 * chinese_locale and its tables to common_extras: it is replaced once common_extras is there.
 * Likewise the old party/loading files retain only their non-localize assets in common_extras; their English
 * text moves to english_locale and Chinese text lives in chinese_locale. Keep both old-name mappings
 * to common_extras: complete new packages must skip retained old 000_* English strings before either locale links.
 * Old-only folders retain the legacy fallback; they are not a qualified bilingual r46 package. */
static const char *const kPatchZoneMerged[][2]={
 /* the zones as installs before the codol name drop (2026-11) carry them; the first column is a file name there */
 {"000_codol_party_modes","codol_common"}, /* LEGACY_ZONE */
 {"codol_loading_mode_names","codol_common"}, /* LEGACY_ZONE */
 {"codol_zm_deathmachine_audio","codol_common"}, /* LEGACY_ZONE */
 {"controller_ingame","codol_common"}, /* LEGACY_ZONE */
 {"usermaps_patch","codol_common"}, /* LEGACY_ZONE */
 {"voice_ui_mp","codol_common_ui_mp"}, /* LEGACY_ZONE */
 {"usermaps_ui_mp","codol_common_ui_mp"}, /* LEGACY_ZONE */
 {"codol_field_orders_postgfx_mp","codol_common_postgfx_mp"}, /* LEGACY_ZONE */
 {"controller_glyphs_postgfx_mp","codol_common_postgfx_mp"}, /* LEGACY_ZONE */
 {"hud_styles_postgfx_mp","codol_common_postgfx_mp"}, /* LEGACY_ZONE */
 {"survival_hud_postgfx_mp","codol_common_postgfx_mp"}, /* LEGACY_ZONE */
 {"codol_english","codol_english_locale"}, /* LEGACY_ZONE */
 {"codol_english_font","codol_common"}, /* LEGACY_ZONE */
 {"codol_english_ui","codol_english_locale"}, /* LEGACY_ZONE */
 {"codol_english_ui_extra","codol_english_locale"}, /* LEGACY_ZONE */
 {"codol_english_usermaps","codol_english_locale"}, /* LEGACY_ZONE */
 {"codol_english_hud_styles","codol_english_locale"}, /* LEGACY_ZONE */
 {"codol_english_zombies_base","codol_english_locale"}, /* LEGACY_ZONE */
 {"codol_chinese_hud_styles","codol_chinese_locale"}, /* LEGACY_ZONE */
 {"codol_chinese_ui","codol_chinese_locale"}, /* LEGACY_ZONE */
 {"codol_chinese_ui_extra","codol_chinese_locale"}, /* LEGACY_ZONE */
 /* the installed zones renamed by that drop: {old file name, new file name} */
 {"codol_common","common_extras"}, /* LEGACY_ZONE */
 {"codol_common_ui_mp","common_ui_mp"}, /* LEGACY_ZONE */
 {"codol_common_postgfx_mp","common_postgfx_mp"}, /* LEGACY_ZONE */
 {"codol_english_locale","english_locale"}, /* LEGACY_ZONE */
 {"codol_chinese_locale","chinese_locale"}, /* LEGACY_ZONE */
 {"codol_infected_cyborg_postgfx_mp","infected_cyborg_postgfx_mp"}, /* LEGACY_ZONE */
 {"codol_zm_m1911_arms_diag","m1911_arms_diag"}, /* LEGACY_ZONE */
 {"mp_codol_weapons","mp_weapons"}, /* LEGACY_ZONE */
 {"mp_codol_weapons_postgfx_mp","mp_weapons_postgfx_mp"}, /* LEGACY_ZONE */
 /* zones that installed under their own name and were packed later: {old file name, the pack that carries it} */
 {"m1911_arms_diag","common_extras"}, /* LEGACY_ZONE */
};
static int AsciiEqual(const char *a,const char *b)
{
 return strlen(a)==strlen(b) && AsciiPrefix(a,b);
}
/* The consolidated zone that replaced `name`, or NULL when `name` is not an old patch zone. */
static const char *PatchZoneMergedInto(const char *name)
{
 unsigned i;
 for(i=0;i<sizeof(kPatchZoneMerged)/sizeof(kPatchZoneMerged[0]);i++)
  if(AsciiEqual(name,kPatchZoneMerged[i][0]))return kPatchZoneMerged[i][1];
 return NULL;
}
/* The zones that replaced `name`, nearest first (an older install's part -> its old pack -> the renamed pack): up to max
 * names in out; the count. kernel33.c skips `name` when any of them is in the patch folder. */
#define PATCH_ZONE_CHAIN_MAX 4
static int PatchZoneMergedChain(const char *name,const char *out[PATCH_ZONE_CHAIN_MAX])
{
 int n=0;
 const char *into=PatchZoneMergedInto(name);
 while(into && n<PATCH_ZONE_CHAIN_MAX) { out[n++]=into; into=PatchZoneMergedInto(into); }
 return n;
}
/* Shared tables/rules/images/audio and postgfx assets are language-independent.
 * Ignore the retired duplicate table on existing installs in every locale. (The two-language form; kernel33.c
 * filters by language name, PatchZoneAllowedFor below, which agrees with it for en and zh.) */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static int PatchZoneLocaleAllowed(const char *name,enum PatchLanguage language)
{
 if(PatchZoneRetired(name))return 0;
 if(AsciiPrefix(name,"english") || AsciiPrefix(name,"codol_english"))return language==PATCH_EN; /* LEGACY_ZONE */
 /* The UI movies are shared and every text in them is a localize key, so a launch
  * with no valid language (PATCH_NATIVE) still needs one set of strings:
  * stock C1 is Chinese, so native loads the Chinese strings. */
 if(AsciiPrefix(name,"chinese") || AsciiPrefix(name,"codol_chinese")) /* LEGACY_ZONE */
  return language==PATCH_ZH || language==PATCH_NATIVE;
 return 1;
}

/* ---- languages by name (loc_languageName, docs/LANGUAGE_SWITCH_20261112.md) ------------------------------------------
 * A language is a short name: "en" and "zh" are the two this project ships; any other <name>_locale.ff in the
 * patch folder is a language too, named <name>. "english"/"chinese" are accepted spellings of en/zh (they are the
 * zone stems: english_locale, chinese_locale). Everything here is pure (tests/patch_locale_test.c). */
#define LANG_NAME_MAX 16
#define LANG_MAX 8

/* in -> canonical lower-case name. 0 when in is not a language name ([A-Za-z0-9]{1,15}; "native" is reserved). */
static int LanguageCanonical(const char *in,char out[LANG_NAME_MAX])
{
 size_t n=0;
 if(!in)return 0;
 for(;in[n];n++) {
  unsigned char c=(unsigned char)in[n];
  if(n>=LANG_NAME_MAX-1)return 0;
  if(c>='A'&&c<='Z')c+=32;
  if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')))return 0;
  out[n]=(char)c;
 }
 if(!n)return 0;
 out[n]=0;
 if(!strcmp(out,"english"))strcpy(out,"en");
 else if(!strcmp(out,"chinese"))strcpy(out,"zh");
 else if(!strcmp(out,"native"))return 0;
 return 1;
}
static int LanguageCanonicalWide(const wchar_t *in,char out[LANG_NAME_MAX])
{
 char narrow[LANG_NAME_MAX+1];
 size_t n=0;
 if(!in)return 0;
 for(;in[n];n++) {
  if(n>=LANG_NAME_MAX || in[n]>0x7f)return 0;
  narrow[n]=(char)in[n];
 }
 narrow[n]=0;
 return LanguageCanonical(narrow,out);
}
/* The zone stem of a canonical language: <stem>_locale. */
static const char *LanguageZoneStem(const char *lang)
{
 if(!strcmp(lang,"en"))return "english";
 if(!strcmp(lang,"zh"))return "chinese";
 return lang;
}
/* "<stem>_locale" -> the language, 0 when name is not a locale zone (the stem is a language name: letters and
 * digits only, so an older install's codol_<name>_locale is not one; PatchZoneLanguage knows it). */
static int LocaleZoneLanguage(const char *name,char out[LANG_NAME_MAX])
{
 char stem[LANG_NAME_MAX];
 size_t len=strlen(name),n;
 if(len<=7 || !AsciiEqual(name+len-7,"_locale"))return 0;
 n=len-7;
 if(n>=sizeof(stem))return 0;
 memcpy(stem,name,n);
 stem[n]=0;
 return LanguageCanonical(stem,out);
}
/* The language a patch zone belongs to, 0 when it loads in every language. english* / chinese* are the
 * two languages' zones under every name they ever had; any other language has exactly its <name>_locale (an older
 * install's <name>_locale too). */
static int PatchZoneLanguage(const char *name,char out[LANG_NAME_MAX])
{
 if(AsciiPrefix(name,"english") || AsciiPrefix(name,"codol_english")) { strcpy(out,"en"); return 1; } /* LEGACY_ZONE */
 if(AsciiPrefix(name,"chinese") || AsciiPrefix(name,"codol_chinese")) { strcpy(out,"zh"); return 1; } /* LEGACY_ZONE */
 if(AsciiPrefix(name,"codol_") && LocaleZoneLanguage(name+6,out))return 1; /* LEGACY_ZONE: codol_<name>_locale */
 return LocaleZoneLanguage(name,out);
}
/* PatchZoneLocaleAllowed for a language name: shared zones always, a language's zones only in that language. */
static int PatchZoneAllowedFor(const char *name,const char *lang)
{
 char owner[LANG_NAME_MAX];
 if(PatchZoneRetired(name))return 0;
 if(!PatchZoneLanguage(name,owner))return 1;
 return !strcmp(owner,lang);
}
/* Where the startup language came from. */
enum LanguageSource { LANG_DEFAULT=0, LANG_ARGV=1, LANG_ALIAS=2, LANG_CONFIG=3 };
/* The last valid "+set loc_languageName <name>" (LEGACY_NAME: or "+set k33_language <name>") on the command line; 0 when
 * there is none. */
static int LanguageFromArgv(int argc,const wchar_t *const *argv,char out[LANG_NAME_MAX])
{
 int i,found=0;
 char lang[LANG_NAME_MAX];
 for(i=1;i+2<argc;i++) {
  if(!AsciiWideEqual(argv[i],L"+set") ||
     (!AsciiWideEqual(argv[i+1],L"loc_languageName") && !AsciiWideEqual(argv[i+1],LANG_OLD_LANGUAGE_W)))continue;
  if(LanguageCanonicalWide(argv[i+2],lang)) { strcpy(out,lang); found=1; }
 }
 return found;
}
/* True when the command line carries one of the older selectors (LEGACY_NAME: a valid loc_english / k33_english, or any
 * loc_patchLang). */
static int PatchLanguageAliasSeen(int argc,const wchar_t *const *argv)
{
 int i;
 for(i=1;i+1<argc;i++) {
  if(!AsciiWideEqual(argv[i],L"+set"))continue;
  if(AsciiWideEqual(argv[i+1],L"loc_patchLang"))return 1;
  if(EnglishSwitchName(argv[i+1]) && i+2<argc &&
     (AsciiWideEqual(argv[i+2],L"0") || AsciiWideEqual(argv[i+2],L"1")))return 1;
 }
 return 0;
}
/* The last  seta <dvar> "<name>"  (or set / sets ...) line of a plaintext config_mp.cfg naming a valid language. */
static int LanguageFromConfigDvar(const char *text,size_t len,const char *dvar,char out[LANG_NAME_MAX])
{
 size_t dl=strlen(dvar);
 size_t i=0;
 int found=0;
 while(i<len) {
  size_t eol=i,p=i,n=0;
  char word[LANG_NAME_MAX+2],lang[LANG_NAME_MAX];
  while(eol<len && text[eol]!='\n' && text[eol]!='\r')eol++;
  while(p<eol && (text[p]==' '||text[p]=='\t'))p++;
  if(eol-p>4 && AsciiPrefix(text+p,"set")) {
   p+=3;
   if(p<eol && (text[p]=='a'||text[p]=='A'||text[p]=='s'||text[p]=='S'||text[p]=='u'||text[p]=='U'))p++;
   if(p<eol && (text[p]==' '||text[p]=='\t')) {
    while(p<eol && (text[p]==' '||text[p]=='\t'))p++;
    if(eol-p>dl && AsciiPrefix(text+p,dvar) && (text[p+dl]==' '||text[p+dl]=='\t')) {
     p+=dl;
     while(p<eol && (text[p]==' '||text[p]=='\t'||text[p]=='"'))p++;
     while(p<eol && text[p]!='"' && text[p]!=' ' && text[p]!='\t' && n<sizeof(word)-1)word[n++]=text[p++];
     word[n]=0;
     if(LanguageCanonical(word,lang)) { strcpy(out,lang); found=1; }
    }
   }
  }
  i=eol+1;
 }
 return found;
}
/* The saved choice: loc_languageName's line, else (LEGACY_NAME: a config saved before the rename) k33_language's.
 * 0 when the text has no valid line (also for an encrypted config: it has no such text). */
static int LanguageFromConfig(const char *text,size_t len,char out[LANG_NAME_MAX])
{
 return LanguageFromConfigDvar(text,len,"loc_languageName",out) || LanguageFromConfigDvar(text,len,LANG_OLD_LANGUAGE,out);
}
/* The language a client starts in: "+set loc_languageName <name>", else the older loc_english / loc_patchLang
 * selectors (PatchLanguageFromArgv; an unsupported loc_patchLang means the stock language, zh), else the
 * config's saved loc_languageName, else English. Returns where it came from. */
static enum LanguageSource StartupLanguage(int argc,const wchar_t *const *argv,const char *config,size_t configLen,
                                                    char out[LANG_NAME_MAX])
{
 if(LanguageFromArgv(argc,argv,out))return LANG_ARGV;
 if(PatchLanguageAliasSeen(argc,argv)) {
  strcpy(out,PatchLanguageFromArgv(argc,argv)==PATCH_EN ? "en" : "zh");
  return LANG_ALIAS;
 }
 if(config && LanguageFromConfig(config,configLen,out))return LANG_CONFIG;
 strcpy(out,"en");
 return LANG_DEFAULT;
}
/* Adds a language to a sorted, duplicate-free list (at most LANG_MAX). Returns its index, -1 when full. */
static int LanguageListAdd(char list[LANG_MAX][LANG_NAME_MAX],int *count,const char *lang)
{
 int i,j;
 for(i=0;i<*count;i++) {
  int c=strcmp(list[i],lang);
  if(!c)return i;
  if(c>0)break;
 }
 if(*count>=LANG_MAX)return -1;
 for(j=*count;j>i;j--)strcpy(list[j],list[j-1]);
 strcpy(list[i],lang);
 ++*count;
 return i;
}
static int LanguageListFind(char list[LANG_MAX][LANG_NAME_MAX],int count,const char *lang)
{
 int i;
 for(i=0;i<count;i++)if(!strcmp(list[i],lang))return i;
 return -1;
}
/* "en;zh" (loc_languageList). */
static void LanguageListJoin(char list[LANG_MAX][LANG_NAME_MAX],int count,char *out,size_t size)
{
 size_t n=0;
 int i;
 if(!size)return;
 for(i=0;i<count;i++) {
  size_t len=strlen(list[i]);
  if(n+len+2>size)break;
  if(i)out[n++]=';';
  memcpy(out+n,list[i],len);
  n+=len;
 }
 out[n]=0;
}
