/*
 * p2p_inproc.c - the sh1x match server inside the host's client, on the game thread (feat/p2p-inprocess).
 *
 * Mode `inprocess` of p2p.c (the default; `mode = child` in p2p_host.ini runs codol_dedrun.exe as before). The
 * server is the sh1x server repo's `host-dll` target, sh1x_host.dll: the same objects as the dedicated exe plus
 * its host API (win32/sys_host.c: Sh1xHost_Init / Frame / Command / ExitCode). Like a listen server, C1's game
 * thread runs it: every pass of C1's Com_EventLoop (the call at 0x595E9D, chained behind voice and discord) runs
 * one server Com_Frame, which returns at once unless a server frame is due (the host API makes NET_Sleep poll).
 * There is no second process and no window.
 *
 * Containment:
 *  - Globals: the DLL's own .data/.bss (323 MB of .bss, mostly IW3 static arrays), loaded from a private copy
 *    (%TEMP%\k33_sh1x_host_<pid>_<n>.dll) so every session starts from fresh globals. A session that ended
 *    leaves its copy loaded: its log and capture threads still run, so FreeLibrary would pull code from under
 *    them.
 *  - Heap: the server allocates through msvcrt.dll and VirtualAlloc (hunk, zone memory), never through C1's
 *    statically linked CRT heap.
 *  - Stack: C1's main thread reserves 640 KB; the dedicated server links with 8 MB, and one server frame
 *    overflowed a 2 MB stack in a harness (0xC00000FD). The server runs on its own fiber (32 MB reserve) on the
 *    game thread, switched to and from synchronously.
 *  - Floating point: the fiber is created with FIBER_FLAG_FLOAT_SWITCH, so the server's x87/SSE control words
 *    (Sys_SetFloatEnv) stay on its fiber; the tick also restores the client's control word and MXCSR.
 *  - Exits: the host API turns every engine exit (quit, Sys_Error, the watchdogs) into an end of the server
 *    (a longjmp back into Sh1xHost_Frame); the client keeps running.
 *  - Faults: a vectored handler catches an access violation, stack overflow, illegal instruction or integer
 *    divide fault raised on the server fiber in sh1x_host.dll or msvcrt.dll, moves execution onto a rescue
 *    stack and switches back to the client's fiber. The session is marked crashed and never resumed; its
 *    sockets stay bound until the client exits, so the next session needs another port.
 *  - Address space: codoMP_client_shipRetail.exe already has IMAGE_FILE_LARGE_ADDRESS_AWARE (characteristics
 *    0x122), so the process has 4 GB on 64-bit Windows; client plus server measured below that.
 *
 * Not contained: a server frame or map load that runs long holds the client's frame (a map load is one server
 * frame, about 2-8 s); an endless loop in the server hangs the client.
 */

#include <psapi.h>

typedef int (__cdecl *Sh1xInitFn)(const char *);
typedef int (__cdecl *Sh1xFrameFn)(void);
typedef void (__cdecl *Sh1xCmdFn)(const char *);
typedef int (__cdecl *Sh1xCodeFn)(void);

enum { PI_NONE, PI_INIT_PENDING, PI_RUNNING, PI_ENDED, PI_CRASHED };
static const char *const kPiState[] = { "none", "starting", "running", "ended", "crashed" };

static volatile LONG g_piState;
static volatile LONG g_piQuit;
#if CODOL_LOCAL_TESTING
static volatile LONG g_piFaultTest;  /* p2pfaulttest: test clients only */
#endif
static Sh1xInitFn g_piInit;
static Sh1xFrameFn g_piFrame;
static Sh1xCmdFn g_piCmd;
static Sh1xCodeFn g_piCode;
static DWORD_PTR g_piBase, g_piEnd, g_crtBase, g_crtEnd;
static void *g_piMainFiber, *g_piFiber;
static DWORD g_piThread;
static char g_piArgs[8192];
static int g_piInitResult;
static DWORD g_piInitMs, g_piFirstFrameMs;
static volatile DWORD g_piFaultCode, g_piFaultEip;
static BYTE g_piRescueStack[64 * 1024] __attribute__((aligned(16)));
/* The assembly tail jump reads this even when shipping removes Log(). */
static DWORD_PTR g_piNext __attribute__((used)); /* prior Com_EventLoop target */
static BOOL g_piHooked;
static BOOL PiProductCurrent(void);
static LONG g_piSession;
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

