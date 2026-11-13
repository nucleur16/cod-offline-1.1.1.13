/* C1 d821 live watch: 71B109 publishes both joins-closed flags after the
 * host reaches minPlayers/2; 71B2FC closes again while searching. The
 * authenticated shared allocator owns admission/population for Quick120.
 * Retain genuine host JoinRequest/JoinResponse and every other room mode. */
#include "managed_quick_policy.h"

#define MANAGED_ROOM_INFO_VA 0x0070EA70u
#define MANAGED_MAIN_ROOM_VA 0x082D5470u
#define MANAGED_QUICK_PTR_VA 0x06C44F0Cu
#define MANAGED_QUICK_VTABLE 0x00C7DE84u

typedef void (__attribute__((thiscall)) *ManagedRoomInfoFn)(void *, const void *);
static ManagedRoomInfoFn g_managedRoomInfo;

static void __attribute__((fastcall,noinline)) ManagedRoomInfoHook(void *room, void *unused,
                                                                 const unsigned char *info)
{
    (void)unused;
    DWORD caller = (DWORD)(DWORD_PTR)__builtin_return_address(0);
    DWORD controller = *(const DWORD *)(DWORD_PTR)MANAGED_QUICK_PTR_VA;
    int native = controller && Readable((void *)(DWORD_PTR)controller, 0xf0) &&
        *(const DWORD *)(DWORD_PTR)controller == MANAGED_QUICK_VTABLE;
    unsigned kind = native ? *(const DWORD *)(DWORD_PTR)(controller+0xec) : 0;
    int main = room == (void *)(DWORD_PTR)MANAGED_MAIN_ROOM_VA;
    int host = main && *((const BYTE *)room+0x44f8) == 1;
    const char *mode = NULL;
    if (info && Readable(info,0x48)) memcpy(&mode,info,sizeof(mode));
    int playlist = mode && Readable(mode,9) && memcmp(mode,"playlist",9)==0;
    if (CodolManagedQuickOpen(caller,main,host,native,kind,playlist,info)) {
        unsigned char local[0x48];
        memcpy(local,info,sizeof(local));
        CodolManagedQuickNormalize(local);
        g_managedRoomInfo(room,local);
    } else {
        g_managedRoomInfo(room,info);
    }
}

static void PatchManagedQuickJoin(void)
{
    static const BYTE entry[]={0x55,0x8b,0xec,0x83,0xec,0x10};
    static const BYTE accessor[]={0x8d,0x81,0x80,0x5a,0x01,0x00,0xc3};
    static const BYTE first[]={0x66,0xc7,0x44,0x24,0x41,0x01,0x01};
    static const BYTE second[]={0xc6,0x44,0x24,0x41,0x01};
    const DWORD addresses[]={MANAGED_ROOM_INFO_VA,0x0070EA40u,0x0071B109u,0x0071B2FCu};
    const BYTE *checks[]={entry,accessor,first,second};
    const unsigned sizes[]={sizeof(entry),sizeof(accessor),sizeof(first),sizeof(second)};
    for (unsigned i=0;i<4;i++) {
        const void *at=(const void *)(DWORD_PTR)addresses[i];
        if (!Readable(at,sizes[i]) || memcmp(at,checks[i],sizes[i])) {
            Log("managed Quick: native byte guard differs at 0x%08lX; unchanged",addresses[i]);
            return;
        }
    }
    BYTE *trampoline=(BYTE *)VirtualAlloc(NULL,32,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if (!trampoline) { Log("managed Quick: no trampoline; unchanged");return; }
    memcpy(trampoline,entry,sizeof(entry));
    trampoline[6]=0xe9;
    LONG back=(LONG)(MANAGED_ROOM_INFO_VA+6-((DWORD_PTR)trampoline+11));
    memcpy(trampoline+7,&back,4);
    DWORD previous;
    if (!VirtualProtect(trampoline,32,PAGE_EXECUTE_READ,&previous)) {
        VirtualFree(trampoline,0,MEM_RELEASE);return;
    }
    FlushInstructionCache(GetCurrentProcess(),trampoline,11);
    BYTE patch[6]={0xe9,0,0,0,0,0x90};
    LONG jump=(LONG)((DWORD_PTR)ManagedRoomInfoHook-(MANAGED_ROOM_INFO_VA+5));
    memcpy(patch+1,&jump,4);
    void *destination=(void *)(DWORD_PTR)MANAGED_ROOM_INFO_VA;
    DWORD codeProtection;
    if(!VirtualProtect(destination,sizeof(patch),PAGE_EXECUTE_READWRITE,&codeProtection)) {
        VirtualFree(trampoline,0,MEM_RELEASE);return;
    }
    g_managedRoomInfo=(ManagedRoomInfoFn)(void *)trampoline;
    memcpy(destination,patch,sizeof(patch));
    DWORD ignored;
    if(!VirtualProtect(destination,sizeof(patch),codeProtection,&ignored))
        Log("managed Quick: entry protection restore failed");
    FlushInstructionCache(GetCurrentProcess(),destination,sizeof(patch));
    Log("managed Quick: native search closure stays open for authenticated shared admission");
}
