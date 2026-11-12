/*
 * hostgate.c - the client talks UDP only to hosts the lobby vouches for (feat/p2p-hosting).
 *
 * Hard requirement of docs/P2P_HOSTING.md ("Trust"): the client security audit
 * (docs/SECURITY_AUDIT_CLIENT_20261026.md, branch sec/client-audit-20261026) could not verify C1's
 * gamestate/configstring parser (VMProtect-virtualized CL_ParseGamestate, one of IW4's worst client
 * RCEs), has not audited the DemonWare lobby/party/relay layer, and only reasoned about snapshot bounds
 * and the out-of-band handlers. A player-hosted server is an arbitrary peer, so until those are resolved
 * no datagram from a host the lobby has not vouched for may reach that code.
 *
 * C1 does all of its UDP (DemonWare, STUN, NAT traversal, bdDTLS game traffic) through two import slots:
 * WSOCK32!sendto (0xB42538) and WS2_32!recvfrom (0xB424E0). Both are wrapped here:
 *   - a datagram from an address that is not vouched is dropped before C1 sees it (the call moves on to
 *     the next queued datagram, or reports WSAEWOULDBLOCK as an empty non-blocking socket does);
 *   - a datagram to such an address is not sent (reported as sent, like a lost packet).
 * Vouched (by IPv4 address, any port):
 *   - the lobby: the hosts in dwauth.ini (lines 1 and 3) and stun.ini, as C1 resolves them (the
 *     gethostbyname slot 0xB42530 is wrapped to learn the results) and as resolved here at startup;
 *     the dedicated servers run at the lobby's address (CODOL_MM_HOST_IP is the VPS);
 *   - loopback 127.0.0.0/8: this PC (the offline lobby, a server this client hosts, local test servers);
 *   - UPnP: the SSDP group 239.255.255.250 and this PC's default gateways (the router), for bdUPnP;
 *   - addresses added by HostGateVouch(): stage 2 feeds it from the lobby's signed host tickets
 *     (docs/P2P_HOSTING.md); nothing calls it yet, so a remote player-hosted server is refused.
 * The lobby's TCP connection (connect/send/recv) is not touched.
 *
 * k33_hostGate on the command line: 2 (default) enforce; explicit local-testing builds alone may
 * use 1 (log only). Shipping builds always enforce. Each refused address is logged once to
 * Kernel33.log (the first 64 addresses).
 */

#include <iphlpapi.h>
#include "production_profile.h"

#ifndef HG_SLOT_RECVFROM /* Native fixture supplies its own import table. */
#define HG_SLOT_RECVFROM      0x00B424E0u
#define HG_SLOT_SENDTO        0x00B42538u
#define HG_SLOT_GETHOSTBYNAME 0x00B42530u
#endif
#define HG_MAX                128
#define HG_SSDP               0xFAFFFFEFu /* 239.255.255.250, network order */

typedef int (WINAPI *HgRecvFromFn)(SOCKET, char *, int, int, struct sockaddr *, int *);
typedef int (WINAPI *HgSendToFn)(SOCKET, const char *, int, int, const struct sockaddr *, int);
typedef struct hostent *(WINAPI *HgGetHostByNameFn)(const char *);

static HgRecvFromFn g_hgRecvFrom;
static HgSendToFn g_hgSendTo;
static HgGetHostByNameFn g_hgGetHostByName;
static CRITICAL_SECTION g_hgLock;
static DWORD g_hgAllowed[HG_MAX];      /* network-order IPv4 */
static int g_hgAllowedCount;
#if CODOL_DIAGNOSTICS
static DWORD g_hgRefused[64];
static int g_hgRefusedCount;
#endif
static char g_hgNames[24][128];        /* lobby host names from dwauth.ini / stun.ini */
static int g_hgNameCount;
static int g_hgMode = 2;
#if CODOL_DIAGNOSTICS
static volatile LONG g_hgDropsIn, g_hgDropsOut;
#endif

static void HostGateAllow(DWORD ip, const char *why)
{
    if (!ip || ip == INADDR_NONE) return;
    EnterCriticalSection(&g_hgLock);
    for (int i = 0; i < g_hgAllowedCount; i++)
        if (g_hgAllowed[i] == ip) { LeaveCriticalSection(&g_hgLock); return; }
    if (g_hgAllowedCount < HG_MAX) g_hgAllowed[g_hgAllowedCount++] = ip;
    LeaveCriticalSection(&g_hgLock);
    struct in_addr a;
    a.s_addr = ip;
    Log("hostgate: vouched %s (%s)", inet_ntoa(a), why);
}