static void PiModuleRange(HMODULE m, DWORD_PTR *base, DWORD_PTR *end)
{
    *base = *end = 0;
    if (!m) return;
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)m;
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)((const BYTE *)m + dos->e_lfanew);
    *base = (DWORD_PTR)m;
    *end = *base + nt->OptionalHeader.SizeOfImage;
}

/* ---- the server fiber ------------------------------------------------------------------------------- */

static void __cdecl PiRescue(void)
{
    /* On the rescue stack, on the server fiber: never return into the faulting frame. */
    InterlockedExchange(&g_piState, PI_CRASHED);
    for (;;) SwitchToFiber(g_piMainFiber);
}

#if CODOL_KERNEL_ROOM_CONTEXT
#include "p2p_exception_policy.h"
/* Definition remains in the byte-identical repaired kernel_call_guard.c,
 * included later by kernel_room_context.c in this translation unit. */
static EXCEPTION_DISPOSITION __cdecl KrcCallException(EXCEPTION_RECORD *,void *,CONTEXT *,void *);
static int PiReadSehRegistration(uint32_t address,uint32_t words[2],void *unused)
{
    SIZE_T got=0;(void)unused;
    return ReadProcessMemory(GetCurrentProcess(),(const void *)(uintptr_t)address,
                             words,2*sizeof(*words),&got) && got==2*sizeof(*words);
}
static int PiMigrationGuardActive(void)
{
    uint32_t head;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(head));
    return PiSehRequiresThreadDispatch(head,(uint32_t)(uintptr_t)KrcCallException,
                                      PiReadSehRegistration,NULL);
}
#endif

static LONG CALLBACK PiFaultHandler(EXCEPTION_POINTERS *ep)
{
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (!g_piFiber || GetCurrentThreadId() != g_piThread || PiCurrentFiber() != g_piFiber) return EXCEPTION_CONTINUE_SEARCH;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_PRIV_INSTRUCTION)
        return EXCEPTION_CONTINUE_SEARCH;
    DWORD_PTR eip = ep->ContextRecord->Eip;
    if (!(eip >= g_piBase && eip < g_piEnd) && !(eip >= g_crtBase && eip < g_crtEnd)) return EXCEPTION_CONTINUE_SEARCH;
#if CODOL_KERNEL_ROOM_CONTEXT
    if (PiMigrationGuardActive()) return EXCEPTION_CONTINUE_SEARCH;
#endif
    g_piFaultCode = code;
    g_piFaultEip = (DWORD)eip;
    ep->ContextRecord->Esp = ((DWORD)(DWORD_PTR)(g_piRescueStack + sizeof(g_piRescueStack) - 64)) & ~15u;
    ep->ContextRecord->Eip = (DWORD)(DWORD_PTR)PiRescue;
    return EXCEPTION_CONTINUE_EXECUTION;
}

/* A call into the server's API made from C1's side (p2passetcheck, the stage 3 asset provider) runs on the
 * server's fiber, between frames, like a frame: its stack, its error longjmps and its fault containment. */
static void (*volatile g_piCallFn)(void *);
static void *g_piCallCtx;

static void WINAPI PiServerFiber(void *unused)
{
    (void)unused;
    for (;;) {
        LONG st = g_piState;
        if (g_piCallFn) {
            void (*fn)(void *) = g_piCallFn;
            fn(g_piCallCtx);
            g_piCallFn = NULL;
        } else if (st == PI_INIT_PENDING) {
            DWORD t = GetTickCount();
            g_piInitResult = g_piInit(g_piArgs);
            g_piInitMs = GetTickCount() - t;
            InterlockedExchange(&g_piState, g_piInitResult == 0 ? PI_RUNNING : PI_ENDED);
            if (g_piInitResult == 0) {
                /* The +map on the command line loads in the first frame: one long frame now, while the host
                 * has just asked to host, rather than at an arbitrary later client frame. */
                t = GetTickCount();
                if (!g_piFrame()) InterlockedExchange(&g_piState, PI_ENDED);
                g_piFirstFrameMs = GetTickCount() - t;
            }
        } else if (st == PI_RUNNING) {
#if CODOL_LOCAL_TESTING
            /* p2pfaulttest: a real fault inside the server's code (Cbuf_AddText reading address 1). */
            if (g_piFaultTest) { g_piFaultTest = 0; g_piCmd((const char *)1); }
#endif
            if (g_piQuit == 1) { g_piCmd("quit\n"); g_piQuit = 2; }
            if (!g_piFrame()) InterlockedExchange(&g_piState, PI_ENDED);
        }
        SwitchToFiber(g_piMainFiber);
    }
}

