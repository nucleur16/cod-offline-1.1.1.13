/* Exact C1 Bot Training: publish native card state before genuine host Ready.
 * Discovery does not carry this choice; the later mmbook commit is too late
 * to choose a dedicated map. Preserve Ready, card, session and roster fields.
 * Opt-in until the paired backend/client has real-client qualification. */
#ifndef CTP_ROOM
#define CTP_ROOM ((uintptr_t)0x082d5470u)
#endif
#ifndef CTP_IDENTITY
#define CTP_IDENTITY() CioIdentity()
#endif
#ifndef CTP_STATE
#define CTP_STATE() (*(const int *)(uintptr_t)0x016fc2e8u)
#endif
#ifndef CTP_SEND
typedef void (__attribute__((thiscall)) *CtpTransportFn)(void *,const void *);
#ifndef CTP_TRANSPORT
#define CTP_TRANSPORT(room,message) ((CtpTransportFn)(uintptr_t)0x0070d8c0u)(room,message)
#endif
static void CtpSend(void *room,const void *info)
{
    uint32_t message[3]={0x00c7d8d8u,15u,(uint32_t)(uintptr_t)info};
    CTP_TRANSPORT(room,message);
}
#define CTP_SEND(room,info) CtpSend(room,info)
#endif
typedef void (__attribute__((thiscall)) *CtpReady)(void *,unsigned);
static CtpReady g_ctpReady;
static int g_ctpBusy;

static int CtpPublish(void *object)
{
    const BYTE *room=(const BYTE *)object;
    if (g_ctpBusy || (uintptr_t)object!=CTP_ROOM || CTP_STATE()!=0 ||
        !Readable(room,0x15b68) || *(const uint32_t *)room!=0x00c7d66cu ||
        !room[0x44f8] || !room[0x4501] || room[0x4503] ||
        *(const uint32_t *)(room+0x44fc) || room[0x15aa8] ||
        *(const uint32_t *)(room+0x15b20) || *(const uint32_t *)(room+0x15b24))return 0;
    const BYTE *info=room+0x15a80;
    const char *mode=*(const char *const *)info;
    if (info[0xc]!=2 || info[0xd]!=1 || info[0x2c]>=9 ||
        !mode || !Readable(mode,9) ||
        (memcmp(mode,"playlist",9) && memcmp(mode,"combat",7)))return 0;
    uintptr_t address=*(const uint32_t *)(room+0x10);
    if (!Readable((const void *)address,0x28))return 0;
    const BYTE *cache=(const BYTE *)address;
    uint64_t channel=0,owner=0,local=CTP_IDENTITY();
    memcpy(&channel,cache+0x10,8);memcpy(&owner,cache+0x20,8);
    if (!cache[0] || !cache[1] || !cache[2] || !channel || !local || owner!=local)return 0;
    g_ctpBusy=1;
    CTP_SEND(object,info);
    g_ctpBusy=0;
    return 1;
}

static void __attribute__((fastcall,noinline)) CtpReadyHook(void *room,void *unused,unsigned ready)
{
    (void)unused;
    if ((ready&255)==1)CtpPublish(room);
    g_ctpReady(room,ready);
}

static void PatchTrainingPreviewPublish(void)
{
    char enabled[8];
    if (GetEnvironmentVariableA("CODOL_TRAINING_PREVIEW_PUBLISH",enabled,sizeof(enabled))!=1 || enabled[0]!='1')return;
    static const BYTE entry[]={0x55,0x8b,0xec,0x83,0xec,0x0c};
    static const BYTE sender[]={0x55,0x8b,0xec,0x8b,0x45,0x08,0x50,0x51,0xb8,0x02,0x00,0x00,0x00};
    void *target=(void *)(uintptr_t)0x0070eb70u;
    if (!Readable(target,sizeof(entry)) || memcmp(target,entry,sizeof(entry)) ||
        !Readable((void *)(uintptr_t)0x0070d8c0u,sizeof(sender)) ||
        memcmp((void *)(uintptr_t)0x0070d8c0u,sender,sizeof(sender))) {
        Log("training preview: native guards differ; unchanged");return;
    }
    BYTE *trampoline=VirtualAlloc(NULL,32,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if (!trampoline)return;
    memcpy(trampoline,entry,sizeof(entry));trampoline[6]=0xe9;
    LONG back=(LONG)((uintptr_t)target+6-((uintptr_t)trampoline+11));memcpy(trampoline+7,&back,4);
    DWORD protection;
    if(!VirtualProtect(trampoline,32,PAGE_EXECUTE_READ,&protection)){VirtualFree(trampoline,0,MEM_RELEASE);return;}
    FlushInstructionCache(GetCurrentProcess(),trampoline,11);
    BYTE patch[6]={0xe9,0,0,0,0,0x90};
    LONG jump=(LONG)((uintptr_t)CtpReadyHook-((uintptr_t)target+5));memcpy(patch+1,&jump,4);
    if(!VirtualProtect(target,sizeof(patch),PAGE_EXECUTE_READWRITE,&protection)){VirtualFree(trampoline,0,MEM_RELEASE);return;}
    g_ctpReady=(CtpReady)(void *)trampoline;
    memcpy(target,patch,sizeof(patch));
    DWORD ignored;
    if(!VirtualProtect(target,sizeof(patch),protection,&ignored))Log("training preview: protection restore failed");
    FlushInstructionCache(GetCurrentProcess(),target,sizeof(patch));
    Log("training preview: native host card publishes before Ready; paired local qualification");
}
