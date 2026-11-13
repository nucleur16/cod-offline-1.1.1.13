/*
 * p2p_inproc_retire.c - a hosted session is taken down completely, so the next match gets a fresh server
 * (included by p2p_inproc.c).
 *
 * The in-process server cannot be re-initialised: after Com_Init and a quit its globals are spent
 * (server/sh1x win32/sys_host.c: "One server per DLL instance ... the host loads a fresh copy of the DLL to
 * host again"). A fresh copy per match is what p2p_inproc.c always did; what kept a second match from
 * running was that the spent copy stayed in the process: its 228 MB image, the hunk and zone memory it
 * VirtualAlloc'd (about 730 MB of private memory and 1.2 GB of address space per session, measured) and its
 * threads. Two sessions do not fit in the 32-bit address space next to C1.
 *
 * So the copy's own imports are redirected, right after LoadLibrary and before Sh1xHost_Init, to wrappers that
 * remember what the server takes from the process:
 *   kernel32  VirtualAlloc / VirtualFree, CreateThread, CreateEventA, CreateFileA, CreateFileMappingA,
 *             MapViewOfFile / UnmapViewOfFile, CloseHandle
 *   msvcrt    malloc / calloc / realloc / free / _strdup (onto a private heap; a block carries a tag, so a
 *             pointer msvcrt itself allocated still goes back to msvcrt), _beginthread
 *   ws2_32    socket / accept / closesocket
 * When the session has ended (a quit, a fatal error or a contained crash) and C1's game thread is off the
 * server's fiber, PiRetire: deletes the fiber; ends the server's threads that are still there, each only while
 * it sits in a system call (a wait: the log writer, the capture worker, a thread Sys_HostExit parked), so no
 * lock of the process heap or the CRT is left owned; closes its sockets, views and handles; unloads the copy;
 * releases its remaining VirtualAlloc regions; destroys its heap; deletes the copy's file. Then the host is
 * back at PI_NONE and the next Ready loads a new copy. If a thread does not reach a wait within two seconds,
 * nothing is unloaded and the client keeps the old rule (restart before hosting again).
 */

#define PR_MAX_REGIONS 8192
#define PR_MAX_HANDLES 2048
#define PR_MAX_THREADS 128
#define PR_TAG 0x4B33A11Cu

typedef struct { void *base; } PrRegion;
static CRITICAL_SECTION g_prLock;
static BOOL g_prLockInit;
static HANDLE g_prHeap;
static void *g_prRegions[PR_MAX_REGIONS];
static int g_prRegionCount;
static HANDLE g_prHandles[PR_MAX_HANDLES];
static BYTE g_prHandleKind[PR_MAX_HANDLES]; /* 1 handle, 2 view, 3 socket */
static int g_prHandleCount;
static DWORD g_prThreads[PR_MAX_THREADS];
static int g_prThreadCount;
/* g_piRetireFailed (p2p_inproc.c): a session could not be taken down, the old one-per-run rule applies */
static volatile LONG g_piMatchOver;    /* the hosted match ended and its player left: p2p.c re-arms the room */
static int g_prPeakRegions, g_prPeakHandles;

static FARPROC g_prVirtualAlloc, g_prVirtualFree, g_prCreateThread, g_prCreateEventA, g_prCreateFileA, g_prCreateFileMappingA,
    g_prMapViewOfFile, g_prUnmapViewOfFile, g_prCloseHandle, g_prMalloc, g_prCalloc, g_prRealloc, g_prFree, g_prBeginThread,
    g_prSocket, g_prAccept, g_prCloseSocket;

static void PrAdd(void **table, int *count, int max, void *v, BYTE *kinds, BYTE kind)
{
    if (!v) return;
    EnterCriticalSection(&g_prLock);
    if (*count < max) { if (kinds) kinds[*count] = kind; table[(*count)++] = v; }
    LeaveCriticalSection(&g_prLock);
}

