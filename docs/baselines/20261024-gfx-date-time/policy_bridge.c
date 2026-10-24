#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
static unsigned writes;
static BOOL Readable(const void *p,SIZE_T n){return (uintptr_t)p>=0x400000u && (uintptr_t)p+n<=0xA100000u;}
static void Log(const char *format,...){(void)format;}
static void WriteCode(void *p,const void *s,SIZE_T n){memcpy(p,s,n);writes++;}
#include "gfx_date_time.c"
__declspec(dllexport) DWORD WINAPI BridgeDynamic(PDYNAMIC_TIME_ZONE_INFORMATION p){(void)p;return TIME_ZONE_ID_INVALID;}
__declspec(dllexport) BOOL WINAPI BridgeToLocal(const DYNAMIC_TIME_ZONE_INFORMATION *z,const SYSTEMTIME *u,LPSYSTEMTIME l){(void)z;(void)u;(void)l;return FALSE;}
__declspec(dllexport) void BridgeInit(void){g_gdtGetDynamic=BridgeDynamic;g_gdtToLocal=BridgeToLocal;}
__declspec(dllexport) unsigned BridgePatch(void){writes=0;PatchGfxDateTime();return writes;}
__declspec(dllexport) unsigned BridgeWrapper(void){return (unsigned)(uintptr_t)GfxDateSetTime;}