/* Stage 2's entry point: an address the lobby vouched for (a verified host ticket). */
__attribute__((unused)) static void HostGateVouch(DWORD ip) { HostGateAllow(ip, "lobby host ticket"); }

static BOOL HostGateOk(DWORD ip)
{
    if ((ntohl(ip) >> 24) == 127 || ip == HG_SSDP) return TRUE;
    BOOL ok = FALSE;
    EnterCriticalSection(&g_hgLock);
    for (int i = 0; i < g_hgAllowedCount && !ok; i++) ok = g_hgAllowed[i] == ip;
    LeaveCriticalSection(&g_hgLock);
    return ok;
}

#if CODOL_DIAGNOSTICS
static void HostGateRefused(DWORD ip, int out, int port)
{
    InterlockedIncrement(out ? &g_hgDropsOut : &g_hgDropsIn);
    EnterCriticalSection(&g_hgLock);
    BOOL seen = FALSE;
    for (int i = 0; i < g_hgRefusedCount && !seen; i++) seen = g_hgRefused[i] == ip;
    if (!seen && g_hgRefusedCount < 64) g_hgRefused[g_hgRefusedCount++] = ip;
    LeaveCriticalSection(&g_hgLock);
    if (seen || g_hgRefusedCount >= 64) return;
    struct in_addr a;
    a.s_addr = ip;
    Log("hostgate: %s %s %s:%d - not a lobby-vouched host", g_hgMode >= 2 ? "refused" : "WOULD refuse",
        out ? "datagram to" : "datagram from", inet_ntoa(a), port);
}
#else
#define HostGateRefused(...) ((void)0)
#endif

static int WINAPI HostGateRecvFrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
    /* Yield even if a hostile sender keeps the receive queue permanently full. */
    for (int dropped = 0; dropped < 64; ++dropped) {
        struct sockaddr_storage own;
        int ownLen = sizeof(own);
        struct sockaddr *f = from ? from : (struct sockaddr *)&own;
        int *fl = from ? fromlen : &ownLen;
        int r = g_hgRecvFrom(s, buf, len, flags, f, fl);
        if (r == SOCKET_ERROR || !fl || *fl < (int)sizeof(struct sockaddr_in) || f->sa_family != AF_INET) return r;
        const struct sockaddr_in *in = (const struct sockaddr_in *)f;
        int production=P2PProductionGuestAddress(in->sin_addr.s_addr,ntohs(in->sin_port));
        if(production>0)return r;
        if(production==0&&HostGateOk(in->sin_addr.s_addr)) return r;
        HostGateRefused(in->sin_addr.s_addr, 0, ntohs(in->sin_port));
        if (production == 0 && g_hgMode < 2) return r;
        if (flags & MSG_PEEK) { /* drop it for real, then look again */
            char sink[1];
            int sl = sizeof(own);
            g_hgRecvFrom(s, sink, sizeof(sink), 0, (struct sockaddr *)&own, &sl);
        }
        /* Next datagram, without blocking: a blocking socket would otherwise wait for one here. */
        u_long pending = 0;
        if (ioctlsocket(s, FIONREAD, &pending) != 0 || !pending) {
            fd_set rd;
            FD_ZERO(&rd);
            FD_SET(s, &rd);
            struct timeval tv = { 0, 0 };
            if (select(0, &rd, NULL, NULL, &tv) <= 0) { WSASetLastError(WSAEWOULDBLOCK); return SOCKET_ERROR; }
        }
    }
    WSASetLastError(WSAEWOULDBLOCK);
    return SOCKET_ERROR;
}

static int WINAPI HostGateSendTo(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
    P2PProductionBorrowSend(s);
    if (to && tolen >= (int)sizeof(struct sockaddr_in) && to->sa_family == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)to;
        /* The armed Local Play host starts when C1 first talks to its loopback port (p2p.c,
         * docs/P2P_DEFERRED_HOST_START.md). Observation only: the datagram is sent as before. */
        if ((ntohl(in->sin_addr.s_addr) >> 24) == 127) P2PLaunchObserve(ntohs(in->sin_port));
        int production=P2PProductionGuestAddress(in->sin_addr.s_addr,ntohs(in->sin_port));
        if (production<0 || (production==0&&!HostGateOk(in->sin_addr.s_addr))) {
            HostGateRefused(in->sin_addr.s_addr, 1, ntohs(in->sin_port));
            if (production < 0 || g_hgMode >= 2) return len;
        }
    }
    return g_hgSendTo(s, buf, len, flags, to, tolen);
}

