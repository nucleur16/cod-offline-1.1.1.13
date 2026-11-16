/*
 * p2p_inproc.c - the sh1x match server inside the host's client, on the game thread (feat/p2p-inprocess,
 * feat/server-in-kernel33).
 *
 * Mode `inprocess` of p2p.c (the default; `mode = child` in p2p_host.ini runs sh1x_dedrun.exe as before). The
 * server is linked into Kernel33.dll: the sh1x server repo's `host-lib` target (bin/libsh1x_host.a), the same
 * objects as the dedicated exe plus its host API (win32/sys_host.c: Sh1xHost_Init / Frame / Command / ExitCode),
 * called directly (p2p_server_api.h). Like a listen server, C1's game thread runs it: every pass of C1's
 * Com_EventLoop (the call at 0x595E9D, chained behind voice and discord) runs one server Com_Frame, which returns
 * at once unless a server frame is due (the host API makes NET_Sleep poll). There is no second process, no
 * second image and no window.
 *
 * One server per client run, started when the player first hosts and kept for the rest of the run:
 *  - The first match starts it (Sh1xHost_Init with the full command line, +map). Its large state (svs and the
 *    CoDOL snapshot rings, about 200 MB) is allocated then (server sys_ondemand.h), not held in Kernel33.dll's
 *    image, so a client that never hosts reserves only the rest of the server's .bss.
 *  - A match ends without ending the server: when its player left the finished level (offline: one match per
 *    session), the level simply stays loaded, as a one-shot lobby worker keeps it; a stop (p2pstop, a hung or
 *    timed-out start) unloads it with `killserver`. Either way the server keeps its sockets, its port and its
 *    threads, and is still ticked.
 *  - The next match is the same server: its settings (codol_zoneType, codol_sessionMap, g_gametype,
 *    g_difficulty) and `map <map>` are queued with Sh1xHost_Command, as the lobby retargets a warm dedicated
 *    worker. The server is never initialised twice: its globals are spent once it has ended.
 *  - Only an end of the server itself (a quit, a Sys_Error: Sh1xHost_Frame returns 0) is final for the run:
 *    hosting again then needs a client restart (P2PInprocUsed).
 *
 * Process sharing:
 *  - Heap: the server allocates through msvcrt.dll (the same CRT as Kernel33.dll; C1 has its own static CRT
 *    heap) and VirtualAlloc (hunk, zone memory). Its symbols are all local but the host API (server
 *    tools/sh1x_host_lib_symbols.py), so its zlib, its __cxa_atexit and the rest cannot clash with Kernel33's.
 *  - DllMain: the server has one static constructor (an empty std::vector in stringed_ingame.cpp) and no static
 *    TLS; everything else happens in Sh1xHost_Init, on the first host, never under the loader lock.
 *  - Stack: C1's main thread reserves 640 KB; the dedicated server links with 8 MB, and one server frame
 *    overflowed a 2 MB stack in a harness (0xC00000FD). The server runs on its own fiber (32 MB reserve) on the
 *    game thread, switched to and from synchronously.
 *  - Floating point: the fiber is created with FIBER_FLAG_FLOAT_SWITCH, so the server's x87/SSE control words
 *    (Sys_SetFloatEnv) stay on its fiber; the tick also restores the client's control word and MXCSR.
 *  - Exits: the host API turns every engine exit (quit, Sys_Error, the watchdogs) into an end of the server
 *    (a longjmp back into Sh1xHost_Frame); the client keeps running.
 *  - Faults are not contained: a fault in the server is a fault of the client, with Kernel33's and C1's normal
 *    crash logging and dump (the server's code is in Kernel33.dll, so logged offsets resolve against one image).
 *    The server never installs its own crash filter (only its WinMain does).
 *  - Address space: codoMP_client_shipRetail.exe already has IMAGE_FILE_LARGE_ADDRESS_AWARE (characteristics
 *    0x122), so the process has 4 GB on 64-bit Windows; client plus server measured below that.
 *
 * Not contained: a server frame or map load that runs long holds the client's frame (a map load is one server
 * frame, about 2-8 s); an endless loop in the server hangs the client.
 */

#include <psapi.h>
#include "p2p_server_api.h"

typedef int (__cdecl *Sh1xCodeFn)(void);

/* g_piState is the state of the current match (session); g_piServer the state of the server for the run. */
enum { PI_NONE, PI_INIT_PENDING, PI_RUNNING, PI_ENDED, PI_CRASHED };
static const char *const kPiState[] = { "none", "starting", "running", "ended", "crashed" };
enum { PS_NONE, PS_RUNNING, PS_ENDED };
static const char *const kPiServer[] = { "not started", "running", "ended" };

static volatile LONG g_piState;
static volatile LONG g_piServer;
static volatile LONG g_piQuit;        /* 1: end the match; 3: a stop, players told to leave, unload next; 2: done */
static volatile LONG g_piMatchOver;   /* the hosted match ended and its player left: p2p.c re-arms the room */
static Sh1xCodeFn g_piCode = Sh1xHost_ExitCode;
static void *g_piMainFiber, *g_piFiber;
static DWORD g_piThread;
static char g_piArgs[8192];           /* the first match: the server's command line */
static char g_piRetarget[1024];       /* a later match: the commands that move the running server to it */
static BOOL g_piRetargetSent;         /* the fiber has queued g_piRetarget for the pending match */
static unsigned g_piRetargetSerial;   /* Sh1xHost_LevelSerial before it: the new level is up once it moved */
static DWORD g_piRetargetTick, g_piLobbyTick;
static int g_piLobbyTold;             /* players sent back to the lobby by a stop (g_piQuit 3) */
static int g_piInitResult;
static DWORD g_piInitMs, g_piFirstFrameMs, g_piServerStartTick;
static volatile DWORD g_piFaultCode;  /* no fault is contained any more: always 0 */
/* The assembly tail jump reads this even when shipping removes Log(). */
static DWORD_PTR g_piNext __attribute__((used)); /* prior Com_EventLoop target */
static BOOL g_piHooked;
static BOOL PiProductCurrent(void);
static LONG g_piSession;
static P2PConfig g_piServerCfg;       /* what the running server was started with */
/* frame cost on the game thread */
static LONGLONG g_piQpf;
static volatile LONGLONG g_piCalls, g_piTotalUs, g_piMaxUs, g_piWinCalls, g_piWinTotalUs, g_piWinMaxUs;

/* GetCurrentFiber without the intrinsic's -Warray-bounds noise: TEB NtTib.FiberData. */
static void *PiCurrentFiber(void)
{
    void *f;
    __asm__ volatile("movl %%fs:0x10, %0" : "=r"(f));
    return f;
}

/* The game thread ticks the server from its first start to its end, between matches too. */
static BOOL PiServerTicked(void)
{
    return g_piServer == PS_RUNNING || (g_piServer == PS_NONE && g_piState == PI_INIT_PENDING);
}

/* ---- the server fiber ------------------------------------------------------------------------------- */

/* A call into the server's API made from C1's side (p2passetcheck) runs on the server's fiber, between frames,
 * like a frame: its stack and its error longjmps. */