static BOOL PrRemove(void **table, int *count, void *v, BYTE *kinds)
{
    BOOL found = FALSE;
    EnterCriticalSection(&g_prLock);
    for (int i = *count - 1; i >= 0; i--)
        if (table[i] == v) {
            table[i] = table[*count - 1];
            if (kinds) kinds[i] = kinds[*count - 1];
            (*count)--;
            found = TRUE;
            break;
        }
    LeaveCriticalSection(&g_prLock);
    return found;
}

/* ---- the wrappers ---- */

static LPVOID WINAPI PrVirtualAlloc(LPVOID a, SIZE_T n, DWORD type, DWORD prot)
{
    LPVOID r = ((LPVOID(WINAPI *)(LPVOID, SIZE_T, DWORD, DWORD))g_prVirtualAlloc)(a, n, type, prot);
    if (r && (type & MEM_RESERVE)) {
        PrAdd(g_prRegions, &g_prRegionCount, PR_MAX_REGIONS, r, NULL, 0);
        if (g_prRegionCount > g_prPeakRegions) g_prPeakRegions = g_prRegionCount;
    }
    return r;
}

static BOOL WINAPI PrVirtualFree(LPVOID a, SIZE_T n, DWORD type)
{
    BOOL r = ((BOOL(WINAPI *)(LPVOID, SIZE_T, DWORD))g_prVirtualFree)(a, n, type);
    if (r && (type & MEM_RELEASE)) PrRemove(g_prRegions, &g_prRegionCount, a, NULL);
    return r;
}

static void PrAddHandle(HANDLE h, BYTE kind)
{
    if (!h || h == INVALID_HANDLE_VALUE) return;
    PrAdd(g_prHandles, &g_prHandleCount, PR_MAX_HANDLES, h, g_prHandleKind, kind);
    if (g_prHandleCount > g_prPeakHandles) g_prPeakHandles = g_prHandleCount;
}

static void PrAddThread(DWORD tid)
{
    if (!tid) return;
    EnterCriticalSection(&g_prLock);
    if (g_prThreadCount < PR_MAX_THREADS) g_prThreads[g_prThreadCount++] = tid;
    LeaveCriticalSection(&g_prLock);
}

static HANDLE WINAPI PrCreateThread(LPSECURITY_ATTRIBUTES sa, SIZE_T stack, LPTHREAD_START_ROUTINE fn, LPVOID arg, DWORD flags, LPDWORD tid)
{
    DWORD own = 0;
    HANDLE h = ((HANDLE(WINAPI *)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD))g_prCreateThread)(
        sa, stack, fn, arg, flags, tid ? tid : &own);
    if (h) { PrAddThread(tid ? *tid : own); PrAddHandle(h, 1); }
    return h;
}

typedef struct { void (__cdecl *fn)(void *); void *arg; } PrThreadStart;
static void __cdecl PrBeginThreadStub(void *p)
{
    PrThreadStart s = *(PrThreadStart *)p;
    HeapFree(GetProcessHeap(), 0, p);
    PrAddThread(GetCurrentThreadId());
    s.fn(s.arg);
}

static uintptr_t __cdecl PrBeginThread(void (__cdecl *fn)(void *), unsigned stack, void *arg)
{
    PrThreadStart *s = (PrThreadStart *)HeapAlloc(GetProcessHeap(), 0, sizeof(*s));
    if (!s) return (uintptr_t)-1;
    s->fn = fn;
    s->arg = arg;
    uintptr_t r = ((uintptr_t(__cdecl *)(void (__cdecl *)(void *), unsigned, void *))g_prBeginThread)(PrBeginThreadStub, stack, s);
    if (r == (uintptr_t)-1) HeapFree(GetProcessHeap(), 0, s);
    return r;
}

static HANDLE WINAPI PrCreateEventA(LPSECURITY_ATTRIBUTES sa, BOOL manual, BOOL initial, LPCSTR name)
{
    HANDLE h = ((HANDLE(WINAPI *)(LPSECURITY_ATTRIBUTES, BOOL, BOOL, LPCSTR))g_prCreateEventA)(sa, manual, initial, name);
    PrAddHandle(h, 1);
    return h;
}

