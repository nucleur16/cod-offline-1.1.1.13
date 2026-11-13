#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
int __cdecl __ms_vsnprintf(char *out,size_t size,const char *format,va_list args)
{return _vsnprintf(out,size,format,args);}
__declspec(dllexport) int fixture_image=1;
static BOOL K33Sha256Hex(const void *data,DWORD size,const WCHAR *path,char out[65])
{
    (void)data;(void)size;(void)path;
    const char *hash=fixture_image==1?"a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976":
                     fixture_image==2?"d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60":
                     "0000000000000000000000000000000000000000000000000000000000000000";
    memcpy(out,hash,65);return 1;
}
void VOICE_TickHook(void){}
void DISCORD_TickHook(void){}
#include "../src/kernel/kernel_room_context.c"
__declspec(dllexport) int Install(void){return InstallKernelRoomContext();}
__declspec(dllexport) void Tick(void){KernelRoomMainTick();}
__declspec(dllexport) unsigned Build(void){return g_krcBuild;}
__declspec(dllexport) unsigned Serial(void){return g_krcPublisher.serial;}
__declspec(dllexport) unsigned Pending(void){return g_krcPublisher.pending;}
__declspec(dllexport) unsigned Next(void){return g_krcNext;}
__declspec(dllexport) const uint32_t hook_address=(uint32_t)(uintptr_t)KRC_MainTickHook;
__declspec(dllexport) const uint32_t voice_address=(uint32_t)(uintptr_t)VOICE_TickHook;
__declspec(dllexport) const uint32_t discord_address=(uint32_t)(uintptr_t)DISCORD_TickHook;