static void (*volatile g_piCallFn)(void *);
static void *g_piCallCtx;

static void PiServerEnded(void)
{
    InterlockedExchange(&g_piServer, PS_ENDED);
    InterlockedExchange(&g_piState, PI_ENDED);
}

static void WINAPI PiServerFiber(void *unused)
{
    (void)unused;
    for (;;) {
        LONG st = g_piState;
        if (g_piCallFn) {
            void (*fn)(void *) = g_piCallFn;
            fn(g_piCallCtx);
            g_piCallFn = NULL;
        } else if (g_piServer == PS_NONE && st == PI_INIT_PENDING) {
            /* The first match of the run starts the server. */
            DWORD t = GetTickCount();
            g_piInitResult = Sh1xHost_Init(g_piArgs);
            g_piInitMs = GetTickCount() - t;
            g_piServerStartTick = GetTickCount();
            if (g_piInitResult == 0) {
                InterlockedExchange(&g_piServer, PS_RUNNING);
                InterlockedExchange(&g_piState, PI_RUNNING);
                /* The +map on the command line loads in the first frame: one long frame now, while the host
                 * has just asked to host, rather than at an arbitrary later client frame. */
                t = GetTickCount();
                if (!Sh1xHost_Frame()) PiServerEnded();
                g_piFirstFrameMs = GetTickCount() - t;
            } else {
                PiServerEnded();
            }
        } else if (g_piServer == PS_RUNNING) {
            BOOL endMatch = FALSE, alive = TRUE;
            if (st == PI_INIT_PENDING) {
                /* A later match: the running server moves to it. The match is running (and its status can be
                 * trusted) only once the server has spawned a new level: the replaced one may be the same map,
                 * finished, and a map change can wait on the previous match's report for a few frames. */
                if (!g_piRetargetSent) {
                    g_piRetargetSerial = Sh1xHost_LevelSerial();
                    g_piRetargetTick = GetTickCount();
                    Sh1xHost_Command(g_piRetarget);
                    g_piRetargetSent = TRUE;
                    g_piInitMs = 0;
                }
                if (g_piQuit == 1) {
                    /* stopped before its level came up: the match ends, the server stays */
                    g_piQuit = 2;
                    endMatch = TRUE;
                }
                alive = Sh1xHost_Frame();
                if (alive && !endMatch && Sh1xHost_LevelSerial() != g_piRetargetSerial) {
                    g_piFirstFrameMs = GetTickCount() - g_piRetargetTick;
                    InterlockedExchange(&g_piState, PI_RUNNING);
                }
                if (!alive) PiServerEnded();
                else if (endMatch) InterlockedExchange(&g_piState, PI_ENDED);
                SwitchToFiber(g_piMainFiber);
                continue;
            }
            if (st == PI_RUNNING && g_piQuit == 1) {
                /* The end of a match, never of the server. A match that is over (or a start that failed) keeps
                 * its level until the next match's map replaces it, as a one-shot lobby worker does. The
                 * player's stop (p2pstop) first sends everyone in the game back to the lobby, as a finished
                 * match does (t "EXE_MATCHENDED"), lets a few frames carry it, then unloads the level. */
                if (g_p2pStop && !g_piMatchOver) {
                    g_piLobbyTold = Sh1xHost_ReturnPlayersToLobby();
                    g_piLobbyTick = GetTickCount();
                    g_piQuit = 3;
                } else {
                    g_piQuit = 2;
                    endMatch = TRUE;
                }
            } else if (st == PI_RUNNING && g_piQuit == 3 &&
                       (!g_piLobbyTold || GetTickCount() - g_piLobbyTick >= 1500)) {
                Sh1xHost_Command("killserver\n");
                g_piQuit = 2;
                endMatch = TRUE;
            }
            alive = Sh1xHost_Frame();
            if (!alive) PiServerEnded();
            else if (endMatch) InterlockedExchange(&g_piState, PI_ENDED);
        }
        SwitchToFiber(g_piMainFiber);
    }
}

#if K33_DIAGNOSTICS /* K33_SHIPPING_TIMING_GAP_STATE */
static LONGLONG g_piLastTickQpc, g_piMaxGapUs;
#else /* K33_SHIPPING_TIMING_GAP_STATE */
static LONGLONG g_piLastTickQpc;
#endif /* K33_SHIPPING_TIMING_GAP_STATE */
static volatile LONG g_piInTick;
#if K33_DIAGNOSTICS /* K33_SHIPPING_TIMING_GAP_COUNT */
static volatile LONG g_piGapsOver1s;
#endif /* K33_SHIPPING_TIMING_GAP_COUNT */

/* Main (game) thread: once per C1 Com_EventLoop pass, and from C1's waits (Sleep, SleepEx, PeekMessageA), so
 * the server keeps running while C1 loads a map or sits in a loop that skips Com_EventLoop. */
__attribute__((used)) void __cdecl PiTick(void)
{
    LONG st = g_piState;
    if (!PiServerTicked()) return;
    if (g_piInTick) return;
    if(GetCurrentThreadId()!=g_p2pMainThread)return;
    if(!PiProductCurrent()) {
        /* Unknown callback/import mutation: never resume the retained server. */
        InterlockedExchange(&g_piQuit,1);InterlockedExchange(&g_piServer,PS_ENDED);InterlockedExchange(&g_piState,PI_CRASHED);return;
    }
#if K33_DIAGNOSTICS /* K33_SHIPPING_TIMING_GAP_SAMPLE */
    if (g_piLastTickQpc && g_piQpf) {
        LARGE_INTEGER n;
        QueryPerformanceCounter(&n);
        LONGLONG gap = (n.QuadPart - g_piLastTickQpc) * 1000000 / g_piQpf;
        if (st == PI_RUNNING && gap > g_piMaxGapUs) g_piMaxGapUs = gap;
        if (st == PI_RUNNING && gap > 1000000) InterlockedIncrement(&g_piGapsOver1s);
    }
#endif /* K33_SHIPPING_TIMING_GAP_SAMPLE */
    if (!g_piMainFiber) {
        g_piMainFiber = IsThreadAFiber() ? PiCurrentFiber() : ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
        if (!g_piMainFiber) { Log("p2p in-process: ConvertThreadToFiber failed (%lu)", GetLastError()); PiServerEnded(); return; }
        g_piThread = GetCurrentThreadId();
    }
    if (GetCurrentThreadId() != g_piThread) return;
    if (!g_piFiber) {
        g_piFiber = CreateFiberEx(1u << 20, 32u << 20, FIBER_FLAG_FLOAT_SWITCH, PiServerFiber, NULL);
        if (!g_piFiber) { Log("p2p in-process: CreateFiberEx failed (%lu)", GetLastError()); PiServerEnded(); return; }
    }
    unsigned short fcw;
    unsigned int mxcsr;
    __asm__ volatile("fnstcw %0" : "=m"(fcw));
    __asm__ volatile("stmxcsr %0" : "=m"(mxcsr));
#if K33_DIAGNOSTICS /* K33_SHIPPING_TIMING_SAMPLE_DECL */
    LARGE_INTEGER a, b;
#else /* K33_SHIPPING_TIMING_SAMPLE_DECL */
    LARGE_INTEGER b;
#endif /* K33_SHIPPING_TIMING_SAMPLE_DECL */
#if K33_DIAGNOSTICS /* K33_SHIPPING_TIMING_SAMPLE_START */
    QueryPerformanceCounter(&a);
#endif /* K33_SHIPPING_TIMING_SAMPLE_START */
    g_piInTick = 1;
    SwitchToFiber(g_piFiber);
    g_piInTick = 0;
    QueryPerformanceCounter(&b);
    __asm__ volatile("fldcw %0" : : "m"(fcw));
    __asm__ volatile("ldmxcsr %0" : : "m"(mxcsr));
    g_piLastTickQpc = b.QuadPart;
#if K33_DIAGNOSTICS /* K33_SHIPPING_TIMING_SAMPLE_TOTALS */
    if (st == PI_RUNNING && g_piQpf) {
        LONGLONG us = (b.QuadPart - a.QuadPart) * 1000000 / g_piQpf;
        g_piCalls++; g_piTotalUs += us; if (us > g_piMaxUs) g_piMaxUs = us;
        g_piWinCalls++; g_piWinTotalUs += us; if (us > g_piWinMaxUs) g_piWinMaxUs = us;
    }
#endif /* K33_SHIPPING_TIMING_SAMPLE_TOTALS */
}

