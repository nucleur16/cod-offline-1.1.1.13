/* Guest binding plus optional local queued-connect continuation. The role environment selects this route;
 * ONLY the retained SDK's verified recipient material supplies scope facts.
 * No Host DLL, CVSP/CPRB/options installer, Ready or forwarding.
 * The opt-in command receipt means QUEUED only, never connected. */
static volatile LONG g_p2pgRoleState,g_p2pgRole;
static int P2PGuestRouteMode(void)
{
    char role[8];DWORD previous=GetLastError(),n,error;LONG value,state;
    SetLastError(ERROR_SUCCESS);n=GetEnvironmentVariableA("CODOL_P2P_STARTUP_ROLE",role,sizeof(role));
    error=GetLastError();SetLastError(previous);
    value=n==0&&error==ERROR_ENVVAR_NOT_FOUND?0:-1; /* empty/error is not absent */
#if CODOL_LOCAL_TESTING
    if(n==5&&!memcmp(role,"guest",5)&&P2PStartupProtocolMode()>0)value=1;
#endif
    state=InterlockedCompareExchange(&g_p2pgRoleState,1,0);
    if(state==0){InterlockedExchange(&g_p2pgRole,value);InterlockedExchange(&g_p2pgRoleState,2);return value;}
    if(state!=2)return -1;
    if(value!=InterlockedCompareExchange(&g_p2pgRole,0,0))InterlockedExchange(&g_p2pgRole,-1);
    return (int)InterlockedCompareExchange(&g_p2pgRole,0,0);
}
#if CODOL_LOCAL_TESTING
#include "p2p_guest_binding_wire.c"
static HANDLE g_p2pgRetire,g_p2pgFailedClose,g_p2pgFailedInheritedClose;
static DWORD g_p2pgRetireFirstError;
static ULONG g_p2pgWireRetire;static unsigned char g_p2pgRetireToken[32];
static BOOL P2PGCaptureRetirement(void)
{
    char value[32],token[65];DWORD n,tn,flags;ULONG number;HANDLE inherited,owned=NULL;unsigned i;
    if(g_p2pgRetire)return WaitForSingleObject(g_p2pgRetire,0)==WAIT_TIMEOUT;
    n=GetEnvironmentVariableA("CODOL_P2P_GUEST_RETIRE_HANDLE",value,sizeof(value));
    tn=GetEnvironmentVariableA("CODOL_P2P_GUEST_RETIRE_TOKEN",token,sizeof(token));
    if(!n||n>=sizeof(value)||tn!=64||!P2PVSPid(value,&number)||number==0xfffffffful)return FALSE;
    for(i=0;i<32;++i) {
        unsigned a=(unsigned char)token[i*2],b=(unsigned char)token[i*2+1];
        a=a>='0'&&a<='9'?a-'0':a>='a'&&a<='f'?a-'a'+10:256;
        b=b>='0'&&b<='9'?b-'0':b>='a'&&b<='f'?b-'a'+10:256;
        if(a>15||b>15)return FALSE;
        g_p2pgRetireToken[i]=(unsigned char)(a*16+b);
    }
    if(!p2pg_nonzero(g_p2pgRetireToken,32))return FALSE;
    inherited=(HANDLE)(ULONG_PTR)number;
    if(inherited==g_p2psParent||!GetHandleInformation(inherited,&flags)||!(flags&HANDLE_FLAG_INHERIT)||
       WaitForSingleObject(inherited,0)!=WAIT_TIMEOUT)return FALSE;
    if(!DuplicateHandle(GetCurrentProcess(),inherited,GetCurrentProcess(),&owned,SYNCHRONIZE,FALSE,0))return FALSE;
    if(!CloseHandle(inherited)) {
        g_p2pgFailedInheritedClose=inherited;g_p2pgRetireFirstError=GetLastError();
        if(!CloseHandle(owned))g_p2pgFailedClose=owned;
        return FALSE;
    }
    g_p2pgRetire=owned;g_p2pgWireRetire=number;
    SetEnvironmentVariableA("CODOL_P2P_GUEST_RETIRE_HANDLE",NULL);SetEnvironmentVariableA("CODOL_P2P_GUEST_RETIRE_TOKEN",NULL);
    return WaitForSingleObject(owned,0)==WAIT_TIMEOUT;
}
static struct {
    P2PStartupBridge io;
    p2pg_view_t view;
    P2PConfig cfg;
    uint64_t local_uid;
    volatile LONG copied,bound;
    volatile LONG connect_pending,connect_spent,connect_done,connect_queued;
    unsigned connect_port;
    unsigned char bound_packet[P2PG_PACKET_SIZE];
} g_p2pg;
static BOOL P2PGCurrent(void)
{
    return P2PGuestRouteMode()==1&&g_p2pgRetire&&P2PSCurrent(&g_p2pg.io);
}
static BOOL P2PGRoomCurrent(void)
{
    const P2PConfig *c=&g_p2pg.cfg;
    return g_p2pEnabled&&g_p2pHasRoom&&P2PLocalUid()==g_p2pg.local_uid&&
        !strcmp(g_p2pRoom.map,c->map)&&!strcmp(g_p2pRoom.session,c->sessionMap)&&
        !strcmp(g_p2pRoom.gametype,c->gametype)&&!strcmp(g_p2pRoom.category,c->zoneType)&&
        g_p2pRoom.difficultyPresent==c->difficultyPresent&&g_p2pRoom.difficulty==c->difficulty;
}
static BOOL P2PGNativeCurrent(void)
{
    ULONGLONG wall;
    return P2PGCurrent()&&P2PSWallMillis(&wall)&&wall<p2pg_u64(g_p2pg.view.packet+88);
}
/* Source-bound named connect registration, not a guessed handler ABI.
 * C1 d821...ba60: secondary registration call0x498BDF ->0x58BD10,
 * node(name,fn) connect ->0x49B0F0. Protected handler body/completion are unproven.
 * We only queue text through existing byte-checked C1CbufAddText. */