static BOOL HostGateIsLobbyName(const char *name)
{
    if (!name) return FALSE;
    for (int i = 0; i < g_hgNameCount; i++) if (!_stricmp(g_hgNames[i], name)) return TRUE;
    return FALSE;
}

static void HostGateAllowHostent(const struct hostent *h, const char *why)
{
    if (!h || h->h_addrtype != AF_INET || h->h_length != 4 || !h->h_addr_list) return;
    for (int i = 0; h->h_addr_list[i] && i < 16; i++) {
        DWORD ip;
        memcpy(&ip, h->h_addr_list[i], 4);
        HostGateAllow(ip, why);
    }
}

static struct hostent *WINAPI HostGateGetHostByName(const char *name)
{
    struct hostent *h = g_hgGetHostByName(name);
    if (h && HostGateIsLobbyName(name)) HostGateAllowHostent(h, name);
    return h;
}

/* dwauth.ini lines 1 and 3, every stun.ini line: names or dotted addresses. */
static void HostGateReadLobbyNames(void)
{
    static const char *const files[] = { "dwauth.ini", "stun.ini" };
    char root[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, root, MAX_PATH);
    while (n && root[n - 1] != '\\') n--;
    root[n] = 0;
    for (int fi = 0; fi < 2; fi++) {
#if defined(CODOL_LOCAL_TESTING) && CODOL_LOCAL_TESTING == 1
        if(fi==0&&LocalQaRouteState()!=1){
            char data[257];DWORD size=0;
            if(LocalQaReadRoute(data,256,&size)&&CodolLocalMultiRoute(data,size)){
                data[size]=0;char* end=strchr(data,'\n');if(end)*end=0;
                P2PTrim(data);DWORD ip=inet_addr(data);if(ip!=INADDR_NONE)HostGateAllow(ip,"dwauth.ini");
            }
            continue; // malformed explicit selection never reads the shared route
        }
#endif
        char path[MAX_PATH + 16];
        _snprintf(path, sizeof(path), "%s%s", root, files[fi]);
        path[sizeof(path) - 1] = 0;
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        char line[160];
        for (int ln = 0; fgets(line, sizeof(line), f) && ln < 16; ln++) {
            if (fi == 0 && ln != 0 && ln != 2) continue;
            P2PTrim(line);
            if (!line[0] || g_hgNameCount >= (int)(sizeof(g_hgNames) / sizeof(g_hgNames[0]))) continue;
            DWORD ip = inet_addr(line);
            if (ip != INADDR_NONE) { HostGateAllow(ip, files[fi]); continue; }
            if (!HostGateIsLobbyName(line)) lstrcpynA(g_hgNames[g_hgNameCount++], line, sizeof(g_hgNames[0]));
        }
        fclose(f);
    }
}

/* Resolve the lobby names once in the background too, so the gate does not depend on C1 resolving
 * them through gethostbyname (it may use getaddrinfo for some). */
static DWORD WINAPI HostGateResolveThread(LPVOID unused)
{
    (void)unused;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa)) return 1;
    for (int i = 0; i < g_hgNameCount; i++) {
        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(g_hgNames[i], NULL, &hints, &res) == 0) {
            for (struct addrinfo *r = res; r; r = r->ai_next)
                if (r->ai_family == AF_INET) HostGateAllow(((struct sockaddr_in *)r->ai_addr)->sin_addr.s_addr, g_hgNames[i]);
            freeaddrinfo(res);
        } else {
            Log("hostgate: could not resolve lobby host %s (C1's own lookup still vouches it)", g_hgNames[i]);
        }
    }
    WSACleanup();
    return 0;
}

/* The default gateways: DemonWare's UPnP (bdUPnP) asks the router to map the game port; the SSDP search goes to
 * 239.255.255.250:1900 and the router answers from its LAN address. Without them UPnP discovery times out
 * (about a minute before the DW login starts) and stage 2's port mapping cannot work. */