/* From C1's waits: at most one tick per 2 ms (a server frame is due every 16.7 ms at sv_fps 60). */
static void PiWaitTick(void)
{
    if (GetCurrentThreadId() != g_piThread || !PiServerTicked() || !g_piQpf) return;
    LARGE_INTEGER n;
    QueryPerformanceCounter(&n);
    if (g_piLastTickQpc && (n.QuadPart - g_piLastTickQpc) * 1000 / g_piQpf < 2) return;
    PiTick();
}

static FARPROC g_piRealSleep, g_piRealSleepEx, g_piRealPeek;
static void WINAPI PiSleep(DWORD ms) { PiWaitTick(); ((void(WINAPI *)(DWORD))g_piRealSleep)(ms); }
static DWORD WINAPI PiSleepEx(DWORD ms, BOOL alertable) { PiWaitTick(); return ((DWORD(WINAPI *)(DWORD, BOOL))g_piRealSleepEx)(ms, alertable); }
static BOOL WINAPI PiPeek(LPMSG m, HWND h, UINT a, UINT b, UINT r)
{
    PiWaitTick();
    return ((BOOL(WINAPI *)(LPMSG, HWND, UINT, UINT, UINT))g_piRealPeek)(m, h, a, b, r);
}


/* Main thread, between frames: run fn(ctx) on the server's fiber. FALSE when no server is running. */
static volatile LONG g_piYielding;
static BOOL PiRunOnServer(void (*fn)(void *), void *ctx)
{
    /* not while the server waits mid-frame (PiYield): the fiber would resume in its wait, not run fn */
    if (g_piServer != PS_RUNNING || !g_piFiber || GetCurrentThreadId() != g_piThread || g_piInTick || g_piYielding) return FALSE;
    unsigned short fcw;
    unsigned int mxcsr;
    __asm__ volatile("fnstcw %0" : "=m"(fcw));
    __asm__ volatile("stmxcsr %0" : "=m"(mxcsr));
    g_piCallCtx = ctx;
    g_piCallFn = fn;
    g_piInTick = 1;
    SwitchToFiber(g_piFiber);
    g_piInTick = 0;
    __asm__ volatile("fldcw %0" : : "m"(fcw));
    __asm__ volatile("ldmxcsr %0" : : "m"(mxcsr));
    return g_piCallFn == NULL;
}

void PI_TickHook(void);
__asm__(
    ".text\n"
    ".globl _PI_TickHook\n"
    "_PI_TickHook:\n"
    "    pushal\n"
    "    pushfl\n"
    "    call _PiTick\n"
    "    popfl\n"
    "    popal\n"
    "    jmp *_g_piNext\n");

/* The server of this run has ended (a quit, a Sys_Error, a changed hook): it cannot be started again. */
static BOOL P2PInprocUsed(void) { return g_piServer == PS_ENDED; }
/* The server of this run is up: it holds its port between matches. */
static BOOL P2PInprocServerAlive(void) { return g_piServer == PS_RUNNING; }
/* ---- the session (p2p.c's host thread) --------------------------------------------------------------- */

static void PiMemory(SIZE_T *privateMb, SIZE_T *virtualMb)
{
    PROCESS_MEMORY_COUNTERS_EX pmc;
    memset(&pmc, 0, sizeof(pmc));
    pmc.cb = sizeof(pmc);
    *privateMb = GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof(pmc)) ? pmc.PrivateUsage >> 20 : 0;
    MEMORYSTATUSEX ms;
    memset(&ms, 0, sizeof(ms));
    ms.dwLength = sizeof(ms);
    *virtualMb = GlobalMemoryStatusEx(&ms) ? (SIZE_T)((ms.ullTotalVirtual - ms.ullAvailVirtual) >> 20) : 0;
}


/* ---- C1's zones, for the hosted server's map loads (stage 3) ---------------------------------------------------
 * C1's loaded-zone table, as its "zone loaded?" test 0x4CD7D0 reads it: count 0x18FCFBC, a handle byte per slot at
 * 0x1C402A4, zones at 0x1F0E108 + handle * 0xA8 with the name inline at +0x0C; [0x17C6880] is a zone the test
 * skips (the one being unloaded). */
#define C1_ZONE_COUNT_VA    0x018FCFBCu
#define C1_ZONE_HANDLES_VA  0x01C402A4u
#define C1_ZONES_VA         0x01F0E108u
#define C1_ZONE_SKIPPED_VA  0x017C6880u
#define C1_DB_LOADING_VA    0x02007A60u
static BOOL PiC1ZoneLoaded(const char *name)
{
    int n = *(volatile const int *)(DWORD_PTR)C1_ZONE_COUNT_VA;
    DWORD skipped = *(volatile const DWORD *)(DWORD_PTR)C1_ZONE_SKIPPED_VA;
    if (!name || n < 0 || n > 64) return FALSE;
    for (int i = 0; i < n; i++) {
        DWORD zone = C1_ZONES_VA + 0xA8u * ((const BYTE *)(DWORD_PTR)C1_ZONE_HANDLES_VA)[i];
        if (zone == skipped) continue;
        if (!lstrcmpiA((const char *)(DWORD_PTR)(zone + 0x0C), name)) return TRUE;
    }
    return FALSE;
}
static unsigned PiC1ZoneSignature(void)
{
    int n = *(volatile const int *)(DWORD_PTR)C1_ZONE_COUNT_VA;
    unsigned h = (unsigned)n * 2654435761u;
    if (n < 0 || n > 64) return 0;
    for (int i = 0; i < n; i++) h = h * 31 + ((const BYTE *)(DWORD_PTR)C1_ZONE_HANDLES_VA)[i];
    return h;
}
static void PiLogC1Zones(const char *why)
{
    char line[1024]; int used = 0, n = *(volatile const int *)(DWORD_PTR)C1_ZONE_COUNT_VA;
    line[0] = 0;
    for (int i = 0; i < n && i < 64 && used < 960; i++) {
        DWORD zone = C1_ZONES_VA + 0xA8u * ((const BYTE *)(DWORD_PTR)C1_ZONE_HANDLES_VA)[i];
        size_t remaining = sizeof(line) - (size_t)used;
        int written = _snprintf(line + used, remaining, " %s", (const char *)(DWORD_PTR)(zone + 0x0C));
        if (written < 0 || (size_t)written >= remaining) {
            line[sizeof(line) - 1] = 0;
            break;
        }
        used += written;
    }
    Log("p2p in-process: C1 zones (%s, %d, loading %lu):%s", why, n, *(volatile const DWORD *)(DWORD_PTR)C1_DB_LOADING_VA, line);
}

