/* Kernel33 ownership of the retained name-only UI/roster/HUD paths. Install
 * before game entry; no render proxy, developer mapping or thread suspension.
 * This prototype remains opt-in and must pass proxy-free client qualification. */
#include "kernel_name_policy.h"
#include "kernel_name_guards.h"
typedef struct {char alias[64],hud[1024];} KndBuffers;
static SRWLOCK g_kndLock=SRWLOCK_INIT;
static KnpCache g_kndCache;
static KnpIdentity g_kndRequest;
static HANDLE g_kndEvent;
static DWORD g_kndFls=FLS_OUT_OF_INDEXES,g_kndNextSample;
static int g_kndReady,g_kndSampled,g_kndPending,g_kndBusy;
static int KrcSystemD3D(void);

static int KndRead(uint32_t address,void *out,size_t size)
{
    SIZE_T got=0;
    return ReadProcessMemory(GetCurrentProcess(),(const void *)(uintptr_t)address,out,size,&got) && got==size;
}
static int KndCString(const char *text,char *out,size_t capacity)
{
    MEMORY_BASIC_INFORMATION region;
    if(!text || !capacity || !VirtualQuery(text,&region,sizeof(region)) || region.State!=MEM_COMMIT ||
       (region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return 0;
    uint64_t end=(uint64_t)(uintptr_t)region.BaseAddress+region.RegionSize;
    if(end<=(uintptr_t)text)return 0;
    uint64_t available=end-(uintptr_t)text;size_t size=available<capacity?(size_t)available:capacity;
    return KndRead((uint32_t)(uintptr_t)text,out,size) && memchr(out,0,size)!=NULL;
}
static VOID CALLBACK KndFreeBuffers(PVOID value)
{
    /* FLS owns successful allocations until fiber/thread exit or FlsFree.
     * No renderer/cache/logger work may run in this lifetime callback. */
    if(value)HeapFree(GetProcessHeap(),0,value);
}
static KndBuffers *KndThreadBuffers(void)
{
    KndBuffers *buffers;
    if(g_kndFls==FLS_OUT_OF_INDEXES)return NULL;
    buffers=(KndBuffers *)FlsGetValue(g_kndFls);
    if(buffers)return buffers;
    buffers=(KndBuffers *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*buffers));
    if(!buffers)return NULL;
    if(!FlsSetValue(g_kndFls,buffers)){KndFreeBuffers(buffers);return NULL;}
    return buffers;
}
#include "kernel_name_network.c"
#include "kernel_name_render.c"

static void KernelNameMainTick(void)
{
    DWORD now=GetTickCount();uint32_t dvar=0,text=0;char canonical[128]={0};uint64_t uid;
    if(!g_kndReady || (g_kndSampled && (int32_t)(now-g_kndNextSample)<0))return;
    g_kndSampled=1;g_kndNextSample=now+250;uid=KndLocalUID();
    int valid=uid && KndRead(0x6c26318u,&dvar,4) && dvar && KndRead(dvar+12,&text,4) &&
              KndCString((const char *)(uintptr_t)text,canonical,sizeof(canonical)) && canonical[0];
    for(size_t i=0;valid && canonical[i];++i)
        if(i>=63 || (unsigned char)canonical[i]<33 || (unsigned char)canonical[i]>126)valid=0;
    AcquireSRWLockExclusive(&g_kndLock);
    /* Public r44/r45 retired the Theatre-session receiver, so its generation
     * input is always zero. Canonical/UID changes still advance our generation. */
    KnpIdentityUpdate(&g_kndCache,valid?canonical:"",valid?uid:0,0,now);
    if(valid && !g_kndBusy && (uint32_t)(now-g_kndCache.next_request)<0x80000000u) {
        g_kndRequest=g_kndCache.identity;g_kndPending=g_kndBusy=1;g_kndCache.next_request=now+30000;
        if(!SetEvent(g_kndEvent)){g_kndPending=g_kndBusy=0;g_kndCache.next_request=now+1000;}
    }
    ReleaseSRWLockExclusive(&g_kndLock);
}
typedef struct {
    uint32_t address;const BYTE *expected;size_t size;void (*target)(void);BYTE opcode;
    BYTE replacement[7];DWORD protection;
} KndPatch;
static int KndAllGuards(void)
{
    BYTE bytes[32];
    for(size_t i=0;i<sizeof(kndCodeGuards)/sizeof(kndCodeGuards[0]);++i) {
        const KndCodeGuard *guard=&kndCodeGuards[i];
        if(guard->size>sizeof(bytes) || !KndRead(guard->address,bytes,guard->size) || memcmp(bytes,guard->bytes,guard->size))return 0;
    }
    return 1;
}
static int KndRestoreProtections(KndPatch *patches,unsigned count)
{
    int ok=1;DWORD ignored;
    while(count){--count;if(!VirtualProtect((void *)(uintptr_t)patches[count].address,patches[count].size,patches[count].protection,&ignored))ok=0;}
    return ok;
}
static void KndRollback(KndPatch *patches,unsigned count)
{
    DWORD ignored;BYTE actual[7];
    for(unsigned i=0;i<count;++i)
        if(!VirtualProtect((void *)(uintptr_t)patches[i].address,patches[i].size,PAGE_EXECUTE_READWRITE,&ignored))
            TerminateProcess(GetCurrentProcess(),0x4b4e4401u);
    for(unsigned i=0;i<count;++i)memcpy((void *)(uintptr_t)patches[i].address,patches[i].expected,patches[i].size);
    if(!KndRestoreProtections(patches,count) || !FlushInstructionCache(GetCurrentProcess(),NULL,0))
        TerminateProcess(GetCurrentProcess(),0x4b4e4402u);
    for(unsigned i=0;i<count;++i)
        if(!KndRead(patches[i].address,actual,patches[i].size) || memcmp(actual,patches[i].expected,patches[i].size))
            TerminateProcess(GetCurrentProcess(),0x4b4e4403u);
}
static int KndPatchNames(KndPatch *patches,unsigned count)
{
    unsigned protected_count=0;BYTE actual[7];int ok=1;
    for(unsigned i=0;i<count;++i) {
        KndPatch *patch=&patches[i];
        if(patch->size>sizeof(actual) || !KndRead(patch->address,actual,patch->size) || memcmp(actual,patch->expected,patch->size))return 0;
        memset(patch->replacement,0x90,sizeof(patch->replacement));patch->replacement[0]=patch->opcode;
        uint32_t relative=(uint32_t)(uintptr_t)patch->target-patch->address-5;
        memcpy(patch->replacement+1,&relative,4);
    }
    for(unsigned i=0;i<count;++i) {
        if(!VirtualProtect((void *)(uintptr_t)patches[i].address,patches[i].size,PAGE_EXECUTE_READWRITE,&patches[i].protection)){ok=0;break;}
        ++protected_count;
    }
    if(!ok) {
        if(!KndRestoreProtections(patches,protected_count))TerminateProcess(GetCurrentProcess(),0x4b4e4404u);
        return 0;
    }
    for(unsigned i=0;i<count;++i)memcpy((void *)(uintptr_t)patches[i].address,patches[i].replacement,patches[i].size);
    ok=FlushInstructionCache(GetCurrentProcess(),NULL,0)!=FALSE;
    if(!KndRestoreProtections(patches,count))ok=0;
    for(unsigned i=0;ok && i<count;++i)
        if(!KndRead(patches[i].address,actual,patches[i].size) || memcmp(actual,patches[i].replacement,patches[i].size))ok=0;
    if(!ok)KndRollback(patches,count);
    return ok;
}
static int InstallKernelNameDisplay(void)
{
    wchar_t path[MAX_PATH];char hash[65];HANDLE worker;HMODULE owner=NULL;
    DWORD length=GetModuleFileNameW(NULL,path,MAX_PATH);
    KndPatch patches[]={
        {0x5aa780u,knd_SOURCE,sizeof(knd_SOURCE),KND_Source,0xe9,{0},0},
        {0x5aa7cfu,knd_LOCAL,sizeof(knd_LOCAL),KND_Convert,0xe8,{0},0},
        {0x5b4e41u,knd_SCOREBOARD,sizeof(knd_SCOREBOARD),KND_Convert,0xe8,{0},0},
        {0x69598au,knd_MEMBER,sizeof(knd_MEMBER),KND_Member,0xe9,{0},0},
        {0x60ad94u,knd_LOBBY,sizeof(knd_LOBBY),KND_Lobby,0xe9,{0},0},
        {0x469453u,knd_HUD_E,sizeof(knd_HUD_E),KND_HUD,0xe8,{0},0},
        {0x469492u,knd_HUD_F,sizeof(knd_HUD_F),KND_HUD,0xe8,{0},0},
        {0x44477du,knd_VICTIM,sizeof(knd_VICTIM),KND_Obituary,0xe8,{0},0},
        {0x4447ddu,knd_ATTACKER,sizeof(knd_ATTACKER),KND_Obituary,0xe8,{0},0}};
    unsigned count=sizeof(patches)/sizeof(patches[0]);
    if(!length || length>=MAX_PATH || (uintptr_t)GetModuleHandleW(NULL)!=0x400000u || !KrcSystemD3D() ||
       !LauncherBridgeHash(path,hash) || strcmp(hash,"a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976") ||
       !KndAllGuards())return 0;
    if(g_kndFls!=FLS_OUT_OF_INDEXES)return 0;
    /* Hook targets and the FLS destructor require process lifetime. Pin the
     * actual callback's module before publishing any slot or native branch.
     * Do not use DLL_THREAD_DETACH or loader-detach cleanup for heap buffers. */
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                          (LPCWSTR)(uintptr_t)KndFreeBuffers,&owner) || !owner)return 0;
    g_kndFls=FlsAlloc(KndFreeBuffers);if(g_kndFls==FLS_OUT_OF_INDEXES)return 0;
    g_kndEvent=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(!g_kndEvent){FlsFree(g_kndFls);g_kndFls=FLS_OUT_OF_INDEXES;return 0;}
    if(!KndPatchNames(patches,count)){CloseHandle(g_kndEvent);g_kndEvent=NULL;FlsFree(g_kndFls);g_kndFls=FLS_OUT_OF_INDEXES;return 0;}
    worker=CreateThread(NULL,0,KndWorker,NULL,0,NULL);
    if(!worker){KndRollback(patches,count);CloseHandle(g_kndEvent);g_kndEvent=NULL;FlsFree(g_kndFls);g_kndFls=FLS_OUT_OF_INDEXES;return 0;}
    CloseHandle(worker);g_kndReady=1;return 1;
}
