/*
 * egress_gate.c - nothing in this process reaches an address that is not loopback.
 *
 * This client is offline only (docs/OFFLINE_LOBBY.md). The lobby and the match host are inside this DLL
 * and listen on 127.0.0.1; whatever else the process loads - C1, the Tencent DLLs of the install
 * (TenSLX, TerSafe, TenProxy, BugTrace, GbSpy, ...), the embedded sh1x host, Windows' own HTTP stacks
 * - is held to loopback here. The gate is in every build profile, has no dvar, no ini key and no
 * command-line switch, and is installed from DllMain, i.e. before any other module of the install is
 * loaded (the exe imports Kernel33.dll alone).
 *
 * Two layers.
 *   A. The Winsock / name-resolution / HTTP entry points are hooked INLINE (the first bytes of the
 *      system DLL's function), so the caller does not matter: an import slot, a GetProcAddress result,
 *      a pointer captured earlier, a call from inside another system DLL all arrive here.
 *        ws2_32   connect WSAConnect WSAConnectByNameA/W WSAConnectByList WSAJoinLeaf ConnectEx (through
 *                 WSAIoctl) sendto WSASendTo WSASendMsg - a destination that is not 127.0.0.0/8, ::1 or
 *                 ::ffff:127.x is refused (connect: WSAENETUNREACH; a datagram: reported sent, i.e. lost);
 *                 bind - a socket bound to "any" or to a LAN address is bound to loopback instead, and a
 *                 socket that sends or connects unbound is bound to loopback first, so the process
 *                 neither listens on the network nor raises a firewall prompt;
 *                 setsockopt - multicast group joins are refused (a join is an IGMP packet);
 *                 gethostbyname getaddrinfo GetAddrInfoW GetAddrInfoExA/W WSAAsyncGetHostByName
 *                 gethostbyaddr WSAAsyncGetHostByAddr getnameinfo GetNameInfoW WSALookupServiceBeginA/W -
 *                 a literal address, "localhost" and this PC's own name are passed on (Windows answers
 *                 them itself, from the adapters), every other name fails at once (WSAHOST_NOT_FOUND) without a DNS, LLMNR,
 *                 mDNS or NetBIOS query (those are sent by a Windows service, not by this process, so
 *                 the lookup itself has to be refused);
 *        dnsapi   DnsQuery_A/_W/_UTF8 DnsQueryEx - refused (DNS_ERROR_RCODE_NAME_ERROR);
 *        wininet  InternetConnectA/W InternetOpenUrlA/W InternetCheckConnectionA/W InternetAttemptConnect;
 *        winhttp  WinHttpConnect WinHttpGetProxyForUrl(Ex) WinHttpDetectAutoProxyConfigUrl - a host that
 *                 is not loopback is refused before the stack can ask a proxy or the WPAD service;
 *        netapi32 Netbios - refused (NetBT traffic is sent by the kernel);
 *        iphlpapi IcmpSendEcho IcmpSendEcho2 IcmpSendEcho2Ex Icmp6SendEcho2 - loopback only;
 *        shell32  ShellExecuteA/W ShellExecuteExA/W - a web address is not handed to the browser.
 *      DLLs that are not loaded yet are hooked when they load (LdrRegisterDllNotification).
 *      A function whose first bytes are not one of the three known forms is hooked through the export
 *      table and every module's import table instead; if one of the REQUIRED functions cannot be
 *      hooked either way the client does not start.
 *   B. Backstop: ntdll!NtDeviceIoControlFile. Every Winsock call of this process ends in an AFD ioctl.
 *      At start-up the gate performs each kind of operation once against 127.77.33.1 and learns which
 *      ioctl carries the destination where; afterwards such an ioctl with a destination that is not
 *      loopback is failed with STATUS_NETWORK_UNREACHABLE. It never fires unless layer A missed a path,
 *      and says so in the log when it does.
 *
 * The log: <exe dir>\Kernel33_egress.log, in every profile. One line per distinct refused destination
 * (what, where, which module asked), one per distinct loopback destination, and a summary at exit.
 * Not covered, by nature: a UNC path or a remote pipe name handed to a file API (the SMB redirector is
 * kernel code), and other processes. Nothing in the install's modules names either.
 */
#include <ws2tcpip.h>
#include <mswsock.h>

#define EG_MAX_HOOKS   64
#define EG_TRAMP_SIZE  16
#define EG_SEEN_MAX    1024
#define EG_LOG_MAX     (512 * 1024)

typedef LONG EG_NTSTATUS;
#define EG_STATUS_NETWORK_UNREACHABLE ((EG_NTSTATUS)0xC000023CL)

typedef struct {
    const char *dll, *name;
    void *hook;
    void **real;      /* receives the callable original */
    BOOL required;
    BYTE state;       /* 0 not installed, 1 inline, 2 export/import tables */
    void *target;     /* the function's address in the system DLL */
} EgHook;

static CRITICAL_SECTION g_egLock;
static BOOL g_egReady, g_egFailed;
static BYTE *g_egTramp;
static int g_egTrampUsed;
static DWORD g_egSeen[EG_SEEN_MAX];
static int g_egSeenCount;
static volatile LONG g_egRefused, g_egRefusedLines, g_egPassed, g_egBackstop, g_egMoved;
static char g_egLogPath[MAX_PATH + 32];
static char g_egWinDir[MAX_PATH];
static char g_egOwnName[256];
static void *g_egCookie;

/* ---- log ---------------------------------------------------------------------------------------- */

static void EgWrite(const char *line)
{
    HANDLE h = CreateFileA(g_egLogPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w;
    WriteFile(h, line, (DWORD)lstrlenA(line), &w, NULL);
    WriteFile(h, "\r\n", 2, &w, NULL);
    CloseHandle(h);
}

static BOOL EgSeen(const char *key)
{
    DWORD hsh = 2166136261u;
    for (const BYTE *p = (const BYTE *)key; *p; p++) hsh = (hsh ^ *p) * 16777619u;
    if (!hsh) hsh = 1;
    BOOL seen = FALSE;
    EnterCriticalSection(&g_egLock);
    for (int i = 0; i < g_egSeenCount && !seen; i++) seen = g_egSeen[i] == hsh;
    if (!seen) {
        if (g_egSeenCount < EG_SEEN_MAX) g_egSeen[g_egSeenCount++] = hsh;
        else seen = TRUE; /* table full: stop writing, keep refusing */
    }
    LeaveCriticalSection(&g_egLock);
    return seen;
}

/* "TenSLX.dll+0x1A2B3" for an address; the exe's own code is "C1@0x00ABCDEF". system = under %SystemRoot%. */
static void EgModuleOf(const void *addr, char *out, int cap, BOOL *system)
{
    HMODULE mod = NULL;
    char path[MAX_PATH];
    if (system) *system = FALSE;
    if (addr && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)addr, &mod) && mod && GetModuleFileNameA(mod, path, sizeof(path))) {
        if (mod == GetModuleHandleA(NULL)) { _snprintf(out, cap, "C1@0x%08lX", (unsigned long)(DWORD_PTR)addr); out[cap - 1] = 0; return; }
        const char *base = path + lstrlenA(path);
        while (base > path && base[-1] != '\\') base--;
        if (system && g_egWinDir[0] && !_strnicmp(path, g_egWinDir, lstrlenA(g_egWinDir))) *system = TRUE;
        _snprintf(out, cap, "%s+0x%lX", base, (unsigned long)((DWORD_PTR)addr - (DWORD_PTR)mod));
    } else {
        /* the unpacked game code lives in the packer section, outside any module's mapped headers */
        _snprintf(out, cap, "%s@0x%08lX", (DWORD_PTR)addr >= 0x08849000u && (DWORD_PTR)addr < 0x09A10000u ? "C1" : "unmapped",
                  (unsigned long)(DWORD_PTR)addr);
    }
    out[cap - 1] = 0;
}

/* Who asked: the first frame that is neither Windows' own code nor Kernel33's wrapper of a C1 import slot, and the
 * immediate caller when that differs. The frame chain first; code built without frame pointers breaks it, so
 * the stack is then searched for the first return address into such a module (a word that follows a call). */
static BOOL EgAfterCall(const BYTE *ret)
{
    return ret[-5] == 0xE8 || (ret[-6] == 0xFF && (ret[-5] == 0x15 || (ret[-5] & 0xF8) == 0x90)) ||
           (ret[-2] == 0xFF && (ret[-1] & 0xF8) == 0xD0) || (ret[-2] == 0xFF && (ret[-1] & 0xF8) == 0x10) ||
           (ret[-3] == 0xFF && (ret[-2] & 0xF8) == 0x50);
}

static void EgWho(const void *ra, char *out, int cap)
{
    char near_[96], far_[96];
    BOOL sys = FALSE;
    EgModuleOf(ra, near_, sizeof(near_), &sys);
    if (!sys && _strnicmp(near_, "Kernel33.dll", 12)) { lstrcpynA(out, near_, cap); return; }
    void *frames[24];
    USHORT n = RtlCaptureStackBackTrace(1, 24, frames, NULL);
    BOOL past = FALSE;
    for (USHORT i = 0; i < n; i++) {
        if (!past) { if (frames[i] == ra) past = TRUE; continue; }
        BOOL s2 = FALSE;
        EgModuleOf(frames[i], far_, sizeof(far_), &s2);
        if (!s2 && _strnicmp(far_, "Kernel33.dll", 12)) { _snprintf(out, cap, "%s (through %s)", far_, near_); out[cap - 1] = 0; return; }
    }
    DWORD_PTR *sp = (DWORD_PTR *)&frames[0], *top;
    __asm__("movl %%fs:0x04, %0" : "=r"(top));
    for (int i = 0; i < 2048 && sp + i < top; i++) {
        DWORD_PTR v = sp[i];
        HMODULE mod = NULL;
        if (v < 0x10000 || v == (DWORD_PTR)ra) continue;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)v, &mod) || !mod ||
            mod == (HMODULE)g_self) continue;
        if (!Readable((const BYTE *)v - 6, 7) || !EgAfterCall((const BYTE *)v)) continue;
        BOOL s2 = FALSE;
        EgModuleOf((const void *)v, far_, sizeof(far_), &s2);
        if (s2) continue;
        _snprintf(out, cap, "%s (through %s)", far_, near_);
        out[cap - 1] = 0;
        return;
    }
    lstrcpynA(out, near_, cap);
}