static BOOL P2PGConnectCommandExists(void)
{
    static const BYTE registration[]={0xbe,0xb0,0x85,0xc4,0x00,0xe8,0xdd,0x3b,0x0f,0x00,0x85,0xc0,0x75,0x1c,0x89,0x35,0x6c,0xc4,0x2e,0x08,0x89,0x1d,0x70,0xc4,0x2e,0x08,0x89,0x3d,0x68,0xc4,0x2e,0x08,0xc7,0x05,0x24,0x64,0x4f,0x02,0x68,0xc4,0x2e,0x08,0x68,0x5c,0xc4,0x2e,0x08,0x68,0xf0,0xb0,0x49,0x00,0x8b,0xfe,0xe8,0x2c,0x31,0x0f,0x00,0x83,0xc4,0x08,0xbe,0xb8};
    static const BYTE entry[]={0xe9,0xce,0x8e,0xf4,0x07};
    const C1Cmd *node;unsigned i,matches=0;
    if(!g_c1CbufOk||!Readable((void *)(DWORD_PTR)0x00498BA9u,sizeof(registration))||
       memcmp((void *)(DWORD_PTR)0x00498BA9u,registration,sizeof(registration))||
       !Readable((void *)(DWORD_PTR)0x0049B0F0u,sizeof(entry))||
       memcmp((void *)(DWORD_PTR)0x0049B0F0u,entry,sizeof(entry))||
       !Readable((void *)(DWORD_PTR)0x024F639Cu,sizeof(node)))return FALSE;
    node=*(const C1Cmd *volatile *)(DWORD_PTR)0x024F639Cu;
    for(i=0;node&&i<256u;++i) {
        if(!Readable(node,sizeof(*node))||!node->name||!Readable(node->name,8))return FALSE;
        if(!memcmp(node->name,"connect",8)) {
            if(node->fn!=(void *)(DWORD_PTR)0x0049B0F0u)return FALSE;
            ++matches;
        }
        node=node->next;
    }
    return node==NULL&&matches==1;
}
static void P2PGConnectDispatchTick(void)
{
    char line[48];int n;unsigned port;
    if(InterlockedCompareExchange(&g_p2pg.connect_pending,0,0)!=1||
       InterlockedCompareExchange(&g_p2pg.connect_spent,1,0)!=0)return;
    port=g_p2pg.connect_port;
    if(port>=1024u&&port<=65535u&&port!=p2pg_u16(g_p2pg.view.packet+516)&&
       g_p2pg.view.connect_dispatch==1&&InterlockedCompareExchange(&g_p2pg.bound,0,0)==1&&
       P2PGNativeCurrent()&&P2PGRoomCurrent()&&P2PGConnectCommandExists()) {
        n=_snprintf(line,sizeof(line),"connect 127.0.0.1:%u\n",port);
        if(n>0&&n<(int)sizeof(line)&&P2PGNativeCurrent()&&P2PGRoomCurrent()) {
            /* Queued is an irreversible historical side effect, NOT connected.
             * Later retirement cannot remove already-queued C1 input. */
            if(C1CbufAddText(line))InterlockedExchange(&g_p2pg.connect_queued,1);
        }
    }
    InterlockedExchange(&g_p2pg.connect_done,1);
}
static void P2PGuestTick(void)
{
    static int published=-1;int current=0;LONG phase=InterlockedCompareExchange(&g_p2pg.io.phase,0,0);
    if(phase==P2PS_FETCHED&&C1DvarsReady()&&P2PSClaimForFiber(&g_p2pg.io.phase,P2PSAtomicCAS)) {
        /* This is the existing main-thread tick, not a server fiber. Copy only
         * after matching the genuine game's UID/selected attempt and parent.
         * Worker never rewrites/reclaims this one static bounded view. */
        if(g_p2pg.view.valid==1&&P2PGNativeCurrent()&&P2PGRoomCurrent()&&
           p2pg_u64(g_p2pg.view.packet+24)==g_p2pg.local_uid&&
           p2pg_u64(g_p2pg.view.packet+32)!=g_p2pg.local_uid&&
           p2pg_u32(g_p2pg.view.packet+568)==g_p2pgWireRetire&&
           p2pg_same(g_p2pg.view.packet+576,g_p2pgRetireToken,32)) {
            memcpy(g_p2pg.bound_packet,g_p2pg.view.packet,P2PG_PACKET_SIZE);
            if(P2PGNativeCurrent()&&P2PGRoomCurrent()) {
                InterlockedExchange(&g_p2pg.bound,1);InterlockedExchange(&g_p2pg.copied,1);
            }
        }
        if(!InterlockedCompareExchange(&g_p2pg.copied,0,0))InterlockedExchange(&g_p2pg.io.aborted,1);
        InterlockedExchange(&g_p2pg.io.phase,P2PS_FIBER_DONE); /* final view read by tick */
    }
    if(InterlockedCompareExchange(&g_p2pg.bound,0,0)) {
        if(P2PGNativeCurrent()&&P2PGRoomCurrent())current=1;
        else {
            InterlockedExchange(&g_p2pg.io.aborted,1);
            InterlockedExchange(&g_p2pg.bound,0);SecureZeroMemory(g_p2pg.bound_packet,sizeof(g_p2pg.bound_packet));
            P2PStartupInvalidate();P2PSetState(P2P_FAILED,"Guest binding ended. No gameplay route was enabled.");
        }
    }
    /* Never close a handle while the owned worker can still inspect it.
     * Denial is already latched; keep custody until exact thread terminal. */
    if(!current&&g_p2pgRetire&&InterlockedCompareExchange(&g_p2pg.io.aborted,0,0)&&
       g_p2pThread&&WaitForSingleObject(g_p2pThread,0)==WAIT_OBJECT_0) {
        HANDLE owned=g_p2pgRetire;g_p2pgRetire=NULL;g_p2pg.io.retire_event=NULL;
        if(!CloseHandle(owned)){g_p2pgFailedClose=owned;if(!g_p2pgRetireFirstError)g_p2pgRetireFirstError=GetLastError();P2PSetState(P2P_FAILED,"Guest retirement handle close failed.");}
    }
    if(current)P2PGConnectDispatchTick();
    if(published!=current&&C1DvarsReady()) {
        BOOL done=C1QueueSet("ui_p2p_guest_bound",current?"1":"0");
        done=C1QueueSet("ui_p2p_ready","0")&&done; /* never release normal Ready */
        if(done)published=current;
    }
}
static DWORD WINAPI P2PGuestThread(LPVOID ignored)
{
    unsigned char request[P2P_STARTUP_V2_MAX_REQUEST],nonce[32],envelope[P2PG_ENVELOPE_SIZE],ack[P2PG_ACK_SIZE];
    size_t qn=0;DWORD size,name_size;char name[128];P2PStartupBridge *io=&g_p2pg.io;
    const char *failure="Guest binding could not be verified.";BOOL okay=FALSE;ULONGLONG wall,expires,issued;
    (void)ignored;
    if(!g_p2pHasOverride||P2PGuestRouteMode()!=1||!P2PSReserveContext(&io->fetch_claimed,P2PSAtomicCAS)) {
        P2PSetState(P2P_FAILED,"Guest binding attempt is unavailable or spent. Restart the client.");return 1;
    }
    /* One binding attempt per C1 run; no spent-scope replay or implicit renewal.
     * Even a failed context is retained. No buffer used by main tick is freed. */
    g_p2pg.cfg=g_p2pOverride;g_p2pg.local_uid=g_p2pStartupOwner;
    io->pipe=INVALID_HANDLE_VALUE;io->retire_event=NULL;io->packet=NULL;io->packet_size=0;io->expires_ms=0;
    io->phase=io->aborted=0;io->attempt=g_p2pg.cfg.startupGeneration;io->started=GetTickCount();
    io->protocol=P2PStartupProtocolMode();io->scope_started=io->scope_lifetime=0;
    if((io->protocol!=1&&io->protocol!=2)||!g_p2pg.local_uid||!P2PSCaptureParent()||!P2PGCaptureRetirement())goto done;
    io->retire_event=g_p2pgRetire;
    if(!P2PGCurrent()||!P2PSNonce(nonce))goto done;
    if(io->protocol==2) {
        if(!P2PStartupMakeRequestV2(g_p2pg.cfg.sessionMap[0]?g_p2pg.cfg.sessionMap:g_p2pg.cfg.map,
            g_p2pg.cfg.gametype,g_p2pg.cfg.zoneType,g_p2pg.cfg.difficultyPresent,g_p2pg.cfg.difficulty,
            nonce,request,sizeof(request),&qn))goto done;
    } else if(!P2PStartupMakeRequest(g_p2pg.cfg.sessionMap[0]?g_p2pg.cfg.sessionMap:g_p2pg.cfg.map,
            g_p2pg.cfg.gametype,g_p2pg.cfg.zoneType,nonce,request,sizeof(request),&qn))goto done;
    name_size=GetEnvironmentVariableA("CODOL_VERIFIED_SCRIPT_PIPE",name,sizeof(name));
    if(!name_size||name_size>=sizeof(name)||!P2PVSPipeName(name))goto done;
    for(;;) {
        DWORD error;if(!P2PGCurrent())goto done;if(WaitNamedPipeA(name,50))break;
        error=GetLastError();if(error!=ERROR_SEM_TIMEOUT&&error!=ERROR_PIPE_BUSY)goto done;
    }
    io->pipe=CreateFileA(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,NULL);
    if(io->pipe==INVALID_HANDLE_VALUE||!P2PGCurrent())goto done;
    size=(DWORD)qn;
    if(!P2PSTransfer(io,&size,4,TRUE)||!P2PSTransfer(io,request,size,TRUE)||
       !P2PSTransfer(io,&size,4,FALSE)||size!=sizeof(envelope)||
       !P2PSTransfer(io,envelope,sizeof(envelope),FALSE)||
       !P2PGuestBindingParse(request,qn,envelope,sizeof(envelope),P2PSSha256,&g_p2pg.view)||
       g_p2pg.view.protocol!=(unsigned)io->protocol||p2pg_u64(g_p2pg.view.packet+24)!=g_p2pg.local_uid||
       p2pg_u32(g_p2pg.view.packet+568)!=g_p2pgWireRetire||
       !p2pg_same(g_p2pg.view.packet+576,g_p2pgRetireToken,32))goto done;
    issued=p2pg_u64(g_p2pg.view.packet+80);expires=p2pg_u64(g_p2pg.view.packet+88);
    if(!P2PSWallMillis(&wall)||expires<=wall||(issued>wall&&issued-wall>2000u))goto done;
    io->expires_ms=expires;io->scope_started=GetTickCount();
    io->scope_lifetime=expires-wall>P2PVS_TIMEOUT_MS?P2PVS_TIMEOUT_MS:(DWORD)(expires-wall);
    if(!P2PGCurrent())goto done;
    InterlockedExchange(&io->phase,P2PS_FETCHED);
    while(InterlockedCompareExchange(&io->phase,0,0)!=P2PS_FIBER_DONE&&P2PGCurrent())Sleep(5);
    if(InterlockedCompareExchange(&io->phase,0,0)==P2PS_FIBER_DONE&&P2PGCurrent()&&
       InterlockedCompareExchange(&g_p2pg.copied,0,0)==1&&
       P2PGuestBindingAck(&g_p2pg.view,1,ack))okay=P2PSTransfer(io,ack,sizeof(ack),TRUE);
    if(okay&&g_p2pg.view.connect_dispatch==1) {
        unsigned char command[P2PG_DISPATCH_SIZE],queued[P2PG_QUEUED_SIZE];unsigned port=0;
        okay=FALSE;
        if(!P2PGNativeCurrent()||!P2PSTransfer(io,&size,4,FALSE)||size!=sizeof(command)||
           !P2PSTransfer(io,command,sizeof(command),FALSE)||
           !P2PGuestConnectParse(&g_p2pg.view,command,sizeof(command),&port)||!P2PGNativeCurrent())goto done;
        g_p2pg.connect_port=port;
        InterlockedExchange(&g_p2pg.connect_pending,1);
        while(InterlockedCompareExchange(&g_p2pg.connect_done,0,0)!=1&&P2PGCurrent())Sleep(5);
        if(InterlockedCompareExchange(&g_p2pg.connect_done,0,0)==1&&P2PGNativeCurrent()&&P2PGRoomCurrent()&&
           P2PGuestConnectQueued(&g_p2pg.view,port,InterlockedCompareExchange(&g_p2pg.connect_queued,0,0)==1,queued))
            okay=P2PSTransfer(io,queued,sizeof(queued),TRUE);
        SecureZeroMemory(command,sizeof(command));SecureZeroMemory(queued,sizeof(queued));
    }
done:
    P2PSClose(io,!okay); /* closes exact handle/EOF; original OVERLAPPED drains retained */
    if(!okay)P2PSetState(P2P_FAILED,failure);
    SecureZeroMemory(request,sizeof(request));SecureZeroMemory(nonce,sizeof(nonce));
    SecureZeroMemory(envelope,sizeof(envelope));SecureZeroMemory(ack,sizeof(ack));
    return okay?0:1;
}
#else
static void P2PGuestTick(void)
{
    static int denied;
    if(!denied&&C1DvarsReady()){BOOL okay=C1QueueSet("ui_p2p_ready","0");okay=C1QueueSet("ui_p2p_guest_bound","0")&&okay;if(okay)denied=1;}
}
static DWORD WINAPI P2PGuestThread(LPVOID ignored){(void)ignored;return 1;}
#endif
