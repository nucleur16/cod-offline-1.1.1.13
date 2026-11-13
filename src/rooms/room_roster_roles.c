/* C1 member records don't serialize +8b (host). Reconcile that derived role
 * from the SDK's actual owner after a complete roster/subscription update.
 * Neither membership, channel ownership nor migration state is invented. */
#ifndef CMR_ROOM
#define CMR_ROOM ((uintptr_t)0x082d5470u)
#endif
#ifndef CMR_IDENTITY
#define CMR_IDENTITY() CioIdentity()
#endif
static void *g_cmrPrune __attribute__((used))=(void *)(uintptr_t)0x0070c2f0u;
static void *g_cmrCookie __attribute__((used))=(void *)(uintptr_t)0x00ad9f0fu;
#ifndef CMR_ROLE
static void CmrNativeRole(BYTE *member,BYTE *room)
{
    uintptr_t entry=(uintptr_t)member,object=(uintptr_t)room;
    __asm__ __volatile__("call *%2" : "+a"(entry),"+c"(object)
        : "r"((uintptr_t)0x0070ece0u) : "edx","memory","cc");
}
#define CMR_ROLE(member,room) CmrNativeRole(member,room)
#endif

static int __attribute__((cdecl,noinline,used)) CmrRefresh(void *object)
{
    BYTE *room=(BYTE *)object;
    if ((uintptr_t)object!=CMR_ROOM || !Readable(room,0x15b68) ||
        *(const uint32_t *)room!=0x00c7d66cu || !room[0x4501] || room[0x4503] ||
        *(const uint32_t *)(room+0x44fc) ||
        *(const uint32_t *)(room+0x15b20) || *(const uint32_t *)(room+0x15b24))return 0;
    uintptr_t address=*(const uint32_t *)(room+0x10);
    if (!Readable((const void *)address,0x28))return 0;
    const BYTE *cache=(const BYTE *)address;
    uint64_t owner=0,channel=0,local=CMR_IDENTITY();
    memcpy(&channel,cache+0x10,8);memcpy(&owner,cache+0x20,8);
    unsigned count=*(const uint32_t *)(room+0x15a70),subscribers=*(const uint32_t *)(cache+0x18);
    if (!cache[0] || !cache[1] || !cache[2] || !channel || !owner || !local ||
        count<1 || count>18 || subscribers<count || subscribers>18 ||
        !Readable(cache,0x38+subscribers*0x58))return 0;
    unsigned locals=0,owners=0;
    for (unsigned i=0;i<count;i++) {
        uint64_t uid=0;memcpy(&uid,room+0x13f20+i*0x170,8);
        if (!uid)return 0;
        locals+=uid==local;owners+=uid==owner;
        for (unsigned j=0;j<i;j++) {
            uint64_t prior=0;memcpy(&prior,room+0x13f20+j*0x170,8);
            if (uid==prior)return 0;
        }
        unsigned found=0;
        for (unsigned j=0;j<subscribers;j++) {
            uint64_t subscribed=0;memcpy(&subscribed,cache+0x38+j*0x58,8);
            found+=uid==subscribed;
        }
        if (found!=1)return 0;
    }
    if (locals!=1 || owners!=1)return 0;
    for (unsigned i=0;i<count;i++)CMR_ROLE(room+0x13f20+i*0x170,room);
    return 1;
}

static void __attribute__((cdecl,noinline,used)) CmrAfterSdk(const void *info)
{
    BYTE *room=(BYTE *)CMR_ROOM;
    if (!Readable(info,0x20) || !Readable(room,0x14))return;
    uintptr_t address=*(const uint32_t *)(room+0x10);
    if (!Readable((const void *)address,0x28))return;
    uint64_t channel=0,owner=0,stored_channel=0,stored_owner=0;
    memcpy(&channel,(const BYTE *)info+8,8);memcpy(&owner,(const BYTE *)info+0x18,8);
    memcpy(&stored_channel,(const BYTE *)address+0x10,8);memcpy(&stored_owner,(const BYTE *)address+0x20,8);
    if (!channel || !owner || channel!=stored_channel || owner!=stored_owner)return;
    if (CmrRefresh(room)) {
        /* The original prune/notify emits the same UI update used by type14.
         * Every roster UID was just proved to exist once in this SDK cache. */
        ((void (__attribute__((stdcall)) *)(void *))g_cmrPrune)(room);
    }
}

static void __attribute__((naked,used)) CmrMemberListHook(void)
{
    __asm__ __volatile__("pushfl\n\tpushal\n\tpushl 40(%esp)\n\t"
        "call _CmrRefresh\n\taddl $4,%esp\n\tpopal\n\tpopfl\n\tjmp *_g_cmrPrune\n");
}
static void __attribute__((naked,used)) CmrSdkCompleteHook(void)
{
    __asm__ __volatile__("call *_g_cmrCookie\n\tpushfl\n\tpushal\n\tpushl 8(%ebp)\n\t"
        "call _CmrAfterSdk\n\taddl $4,%esp\n\tpopal\n\tpopfl\n\tret\n");
}

static void PatchRoomRosterRoles(void)
{
    static const BYTE role[]={0x53,0x56,0x57,0x8b,0xf8,0x8b,0x17,0x8b,0x41,0x10};
    static const BYTE prune[]={0x55,0x8b,0xec,0x83,0xec,0x0c,0x8b,0x4d,0x08};
    static const uintptr_t sites[]={0x00712cc5u,0x006fd23du,0x006fd275u};
    const uintptr_t targets[]={0x0070c2f0u,0x00ad9f0fu,0x00ad9f0fu};
    const uintptr_t hooks[]={(uintptr_t)CmrMemberListHook,(uintptr_t)CmrSdkCompleteHook,(uintptr_t)CmrSdkCompleteHook};
    if (!Readable((const void *)(uintptr_t)0x0070ece0u,sizeof(role)) ||
        memcmp((const void *)(uintptr_t)0x0070ece0u,role,sizeof(role)) ||
        !Readable((const void *)(uintptr_t)0x0070c2f0u,sizeof(prune)) ||
        memcmp((const void *)(uintptr_t)0x0070c2f0u,prune,sizeof(prune))) {
        Log("room roster roles: native guards differ; unchanged");return;
    }
    for (unsigned i=0;i<3;i++) {
        BYTE expected[5]={0xe8,0,0,0,0};LONG offset=(LONG)(targets[i]-(sites[i]+5));memcpy(expected+1,&offset,4);
        if (!Readable((const void *)sites[i],5) || memcmp((const void *)sites[i],expected,5)) {
            Log("room roster roles: call guard differs; unchanged");return;
        }
    }
    /* Each call is independently valid: failure leaves that original call. */
    for (unsigned i=0;i<3;i++) {
        BYTE patch[5]={0xe8,0,0,0,0};LONG offset=(LONG)(hooks[i]-(sites[i]+5));memcpy(patch+1,&offset,4);
        DWORD previous,ignored;
        if (!VirtualProtect((void *)sites[i],5,PAGE_EXECUTE_READWRITE,&previous))continue;
        memcpy((void *)sites[i],patch,5);
        if (!VirtualProtect((void *)sites[i],5,previous,&ignored))Log("room roster roles: protection restore failed");
        FlushInstructionCache(GetCurrentProcess(),(const void *)sites[i],5);
    }
    Log("room roster roles: reconcile native roles after accepted SDK/roster updates");
}