/* On the server's fiber: let C1's thread run until the next tick resumes the server here. */
static void PiYield(void)
{
    if (!g_piMainFiber || GetCurrentThreadId() != g_piThread) { Sleep(1); return; }
    g_piYielding = 1;
    SwitchToFiber(g_piMainFiber);
    g_piYielding = 0;
}
/* The server's await_zones (asset_host.h), on its fiber during a map load. C1 starts loading a hosted map
 * itself when it is connected (the server's loadingnewmap); otherwise, after a short grace, preloadmap starts it.
 * Then the server waits, yielding, until C1 holds the level and its common_gametype zone
 * and its database is idle. */
static int g_piC1Preload;
static volatile LONG g_piFirstMapAwait;
static int __cdecl PiAwaitZones(const char *map, const char *zoneType, unsigned timeoutMs)
{
    char level[64], common[64], cmd[192];
    const char *tail;
    DWORD t0 = GetTickCount(), started = 0, busy = 0;
    unsigned signature = PiC1ZoneSignature(), lastSig = signature;
    BOOL preloaded = FALSE;
    if (!map || !zoneType) return 0;
    BOOL firstMap = InterlockedExchange(&g_piFirstMapAwait, 0) != 0;
    lstrcpynA(level, map, sizeof(level));
    if (!strncmp(map, "coop_", 5) && (tail = strchr(map + 5, '_')) && tail[1]) lstrcpynA(level, tail + 1, sizeof(level));
    _snprintf(common, sizeof(common), "common_gametype_%s", zoneType); common[sizeof(common) - 1] = 0;
    PiLogC1Zones("map load begins");
    /* C1 still loading its front end (hosting at start-up): it cannot load a map yet; the server mounts its own */
    if (!PiC1ZoneLoaded("ui_mp") && !PiC1ZoneLoaded("common_mp")) {
        Log("p2p in-process: C1 has not loaded its front end; the server mounts its own copy of %s", map);
        return 0;
    }
    for (;;) {
        DWORD now = GetTickCount();
        BOOL loading = *(volatile const DWORD *)(DWORD_PTR)C1_DB_LOADING_VA != 0;
        /* a co-op session zone (coop_<mode>_<level>) is loaded by C1 only when the map string names it; the
         * server's loadingnewmap sends that spelling (server 2026-10-27), so C1 holds the session zone too */
        BOOL held = PiC1ZoneLoaded(level) && PiC1ZoneLoaded(common) && PiC1ZoneLoaded(map);
        if (held && !loading) {
            PiLogC1Zones("held");
            Log("p2p in-process: C1 holds %s (%s) after %lu ms%s", map, zoneType, now - t0, preloaded ? " (preloadmap)" : "");
            return 1;
        }
        /* During the first server map load, no player has connected to this
         * new host yet. If C1 is idle in its menu, its own map load cannot
         * begin until the server answers. Do not spend the usual two-second
         * late-client grace here; subsequent map changes retain that grace. */
        if (firstMap && !g_piC1Preload && !loading) {
            Log("p2p in-process: first host map has no C1 zone load; mounting the server copy now");
            return 0;
        }
        if (!started && (loading || PiC1ZoneSignature() != signature)) started = now;
        {
            unsigned sig = PiC1ZoneSignature();
            if (loading || sig != lastSig) { busy = now; lastSig = sig; }
        }
        /* C1's load stopped without the zones (a zone change that was not this map, or C1 left the match): the
         * server mounts its own instead of waiting out the timeout, which held its rcon and network for 60 s.
         * C1 links zone after zone with idle reads between them (a 60 s zm load under memory pressure), so only
         * 30 s with neither the database busy nor the zone list changing counts (a zm load paused 15+ s, measured). */
        if (started && !loading && now - (busy ? busy : started) > 30000) {
            PiLogC1Zones("load stopped");
            Log("p2p in-process: C1 stopped loading without %s (%s); the server mounts its own copy", map, zoneType);
            return 0;
        }
        /* No C1 load under way after a short grace: C1 is not in this server's match (the first map, or the host
         * player away). Starting C1's load from the menu (preloadmap) is opt-in (p2p_host.ini c1_preload = 1):
         * measured, it can leave C1's menu flow crashing in its next room join. Otherwise the server mounts its own. */
        if (!started && !g_piC1Preload && now - t0 > 2000) {
            Log("p2p in-process: C1 is not loading %s; the server mounts its own copy", map);
            return 0;
        }
        if (!started && !preloaded && now - t0 > 2000) {
            _snprintf(cmd, sizeof(cmd), "preloadmap %s %s\n", level, zoneType); /* the spelling loadingnewmap uses */ cmd[sizeof(cmd) - 1] = 0;
            C1CbufAddText(cmd);
            preloaded = TRUE;
            Log("p2p in-process: C1 not loading %s, sent preloadmap", map);
        }
        if (now - t0 > timeoutMs) {
            PiLogC1Zones("timed out");
            Log("p2p in-process: C1 did not hold %s (%s) within %u ms; the server mounts its own copy", map, zoneType, timeoutMs);
            return 0;
        }
        PiYield();
    }
}
static int __cdecl PiZoneHeld(const char *zone) { return PiC1ZoneLoaded(zone); }

/* C1's record of (type, name) for the server (p2p_assets.c PiC1Find); none while C1's database thread links a zone. */
static const void *PiC1Find(unsigned type, const char *name);
static const char *PiC1ZoneOf(unsigned type, const char *name);
static const char *__cdecl PiZoneOf(unsigned type, const char *name) { return PiC1ZoneOf(type, name); }
/* C1's script strings: a static array of 16-byte entries at 0x02627E00, the text 4 bytes into each (found by
 * p2pslscan 2026-10-27: defaultactor's bone handles 570 and 3938 give tag_origin and j_mainroot; the code at
 * 0x004410B4 holds the address as an immediate). */