static HANDLE WINAPI PrCreateFileA(LPCSTR p, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tmpl)
{
    HANDLE h = ((HANDLE(WINAPI *)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE))g_prCreateFileA)(p, acc, share, sa, disp, flags, tmpl);
    PrAddHandle(h, 1);
    return h;
}

static HANDLE WINAPI PrCreateFileMappingA(HANDLE f, LPSECURITY_ATTRIBUTES sa, DWORD prot, DWORD hi, DWORD lo, LPCSTR name)
{
    HANDLE h = ((HANDLE(WINAPI *)(HANDLE, LPSECURITY_ATTRIBUTES, DWORD, DWORD, DWORD, LPCSTR))g_prCreateFileMappingA)(f, sa, prot, hi, lo, name);
    PrAddHandle(h, 1);
    return h;
}

static LPVOID WINAPI PrMapViewOfFile(HANDLE m, DWORD acc, DWORD hi, DWORD lo, SIZE_T n)
{
    LPVOID v = ((LPVOID(WINAPI *)(HANDLE, DWORD, DWORD, DWORD, SIZE_T))g_prMapViewOfFile)(m, acc, hi, lo, n);
    PrAddHandle(v, 2);
    return v;
}

static BOOL WINAPI PrUnmapViewOfFile(LPCVOID v)
{
    PrRemove(g_prHandles, &g_prHandleCount, (void *)v, g_prHandleKind);
    return ((BOOL(WINAPI *)(LPCVOID))g_prUnmapViewOfFile)(v);
}

static BOOL WINAPI PrCloseHandle(HANDLE h)
{
    PrRemove(g_prHandles, &g_prHandleCount, h, g_prHandleKind);
    return ((BOOL(WINAPI *)(HANDLE))g_prCloseHandle)(h);
}

static SOCKET WSAAPI PrSocket(int af, int type, int proto)
{
    SOCKET s = ((SOCKET(WSAAPI *)(int, int, int))g_prSocket)(af, type, proto);
    if (s != INVALID_SOCKET) PrAddHandle((HANDLE)s, 3);
    return s;
}

static SOCKET WSAAPI PrAccept(SOCKET l, struct sockaddr *a, int *n)
{
    SOCKET s = ((SOCKET(WSAAPI *)(SOCKET, struct sockaddr *, int *))g_prAccept)(l, a, n);
    if (s != INVALID_SOCKET) PrAddHandle((HANDLE)s, 3);
    return s;
}

static int WSAAPI PrCloseSocket(SOCKET s)
{
    PrRemove(g_prHandles, &g_prHandleCount, (void *)s, g_prHandleKind);
    return ((int(WSAAPI *)(SOCKET))g_prCloseSocket)(s);
}

/* The heap: 8-byte header {tag ^ address, size}; 8-byte alignment as msvcrt's. */
static BOOL PrOwn(void *p)
{
    return p && ((const DWORD *)p)[-2] == (PR_TAG ^ (DWORD)(DWORD_PTR)p);
}

static void *__cdecl PrMalloc(size_t n)
{
    if (n > 0x7FFFFFF0u) return NULL;
    DWORD *b = (DWORD *)HeapAlloc(g_prHeap, 0, n + 8);
    if (!b) return NULL;
    b[0] = PR_TAG ^ (DWORD)(DWORD_PTR)(b + 2);
    b[1] = (DWORD)n;
    return b + 2;
}

static void *__cdecl PrCalloc(size_t n, size_t m)
{
    if (m && n > 0x7FFFFFF0u / m) return NULL;
    void *p = PrMalloc(n * m);
    if (p) memset(p, 0, n * m);
    return p;
}

static void __cdecl PrFree(void *p)
{
    if (!p) return;
    if (!PrOwn(p)) { ((void(__cdecl *)(void *))g_prFree)(p); return; }
    DWORD *b = (DWORD *)p - 2;
    b[0] = 0;
    HeapFree(g_prHeap, 0, b);
}