static void HostGateAllowGateways(void)
{
    ULONG size = 16384;
    IP_ADAPTER_ADDRESSES *list = (IP_ADAPTER_ADDRESSES *)malloc(size);
    if (!list) return;
    ULONG r = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                                   GAA_FLAG_SKIP_DNS_SERVER, NULL, list, &size);
    if (r == ERROR_BUFFER_OVERFLOW) {
        free(list);
        list = (IP_ADAPTER_ADDRESSES *)malloc(size);
        if (!list) return;
        r = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                                 GAA_FLAG_SKIP_DNS_SERVER, NULL, list, &size);
    }
    if (r == NO_ERROR)
        for (IP_ADAPTER_ADDRESSES *a = list; a; a = a->Next)
            for (IP_ADAPTER_GATEWAY_ADDRESS_LH *g = a->FirstGatewayAddress; g; g = g->Next)
                if (g->Address.lpSockaddr && g->Address.lpSockaddr->sa_family == AF_INET)
                    HostGateAllow(((struct sockaddr_in *)g->Address.lpSockaddr)->sin_addr.s_addr, "default gateway (UPnP)");
    free(list);
}

/* TakeOver, after FillImports and before the game entry. */
static BOOL InstallHostGate(void)
{
    InitializeCriticalSection(&g_hgLock);
#if CODOL_LOCAL_TESTING
    const char *cl = GetCommandLineA();
    if (cl && (strstr(cl, "k33_hostGate 1") || strstr(cl, "k33_hostGate \"1\""))) g_hgMode = 1;
#endif
    FARPROC *slotRecv = (FARPROC *)(DWORD_PTR)HG_SLOT_RECVFROM;
    FARPROC *slotSend = (FARPROC *)(DWORD_PTR)HG_SLOT_SENDTO;
    FARPROC *slotHost = (FARPROC *)(DWORD_PTR)HG_SLOT_GETHOSTBYNAME;
    HMODULE ws2 = GetModuleHandleA("ws2_32.dll"), ws1 = GetModuleHandleA("wsock32.dll");
    if (!Readable(slotRecv, 4) || !Readable(slotSend, 4) || !Readable(slotHost, 4) || !*slotRecv || !*slotSend || !*slotHost) {
        Log("hostgate: required socket import slots unavailable; refusing startup");
        return FALSE;
    }
    /* The slots must hold what FillImports put there: the real Winsock functions. */
    FARPROC wantRecv = ws2 ? GetProcAddress(ws2, "recvfrom") : NULL;
    FARPROC wantSend = ws1 ? GetProcAddress(ws1, "sendto") : NULL;
    FARPROC wantSend2 = ws2 ? GetProcAddress(ws2, "sendto") : NULL;
    FARPROC wantHost = ws1 ? GetProcAddress(ws1, "gethostbyname") : NULL;
    FARPROC wantHost2 = ws2 ? GetProcAddress(ws2, "gethostbyname") : NULL;
    if (!wantRecv || *slotRecv != wantRecv ||
        (*slotSend != wantSend && *slotSend != wantSend2) ||
        (*slotHost != wantHost && *slotHost != wantHost2)) {
        Log("hostgate: required socket imports do not match; refusing startup");
        return FALSE;
    }
    HostGateReadLobbyNames();
    HostGateAllowGateways();
    g_hgRecvFrom = (HgRecvFromFn)(void *)*slotRecv;
    g_hgSendTo = (HgSendToFn)(void *)*slotSend;
    g_hgGetHostByName = (HgGetHostByNameFn)(void *)*slotHost;
    FARPROC r = (FARPROC)HostGateRecvFrom, s = (FARPROC)HostGateSendTo, h = (FARPROC)HostGateGetHostByName;
    WriteCode(slotRecv, &r, 4);
    WriteCode(slotSend, &s, 4);
    WriteCode(slotHost, &h, 4);
    if (*slotRecv != r || *slotSend != s || *slotHost != h) {
        Log("hostgate: required socket filter write failed; refusing startup");
        return FALSE;
    }
    if(!P2PProductionInstallSocketHooks())return FALSE;
    HANDLE t = CreateThread(NULL, 0, HostGateResolveThread, NULL, 0, NULL);
    if (t) CloseHandle(t);
    Log("hostgate: %s; UDP only with loopback, the lobby (%d names from dwauth.ini/stun.ini) and lobby-vouched hosts",
        g_hgMode >= 2 ? "enforcing" : "LOG ONLY (k33_hostGate 1)", g_hgNameCount);
    return TRUE;
}