#define C1_SL_BUFFER_VA 0x02627E00u
static const char *__cdecl PiScriptString(unsigned handle)
{
    const char *s;
    if (!handle || handle > 0xffffu) return NULL;
    s = (const char *)(DWORD_PTR)(C1_SL_BUFFER_VA + 16u * handle + 4u);
    return Readable(s, 1) ? s : NULL;
}
/* Offline: the match is over once its level ended and its player left (or nobody came back for a minute). */
static BOOL PiMatchOverPoll(const char *line, int *state, int *emptyPolls)
{
    const char *h = strstr(line, " humans="), *e = strstr(line, " ended=");
    int humans = h ? atoi(h + 8) : -1, ended = e ? atoi(e + 7) : 0;
    if (ended) *state |= 2;
    if (humans > 0) { *state |= 1; *emptyPolls = 0; return FALSE; }
    if (!(*state & 1) || humans != 0) return FALSE;
    ++*emptyPolls;
    if ((*state & 2) && *emptyPolls >= 3) { Log("p2p in-process: the match ended and its player left; ending the match (the server stays up)"); return TRUE; }
    if (*emptyPolls >= 30) { Log("p2p in-process: nobody has been on the match for a minute; ending the match (the server stays up)"); return TRUE; }
    return FALSE;
}
static const void *__cdecl PiC1Provide(unsigned type, const char *name)
{
    if (*(volatile const DWORD *)(DWORD_PTR)0x02007A60u) return NULL;
    return PiC1Find(type, name);
}
/* The settings a running server cannot change: they were on its command line (sockets, paths, rcon). */
static const char *PiServerCfgChanged(const P2PConfig *c)
{
    const P2PConfig *s = &g_piServerCfg;
    if (c->port != s->port || strcmp(c->netIp, s->netIp)) return "port";
    if (_stricmp(c->basepath, s->basepath) || _stricmp(c->home, s->home) || _stricmp(c->scripts, s->scripts) ||
        _stricmp(c->state, s->state) || _stricmp(c->profile, s->profile) || _stricmp(c->rconFile, s->rconFile))
        return "paths";
    if (c->admission != s->admission || c->developer != s->developer || c->pmemMegs != s->pmemMegs ||
        c->c1Assets != s->c1Assets || c->c1Zones != s->c1Zones || strcmp(c->extra, s->extra))
        return "settings";
    return NULL;
}
static int P2PInprocRun(const P2PConfig *cp, const char *password, const char *args)
{
    P2PConfig c = *cp;
    (void)password;
    if (!g_piHooked || !PiProductCurrent()) {
        Log("p2p in-process: the frame hook is not installed; not hosting");
        P2PSetState(P2P_FAILED, "no frame hook");
        return 1;
    }
    if (!K33_HAS_SERVER) {
        Log("p2p in-process: this Kernel33.dll was built without the match server (scripts/build.sh K33_SERVER)");
        P2PSetState(P2P_FAILED, "built without the match server");
        return 1;
    }
    LONG st = g_piState;
    if (st == PI_INIT_PENDING || st == PI_RUNNING) { P2PSetState(P2P_FAILED, "a match is already running"); return 1; }
    /* The server of this run ended (quit, Sys_Error): its globals are spent, it cannot start again. */
    if (P2PInprocUsed()) {
        Log("p2p in-process: the match server of this client run has ended; restart the client to host again");
        P2PSetState(P2P_FAILED, "restart the client to host again");
        return 1;
    }
    BOOL first = g_piServer == PS_NONE;
    if (!first) {
        const char *changed = PiServerCfgChanged(&c);
        if (changed) {
            Log("p2p in-process: the match server runs with other %s than this match asks for; restart the client", changed);
            P2PSetState(P2P_FAILED, "restart the client to host with these settings");
            return 1;
        }
    }
    LONG n = InterlockedIncrement(&g_piSession);
    SIZE_T priv0, virt0, priv1, virt1;
    PiMemory(&priv0, &virt0);
    DWORD t0 = GetTickCount();
    BOOL tracked = TRUE; /* the match-over rule applies to every match (offline) */
    if (first) {
        /* The server's hooks into C1 (stage 3 of its asset layout): C1's own records, zones and script strings.
         * p2p_host.ini c1_assets = 0 / c1_zones = 0 turn them off. They are set once, before Sh1xHost_Init. */
        g_piC1Preload = c.c1Preload;
        if (c.c1Assets) {
            Sh1xHost_SetC1AssetProvider(PiC1Provide);
            Sh1xHost_SetC1ZoneHost(PiAwaitZones, PiZoneHeld);
            Log("p2p in-process: C1 asset provider and zone host installed");
            if (c.c1Zones) {
                Sh1xHost_SetC1ZoneFind(PiZoneOf);
                Sh1xHost_SetC1ScriptStrings(PiScriptString);
                Log("p2p in-process: C1-backed map zones on");
            }
        }
        lstrcpynA(g_piArgs, args, sizeof(g_piArgs));
        g_piServerCfg = c;
        Log("p2p in-process: starting the match server linked into Kernel33.dll (match %ld, the first of this client run); "
            "hosting %s %s on %s:%d on the game thread", n, c.map, c.gametype, c.netIp, c.port);
    } else {
        /* The same server, moved to this match: latched settings, then the map (as the lobby retargets a worker). */
        const char *session = c.sessionMap[0] ? c.sessionMap : c.map;
        int difficulty = !strcmp(c.zoneType, "zm") && c.difficultyPresent && c.difficulty >= 0 && c.difficulty <= 2 ? c.difficulty : 0;
        /* fs_game carries the category for exactly the next spawn (server filesystem.c FS_NormalizeGameDir,
         * sv_main.c SV_ResolveAssetCategory), as the lobby dispatches a match. codol_zoneType alone only
         * counts while it differs from the running value: a second co-op or survival match in a row would
         * otherwise resolve as mp from its mp_* level name. */
        _snprintf(g_piRetarget, sizeof(g_piRetarget),
                  "set fs_game %s\nset codol_zoneType %s\nset codol_sessionMap %s\nset g_gametype %s\nset g_difficulty %d\nmap %s\n",
                  c.zoneType, c.zoneType, session, c.gametype, difficulty, c.map);
        g_piRetarget[sizeof(g_piRetarget) - 1] = 0;
        Log("p2p in-process: reusing the match server started %lu s ago (match %ld): %s %s on %s:%d",
            (GetTickCount() - g_piServerStartTick) / 1000, n, c.map, c.gametype, c.netIp, c.port);
    }
    InterlockedExchange(&g_piFirstMapAwait, 1);
    g_piRetargetSent = FALSE;
#if K33_DIAGNOSTICS /* K33_SHIPPING_TIMING_START_TOTALS */
    g_piCalls = g_piTotalUs = g_piMaxUs = g_piWinCalls = g_piWinTotalUs = g_piWinMaxUs = 0;
#endif /* K33_SHIPPING_TIMING_START_TOTALS */
    g_piInitMs = g_piFirstFrameMs = 0;
    g_piFaultCode = 0;
    InterlockedExchange(&g_piQuit, 0);
    EnterCriticalSection(&g_p2pLock);
    g_p2p.pid = GetCurrentProcessId();
    g_p2p.startTick = t0;
    LeaveCriticalSection(&g_p2pLock);
    InterlockedExchange(&g_piState, PI_INIT_PENDING);

    BOOL ready = FALSE, quitAsked = FALSE;
    int matchState = 0;
    int emptyPolls = 0;
    char current[160] = ""; /* the running match's identity (P2PStatusIdentity), to follow a map change */
#if K33_DIAGNOSTICS /* K33_SHIPPING_TIMING_MONITOR_DECL */
    DWORD lastPoll = 0, lastAnswer = 0, lastStats = GetTickCount(), quitAt = 0;
#else /* K33_SHIPPING_TIMING_MONITOR_DECL */
    DWORD lastPoll = 0, lastAnswer = 0, quitAt = 0;
#endif /* K33_SHIPPING_TIMING_MONITOR_DECL */
    for (;;) {
        Sleep(250);
        st = g_piState;
        if (st == PI_ENDED || st == PI_CRASHED) break;
        DWORD now = GetTickCount();
        if (g_p2pStop && !quitAsked) {
            quitAsked = TRUE;
            quitAt = now;
            P2PSetState(P2P_STOPPING, NULL);
            InterlockedExchange(&g_piQuit, 1);
        }
        if (quitAsked && now - quitAt > 30000) {
            Log("p2p in-process: the server did not end within 30 s of quit (state %s)", kPiState[st]);
            break;
        }
#if K33_DIAGNOSTICS /* K33_SHIPPING_TIMING_MONITOR_TOTALS */
        if (st == PI_RUNNING && now - lastStats >= 30000 && !g_piWinCalls) {
            Log("p2p in-process: no server ticks in the last %lu s (the game thread is blocked or skips every tick source)",
                (now - lastStats) / 1000);
            lastStats = now;
        }
        if (st == PI_RUNNING && now - lastStats >= 30000 && g_piWinCalls) {
            Log("p2p in-process: game-thread cost over %lu s: %lld calls, mean %lld us, max %lld us; longest gap "
                "between ticks %lld ms, gaps over 1 s so far %ld", (now - lastStats) / 1000, g_piWinCalls,
                g_piWinTotalUs / g_piWinCalls, g_piWinMaxUs, g_piMaxGapUs / 1000, g_piGapsOver1s);
            g_piMaxGapUs = 0;
            g_piWinCalls = g_piWinTotalUs = g_piWinMaxUs = 0;
            lastStats = now;
        }
#endif /* K33_SHIPPING_TIMING_MONITOR_TOTALS */
        if (now - lastPoll < (ready ? P2P_STATUS_MS : 1000) || st != PI_RUNNING) continue;
        lastPoll = now;
        /* A late matching reply must not reopen Ready once shutdown starts.
         * It also must not reset quitAt on every failed startup poll. */
        if (quitAsked) continue;
        char reply[2048], line[512], ident[160];
        int kind = P2P_STATUS_BAD;
        if (P2PRcon(c.port, password, "sh1x_status", reply, sizeof(reply), 400) > 0 &&
            P2PStatusLine(reply, line, sizeof(line)))
            kind = P2PStatusClassify(line, c.map, c.sessionMap, c.gametype, c.zoneType);
        /* Readiness is first confirmed only by the requested match (EXACT): that proves the server loaded what the
         * room asked for. Once ready, a running match on another map/gametype is still this session's match (a
         * console or rcon `map`): the session follows it, and it answers, so it is not hung. Requiring the
         * requested map here ended every session 20 s after a map change ("stopped answering"). */
        if (kind == P2P_STATUS_EXACT || (ready && kind == P2P_STATUS_LIVE)) {
            lastAnswer = now;
            P2PStatusIdentity(line, ident, sizeof(ident));
            if (strcmp(ident, current)) {
                if (current[0]) {
                    Log("p2p in-process: the match changed from %s to %s; the session follows it", current, ident);
                    /* a new level is a new match: an earlier level exit does not end this one */
                    matchState &= ~2;
                    emptyPolls = 0;
                }
                lstrcpynA(current, ident, sizeof(current));
            }
            if (OfflineLobbyActive() && tracked && PiMatchOverPoll(line, &matchState, &emptyPolls)) {
                /* offline: one match per session; the room is re-armed for the next Ready (p2p.c) */
                InterlockedExchange(&g_piMatchOver, 1);
                quitAsked = TRUE;
                quitAt = now;
                P2PSetState(P2P_STOPPING, NULL);
                InterlockedExchange(&g_piQuit, 1);
                continue;
            }
            EnterCriticalSection(&g_p2pLock);
            lstrcpynA(g_p2p.status, line, sizeof(g_p2p.status));
            if (!ready) g_p2p.readyMs = now - t0;
            g_p2p.state = P2P_READY;
            LeaveCriticalSection(&g_p2pLock);
            if (!ready) {
                ready = TRUE;
                PiMemory(&priv1, &virt1);
                Log("p2p in-process: server ready after %lu ms (Sh1xHost_Init %lu ms, first frame / map load %lu ms); "
                    "process private %lu -> %lu MB, address space used %lu -> %lu MB: %s", now - t0, g_piInitMs,
                    g_piFirstFrameMs, (unsigned long)priv0, (unsigned long)priv1, (unsigned long)virt0, (unsigned long)virt1, line);
            }
        } else if (ready && g_piYielding) {
            P2PSetState(P2P_STARTING, NULL);
            lastAnswer = now; /* the server is waiting on C1's zone loading (PiAwaitZones), not hung */
        } else if (ready && !quitAsked && now - lastAnswer > 20000) {
            Log("p2p in-process: the server stopped answering sh1x_status for 20 s; ending it");
            P2PSetState(P2P_FAILED, "the server stopped answering");
            InterlockedExchange(&g_piQuit, 1);
            quitAsked = TRUE;
            quitAt = now;
        } else if (!ready && (int)(now - t0) > c.readyTimeout * 1000) {
            Log("p2p in-process: no sh1x_status within %d s; ending the server", c.readyTimeout);
            P2PSetState(P2P_FAILED, "the server did not answer in time");
            InterlockedExchange(&g_piQuit, 1);
            quitAsked = TRUE;
            quitAt = now;
        } else if (ready) {
            /* Match loading, an ended/parked map, or a missed response revokes
             * readiness immediately; only a fresh running status restores it.
             * A server with no level (IDLE: running=0, map=none after a failed
             * map load) answers but is not refreshed: nobody can play on it, so
             * it is still ended 20 s later, as a silent one is. */
            P2PSetState(P2P_STARTING, NULL);
        }
    }
    st = g_piState;
    EnterCriticalSection(&g_p2pLock);
    g_p2p.exitCode = st == PI_CRASHED ? g_piFaultCode : (DWORD)g_piCode();
    if (st == PI_CRASHED) { g_p2p.state = P2P_FAILED; lstrcpynA(g_p2p.error, "the server's frame hook changed; restart the client", sizeof(g_p2p.error)); }
    else if (st != PI_ENDED) {
        /* A quit timeout can leave the original game-thread fiber live.
         * Worker completion is not native host retirement. Keep ownership
         * visible and require client restart; never unload/reset it here. */
        g_p2p.state = P2P_FAILED;
        lstrcpynA(g_p2p.error,"The local host did not stop. Restart the client before hosting again.",sizeof(g_p2p.error));
    }
    else if (g_p2p.state != P2P_FAILED) g_p2p.state = P2P_EXITED;
    if (g_piInitResult) {
        g_p2p.state=P2P_FAILED;
        lstrcpynA(g_p2p.error,"Sh1xHost_Init failed",sizeof(g_p2p.error));
    }
    g_p2p.pid = (st == PI_ENDED || st == PI_CRASHED) ? 0 : GetCurrentProcessId();
    LeaveCriticalSection(&g_p2pLock);
    Log("p2p in-process: match %ld %s after %lu ms (server %s, init %d, exit code %d); %lld frames on the game thread, "
        "mean %lld us, max %lld us", n, kPiState[st], GetTickCount() - t0, kPiServer[g_piServer], g_piInitResult, g_piCode(),
        g_piCalls, g_piCalls ? g_piTotalUs / g_piCalls : 0, g_piMaxUs);
    PiMemory(&priv1, &virt1);
    if (g_piServer == PS_RUNNING)
        Log("p2p in-process: the match server stays up for the next match (%lu KB of on-demand state; process private "
            "%lu MB, address space %lu MB)", (unsigned long)(Sh1xHost_OnDemandBytes() >> 10), (unsigned long)priv1,
            (unsigned long)virt1);
    else if (g_piServer == PS_ENDED)
        Log("p2p in-process: the match server has ended; restart the client to host again");
    (void)tracked;
    return 0;
}

