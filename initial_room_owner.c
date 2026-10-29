/* Bind authenticated initial channel ownership after the real native join.
 * The SDK still writes the owner normally. An initial self-subscription is
 * not a host departure/election; only that exact accepted case bypasses the
 * room's migration callback. Genuine ownership changes use the original path. */
#ifndef CIO_ROOM
#define CIO_ROOM ((uintptr_t)0x082d5470u)
#endif
#ifndef CIO_IDENTITY
static uint64_t CioIdentity(void)
{
    uint64_t uid=0;
    if (Readable((const void *)(uintptr_t)0x00cea9f8u,8))
        memcpy(&uid,(const void *)(uintptr_t)0x00cea9f8u,8);
    if (!uid && Readable((const void *)(uintptr_t)0x06c44f27u,1) &&
        *(const BYTE *)(uintptr_t)0x06c44f27u &&
        Readable((const void *)(uintptr_t)0x06c44f58u,8))
        memcpy(&uid,(const void *)(uintptr_t)0x06c44f58u,8);
    return uid;
}
#define CIO_IDENTITY() CioIdentity()
#endif
static void *g_cioCallback __attribute__((used));

static int __attribute__((cdecl,noinline,used)) CioInitial(void *object,const void *info,const void *member,unsigned joined)
{
    const BYTE *room=(const BYTE *)object;
    if ((uintptr_t)object!=CIO_ROOM || (joined&255)!=1 ||
        !Readable(room,0x15b68) || *(const uint32_t *)room!=0x00c7d66cu ||
        room[0x44f8] || !room[0x4501] || room[0x4503] ||
        *(const uint32_t *)(room+0x44fc) ||
        *(const uint32_t *)(room+0x15b20) || *(const uint32_t *)(room+0x15b24) ||
        !Readable(info,0x20) || !Readable(member,0x10)) return 0;
    uint64_t local=CIO_IDENTITY(),subscriber=0,owner=0,channel=0,current=0,previous=0;
    memcpy(&subscriber,(const BYTE *)member+8,8);
    memcpy(&owner,(const BYTE *)info+0x18,8);
    memcpy(&channel,(const BYTE *)info+8,8);
    uintptr_t record=*(const uint32_t *)(room+0x10);
    if (!local || subscriber!=local || !owner || owner==local || !channel ||
        !Readable((const void *)record,0x28)) return 0;
    const BYTE *cache=(const BYTE *)record;
    memcpy(&current,cache+0x10,8);memcpy(&previous,cache+0x20,8);
    if (!cache[0] || !cache[1] || !cache[2] || current!=channel || previous) return 0;
    unsigned count=*(const uint32_t *)(room+0x15a70),own_count=0,host_count=0;
    if (count<2 || count>18) return 0;
    for (unsigned i=0;i<count;i++) {
        uint64_t uid=0;
        memcpy(&uid,room+0x13f20+i*0x170,8);
        if (!uid) return 0;
        own_count+=uid==local;host_count+=uid==owner;
        for (unsigned j=0;j<i;j++) {
            uint64_t prior=0;memcpy(&prior,room+0x13f20+j*0x170,8);
            if (uid==prior) return 0;
        }
    }
    return own_count==1 && host_count==1;
}

static void __attribute__((naked,used)) CioCallbackHook(void)
{
    __asm__ __volatile__(
        "pushfl\n\tpushal\n\tmovl %esp,%eax\n\t"
        "pushl 48(%eax)\n\tpushl 44(%eax)\n\tpushl 40(%eax)\n\tpushl 4(%eax)\n\t"
        "call _CioInitial\n\taddl $16,%esp\n\ttestl %eax,%eax\n\tjz 1f\n\t"
        "popal\n\tpopfl\n\tret $12\n"
        "1: popal\n\tpopfl\n\tjmp *_g_cioCallback\n");
}

static void PatchInitialRoomOwner(void)
{
    static const BYTE entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
    static const BYTE owner_store[]={0x8b,0x57,0x18,0x89,0x96,0x8c,0xdd,0xff,0xff};
    static const BYTE identity_primary[]={0xa1,0xf8,0xa9,0xce,0x00,0x8b,0x0d,0xfc,0xa9,0xce,0x00};
    static const BYTE identity_fallback[]={0xa0,0x27,0x4f,0xc4,0x06,0x84,0xc0,0x74,0x20,
        0xa1,0x58,0x4f,0xc4,0x06,0x8b,0x0d,0x5c,0x4f,0xc4,0x06};
    const void *at=(const void *)(uintptr_t)0x00713f90u;
    /* SDK invokes this callback before its ordinary owner write at6FD18D. */
    if (!Readable(at,sizeof(entry)) || memcmp(at,entry,sizeof(entry)) ||
        !Readable((const void *)(uintptr_t)0x006fd18du,sizeof(owner_store)) ||
        memcmp((const void *)(uintptr_t)0x006fd18du,owner_store,sizeof(owner_store)) ||
        !Readable((const void *)(uintptr_t)0x00722773u,sizeof(identity_primary)) ||
        memcmp((const void *)(uintptr_t)0x00722773u,identity_primary,sizeof(identity_primary)) ||
        !Readable((const void *)(uintptr_t)0x00722799u,sizeof(identity_fallback)) ||
        memcmp((const void *)(uintptr_t)0x00722799u,identity_fallback,sizeof(identity_fallback))) {
        Log("initial room owner: native guards differ; unchanged");return;
    }
    BYTE *trampoline=VirtualAlloc(NULL,32,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if (!trampoline) return;
    memcpy(trampoline,entry,sizeof(entry));trampoline[6]=0xe9;
    LONG back=(LONG)((uintptr_t)at+6-((uintptr_t)trampoline+11));memcpy(trampoline+7,&back,4);
    DWORD previous;
    if (!VirtualProtect(trampoline,32,PAGE_EXECUTE_READ,&previous)) {VirtualFree(trampoline,0,MEM_RELEASE);return;}
    FlushInstructionCache(GetCurrentProcess(),trampoline,11);
    BYTE patch[6]={0xe9,0,0,0,0,0x90};
    LONG jump=(LONG)((uintptr_t)CioCallbackHook-((uintptr_t)at+5));memcpy(patch+1,&jump,4);
    if (!VirtualProtect((void *)at,sizeof(patch),PAGE_EXECUTE_READWRITE,&previous)) {VirtualFree(trampoline,0,MEM_RELEASE);return;}
    g_cioCallback=trampoline;memcpy((void *)at,patch,sizeof(patch));
    DWORD ignored;
    if (!VirtualProtect((void *)at,sizeof(patch),previous,&ignored))Log("initial room owner: protection restore failed");
    FlushInstructionCache(GetCurrentProcess(),at,sizeof(patch));
    Log("initial room owner: accepted self-subscription initializes owner without a migration");
}