/* verdict: 0 refused, 1 loopback (passed), 2 moved, 3 backstop refusal */
static void EgReport(int verdict, const char *what, const char *where, const void *ra)
{
    /* a loopback line names the first port seen for that address and module; further ports are not listed */
    static const char *const kTag[] = { "refused ", "loopback", "moved   ", "REFUSED AT THE BACKSTOP (layer A missed this path)" };
    char who[224], line[640];
    if (verdict == 0) InterlockedIncrement(&g_egRefused);
    else if (verdict == 1) InterlockedIncrement(&g_egPassed);
    else if (verdict == 2) InterlockedIncrement(&g_egMoved);
    else InterlockedIncrement(&g_egBackstop);
    EgWho(ra, who, sizeof(who));
    /* one line per (what, where, module): the offset inside the module is not part of the key */
    char key[512], modname[96];
    lstrcpynA(modname, who, sizeof(modname));
    for (char *p = modname; *p; p++) if (*p == '+' || *p == '@' || *p == ' ') { *p = 0; break; }
    _snprintf(key, sizeof(key), "%d|%s|%s|%s", verdict, what, where, modname);
    key[sizeof(key) - 1] = 0;
    if (verdict == 1) { /* loopback: the port is not part of the key (a match host talks to dozens of ephemeral ports) */
        char *colon = strrchr(key, ':');
        char *bar = strrchr(key, '|');
        if (colon && bar && colon < bar && colon[1] >= '0' && colon[1] <= '9') memmove(colon, bar, lstrlenA(bar) + 1);
    }
    if (EgSeen(key)) return;
    if (verdict == 0 || verdict == 3) InterlockedIncrement(&g_egRefusedLines);
    _snprintf(line, sizeof(line), "%s %-22s %-44s by %s", kTag[verdict], what, where, who);
    line[sizeof(line) - 1] = 0;
    EgWrite(line);
    Log("egress: %s", line);
}

/* ---- addresses and names ------------------------------------------------------------------------ */

static BOOL EgLoopback4(DWORD netOrder) { return (ntohl(netOrder) >> 24) == 127; }

static BOOL EgLoopback6(const BYTE *a)
{
    static const BYTE kLoop[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    static const BYTE kMapped[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF };
    if (!memcmp(a, kLoop, 16)) return TRUE;
    return !memcmp(a, kMapped, 12) && a[12] == 127;
}

/* 1 = may be used as a destination, 0 = refused. An address the real function will reject anyway passes. */
static int EgAddrOk(const struct sockaddr *sa, int len)
{
    if (!sa || len < (int)sizeof(sa->sa_family)) return 1;
    switch (sa->sa_family) {
    case AF_UNSPEC: return 1; /* dissolves a UDP association */
    case 1 /* AF_UNIX */: return 1;
    case AF_INET:
        if (len < (int)sizeof(struct sockaddr_in)) return 1;
        return EgLoopback4(((const struct sockaddr_in *)sa)->sin_addr.s_addr);
    case AF_INET6:
        if (len < 24) return 1;
        return EgLoopback6((const BYTE *)&((const struct sockaddr_in6 *)sa)->sin6_addr);
    }
    return 0;
}

static void EgAddrText(const struct sockaddr *sa, int len, char *out, int cap)
{
    if (sa && sa->sa_family == AF_INET && len >= (int)sizeof(struct sockaddr_in)) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)sa;
        const BYTE *b = (const BYTE *)&in->sin_addr;
        _snprintf(out, cap, "%u.%u.%u.%u:%u", b[0], b[1], b[2], b[3], ntohs(in->sin_port));
    } else if (sa && sa->sa_family == AF_INET6 && len >= 24) {
        const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)sa;
        const BYTE *b = (const BYTE *)&in6->sin6_addr;
        int n = _snprintf(out, cap, "[");
        for (int i = 0; i < 16 && n > 0 && n < cap - 8; i += 2) n += _snprintf(out + n, cap - n, "%s%x", i ? ":" : "", (b[i] << 8) | b[i + 1]);
        if (n > 0 && n < cap - 8) _snprintf(out + n, cap - n, "]:%u", ntohs(in6->sin6_port));
    } else {
        _snprintf(out, cap, "address family %d", sa ? sa->sa_family : -1);
    }
    out[cap - 1] = 0;
}

static const char *EgSockKind(SOCKET s)
{
    int type = 0, tl = sizeof(type);
    if (getsockopt(s, SOL_SOCKET, SO_TYPE, (char *)&type, &tl)) return "socket";
    return type == SOCK_STREAM ? "tcp" : type == SOCK_DGRAM ? "udp" : type == SOCK_RAW ? "raw" : "socket";
}

static int (WSAAPI *g_egBind)(SOCKET, const struct sockaddr *, int);

/* A socket that sends or connects without having been bound is bound by Windows to "any" address and a free port,
 * i.e. it would take datagrams from the network on that port. Bind it to loopback first. */
static void EgBindUnbound(SOCKET s, int family)
{
    struct sockaddr_storage cur;
    int len = sizeof(cur), err = WSAGetLastError();
    if (!g_egBind || !getsockname(s, (struct sockaddr *)&cur, &len) || WSAGetLastError() != WSAEINVAL) { WSASetLastError(err); return; }
    if (family == AF_INET) {
        struct sockaddr_in in;
        memset(&in, 0, sizeof(in));
        in.sin_family = AF_INET;
        in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        g_egBind(s, (const struct sockaddr *)&in, sizeof(in));
    } else if (family == AF_INET6) {
        struct sockaddr_in6 in6;
        memset(&in6, 0, sizeof(in6));
        in6.sin6_family = AF_INET6;
        ((BYTE *)&in6.sin6_addr)[15] = 1;
        g_egBind(s, (const struct sockaddr *)&in6, sizeof(in6));
    }
    WSASetLastError(err);
}

/* A destination check with its report. TRUE = go on. */
static BOOL EgDest(SOCKET s, const char *verb, const struct sockaddr *sa, int len, const void *ra)
{
    static volatile DWORD seenOk[64]; /* loopback destinations already listed: the hot path of every datagram */
    char what[48], where[96];
    int ok = EgAddrOk(sa, len);
    if (ok && (!sa || len < (int)sizeof(struct sockaddr_in) || (sa->sa_family != AF_INET && sa->sa_family != AF_INET6))) return TRUE;
    if (ok) EgBindUnbound(s, sa->sa_family);
    if (ok && sa->sa_family == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)sa;
        DWORD k = (in->sin_addr.s_addr ^ ((DWORD)in->sin_port * 2654435761u) ^ ((DWORD)(BYTE)verb[0] << 8)) | 1u;
        if (seenOk[k & 63] == k) { InterlockedIncrement(&g_egPassed); return TRUE; }
        seenOk[k & 63] = k;
    }
    _snprintf(what, sizeof(what), "%s %s", EgSockKind(s), verb);
    what[sizeof(what) - 1] = 0;
    EgAddrText(sa, len, where, sizeof(where));
    EgReport(ok ? 1 : 0, what, where, ra);
    return ok != 0;
}

/* A host name: 0 = a literal address (or none), 1 = this PC ("localhost", its own name), 2 = anything else. */
static int EgNameKind(const char *name)
{
    if (!name || !*name) return 0;
    if (strchr(name, ':')) return 0; /* an IPv6 literal; no DNS name holds a colon */
    int parts = 0, digits = 0, value = 0, literal = 1;
    for (const char *p = name;; p++) {
        if (*p >= '0' && *p <= '9') { value = value * 10 + (*p - '0'); if (++digits > 3 || value > 255) { literal = 0; break; } }
        else if (*p == '.' || !*p) { if (!digits) { literal = 0; break; } parts++; digits = 0; value = 0; if (!*p) break; }
        else { literal = 0; break; }
    }
    if (literal && parts == 4) return 0;
    if (!_stricmp(name, "localhost") || !_stricmp(name, "localhost.")) return 1;
    if (g_egOwnName[0] && !_stricmp(name, g_egOwnName)) return 1;
    return 2;
}

static int EgNameKindW(const WCHAR *name, char *ansi, int cap)
{
    ansi[0] = 0;
    if (!name || !*name) return 0;
    if (!WideCharToMultiByte(CP_ACP, 0, name, -1, ansi, cap, NULL, NULL)) { lstrcpynA(ansi, "(unprintable name)", cap); return 2; }
    ansi[cap - 1] = 0;
    return EgNameKind(ansi);
}

static void EgName(int verdict, const char *api, const char *name, const void *ra)
{
    char what[48], where[200];
    _snprintf(what, sizeof(what), "dns %s", api);
    what[sizeof(what) - 1] = 0;
    _snprintf(where, sizeof(where), "\"%.180s\"", name ? name : "");
    where[sizeof(where) - 1] = 0;
    EgReport(verdict, what, where, ra);
}

/* A URL or host as WinINet / WinHTTP / the shell take it: TRUE when it names this PC or no host at all. */
static BOOL EgHostOk(const char *host)
{
    char h[256];
    lstrcpynA(h, host ? host : "", sizeof(h));
    char *p = h;
    if (*p == '[') { char *e = strchr(p, ']'); if (e) *e = 0; p++; return !lstrcmpA(p, "::1"); }
    char *colon = strchr(p, ':');
    if (colon) *colon = 0;
    if (!*p) return TRUE;
    if (EgNameKind(p) == 1) return TRUE;
    DWORD ip = inet_addr(p);
    return EgNameKind(p) == 0 && ip != INADDR_NONE && EgLoopback4(ip);
}

/* 0 = not a network URL, 1 = a URL of this PC, 2 = a URL of another host */
static int EgUrlKind(const char *url)
{
    if (!url) return 0;
    while (*url == ' ' || *url == '"') url++;
    const char *sep = strstr(url, "://");
    if (!sep) return !_strnicmp(url, "www.", 4) || !_strnicmp(url, "mailto:", 7) ? 2 : 0;
    int sl = (int)(sep - url);
    if (sl == 4 && !_strnicmp(url, "file", 4)) return (sep[3] == '/' || !sep[3]) ? 0 : 2; /* file://host/share is a network path */
    if (!((sl == 4 && !_strnicmp(url, "http", 4)) || (sl == 5 && !_strnicmp(url, "https", 5)) || (sl == 3 && !_strnicmp(url, "ftp", 3)) ||
          (sl == 2 && !_strnicmp(url, "ws", 2)) || (sl == 3 && !_strnicmp(url, "wss", 3)) || (sl == 6 && !_strnicmp(url, "gopher", 6))))
        return (sl == 3 && !_strnicmp(url, "res", 3)) || (sl == 5 && !_strnicmp(url, "about", 5)) ? 0 : 2;
    char host[256];
    const char *hs = sep + 3;
    const char *at = NULL;
    int n = 0;
    for (const char *q = hs; *q && *q != '/' && *q != '?' && *q != '#'; q++) if (*q == '@') at = q;
    if (at) hs = at + 1;
    for (; hs[n] && hs[n] != '/' && hs[n] != '?' && hs[n] != '#' && n < (int)sizeof(host) - 1; n++) host[n] = hs[n];
    host[n] = 0;
    return EgHostOk(host) ? 1 : 2;
}