#if CODOL_DIAGNOSTICS /* K33_SHIPPING_TIMING_GAP_STATE */
static LONGLONG g_piLastTickQpc, g_piMaxGapUs;
#else /* K33_SHIPPING_TIMING_GAP_STATE */
static LONGLONG g_piLastTickQpc;
#endif /* K33_SHIPPING_TIMING_GAP_STATE */
static volatile LONG g_piInTick;
#if CODOL_DIAGNOSTICS /* K33_SHIPPING_TIMING_GAP_COUNT */
static volatile LONG g_piGapsOver1s;
#endif /* K33_SHIPPING_TIMING_GAP_COUNT */

/* Main (game) thread: once per C1 Com_EventLoop pass, and from C1's waits (Sleep, SleepEx, PeekMessageA), so
 * the server keeps running while C1 loads a map or sits in a loop that skips Com_EventLoop. */
__attribute__((used)) void __cdecl PiTick(void)
{
    LONG st = g_piState;
    if (st != PI_INIT_PENDING && st != PI_RUNNING) return;
    if (g_piInTick) return;
    if(GetCurrentThreadId()!=g_p2pMainThread)return;
    if(!PiProductCurrent()) {
        /* Unknown callback/import mutation: never resume the retained server. */
        InterlockedExchange(&g_piQuit,1);InterlockedExchange(&g_piState,PI_CRASHED);return;
    }
#if CODOL_DIAGNOSTICS /* K33_SHIPPING_TIMING_GAP_SAMPLE */
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
        if (!g_piMainFiber) { Log("p2p in-process: ConvertThreadToFiber failed (%lu)", GetLastError()); InterlockedExchange(&g_piState, PI_ENDED); return; }
        g_piThread = GetCurrentThreadId();
    }
    if (GetCurrentThreadId() != g_piThread) return;
    if (!g_piFiber) {
        g_piFiber = CreateFiberEx(1u << 20, 32u << 20, FIBER_FLAG_FLOAT_SWITCH, PiServerFiber, NULL);
        if (!g_piFiber) { Log("p2p in-process: CreateFiberEx failed (%lu)", GetLastError()); InterlockedExchange(&g_piState, PI_ENDED); return; }
    }
    unsigned short fcw;
    unsigned int mxcsr;
    __asm__ volatile("fnstcw %0" : "=m"(fcw));
    __asm__ volatile("stmxcsr %0" : "=m"(mxcsr));
#if CODOL_DIAGNOSTICS /* K33_SHIPPING_TIMING_SAMPLE_DECL */
    LARGE_INTEGER a, b;
#else /* K33_SHIPPING_TIMING_SAMPLE_DECL */
    LARGE_INTEGER b;
#endif /* K33_SHIPPING_TIMING_SAMPLE_DECL */
#if CODOL_DIAGNOSTICS /* K33_SHIPPING_TIMING_SAMPLE_START */
    QueryPerformanceCounter(&a);
#endif /* K33_SHIPPING_TIMING_SAMPLE_START */
    g_piInTick = 1;
    SwitchToFiber(g_piFiber);
    g_piInTick = 0;
    QueryPerformanceCounter(&b);
    __asm__ volatile("fldcw %0" : : "m"(fcw));
    __asm__ volatile("ldmxcsr %0" : : "m"(mxcsr));
    g_piLastTickQpc = b.QuadPart;
#if CODOL_DIAGNOSTICS /* K33_SHIPPING_TIMING_SAMPLE_TOTALS */
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
    if (GetCurrentThreadId() != g_piThread || (g_piState != PI_RUNNING && g_piState != PI_INIT_PENDING) || !g_piQpf) return;
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
    if (g_piState != PI_RUNNING || !g_piFiber || GetCurrentThreadId() != g_piThread || g_piInTick || g_piYielding) return FALSE;
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

