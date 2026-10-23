#pragma once
#include <wchar.h>
#include <string.h>
enum CodolPatchLanguage { CODOL_PATCH_EN=0, CODOL_PATCH_ZH=1, CODOL_PATCH_NATIVE=2 };
static int CodolAsciiWideEqual(const wchar_t *a,const wchar_t *b)
{
 while(*a && *b) { wchar_t x=*a++,y=*b++; if(x>=L'A'&&x<=L'Z')x+=32;if(y>=L'A'&&y<=L'Z')y+=32;if(x!=y)return 0; }
 return *a==*b;
}
/* Existing launcher sends an explicit language. Missing retains legacy English;
 * malformed/unsupported selects native UI, never an English translation guess. */
static enum CodolPatchLanguage CodolPatchLanguageFromArgv(int argc,const wchar_t *const *argv)
{
 enum CodolPatchLanguage language=CODOL_PATCH_EN;
 int i;
 for(i=1;i<argc;i++) {
  if(!CodolAsciiWideEqual(argv[i],L"+set") || i+1>=argc || !CodolAsciiWideEqual(argv[i+1],L"codol_patchlang"))continue;
  language=CODOL_PATCH_NATIVE;
  if(i+2<argc) {
   if(CodolAsciiWideEqual(argv[i+2],L"en"))language=CODOL_PATCH_EN;
   else if(CodolAsciiWideEqual(argv[i+2],L"zh"))language=CODOL_PATCH_ZH;
  }
 }
 return language;
}
static int CodolAsciiPrefix(const char *name,const char *prefix)
{
 while(*prefix) { unsigned char a=(unsigned char)*name++,b=(unsigned char)*prefix++;if(a>='A'&&a<='Z')a+=32;if(b>='A'&&b<='Z')b+=32;if(a!=b)return 0; }
 return 1;
}
/* Shared tables/rules/images/audio and postgfx assets are language-independent.
 * Only reserved translation/UI overlay names are selected by explicit locale. */
static int CodolPatchZoneLocaleAllowed(const char *name,enum CodolPatchLanguage language)
{
 if(CodolAsciiPrefix(name,"codol_english"))return language==CODOL_PATCH_EN;
 if(CodolAsciiPrefix(name,"codol_chinese"))return language==CODOL_PATCH_ZH;
 return 1;
}