static void EgAnsi(const WCHAR *w, char *out, int cap)
{
    out[0] = 0;
    if (w && !WideCharToMultiByte(CP_ACP, 0, w, -1, out, cap, NULL, NULL)) lstrcpynA(out, "(unprintable)", cap);
    out[cap - 1] = 0;
}

#define EG_RA() __builtin_return_address(0)

/* ---- ws2_32: destinations ----------------------------------------------------------------------- */

static int (WSAAPI *g_egConnect)(SOCKET, const struct sockaddr *, int);
static int (WSAAPI *g_egWSAConnect)(SOCKET, const struct sockaddr *, int, void *, void *, void *, void *);
static int (WSAAPI *g_egSendTo)(SOCKET, const char *, int, int, const struct sockaddr *, int);
static int (WSAAPI *g_egWSASendTo)(SOCKET, WSABUF *, DWORD, DWORD *, DWORD, const struct sockaddr *, int, void *, void *);
static int (WSAAPI *g_egWSASendMsg)(SOCKET, WSAMSG *, DWORD, DWORD *, void *, void *);
static BOOL (WSAAPI *g_egConnByNameA)(SOCKET, char *, char *, DWORD *, struct sockaddr *, DWORD *, struct sockaddr *, const struct timeval *, void *);
static BOOL (WSAAPI *g_egConnByNameW)(SOCKET, WCHAR *, WCHAR *, DWORD *, struct sockaddr *, DWORD *, struct sockaddr *, const struct timeval *, void *);
static BOOL (WSAAPI *g_egConnByList)(SOCKET, SOCKET_ADDRESS_LIST *, DWORD *, struct sockaddr *, DWORD *, struct sockaddr *, const struct timeval *, void *);
static SOCKET (WSAAPI *g_egJoinLeaf)(SOCKET, const struct sockaddr *, int, void *, void *, void *, void *, DWORD);
static int (WSAAPI *g_egWSAIoctl)(SOCKET, DWORD, void *, DWORD, void *, DWORD, DWORD *, void *, void *);
static int (WSAAPI *g_egSetSockOpt)(SOCKET, int, int, const char *, int);
static BOOL (PASCAL *g_egConnectEx)(SOCKET, const struct sockaddr *, int, void *, DWORD, DWORD *, OVERLAPPED *);

static int WSAAPI EgConnect(SOCKET s, const struct sockaddr *name, int len)
{
    if (!EgDest(s, "connect", name, len, EG_RA())) { WSASetLastError(WSAENETUNREACH); return SOCKET_ERROR; }
    return g_egConnect(s, name, len);
}

static int WSAAPI EgWSAConnect(SOCKET s, const struct sockaddr *name, int len, void *a, void *b, void *c, void *d)
{
    if (!EgDest(s, "connect", name, len, EG_RA())) { WSASetLastError(WSAENETUNREACH); return SOCKET_ERROR; }
    return g_egWSAConnect(s, name, len, a, b, c, d);
}

static BOOL PASCAL EgConnectEx(SOCKET s, const struct sockaddr *name, int len, void *buf, DWORD bufLen, DWORD *sent, OVERLAPPED *ov)
{
    if (!EgDest(s, "connect", name, len, EG_RA())) { WSASetLastError(WSAENETUNREACH); return FALSE; }
    return g_egConnectEx(s, name, len, buf, bufLen, sent, ov);
}

static BOOL EgStream(SOCKET s)
{
    int type = 0, tl = sizeof(type);
    return !getsockopt(s, SOL_SOCKET, SO_TYPE, (char *)&type, &tl) && type == SOCK_STREAM;
}

static int WSAAPI EgSendTo(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
    /* a stream socket ignores the address; its connection was judged at connect */
    if (to && !EgAddrOk(to, tolen) && EgStream(s)) return g_egSendTo(s, buf, len, flags, to, tolen);
    if (!EgDest(s, "sendto", to, tolen, EG_RA())) return len; /* like a lost datagram */
    return g_egSendTo(s, buf, len, flags, to, tolen);
}

static DWORD EgBufTotal(const WSABUF *bufs, DWORD count)
{
    DWORD total = 0;
    for (DWORD i = 0; bufs && i < count; i++) total += bufs[i].len;
    return total;
}

static int WSAAPI EgWSASendTo(SOCKET s, WSABUF *bufs, DWORD count, DWORD *sent, DWORD flags, const struct sockaddr *to, int tolen,
                              void *ov, void *done)
{
    if (to && !EgAddrOk(to, tolen) && EgStream(s)) return g_egWSASendTo(s, bufs, count, sent, flags, to, tolen, ov, done);
    if (!EgDest(s, "sendto", to, tolen, EG_RA())) {
        if (ov) { WSASetLastError(WSAENETUNREACH); return SOCKET_ERROR; } /* no completion will follow */
        if (sent) *sent = EgBufTotal(bufs, count);
        return 0;
    }
    return g_egWSASendTo(s, bufs, count, sent, flags, to, tolen, ov, done);
}

static int WSAAPI EgWSASendMsg(SOCKET s, WSAMSG *msg, DWORD flags, DWORD *sent, void *ov, void *done)
{
    if (msg && msg->name && !EgDest(s, "sendmsg", msg->name, msg->namelen, EG_RA())) {
        if (ov) { WSASetLastError(WSAENETUNREACH); return SOCKET_ERROR; }
        if (sent) *sent = EgBufTotal(msg->lpBuffers, msg->dwBufferCount);
        return 0;
    }
    return g_egWSASendMsg(s, msg, flags, sent, ov, done);
}

static BOOL WSAAPI EgConnByNameA(SOCKET s, char *node, char *service, DWORD *ll, struct sockaddr *la, DWORD *rl, struct sockaddr *ra_,
                                 const struct timeval *tv, void *reserved)
{
    int kind = EgNameKind(node);
    if (kind == 2 || (kind == 0 && !EgHostOk(node))) {
        EgName(0, "WSAConnectByName", node, EG_RA());
        WSASetLastError(WSAHOST_NOT_FOUND);
        return FALSE;
    }
    return g_egConnByNameA(s, node, service, ll, la, rl, ra_, tv, reserved);
}

static BOOL WSAAPI EgConnByNameW(SOCKET s, WCHAR *node, WCHAR *service, DWORD *ll, struct sockaddr *la, DWORD *rl, struct sockaddr *ra_,
                                 const struct timeval *tv, void *reserved)
{
    char a[256];
    int kind = EgNameKindW(node, a, sizeof(a));
    if (kind == 2 || (kind == 0 && !EgHostOk(a))) {
        EgName(0, "WSAConnectByName", a, EG_RA());
        WSASetLastError(WSAHOST_NOT_FOUND);
        return FALSE;
    }
    return g_egConnByNameW(s, node, service, ll, la, rl, ra_, tv, reserved);
}

static BOOL WSAAPI EgConnByList(SOCKET s, SOCKET_ADDRESS_LIST *list, DWORD *ll, struct sockaddr *la, DWORD *rl, struct sockaddr *ra_,
                                const struct timeval *tv, void *reserved)
{
    for (int i = 0; list && i < list->iAddressCount; i++)
        if (!EgDest(s, "connect", list->Address[i].lpSockaddr, list->Address[i].iSockaddrLength, EG_RA())) {
            WSASetLastError(WSAENETUNREACH);
            return FALSE;
        }
    return g_egConnByList(s, list, ll, la, rl, ra_, tv, reserved);
}

static SOCKET WSAAPI EgJoinLeaf(SOCKET s, const struct sockaddr *name, int len, void *a, void *b, void *c, void *d, DWORD flags)
{
    if (!EgDest(s, "join", name, len, EG_RA())) { WSASetLastError(WSAENETUNREACH); return INVALID_SOCKET; }
    return g_egJoinLeaf(s, name, len, a, b, c, d, flags);
}

/* ConnectEx and WSASendMsg are handed out as raw provider pointers by WSAIoctl; give out the gated ones. */
static int WSAAPI EgWSAIoctl(SOCKET s, DWORD code, void *in, DWORD inLen, void *out, DWORD outLen, DWORD *ret, void *ov, void *done)
{
    static const GUID kConnectEx = { 0x25a207b9, 0xddf3, 0x4660, { 0x8e, 0xe9, 0x76, 0xe5, 0x8c, 0x74, 0x06, 0x3e } };
    static const GUID kSendMsg = { 0xa441e712, 0x754f, 0x43ca, { 0x84, 0xa7, 0x0d, 0xee, 0x44, 0xcf, 0x60, 0x6d } };
    int r = g_egWSAIoctl(s, code, in, inLen, out, outLen, ret, ov, done);
    if (r == 0 && code == 0xC8000006u /* SIO_GET_EXTENSION_FUNCTION_POINTER */ && in && inLen >= sizeof(GUID) && out && outLen >= sizeof(void *)) {
        if (!memcmp(in, &kConnectEx, sizeof(GUID))) {
            void *real = *(void **)out;
            EnterCriticalSection(&g_egLock);
            if (!g_egConnectEx) g_egConnectEx = (BOOL (PASCAL *)(SOCKET, const struct sockaddr *, int, void *, DWORD, DWORD *, OVERLAPPED *))real;
            BOOL same = (void *)g_egConnectEx == real;
            LeaveCriticalSection(&g_egLock);
            if (!same) { /* a second provider's ConnectEx cannot be wrapped by the one wrapper: not handed out */
                *(void **)out = NULL;
                WSASetLastError(WSAEOPNOTSUPP);
                return SOCKET_ERROR;
            }
            *(void **)out = (void *)EgConnectEx;
        } else if (!memcmp(in, &kSendMsg, sizeof(GUID)) && g_egWSASendMsg) {
            *(void **)out = (void *)EgWSASendMsg;
        }
    }
    return r;
}