static volatile LONG g_piRetireFailed; /* p2p_inproc_retire.c */
/* A spent session blocks the next one only when it could not be taken down (p2p_inproc_retire.c). */
static BOOL P2PInprocUsed(void) { return g_piSession > 0 && (g_piState == PI_ENDED || g_piState == PI_CRASHED) && g_piRetireFailed; }
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
static DWORD g_piPhaseLast;
static void __cdecl PiHostPhaseYield(const char *phase)
{
    DWORD now = GetTickCount();
    Log("p2p in-process: map phase %s after %lu ms", phase ? phase : "unknown", now - g_piPhaseLast);
    g_piPhaseLast = now;
    PiYield();
}
/* The server's await_zones (codol_asset_host.h), on its fiber during a map load. C1 starts loading a hosted map
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
#include "p2p_inproc_retire.c"
static const void *__cdecl PiC1Provide(unsigned type, const char *name)
{
    if (*(volatile const DWORD *)(DWORD_PTR)0x02007A60u) return NULL;
    return PiC1Find(type, name);
}
static int P2PInprocRun(const P2PConfig *cp, const char *password, const char *args)
{
    P2PConfig c = *cp;
    if (!g_piHooked || !PiProductCurrent()) {
        Log("p2p in-process: the frame hook is not installed; not hosting");
        P2PSetState(P2P_FAILED, "no frame hook");
        return 1;
    }
    LONG st = g_piState;
    if (st == PI_INIT_PENDING || st == PI_RUNNING) { P2PSetState(P2P_FAILED, "a server is already running"); return 1; }
    /* One hosted session per client run (user, 2026-10-27). A session's server memory (its DLL copy, the PMem block,
     * the hunk) is not released, and a second session would not fit in the address space. */
    if (P2PInprocUsed()) {
        Log("p2p in-process: this client already hosted a match; restart the client to host again");
        P2PSetState(P2P_FAILED, "restart the client to host again");
        return 1;
    }
    /* The host compiles the GSC tree installed with the client (codol_scriptPath); P2PCheck refused
     * every host but the offline lobby's. */
    /* A private copy: a fresh set of globals for every session. */
    char copy[MAX_PATH], tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    LONG n = InterlockedIncrement(&g_piSession);
    _snprintf(copy, MAX_PATH, "%sk33_sh1x_host_%lu_%ld.dll", tmp, GetCurrentProcessId(), n);
    copy[MAX_PATH - 1] = 0;
    if (!CopyFileA(c.dll, copy, FALSE)) {
        Log("p2p in-process: cannot copy %s to %s (%lu)", c.dll, copy, GetLastError());
        P2PSetState(P2P_FAILED, "cannot copy the server DLL");
        return 1;
    }
    SIZE_T priv0, virt0, priv1, virt1;
    PiMemory(&priv0, &virt0);
    DWORD t0 = GetTickCount();
    HMODULE m = LoadLibraryExA(copy, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!m) {
        Log("p2p in-process: LoadLibrary %s failed (%lu)", copy, GetLastError());
        P2PSetState(P2P_FAILED, "the server DLL did not load");
        DeleteFileA(copy);
        return 1;
    }
    /* What the copy takes from the process is tracked from here on, for its teardown (p2p_inproc_retire.c). */
    BOOL tracked = PiTrackInstall(m);
    Sh1xInitFn init = (Sh1xInitFn)(void *)GetProcAddress(m, "Sh1xHost_Init");
    Sh1xFrameFn frame = (Sh1xFrameFn)(void *)GetProcAddress(m, "Sh1xHost_Frame");
    Sh1xCmdFn cmd = (Sh1xCmdFn)(void *)GetProcAddress(m, "Sh1xHost_Command");
    Sh1xCodeFn code = (Sh1xCodeFn)(void *)GetProcAddress(m, "Sh1xHost_ExitCode");
    if (!init || !frame || !cmd || !code) {
        Log("p2p in-process: %s lacks the Sh1xHost_* API (build the server's host-dll target)", c.dll);
        P2PSetState(P2P_FAILED, "the server DLL has no host API");
        FreeLibrary(m);
        DeleteFileA(copy);
        return 1;
    }
    PiModuleRange(m, &g_piBase, &g_piEnd);
    PiModuleRange(GetModuleHandleA("msvcrt.dll"), &g_crtBase, &g_crtEnd);
    /* Stage 3 of the server's asset layout: the server takes C1's own records for zones C1 loads once at startup
     * (the code_post_gfx set) instead of its copies. p2p_host.ini c1_assets = 0 turns it off. */
    {
        typedef void (__cdecl *Sh1xSetProviderFn)(const void *(__cdecl *)(unsigned, const char *));
        Sh1xSetProviderFn setProvider = (Sh1xSetProviderFn)(void *)GetProcAddress(m, "Sh1xHost_SetC1AssetProvider");
        if (setProvider && c.c1Assets) { setProvider(PiC1Provide); Log("p2p in-process: C1 asset provider installed"); }
        typedef void (__cdecl *Sh1xSetZoneHostFn)(int (__cdecl *)(const char *, const char *, unsigned), int (__cdecl *)(const char *));
        Sh1xSetZoneHostFn setZoneHost = (Sh1xSetZoneHostFn)(void *)GetProcAddress(m, "Sh1xHost_SetC1ZoneHost");
        g_piC1Preload = c.c1Preload;
        if (setZoneHost && c.c1Assets) { setZoneHost(PiAwaitZones, PiZoneHeld); Log("p2p in-process: C1 zone host installed"); }
        /* Zones C1 holds are not mounted: the server asks C1 where a definition is and mounts its own copy of a zone
         * only when a lookup needs one (p2p_host.ini c1_zones = 0: mount them all, as before). */
        typedef void (__cdecl *Sh1xSetZoneFindFn)(const char *(__cdecl *)(unsigned, const char *));
        Sh1xSetZoneFindFn setZoneFind = (Sh1xSetZoneFindFn)(void *)GetProcAddress(m, "Sh1xHost_SetC1ZoneFind");
        if (setZoneFind && c.c1Assets && c.c1Zones) { setZoneFind(PiZoneOf); Log("p2p in-process: C1-backed map zones on"); }
        typedef void (__cdecl *Sh1xSetScriptStringsFn)(const char *(__cdecl *)(unsigned));
        Sh1xSetScriptStringsFn setStrings = (Sh1xSetScriptStringsFn)(void *)GetProcAddress(m, "Sh1xHost_SetC1ScriptStrings");
        if (setStrings && c.c1Assets && c.c1Zones) setStrings(PiScriptString);
        typedef void (__cdecl *Sh1xSetYieldFn)(void (__cdecl *)(const char *));
        Sh1xSetYieldFn setYield = (Sh1xSetYieldFn)(void *)GetProcAddress(m, "Sh1xHost_SetYield");
        if (setYield) { setYield(PiHostPhaseYield); Log("p2p in-process: cooperative map-phase yield installed"); }
    }
    g_piInit = init; g_piFrame = frame; g_piCmd = cmd; g_piCode = code;
    InterlockedExchange(&g_piFirstMapAwait, 1);
    lstrcpynA(g_piArgs, args, sizeof(g_piArgs));
    if (g_piFiber && g_piState == PI_CRASHED) { DeleteFiber(g_piFiber); g_piFiber = NULL; }
    if (g_piFiber && g_piState == PI_ENDED) { DeleteFiber(g_piFiber); g_piFiber = NULL; }