static void *__cdecl PrRealloc(void *p, size_t n)
{
    if (!p) return PrMalloc(n);
    if (!PrOwn(p)) return ((void *(__cdecl *)(void *, size_t))g_prRealloc)(p, n);
    if (!n) { PrFree(p); return NULL; }
    if (n > 0x7FFFFFF0u) return NULL;
    DWORD *b = (DWORD *)HeapReAlloc(g_prHeap, 0, (DWORD *)p - 2, n + 8);
    if (!b) return NULL;
    b[0] = PR_TAG ^ (DWORD)(DWORD_PTR)(b + 2);
    b[1] = (DWORD)n;
    return b + 2;
}

static char *__cdecl PrStrdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = (char *)PrMalloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* ---- installing into the copy's import table ---- */

typedef struct { const char *dll, *name; void *hook; FARPROC *real; } PrImport;

static int PrPatchImports(HMODULE m)
{
    PrImport list[] = {
        { "kernel32.dll", "VirtualAlloc", (void *)PrVirtualAlloc, &g_prVirtualAlloc },
        { "kernel32.dll", "VirtualFree", (void *)PrVirtualFree, &g_prVirtualFree },
        { "kernel32.dll", "CreateThread", (void *)PrCreateThread, &g_prCreateThread },
        { "kernel32.dll", "CreateEventA", (void *)PrCreateEventA, &g_prCreateEventA },
        { "kernel32.dll", "CreateFileA", (void *)PrCreateFileA, &g_prCreateFileA },
        { "kernel32.dll", "CreateFileMappingA", (void *)PrCreateFileMappingA, &g_prCreateFileMappingA },
        { "kernel32.dll", "MapViewOfFile", (void *)PrMapViewOfFile, &g_prMapViewOfFile },
        { "kernel32.dll", "UnmapViewOfFile", (void *)PrUnmapViewOfFile, &g_prUnmapViewOfFile },
        { "kernel32.dll", "CloseHandle", (void *)PrCloseHandle, &g_prCloseHandle },
        { "msvcrt.dll", "malloc", (void *)PrMalloc, &g_prMalloc },
        { "msvcrt.dll", "calloc", (void *)PrCalloc, &g_prCalloc },
        { "msvcrt.dll", "realloc", (void *)PrRealloc, &g_prRealloc },
        { "msvcrt.dll", "free", (void *)PrFree, &g_prFree },
        { "msvcrt.dll", "_strdup", (void *)PrStrdup, NULL },
        { "msvcrt.dll", "_beginthread", (void *)PrBeginThread, &g_prBeginThread },
        { "ws2_32.dll", "socket", (void *)PrSocket, &g_prSocket },
        { "ws2_32.dll", "accept", (void *)PrAccept, &g_prAccept },
        { "ws2_32.dll", "closesocket", (void *)PrCloseSocket, &g_prCloseSocket },
    };
    int patched = 0;
    BYTE *base = (BYTE *)m;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir->VirtualAddress) return 0;
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress); d->Name; d++) {
        const char *dll = (const char *)(base + d->Name);
        if (!d->OriginalFirstThunk || !d->FirstThunk) continue;
        IMAGE_THUNK_DATA *names = (IMAGE_THUNK_DATA *)(base + d->OriginalFirstThunk);
        void **slots = (void **)(base + d->FirstThunk);
        for (int i = 0; names[i].u1.AddressOfData; i++) {
            if (IMAGE_SNAP_BY_ORDINAL(names[i].u1.Ordinal)) continue;
            const char *fn = (const char *)((IMAGE_IMPORT_BY_NAME *)(base + names[i].u1.AddressOfData))->Name;
            for (unsigned k = 0; k < sizeof(list) / sizeof(list[0]); k++) {
                if (lstrcmpiA(dll, list[k].dll) || strcmp(fn, list[k].name)) continue;
                if (list[k].real) *list[k].real = (FARPROC)slots[i];
                void *hook = list[k].hook;
                WriteCode(&slots[i], &hook, sizeof(hook));
                patched++;
            }
        }
    }
    return patched;
}