/* Nothing of this process listens on the network: "any" and LAN addresses become loopback. */
static int WSAAPI EgBind(SOCKET s, const struct sockaddr *name, int len)
{
    if (name && name->sa_family == AF_INET && len >= (int)sizeof(struct sockaddr_in) &&
        !EgLoopback4(((const struct sockaddr_in *)name)->sin_addr.s_addr)) {
        struct sockaddr_in in = *(const struct sockaddr_in *)name;
        char what[48], where[128], from[64];
        EgAddrText(name, len, from, sizeof(from));
        in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        _snprintf(what, sizeof(what), "%s bind", EgSockKind(s));
        _snprintf(where, sizeof(where), "%s -> 127.0.0.1", from);
        what[sizeof(what) - 1] = where[sizeof(where) - 1] = 0;
        EgReport(2, what, where, EG_RA());
        return g_egBind(s, (const struct sockaddr *)&in, sizeof(in));
    }
    if (name && name->sa_family == AF_INET6 && len >= 24 && !EgLoopback6((const BYTE *)&((const struct sockaddr_in6 *)name)->sin6_addr)) {
        BYTE copy[64];
        int n = len > (int)sizeof(copy) ? (int)sizeof(copy) : len;
        char what[48], where[160], from[96];
        memcpy(copy, name, n);
        struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)copy;
        EgAddrText(name, len, from, sizeof(from));
        memset(&in6->sin6_addr, 0, 16);
        ((BYTE *)&in6->sin6_addr)[15] = 1;
        in6->sin6_scope_id = 0;
        _snprintf(what, sizeof(what), "%s bind", EgSockKind(s));
        _snprintf(where, sizeof(where), "%s -> [::1]", from);
        what[sizeof(what) - 1] = where[sizeof(where) - 1] = 0;
        EgReport(2, what, where, EG_RA());
        return g_egBind(s, (const struct sockaddr *)copy, n);
    }
    return g_egBind(s, name, len);
}

static int WSAAPI EgSetSockOpt(SOCKET s, int level, int opt, const char *val, int len)
{
    /* IP_ADD_MEMBERSHIP 12, IP_ADD_SOURCE_MEMBERSHIP 15, IPV6_ADD_MEMBERSHIP 12, MCAST_JOIN_GROUP 41, MCAST_JOIN_SOURCE_GROUP 45 */
    BOOL join = (level == IPPROTO_IP && (opt == 12 || opt == 15 || opt == 41 || opt == 45)) ||
                (level == IPPROTO_IPV6 && (opt == 12 || opt == 41 || opt == 45));
    if (join) {
        char where[96];
        if (level == IPPROTO_IP && (opt == 12 || opt == 15) && val && len >= 4) {
            const BYTE *b = (const BYTE *)val;
            _snprintf(where, sizeof(where), "group %u.%u.%u.%u", b[0], b[1], b[2], b[3]);
        } else {
            _snprintf(where, sizeof(where), "group (level %d option %d)", level, opt);
        }
        where[sizeof(where) - 1] = 0;
        EgReport(0, "multicast join", where, EG_RA());
        WSASetLastError(WSAEADDRNOTAVAIL);
        return SOCKET_ERROR;
    }
    return g_egSetSockOpt(s, level, opt, val, len);
}

/* ---- ws2_32: names ------------------------------------------------------------------------------ */

static struct hostent *(WSAAPI *g_egGetHostByName)(const char *);
static struct hostent *(WSAAPI *g_egGetHostByAddr)(const char *, int, int);
static int (WSAAPI *g_egGetAddrInfo)(const char *, const char *, const struct addrinfo *, struct addrinfo **);
static int (WSAAPI *g_egGetAddrInfoW)(const WCHAR *, const WCHAR *, const void *, void **);
static int (WSAAPI *g_egGetAddrInfoExA)(const char *, const char *, DWORD, GUID *, const void *, void **, struct timeval *, void *, void *, HANDLE *);
static int (WSAAPI *g_egGetAddrInfoExW)(const WCHAR *, const WCHAR *, DWORD, GUID *, const void *, void **, struct timeval *, void *, void *, HANDLE *);
static int (WSAAPI *g_egGetNameInfo)(const struct sockaddr *, int, char *, DWORD, char *, DWORD, int);
static int (WSAAPI *g_egGetNameInfoW)(const struct sockaddr *, int, WCHAR *, DWORD, WCHAR *, DWORD, int);
static HANDLE (WSAAPI *g_egAsyncByName)(HWND, UINT, const char *, char *, int);
static HANDLE (WSAAPI *g_egAsyncByAddr)(HWND, UINT, const char *, int, int, char *, int);
static int (WSAAPI *g_egLookupBeginA)(BYTE *, DWORD, HANDLE *);
static int (WSAAPI *g_egLookupBeginW)(BYTE *, DWORD, HANDLE *);

static struct hostent *WSAAPI EgGetHostByName(const char *name)
{
    int kind = EgNameKind(name);
    if (kind == 2) { EgName(0, "gethostbyname", name, EG_RA()); WSASetLastError(WSAHOST_NOT_FOUND); return NULL; }
    /* "localhost", this PC's own name and "" are answered by Windows from the adapters, without a query on the
     * wire. (Answering them with 127.0.0.1 instead leaves DemonWare's local-address list empty: it drops
     * loopback, and C1 then reads element 0 of an empty array when a match starts.) */
    if (kind == 1) EgName(1, "gethostbyname", name, EG_RA());
    return g_egGetHostByName(name);
}

static struct hostent *WSAAPI EgGetHostByAddr(const char *addr, int len, int type)
{
    BOOL ok = addr && ((type == AF_INET && len >= 4 && EgLoopback4(*(const DWORD *)addr)) ||
                       (type == AF_INET6 && len >= 16 && EgLoopback6((const BYTE *)addr)));
    if (!ok) { EgName(0, "gethostbyaddr", "(reverse lookup)", EG_RA()); WSASetLastError(WSAHOST_NOT_FOUND); return NULL; }
    return g_egGetHostByAddr(addr, len, type);
}

static int WSAAPI EgGetAddrInfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res)
{
    int kind = EgNameKind(node);
    if (kind == 2) { EgName(0, "getaddrinfo", node, EG_RA()); if (res) *res = NULL; WSASetLastError(WSAHOST_NOT_FOUND); return WSAHOST_NOT_FOUND; }
    if (kind == 1) EgName(1, "getaddrinfo", node, EG_RA());
    return g_egGetAddrInfo(node, service, hints, res);
}

static int WSAAPI EgGetAddrInfoW(const WCHAR *node, const WCHAR *service, const void *hints, void **res)
{
    char a[256];
    int kind = EgNameKindW(node, a, sizeof(a));
    if (kind == 2) { EgName(0, "getaddrinfo", a, EG_RA()); if (res) *res = NULL; WSASetLastError(WSAHOST_NOT_FOUND); return WSAHOST_NOT_FOUND; }
    if (kind == 1) EgName(1, "getaddrinfo", a, EG_RA());
    return g_egGetAddrInfoW(node, service, hints, res);
}

static int WSAAPI EgGetAddrInfoExA(const char *node, const char *service, DWORD ns, GUID *nsp, const void *hints, void **res,
                                   struct timeval *tv, void *ov, void *done, HANDLE *handle)
{
    int kind = EgNameKind(node);
    if (kind == 2) { EgName(0, "getaddrinfo", node, EG_RA()); if (res) *res = NULL; WSASetLastError(WSAHOST_NOT_FOUND); return WSAHOST_NOT_FOUND; }
    if (kind == 1) EgName(1, "getaddrinfo", node, EG_RA());
    return g_egGetAddrInfoExA(node, service, ns, nsp, hints, res, tv, ov, done, handle);
}

static int WSAAPI EgGetAddrInfoExW(const WCHAR *node, const WCHAR *service, DWORD ns, GUID *nsp, const void *hints, void **res,
                                   struct timeval *tv, void *ov, void *done, HANDLE *handle)
{
    char a[256];
    int kind = EgNameKindW(node, a, sizeof(a));
    if (kind == 2) { EgName(0, "getaddrinfo", a, EG_RA()); if (res) *res = NULL; WSASetLastError(WSAHOST_NOT_FOUND); return WSAHOST_NOT_FOUND; }
    if (kind == 1) EgName(1, "getaddrinfo", a, EG_RA());
    return g_egGetAddrInfoExW(node, service, ns, nsp, hints, res, tv, ov, done, handle);
}

/* A reverse lookup of a foreign address is answered with the literal: NI_NUMERICHOST. */
static int WSAAPI EgGetNameInfo(const struct sockaddr *sa, int len, char *host, DWORD hostLen, char *serv, DWORD servLen, int flags)
{
    if (!EgAddrOk(sa, len) || (sa && sa->sa_family != AF_INET && sa->sa_family != AF_INET6)) flags = (flags | 0x02) & ~0x04;
    return g_egGetNameInfo(sa, len, host, hostLen, serv, servLen, flags);
}

static int WSAAPI EgGetNameInfoW(const struct sockaddr *sa, int len, WCHAR *host, DWORD hostLen, WCHAR *serv, DWORD servLen, int flags)
{
    if (!EgAddrOk(sa, len) || (sa && sa->sa_family != AF_INET && sa->sa_family != AF_INET6)) flags = (flags | 0x02) & ~0x04;
    return g_egGetNameInfoW(sa, len, host, hostLen, serv, servLen, flags);
}

static HANDLE WSAAPI EgAsyncByName(HWND wnd, UINT msg, const char *name, char *buf, int len)
{
    if (EgNameKind(name) == 2) { EgName(0, "WSAAsyncGetHostByName", name, EG_RA()); WSASetLastError(WSAHOST_NOT_FOUND); return NULL; }
    return g_egAsyncByName(wnd, msg, name, buf, len);
}

static HANDLE WSAAPI EgAsyncByAddr(HWND wnd, UINT msg, const char *addr, int len, int type, char *buf, int bufLen)
{
    BOOL ok = addr && ((type == AF_INET && len >= 4 && EgLoopback4(*(const DWORD *)addr)) ||
                       (type == AF_INET6 && len >= 16 && EgLoopback6((const BYTE *)addr)));
    if (!ok) { EgName(0, "WSAAsyncGetHostByAddr", "(reverse lookup)", EG_RA()); WSASetLastError(WSAHOST_NOT_FOUND); return NULL; }
    return g_egAsyncByAddr(wnd, msg, addr, len, type, buf, bufLen);
}