#if CODOL_DIAGNOSTICS /* K33_SHIPPING_TIMING_START_TOTALS */
    g_piCalls = g_piTotalUs = g_piMaxUs = g_piWinCalls = g_piWinTotalUs = g_piWinMaxUs = 0;
#endif /* K33_SHIPPING_TIMING_START_TOTALS */
    g_piInitMs = g_piFirstFrameMs = 0;
    g_piPhaseLast = GetTickCount();
    g_piFaultCode = 0;
    InterlockedExchange(&g_piQuit, 0);
    EnterCriticalSection(&g_p2pLock);
    g_p2p.pid = GetCurrentProcessId();
    g_p2p.startTick = t0;
    LeaveCriticalSection(&g_p2pLock);
    Log("p2p in-process: %s loaded at %p (%lu KB image); hosting %s %s on %s:%d on the game thread", copy, (void *)m,
        (unsigned long)((g_piEnd - g_piBase) >> 10), c.map, c.gametype, c.netIp, c.port);
    InterlockedExchange(&g_piState, PI_INIT_PENDING);

    BOOL ready = FALSE, quitAsked = FALSE;
    int matchState = 0;
    int emptyPolls = 0;
#if CODOL_DIAGNOSTICS /* K33_SHIPPING_TIMING_MONITOR_DECL */
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
#if CODOL_DIAGNOSTICS /* K33_SHIPPING_TIMING_MONITOR_TOTALS */
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
        char reply[2048], line[512];
        if (P2PRcon(c.port, password, "sh1x_status", reply, sizeof(reply), 400) > 0 &&
            P2PStatusLine(reply, line, sizeof(line)) &&
            P2PStatusMatches(line, c.map, c.sessionMap, c.gametype, c.zoneType)) {
            lastAnswer = now;
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
             * readiness immediately; only fresh exact status restores it. */
            P2PSetState(P2P_STARTING, NULL);
        }
    }
    st = g_piState;
    EnterCriticalSection(&g_p2pLock);
    g_p2p.exitCode = st == PI_CRASHED ? g_piFaultCode : (DWORD)g_piCode();
    if (st == PI_CRASHED) { g_p2p.state = P2P_FAILED; lstrcpynA(g_p2p.error, "the server crashed (contained)", sizeof(g_p2p.error)); }
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
    if (st == PI_CRASHED)
        Log("p2p in-process: server fault 0x%08lX at 0x%08lX (%s+0x%lX) contained; the client continues. "
            "Its port stays bound until the client exits.", g_piFaultCode, g_piFaultEip,
            g_piFaultEip >= g_piBase && g_piFaultEip < g_piEnd ? "sh1x_host" : "msvcrt",
            (unsigned long)(g_piFaultEip - (g_piFaultEip >= g_piBase && g_piFaultEip < g_piEnd ? g_piBase : g_crtBase)));
    Log("p2p in-process: server %s (init %d, exit code %d) after %lu ms; %lld frames on the game thread, mean %lld us, max %lld us",
        kPiState[st], g_piInitResult, g_piCode(), GetTickCount() - t0, g_piCalls, g_piCalls ? g_piTotalUs / g_piCalls : 0, g_piMaxUs);
    /* Take the spent session down so that the next match can load a fresh copy (p2p_inproc_retire.c). */
    if (st == PI_ENDED || st == PI_CRASHED) {
        if (!tracked || !PiRetire(m, copy)) {
            InterlockedExchange(&g_piRetireFailed, 1);
            Log("p2p in-process: the session stays loaded; restart the client to host again");
        }
    }
    return 0;
}

