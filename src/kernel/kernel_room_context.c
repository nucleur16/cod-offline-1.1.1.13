/* Kernel33-owned room context migration. Include after k33sha256.c (K33Sha256Hex)
 * and install after the existing voice/Discord tick wrappers. This module is
 * opt-in until the complete proxy/name-display cutover is qualified. */
#include "kernel_room_policy.h"
#include "kernel_room_guards.h"
#include "kernel_call_guard.c"

static KrpPublisher g_krcPublisher;
static DWORD g_krcDvar;
static unsigned g_krcBuild;
static int g_krcDisabled;
__attribute__((used)) DWORD g_krcNext;

static int KrcRead(void *unused,uint32_t at,void *out,size_t size)
{
    SIZE_T got=0;(void)unused;
    return ReadProcessMemory(GetCurrentProcess(),(const void *)(uintptr_t)at,out,size,&got) && got==size;
}
static int KrcMatch(uint32_t at,const void *bytes,size_t size)
{
    BYTE actual[256];
    return size<=sizeof(actual) && KrcRead(NULL,at,actual,size) && !memcmp(actual,bytes,size);
}
#define KRC_MATCH(at,name) KrcMatch(at,krc_##name,sizeof(krc_##name))
static int KrcInRoom(void *unused,uint32_t object,uint32_t function)
{
    (void)unused;
    return object==0x82d5470u && function==0x70e9f0u && KRC_CallGuarded(object,function,0)!=0;
}
static int KrcAdd(void *unused,const char *command)
{
    (void)unused;
    return KRC_CallGuarded((uint32_t)(uintptr_t)command,0x58ba60u,4)!=0;
}
static int KrcLock(void *unused)
{
    uint32_t enter_slot=0,leave_slot=0,enter=0,leave=0;
    HMODULE kernel=GetModuleHandleW(L"kernel32.dll");(void)unused;
    if(g_krcBuild==1 && KRC_MATCH(0x58ba60u,a5a7CbufLockBody)) {enter_slot=0xb421ecu;leave_slot=0xb421e8u;}
    else if(g_krcBuild==2 && KRC_MATCH(0x58ba60u,d821CbufLockBody)) {enter_slot=0x9a1d2fcu;leave_slot=0x9a1d3d4u;}
    else return 0;
    if(!kernel || !KrcRead(NULL,enter_slot,&enter,4) || !KrcRead(NULL,leave_slot,&leave,4) ||
       enter!=(uint32_t)(uintptr_t)GetProcAddress(kernel,"EnterCriticalSection") ||
       leave!=(uint32_t)(uintptr_t)GetProcAddress(kernel,"LeaveCriticalSection"))return 0;
    return KRC_CallGuarded(0x2b62de0u,(uint32_t)(uintptr_t)TryEnterCriticalSection,1)!=0;
}
static void KrcUnlock(void *unused)
{
    (void)unused;
    if(!KRC_CallGuarded(0x2b62de0u,(uint32_t)(uintptr_t)LeaveCriticalSection,2))g_krcDisabled=1;
}
static KrpSample KrcCurrent(void *unused);
static const KrpAccess g_krcAccess={NULL,KrcRead,KrcInRoom,KrcAdd,KrcLock,KrcUnlock,KrcCurrent};
static KrpSample KrcCurrent(void *unused)
{
    (void)unused;return KrpReadSample(&g_krcAccess);
}
static int KrcAvailable(void *unused)
{
    uint32_t count=0;(void)unused;
    return !g_krcDisabled && g_krcBuild && KrcRead(NULL,0x2ab98c4u,&count,4) && count>0 && count<=65536;
}
static int KrcPublished(void *unused,char *out,size_t capacity)
{
    static const char field[]="codol_quick_native_room_context";
    uint8_t type=0;uint32_t text=0;(void)unused;
    if(!capacity)return 0;
    out[0]=0;
    if(!g_krcDvar)g_krcDvar=(uint32_t)KRC_CallGuarded((uint32_t)(uintptr_t)field,0x6232c0u,3);
    if(!g_krcDvar)return 1;
    if(!KrcRead(NULL,g_krcDvar+8,&type,1) || type!=7 ||
       !KrcRead(NULL,g_krcDvar+12,&text,4) || !text)return 0;
    for(size_t i=0;i<capacity;++i) {
        char c=0;if(!KrcRead(NULL,text+(uint32_t)i,&c,1))return 0;
        if(!c){out[i]=0;return 1;}
        if((unsigned char)c<32 || (unsigned char)c>126)return 0;
        out[i]=c;
    }
    return 0;
}
static int KrcQueue(void *unused,const char *value,int recovery,const KrpSample *expected)
{
    (void)unused;
    return recovery ? KrpRecover(&g_krcAccess,value,*expected) : KrpQueue(&g_krcAccess,value,0);
}
static const KrpBackend g_krcBackend={NULL,KrcAvailable,KrcPublished,KrcCurrent,KrcQueue};
__attribute__((used)) void __cdecl KernelRoomMainTick(void)
{
#if K33_KERNEL_NAME_DISPLAY
    KernelNameMainTick();
#endif
    KrpTick(&g_krcPublisher,GetTickCount(),&g_krcBackend);
}
void KRC_MainTickHook(void);
__asm__(".text\n.globl _KRC_MainTickHook\n_KRC_MainTickHook:\n"
        "pushfl\npushal\ncall _KernelRoomMainTick\npopal\npopfl\njmp *_g_krcNext\n");