/* A name-space query (what the lookups above end in, and what a caller can issue directly): judged by the name it
 * asks for. No name at all is "this PC" on the default and DNS name spaces (gethostbyname("") ends in one) and
 * the local network-location list on NS_NLA; on any other name space (Bluetooth, ...) it is a discovery. */
static int WSAAPI EgLookupBeginA(BYTE *q, DWORD flags, HANDLE *h)
{
    const char *name = q && Readable(q, 24) ? *(const char **)(q + 4) : NULL;
    DWORD ns = q && Readable(q, 24) ? *(DWORD *)(q + 20) : 0;
    if (name ? EgNameKind(name) == 2 : (ns != 0 && ns != 12 && ns != 15)) {
        char en[48];
        _snprintf(en, sizeof(en), "(enumeration, name space %lu)", (unsigned long)ns);
        en[sizeof(en) - 1] = 0;
        EgName(0, "WSALookupServiceBegin", name ? name : en, EG_RA());
        WSASetLastError(WSASERVICE_NOT_FOUND);
        return SOCKET_ERROR;
    }
    return g_egLookupBeginA(q, flags, h);
}

static int WSAAPI EgLookupBeginW(BYTE *q, DWORD flags, HANDLE *h)
{
    char a[256] = "";
    const WCHAR *name = q && Readable(q, 24) ? *(const WCHAR **)(q + 4) : NULL;
    DWORD ns = q && Readable(q, 24) ? *(DWORD *)(q + 20) : 0;
    if (name ? EgNameKindW(name, a, sizeof(a)) == 2 : (ns != 0 && ns != 12 && ns != 15)) {
        char en[48];
        _snprintf(en, sizeof(en), "(enumeration, name space %lu)", (unsigned long)ns);
        en[sizeof(en) - 1] = 0;
        EgName(0, "WSALookupServiceBegin", name ? a : en, EG_RA());
        WSASetLastError(WSASERVICE_NOT_FOUND);
        return SOCKET_ERROR;
    }
    return g_egLookupBeginW(q, flags, h);
}

/* ---- dnsapi, wininet, winhttp, netapi32, iphlpapi, shell32 ---------------------------------------- */

static LONG (WINAPI *g_egDnsA)(const char *, WORD, DWORD, void *, void **, void **);
static LONG (WINAPI *g_egDnsW)(const WCHAR *, WORD, DWORD, void *, void **, void **);
static LONG (WINAPI *g_egDnsU)(const char *, WORD, DWORD, void *, void **, void **);
static LONG (WINAPI *g_egDnsEx)(void *, void *, void *);

static LONG WINAPI EgDnsA(const char *name, WORD type, DWORD opt, void *extra, void **res, void **rsv)
{
    /* a literal, "localhost" and this PC's name are answered by Windows itself, without a query on the wire */
    if (EgNameKind(name) != 2) return g_egDnsA(name, type, opt, extra, res, rsv);
    EgName(0, "DnsQuery", name, EG_RA());
    if (res) *res = NULL;
    return 9003; /* DNS_ERROR_RCODE_NAME_ERROR */
}

static LONG WINAPI EgDnsW(const WCHAR *name, WORD type, DWORD opt, void *extra, void **res, void **rsv)
{
    char a[256];
    if (EgNameKindW(name, a, sizeof(a)) != 2) return g_egDnsW(name, type, opt, extra, res, rsv);
    EgName(0, "DnsQuery", a, EG_RA());
    if (res) *res = NULL;
    return 9003;
}

static LONG WINAPI EgDnsU(const char *name, WORD type, DWORD opt, void *extra, void **res, void **rsv)
{
    if (EgNameKind(name) != 2) return g_egDnsU(name, type, opt, extra, res, rsv);
    EgName(0, "DnsQuery", name, EG_RA());
    if (res) *res = NULL;
    return 9003;
}

static LONG WINAPI EgDnsEx(void *req, void *res, void *cancel)
{
    char a[256] = "";
    const WCHAR *name = req && Readable(req, 8) && Readable(*(void **)((BYTE *)req + 4), 2) ? *(const WCHAR **)((BYTE *)req + 4) : NULL;
    if (name && EgNameKindW(name, a, sizeof(a)) != 2) return g_egDnsEx(req, res, cancel);
    EgName(0, "DnsQueryEx", a, EG_RA());
    return 9003;
}

static void *(WINAPI *g_egInetConnectA)(void *, const char *, WORD, const char *, const char *, DWORD, DWORD, DWORD_PTR);
static void *(WINAPI *g_egInetConnectW)(void *, const WCHAR *, WORD, const WCHAR *, const WCHAR *, DWORD, DWORD, DWORD_PTR);
static void *(WINAPI *g_egInetOpenUrlA)(void *, const char *, const char *, DWORD, DWORD, DWORD_PTR);
static void *(WINAPI *g_egInetOpenUrlW)(void *, const WCHAR *, const WCHAR *, DWORD, DWORD, DWORD_PTR);
static BOOL (WINAPI *g_egInetCheckA)(const char *, DWORD, DWORD);
static BOOL (WINAPI *g_egInetCheckW)(const WCHAR *, DWORD, DWORD);
static DWORD (WINAPI *g_egInetAttempt)(DWORD);

static void *WINAPI EgInetConnectA(void *h, const char *server, WORD port, const char *user, const char *pass, DWORD svc, DWORD flags, DWORD_PTR ctx)
{
    if (!EgHostOk(server)) { EgReport(0, "wininet connect", server ? server : "", EG_RA()); SetLastError(12029); return NULL; }
    return g_egInetConnectA(h, server, port, user, pass, svc, flags, ctx);
}

static void *WINAPI EgInetConnectW(void *h, const WCHAR *server, WORD port, const WCHAR *user, const WCHAR *pass, DWORD svc, DWORD flags, DWORD_PTR ctx)
{
    char a[256];
    EgAnsi(server, a, sizeof(a));
    if (!EgHostOk(a)) { EgReport(0, "wininet connect", a, EG_RA()); SetLastError(12029); return NULL; }
    return g_egInetConnectW(h, server, port, user, pass, svc, flags, ctx);
}

static void EgUrlReport(const char *what, const char *url, const void *ra)
{
    char where[200];
    _snprintf(where, sizeof(where), "%.190s", url ? url : "");
    where[sizeof(where) - 1] = 0;
    EgReport(0, what, where, ra);
}

static void *WINAPI EgInetOpenUrlA(void *h, const char *url, const char *hdr, DWORD hdrLen, DWORD flags, DWORD_PTR ctx)
{
    if (EgUrlKind(url) == 2) { EgUrlReport("wininet open url", url, EG_RA()); SetLastError(12029); return NULL; }
    return g_egInetOpenUrlA(h, url, hdr, hdrLen, flags, ctx);
}

static void *WINAPI EgInetOpenUrlW(void *h, const WCHAR *url, const WCHAR *hdr, DWORD hdrLen, DWORD flags, DWORD_PTR ctx)
{
    char a[512];
    EgAnsi(url, a, sizeof(a));
    if (EgUrlKind(a) == 2) { EgUrlReport("wininet open url", a, EG_RA()); SetLastError(12029); return NULL; }
    return g_egInetOpenUrlW(h, url, hdr, hdrLen, flags, ctx);
}

static BOOL WINAPI EgInetCheckA(const char *url, DWORD flags, DWORD rsv)
{
    (void)flags; (void)rsv;
    EgUrlReport("wininet check", url ? url : "(default)", EG_RA());
    SetLastError(12029);
    return FALSE;
}

static BOOL WINAPI EgInetCheckW(const WCHAR *url, DWORD flags, DWORD rsv)
{
    char a[512];
    (void)flags; (void)rsv;
    EgAnsi(url, a, sizeof(a));
    EgUrlReport("wininet check", a[0] ? a : "(default)", EG_RA());
    SetLastError(12029);
    return FALSE;
}

static DWORD WINAPI EgInetAttempt(DWORD rsv)
{
    (void)rsv;
    EgReport(0, "wininet attempt", "(internet connection)", EG_RA());
    return 12029; /* ERROR_INTERNET_CANNOT_CONNECT */
}

static void *(WINAPI *g_egHttpConnect)(void *, const WCHAR *, WORD, DWORD);
static BOOL (WINAPI *g_egHttpProxy)(void *, const WCHAR *, void *, void *);
static DWORD (WINAPI *g_egHttpProxyEx)(void *, const WCHAR *, void *, DWORD_PTR);
static BOOL (WINAPI *g_egHttpDetect)(DWORD, WCHAR **);

static void *WINAPI EgHttpConnect(void *h, const WCHAR *server, WORD port, DWORD rsv)
{
    char a[256];
    EgAnsi(server, a, sizeof(a));
    if (!EgHostOk(a)) { EgReport(0, "winhttp connect", a, EG_RA()); SetLastError(12029); return NULL; }
    return g_egHttpConnect(h, server, port, rsv);
}

static BOOL WINAPI EgHttpProxy(void *h, const WCHAR *url, void *opt, void *info)
{
    char a[512];
    (void)h; (void)opt; (void)info;
    EgAnsi(url, a, sizeof(a));
    if (EgUrlKind(a) != 2) { SetLastError(12180); return FALSE; } /* loopback needs no proxy; nothing is asked */
    EgUrlReport("winhttp proxy lookup", a, EG_RA());
    SetLastError(12180); /* ERROR_WINHTTP_AUTODETECTION_FAILED */
    return FALSE;
}

static DWORD WINAPI EgHttpProxyEx(void *h, const WCHAR *url, void *opt, DWORD_PTR ctx)
{
    char a[512];
    (void)h; (void)opt; (void)ctx;
    EgAnsi(url, a, sizeof(a));
    if (EgUrlKind(a) == 2) EgUrlReport("winhttp proxy lookup", a, EG_RA());
    return 12180;
}

static BOOL WINAPI EgHttpDetect(DWORD flags, WCHAR **url)
{
    (void)flags;
    if (url) *url = NULL;
    EgReport(0, "winhttp proxy detect", "(WPAD)", EG_RA());
    SetLastError(12180);
    return FALSE;
}

static UCHAR (WINAPI *g_egNetbios)(BYTE *);

static UCHAR WINAPI EgNetbios(BYTE *ncb)
{
    EgReport(0, "netbios", "(NetBIOS command)", EG_RA());
    if (ncb && Readable(ncb, 64)) { ncb[1] = 0x34; ncb[49] = 0x34; } /* ncb_retcode / ncb_cmd_cplt = NRC_ENVNOTDEF */
    return 0x34;
}

