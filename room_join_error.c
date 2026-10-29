/* Exact C1 channel failure -> existing localized room-full result.
 * No subscription, host acceptance, member, Ready or native phase is created. */
#ifndef CJR_ROOM
#define CJR_ROOM ((uintptr_t)0x082d5470u)
#endif
#ifndef CJR_LEAVE
typedef void (__attribute__((thiscall)) *CjrLeaveFn)(void *);
#define CJR_LEAVE(room) ((CjrLeaveFn)(uintptr_t)0x0070e1f0u)(room)
#endif
typedef void (__attribute__((cdecl)) *CjrCompletionFn)(void *,unsigned);
static CjrCompletionFn g_cjrCompletion;

static void __attribute__((cdecl,noinline)) CjrCompletionHook(void *task,unsigned success)
{
    unsigned char *room=(unsigned char *)CJR_ROOM;
    if (!(success&255) && Readable(task,0x3c) &&
        *(const uint32_t *)task==19 &&
        *(const uint32_t *)((const unsigned char *)task+0x38)==0x00711350u &&
        Readable(room,0x4504) && *(const uint32_t *)room==0x00c7d66cu && !room[0x4503]) {
        uintptr_t record=*(const uint32_t *)((const unsigned char *)task+0x34);
        uintptr_t remote=*(const uint32_t *)((const unsigned char *)task+0x24);
        if (record && record==*(const uint32_t *)(room+0x10) &&
            Readable((const void *)record,0x22a4) &&
            *(const uint32_t *)(record+0x22a0)==CJR_ROOM &&
            Readable((const void *)remote,0x3c) &&
            *(const uint32_t *)(remote+0x38)==1104) {
            /* 0x7230E0 names1104 BD_CHANNEL_FULL. The original711350 loses
             * this distinction and writes reason3 for every failed task.
             * Reason1 uses C1's existing SF_UI_ERROR_ROOM_FULL string. */
            *(uint32_t *)(room+0x44fc)=1;
            CJR_LEAVE(room);
            return;
        }
    }
    g_cjrCompletion(task,success);
}

static void PatchRoomJoinError(void)
{
    static const BYTE entry[]={0x55,0x8b,0xec,0x8b,0x55,0x0c};
    static const BYTE accessor[]={0x8b,0x41,0x38,0xc3};
    static const BYTE failure[]={0xc7,0x81,0xfc,0x44,0,0,3,0,0,0};
    const void *destination=(const void *)(uintptr_t)0x006fd840u;
    if (!Readable(destination,sizeof(entry)) || memcmp(destination,entry,sizeof(entry)) ||
        !Readable((const void *)(uintptr_t)0x00a366f0u,sizeof(accessor)) ||
        memcmp((const void *)(uintptr_t)0x00a366f0u,accessor,sizeof(accessor)) ||
        !Readable((const void *)(uintptr_t)0x00711370u,sizeof(failure)) ||
        memcmp((const void *)(uintptr_t)0x00711370u,failure,sizeof(failure))) {
        Log("room join error: exact native guard differs; unchanged");return;
    }
    BYTE *trampoline=VirtualAlloc(NULL,32,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if (!trampoline) return;
    memcpy(trampoline,entry,sizeof(entry));trampoline[6]=0xe9;
    LONG back=(LONG)((uintptr_t)destination+6-((uintptr_t)trampoline+11));
    memcpy(trampoline+7,&back,4);
    DWORD previous;
    if (!VirtualProtect(trampoline,32,PAGE_EXECUTE_READ,&previous)) {
        VirtualFree(trampoline,0,MEM_RELEASE);return;
    }
    FlushInstructionCache(GetCurrentProcess(),trampoline,11);
    BYTE patch[6]={0xe9,0,0,0,0,0x90};
    LONG jump=(LONG)((uintptr_t)CjrCompletionHook-((uintptr_t)destination+5));
    memcpy(patch+1,&jump,4);
    if (!VirtualProtect((void *)destination,sizeof(patch),PAGE_EXECUTE_READWRITE,&previous)) {
        VirtualFree(trampoline,0,MEM_RELEASE);return;
    }
    g_cjrCompletion=(CjrCompletionFn)(void *)trampoline;
    memcpy((void *)destination,patch,sizeof(patch));
    DWORD ignored;
    if (!VirtualProtect((void *)destination,sizeof(patch),previous,&ignored))
        Log("room join error: entry protection restore failed");
    FlushInstructionCache(GetCurrentProcess(),destination,sizeof(patch));
    Log("room join error: explicit full-channel failures retain localized room-full result");
}