/* p2p_inproc.c, after LoadLibrary of the copy and before Sh1xHost_Init. FALSE: the copy cannot be tracked. */
static BOOL PiTrackInstall(HMODULE m)
{
    if (!g_prLockInit) { InitializeCriticalSection(&g_prLock); g_prLockInit = TRUE; }
    g_prRegionCount = g_prHandleCount = g_prThreadCount = 0;
    g_prPeakRegions = g_prPeakHandles = 0;
    g_prHeap = HeapCreate(0, 0, 0);
    if (!g_prHeap) return FALSE;
    int n = PrPatchImports(m);
    Log("p2p in-process: %d imports of the server copy tracked for its teardown", n);
    return g_prVirtualAlloc && g_prVirtualFree && g_prCreateThread && g_prMalloc && g_prFree && g_prRealloc && g_prCalloc &&
           g_prBeginThread && g_prSocket && g_prCloseSocket && g_prCloseHandle;
}

/* A suspended thread whose next instruction returns from a system-call stub ("call edx" / "call [fs:..]"
 * then "ret n"): it waits in the kernel and holds no user-mode lock it is in the middle of taking. */
static BOOL PrInSyscall(HANDLE t)
{
    CONTEXT c;
    memset(&c, 0, sizeof(c));
    c.ContextFlags = CONTEXT_CONTROL;
    if (!GetThreadContext(t, &c)) return FALSE;
    const BYTE *ip = (const BYTE *)(DWORD_PTR)c.Eip;
    if (!Readable(ip - 7, 10)) return FALSE;
    HMODULE nt = GetModuleHandleA("ntdll.dll"), mod = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)ip, &mod) || mod != nt)
        return FALSE;
    return (ip[0] == 0xC2 || ip[0] == 0xC3) && ((ip[-2] == 0xFF && ip[-1] == 0xD2) || (ip[-7] == 0x64 && ip[-6] == 0xFF && ip[-5] == 0x15));
}

/* End the server's remaining threads; FALSE if one of them never reached a wait. */
static BOOL PrEndThreads(void)
{
    int ended = 0;
    for (int i = 0; i < g_prThreadCount; i++) {
        HANDLE t = OpenThread(THREAD_ALL_ACCESS, FALSE, g_prThreads[i]);
        if (!t) continue; /* already gone */
        if (WaitForSingleObject(t, 0) == WAIT_OBJECT_0) { CloseHandle(t); continue; }
        BOOL done = FALSE;
        for (int tries = 0; tries < 40 && !done; tries++) {
            if (WaitForSingleObject(t, tries ? 50 : 0) == WAIT_OBJECT_0) { done = TRUE; break; }
            if (SuspendThread(t) == (DWORD)-1) break;
            if (PrInSyscall(t)) {
                TerminateThread(t, 0);
                WaitForSingleObject(t, 2000);
                done = TRUE;
                ended++;
            } else {
                ResumeThread(t);
            }
        }
        CloseHandle(t);
        if (!done) {
            Log("p2p in-process: server thread %lu did not reach a wait; the session is not taken down", g_prThreads[i]);
            return FALSE;
        }
    }
    Log("p2p in-process: %d server thread(s) ended", ended);
    return TRUE;
}