static DWORD (WINAPI *g_egIcmp)(HANDLE, DWORD, void *, WORD, void *, void *, DWORD, DWORD);
static DWORD (WINAPI *g_egIcmp2)(HANDLE, HANDLE, void *, void *, DWORD, void *, WORD, void *, void *, DWORD, DWORD);
static DWORD (WINAPI *g_egIcmp2Ex)(HANDLE, HANDLE, void *, void *, DWORD, DWORD, void *, WORD, void *, void *, DWORD, DWORD);
static DWORD (WINAPI *g_egIcmp6)(HANDLE, HANDLE, void *, void *, struct sockaddr_in6 *, struct sockaddr_in6 *, void *, WORD, void *, void *, DWORD, DWORD);

static BOOL EgIcmpOk(DWORD dest, const void *ra)
{
    char where[32];
    const BYTE *b = (const BYTE *)&dest;
    if (EgLoopback4(dest)) return TRUE;
    _snprintf(where, sizeof(where), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    where[sizeof(where) - 1] = 0;
    EgReport(0, "icmp echo", where, ra);
    SetLastError(ERROR_NETWORK_UNREACHABLE);
    return FALSE;
}

static DWORD WINAPI EgIcmp(HANDLE h, DWORD dest, void *req, WORD reqLen, void *opt, void *reply, DWORD replyLen, DWORD timeout)
{
    if (!EgIcmpOk(dest, EG_RA())) return 0;
    return g_egIcmp(h, dest, req, reqLen, opt, reply, replyLen, timeout);
}

static DWORD WINAPI EgIcmp2(HANDLE h, HANDLE ev, void *apc, void *ctx, DWORD dest, void *req, WORD reqLen, void *opt, void *reply, DWORD replyLen, DWORD timeout)
{
    if (!EgIcmpOk(dest, EG_RA())) return 0;
    return g_egIcmp2(h, ev, apc, ctx, dest, req, reqLen, opt, reply, replyLen, timeout);
}

static DWORD WINAPI EgIcmp2Ex(HANDLE h, HANDLE ev, void *apc, void *ctx, DWORD src, DWORD dest, void *req, WORD reqLen, void *opt, void *reply,
                              DWORD replyLen, DWORD timeout)
{
    if (!EgIcmpOk(dest, EG_RA())) return 0;
    return g_egIcmp2Ex(h, ev, apc, ctx, src, dest, req, reqLen, opt, reply, replyLen, timeout);
}

static DWORD WINAPI EgIcmp6(HANDLE h, HANDLE ev, void *apc, void *ctx, struct sockaddr_in6 *src, struct sockaddr_in6 *dest, void *req, WORD reqLen,
                            void *opt, void *reply, DWORD replyLen, DWORD timeout)
{
    if (!dest || !EgLoopback6((const BYTE *)&dest->sin6_addr)) {
        EgReport(0, "icmp echo", "(IPv6)", EG_RA());
        SetLastError(ERROR_NETWORK_UNREACHABLE);
        return 0;
    }
    return g_egIcmp6(h, ev, apc, ctx, src, dest, req, reqLen, opt, reply, replyLen, timeout);
}

static HINSTANCE (WINAPI *g_egShellA)(HWND, const char *, const char *, const char *, const char *, int);
static HINSTANCE (WINAPI *g_egShellW)(HWND, const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *, int);
static BOOL (WINAPI *g_egShellExA)(SHELLEXECUTEINFOA *);
static BOOL (WINAPI *g_egShellExW)(SHELLEXECUTEINFOW *);

static HINSTANCE WINAPI EgShellA(HWND w, const char *op, const char *file, const char *par, const char *dir, int show)
{
    if (EgUrlKind(file) == 2) { EgUrlReport("open in browser", file, EG_RA()); SetLastError(ERROR_ACCESS_DENIED); return (HINSTANCE)(INT_PTR)SE_ERR_ACCESSDENIED; }
    return g_egShellA(w, op, file, par, dir, show);
}

static HINSTANCE WINAPI EgShellW(HWND w, const WCHAR *op, const WCHAR *file, const WCHAR *par, const WCHAR *dir, int show)
{
    char a[512];
    EgAnsi(file, a, sizeof(a));
    if (EgUrlKind(a) == 2) { EgUrlReport("open in browser", a, EG_RA()); SetLastError(ERROR_ACCESS_DENIED); return (HINSTANCE)(INT_PTR)SE_ERR_ACCESSDENIED; }
    return g_egShellW(w, op, file, par, dir, show);
}

static BOOL WINAPI EgShellExA(SHELLEXECUTEINFOA *i)
{
    if (i && Readable(i, 24) && i->lpFile && EgUrlKind(i->lpFile) == 2) {
        EgUrlReport("open in browser", i->lpFile, EG_RA());
        i->hInstApp = (HINSTANCE)(INT_PTR)SE_ERR_ACCESSDENIED;
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    return g_egShellExA(i);
}

static BOOL WINAPI EgShellExW(SHELLEXECUTEINFOW *i)
{
    char a[512];
    if (i && Readable(i, 24) && i->lpFile) {
        EgAnsi(i->lpFile, a, sizeof(a));
        if (EgUrlKind(a) == 2) {
            EgUrlReport("open in browser", a, EG_RA());
            i->hInstApp = (HINSTANCE)(INT_PTR)SE_ERR_ACCESSDENIED;
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
    }
    return g_egShellExW(i);
}

/* ---- layer B: the AFD backstop -------------------------------------------------------------------- */

typedef struct { DWORD code; BYTE kind; WORD off; } EgAfd; /* kind 1: sockaddr inside the input buffer; 2: a pointer to it */
static EgAfd g_egAfd[8];
static volatile LONG g_egAfdCount;
static volatile DWORD g_egLearnThread;
static BYTE g_egLearnAddr[8];

static EG_NTSTATUS (NTAPI *g_egNtIoctl)(HANDLE, HANDLE, void *, void *, void *, ULONG, void *, ULONG, void *, ULONG);

static void EgAfdLearn(DWORD code, const BYTE *in, ULONG inLen)
{
    for (LONG i = 0; i < g_egAfdCount; i++) if (g_egAfd[i].code == code) return;
    if (g_egAfdCount >= (LONG)(sizeof(g_egAfd) / sizeof(g_egAfd[0])) || !in || inLen < 8 || inLen > 4096) return;
    for (ULONG off = 0; off + 8 <= inLen; off += 2)
        if (!memcmp(in + off, g_egLearnAddr, 8)) { EgAfd e = { code, 1, (WORD)off }; g_egAfd[g_egAfdCount] = e; InterlockedIncrement(&g_egAfdCount); return; }
    for (ULONG off = 0; off + 4 <= inLen; off += 4) {
        const void *p = *(const void *const *)(in + off);
        if ((DWORD_PTR)p > 0x10000 && Readable(p, 8) && !memcmp(p, g_egLearnAddr, 8)) {
            EgAfd e = { code, 2, (WORD)off };
            g_egAfd[g_egAfdCount] = e;
            InterlockedIncrement(&g_egAfdCount);
            return;
        }
    }
}

static EG_NTSTATUS NTAPI EgNtIoctl(HANDLE file, HANDLE ev, void *apc, void *apcCtx, void *iosb, ULONG code, void *in, ULONG inLen, void *out, ULONG outLen)
{
    if ((code & 0xFFFFF000u) == 0x00012000u && in) {
        if (g_egLearnThread && g_egLearnThread == GetCurrentThreadId()) {
            EgAfdLearn(code, (const BYTE *)in, inLen);
        } else {
            for (LONG i = 0; i < g_egAfdCount; i++) {
                if (g_egAfd[i].code != code) continue;
                BYTE sa[28];
                SIZE_T got = 0;
                memset(sa, 0, sizeof(sa));
                if (g_egAfd[i].kind == 1) {
                    if (g_egAfd[i].off + 8u > inLen) break;
                    got = inLen - g_egAfd[i].off;
                    if (got > sizeof(sa)) got = sizeof(sa);
                    memcpy(sa, (const BYTE *)in + g_egAfd[i].off, got);
                } else {
                    if (g_egAfd[i].off + 4u > inLen) break;
                    const void *p = *(const void *const *)((const BYTE *)in + g_egAfd[i].off);
                    if (!p) break; /* a send on a connected socket carries no address */
                    if (!ReadProcessMemory(GetCurrentProcess(), p, sa, sizeof(sa), &got) &&
                        !ReadProcessMemory(GetCurrentProcess(), p, sa, 16, &got)) break;
                }
                const struct sockaddr *a = (const struct sockaddr *)sa;
                if ((a->sa_family == AF_INET || a->sa_family == AF_INET6) && !EgAddrOk(a, (int)got)) {
                    char where[96];
                    EgAddrText(a, (int)got, where, sizeof(where));
                    EgReport(3, "afd ioctl", where, EG_RA());
                    if (iosb && Readable(iosb, 8)) { ((DWORD *)iosb)[0] = (DWORD)EG_STATUS_NETWORK_UNREACHABLE; ((DWORD *)iosb)[1] = 0; }
                    return EG_STATUS_NETWORK_UNREACHABLE;
                }
                break;
            }
        }
    }
    return g_egNtIoctl(file, ev, apc, apcCtx, iosb, code, in, inLen, out, outLen);
}

/* ---- installing ----------------------------------------------------------------------------------- */

#define EG(dll, name, hook, real, req) { dll, name, (void *)hook, (void **)(void *)&real, req, 0, NULL }
static EgHook g_egHooks[] = {
    EG("ws2_32.dll", "connect", EgConnect, g_egConnect, TRUE),
    EG("ws2_32.dll", "WSAConnect", EgWSAConnect, g_egWSAConnect, TRUE),
    EG("ws2_32.dll", "sendto", EgSendTo, g_egSendTo, TRUE),
    EG("ws2_32.dll", "WSASendTo", EgWSASendTo, g_egWSASendTo, TRUE),
    EG("ws2_32.dll", "WSAIoctl", EgWSAIoctl, g_egWSAIoctl, TRUE),
    EG("ws2_32.dll", "gethostbyname", EgGetHostByName, g_egGetHostByName, TRUE),
    EG("ws2_32.dll", "getaddrinfo", EgGetAddrInfo, g_egGetAddrInfo, TRUE),
    EG("ws2_32.dll", "bind", EgBind, g_egBind, TRUE),
    EG("ws2_32.dll", "setsockopt", EgSetSockOpt, g_egSetSockOpt, TRUE),
    EG("ws2_32.dll", "WSASendMsg", EgWSASendMsg, g_egWSASendMsg, FALSE),
    EG("ws2_32.dll", "WSAConnectByNameA", EgConnByNameA, g_egConnByNameA, FALSE),
    EG("ws2_32.dll", "WSAConnectByNameW", EgConnByNameW, g_egConnByNameW, FALSE),
    EG("ws2_32.dll", "WSAConnectByList", EgConnByList, g_egConnByList, FALSE),
    EG("ws2_32.dll", "WSAJoinLeaf", EgJoinLeaf, g_egJoinLeaf, FALSE),
    EG("ws2_32.dll", "gethostbyaddr", EgGetHostByAddr, g_egGetHostByAddr, FALSE),
    EG("ws2_32.dll", "GetAddrInfoW", EgGetAddrInfoW, g_egGetAddrInfoW, FALSE),
    EG("ws2_32.dll", "GetAddrInfoExA", EgGetAddrInfoExA, g_egGetAddrInfoExA, FALSE),
    EG("ws2_32.dll", "GetAddrInfoExW", EgGetAddrInfoExW, g_egGetAddrInfoExW, FALSE),
    EG("ws2_32.dll", "getnameinfo", EgGetNameInfo, g_egGetNameInfo, FALSE),
    EG("ws2_32.dll", "GetNameInfoW", EgGetNameInfoW, g_egGetNameInfoW, FALSE),
    EG("ws2_32.dll", "WSAAsyncGetHostByName", EgAsyncByName, g_egAsyncByName, FALSE),
    EG("ws2_32.dll", "WSAAsyncGetHostByAddr", EgAsyncByAddr, g_egAsyncByAddr, FALSE),
    EG("ws2_32.dll", "WSALookupServiceBeginA", EgLookupBeginA, g_egLookupBeginA, FALSE),
    EG("ws2_32.dll", "WSALookupServiceBeginW", EgLookupBeginW, g_egLookupBeginW, FALSE),
    EG("ntdll.dll", "NtDeviceIoControlFile", EgNtIoctl, g_egNtIoctl, FALSE),
    EG("dnsapi.dll", "DnsQuery_A", EgDnsA, g_egDnsA, FALSE),
    EG("dnsapi.dll", "DnsQuery_W", EgDnsW, g_egDnsW, FALSE),
    EG("dnsapi.dll", "DnsQuery_UTF8", EgDnsU, g_egDnsU, FALSE),
    EG("dnsapi.dll", "DnsQueryEx", EgDnsEx, g_egDnsEx, FALSE),
    EG("wininet.dll", "InternetConnectA", EgInetConnectA, g_egInetConnectA, FALSE),
    EG("wininet.dll", "InternetConnectW", EgInetConnectW, g_egInetConnectW, FALSE),
    EG("wininet.dll", "InternetOpenUrlA", EgInetOpenUrlA, g_egInetOpenUrlA, FALSE),
    EG("wininet.dll", "InternetOpenUrlW", EgInetOpenUrlW, g_egInetOpenUrlW, FALSE),
    EG("wininet.dll", "InternetCheckConnectionA", EgInetCheckA, g_egInetCheckA, FALSE),
    EG("wininet.dll", "InternetCheckConnectionW", EgInetCheckW, g_egInetCheckW, FALSE),
    EG("wininet.dll", "InternetAttemptConnect", EgInetAttempt, g_egInetAttempt, FALSE),
    EG("winhttp.dll", "WinHttpConnect", EgHttpConnect, g_egHttpConnect, FALSE),
    EG("winhttp.dll", "WinHttpGetProxyForUrl", EgHttpProxy, g_egHttpProxy, FALSE),
    EG("winhttp.dll", "WinHttpGetProxyForUrlEx", EgHttpProxyEx, g_egHttpProxyEx, FALSE),
    EG("winhttp.dll", "WinHttpDetectAutoProxyConfigUrl", EgHttpDetect, g_egHttpDetect, FALSE),
    EG("netapi32.dll", "Netbios", EgNetbios, g_egNetbios, FALSE),
    EG("iphlpapi.dll", "IcmpSendEcho", EgIcmp, g_egIcmp, FALSE),
    EG("iphlpapi.dll", "IcmpSendEcho2", EgIcmp2, g_egIcmp2, FALSE),
    EG("iphlpapi.dll", "IcmpSendEcho2Ex", EgIcmp2Ex, g_egIcmp2Ex, FALSE),
    EG("iphlpapi.dll", "Icmp6SendEcho2", EgIcmp6, g_egIcmp6, FALSE),
    EG("shell32.dll", "ShellExecuteA", EgShellA, g_egShellA, FALSE),
    EG("shell32.dll", "ShellExecuteW", EgShellW, g_egShellW, FALSE),
    EG("shell32.dll", "ShellExecuteExA", EgShellExA, g_egShellExA, FALSE),
    EG("shell32.dll", "ShellExecuteExW", EgShellExW, g_egShellExW, FALSE),
};
#undef EG
#define EG_HOOKS ((int)(sizeof(g_egHooks) / sizeof(g_egHooks[0])))

/* An export by name, read from the image (usable while the module is still being loaded). Forwarders: NULL.
 * slot receives the address of the export table's RVA entry. */
static void *EgExport(HMODULE mod, const char *name, DWORD **slot)
{
    BYTE *base = (BYTE *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir->VirtualAddress || !dir->Size) return NULL;
    IMAGE_EXPORT_DIRECTORY *ex = (IMAGE_EXPORT_DIRECTORY *)(base + dir->VirtualAddress);
    DWORD *names = (DWORD *)(base + ex->AddressOfNames), *funcs = (DWORD *)(base + ex->AddressOfFunctions);
    WORD *ords = (WORD *)(base + ex->AddressOfNameOrdinals);
    for (DWORD i = 0; i < ex->NumberOfNames; i++) {
        if (lstrcmpA((const char *)(base + names[i]), name)) continue;
        DWORD rva = funcs[ords[i]];
        if (rva >= dir->VirtualAddress && rva < dir->VirtualAddress + dir->Size) return NULL;
        if (slot) *slot = &funcs[ords[i]];
        return base + rva;
    }
    return NULL;
}

static void EgPatch(void *dst, const void *src, SIZE_T n)
{
    DWORD old;
    if (!VirtualProtect(dst, n, PAGE_EXECUTE_READWRITE, &old)) return;
    memcpy(dst, src, n);
    VirtualProtect(dst, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), dst, n);
}

/* Replace `real` by `hook` in one module's import table (the fallback for a function that cannot be hooked inline). */
static void EgSweepImports(HMODULE mod, void *real, void *hook)
{
    BYTE *base = (BYTE *)mod;
    if (!Readable(base, 0x400) || ((IMAGE_DOS_HEADER *)base)->e_magic != IMAGE_DOS_SIGNATURE) return;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir->VirtualAddress || !dir->Size) return;
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress); d->Name && d->FirstThunk; d++)
        for (void **t = (void **)(base + d->FirstThunk); Readable(t, sizeof(*t)) && *t; t++)
            if (*t == real) EgPatch(t, &hook, sizeof(hook));
}

