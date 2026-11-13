/* Exact C1 manual discovery timeout renewal from the authenticated lobby's
 * scoped ticker. Normal ticker display, discovery callback, mmbook, Ready,
 * admissions, room fields and all other remote tasks remain native.
 * A stopped heartbeat cannot keep the search alive indefinitely. */
#ifndef CQW_ROOM
#define CQW_ROOM ((uintptr_t)0x082d5470u)
#endif
typedef void (__attribute__((thiscall)) *CqwTickerFn)(void *,uint64_t,const char *,const void *,unsigned);
static CqwTickerFn g_cqwTicker;
#if CODOL_LOCAL_TESTING
/* QA-only passive receipt. No credential, input, Ready or game-state writes. */
__declspec(dllexport) uint32_t CqwTelemetry[16];
__declspec(dllexport) char CqwTelemetryName[64],CqwTelemetryData[80];
#define CQW_REASON(n) (CqwTelemetry[1]=(n))
#define CQW_CAPTURE(field,ptr,n) do {if(Readable(ptr,n))memcpy(field,ptr,n);}while(0)
#else
#define CQW_REASON(n) ((void)0)
#endif

#ifndef CQW_NOW
static uint64_t CqwNow(void)
{
    FILETIME ft; ULARGE_INTEGER t;
    GetSystemTimeAsFileTime(&ft); t.LowPart=ft.dwLowDateTime;t.HighPart=ft.dwHighDateTime;
    return t.QuadPart/10000000u-11644473600ULL;
}
#define CQW_NOW() CqwNow()
#endif
#ifndef CQW_ELAPSED
typedef float (__attribute__((thiscall)) *CqwElapsedFn)(const void *);
#define CQW_ELAPSED(remote) ((CqwElapsedFn)(uintptr_t)0x00a2ce00u)((const BYTE *)(remote)+8)
#endif
#ifndef CQW_TIMEOUT
typedef void (__attribute__((thiscall)) *CqwTimeoutFn)(void *,float);
#define CQW_TIMEOUT(remote,value) ((CqwTimeoutFn)(uintptr_t)0x00a366d0u)(remote,value)
#endif

static int CqwFresh(const BYTE *data,unsigned size,uint64_t *scope)
{
    static const char prefix[]="<MSG P=1 T=";
    if (size<sizeof(prefix)+5 || size>=0x400 || !Readable(data,size) ||
        memcmp(data,prefix,sizeof(prefix)-1)) return 0;
    unsigned i=sizeof(prefix)-1,digits=0; uint64_t expires=0;
    while (i<size && data[i]>='0' && data[i]<='9') {
        if (++digits>12) return 0;
        expires=expires*10+(data[i++]-'0');
    }
    if (!digits || i+3>=size || memcmp(data+i," Q=",3)) return 0;
    i+=3;digits=0;uint64_t room=0;
    while(i<size && data[i]>='0' && data[i]<='9') {
        unsigned digit=data[i++]-'0';
        if(++digits>19 || room>(UINT64_MAX-digit)/10)return 0;
        room=room*10+digit;
    }
    if(!digits || !room || i+3>=size || memcmp(data+i," />",3))return 0;
    uint64_t now=CQW_NOW();
#if CODOL_LOCAL_TESTING
    CqwTelemetry[11]=(uint32_t)now;CqwTelemetry[12]=(uint32_t)expires;
#endif
    *scope=room;
    return expires>now && expires-now<=30;
}

