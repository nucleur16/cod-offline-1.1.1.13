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
 * codol_chinese_locale and its tables to codol_common: it is replaced once codol_common is there.
 * Likewise the old party/loading files retain only their non-localize assets in codol_common; their English
 * text moves to codol_english_locale and Chinese text lives in codol_chinese_locale. Keep both old-name mappings
 * to codol_common: complete new packages must skip retained old 000_* English strings before either locale links.
 * Old-only folders retain the legacy fallback; they are not a qualified bilingual r46 package. */
static const char *const kCodolPatchZoneMerged[][2]={
 {"000_codol_party_modes","codol_common"},
 {"codol_loading_mode_names","codol_common"},
 {"codol_zm_deathmachine_audio","codol_common"},
 {"controller_ingame","codol_common"},
 {"usermaps_patch","codol_common"},
 {"voice_ui_mp","codol_common_ui_mp"},
 {"usermaps_ui_mp","codol_common_ui_mp"},
 {"codol_field_orders_postgfx_mp","codol_common_postgfx_mp"},
 {"controller_glyphs_postgfx_mp","codol_common_postgfx_mp"},
 {"hud_styles_postgfx_mp","codol_common_postgfx_mp"},
 {"survival_hud_postgfx_mp","codol_common_postgfx_mp"},
 {"codol_english","codol_english_locale"},
 {"codol_english_font","codol_english_locale"},
 {"codol_english_ui","codol_english_locale"},
 {"codol_english_ui_extra","codol_english_locale"},
 {"codol_english_usermaps","codol_english_locale"},
 {"codol_english_hud_styles","codol_english_locale"},
 {"codol_english_zombies_base","codol_english_locale"},
 {"codol_chinese_hud_styles","codol_chinese_locale"},
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
 * Ignore the retired duplicate table on existing installs in every locale. (The two-language form; kernel33.c
 * filters by language name, CodolPatchZoneAllowedFor below, which agrees with it for en and zh.) */
#if defined(__GNUC__)
__attribute__((unused))
#endif
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

/* ---- languages by name (k33_language, docs/LANGUAGE_SWITCH_20261112.md) ------------------------------------------
 * A language is a short name: "en" and "zh" are the two this project ships; any other codol_<name>_locale.ff in the
 * patch folder is a language too, named <name>. "english"/"chinese" are accepted spellings of en/zh (they are the
 * zone stems: codol_english_locale, codol_chinese_locale). Everything here is pure (tests/patch_locale_test.c). */
#define CODOL_LANG_NAME_MAX 16
#define CODOL_LANG_MAX 8

/* in -> canonical lower-case name. 0 when in is not a language name ([A-Za-z0-9]{1,15}; "native" is reserved). */
static int CodolLanguageCanonical(const char *in,char out[CODOL_LANG_NAME_MAX])
{
 size_t n=0;
 if(!in)return 0;
 for(;in[n];n++) {
  unsigned char c=(unsigned char)in[n];
  if(n>=CODOL_LANG_NAME_MAX-1)return 0;
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
static int CodolLanguageCanonicalWide(const wchar_t *in,char out[CODOL_LANG_NAME_MAX])
{
 char narrow[CODOL_LANG_NAME_MAX+1];
 size_t n=0;
 if(!in)return 0;
 for(;in[n];n++) {
  if(n>=CODOL_LANG_NAME_MAX || in[n]>0x7f)return 0;
  narrow[n]=(char)in[n];
 }
 narrow[n]=0;
 return CodolLanguageCanonical(narrow,out);
}
/* The zone stem of a canonical language: codol_<stem>_locale. */
static const char *CodolLanguageZoneStem(const char *lang)
{
 if(!strcmp(lang,"en"))return "english";
 if(!strcmp(lang,"zh"))return "chinese";
 return lang;
}
/* "codol_<stem>_locale" -> the language, 0 when name is not a locale zone. */
static int CodolLocaleZoneLanguage(const char *name,char out[CODOL_LANG_NAME_MAX])
{
 char stem[CODOL_LANG_NAME_MAX];
 size_t len=strlen(name),n;
 if(len<=6+7 || !CodolAsciiPrefix(name,"codol_") || !CodolAsciiEqual(name+len-7,"_locale"))return 0;
 n=len-6-7;
 if(n>=sizeof(stem))return 0;
 memcpy(stem,name+6,n);
 stem[n]=0;
 return CodolLanguageCanonical(stem,out);
}
/* The language a patch zone belongs to, 0 when it loads in every language. codol_english* / codol_chinese* are the
 * two languages' zones under every name they ever had; any other language has exactly its codol_<name>_locale. */
static int CodolPatchZoneLanguage(const char *name,char out[CODOL_LANG_NAME_MAX])
{
 if(CodolAsciiPrefix(name,"codol_english")) { strcpy(out,"en"); return 1; }
 if(CodolAsciiPrefix(name,"codol_chinese")) { strcpy(out,"zh"); return 1; }
 return CodolLocaleZoneLanguage(name,out);
}
/* CodolPatchZoneLocaleAllowed for a language name: shared zones always, a language's zones only in that language. */
static int CodolPatchZoneAllowedFor(const char *name,const char *lang)
{
 char owner[CODOL_LANG_NAME_MAX];
 if(CodolPatchZoneRetired(name))return 0;
 if(!CodolPatchZoneLanguage(name,owner))return 1;
 return !strcmp(owner,lang);
}
/* Where the startup language came from. */
enum CodolLanguageSource { CODOL_LANG_DEFAULT=0, CODOL_LANG_ARGV=1, CODOL_LANG_ALIAS=2, CODOL_LANG_CONFIG=3 };
/* The last valid "+set k33_language <name>" on the command line; 0 when there is none. */
static int CodolLanguageFromArgv(int argc,const wchar_t *const *argv,char out[CODOL_LANG_NAME_MAX])
{
 int i,found=0;
 char lang[CODOL_LANG_NAME_MAX];
 for(i=1;i+2<argc;i++) {
  if(!CodolAsciiWideEqual(argv[i],L"+set") || !CodolAsciiWideEqual(argv[i+1],L"k33_language"))continue;
  if(CodolLanguageCanonicalWide(argv[i+2],lang)) { strcpy(out,lang); found=1; }
 }
 return found;
}
/* True when the command line carries one of the older selectors (a valid k33_english, or any codol_patchlang). */
static int CodolPatchLanguageAliasSeen(int argc,const wchar_t *const *argv)
{
 int i;
 for(i=1;i+1<argc;i++) {
  if(!CodolAsciiWideEqual(argv[i],L"+set"))continue;
  if(CodolAsciiWideEqual(argv[i+1],L"codol_patchlang"))return 1;
  if(CodolAsciiWideEqual(argv[i+1],L"k33_english") && i+2<argc &&
     (CodolAsciiWideEqual(argv[i+2],L"0") || CodolAsciiWideEqual(argv[i+2],L"1")))return 1;
 }
 return 0;
}
/* The saved choice: the last  seta k33_language "<name>"  (or set / sets ...) line of a plaintext config_mp.cfg.
 * 0 when the text has no valid line (also for an encrypted config: it has no such text). */
static int CodolLanguageFromConfig(const char *text,size_t len,char out[CODOL_LANG_NAME_MAX])
{
 size_t i=0;
 int found=0;
 while(i<len) {
  size_t eol=i,p=i,n=0;
  char word[CODOL_LANG_NAME_MAX+2],lang[CODOL_LANG_NAME_MAX];
  while(eol<len && text[eol]!='\n' && text[eol]!='\r')eol++;
  while(p<eol && (text[p]==' '||text[p]=='\t'))p++;
  if(eol-p>4 && CodolAsciiPrefix(text+p,"set")) {
   p+=3;
   if(p<eol && (text[p]=='a'||text[p]=='A'||text[p]=='s'||text[p]=='S'||text[p]=='u'||text[p]=='U'))p++;
   if(p<eol && (text[p]==' '||text[p]=='\t')) {
    while(p<eol && (text[p]==' '||text[p]=='\t'))p++;
    if(eol-p>12 && CodolAsciiPrefix(text+p,"k33_language") && (text[p+12]==' '||text[p+12]=='\t')) {
     p+=12;
     while(p<eol && (text[p]==' '||text[p]=='\t'||text[p]=='"'))p++;
     while(p<eol && text[p]!='"' && text[p]!=' ' && text[p]!='\t' && n<sizeof(word)-1)word[n++]=text[p++];
     word[n]=0;
     if(CodolLanguageCanonical(word,lang)) { strcpy(out,lang); found=1; }
    }
   }
  }
  i=eol+1;
 }
 return found;
}
/* The language a client starts in: "+set k33_language <name>", else the older k33_english / codol_patchlang
 * selectors (CodolPatchLanguageFromArgv; an unsupported codol_patchlang means the stock language, zh), else the
 * config's saved k33_language, else English. Returns where it came from. */
static enum CodolLanguageSource CodolStartupLanguage(int argc,const wchar_t *const *argv,const char *config,size_t configLen,
                                                    char out[CODOL_LANG_NAME_MAX])
{
 if(CodolLanguageFromArgv(argc,argv,out))return CODOL_LANG_ARGV;
 if(CodolPatchLanguageAliasSeen(argc,argv)) {
  strcpy(out,CodolPatchLanguageFromArgv(argc,argv)==CODOL_PATCH_EN ? "en" : "zh");
  return CODOL_LANG_ALIAS;
 }
 if(config && CodolLanguageFromConfig(config,configLen,out))return CODOL_LANG_CONFIG;
 strcpy(out,"en");
 return CODOL_LANG_DEFAULT;
}
/* Adds a language to a sorted, duplicate-free list (at most CODOL_LANG_MAX). Returns its index, -1 when full. */
static int CodolLanguageListAdd(char list[CODOL_LANG_MAX][CODOL_LANG_NAME_MAX],int *count,const char *lang)
{
 int i,j;
 for(i=0;i<*count;i++) {
  int c=strcmp(list[i],lang);
  if(!c)return i;
  if(c>0)break;
 }
 if(*count>=CODOL_LANG_MAX)return -1;
 for(j=*count;j>i;j--)strcpy(list[j],list[j-1]);
 strcpy(list[i],lang);
 ++*count;
 return i;
}
static int CodolLanguageListFind(char list[CODOL_LANG_MAX][CODOL_LANG_NAME_MAX],int count,const char *lang)
{
 int i;
 for(i=0;i<count;i++)if(!strcmp(list[i],lang))return i;
 return -1;
}
/* "en;zh" (k33_languageList). */
static void CodolLanguageListJoin(char list[CODOL_LANG_MAX][CODOL_LANG_NAME_MAX],int count,char *out,size_t size)
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