static BOOL EgInstallOne(EgHook *h, HMODULE mod)
{
    DWORD *slot = NULL;
    BYTE *f = (BYTE *)EgExport(mod, h->name, &slot);
    if (!f) return FALSE;
    if (g_egTrampUsed >= EG_MAX_HOOKS) return FALSE;
    int n = 0;
    if (f[0] == 0x8B && f[1] == 0xFF && f[2] == 0x55 && f[3] == 0x8B && f[4] == 0xEC) n = 5;      /* mov edi,edi; push ebp; mov ebp,esp */
    else if (f[0] == 0x6A && f[2] == 0x68) n = 7;                                                  /* push imm8; push imm32 (SEH prolog) */
    else if (f[0] == 0xB8 && !lstrcmpiA(h->dll, "ntdll.dll")) n = 5;                              /* mov eax, <service number> */
    h->target = f;
    if (n) {
        BYTE *t = g_egTramp + g_egTrampUsed++ * EG_TRAMP_SIZE;
        BYTE jmp[7] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90 };
        memcpy(t, f, n);
        t[n] = 0xE9;
        *(DWORD *)(t + n + 1) = (DWORD)((f + n) - (t + n + 5));
        *h->real = t;
        *(DWORD *)(jmp + 1) = (DWORD)((BYTE *)h->hook - (f + 5));
        EgPatch(f, jmp, n);
        h->state = 1;
        return f[0] == 0xE9;
    }
    /* Unknown first bytes (another hook is already there, or a Windows build laid the function out differently):
     * the export table, so that every later lookup gets the gated function, and every import table loaded so far. */
    if (!slot) return FALSE;
    *h->real = f;
    DWORD rva = (DWORD)((BYTE *)h->hook - (BYTE *)mod);
    EgPatch(slot, &rva, sizeof(rva));
    h->state = 2;
    return TRUE;
}

static void EgSweepAll(void *real, void *hook);

static BOOL EgInstallFor(HMODULE mod, const char *dllName)
{
    BOOL ok = TRUE;
    for (int i = 0; i < EG_HOOKS; i++) {
        EgHook *h = &g_egHooks[i];
        if (h->state || lstrcmpiA(h->dll, dllName)) continue;
        if (!EgInstallOne(h, mod)) {
            if (h->required) ok = FALSE;
            Log("egress: %s!%s could not be hooked%s", h->dll, h->name, h->required ? " (REQUIRED)" : "");
        } else if (h->state == 2) {
            EgSweepAll(h->target, h->hook);
            Log("egress: %s!%s hooked through the export and import tables (unknown first bytes)", h->dll, h->name);
        }
    }
    return ok;
}

typedef struct { USHORT Length, MaximumLength; WCHAR *Buffer; } EgUnicode;
typedef struct { ULONG Flags; const EgUnicode *FullDllName; const EgUnicode *BaseDllName; void *DllBase; ULONG SizeOfImage; } EgLdrData;
typedef struct EgLdrEntry { LIST_ENTRY InLoadOrder, InMemoryOrder, InInitOrder; void *DllBase, *EntryPoint; ULONG SizeOfImage; EgUnicode FullDllName, BaseDllName; } EgLdrEntry;

