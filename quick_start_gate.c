/* Exact a5a/d821 native Quick auto-start gate. No Ready, timer, room, dvar or
 * native phase is written by this module. See docs/QUICK_START_GATE.md. */
#include "quick_start_policy.h"
#include "quick_start_guards.h"

#define QSG_ROOM 0x082D5470u
#define QSG_CONTROLLER_SLOT 0x06C44F0Cu
#define QSG_CONTROLLER_VTABLE 0x00C7DE84u
#define QSG_ROOM_VTABLE 0x00C7D66Cu
#define QSG_BRIDGE 0x006AEFB0u
#define QSG_FRAME 0x0071AD60u
#define QSG_COMMIT 0x0071B437u

typedef void (__attribute__((thiscall)) *QsgBridgeFn)(void *, const void *);
typedef void (__attribute__((thiscall)) *QsgFrameFn)(void *);
static QsgBridgeFn g_qsgBridge;
static QsgFrameFn g_qsgFrame;
static QsgPolicy g_qsgPolicy;
static CRITICAL_SECTION g_qsgLock;
static unsigned g_qsgBridgeDepth;

static int QsgString(char *out, unsigned capacity, uint32_t pointer)
{
    const char *s=(const char *)(uintptr_t)pointer;
    for (unsigned i=0;i<capacity;i++) {
        if (!Readable(s+i,1)) return 0;
        out[i]=s[i];
        if (!s[i]) return 1;
    }
    return 0;
}
static QsgSnapshot QsgRead(void *expected)
{
    QsgSnapshot s;
    memset(&s,0,sizeof(s));
    if (!Readable((void *)(uintptr_t)QSG_CONTROLLER_SLOT,4)) return s;
    uint32_t at=*(const uint32_t *)(uintptr_t)QSG_CONTROLLER_SLOT;
    if (!at || (expected && (uintptr_t)expected!=at) ||
        !Readable((void *)(uintptr_t)at,0x100)) return s;
    const unsigned char *controller=(const unsigned char *)(uintptr_t)at;
    if (*(const uint32_t *)controller!=QSG_CONTROLLER_VTABLE ||
        *(const uint32_t *)(controller+0xec)!=1) return s;
    s.identity.controller=at;
    s.phase=*(const uint32_t *)(controller+0x10);
    s.phase_known=1;
    const unsigned char *room=(const unsigned char *)(uintptr_t)QSG_ROOM;
    if (!Readable(room,0x15ac8) || *(const uint32_t *)room!=QSG_ROOM_VTABLE) {
        s.managed=1; return s; /* Unknown managed state cannot bypass the gate. */
    }
    if (!room[0x15aa8]) return s;
    const unsigned char *info=room+0x15a80;
    if (!QsgString(s.identity.mode,sizeof(s.identity.mode),*(const uint32_t *)info)) {
        s.managed=1; return s;
    }
    if (strcmp(s.identity.mode,"playlist") && info[0xc]!=11) return s;
    s.managed=1;
    if (strcmp(s.identity.mode,"playlist") || info[0xc]!=11 ||
        info[0xd]!=2 || info[0xe]!=12 || info[0x28]!=1) return s;
    s.host=room[0x44f8]==1;
    if (!room[0x4501] || !s.host ||
        !QsgString(s.identity.alternate,sizeof(s.identity.alternate),*(const uint32_t *)(info+4)) ||
        !QsgString(s.identity.map,sizeof(s.identity.map),*(const uint32_t *)(info+8))) return s;
    uint32_t record=*(const uint32_t *)(room+0x10);
    if (!record || !Readable((void *)(uintptr_t)record,0x18)) return s;
    memcpy(&s.identity.room,(const void *)(uintptr_t)(record+0x10),8);
    s.identity.controller=at;
    s.identity.count=*(const uint32_t *)(room+0x15a70);
    if (!s.identity.room || s.identity.count<2 || s.identity.count>QSG_MAX_MEMBERS) return s;
    memcpy(s.identity.settings,info+0xc,sizeof(s.identity.settings));
    s.identity.next_entry=info[0x2c]; /* Zero is a valid final voting entry. */
    s.identity.last_entry=info[0x2d];
    s.all_ready=1;
    for (unsigned i=0;i<s.identity.count;i++) {
        const unsigned char *member=room+0x13f20+i*0x170;
        memcpy(&s.identity.members[i],member,8);
        if (!s.identity.members[i]) return s;
        if (member[0x89]!=1) s.all_ready=0;
    }
    for (unsigned i=1;i<s.identity.count;i++) {
        uint64_t uid=s.identity.members[i]; unsigned j=i;
        while (j && s.identity.members[j-1]>uid) {
            s.identity.members[j]=s.identity.members[j-1];j--;
        }
        s.identity.members[j]=uid;
    }
    for (unsigned i=1;i<s.identity.count;i++)
        if (s.identity.members[i]==s.identity.members[i-1]) return s;
    s.valid=1;
    return s;
}
static void QsgTick(void *controller)
{
    QsgSnapshot s=QsgRead(controller);
    EnterCriticalSection(&g_qsgLock);
    QsgObserve(&g_qsgPolicy,&s,GetTickCount());
    LeaveCriticalSection(&g_qsgLock);
}
static void __attribute__((fastcall,noinline)) QsgFrameHook(void *controller,void *unused)
{
    (void)unused;
    QsgTick(controller);
    g_qsgFrame(controller);
    QsgTick(controller);
}
static void __attribute__((fastcall,noinline)) QsgBridgeHook(void *self,void *unused,const void *params)
{
    (void)unused;
    QsgSnapshot s=QsgRead(NULL);
    int granted=0;
    EnterCriticalSection(&g_qsgLock);
    /* Reserve the intent before calling the real bridge, which can synchronously
     * invoke AS again. A duplicate never extends its lifetime or rearms it. */
    if (!g_qsgBridgeDepth) granted=QsgGrant(&g_qsgPolicy,&s,GetTickCount());
    g_qsgBridgeDepth++;
    LeaveCriticalSection(&g_qsgLock);
    if (granted) Log("Quick start gate: genuine intent phase=%lu members=%lu next=%u",
        (DWORD)s.phase,(DWORD)s.identity.count,(unsigned)s.identity.next_entry);
    g_qsgBridge(self,params);
    EnterCriticalSection(&g_qsgLock);
    g_qsgBridgeDepth--;
    LeaveCriticalSection(&g_qsgLock);
}
static int __attribute__((cdecl,noinline,used)) QsgAllow(void *controller)
{
    QsgSnapshot s=QsgRead(controller);
    /* The hook's EBX must be the current controller. An unreadable or replaced
     * object is not proof that this is an unrelated mode. */
    if (!Readable(controller,0xf0) ||
        (*(const uint32_t *)controller==QSG_CONTROLLER_VTABLE &&
         *(const uint32_t *)((const unsigned char *)controller+0xec)==1 &&
         s.identity.controller!=(uint32_t)(uintptr_t)controller)) {
        EnterCriticalSection(&g_qsgLock);
        g_qsgPolicy.state=3;
        LeaveCriticalSection(&g_qsgLock);
        return 0;
    }
    EnterCriticalSection(&g_qsgLock);
    int allow=QsgConsume(&g_qsgPolicy,&s,GetTickCount());
    LeaveCriticalSection(&g_qsgLock);
    if (allow && s.managed) Log("Quick start gate: consuming native phase5 intent once");
    return allow;
}
static void __attribute__((naked,used)) QsgCommitHook(void)
{
    __asm__ __volatile__(
        "pushfl\n\tpushal\n\tpushl %ebx\n\tcall _QsgAllow\n\taddl $4,%esp\n\t"
        "testl %eax,%eax\n\tjz 1f\n\tpopal\n\tpopfl\n\t"
        "movl $6,0x10(%ebx)\n\tpushl $0x71b43e\n\tret\n"
        "1: popal\n\tpopfl\n\tpushl $0x71b457\n\tret\n");
}
static uint32_t QsgHash(const unsigned char *at,unsigned size)
{
    uint32_t h=2166136261u;
    for (unsigned i=0;i<size;i++) h=(h^at[i])*16777619u;
    return h;
}
static void QsgJump(unsigned char *out,uintptr_t from,const void *target,unsigned size)
{
    memset(out,0x90,size);out[0]=0xe9;
    int32_t relative=(int32_t)((uintptr_t)target-(from+5));
    memcpy(out+1,&relative,4);
}
static void PatchQuickStartGate(void)
{
    for (unsigned i=0;i<sizeof(qsg_guards)/sizeof(qsg_guards[0]);i++) {
        const QsgGuard *g=&qsg_guards[i];
        const unsigned char *at=(const unsigned char *)(uintptr_t)g->address;
        if (!Readable(at,g->size) || QsgHash(at,g->size)!=g->fnv1a) {
            Log("Quick start gate: exact native guard differs at %08lX; unchanged",(DWORD)g->address);
            return;
        }
    }
    /* Main game entry has not run yet. Prepare every trampoline/protection
     * before changing any entry; never leave an auto blocker without producer. */
    const uint32_t addresses[]={QSG_BRIDGE,QSG_FRAME,QSG_COMMIT};
    const unsigned sizes[]={5,6,7};
    const void *targets[]={QsgBridgeHook,QsgFrameHook,QsgCommitHook};
    unsigned char original[3][7],patch[3][7];
    DWORD protections[3],ignored;
    BYTE *code=(BYTE *)VirtualAlloc(NULL,64,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if (!code) return;
    for (unsigned i=0;i<3;i++) {
        memcpy(original[i],(void *)(uintptr_t)addresses[i],sizes[i]);
        QsgJump(patch[i],addresses[i],targets[i],sizes[i]);
    }
    memcpy(code,original[0],5);QsgJump(code+5,(uintptr_t)(code+5),(void *)(uintptr_t)(QSG_BRIDGE+5),5);
    memcpy(code+16,original[1],6);QsgJump(code+22,(uintptr_t)(code+22),(void *)(uintptr_t)(QSG_FRAME+6),5);
    if (!VirtualProtect(code,64,PAGE_EXECUTE_READ,&ignored)) {VirtualFree(code,0,MEM_RELEASE);return;}
    if (!FlushInstructionCache(GetCurrentProcess(),code,64)) {VirtualFree(code,0,MEM_RELEASE);return;}
    unsigned prepared=0;
    for (;prepared<3;prepared++)
        if (!VirtualProtect((void *)(uintptr_t)addresses[prepared],sizes[prepared],PAGE_EXECUTE_READWRITE,&protections[prepared])) break;
    if (prepared!=3) {
        while (prepared) {--prepared;VirtualProtect((void *)(uintptr_t)addresses[prepared],sizes[prepared],protections[prepared],&ignored);}
        VirtualFree(code,0,MEM_RELEASE);return;
    }
    InitializeCriticalSection(&g_qsgLock);
    g_qsgBridge=(QsgBridgeFn)(void *)code;
    g_qsgFrame=(QsgFrameFn)(void *)(code+16);
    for (unsigned i=0;i<3;i++) memcpy((void *)(uintptr_t)addresses[i],patch[i],sizes[i]);
    int success=1;
    for (unsigned i=0;i<3;i++) {
        if (!FlushInstructionCache(GetCurrentProcess(),(void *)(uintptr_t)addresses[i],sizes[i])) success=0;
        if (!VirtualProtect((void *)(uintptr_t)addresses[i],sizes[i],protections[i],&ignored)) success=0;
    }
    if (!success) {
        for (unsigned i=0;i<3;i++) {
            if (!VirtualProtect((void *)(uintptr_t)addresses[i],sizes[i],PAGE_EXECUTE_READWRITE,&ignored)) {
                Log("Quick start gate: cannot restore native transaction; terminating before game entry");
                TerminateProcess(GetCurrentProcess(),1);return;
            }
            memcpy((void *)(uintptr_t)addresses[i],original[i],sizes[i]);
            FlushInstructionCache(GetCurrentProcess(),(void *)(uintptr_t)addresses[i],sizes[i]);
            VirtualProtect((void *)(uintptr_t)addresses[i],sizes[i],protections[i],&ignored);
        }
        DeleteCriticalSection(&g_qsgLock);VirtualFree(code,0,MEM_RELEASE);
        Log("Quick start gate: native transaction rolled back; unchanged");return;
    }
    Log("Quick start gate: genuine AS intent controls managed native Quick start; manual countdown retained");
}