static unsigned CqwRenew(uint64_t sender,const char *name,const void *data,unsigned size)
{
#if CODOL_LOCAL_TESTING
    CqwTelemetry[0]++;CQW_REASON(1);CqwTelemetry[2]++;
    memcpy(CqwTelemetry+3,&sender,8);CqwTelemetry[5]=size;
    CQW_CAPTURE(CqwTelemetryName,name,sizeof(CqwTelemetryName));
    CQW_CAPTURE(CqwTelemetryData,data,size<sizeof(CqwTelemetryData)?size:sizeof(CqwTelemetryData));
#endif
    /* Actual C1 can discard sender/name while retaining the whole MSG body.
     * Bind the freshness-checked lobby metadata to the actual current room. */
    uint64_t scope=0;
    (void)sender;(void)name;
    CQW_REASON(2);
    if (!CqwFresh(data,size,&scope)) return 0;
    CQW_REASON(3);
    BYTE *room=(BYTE *)CQW_ROOM;
    if (!Readable(room,0x4504) || *(const uint32_t *)room!=0x00c7d66cu ||
        !room[0x44f8] || !room[0x4501] || room[0x4503]) return 0;
    uintptr_t record=*(const uint32_t *)(room+0x10);uint64_t rid=0;
    if (!Readable((const void *)record,0x18)) return 0;
    memcpy(&rid,(const void *)(record+0x10),8);
#if CODOL_LOCAL_TESTING
    memcpy(CqwTelemetry+6,&rid,8);
#endif
    CQW_REASON(4);
    if (rid!=scope) return 0;
    CQW_REASON(5);
    unsigned renewed=0;
    for (unsigned i=0;i<32;i++) {
        BYTE *entry=room+0x18+i*0x10;
        if (!entry[8] || *(const uint32_t *)(entry+4)!=3 ||
            *(const uint32_t *)(entry+12)!=0x007143c0u) continue;
        BYTE *remote=(BYTE *)(uintptr_t)*(const uint32_t *)entry;
        if (!Readable(remote,0x3c) || *(const uint32_t *)remote!=0x00bdd530u ||
            *(const uint32_t *)(remote+0x14)!=1) continue;
        float elapsed=CQW_ELAPSED(remote),limit=*(const float *)(remote+0x10);
#if CODOL_LOCAL_TESTING
        CqwTelemetry[8]=(uintptr_t)remote;memcpy(CqwTelemetry+9,&elapsed,4);memcpy(CqwTelemetry+10,&limit,4);
#endif
        CQW_REASON(6);
        /* Both comparisons reject NaN; no stale/finished task can be revived. */
        if (!(elapsed>=0 && elapsed<=604800 && limit>=60 && limit<=604830 && elapsed<limit)) continue;
        /* Backend authority lasts30s and tickers are at most10s apart. Keep
         * 5s for its bounded expiry reply before the native task can time out. */
        if (elapsed+45>limit) {CQW_TIMEOUT(remote,elapsed+45);renewed++;CQW_REASON(7);}
    }
    return renewed;
}

static void __attribute__((fastcall,noinline)) CqwTickerHook(void *self,void *unused,
    uint64_t sender,const char *name,const void *data,unsigned size)
{
    (void)unused;
    CqwRenew(sender,name,data,size);
    g_cqwTicker(self,sender,name,data,size);
}

static void PatchCapacityQueueWait(void)
{
    static const BYTE entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
    static const BYTE setter[]={0x55,0x8b,0xec,0xd9,0x45,0x08,0xd9,0x59,0x10,0x5d,0xc2,0x04,0x00};
    static const BYTE pending[]={0x83,0x7e,0x14,0x01,0x0f,0x85,0x86,0x00,0x00,0x00};
    const void *at=(const void *)(uintptr_t)0x005bb560u;
    if (!Readable(at,sizeof(entry)) || memcmp(at,entry,sizeof(entry)) ||
        !Readable((const void *)(uintptr_t)0x00a366d0u,sizeof(setter)) ||
        memcmp((const void *)(uintptr_t)0x00a366d0u,setter,sizeof(setter)) ||
        !Readable((const void *)(uintptr_t)0x00a36857u,sizeof(pending)) ||
        memcmp((const void *)(uintptr_t)0x00a36857u,pending,sizeof(pending))) {
        Log("capacity queue: exact native guards differ; unchanged");return;
    }
    BYTE *code=VirtualAlloc(NULL,32,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if (!code) return;
    memcpy(code,entry,6);code[6]=0xe9;
    LONG back=(LONG)((uintptr_t)at+6-((uintptr_t)code+11));memcpy(code+7,&back,4);
    DWORD old;
    if (!VirtualProtect(code,32,PAGE_EXECUTE_READ,&old)) {VirtualFree(code,0,MEM_RELEASE);return;}
    FlushInstructionCache(GetCurrentProcess(),code,11);
    BYTE patch[6]={0xe9,0,0,0,0,0x90};
    LONG jump=(LONG)((uintptr_t)CqwTickerHook-((uintptr_t)at+5));memcpy(patch+1,&jump,4);
    if (!VirtualProtect((void *)at,6,PAGE_EXECUTE_READWRITE,&old)) {VirtualFree(code,0,MEM_RELEASE);return;}
    g_cqwTicker=(CqwTickerFn)(void *)code;memcpy((void *)at,patch,6);
    DWORD ignored;
    if (!VirtualProtect((void *)at,6,old,&ignored))Log("capacity queue: entry protection restore failed");
    FlushInstructionCache(GetCurrentProcess(),at,6);
    Log("capacity queue: current manual task follows fresh scoped ticker; native service failures retained");
}