/* Every module of the process, from the loader's own list (no snapshot API: usable under the loader lock). */
static void EgSweepAll(void *real, void *hook)
{
    BYTE *peb;
    __asm__("movl %%fs:0x30, %0" : "=r"(peb));
    BYTE *ldr = *(BYTE **)(peb + 0x0C);
    LIST_ENTRY *head = (LIST_ENTRY *)(ldr + 0x0C);
    for (LIST_ENTRY *e = head->Flink; e && e != head; e = e->Flink) {
        EgLdrEntry *m = (EgLdrEntry *)e;
        if (m->DllBase && m->DllBase != (void *)g_self) EgSweepImports((HMODULE)m->DllBase, real, hook);
    }
}

static void CALLBACK EgDllNotify(ULONG reason, const EgLdrData *data, void *ctx)
{
    (void)ctx;
    if (reason != 1 || !data || !data->BaseDllName || !data->BaseDllName->Buffer) return;
    char name[96];
    int n = WideCharToMultiByte(CP_ACP, 0, data->BaseDllName->Buffer, data->BaseDllName->Length / 2, name, sizeof(name) - 1, NULL, NULL);
    name[n > 0 ? n : 0] = 0;
    EgInstallFor((HMODULE)data->DllBase, name);
    for (int i = 0; i < EG_HOOKS; i++) /* the new module's imports of a function hooked through the tables */
        if (g_egHooks[i].state == 2) EgSweepImports((HMODULE)data->DllBase, g_egHooks[i].target, g_egHooks[i].hook);
#if CODOL_DIAGNOSTICS
    {
        char full[MAX_PATH] = "";
        if (data->FullDllName && data->FullDllName->Buffer)
            WideCharToMultiByte(CP_ACP, 0, data->FullDllName->Buffer, data->FullDllName->Length / 2, full, sizeof(full) - 1, NULL, NULL);
        if (!g_egWinDir[0] || _strnicmp(full, g_egWinDir, lstrlenA(g_egWinDir))) Log("egress: module loaded: %s", full);
    }
#endif
}

/* DllMain (process attach): hook what is loaded, watch for the rest. FALSE = a required function is not gated. */
static BOOL EgressGateAttach(void)
{
    static const char *const kNow[] = { "ws2_32.dll", "dnsapi.dll", "wininet.dll", "winhttp.dll", "netapi32.dll", "iphlpapi.dll", "shell32.dll" };
    InitializeCriticalSection(&g_egLock);
    DWORD n = GetModuleFileNameA(NULL, g_egLogPath, MAX_PATH);
    while (n && g_egLogPath[n - 1] != '\\') n--;
    lstrcpyA(g_egLogPath + n, "Kernel33_egress.log");
    UINT wl = GetWindowsDirectoryA(g_egWinDir, sizeof(g_egWinDir) - 2);
    if (!wl || wl >= sizeof(g_egWinDir) - 2) g_egWinDir[0] = 0;
    else lstrcatA(g_egWinDir, "\\");
    DWORD cn = sizeof(g_egOwnName);
    if (!GetComputerNameExA(ComputerNameDnsHostname, g_egOwnName, &cn)) g_egOwnName[0] = 0;

    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesExA(g_egLogPath, GetFileExInfoStandard, &fa) && (fa.nFileSizeHigh || fa.nFileSizeLow > EG_LOG_MAX)) DeleteFileA(g_egLogPath);
    SYSTEMTIME st;
    GetLocalTime(&st);
    char line[256];
    _snprintf(line, sizeof(line), "---- Kernel33 egress gate, pid %lu, %04u-%02u-%02u %02u:%02u:%02u. Only loopback (127.0.0.0/8, ::1) can be reached; "
              "one line per distinct destination.", GetCurrentProcessId(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    line[sizeof(line) - 1] = 0;
    EgWrite(line);

    g_egTramp = (BYTE *)VirtualAlloc(NULL, EG_MAX_HOOKS * EG_TRAMP_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_egTramp) { g_egFailed = TRUE; return FALSE; }
    BOOL ok = TRUE;
    HMODULE ws2 = GetModuleHandleA("ws2_32.dll");
    if (!ws2) ws2 = LoadLibraryA("ws2_32.dll");
    if (!ws2) ok = FALSE;
    for (unsigned i = 0; i < sizeof(kNow) / sizeof(kNow[0]); i++) {
        HMODULE m = GetModuleHandleA(kNow[i]);
        if (m && !EgInstallFor(m, kNow[i])) ok = FALSE;
    }
    for (int i = 0; i < EG_HOOKS; i++) if (g_egHooks[i].required && !g_egHooks[i].state) ok = FALSE;
    LONG (NTAPI *reg)(ULONG, void *, void *, void **) =
        (LONG (NTAPI *)(ULONG, void *, void *, void **))(void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "LdrRegisterDllNotification");
    if (!reg || reg(0, (void *)EgDllNotify, NULL, &g_egCookie) != 0) {
        /* Without the notification a DLL loaded later would keep its HTTP entry points; Winsock is hooked already. */
        Log("egress: no DLL load notification; loading the HTTP stacks now");
        static const char *const kLater[] = { "dnsapi.dll", "wininet.dll", "winhttp.dll", "netapi32.dll", "iphlpapi.dll", "shell32.dll" };
        for (unsigned i = 0; i < sizeof(kLater) / sizeof(kLater[0]); i++) {
            HMODULE m = LoadLibraryA(kLater[i]);
            if (m) EgInstallFor(m, kLater[i]);
        }
    }
    g_egReady = ok;
    g_egFailed = !ok;
    if (!ok) EgWrite("THE GATE COULD NOT BE INSTALLED: the client does not start.");
    return ok;
}

/* TakeOver, before C1's entry point: the backstop learns where each AFD ioctl carries its destination. */
static BOOL EgressGateArm(void)
{
    if (!g_egReady || g_egFailed) return FALSE;
    int inl = 0, tab = 0;
    for (int i = 0; i < EG_HOOKS; i++) { inl += g_egHooks[i].state == 1; tab += g_egHooks[i].state == 2; }
    WSADATA wsa;
    EgInstallFor(GetModuleHandleA("ntdll.dll"), "ntdll.dll"); /* here, not in DllMain: after the packer stub has run */
    inl += g_egNtIoctl != NULL;
    if (!g_egNtIoctl || WSAStartup(MAKEWORD(2, 2), &wsa)) {
        Log("egress: gate on (%d inline, %d through tables); backstop unavailable", inl, tab);
        EgWrite("gate on; the AFD backstop is unavailable on this system (layer A alone)");
        return TRUE;
    }
    struct sockaddr_in to;
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(0x3321);
    to.sin_addr.s_addr = inet_addr("127.77.33.1");
    memcpy(g_egLearnAddr, &to, 8);
    g_egLearnThread = GetCurrentThreadId();
    SOCKET u = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (u != INVALID_SOCKET) {
        char one = 0;
        WSABUF wb = { 1, &one };
        DWORD sent = 0;
        g_egSendTo(u, &one, 1, 0, (const struct sockaddr *)&to, sizeof(to));
        if (g_egWSASendMsg) {
            WSAMSG msg;
            memset(&msg, 0, sizeof(msg));
            msg.name = (struct sockaddr *)&to;
            msg.namelen = sizeof(to);
            msg.lpBuffers = &wb;
            msg.dwBufferCount = 1;
            g_egWSASendMsg(u, &msg, 0, &sent, NULL, NULL);
        }
        g_egConnect(u, (const struct sockaddr *)&to, sizeof(to));
        closesocket(u);
    }
    SOCKET t = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (t != INVALID_SOCKET) {
        u_long nb = 1;
        ioctlsocket(t, FIONBIO, &nb);
        g_egConnect(t, (const struct sockaddr *)&to, sizeof(to));
        closesocket(t);
    }
    t = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (t != INVALID_SOCKET) {
        static const GUID kConnectEx = { 0x25a207b9, 0xddf3, 0x4660, { 0x8e, 0xe9, 0x76, 0xe5, 0x8c, 0x74, 0x06, 0x3e } };
        void *cx = NULL;
        DWORD got = 0;
        struct sockaddr_in any;
        memset(&any, 0, sizeof(any));
        any.sin_family = AF_INET;
        any.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (!g_egBind(t, (const struct sockaddr *)&any, sizeof(any)) &&
            !g_egWSAIoctl(t, 0xC8000006u, (void *)&kConnectEx, sizeof(kConnectEx), &cx, sizeof(cx), &got, NULL, NULL) && cx) {
            OVERLAPPED ov;
            memset(&ov, 0, sizeof(ov));
            EnterCriticalSection(&g_egLock);
            if (!g_egConnectEx) g_egConnectEx = (BOOL (PASCAL *)(SOCKET, const struct sockaddr *, int, void *, DWORD, DWORD *, OVERLAPPED *))cx;
            LeaveCriticalSection(&g_egLock);
            ((BOOL (PASCAL *)(SOCKET, const struct sockaddr *, int, void *, DWORD, DWORD *, OVERLAPPED *))cx)(t, (const struct sockaddr *)&to, sizeof(to), NULL, 0, NULL, &ov);
            CancelIo((HANDLE)t);
        }
        closesocket(t);
    }
    g_egLearnThread = 0;
    char line[256];
    int n = _snprintf(line, sizeof(line), "gate on: %d functions hooked inline, %d through tables; backstop armed on %ld AFD ioctl(s):", inl, tab, g_egAfdCount);
    for (LONG i = 0; i < g_egAfdCount && n > 0 && n < (int)sizeof(line) - 24; i++)
        n += _snprintf(line + n, sizeof(line) - n, " 0x%lX%s+%u", (unsigned long)g_egAfd[i].code, g_egAfd[i].kind == 1 ? "" : "*", g_egAfd[i].off);
    line[sizeof(line) - 1] = 0;
    EgWrite(line);
    Log("egress: %s", line);
    return TRUE;
}

/* DllMain (process detach). */
static void EgressGateSummary(void)
{
    if (!g_egLogPath[0]) return;
    char line[256];
    _snprintf(line, sizeof(line), "---- end of run: %ld attempt(s) to leave loopback refused (%ld distinct, listed above), %ld at the backstop; "
              "%ld loopback operation(s) passed; %ld bind(s) moved to loopback.",
              g_egRefused + g_egBackstop, g_egRefusedLines, g_egBackstop, g_egPassed, g_egMoved);
    line[sizeof(line) - 1] = 0;
    EgWrite(line);
}
