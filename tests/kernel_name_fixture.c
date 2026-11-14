#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
int __cdecl __ms_vsnprintf(char *out,size_t size,const char *format,va_list args)
{return _vsnprintf(out,size,format,args);}
static BOOL K33Sha256Hex(const void *data,DWORD size,const WCHAR *path,char hash[65])
{(void)data;(void)size;(void)path;memcpy(hash,"a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976",65);return 1;}
static int KrcSystemD3D(void){return 1;}
/* Explicit main-thread Theatre counter double; getter body matches theatre.c. */
static unsigned g_thSession;
/* The main-thread Theatre flow owns this counter. Zero and unsigned wrap are
 * valid generations; expose the current value without interpreting it. */
__attribute__((unused)) static unsigned ThSessionGeneration(void)
{
    return g_thSession;
}

#include "../src/kernel/kernel_name_display.c"
#include "../src/kernel/kernel_video_mode.c"
__declspec(dllexport) int VideoLoad(int fallback){return KvLoad(fallback);}
__declspec(dllexport) void VideoSave(int enabled){KvSave(enabled);}
__declspec(dllexport) int Install(void){return InstallKernelNameDisplay();}
__declspec(dllexport) void Tick(void){KernelNameMainTick();}
__declspec(dllexport) void TheatreSession(unsigned value){g_thSession=value;}
__declspec(dllexport) void Seed(const char *canonical,uint64_t uid,const char *alias,uint32_t now)
{KnpIdentityUpdate(&g_kndCache,canonical,uid,ThSessionGeneration(),now);KnpReply(&g_kndCache,&g_kndCache.identity,alias,1,now);}
__declspec(dllexport) const char *Alias(const char *original){return KndSelectAlias(original);}
__declspec(dllexport) const char *Member(const char *original,const void *member){return KndSelectMember(original,member);}
__declspec(dllexport) const char *HUD(const char *original){return KndHUDMessage(original);}
__declspec(dllexport) void Obituary(char *output){KndObituary(output);}
__declspec(dllexport) int Lobby(const char *original,char *out){return KndLobbyLabel(original,out);}
__declspec(dllexport) void *Convert(void *output,const char *text){return KndConvert(output,text);}
__declspec(dllexport) const uint32_t hooks[]={
 (uint32_t)(uintptr_t)KND_Source,(uint32_t)(uintptr_t)KND_Convert,
 (uint32_t)(uintptr_t)KND_Convert,(uint32_t)(uintptr_t)KND_Member,
 (uint32_t)(uintptr_t)KND_Lobby,(uint32_t)(uintptr_t)KND_HUD,
 (uint32_t)(uintptr_t)KND_HUD,(uint32_t)(uintptr_t)KND_Obituary,
 (uint32_t)(uintptr_t)KND_Obituary};