/* p2pstatus adds the in-process numbers. */
static void __cdecl PiCmdStatus(void)
{
    P2PCmdStatus();
    SIZE_T priv, virt;
    PiMemory(&priv, &virt);
    C1Printf("p2p in-process: server %s; init %lu ms, map load %lu ms; %lld frames, mean %lld us, max %lld us; process private %lu MB, address space %lu MB\n",
             kPiServer[g_piServer], g_piInitMs, g_piFirstFrameMs, g_piCalls, g_piCalls ? g_piTotalUs / g_piCalls : 0, g_piMaxUs,
             (unsigned long)priv, (unsigned long)virt);
}

#if K33_LOCAL_TESTING
static void __cdecl PiCmdAssetCheck(void); /* p2p_assets.c */
static void __cdecl PiCmdSlScan(void); /* p2p_assets.c */
static void __cdecl PiServerCommand(void *line) { Sh1xHost_Command((const char *)line); }
/* p2psv <command...>: test clients only (+set com_cmdFile 1): one console command for the running match server,
 * queued with Sh1xHost_Command on its fiber (C1 splits a line at ';', so one command per p2psv). */
static void __cdecl PiCmdServer(void)
{
    const char *cl = GetCommandLineA();
    char line[1024];
    size_t used = 0;
    if (!cl || !strstr(cl, "com_cmdFile 1")) { C1Printf("p2psv: test clients only (+set com_cmdFile 1)\n"); return; }
    line[0] = 0;
    for (int i = 1; i < VoiceArgc() && used + 2 < sizeof(line); i++) {
        int n = _snprintf(line + used, sizeof(line) - used - 2, "%s%s", i > 1 ? " " : "", VoiceArgv(i));
        if (n < 0) break;
        used += (size_t)n;
    }
    lstrcpynA(line + used, "\n", (int)(sizeof(line) - used));
    if (!PiRunOnServer(PiServerCommand, line)) { C1Printf("p2psv: no in-process server running\n"); return; }
    Log("p2p in-process: p2psv %s", line);
}
static C1Cmd g_piCmds[] = { { NULL, "p2passetcheck", (void *)PiCmdAssetCheck }, { NULL, "p2pslscan", (void *)PiCmdSlScan },
                            { NULL, "p2psv", (void *)PiCmdServer } };