static int KrcSystemD3D(void)
{
    wchar_t actual[MAX_PATH],expected[MAX_PATH];
    HMODULE module=GetModuleHandleW(L"d3d9.dll");
    UINT n=GetSystemDirectoryW(expected,MAX_PATH);
    DWORD actual_length=module ? GetModuleFileNameW(module,actual,MAX_PATH) : 0;
    if(!module || !n || n>=MAX_PATH-10 || !actual_length || actual_length>=MAX_PATH)return 0;
    lstrcatW(expected,L"\\d3d9.dll");
    if(!_wcsicmp(expected,actual))return 1;
    /* A 32-bit game on 64-bit Windows may report the redirected SysWOW64
     * module path. Both candidates come from Windows, never the game folder. */
    n=GetSystemWow64DirectoryW(expected,MAX_PATH);
    if(!n || n>=MAX_PATH-10)return 0;
    lstrcatW(expected,L"\\d3d9.dll");return !_wcsicmp(expected,actual);
}
static int InstallKernelRoomContext(void)
{
    wchar_t path[MAX_PATH];char digest[65];BYTE before[5],patch[5]={0xe8,0,0,0,0};
    DWORD old=0,ignored=0;int32_t displacement;uint32_t target;
    void *site=(void *)(uintptr_t)0x595e9du;
    /* No parallel proxy writer, and no install on an approximate build. */
    DWORD path_length=GetModuleFileNameW(NULL,path,MAX_PATH);
    if(!KrcSystemD3D() || (uintptr_t)GetModuleHandleW(NULL)!=0x400000u ||
       !path_length || path_length>=MAX_PATH || !K33Sha256Hex(NULL,0,path,digest))return 0;
    if(!strcmp(digest,"a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976") &&
       KRC_MATCH(0x70e9f0u,a5a7IsInRoom) && KRC_MATCH(0x70e9b0u,a5a7Membership) &&
       KRC_MATCH(0x6232c0u,a5a7DvarFind) && KRC_MATCH(0x58ba60u,a5a7CbufAdd))g_krcBuild=1;
    else if(!strcmp(digest,"d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60") &&
            KRC_MATCH(0x70e9f0u,d821IsInRoom) && KRC_MATCH(0x70e9b0u,d821Membership) &&
            KRC_MATCH(0x6232c0u,d821DvarFind) && KRC_MATCH(0x58ba60u,d821CbufAdd))g_krcBuild=2;
    else return 0;
    if(!KrcRead(NULL,(uint32_t)(uintptr_t)site,before,sizeof(before)) || before[0]!=0xe8)return 0;
    memcpy(&displacement,before+1,4);target=0x595e9du+5+(uint32_t)displacement;
    if(target!=0x595cf0u && target!=(uint32_t)(uintptr_t)VOICE_TickHook &&
       target!=(uint32_t)(uintptr_t)DISCORD_TickHook)return 0;
    displacement=(int32_t)((uintptr_t)KRC_MainTickHook-((uintptr_t)site+5));memcpy(patch+1,&displacement,4);
    if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old))return 0;
    g_krcNext=target;memcpy(site,patch,5);
    if(FlushInstructionCache(GetCurrentProcess(),site,5) && VirtualProtect(site,5,old,&ignored) &&
       !memcmp(site,patch,5))return 1;
    /* Startup is still single-threaded. Never enter C1 with a partial hook. */
    if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&ignored))TerminateProcess(GetCurrentProcess(),0x4b524301u);
    memcpy(site,before,5);
    if(!VirtualProtect(site,5,old,&ignored) || !FlushInstructionCache(GetCurrentProcess(),site,5) ||
       memcmp(site,before,5))TerminateProcess(GetCurrentProcess(),0x4b524302u);
    g_krcBuild=0;g_krcNext=0;return 0;
}
#undef KRC_MATCH