/* p2p_inproc.c, on the host thread, after the session ended. TRUE: everything is back, a new session may start. */
static BOOL PiRetire(HMODULE m, const char *copy)
{
    SIZE_T priv0, virt0, priv1, virt1;
    PiMemory(&priv0, &virt0);
    /* C1's game thread stops entering the fiber once the state is not running; wait for a call in flight */
    for (int i = 0; i < 400 && (g_piInTick || g_piCallFn); i++) Sleep(5);
    if (g_piInTick || g_piCallFn) { Log("p2p in-process: the game thread is still on the server fiber; not taken down"); goto fail; }
    if (!PrEndThreads()) goto fail;
    if (g_piFiber) { DeleteFiber(g_piFiber); g_piFiber = NULL; }
    g_piInit = NULL; g_piFrame = NULL; g_piCmd = NULL; g_piCode = NULL;
    int sockets = 0, views = 0, handles = 0;
    EnterCriticalSection(&g_prLock);
    for (int i = 0; i < g_prHandleCount; i++) {
        if (g_prHandleKind[i] == 3) { ((int(WSAAPI *)(SOCKET))g_prCloseSocket)((SOCKET)g_prHandles[i]); sockets++; }
        else if (g_prHandleKind[i] == 2) { ((BOOL(WINAPI *)(LPCVOID))g_prUnmapViewOfFile)(g_prHandles[i]); views++; }
        else { ((BOOL(WINAPI *)(HANDLE))g_prCloseHandle)(g_prHandles[i]); handles++; }
    }
    g_prHandleCount = 0;
    LeaveCriticalSection(&g_prLock);
    char path[MAX_PATH];
    int unloads = 0;
    while (unloads < 16 && GetModuleFileNameA(m, path, sizeof(path)) && FreeLibrary(m)) unloads++;
    if (GetModuleFileNameA(m, path, sizeof(path))) { Log("p2p in-process: the server copy is still loaded after %d FreeLibrary", unloads); goto fail_unloaded; }
    g_piBase = g_piEnd = 0;
    int regions = g_prRegionCount;
    EnterCriticalSection(&g_prLock);
    for (int i = 0; i < g_prRegionCount; i++) ((BOOL(WINAPI *)(LPVOID, SIZE_T, DWORD))g_prVirtualFree)(g_prRegions[i], 0, MEM_RELEASE);
    g_prRegionCount = 0;
    LeaveCriticalSection(&g_prLock);
    if (g_prHeap) { HeapDestroy(g_prHeap); g_prHeap = NULL; }
    if (copy && copy[0]) DeleteFileA(copy);
    PiMemory(&priv1, &virt1);
    Log("p2p in-process: session taken down: copy unloaded (%d FreeLibrary), %d region(s) released, %d socket(s), %d view(s), "
        "%d handle(s) closed, heap destroyed; process private %lu -> %lu MB, address space %lu -> %lu MB",
        unloads, regions, sockets, views, handles, (unsigned long)priv0, (unsigned long)priv1, (unsigned long)virt0, (unsigned long)virt1);
    InterlockedExchange(&g_piState, PI_NONE);
    return TRUE;
fail:
fail_unloaded:
    InterlockedExchange(&g_piRetireFailed, 1);
    return FALSE;
}

/* p2p_inproc.c's status poll (every 2 s): the session is over once the match has ended (the level exit, ended=1: the
 * server then keeps its finished map, codol_oneShotMatch in p2p.c) and the player has left (no human for three polls),
 * or once nobody has been on it for a minute (the player went back to the lobby mid-match). A level exit alone is not
 * the end - a player still on the scoreboard would be dropped with "Server quit" - and a short gap is not either: during
 * the match's own map_restart ("waiting for teams", seen live) the server counted no human for several seconds.
 * *state: bit 0 a human was on it, bit 1 the level exit was seen; *emptyPolls counts consecutive empty polls. */
static BOOL PiMatchOverPoll(const char *line, int *state, int *emptyPolls)
{
    const char *h = strstr(line, " humans="), *e = strstr(line, " ended=");
    int humans = h ? atoi(h + 8) : -1, ended = e ? atoi(e + 7) : 0;
    if (ended) *state |= 2;
    if (humans > 0) { *state |= 1; *emptyPolls = 0; return FALSE; }
    if (!(*state & 1) || humans != 0) return FALSE;
    ++*emptyPolls;
    if ((*state & 2) && *emptyPolls >= 3) { Log("p2p in-process: the match ended and its player left; ending the session"); return TRUE; }
    if (*emptyPolls >= 30) { Log("p2p in-process: nobody has been on the match for a minute; ending the session"); return TRUE; }
    return FALSE;
}