#endif

/* Product activation runs only on the captured C1 main thread and appends Pi
 * AFTER all existing dispatcher callbacks. The event-loop CALL is never changed. */
#include "p2p_activation_transaction.h"
typedef struct {
    FARPROC original[3];
    DWORD protection[3], next;
    DWORD restore_protection[3];
    int writable[3];
    BYTE call[5];
    K33TickFn ticks[K33_MAX_TICKS];
    int count;
    PVOID handler;
} PiActivationWindows;
static PiActivation g_piActivation;
static PiActivationWindows g_piActivationWindows;
static const DWORD g_piActivationSlots[3] = { 0x00B42184u, 0x00B42134u, 0x00B42424u };

static FARPROC PiActivationHook(unsigned index)
{
    return index == 0 ? (FARPROC)PiSleep : index == 1 ? (FARPROC)PiSleepEx : (FARPROC)PiPeek;
}
static int PiActivationChain(PiActivationWindows *w, int appended)
{
    const BYTE *call=(const BYTE *)(DWORD_PTR)VOICE_EVENTLOOP_CALL_VA;
    if (!g_k33TickInstalled ||
        !Readable(call,5) || memcmp(call,w->call,5) || g_k33TickNext!=w->next ||
        g_k33TickCount!=w->count+appended) return 0;
    for (int i=0;i<w->count;++i) if(g_k33Ticks[i]!=w->ticks[i])return 0;
    return !appended || g_k33Ticks[w->count]==PiTick;
}
static int PiActivationPrepare(void *context)
{
    PiActivationWindows *w=(PiActivationWindows *)context;
    const BYTE *call=(const BYTE *)(DWORD_PTR)VOICE_EVENTLOOP_CALL_VA;
    LARGE_INTEGER frequency;
    HMODULE kernel=GetModuleHandleA("kernel32.dll"), user=GetModuleHandleA("user32.dll");
    const char *names[3]={"Sleep","SleepEx","PeekMessageA"};
    if (!g_p2pMainThread || GetCurrentThreadId()!=g_p2pMainThread || !g_k33TickInstalled ||
        !kernel || !user || !QueryPerformanceFrequency(&frequency) || frequency.QuadPart<=0 ||
        !Readable(call,5) || !CallsTarget(VOICE_EVENTLOOP_CALL_VA,(DWORD)(DWORD_PTR)K33_MainTickHook) ||
        K33HasMainTick(PiTick) || g_k33TickCount>=K33_MAX_TICKS) return 0;
    /* Preserve known retained room/voice/discord chain; never double-run an old PI hook. */
    if(g_k33TickNext!=VOICE_EVENTLOOP_FUNC_VA &&
       g_k33TickNext!=(DWORD)(DWORD_PTR)VOICE_TickHook &&
       g_k33TickNext!=(DWORD)(DWORD_PTR)DISCORD_TickHook
#if K33_KERNEL_ROOM_CONTEXT
       && g_k33TickNext!=(DWORD)(DWORD_PTR)KRC_MainTickHook
#endif
       )return 0;
    memcpy(w->call,call,5);w->next=g_k33TickNext;w->count=g_k33TickCount;
    for(int i=0;i<w->count;++i)w->ticks[i]=g_k33Ticks[i];
    for(unsigned i=0;i<3;++i) {
        FARPROC *slot=(FARPROC *)(DWORD_PTR)g_piActivationSlots[i];
        MEMORY_BASIC_INFORMATION memory;
        FARPROC genuine=GetProcAddress(i==2?user:kernel,names[i]);
        if(!genuine || !Readable(slot,sizeof(*slot)) || *slot!=genuine ||
           VirtualQuery(slot,&memory,sizeof(memory))!=sizeof(memory) || memory.State!=MEM_COMMIT ||
           (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return 0;
        w->original[i]=genuine;w->protection[i]=memory.Protect;
    }
    g_piRealSleep=w->original[0];g_piRealSleepEx=w->original[1];g_piRealPeek=w->original[2];
    g_piQpf=frequency.QuadPart;
    return PiActivationChain(w,0);
}
static int PiActivationHandler(void *context,int install)
{
    PiActivationWindows *w=(PiActivationWindows *)context;
    /* The fault-containment step of the transaction: nothing to install since the server is part of Kernel33.dll
     * (its faults are the client's, with the normal crash logging). The step stays so the transaction's order holds. */
    if(install) {
        if(w->handler)return 0;
        w->handler=(PVOID)w;
        return 1;
    }
    w->handler=NULL;return 1;
}
static int PiActivationSlot(void *context,unsigned index,int install)
{
    PiActivationWindows *w=(PiActivationWindows *)context;
    if(index>=3 || GetCurrentThreadId()!=g_p2pMainThread)return 0;
    FARPROC *slot=(FARPROC *)(DWORD_PTR)g_piActivationSlots[index];
    FARPROC desired=install?PiActivationHook(index):w->original[index];
    FARPROC expected=install?w->original[index]:PiActivationHook(index);
    DWORD old=0,ignored=0;MEMORY_BASIC_INFORMATION memory;int ok=1;
    if(!Readable(slot,sizeof(*slot)) || VirtualQuery(slot,&memory,sizeof(memory))!=sizeof(memory))return 0;
    /* Already-original on rollback means failed operation had no pointer effect.
     * Never replace a third party's function pointer. */
    if(!install && *slot==desired) {
        if(w->writable[index]) {
            if(memory.Protect!=PAGE_READWRITE || !VirtualProtect(slot,sizeof(*slot),w->restore_protection[index],&ignored))return 0;
            w->writable[index]=0;
            if(VirtualQuery(slot,&memory,sizeof(memory))!=sizeof(memory))return 0;
        }
        return memory.Protect==w->protection[index]; /* preserve unknown foreign rights */
    }
    if(*slot!=expected)return 0;
    if(w->writable[index]) {
        if(install || memory.Protect!=PAGE_READWRITE)return 0;
    } else {
        if(memory.Protect!=w->protection[index] || !VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&old))return 0;
        w->writable[index]=1;w->restore_protection[index]=old;
        if(old!=w->protection[index]) {
            /* A rights mutation raced the precheck: do NOT publish a pointer.
             * Restore exactly the returned prior rights, not our stale sample. */
            if(VirtualProtect(slot,sizeof(*slot),old,&ignored))w->writable[index]=0;
            return 0;
        }
    }
    if(InterlockedCompareExchangePointer((PVOID volatile *)slot,(PVOID)desired,(PVOID)expected)!=(PVOID)expected)ok=0;
    if(!VirtualProtect(slot,sizeof(*slot),w->restore_protection[index],&ignored))ok=0;
    else w->writable[index]=0;
    if(!Readable(slot,sizeof(*slot)) || *slot!=desired ||
       VirtualQuery(slot,&memory,sizeof(memory))!=sizeof(memory) || memory.Protect!=w->protection[index])ok=0;
    return ok;
}
static int PiActivationTick(void *context,int install)
{
    PiActivationWindows *w=(PiActivationWindows *)context;
    if(GetCurrentThreadId()!=g_p2pMainThread)return 0;
    if(install) {
        if(!PiActivationChain(w,0) || !K33OnMainTick(PiTick))return 0;
        return PiActivationChain(w,1);
    }
    if(PiActivationChain(w,0))return 1;
    if(!PiActivationChain(w,1))return 0; /* unknown mutation retained, never delete another callback */
    g_k33TickCount=w->count;g_k33Ticks[w->count]=NULL;
    return PiActivationChain(w,0);
}
static int PiActivationReadback(void *context)
{
    PiActivationWindows *w=(PiActivationWindows *)context;
    if(!w->handler || !PiActivationChain(w,1))return 0;
    for(unsigned i=0;i<3;++i) {
        FARPROC *slot=(FARPROC *)(DWORD_PTR)g_piActivationSlots[i];MEMORY_BASIC_INFORMATION memory;
        if(w->writable[i] || !Readable(slot,sizeof(*slot)) || *slot!=PiActivationHook(i) ||
           VirtualQuery(slot,&memory,sizeof(memory))!=sizeof(memory) || memory.Protect!=w->protection[i])return 0;
    }
    return 1;
}
static const PiActivationOps g_piActivationOps={&g_piActivationWindows,
    PiActivationPrepare,PiActivationHandler,PiActivationSlot,PiActivationTick,PiActivationReadback};
static BOOL PiProductCurrent(void)
{
    return PiActivationCurrent(&g_piActivation,&g_piActivationOps)!=0;
}
static BOOL P2PEnsureInprocActivation(void)
{
    if(GetCurrentThreadId()!=g_p2pMainThread)return FALSE;
    if(!PiActivationEnsure(&g_piActivation,&g_piActivationOps))return FALSE;
    g_piHooked=TRUE; /* only complete transaction; never a diagnostic success bit */
    return TRUE;
}
#if K33_LOCAL_TESTING
static void InstallP2PInprocDiagnostics(void)
{
    for(unsigned i=0;i<sizeof(g_piCmds)/sizeof(g_piCmds[0]);++i)C1AddCommand(&g_piCmds[i]);
    for(unsigned i=0;i<sizeof(g_p2pCmds)/sizeof(g_p2pCmds[0]);++i)
        if(!strcmp(g_p2pCmds[i].name,"p2pstatus"))g_p2pCmds[i].fn=(void *)PiCmdStatus;
}
#endif

/* ---- the console's route to the hosted server (console/console_server.c, docs/OFFLINE_CONSOLE_20261112.md) ----
 * Self-contained block (feat/offline-console): reads p2p.c's g_p2p under its lock, changes nothing, keeps
 * nothing. 1 while a host session of this client is starting or ready on loopback: its UDP port, the file that
 * holds its rcon password and a number that differs from one session to the next (the tick it started at). The
 * console asks again for every command, so a host that was torn down or started again is seen as such; it never
 * calls into or holds a pointer into the server. 0: this client hosts nothing ("no match running"). */
__attribute__((unused)) static int P2PConsoleHost(int *port, char *rconFile, DWORD *session)
{
    if (!g_p2pInit) return 0;
    EnterCriticalSection(&g_p2pLock);
    int alive = (g_p2p.state == P2P_STARTING || g_p2p.state == P2P_READY) && g_p2p.startTick &&
                g_p2p.cfg.port > 0 && g_p2p.cfg.rconFile[0] && !strcmp(g_p2p.cfg.netIp, "127.0.0.1");
    if (alive) {
        *port = g_p2p.cfg.port;
        lstrcpynA(rconFile, g_p2p.cfg.rconFile, MAX_PATH);
        *session = g_p2p.startTick;
    }
    LeaveCriticalSection(&g_p2pLock);
    return alive;
}