/* p2pstatus adds the in-process numbers. */
static void __cdecl PiCmdStatus(void)
{
    P2PCmdStatus();
    SIZE_T priv, virt;
    PiMemory(&priv, &virt);
    C1Printf("p2p in-process: %s; init %lu ms, map load %lu ms; %lld frames, mean %lld us, max %lld us; process private %lu MB, address space %lu MB\n",
             kPiState[g_piState], g_piInitMs, g_piFirstFrameMs, g_piCalls, g_piCalls ? g_piTotalUs / g_piCalls : 0, g_piMaxUs,
             (unsigned long)priv, (unsigned long)virt);
}

#if CODOL_LOCAL_TESTING
/* Test clients only (+set k33_cmdFile 1): make the running server fault, to check the containment. */
static void __cdecl PiCmdFaultTest(void)
{
    const char *cl = GetCommandLineA();
    if (!cl || !strstr(cl, "k33_cmdFile 1")) { C1Printf("p2pfaulttest: test clients only (+set k33_cmdFile 1)\n"); return; }
    if (g_piState != PI_RUNNING) { C1Printf("p2pfaulttest: no in-process server running\n"); return; }
    InterlockedExchange(&g_piFaultTest, 1);
    C1Printf("p2pfaulttest: the server faults in its next frame\n");
}
static void __cdecl PiCmdAssetCheck(void); /* p2p_assets.c */
static void __cdecl PiCmdSlScan(void); /* p2p_assets.c */
static C1Cmd g_piCmds[] = { { NULL, "p2pfaulttest", (void *)PiCmdFaultTest }, { NULL, "p2passetcheck", (void *)PiCmdAssetCheck },
                            { NULL, "p2pslscan", (void *)PiCmdSlScan } };
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
#if CODOL_KERNEL_ROOM_CONTEXT
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
    if(install) {
        if(w->handler)return 0;
        w->handler=AddVectoredExceptionHandler(1,PiFaultHandler);
        return w->handler!=NULL;
    }
    if(!w->handler)return 1;
    if(!RemoveVectoredExceptionHandler(w->handler))return 0;
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
#if CODOL_LOCAL_TESTING
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
 * calls into or holds a pointer into sh1x_host.dll. 0: this client hosts nothing ("no match running"). */
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
