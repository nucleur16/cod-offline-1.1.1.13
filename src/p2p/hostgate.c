/*
 * hostgate.c - the client exchanges UDP only with this PC (feat/p2p-hosting, offline only).
 *
 * Hard requirement of docs/P2P_HOSTING.md ("Trust"): the client security audit
 * (docs/SECURITY_AUDIT_CLIENT_20261026.md, branch sec/client-audit-20261026) could not verify C1's
 * gamestate/configstring parser (VMProtect-virtualized CL_ParseGamestate, one of IW4's worst client
 * RCEs), has not audited the DemonWare lobby/party/relay layer, and only reasoned about snapshot bounds
 * and the out-of-band handlers. No datagram from another host may reach that code.
 *
 * C1 does all of its UDP (DemonWare, STUN, NAT traversal, bdDTLS game traffic) through two import slots:
 * WSOCK32!sendto (0xB42538) and WS2_32!recvfrom (0xB424E0). Both are wrapped here:
 *   - a datagram from an address outside 127.0.0.0/8 is dropped before C1 sees it (the call moves on to
 *     the next queued datagram, or reports WSAEWOULDBLOCK as an empty non-blocking socket does);
 *   - a datagram to such an address is not sent (reported as sent, like a lost packet).
 * This client is offline only (the lobby and every match server run on 127.0.0.1), so loopback is the one
 * thing allowed: no lobby name, no gateway, no SSDP group and no host ticket is ever vouched (the vouching
 * paths of the online client were removed). Each loopback datagram C1 sends is also reported to p2p.c
 * (P2PLaunchObserve: the armed Local Play host starts at C1's first datagram to its port).
 * The process-wide egress gate (egress_gate.c) keeps every other socket on loopback as well.
 *
 * k33_hostGate on the command line: 2 (default) enforce; explicit local-testing builds alone may
 * use 1 (log only). Shipping builds always enforce. Diagnostic builds log each refused address once to
 * Kernel33.log (the first 64 addresses).
 */

#include "../production_profile.h"

#ifndef HG_SLOT_RECVFROM /* Native fixture supplies its own import table. */
#define HG_SLOT_RECVFROM      0x00B424E0u
#define HG_SLOT_SENDTO        0x00B42538u
#endif

typedef int (WINAPI *HgRecvFromFn)(SOCKET, char *, int, int, struct sockaddr *, int *);
typedef int (WINAPI *HgSendToFn)(SOCKET, const char *, int, int, const struct sockaddr *, int);

static HgRecvFromFn g_hgRecvFrom;
static HgSendToFn g_hgSendTo;
static CRITICAL_SECTION g_hgLock;
#if K33_DIAGNOSTICS
static DWORD g_hgRefused[64];
static int g_hgRefusedCount;
#endif
static int g_hgMode = 2;
#if K33_DIAGNOSTICS
static volatile LONG g_hgDropsIn, g_hgDropsOut;
#endif

/* This PC only: 127.0.0.0/8. */
static BOOL HostGateOk(DWORD ip)
{
    BOOL ok = (ntohl(ip) >> 24) == 127;
    return ok;
}

#if K33_DIAGNOSTICS
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
    Log("hostgate: %s %s %s:%d - not this PC (loopback only)", g_hgMode >= 2 ? "refused" : "WOULD refuse",
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
        if (HostGateOk(in->sin_addr.s_addr)) return r;
        HostGateRefused(in->sin_addr.s_addr, 0, ntohs(in->sin_port));
        if (g_hgMode < 2) return r;
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
    if (to && tolen >= (int)sizeof(struct sockaddr_in) && to->sa_family == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)to;
        /* The armed Local Play host starts when C1 first talks to its loopback port (p2p.c,
         * docs/P2P_DEFERRED_HOST_START.md). Observation only: the datagram is sent as before. */
        if ((ntohl(in->sin_addr.s_addr) >> 24) == 127) P2PLaunchObserve(ntohs(in->sin_port));
        if (!HostGateOk(in->sin_addr.s_addr)) {
            HostGateRefused(in->sin_addr.s_addr, 1, ntohs(in->sin_port));
            if (g_hgMode >= 2) return len;
        }
    }
    return g_hgSendTo(s, buf, len, flags, to, tolen);
}

/* TakeOver, after FillImports and before the game entry. */
static BOOL InstallHostGate(void)
{
    InitializeCriticalSection(&g_hgLock);
#if K33_LOCAL_TESTING
    const char *cl = GetCommandLineA();
    if (cl && (strstr(cl, "k33_hostGate 1") || strstr(cl, "k33_hostGate \"1\""))) g_hgMode = 1;
#endif
    FARPROC *slotRecv = (FARPROC *)(DWORD_PTR)HG_SLOT_RECVFROM;
    FARPROC *slotSend = (FARPROC *)(DWORD_PTR)HG_SLOT_SENDTO;
    HMODULE ws2 = GetModuleHandleA("ws2_32.dll"), ws1 = GetModuleHandleA("wsock32.dll");
    if (!Readable(slotRecv, 4) || !Readable(slotSend, 4) || !*slotRecv || !*slotSend) {
        Log("hostgate: required socket import slots unavailable; refusing startup");
        return FALSE;
    }
    /* The slots must hold what FillImports put there: the real Winsock functions. */
    FARPROC wantRecv = ws2 ? GetProcAddress(ws2, "recvfrom") : NULL;
    FARPROC wantSend = ws1 ? GetProcAddress(ws1, "sendto") : NULL;
    FARPROC wantSend2 = ws2 ? GetProcAddress(ws2, "sendto") : NULL;
    if (!wantRecv || *slotRecv != wantRecv ||
        (*slotSend != wantSend && *slotSend != wantSend2)) {
        Log("hostgate: required socket imports do not match; refusing startup");
        return FALSE;
    }
    g_hgRecvFrom = (HgRecvFromFn)(void *)*slotRecv;
    g_hgSendTo = (HgSendToFn)(void *)*slotSend;
    FARPROC r = (FARPROC)HostGateRecvFrom, s = (FARPROC)HostGateSendTo;
    WriteCode(slotRecv, &r, 4);
    WriteCode(slotSend, &s, 4);
    if (*slotRecv != r || *slotSend != s) {
        Log("hostgate: required socket filter write failed; refusing startup");
        return FALSE;
    }
    Log("hostgate: %s; UDP only with loopback (this PC)", g_hgMode >= 2 ? "enforcing" : "LOG ONLY (k33_hostGate 1)");
    return TRUE;
}
