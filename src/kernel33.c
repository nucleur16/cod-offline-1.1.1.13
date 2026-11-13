/*
 * Kernel33.dll - runs the packed codoMP_client_shipRetail.exe (1.1.1.13) the way C1.exe runs,
 * with TenProtect never executing a single instruction.
 *
 * How the packed exe starts, measured with this DLL:
 *   1. The packer stub (VMProtect) maps an embedded TenProtect module ("TenPlat", 0x261000
 *      bytes, headerless) into one RWX VirtualAlloc and resolves its imports.
 *   2. It calls TenPlat's DllMain (entry +0xBAA61), then jumps to TenPlat's start routine
 *      (+0x20DD7) with (hExe, param block, hTen) and a fake return address of hExe.
 *   3. The start routine runs the protection (TenSafe_1.exe, driver, OS no newer than 8.1),
 *      decompresses the game code into the packer section (VA 0x08849000-0x09A10000), fills
 *      the game's imports and jumps to the game entry (RVA 0x6DFAFA from the param block).
 *
 * C1.exe is that process dumped at the game entry, plus C1.dll which fills the imports. This DLL
 * reproduces the dump instead of letting TenPlat run:
 *   - TenPlat's allocation is kept non-executable, so every jump into it faults (DEP);
 *   - DllMain gets TRUE back without running;
 *   - the start routine is replaced by TakeOver(): inflate C1.exe's copy of the decompressed
 *     range (embedded in this DLL as a resource by build.sh from make_exe.py's unpacked.z), fill the 424 import slots C1.dll
 *     fills (c1_tables.h), patch the render gate, and call the game entry.
 *
 * Render gate: C1.exe carries a one-byte patch in CG_DrawActiveFrame,
 *        test byte [eax+3368h], 0Ah ; jnz <skip all rendering>   (0x0A -> 0x00 @ 0x4787D1)
 * Our snapshots set snapFlags 0x08, inside that mask, so without it the world renders black.
 *
 * Loading: the packer's only import is Kernel32.dll!LoadLibraryA/GetProcAddress. make_exe.py
 * renames it to Kernel33.dll (same length), so this DLL loads first and sees every lookup the
 * stub makes; that is how VirtualAlloc/VirtualProtect are intercepted.
 */
#include <winsock2.h> /* before windows.h: discord.c's getinfo socket */
#include <windows.h>
#include <imm.h>
#include <shellapi.h>
#include "patch_locale_policy.h"
#include "production_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "c1_tables.h"

#define BUILD_TIMESTAMP 0x5314DEF0u /* 1.1.1.13, identical to C1.exe */
#define GAME_ENTRY_VA   0x00ADFAFAu /* C1.exe AddressOfEntryPoint 0x6DFAFA + image base */
#define RENDER_GATE_VA  0x004787CBu
#define UNPACKED_VA     0x08849000u /* must match make_exe.py */
#define UNPACKED_SIZE   0x011C7000u

static const BYTE kGate[] = { 0xF6, 0x80, 0x68, 0x33, 0x00, 0x00, 0x0A, 0x0F, 0x85 };
#define GATE_IMM_OFFSET 6

static HINSTANCE g_self;
static FARPROC g_loadLibraryA;
static FARPROC g_getProcAddress;
static FARPROC g_virtualAlloc;
static FARPROC g_virtualProtect;
static BOOL g_supportedBuild;
static BYTE *volatile g_tenBase;
static SIZE_T g_tenSize;

#if CODOL_DIAGNOSTICS
static void Log(const char *fmt, ...)
{
    char path[MAX_PATH], line[512];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    while (n && path[n - 1] != '\\') n--;
    lstrcpyA(path + n, "Kernel33.log");

    va_list ap;
    va_start(ap, fmt);
    int len = _vsnprintf(line, sizeof(line) - 2, fmt, ap);
    va_end(ap);
    if (len < 0) len = (int)sizeof(line) - 2;
    line[len++] = '\r';
    line[len++] = '\n';

    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w;
    WriteFile(h, line, (DWORD)len, &w, NULL);
    CloseHandle(h);
}
#else
/* Shipping has no diagnostic file writer or formatting on hot paths. */
#define Log(...) ((void)0)
#endif

static void Fatal(const char *msg)
{
    Log("FATAL: %s", msg);
    MessageBoxA(NULL, msg, "Kernel33", MB_ICONERROR);
    ExitProcess(1);
}

static BOOL Readable(const void *p, SIZE_T n)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return FALSE;
    return (const BYTE *)p + n <= (const BYTE *)mbi.BaseAddress + mbi.RegionSize;
}

static void WriteCode(void *dst, const void *src, SIZE_T n)
{
    DWORD old;
    VirtualProtect(dst, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(dst, src, n);
    VirtualProtect(dst, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), dst, n);
}

/* ---- the C1.exe reproduction ------------------------------------------------------------ */

/* The image travels inside this DLL as RCDATA 1 ("unpacked.z": 16-byte header, then zlib). */
static void LoadUnpackedImage(void)
{
    HRSRC res = FindResourceA(g_self, MAKEINTRESOURCEA(1), (LPCSTR)RT_RCDATA);
    HGLOBAL mem = res ? LoadResource(g_self, res) : NULL;
    const DWORD *hdr = mem ? (const DWORD *)LockResource(mem) : NULL;
    DWORD len = res ? SizeofResource(g_self, res) : 0;
    if (!hdr || len < 16)
        Fatal("Kernel33.dll has no embedded game image; rebuild it with build.sh.");
    if (hdr[0] != 0x5A33334Bu /* "K33Z" */ || hdr[1] != BUILD_TIMESTAMP ||
        hdr[2] != UNPACKED_VA || hdr[3] != UNPACKED_SIZE)
        Fatal("Kernel33.dll's embedded image does not match this 1.1.1.13 build.");

    BYTE *dst = (BYTE *)(DWORD_PTR)UNPACKED_VA;
    DWORD old;
    if (!VirtualProtect(dst, UNPACKED_SIZE, PAGE_EXECUTE_READWRITE, &old))
        Fatal("could not unprotect the packer section");
    uLongf outLen = UNPACKED_SIZE;
    int rc = uncompress(dst, &outLen, (const Bytef *)(hdr + 4), len - 16);
    if (rc != Z_OK || outLen != UNPACKED_SIZE)
        Fatal("Kernel33.dll's embedded game image is corrupt.");
    FlushInstructionCache(GetCurrentProcess(), dst, UNPACKED_SIZE);
    Log("unpacked image: 0x%lX bytes inflated over 0x%08X", (unsigned long)UNPACKED_SIZE, UNPACKED_VA);
}

#include "qa_background_window.c"

static void PatchActiveHandFiringMarker(void)
{
    static const BYTE expected[] = {
        0x81, 0xA3, 0xEC, 0x00, 0x00, 0x00, 0xFF, 0xFD, 0xFF, 0xFF, 0x83, 0x7B,
        0x04, 0x06, 0x8B, 0xB3, 0xEC, 0x00, 0x00, 0x00, 0x74, 0x3E, 0xF7, 0x43,
        0x0C, 0x00, 0x04, 0x00, 0x00, 0x75, 0x35, 0x8B, 0x83, 0x78, 0x02, 0x00,
        0x00, 0x85, 0xC0, 0x74, 0x14, 0x83, 0xF8, 0x06, 0x74, 0x0F, 0x8B, 0x83,
        0x94, 0x02, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x05, 0x83, 0xF8, 0x06, 0x75,
        0x17, 0x8B, 0xC7, 0xE8, 0x53, 0xFC, 0xFF, 0xFF, 0x85, 0xC0, 0x74, 0x0C,
        0x81, 0xCE, 0x00, 0x02, 0x00, 0x00, 0x89, 0xB3, 0xEC, 0x00, 0x00, 0x00,
    };
    static const BYTE replacement[] = {
        0x80, 0xA3, 0xED, 0x00, 0x00, 0x00, 0xFD, 0x8B, 0xB3, 0xEC, 0x00, 0x00,
        0x00, 0x83, 0x7B, 0x04, 0x06, 0x74, 0x41, 0xF6, 0x43, 0x0D, 0x04, 0x75,
        0x3B, 0x8B, 0x83, 0x78, 0x02, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x1B, 0x3C,
        0x06, 0x74, 0x17, 0x80, 0xBB, 0x5C, 0x03, 0x00, 0x00, 0x01, 0x75, 0x24,
        0x8B, 0x83, 0x94, 0x02, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x04, 0x3C, 0x06,
        0x75, 0x16, 0x8B, 0xC7, 0xE8, 0x52, 0xFC, 0xFF, 0xFF, 0x85, 0xC0, 0x74,
        0x0B, 0x66, 0x81, 0xCE, 0x00, 0x02, 0x89, 0xB3, 0xEC, 0x00, 0x00, 0x00,
    };
    BYTE *site = (BYTE *)(DWORD_PTR)0x00416289u;
    if (sizeof(expected) != 84 || sizeof(replacement) != sizeof(expected)) {
        Log("active hand firing marker: invalid replacement size, not installed");
        return;
    }
    if (!Readable(site, sizeof(expected))) {
        Log("active hand firing marker: unreadable region, not installed");
        return;
    }
    if (!memcmp(site, replacement, sizeof(replacement))) return;
    if (memcmp(site, expected, sizeof(expected))) {
        Log("active hand firing marker: C1 bytes differ, not installed");
        return;
    }
    WriteCode(site, replacement, sizeof(replacement));
    Log("active hand firing marker: inactive secondary readiness excluded");
}

#include "minigun_pair_native.c.inc"

#if defined(CODOL_LOCAL_TESTING) && CODOL_LOCAL_TESTING == 1
static FARPROC LocalQaRouteImport(const C1Import *imp,FARPROC original);
#endif
/* The lobby inside this DLL (offline_lobby.c): TRUE once it listens and C1 is routed to it. This client is
 * offline only, so that is before C1's entry point in every run; a start that fails stops the client. */
static BOOL OfflineLobbyActive(void);
#if CODOL_OFFLINE_LOBBY
static FARPROC OfflineRouteImport(const C1Import *imp, FARPROC original);
#endif
static void FillImports(void)
{
    int failed = 0;
    Log("imports: start (thread %lu)", GetCurrentThreadId());
    for (unsigned i = 0; i < sizeof(kC1Imports) / sizeof(kC1Imports[0]); i++) {
        const C1Import *imp = &kC1Imports[i];
        HMODULE m;
#if CODOL_KERNEL_NAME_DISPLAY
        if (!lstrcmpiA(imp->dll, "d3d9.dll")) {
            /* Full cutover bypasses the retired local proxy. Resolve only the
             * Windows copy, and let the guarded binders verify its path. */
            wchar_t system[MAX_PATH];
            UINT n = GetSystemDirectoryW(system, MAX_PATH);
            if (!n || n >= MAX_PATH - 10) Fatal("Kernel33 could not resolve Windows Direct3D.");
            lstrcatW(system, L"\\d3d9.dll");
            m = LoadLibraryW(system);
            if (!m) Fatal("Kernel33 could not load Windows Direct3D.");
        } else
#endif
        m = ((HMODULE(WINAPI *)(LPCSTR))g_loadLibraryA)(imp->dll);
        FARPROC f = m ? ((FARPROC(WINAPI *)(HMODULE, LPCSTR))g_getProcAddress)(m, imp->name) : NULL;
        if (!f) {
            Log("import %s!%s unresolved (slot 0x%08lX)", imp->dll, imp->name, (unsigned long)imp->slot);
            failed++;
            continue;
        }
        f = QbgImport(imp, f);
#if defined(CODOL_LOCAL_TESTING) && CODOL_LOCAL_TESTING == 1
        f = LocalQaRouteImport(imp,f);
#endif
#if CODOL_OFFLINE_LOBBY
        f = OfflineRouteImport(imp, f);
#endif
        WriteCode((void *)(DWORD_PTR)imp->slot, &f, sizeof(f));
    }
    Log("imports: %d slots filled, %d unresolved",
        (int)(sizeof(kC1Imports) / sizeof(kC1Imports[0])) - failed, failed);
}

static void PatchRenderGate(void)
{
    BYTE *gate = (BYTE *)RENDER_GATE_VA;
    if (!Readable(gate, sizeof(kGate)) || memcmp(gate, kGate, sizeof(kGate)) != 0) {
        Log("render gate: bytes at 0x%08X do not match, not patched", RENDER_GATE_VA);
        return;
    }
    BYTE zero = 0;
    WriteCode(gate + GATE_IMM_OFFSET, &zero, 1);
    Log("render gate: patched 0x%08X 0x0A -> 0x00", RENDER_GATE_VA + GATE_IMM_OFFSET);
}

/* The packer marks every section RWX in the PE header. The MSVC CRT reads those headers
 * (__IsNonwritableInCurrentImage) and skips floating-point init when _FPinit lies in a writable
 * section, which ends in R6002 "floating point support not loaded". Give the in-memory headers
 * C1.exe's flags: .text, .rdata, .data, .tls, .rsrc, then the three packer sections (C1's .tvm0).
 * Only the header changes; page protections stay as they are. */
static void MatchC1SectionFlags(void)
{
    static const DWORD kC1Flags[8] = {
        0x60000020, 0x40000040, 0xC0000040, 0xC0000040, 0x40000040, 0x60000020, 0x60000020, 0x60000020,
    };
    BYTE *base = (BYTE *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    if (nt->FileHeader.NumberOfSections < 8) return;
    DWORD old;
    VirtualProtect(sec, 8 * sizeof(*sec), PAGE_READWRITE, &old);
    for (int i = 0; i < 8; i++) sec[i].Characteristics = kC1Flags[i];
    VirtualProtect(sec, 8 * sizeof(*sec), old, &old);
    Log("section flags: set to C1.exe's");
}

/* Frame cap and FPS counter. The shipping client strips these names to numeric IDs and
 * registers both with flags 0, so a value from the command line, console or config is refused
 * (only archive dvars pass). Re-register them as archive (flag 0x1) with the defaults we want;
 * they then behave like any saved setting and can still be changed.
 *   1848 = com_maxfps: Dvar_RegisterInt(name, default 85, min 0, max 100, flags, desc)  @ 0x597E21
 *   591  = cg_drawFPS: Dvar_RegisterEnum(name, {Off, Simple, SimpleRanges, Verbose,
 *                      Verbose+Viewpos}, default 0, flags, desc)                        @ 0x44C631
 * Pushes are in reverse argument order: desc, flags, ..., name.
 * 591 is registered but nothing in C1 reads it (CG_DrawFPS is gone); the frame rate C1 can show
 * is the "FPS:" graph of its net/fps debug graphs, bool dvar "2614" (CL_Init @ 0x4984AA, drawn
 * by 0x49FA10 in a match). The 1848 domain is raised by PatchFrameCap below. */
typedef struct {
    const char *what;
    DWORD va;
    BYTE expect[18];
    BYTE len, flagsOff, defOff, newDefault;
} DvarRegPatch;

static const DvarRegPatch kDvarPatches[] = {
    { "1848 (com_maxfps) default 85 -> 0 (uncapped)", 0x00597E21,
      { 0x68, 0xAD, 0xFE, 0xC2, 0x00, 0x6A, 0x00, 0x6A, 0x64, 0x6A, 0x00, 0x6A, 0x55, 0x68, 0xDC, 0x18, 0xC6, 0x00 },
      18, 6, 12, 0 },
    { "591 (cg_drawFPS) default 0 -> 1 (Simple)", 0x0044C631,
      { 0x68, 0xAD, 0xFE, 0xC2, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x68, 0x8C, 0x6D, 0xCD, 0x00, 0x68, 0xC4, 0x2B, 0xC4 },
      18, 6, 8, 1 },
};

static void PatchDvarRegistrations(void)
{
    for (unsigned i = 0; i < sizeof(kDvarPatches) / sizeof(kDvarPatches[0]); i++) {
        const DvarRegPatch *d = &kDvarPatches[i];
        BYTE *reg = (BYTE *)(DWORD_PTR)d->va;
        if (!Readable(reg, d->len) || memcmp(reg, d->expect, d->len) != 0) {
            Log("dvar %s: bytes at 0x%08lX do not match, not patched", d->what, (unsigned long)d->va);
            continue;
        }
        BYTE archive = 0x01;
        WriteCode(reg + d->flagsOff, &archive, 1);
        WriteCode(reg + d->defOff, &d->newDefault, 1);
        Log("dvar %s, archive", d->what);
    }
}

/* players/config_mp.cfg in plaintext, so settings can be edited with the game closed.
 * Writer, Com_WriteConfigToFile @ 0x5991B0: writes the text, then only for "config_mp.cfg"
 * reads it back, AES-192-CTR encrypts it (0x59DD40, IV from crc32(name, len)) and rewrites it.
 *     0x599257  jne 0x5992E8   (name != config_mp.cfg: skip the encryption)  -> jmp
 * Loader @ 0x58C060: for config_mp.cfg it decrypts (call 0x59DDE0 @ 0x58C0CD; esi = buffer,
 * ebx = length, 4 dwords of args) and then requires a leading '/', else "%s is corrupted!".
 * The call goes through CfgDecryptHook instead: a file with no control bytes is plaintext and
 * goes straight to the exec path at 0x58C100 (no "//" header required); anything else, i.e. a
 * config written before this patch, is decrypted as before and becomes plaintext on next save. */
#define CFG_WRITE_JNE_VA   0x00599257u
#define CFG_DECRYPT_CALL_VA 0x0058C0CDu
#define CFG_DECRYPT_VA     0x0059DDE0u

__attribute__((used)) DWORD g_cfgDecrypt = CFG_DECRYPT_VA;

__attribute__((used)) int __cdecl CfgLooksPlain(const BYTE *p, DWORD n)
{
    for (DWORD i = 0; i < n; i++)
        if (p[i] < 0x20 ? !(p[i] == '\t' || p[i] == '\n' || p[i] == '\r') : p[i] == 0x7F)
            return 0;
    return 1;
}

void CfgDecryptHook(void);
__asm__(
    ".text\n"
    ".globl _CfgDecryptHook\n"
    "_CfgDecryptHook:\n"
    "    pushal\n"
    "    pushl %ebx\n"
    "    pushl %esi\n"
    "    call _CfgLooksPlain\n"
    "    addl $8, %esp\n"
    "    testl %eax, %eax\n"
    "    popal\n"
    "    jnz 1f\n"
    "    jmp *_g_cfgDecrypt\n"
    "1:  addl $0x14, %esp\n"          /* our return address + the caller's 4 argument dwords */
    "    movb $0, (%esi,%ebx)\n"
    "    pushl $0x0058C100\n"
    "    ret\n");

static void PatchPlainConfig(void)
{
    static const BYTE kJne[] = { 0x0F, 0x85, 0x8B, 0x00, 0x00, 0x00 };
    BYTE *jne = (BYTE *)(DWORD_PTR)CFG_WRITE_JNE_VA;
    BYTE *call = (BYTE *)(DWORD_PTR)CFG_DECRYPT_CALL_VA;
    LONG rel = (LONG)(CFG_DECRYPT_VA - (CFG_DECRYPT_CALL_VA + 5));
    if (!Readable(jne, sizeof(kJne)) || memcmp(jne, kJne, sizeof(kJne)) != 0 ||
        !Readable(call, 5) || call[0] != 0xE8 || memcmp(call + 1, &rel, 4) != 0) {
        Log("config_mp.cfg: bytes at 0x%08X / 0x%08X do not match, left encrypted",
            CFG_WRITE_JNE_VA, CFG_DECRYPT_CALL_VA);
        return;
    }
    static const BYTE kJmp[] = { 0x90, 0xE9 }; /* same rel32: the jmp ends where the jne did */
    WriteCode(jne, kJmp, sizeof(kJmp));
    rel = (LONG)((DWORD_PTR)CfgDecryptHook - (CFG_DECRYPT_CALL_VA + 5));
    WriteCode(call + 1, &rel, 4);
    Log("config_mp.cfg: saved as plaintext, plaintext or encrypted accepted on load");
}

/* Alt+Tab. The client installs a WH_KEYBOARD_LL hook (0x6331E0) whose "hotkey block mode"
 * (0x633270, logged as "New hotkey block mode: %d") is 2 whenever the window is active and
 * 0x726B60 returns false, i.e. in-game. Mode 2 eats Ctrl+Esc, Alt+Tab, Alt+Enter and both Win
 * keys, so players cannot switch away. Only Alt+Tab is released:
 *     0x63322B  cmp eax, 9 ; jne 0x633235 ; test [esi+8], 20h (LLKHF_ALTDOWN) ; jne <eat>
 * The jne becomes jmp, so Tab never reaches the eat branch; the other blocks are untouched. */
#define ALTTAB_CHECK_VA 0x0063322Bu

static void PatchAltTab(void)
{
    static const BYTE kCheck[] = { 0x83, 0xF8, 0x09, 0x75, 0x05, 0x84, 0x56, 0x08, 0x75, 0xEB };
    BYTE *check = (BYTE *)(DWORD_PTR)ALTTAB_CHECK_VA;
    if (!Readable(check, sizeof(kCheck)) || memcmp(check, kCheck, sizeof(kCheck)) != 0) {
        Log("alt+tab: bytes at 0x%08X do not match, still blocked in-game", ALTTAB_CHECK_VA);
        return;
    }
    BYTE jmp = 0xEB;
    WriteCode(check + 3, &jmp, 1);
    Log("alt+tab: keyboard hook no longer eats Alt+Tab");
}

/* Raw mouse input. IN_MouseMove (0x62D990) reads the Windows cursor every frame: GetCursorPos,
 * delta against the last position, recenter. That delta has the Windows pointer curve
 * ("Enhance pointer precision") baked in, and a flick that reaches the window edge before the
 * next recenter is clipped. The client never touches the mouse SPI settings, so both land on aim.
 *     0x62D9FF  push edi (dy) ; push esi (dx) ; push ecx (y) ; push edx (x) ; call 0x48FD40
 * 0x48FD40 feeds dx/dy to the view angles in-game (menus use only x/y) and returns 1 when the
 * caller should recenter. The call goes through MouseEventHook instead: until raw input is
 * registered it passes all four arguments through; after that dx/dy are the relative WM_INPUT
 * counts since the last frame, (0, 0) when there were none. The cursor deltas are never used
 * again, or the pointer curve leaks back in. Absolute packets (remote desktop, tablets, some
 * overlays) are ignored rather than switching back. Recentering still follows the cursor, so the
 * cursor stays inside the window. The game's own curve, cl_mouseAccel ("972", default 0, in
 * CL_GetMouseMovement 0x48FF40), still applies on top when a player has set it. */
#define MOUSEMOVE_CALL_VA  0x0062DA03u
#define MOUSE_EVENT_VA     0x0048FD40u
#define MAIN_HWND_VA       0x02B68104u

typedef int (__cdecl *MouseEventFn)(int x, int y, int dx, int dy);

static HWND g_rawHwnd;
static WNDPROC g_rawPrevProc;
static BOOL g_rawActive;
static volatile LONG g_rawDx, g_rawDy;
static LONG g_rawPackets, g_rawAbsPackets;

static LRESULT CALLBACK RawWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INPUT) {
        RAWINPUT ri;
        UINT size = sizeof(ri);
        if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
            ri.header.dwType == RIM_TYPEMOUSE) {
            if (ri.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
                if (g_rawAbsPackets++ == 0) Log("raw mouse: ignoring absolute-position packets");
            } else {
                if (g_rawPackets++ == 0) Log("raw mouse: first relative packet received");
                InterlockedExchangeAdd(&g_rawDx, ri.data.mouse.lLastX);
                InterlockedExchangeAdd(&g_rawDy, ri.data.mouse.lLastY);
            }
        }
    }
    return CallWindowProcA(g_rawPrevProc, hwnd, msg, wp, lp);
}

/* Put the previous window's procedure back, if that window still exists and still has ours. */
static void DetachRawMouse(void)
{
    if (g_rawHwnd && g_rawPrevProc && IsWindow(g_rawHwnd) &&
        (WNDPROC)GetWindowLongPtrA(g_rawHwnd, GWLP_WNDPROC) == RawWndProc) {
        SetWindowLongPtrA(g_rawHwnd, GWLP_WNDPROC, (LONG_PTR)g_rawPrevProc);
        Log("raw mouse: detached from window %p", g_rawHwnd);
    }
    g_rawHwnd = NULL;
    g_rawPrevProc = NULL;
    g_rawActive = FALSE;
}

/* The main window can be recreated (vid_restart, fullscreen toggle): attach to whichever is current. */
static void AttachRawMouse(HWND hwnd)
{
    DetachRawMouse();
    g_rawHwnd = hwnd;
    InterlockedExchange(&g_rawDx, 0);
    InterlockedExchange(&g_rawDy, 0);
    WNDPROC prev = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)RawWndProc);
    if (!prev) {
        Log("raw mouse: could not subclass window %p (%lu), cursor deltas kept", hwnd, GetLastError());
        return;
    }
    g_rawPrevProc = prev;
    RAWINPUTDEVICE rid = { 0x01 /* generic desktop */, 0x02 /* mouse */, 0, hwnd };
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        Log("raw mouse: RegisterRawInputDevices failed (%lu), cursor deltas kept", GetLastError());
        return; /* stays subclassed, forwarding everything; no WM_INPUT arrives */
    }
    g_rawActive = TRUE;
    Log("raw mouse: attached to window %p", hwnd);
}

/* An overlay that owns the mouse: no view turn, no menu cursor and no recentering while it is open, so the
 * Windows cursor moves freely over it. Nothing sets it at present (the in-game console takes the keyboard only). */
static volatile LONG g_k33OverlayMouse;

static int __cdecl MouseEventHook(int x, int y, int dx, int dy)
{
    HWND hwnd = *(HWND *)(DWORD_PTR)MAIN_HWND_VA;
    if (hwnd && hwnd != g_rawHwnd) AttachRawMouse(hwnd);
    if (g_k33OverlayMouse) {
        InterlockedExchange(&g_rawDx, 0);
        InterlockedExchange(&g_rawDy, 0);
        return 0;
    }
    if (g_rawActive) {
        dx = InterlockedExchange(&g_rawDx, 0);
        dy = InterlockedExchange(&g_rawDy, 0);
    }
    return ((MouseEventFn)(DWORD_PTR)MOUSE_EVENT_VA)(x, y, dx, dy);
}

static void PatchRawMouse(void)
{
    static const BYTE kPushes[] = { 0x57, 0x56, 0x51, 0x52 };
    BYTE *call = (BYTE *)(DWORD_PTR)MOUSEMOVE_CALL_VA;
    LONG rel = (LONG)(MOUSE_EVENT_VA - (MOUSEMOVE_CALL_VA + 5));
    if (!Readable(call - 4, 9) || memcmp(call - 4, kPushes, 4) != 0 || call[0] != 0xE8 ||
        memcmp(call + 1, &rel, 4) != 0) {
        Log("raw mouse: bytes at 0x%08X do not match, cursor deltas kept", MOUSEMOVE_CALL_VA);
        return;
    }
    rel = (LONG)((DWORD_PTR)MouseEventHook - (MOUSEMOVE_CALL_VA + 5));
    WriteCode(call + 1, &rel, 4);
    Log("raw mouse: view deltas come from WM_INPUT");
}

/* Scoreboard after focus loss while loading. The scoreboard is a function of the Scaleform HUD
 * movie scaleform/default/codohud.gfx, which is loaded exactly once per map, when the client
 * enters the game: 0x5EC8EF calls the UI mode switch 0x83DA993(local, 12), whose Scaleform half
 * (0x6B77A0, case 12 -> 0x6B55D0) loads the movie into 0x810C4F4/0x810C50C. 0x6B77A0 refuses
 * every open while the D3D device is lost (0xA7A6C0: no device, or lost flag 0xCEF828) or
 * Scaleform is off (0x6C44D5C). Alt+Tab out of a fullscreen client during the load screen keeps
 * the device lost until the player comes back, so the one open is refused, codohud never loads,
 * and for the rest of the map TAB (CG_ScoresDown 0x436EE0 -> 0x83DA993(local, 7) ->
 * cpp_showScoreboard on the missing movie) shows nothing, although "s" is sent and answered.
 * The next map loads it again.
 * CG_ScoresDown's open call now goes through ScoresOpenHook, which first repeats the enter-game
 * open when the movie is missing and Scaleform is usable again. Nothing else changes: with the
 * movie present the hook is the original call. */
#define SCORES_OPEN_CALL_VA 0x00436F90u /* call 0x83DA993 ; push 7 / push local precede it */
#define UI_MODE_VA          0x083DA993u
#define HUD_MOVIE_VA        0x0810C4F4u
#define HUD_MOVIE_DEF_VA    0x0810C50Cu
#define SCALEFORM_ON_VA     0x06C44D5Cu
#define GFX_UNUSABLE_VA     0x00A7A6C0u
#define UI_MODE_HUD         12

typedef void (__cdecl *UiModeFn)(int localClient, int mode);
typedef unsigned char (__cdecl *GfxUnusableFn)(void);

static void __cdecl ScoresOpenHook(int localClient, int mode)
{
#if CODOL_DIAGNOSTICS
    static LONG reloads;
#endif
    if (!*(volatile DWORD *)(DWORD_PTR)HUD_MOVIE_VA && !*(volatile DWORD *)(DWORD_PTR)HUD_MOVIE_DEF_VA &&
        *(volatile DWORD *)(DWORD_PTR)SCALEFORM_ON_VA && !((GfxUnusableFn)(DWORD_PTR)GFX_UNUSABLE_VA)()) {
        ((UiModeFn)(DWORD_PTR)UI_MODE_VA)(localClient, UI_MODE_HUD);
#if CODOL_DIAGNOSTICS
        if (InterlockedIncrement(&reloads) <= 8)
            Log("scoreboard: HUD movie was not loaded (focus lost while loading?), reloaded it: %s",
                *(volatile DWORD *)(DWORD_PTR)HUD_MOVIE_VA ? "ok" : "still missing");
#endif
    }
    ((UiModeFn)(DWORD_PTR)UI_MODE_VA)(localClient, mode);
}

static void PatchScoresOpen(void)
{
    static const BYTE kPushes[] = { 0x6A, 0x07, 0x51 }; /* push 7 ; push ecx */
    BYTE *call = (BYTE *)(DWORD_PTR)SCORES_OPEN_CALL_VA;
    LONG rel = (LONG)(UI_MODE_VA - (SCORES_OPEN_CALL_VA + 5));
    if (!Readable(call - 3, 8) || memcmp(call - 3, kPushes, 3) != 0 || call[0] != 0xE8 ||
        memcmp(call + 1, &rel, 4) != 0) {
        Log("scoreboard: bytes at 0x%08X do not match, HUD reload on TAB not installed", SCORES_OPEN_CALL_VA);
        return;
    }
    rel = (LONG)((DWORD_PTR)ScoresOpenHook - (SCORES_OPEN_CALL_VA + 5));
    WriteCode(call + 1, &rel, 4);
    Log("scoreboard: TAB reloads the HUD movie when a lost device skipped it");
}

/* Unsigned zones from disk. DB_LoadXFile (0x4B5D50) reads the header and sets signed = 0 for
 * "IWffu100", then refuses it unless the stream has no file handle, i.e. unless it is the
 * lobby's fastfile of the day served from memory:
 *     0x4B5EFB  cmp eax, ebx ; jne 0x4B5F04 (signed) ; cmp [stream.handle], -1 ; jne 0x4B5F1A (error)
 * Past that test nothing depends on where the bytes come from: 0x4B5AC0 records signed = 0 and
 * decrypts the first 0x40000-byte chunk's input once (0x4B5B7D), the reader (0x4B6790) fills
 * chunks with ReadFileEx for a handle or memcpy for a buffer, and 0x4B69E0 inflates unsigned
 * chunks directly (signed ones go through 0x4B5840). The jne becomes two nops, so an unsigned
 * zone loads from a file like any other; signed zones are untouched. Any client_shipRetail zone
 * may now be an unsigned build, in the same layout tools/codol_english/build_font_ff.py writes. */
#define UNSIGNED_GATE_VA 0x004B5EF5u

static BOOL PatchUnsignedZones(void)
{
    static const BYTE kGate[] = { 0x8B, 0x0D, 0x80, 0x68, 0x7C, 0x01, 0x3B, 0xC3, 0x75, 0x05,
                                  0x83, 0x39, 0xFF, 0x75, 0x16 };
    static const BYTE kNops[] = { 0x90, 0x90 };
    BYTE *gate = (BYTE *)(DWORD_PTR)UNSIGNED_GATE_VA;
    if (!Readable(gate, sizeof(kGate)) || memcmp(gate, kGate, sizeof(kGate)) != 0) {
        Log("unsigned zones: bytes at 0x%08X do not match, disk zones must be signed", UNSIGNED_GATE_VA);
        return FALSE;
    }
    WriteCode(gate + 13, kNops, sizeof(kNops));
    Log("unsigned zones: IWffu100 zones load from disk");
    return TRUE;
}

/* Patch folder: client_shipRetail\patch\*.ff loaded ahead of the stock zones, from disk.
 * DB_TryLoadXFileInternal (0x4CD260, called per queued zone by the DB thread at 0x4CD8E5) opens
 * a zone as CreateFile("<zone dir>\<name>.ff", GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
 * FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING) at 0x4CD44E, then 0x4CD55E fills the zone
 * entry {handle, remaining 0, cursor 0, name[64], ..., allocFlags @+0x4C, GetFileSize @+0x9C}.
 *     0x4CD27F  strcmp(name, "ffotd_client") ; 0x4CD2AF jne 0x4CD3E3 (the disk path)
 * Two changes, both byte-checked, and only once PatchUnsignedZones is in:
 *   - the jne at 0x4CD2AF goes to PatchDirOpenHook: a name that is one of the patch files is
 *     opened from patch\ with the same flags and joins 0x4CD55E as a disk zone; any other name
 *     continues to 0x4CD3E3 exactly as before. "ffotd_client" never gets here.
 *   - the code_pre_gfx list's DB_LoadXAssets call (0x59841C, 4 zones, alloc flags 1) goes to
 *     PreGfxLoadHook, which prepends the patch zones (same alloc flags) to the list. C1 keeps the
 *     first entry it links under a name, so their assets win over every stock zone.
 * Files are taken in alphabetical order; each must start IWffu100 version 0x14E. Nothing is held
 * in memory: C1's own streaming reader reads the file on every load.
 * The project's zones are consolidated, one per list and language (build_zones.py, zone_packs/): codol_common (ahead
 * of code_pre_gfx), codol_common_ui_mp (UI list), codol_common_postgfx_mp and codol_infected_cyborg_postgfx_mp (after
 * code_post_gfx), codol_english_locale / codol_chinese_locale (ahead of code_pre_gfx, one language). An old zone name
 * (patch_locale_policy.h kCodolPatchZoneMerged) is skipped while the zone that replaced it is in the folder.
 * A file named like a stock zone (or ffotd_client) is skipped, so it can never shadow one; to
 * replace a stock zone, put the unsigned build in its stock folder instead.
 * "+set codol_patchdir 0" on the command line turns the folder off.
 *
 * Patch zones named *_ui_mp (the stock UI zones' naming: patch_ui_mp, ui_mp) are prepended instead,
 * the same way, to the UI list's DB_LoadXAssets call
 * (0x5985CD: patch_ui_mp, ui_mp, ... 7 zones, alloc flags 2), which loads once the renderer is up and
 * again whenever the front end comes back. Image assets belong there: an image in a zone that loads
 * ahead of code_pre_gfx crashes the client (AV read 0x0000000C at 0x00A92825, 2026-10-14).
 *
 * Patch zones named *_postgfx_mp are appended instead to the code_post_gfx list's DB_LoadXAssets call
 * (0x5984D2: 5 zones, patch_code_post_gfx_mp ... localized_code_post_gfx_mp, alloc flags 1): they load once
 * the renderer is up, after C1's own code assets, and stay loaded (flags 1 are never freed, while the UI
 * list's flags 2 are freed on match entry at 0x598631..0x59864A). Assets needed in the front end and in a
 * match, images included, go there (controller_glyphs_postgfx_mp: the pad button icons). The same suffix
 * and list as codol-classic's Kernel33 (its codol_infected_cyborg_postgfx_mp). */
#define PATCHDIR_OPEN_JNE_VA   0x004CD2AFu
#define PATCHDIR_DISK_PATH_VA  0x004CD3E3u
#define PATCHDIR_OPENED_VA     0x004CD55Eu
#define PREGFX_LOAD_CALL_VA    0x0059841Cu
#define UI_LOAD_CALL_VA        0x005985CDu
#define POSTGFX_LOAD_CALL_VA   0x005984D2u
#define DB_LOAD_XASSETS_VA     0x08465768u
#define PATCHDIR_MAX_FILES     32

typedef struct { const char *name; int allocFlags; int freeFlags; } XZoneInfo;
typedef void (__cdecl *LoadXAssetsFn)(XZoneInfo *info, unsigned count, int sync);

enum { PATCH_PRE_GFX, PATCH_UI, PATCH_POST_GFX };
typedef struct { char name[64]; char path[MAX_PATH]; int stage; } PatchZone;

static PatchZone g_patchZones[PATCHDIR_MAX_FILES];
static int g_patchZoneCount;
__attribute__((used)) DWORD g_patchDirDiskPath = PATCHDIR_DISK_PATH_VA;
__attribute__((used)) DWORD g_patchDirOpenedPath = PATCHDIR_OPENED_VA;

/* "+set codol_patchdir 0" anywhere on the command line; the last setting wins. */
static BOOL PatchDirEnabled(void)
{
    const char *p = GetCommandLineA();
    BOOL on = TRUE;
    while ((p = strstr(p, "codol_patchdir")) != NULL) {
        p += 14;
        if (*p != ' ' && *p != '\t' && *p != '"') continue;
        while (*p == ' ' || *p == '\t' || *p == '"') p++;
        on = !(p[0] == '0' && (p[1] == '\0' || p[1] == ' ' || p[1] == '\t' || p[1] == '"'));
    }
    return on;
}

/* English (launch option k33_english). Both languages use the same UI movies (voice_ui_mp, controller_ingame: every
 * text in them is a localize key); the language is which string zones load. English is the patch zones named
 * codol_english* (build_zones.py: codol_english = localized strings, codol_english_font = the Latin glyphs of
 * fonts_ingame.gfx, codol_english_ui / codol_english_ui_extra = the UI movies' K33UI_* keys, codol_english_zombies_base
 * = zombies strings, codol_english_usermaps = the usermaps' names); they load ahead of code_pre_gfx, so their strings
 * win over the stock Chinese ones. Chinese is C1's own strings plus the zones named codol_chinese* (the UI movies'
 * K33UI_* keys in Chinese). Only one set loads: C1 holds at most 12000 localize assets, and a name loaded by both sets
 * would take two. The language is decided once, at startup, from the command line:
 *     +set k33_english 1    codol_english* load, codol_chinese* are skipped (default, also without the option)
 *     +set k33_english 0    codol_chinese* load, codol_english* are skipped; every other patch zone loads either way
 * C1 execs config_mp.cfg before any of this and applies no +set to a dvar it does not know, so the option is read
 * here and nothing stores it: a config write can never change it. The last setting on the line wins. Nothing else
 * in the DLL depends on the language (GBK text input, fonts, voice, controller work the same in both). The pure
 * check lists client_shipRetail\<language>\ and main\ only, never patch\, so both states pass it. */
static BOOL g_english = TRUE;
static int g_englishZones, g_englishSkipped, g_chineseZones;

static BOOL EnglishZone(const char *name)
{
    return _strnicmp(name, "codol_english", 13) == 0;
}

static BOOL ChineseZone(const char *name)
{
    return _strnicmp(name, "codol_chinese", 13) == 0;
}

/* True when client_shipRetail\<language folder>\<file> exists, i.e. the name is a stock zone. A language folder is one
 * that holds code_pre_gfx_mp.ff (chinese\): other folders there are no zone folder C1 loads from, e.g. a
 * client_shipRetail\temp\ left by the English zone tools with old copies of codol_english*.ff, which made the patch
 * folder skip the real ones as "stock". */
static BOOL StockZoneExists(const char *dir, const char *file)
{
    char pattern[MAX_PATH];
    WIN32_FIND_DATAA fd;
    _snprintf(pattern, sizeof(pattern), "%s*", dir);
    pattern[sizeof(pattern) - 1] = 0;
    HANDLE h = FindFirstFileA(pattern, &fd);
    BOOL found = FALSE;
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.' ||
            lstrcmpiA(fd.cFileName, "patch") == 0)
            continue;
        char path[MAX_PATH];
        _snprintf(path, sizeof(path), "%s%s\\code_pre_gfx_mp.ff", dir, fd.cFileName);
        path[sizeof(path) - 1] = 0;
        if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) continue;
        _snprintf(path, sizeof(path), "%s%s\\%s", dir, fd.cFileName, file);
        path[sizeof(path) - 1] = 0;
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) found = TRUE;
    } while (!found && FindNextFileA(h, &fd));
    FindClose(h);
    return found;
}

static int CompareNames(const void *a, const void *b)
{
    return lstrcmpiA((const char *)a, (const char *)b);
}

/* "*_postgfx_mp" -> after the code_post_gfx zones, "*_ui_mp" -> ahead of the UI zones, anything else ahead of
 * code_pre_gfx */
static int PatchZoneStage(const char *name)
{
    size_t len = strlen(name);
    if (len > 11 && lstrcmpiA(name + len - 11, "_postgfx_mp") == 0) return PATCH_POST_GFX;
    if (len > 6 && lstrcmpiA(name + len - 6, "_ui_mp") == 0) return PATCH_UI;
    return PATCH_PRE_GFX;
}

static void LoadPatchDir(void)
{
    int localeArgc=0;
    LPWSTR *localeArgv=CommandLineToArgvW(GetCommandLineW(),&localeArgc);
    enum CodolPatchLanguage locale=localeArgv ? CodolPatchLanguageFromArgv(localeArgc,(const wchar_t *const*)localeArgv) : CODOL_PATCH_NATIVE;
    if(localeArgv)LocalFree(localeArgv);
    g_english = locale == CODOL_PATCH_EN;
    const char *sub = "patch\\";
    char dir[MAX_PATH], pattern[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, dir, MAX_PATH);
    while (n && dir[n - 1] != '\\') n--;
    lstrcpynA(dir + n, "client_shipRetail\\", MAX_PATH - n);
    char pdir[MAX_PATH];   /* where the patch files are: client_shipRetail\patch\ */
    _snprintf(pdir, sizeof(pdir), "%s%s", dir, sub);
    pdir[sizeof(pdir) - 1] = 0;
#if CODOL_LOCAL_TESTING
    {   /* test clients ("+set k33_cmdFile 1") with a k33patch\ folder next to the exe load that one instead,
         * so a UI build can be tried while the shared client_shipRetail\patch\ is in use */
        const char *cl = GetCommandLineA();
        char test[MAX_PATH];
        memcpy(test, dir, n);
        lstrcpynA(test + n, "k33patch\\", MAX_PATH - n);
        DWORD at = GetFileAttributesA(test);
        if (cl && strstr(cl, "k33_cmdFile 1") && at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY)) {
            lstrcpynA(pdir, test, sizeof(pdir));
            Log("patch folder: test client: using %s", pdir);
        }
    }
#endif
    _snprintf(pattern, sizeof(pattern), "%s*.ff", pdir);
    pattern[sizeof(pattern) - 1] = 0;

    static char files[64][MAX_PATH];
    int count = 0;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        Log("patch folder: no .ff files in %s", pdir);
        return;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (count == 64) { Log("patch folder: more than 64 files, the rest are ignored"); break; }
        lstrcpynA(files[count++], fd.cFileName, MAX_PATH);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    qsort(files, count, sizeof(files[0]), CompareNames);

    for (int i = 0; i < count; i++) {
        const char *file = files[i];
        size_t len = strlen(file);
        if (len < 4 || lstrcmpiA(file + len - 3, ".ff") != 0) continue; /* "*.ff" also matches "*.ffx" */
        if (g_patchZoneCount == PATCHDIR_MAX_FILES) {
            Log("patch folder: skipped %s: only %d files are loaded", file, PATCHDIR_MAX_FILES);
            continue;
        }
        PatchZone *z = &g_patchZones[g_patchZoneCount];
        if (len - 3 >= sizeof(z->name)) { Log("patch folder: skipped %s: name too long", file); continue; }
        memcpy(z->name, file, len - 3);
        z->name[len - 3] = 0;
        if (!CodolPatchZoneLocaleAllowed(z->name,locale)) {
            if (EnglishZone(z->name)) g_englishSkipped++;
            else if (CodolPatchZoneRetired(z->name))
                Log("patch folder: skipped %s: retired duplicate map table", file);
            continue;
        }
        if (lstrcmpiA(z->name, "ffotd_client") == 0) {
            Log("patch folder: skipped %s: the name belongs to the lobby's FFOTD", file);
            continue;
        }
        {   /* an old zone whose assets a consolidated zone in the folder carries now (patch_locale_policy.h) */
            const char *into = CodolPatchZoneMergedInto(z->name);
            BOOL there = FALSE;
            char want[MAX_PATH];
            if (into) {
                _snprintf(want, sizeof(want), "%s.ff", into);
                want[sizeof(want) - 1] = 0;
                for (int j = 0; j < count && !there; j++) there = lstrcmpiA(files[j], want) == 0;
            }
            if (there) {
                Log("patch folder: skipped %s: merged into %s", file, want);
                continue;
            }
        }
        BOOL dup = FALSE;
        for (int j = 0; j < g_patchZoneCount; j++) dup |= lstrcmpiA(g_patchZones[j].name, z->name) == 0;
        if (dup || StockZoneExists(dir, file)) {
            Log("patch folder: skipped %s: a %s zone of that name exists", file, dup ? "patch" : "stock");
            continue;
        }

        _snprintf(z->path, sizeof(z->path), "%s%s", pdir, file);
        z->stage = PatchZoneStage(z->name);
        z->path[sizeof(z->path) - 1] = 0;
        HANDLE f = CreateFileA(z->path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (f == INVALID_HANDLE_VALUE) {
            Log("patch folder: skipped %s: open failed (%lu)", file, GetLastError());
            continue;
        }
        BYTE hdr[12];
        DWORD got = 0;
#if CODOL_DIAGNOSTICS
        DWORD size = GetFileSize(f, NULL);
#endif
        BOOL ok = ReadFile(f, hdr, sizeof(hdr), &got, NULL) && got == sizeof(hdr);
        CloseHandle(f);
        if (!ok || memcmp(hdr, "IWffu100", 8) != 0 || *(DWORD *)(hdr + 8) != 0x14E) {
            Log("patch folder: skipped %s: not an IWffu100 version 0x14E zone", file);
            continue;
        }
        g_patchZoneCount++;
        g_englishZones += EnglishZone(z->name);
        g_chineseZones += ChineseZone(z->name);
        Log("patch folder: %s%s (%lu bytes) queued as zone \"%s\"", sub, file, (unsigned long)size, z->name);
    }
}

static void LogEnglish(void)
{
    if (!g_english)
        Log("english: off (k33_english 0): %d codol_english* zone(s) skipped, %d codol_chinese* zone(s) queued",
            g_englishSkipped, g_chineseZones);
    else if (g_englishZones)
        Log("english: on (k33_english 1): %d codol_english* zone(s) queued", g_englishZones);
    else
        Log("english: on (k33_english 1), but no codol_english*.ff zone is in the patch folder: the game stays Chinese");
}

static int PatchZonesIn(int stage)
{
    int n = 0;
    for (int i = 0; i < g_patchZoneCount; i++) n += g_patchZones[i].stage == stage;
    return n;
}

/* Called on the DB thread with the name DB_TryLoadXFileInternal is about to open from disk.
 * Returns the open patch file, or INVALID_HANDLE_VALUE when the name is not a patch zone. */
__attribute__((used)) HANDLE __cdecl PatchDirOpen(const char *name)
{
    for (int i = 0; i < g_patchZoneCount; i++) {
        if (lstrcmpiA(g_patchZones[i].name, name) != 0) continue;
        HANDLE f = CreateFileA(g_patchZones[i].path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL);
        if (f == INVALID_HANDLE_VALUE)
            Log("patch folder: zone \"%s\": open failed (%lu)", name, GetLastError());
        else
            Log("patch folder: zone \"%s\" loading from %s", name, g_patchZones[i].path);
        return f;
    }
    return INVALID_HANDLE_VALUE;
}

void PatchDirOpenHook(void);
__asm__(
    ".text\n"
    ".globl _PatchDirOpenHook\n"
    "_PatchDirOpenHook:\n"            /* ebx = zone name, edi = 0, ebp = the caller's frame */
    "    pushl %ebx\n"
    "    call _PatchDirOpen\n"
    "    addl $4, %esp\n"
    "    cmpl $-1, %eax\n"
    "    je 1f\n"
    "    movl %eax, %esi\n"            /* the handle, as CreateFileA @ 0x4CD454 leaves it */
    "    movl %esi, -0x218(%ebp)\n"
    "    movl %edi, -0x214(%ebp)\n"    /* remaining 0, cursor 0: a disk zone */
    "    movl %edi, -0x210(%ebp)\n"
    "    jmp *_g_patchDirOpenedPath\n"
    "1:  jmp *_g_patchDirDiskPath\n");

/* The stage's patch zones ahead of the stock list (pre-gfx, UI: their assets win) or after it (post-gfx: new
 * assets that may refer to C1's own code assets), with the list's alloc flags. */
static void SubmitPatchZones(int stage, const char *list, XZoneInfo *info, unsigned count, int sync)
{
    XZoneInfo all[PATCHDIR_MAX_FILES + 8];
    unsigned n = 0;
    if (stage == PATCH_POST_GFX)
        for (unsigned i = 0; i < count && n < sizeof(all) / sizeof(all[0]); i++) all[n++] = info[i];
    for (int i = 0; i < g_patchZoneCount; i++) {
        if (g_patchZones[i].stage != stage) continue;
        all[n].name = g_patchZones[i].name;
        all[n].allocFlags = info[0].allocFlags;
        all[n].freeFlags = 0;
        n++;
    }
    if (stage != PATCH_POST_GFX)
        for (unsigned i = 0; i < count && n < sizeof(all) / sizeof(all[0]); i++) all[n++] = info[i];
    Log("patch folder: %s list: %d patch zone(s) %s %u stock", list, PatchZonesIn(stage),
        stage == PATCH_POST_GFX ? "after" : "ahead of", count);
    ((LoadXAssetsFn)(DWORD_PTR)DB_LOAD_XASSETS_VA)(all, n, sync);
}

static void __cdecl PreGfxLoadHook(XZoneInfo *info, unsigned count, int sync)
{
    SubmitPatchZones(PATCH_PRE_GFX, "code_pre_gfx", info, count, sync);
}

static void __cdecl UiLoadHook(XZoneInfo *info, unsigned count, int sync)
{
    SubmitPatchZones(PATCH_UI, "ui", info, count, sync);
}

static volatile BOOL g_postGfxSubmitted, g_postGfxHooked;

static void __cdecl PostGfxLoadHook(XZoneInfo *info, unsigned count, int sync)
{
    if (count != 5 || !info || info[0].allocFlags != 1 || info[0].freeFlags != 0) {
        Log("patch folder: the code_post_gfx list differs (%u zones), *_postgfx_mp patch zones not loaded", count);
        ((LoadXAssetsFn)(DWORD_PTR)DB_LOAD_XASSETS_VA)(info, count, sync);
        return;
    }
    SubmitPatchZones(PATCH_POST_GFX, "code_post_gfx", info, count, sync);
    g_postGfxSubmitted = TRUE;
}

/* Post-gfx patch zone `name`: -1 not in the patch folder (or not loadable), 0 queued, 1 handed to DB_LoadXAssets
 * in the code_post_gfx list. controller_glyphs.c looks its materials up only at 1, so a missing zone never makes
 * C1 register default materials. */
static int PatchZoneState(const char *name)
{
    for (int i = 0; i < g_patchZoneCount; i++)
        if (g_patchZones[i].stage == PATCH_POST_GFX && lstrcmpiA(g_patchZones[i].name, name) == 0)
            return g_postGfxSubmitted ? 1 : g_postGfxHooked ? 0 : -1;
    return -1;
}

/* 0x598440 builds the code_post_gfx list (last store 0x5984CC mov [ebp-0x16C], ecx) and calls DB_LoadXAssets
 * at 0x5984D2. */
static void PatchPostGfxLoad(void)
{
    static const BYTE kPostList[] = { 0x89, 0x8D, 0x94, 0xFE, 0xFF, 0xFF };
    BYTE *call = (BYTE *)(DWORD_PTR)POSTGFX_LOAD_CALL_VA;
    LONG rel = (LONG)(DB_LOAD_XASSETS_VA - (POSTGFX_LOAD_CALL_VA + 5));
    if (!Readable(call - 6, 11) || memcmp(call - 6, kPostList, 6) != 0 || call[0] != 0xE8 || memcmp(call + 1, &rel, 4) != 0) {
        Log("patch folder: bytes at 0x%08X do not match, *_postgfx_mp patch zones not loaded", POSTGFX_LOAD_CALL_VA);
        return;
    }
    rel = (LONG)((DWORD_PTR)PostGfxLoadHook - (POSTGFX_LOAD_CALL_VA + 5));
    WriteCode(call + 1, &rel, 4);
    g_postGfxHooked = TRUE;
    Log("patch folder: %d zone(s) load after code_post_gfx", PatchZonesIn(PATCH_POST_GFX));
}

static void PatchPatchDir(void)
{
    if (!PatchUnsignedZones()) {
        Log("patch folder: not installed (unsigned zones cannot load from disk)");
        return;
    }
    if (!PatchDirEnabled()) {
        Log("patch folder: off (codol_patchdir 0)");
        return;
    }
    static const BYTE kCmpJne[] = { 0x33, 0xFF, 0x3B, 0xC7, 0x0F, 0x85, 0x2E, 0x01, 0x00, 0x00 };
    static const BYTE kList[] = { 0x89, 0x8D, 0x88, 0xFE, 0xFF, 0xFF }; /* the list's last store */
    BYTE *jne = (BYTE *)(DWORD_PTR)PATCHDIR_OPEN_JNE_VA;
    BYTE *call = (BYTE *)(DWORD_PTR)PREGFX_LOAD_CALL_VA;
    LONG rel = (LONG)(DB_LOAD_XASSETS_VA - (PREGFX_LOAD_CALL_VA + 5));
    if (!Readable(jne - 4, sizeof(kCmpJne)) || memcmp(jne - 4, kCmpJne, sizeof(kCmpJne)) != 0 ||
        !Readable(call - 6, 11) || memcmp(call - 6, kList, 6) != 0 || call[0] != 0xE8 ||
        memcmp(call + 1, &rel, 4) != 0) {
        Log("patch folder: bytes at 0x%08X / 0x%08X do not match, not installed",
            PATCHDIR_OPEN_JNE_VA, PREGFX_LOAD_CALL_VA);
        return;
    }
    LoadPatchDir();
    LogEnglish();
    if (!g_patchZoneCount) return;
    rel = (LONG)((DWORD_PTR)PatchDirOpenHook - (PATCHDIR_OPEN_JNE_VA + 6));
    WriteCode(jne + 2, &rel, 4);
    if (PatchZonesIn(PATCH_PRE_GFX)) {
        rel = (LONG)((DWORD_PTR)PreGfxLoadHook - (PREGFX_LOAD_CALL_VA + 5));
        WriteCode(call + 1, &rel, 4);
    }
    Log("patch folder: %d zone(s) load from disk ahead of code_pre_gfx", PatchZonesIn(PATCH_PRE_GFX));
    if (PatchZonesIn(PATCH_POST_GFX)) PatchPostGfxLoad();
    if (!PatchZonesIn(PATCH_UI)) return;
    static const BYTE kUiList[] = { 0x89, 0xB5, 0xB0, 0xFE, 0xFF, 0xFF }; /* the UI list's last store */
    BYTE *ui = (BYTE *)(DWORD_PTR)UI_LOAD_CALL_VA;
    rel = (LONG)(DB_LOAD_XASSETS_VA - (UI_LOAD_CALL_VA + 5));
    if (!Readable(ui - 6, 11) || memcmp(ui - 6, kUiList, 6) != 0 || ui[0] != 0xE8 || memcmp(ui + 1, &rel, 4) != 0) {
        Log("patch folder: bytes at 0x%08X do not match, *_ui_mp patch zones not loaded", UI_LOAD_CALL_VA);
        return;
    }
    rel = (LONG)((DWORD_PTR)UiLoadHook - (UI_LOAD_CALL_VA + 5));
    WriteCode(ui + 1, &rel, 4);
    Log("patch folder: %d zone(s) load from disk ahead of the UI zones", PatchZonesIn(PATCH_UI));
}

/* GPU rating. Com_Init registers "2658" (the benchmark score) as Dvar_RegisterInt(default 0,
 * min 0, max 20000, archive) @ 0x5988C8 and then calls 0x597DA0: when the saved score is 0, or
 * the saved GPU name ("2289") differs from the detected one, it runs the 2014 GPU benchmark
 0xA7A1E0 (call @ 0x597DFD, logs "GPU rating: %d") and Dvar_SetInt()s the result.
 * Any current card scores 30000-50000, outside the domain, so the set is refused
 * ("'37087' is not a valid value for dvar '2658'") and the score stays 0. Autoconfigure
 * (0x597AE0, configure_mp.csv; the gpu_rating rows @ 0x5976D0 apply the first row whose
 * rating <= "2658") then falls off the bottom of the table: com_belowMinSpec 1 and the
 * lowest preset, which saves r_picmip_limit 1, r_picmip_bump_limit 2, r_picmip_spec_limit 2
 * (and r_specular 0, sm_enable 0, ...). R_PickPicmip (0xA92DF0) floors r_picmip/_bump/_spec
 * at those *_limit dvars in automatic AND manual mode, so every texture loads one mip down,
 * normal and specular maps two, and the menu cannot raise it. The score is never saved, so
 * the benchmark runs again on every launch.
 *   1. The benchmark result is clamped to the dvar's maximum, so it is stored and the
 *      benchmark stops re-running.
 *   2. Autoconfigure is re-run once in the launch where that happens: the "has the hardware
 *      changed" check 0x596560 (called @ 0x597D31 when com_recommendedSet "1908" is already
 *      set) also answers yes. That replaces the stored low preset (and its *_limit floors)
 *      with the top-rated row. Later launches find a nonzero score and skip both. */
#define GPU_RATING_MAX          20000
#define BENCH_CALL_VA           0x00597DFDu
#define BENCH_FUNC_VA           0x00A7A1E0u
#define HWCHANGED_CALL_VA       0x00597D31u
#define HWCHANGED_FUNC_VA       0x00596560u
#define GPU_RATING_REG_VA       0x005988C8u

static BOOL g_reconfigure;

static int __cdecl BenchmarkHook(void)
{
    int rating = ((int(__cdecl *)(void))(DWORD_PTR)BENCH_FUNC_VA)();
    if (rating > GPU_RATING_MAX) {
        Log("gpu rating: benchmark scored %d, stored as %d; autoconfigure re-runs once",
            rating, GPU_RATING_MAX);
        rating = GPU_RATING_MAX;
    }
    if (rating > 0) g_reconfigure = TRUE; /* a failed benchmark (0) would re-run it forever */
    return rating;
}

static int __cdecl HardwareChangedHook(void)
{
    int changed = ((unsigned char(__cdecl *)(void))(DWORD_PTR)HWCHANGED_FUNC_VA)();
    return (changed & 0xFF) || g_reconfigure;
}

static BOOL CallsTarget(DWORD va, DWORD target)
{
    const BYTE *call = (const BYTE *)(DWORD_PTR)va;
    LONG rel = (LONG)(target - (va + 5));
    return Readable(call, 5) && call[0] == 0xE8 && memcmp(call + 1, &rel, 4) == 0;
}

static void RedirectCall(DWORD va, void *hook)
{
    LONG rel = (LONG)((DWORD_PTR)hook - (va + 5));
    WriteCode((BYTE *)(DWORD_PTR)va + 1, &rel, 4);
}

/* Client-side loop effects. At map load CG parses maps/mp/<map>_fx.gsc and maps/createfx/<map>_fx.gsc
 * (0x42DF70) and registers every loop effect (0x42C930) in a per-local-client table at
 * 0x16B01C0 + lc * 0x304: int count, int next[64] (+4), { int period, fx }[64] (+0x104), with
 * next = cg time + period. cg time is still 0 then. Each frame 0x42E1B0 (called from 0x478F0F and
 * 0x49F8C7) replays every period that has passed:
 *     0x42E300  while (next[i] < now) { 0x4FE820(next[i]) ; next[i] += period[i] }
 * so the first frame after CA_ACTIVE spawns (server time / period) effects per loop effect. Server
 * time counts from the server process start: after 15 h (55,767,470 ms) 42 loop effects of 500 ms are
 * 4.7 million spawns, and the client stood on a blank screen for 0.3-1.7 s after the loading screen
 * (2026-10-15, measured between Com_EventLoop passes). Before both calls, an effect more than
 * LOOPFX_REPLAY_MS behind is moved forward by whole periods, so the original loop replays at most
 * that much of its past (long-lived effects still look as if they had been running). */
#define LOOPFX_TABLE_VA       0x016B01C0u
#define LOOPFX_TABLE_STRIDE   0x304u
#define LOOPFX_MAX            64
#define LOOPFX_PROCESS_VA     0x0042E1B0u
#define LOOPFX_CALL_CG_VA     0x00478F0Fu
#define LOOPFX_CALL_CL_VA     0x0049F8C7u
#define LOOPFX_CG_INDEX_VA    0x06C3D2ACu /* the cg lookup 0x42E1B0 itself uses: the slot is chosen at random */
#define LOOPFX_CG_SLOTS_MAX   1024u       /* per run (seen 0 and 4), so it is only sanity-checked */
#define LOOPFX_CG_TABLE_VA    0x016D3B58u
#define LOOPFX_CG_KEY         0x490B400Cu
#define LOOPFX_CG_TIME_OFF    0x000B0350u
#define LOOPFX_REPLAY_MS      10000

typedef void (__cdecl *LoopFxProcessFn)(int localClient);

static void __cdecl LoopFxProcessHook(int localClient)
{
#if CODOL_DIAGNOSTICS
    static LONG logged;
#endif
    DWORD idx = *(volatile DWORD *)(DWORD_PTR)LOOPFX_CG_INDEX_VA;
    const BYTE *cg = NULL;
    if (idx < LOOPFX_CG_SLOTS_MAX && Readable((const void *)(DWORD_PTR)(LOOPFX_CG_TABLE_VA + idx * 4), 4))
        cg =(const BYTE *)(DWORD_PTR)(*(volatile DWORD *)(DWORD_PTR)(LOOPFX_CG_TABLE_VA + idx * 4) ^ LOOPFX_CG_KEY);
    if (cg && (unsigned)localClient < 4 && Readable(cg + LOOPFX_CG_TIME_OFF, 4)) {
        int now = *(const volatile int *)(cg + LOOPFX_CG_TIME_OFF);
        BYTE *t = (BYTE *)(DWORD_PTR)(LOOPFX_TABLE_VA + (DWORD)localClient * LOOPFX_TABLE_STRIDE);
        int count = *(int *)t;
#if CODOL_DIAGNOSTICS
        int moved = 0;
#endif
        int *next = (int *)(t + 4);
#if CODOL_DIAGNOSTICS
        long long skipped = 0, worst = 0;
#endif
        for (int i = 0; i < count && i < LOOPFX_MAX; i++) {
            int period = *(int *)(t + 0x104 + i * 8);
            long long behind = (long long)now - LOOPFX_REPLAY_MS - next[i];
            if (period <= 0 || behind <= 0) continue;
            long long steps = behind / period + 1;
#if CODOL_DIAGNOSTICS
            if (behind > worst) worst = behind;
#endif
            next[i] = (int)(next[i] + steps * period);
#if CODOL_DIAGNOSTICS
            skipped += steps;
            moved++;
#endif
        }
#if CODOL_DIAGNOSTICS
        if (moved && InterlockedIncrement(&logged) <= 8)
            Log("loop fx: %d of %d effects were up to %lld ms behind cg time %d; skipped %lld replays", moved, count,
                worst + LOOPFX_REPLAY_MS, now, skipped);
#endif
    } else {
#if CODOL_DIAGNOSTICS
        static LONG warned;
        if (InterlockedIncrement(&warned) == 1)
            Log("loop fx: cg slot %lu (local client %d) not usable, catch-up not limited", (unsigned long)idx, localClient);
#endif
    }
    ((LoopFxProcessFn)(DWORD_PTR)LOOPFX_PROCESS_VA)(localClient);
}

static void PatchLoopFxCatchUp(void)
{
    if (!CallsTarget(LOOPFX_CALL_CG_VA, LOOPFX_PROCESS_VA) || !CallsTarget(LOOPFX_CALL_CL_VA, LOOPFX_PROCESS_VA)) {
        Log("loop fx: calls at 0x%08X / 0x%08X do not match, catch-up limit not installed", LOOPFX_CALL_CG_VA,
            LOOPFX_CALL_CL_VA);
        return;
    }
    RedirectCall(LOOPFX_CALL_CG_VA, (void *)LoopFxProcessHook);
    RedirectCall(LOOPFX_CALL_CL_VA, (void *)LoopFxProcessHook);
    Log("loop fx: a joining client replays at most %d ms of each loop effect", LOOPFX_REPLAY_MS);
}

/* Localize asset pool. C1 holds localize entries (asset type 27) in a static pool of 12000: g_poolSize[27] = 12000 @
 * 0xCD4DD4, DB_XAssetPool[27] = 0x01FF0358 @ 0xCD5094, a {freeHead, entries[12000]} block of 8-byte entries. C1's
 * own strings (localized_code_post_gfx_mp 7602 names, patch_localized_code_post_gfx_mp 693, the gametype's and the
 * map's), codol_english's (9705, 1966 of them names C1 does not have) and the UI keys came to over 12000 on a zombies
 * map in English: "Exceeded limit of 12000 'localize' assets" (0x4CB1DA), a fatal error. The pools
 * are set up by DB_InitPoolHeaders (0x4CB240: for each type, init[type](DB_XAssetPool[type], g_poolSize[type]); the
 * localize init 0x4CECC0 threads the free list over size 8-byte entries) at DB_Init, after this runs, and every
 * other user (DB_AllocXAssetHeader 0x4CB180, the enumerators) reads both tables, so pointing them at a larger
 * block is all it takes. LOCALIZE_POOL entries: twice the stock size. */
#define LOCALIZE_TYPE        27
#define POOL_SIZE_TABLE_VA   0x00CD4D68u
#define POOL_TABLE_VA        0x00CD5028u
#define LOCALIZE_STOCK_POOL  0x01FF0358u
#define LOCALIZE_POOL        24000

static void PatchLocalizePool(void)
{
    int *size = (int *)(DWORD_PTR)(POOL_SIZE_TABLE_VA + 4 * LOCALIZE_TYPE);
    DWORD *pool = (DWORD *)(DWORD_PTR)(POOL_TABLE_VA + 4 * LOCALIZE_TYPE);
    if (!Readable(size, 4) || !Readable(pool, 4) || *size != 12000 || *pool != LOCALIZE_STOCK_POOL) {
        Log("localize pool: tables at 0x%08X / 0x%08X do not match, the pool stays at 12000", (unsigned)(DWORD_PTR)size,
            (unsigned)(DWORD_PTR)pool);
        return;
    }
    void *block = VirtualAlloc(NULL, 4 + (SIZE_T)LOCALIZE_POOL * 8, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!block) {
        Log("localize pool: VirtualAlloc failed (%lu), the pool stays at 12000", GetLastError());
        return;
    }
    DWORD newPool = (DWORD)(DWORD_PTR)block;
    int newSize = LOCALIZE_POOL;
    WriteCode(pool, &newPool, 4);
    WriteCode(size, &newSize, 4);
    Log("localize pool: %d entries at %p (C1's: 12000)", newSize, block);
}

static void PatchGpuRating(void)
{
    /* push desc ; push 1 (archive) ; push 20000 ; push ebx ; push ebx ; push "2658" */
    static const BYTE kReg[] = { 0x68, 0xAD, 0xFE, 0xC2, 0x00, 0x6A, 0x01, 0x68, 0x20, 0x4E, 0x00, 0x00,
                                 0x53, 0x53, 0x68, 0xD8, 0x1A, 0xC6, 0x00 };
    const BYTE *reg = (const BYTE *)(DWORD_PTR)GPU_RATING_REG_VA;
    if (!Readable(reg, sizeof(kReg)) || memcmp(reg, kReg, sizeof(kReg)) != 0) {
        Log("gpu rating: bytes at 0x%08X do not match, not patched", GPU_RATING_REG_VA);
        return;
    }
    if (!CallsTarget(BENCH_CALL_VA, BENCH_FUNC_VA) || !CallsTarget(HWCHANGED_CALL_VA, HWCHANGED_FUNC_VA)) {
        Log("gpu rating: calls at 0x%08X / 0x%08X do not match, not patched",
            BENCH_CALL_VA, HWCHANGED_CALL_VA);
        return;
    }
    RedirectCall(BENCH_CALL_VA, (void *)BenchmarkHook);
    RedirectCall(HWCHANGED_CALL_VA, (void *)HardwareChangedHook);
    Log("gpu rating: benchmark clamped to %d, low-spec texture floors reset once", GPU_RATING_MAX);
}

/* Frame cap. Com_Init registers "1848" (com_maxfps) @ 0x597E21 with domain 0..100 (default and
 * flags are patched by PatchDvarRegistrations above), so nothing above 100 fps can be set. The
 * registration call @ 0x597E33 goes through MaxFpsRegisterHook, which raises the maximum to
 * FPS_CAP_MAX. Com_Frame (0x599812..0x59987A) is IW's limiter, with no other clamp (no menu or
 * background cap; a minimized window only adds Sleep(5) per loop @ 0x6314A9):
 *     maxfps = com_maxfps; if (timescale < 0.1) maxfps = min(maxfps, timescale * 320)
 *     minMsec = maxfps > 0 ? max(1000 / maxfps, 1) : 1        (0 = no cap; 1 ms is the engine floor)
 *     while (timeGetTime() - lastFrame < minMsec) Sleep(1)
 * 1000 / maxfps is integer division, so 144 gave 6 ms frames (166 fps), 60 gave 16 ms (62.5 fps)
 * and 360 gave 2 ms (500 fps); and Sleep(1) overshoots, so a cap ran a few percent under (a
 * whole-ms wait of 17 for 60 measured 58 fps). The division (0x599864..0x599879, 22 bytes) is
 * replaced with "push esi ; call FrameMsecHook ; add esp, 4 ; mov [ebp-8], eax". FrameMsecHook
 * keeps a schedule of ideal frame starts 1000 / maxfps ms apart and returns the whole-ms wait
 * from the previous frame's actual start (0x2574674) to the next ideal one, so the fraction
 * and the Sleep overshoot are made up on the following frames. A frame more than one period
 * late (GPU-bound, loading) restarts the schedule instead of being caught up.
 * The wait itself is "while (not due) Sleep(1)", and Sleep(1) often returns 1-2 ms late, which
 * matters most at high caps, where the whole wait is 1-3 ms.
 * Its "mov ebx, [Sleep]" @ 0x5998C4 loads FrameSleepHook instead: Sleep(1) while 2 ms or more
 * remain, otherwise yield until timeGetTime() reaches the due tick. */
#define FPS_CAP_MAX             1000
#define MAXFPS_REG_CALL_VA      0x00597E33u
#define DVAR_REGISTER_INT_VA    0x00624420u
#define FRAME_MSEC_VA           0x00599864u
#define FRAME_LAST_VA           0x02574674u /* timeGetTime() when the previous frame started */
#define FRAME_SLEEP_LOAD_VA     0x005998C4u /* mov ebx, [Sleep]: the limiter's wait */
#define IAT_TIMEGETTIME_VA      0x00B424C0u

static DWORD g_frameDue;        /* timeGetTime() at which the limiter lets the frame start */

typedef void *(__cdecl *DvarRegisterIntFn)(const char *name, int def, int min, int max, unsigned flags,
                                           const char *desc);

static void *__cdecl MaxFpsRegisterHook(const char *name, int def, int min, int max, unsigned flags,
                                        const char *desc)
{
    (void)max;
    return ((DvarRegisterIntFn)(DWORD_PTR)DVAR_REGISTER_INT_VA)(name, def, min, FPS_CAP_MAX, flags, desc);
}

static int __cdecl FrameMsecHook(int maxfps)
{
    static double ideal;  /* when the previous frame should have started, timeGetTime() ms */
    static int lastFps;
    DWORD last = *(volatile DWORD *)(DWORD_PTR)FRAME_LAST_VA;
    if (maxfps <= 0) {
        lastFps = 0;
        g_frameDue = last + 1;
        return 1;
    }
    double period = 1000.0 / maxfps;
    double late = (double)last - ideal;
    if (maxfps != lastFps || late > period || late < -period)
        ideal = (double)last;     /* first frame, new cap, or a slow frame: start the schedule here */
    lastFps = maxfps;
    ideal += period;
    double wait = ideal - (double)last;
    int msec = (int)(wait + 0.5);
    if (msec < 1) msec = 1;
    g_frameDue = last + (DWORD)msec;
    return msec;
}

typedef DWORD (WINAPI *TimeGetTimeFn)(void);

static void WINAPI FrameSleepHook(DWORD ms)
{
    TimeGetTimeFn now = *(TimeGetTimeFn *)(DWORD_PTR)IAT_TIMEGETTIME_VA;
    LONG left = (LONG)(g_frameDue - now());
    if (left >= 2 || left <= 0 || left > 1000) {
        Sleep(ms);
        return;
    }
    for (DWORD t0 = now(); now() == t0;)
        if (!SwitchToThread()) YieldProcessor();
}

static void PatchFrameCap(void)
{
    /* test esi,esi ; jle +0x12 ; mov eax,1000 ; cdq ; idiv esi ; mov [ebp-8],eax ; test eax,eax ;
     * jne +3 ; mov [ebp-8],ebx */
    static const BYTE kDiv[22] = { 0x85, 0xF6, 0x7E, 0x12, 0xB8, 0xE8, 0x03, 0x00, 0x00, 0x99, 0xF7,
                                   0xFE, 0x89, 0x45, 0xF8, 0x85, 0xC0, 0x75, 0x03, 0x89, 0x5D, 0xF8 };
    BYTE *div = (BYTE *)(DWORD_PTR)FRAME_MSEC_VA;
    if (!CallsTarget(MAXFPS_REG_CALL_VA, DVAR_REGISTER_INT_VA)) {
        Log("frame cap: call at 0x%08X does not match, 1848 (com_maxfps) stays 0..100", MAXFPS_REG_CALL_VA);
    } else {
        RedirectCall(MAXFPS_REG_CALL_VA, (void *)MaxFpsRegisterHook);
        Log("frame cap: 1848 (com_maxfps) domain 0..%d (0 = no cap)", FPS_CAP_MAX);
    }
    if (!Readable(div, sizeof(kDiv)) || memcmp(div, kDiv, sizeof(kDiv)) != 0) {
        Log("frame cap: bytes at 0x%08X do not match, frame time stays 1000 / com_maxfps whole ms", FRAME_MSEC_VA);
        return;
    }
    BYTE code[22];
    memset(code, 0x90, sizeof(code));
    LONG rel = (LONG)((DWORD_PTR)FrameMsecHook - (FRAME_MSEC_VA + 1 + 5));
    code[0] = 0x56;                                   /* push esi */
    code[1] = 0xE8; memcpy(code + 2, &rel, 4);        /* call FrameMsecHook */
    code[6] = 0x83; code[7] = 0xC4; code[8] = 0x04;   /* add esp, 4 */
    code[9] = 0x89; code[10] = 0x45; code[11] = 0xF8; /* mov [ebp-8], eax */
    WriteCode(div, code, sizeof(code));
    Log("frame cap: frames start on a 1000 / com_maxfps ms schedule, so the cap is exact on average");
    static const BYTE kSleepLoad[6] = { 0x8B, 0x1D, 0x84, 0x21, 0xB4, 0x00 }; /* mov ebx, [0xB42184] (Sleep) */
    BYTE *load = (BYTE *)(DWORD_PTR)FRAME_SLEEP_LOAD_VA;
    if (!Readable(load, sizeof(kSleepLoad)) || memcmp(load, kSleepLoad, sizeof(kSleepLoad)) != 0) {
        Log("frame cap: bytes at 0x%08X do not match, the wait stays Sleep(1)", FRAME_SLEEP_LOAD_VA);
        return;
    }
    BYTE mov[6] = { 0xBB, 0, 0, 0, 0, 0x90 };                                 /* mov ebx, FrameSleepHook ; nop */
    DWORD hook = (DWORD)(DWORD_PTR)FrameSleepHook;
    memcpy(mov + 1, &hook, 4);
    WriteCode(load, mov, sizeof(mov));
    Log("frame cap: the last ms of the wait yields instead of Sleep(1)");
}

/* Field of view 65..120. C1 keeps the plaintext name cg_fov (the .menu / GSC / Scaleform name):
 *   CG_RegisterDvars @ 0x44C50A: Dvar_RegisterFloat("cg_fov", 65, 65, 80, 0x41 ARCHIVE|SAVED, "")
 *     (the max is "fld dword [0xC323B0]" = 80.0f @ 0x44C4DC; dvar_t* at 0x16DA2BC)
 *   then "582" = cg_fovScale (1, 0.2..2, CHEAT) and "583" = cg_fovMin (1, 1..160, CHEAT).
 * A value above 80 (menu, console, config_mp.cfg) is clamped to 80. The registration call goes through
 * FovRegisterHook, which raises the maximum to FOV_MAX; 0.2 and 1 stay as they are.
 * The Scaleform Options > Graphics slider and the pause menu's FOV slider take their range from the
 * native GetFOVRange (vtable 0xC636E4, body 0x5B0D80), which returns the constants {min 65, max 80,
 * val 75}: its "fld qword [0xC84BE0]" (80.0, shared with other code) @ 0x5B0E27 loads FOV_MAX instead.
 * CG_CalcFov (0x471707..) has no other clamp: fov = cg_fov, lerped to the weapon's ADS fov by the ADS
 * fraction, times cg_fovScale, floored at cg_fovMin; the view model is drawn with that same fov, as
 * IW4's is (so it sits further away at a wide fov). The server leaves cg_fov alone (codol_rawfiles
 * _playerlogic.gsc no longer forces 65). */
#define FOV_MAX                 120
#define FOV_REG_CALL_VA         0x0044C50Au
#define DVAR_REGISTER_FLOAT_VA  0x006244D0u
#define FOV_RANGE_MAX_LOAD_VA   0x005B0E27u

static const double g_fovRangeMax = FOV_MAX;

typedef void *(__cdecl *DvarRegisterFloatFn)(const char *name, float def, float min, float max, unsigned flags,
                                             const char *desc);

static void *__cdecl FovRegisterHook(const char *name, float def, float min, float max, unsigned flags,
                                     const char *desc)
{
    if (name && strcmp(name, "cg_fov") == 0 && max < (float)FOV_MAX)
        max = (float)FOV_MAX;
    return ((DvarRegisterFloatFn)(DWORD_PTR)DVAR_REGISTER_FLOAT_VA)(name, def, min, max, flags, desc);
}

static void PatchFov(void)
{
    /* push "cg_fov" ; call Dvar_RegisterFloat */
    static const BYTE kPushName[5] = { 0x68, 0x94, 0x2B, 0xC4, 0x00 };
    const BYTE *push = (const BYTE *)(DWORD_PTR)(FOV_REG_CALL_VA - 5);
    if (!Readable(push, 5) || memcmp(push, kPushName, 5) != 0 || !CallsTarget(FOV_REG_CALL_VA, DVAR_REGISTER_FLOAT_VA)) {
        Log("fov: call at 0x%08X does not match, cg_fov stays 65..80", FOV_REG_CALL_VA);
    } else {
        RedirectCall(FOV_REG_CALL_VA, (void *)FovRegisterHook);
        Log("fov: cg_fov domain 65..%d", FOV_MAX);
    }
    /* fld qword [0xC84BE0] (80.0) */
    static const BYTE kFld[6] = { 0xDD, 0x05, 0xE0, 0x4B, 0xC8, 0x00 };
    BYTE *fld = (BYTE *)(DWORD_PTR)FOV_RANGE_MAX_LOAD_VA;
    if (!Readable(fld, sizeof(kFld)) || memcmp(fld, kFld, sizeof(kFld)) != 0) {
        Log("fov: bytes at 0x%08X do not match, the menu FOV sliders stay 65..80", FOV_RANGE_MAX_LOAD_VA);
        return;
    }
    BYTE code[6] = { 0xDD, 0x05, 0, 0, 0, 0 };
    DWORD addr = (DWORD)(DWORD_PTR)&g_fovRangeMax;
    memcpy(code + 2, &addr, 4);
    WriteCode(fld, code, sizeof(code));
    Log("fov: GetFOVRange max %d (Options and pause menu sliders)", FOV_MAX);
}

/* Chinese text input on any system locale. The game's text is GBK: it converts with code page
 * 936 everywhere it converts at all (0x580679, 0x5BD0C6, 0x6A5B5D, 0x70384A, ...). Text typed
 * into the ANSI main window does not reach it as GBK, though: Windows converts it with the
 * system ANSI code page, so on a 1252 (Western) system every Chinese character arrives as '?'
 * and with the "UTF-8 for worldwide language support" option it arrives as UTF-8 bytes that the
 * game then reads as GBK. Two places, both byte-checked:
 *   - IME results. The main WndProc (0x633590) offers WM_IME_* to Scaleform first (event 0x1E,
 *     0x633A26); what Scaleform leaves goes to DefWindowProcA @ 0x633B6F, which reads the
 *     result with ImmGetCompositionStringA and posts it back as WM_IME_CHAR / WM_CHAR in the ANSI
 *     code page. That call goes to ImeDefWindowProc instead: for the main window's
 *     WM_IME_COMPOSITION it takes GCS_RESULTSTR as UTF-16, converts it to GBK and feeds the bytes
 *     to the WndProc as WM_CHAR (0x6339AC queues each as an SE_CHAR event), exactly what a
 *     Chinese-locale system delivers, then lets DefWindowProcA do the rest without the result.
 *   - Paste. 0x62EED0 reads GetClipboardData(CF_TEXT) @ 0x62EEE5, which Windows synthesizes from
 *     the Unicode clipboard with a locale code page. ClipboardGbk returns CF_UNICODETEXT converted
 *     to GBK; the caller only locks, copies and unlocks it, so the block is freed on the next
 *     paste. Characters with no GBK form become '?', as they would on a Chinese system. */
#define GAME_WNDPROC_VA      0x00633590u
#define WNDPROC_DEFPROC_VA   0x00633B6Fu
#define DEFWINDOWPROCA_SLOT  0x00B42420u
#define CLIPBOARD_CALL_VA    0x0062EEE5u
#define GETCLIPBOARDDATA_SLOT 0x00B4242Cu
#define GAME_CODEPAGE        936

static LRESULT WINAPI ImeDefWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_IME_COMPOSITION && (lp & GCS_RESULTSTR) && hwnd == *(HWND *)(DWORD_PTR)MAIN_HWND_VA) {
        HIMC imc = ImmGetContext(hwnd);
        LONG bytes = imc ? ImmGetCompositionStringW(imc, GCS_RESULTSTR, NULL, 0) : 0;
        if (bytes > 0) {
            WCHAR wide[256];
            char gbk[512];
            if (bytes > (LONG)sizeof(wide)) bytes = sizeof(wide);
            bytes = ImmGetCompositionStringW(imc, GCS_RESULTSTR, wide, bytes);
            int n = bytes > 0 ? WideCharToMultiByte(GAME_CODEPAGE, 0, wide, bytes / 2, gbk, sizeof(gbk), NULL, NULL) : 0;
            for (int i = 0; i < n; i++)
                ((WNDPROC)(DWORD_PTR)GAME_WNDPROC_VA)(hwnd, WM_CHAR, (BYTE)gbk[i], 1);
        }
        if (imc) ImmReleaseContext(hwnd, imc);
        lp &= ~(GCS_RESULTSTR | GCS_RESULTCLAUSE | GCS_RESULTREADSTR | GCS_RESULTREADCLAUSE);
        if (!lp) return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static HGLOBAL g_clipGbk;

static HANDLE WINAPI ClipboardGbk(UINT format)
{
    if (format != CF_TEXT) return GetClipboardData(format);
    HANDLE src = GetClipboardData(CF_UNICODETEXT);
    const WCHAR *wide = src ? (const WCHAR *)GlobalLock(src) : NULL;
    if (!wide) return GetClipboardData(CF_TEXT);
    int n = WideCharToMultiByte(GAME_CODEPAGE, 0, wide, -1, NULL, 0, NULL, NULL);
    HGLOBAL dst = n > 0 ? GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, n) : NULL;
    char *gbk = dst ? (char *)GlobalLock(dst) : NULL;
    if (gbk) {
        WideCharToMultiByte(GAME_CODEPAGE, 0, wide, -1, gbk, n, NULL, NULL);
        GlobalUnlock(dst);
    }
    GlobalUnlock(src);
    if (!gbk) {
        if (dst) GlobalFree(dst);
        return GetClipboardData(CF_TEXT);
    }
    if (g_clipGbk) GlobalFree(g_clipGbk);
    g_clipGbk = dst;
    return dst;
}

/* Replaces "call dword ptr [slot]" (FF 15 slot) with "call hook ; nop". */
static BOOL RedirectIndirectCall(DWORD va, DWORD slot, void *hook)
{
    BYTE *call = (BYTE *)(DWORD_PTR)va;
    if (!Readable(call, 6) || call[0] != 0xFF || call[1] != 0x15 || memcmp(call + 2, &slot, 4) != 0)
        return FALSE;
    BYTE code[6] = { 0xE8, 0, 0, 0, 0, 0x90 };
    LONG rel = (LONG)((DWORD_PTR)hook - (va + 5));
    memcpy(code + 1, &rel, 4);
    WriteCode(call, code, sizeof(code));
    return TRUE;
}

static void PatchGbkInput(void)
{
    static const BYTE kWndProc[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x4C };
    static const BYTE kDefArgs[] = { 0x8B, 0x55, 0x14, 0x52, 0x53, 0x57, 0x56 }; /* push lp, wp, msg, hwnd */
    static const BYTE kClipArg[] = { 0x6A, 0x01 };                               /* push CF_TEXT */
    BYTE *proc = (BYTE *)(DWORD_PTR)GAME_WNDPROC_VA;
    BYTE *def = (BYTE *)(DWORD_PTR)WNDPROC_DEFPROC_VA;
    BYTE *clip = (BYTE *)(DWORD_PTR)CLIPBOARD_CALL_VA;
    if (Readable(proc, sizeof(kWndProc)) && memcmp(proc, kWndProc, sizeof(kWndProc)) == 0 &&
        Readable(def - 7, 7) && memcmp(def - 7, kDefArgs, 7) == 0 &&
        RedirectIndirectCall(WNDPROC_DEFPROC_VA, DEFWINDOWPROCA_SLOT, ImeDefWindowProc))
        Log("gbk input: IME results reach the game as GBK");
    else
        Log("gbk input: bytes at 0x%08X / 0x%08X do not match, IME text uses the ANSI code page",
            GAME_WNDPROC_VA, WNDPROC_DEFPROC_VA);
    if (Readable(clip - 2, 2) && memcmp(clip - 2, kClipArg, 2) == 0 &&
        RedirectIndirectCall(CLIPBOARD_CALL_VA, GETCLIPBOARDDATA_SLOT, ClipboardGbk))
        Log("gbk input: paste converts the Unicode clipboard to GBK");
    else
        Log("gbk input: bytes at 0x%08X do not match, paste uses the ANSI code page", CLIPBOARD_CALL_VA);
}

#include "local_multi.c"
#include "rooms/managed_quick.c"
#include "rooms/quick_start_gate.c"
#include "rooms/quick_collection_default.c"
#include "rooms/room_join_error.c"
#include "rooms/client_queue_wait.c"
#include "rooms/initial_room_owner.c"
#include "rooms/training_preview_publish.c"
#include "rooms/room_roster_roles.c"

/* Snapshot-parity logger, parity-log branch only: see parity.c. */
#include "parity.c"

/* <game>\usermaps\<map>\ zones and iwds: see usermaps.c. */
#include <shlwapi.h>
#include "usermaps.c"

/* The survival match category: its gametype zones and constant configstring table, see survival_category.c. */
#include "rooms/survival_category.c"

/* C1 engine helpers shared by the features below: see c1game.c. */
#include "c1game.c"
#include "infect_scoreboard.c"

/* Fullscreen Windowed (codol_borderless): see borderless.c. */
#include "borderless.c"

/* Developer console overlay (dev branch): see console.c. Builds without it (-DK33_NO_CONSOLE, or delete
 * these three lines and the InstallConsole() call) still compile and link. */
#if CODOL_DIAGNOSTICS && !defined(K33_NO_CONSOLE)
#include "console/k33shot.c" /* screenshots and draw statistics on the console's Present hooks; diagnostics only */
#include "console/console.c"
#endif

/* Sound voices: per-alias voice limit and nearest-wins at a channel cap (soundvoices.c), through the voice
 * diagnostics' hooks (sounddiag.c, which also logs refusals to Kernel33.log at k33_snddiag 1+). */
#include "audio/sounddiag.c"
#include "audio/soundvoices.c"

/* In-game voice chat (feat/voicechat): see voice.c. */
#include "audio/voice.c"

/* XInput gamepad (feat/controller): see controller.c and docs/CONTROLLER.md. */
#include "controller/controller.c"
#include "controller/padsnap.c"     /* the pad's menu cursor snaps to Scaleform controls (after controller.c) */

/* Discord Rich Presence (CoDOLPresence.exe moved into the client): see discord.c. */
#include "discord.c"

/* Shared plumbing for the features ported from codol-classic's d3d9.dll: a main-thread tick list (k33tick.c, chained
 * after voice.c's and discord.c's Com_EventLoop hooks), an HTTPS/SHA-256 helper (k33http.c) and a JSON reader (k33json.c). */
#include "k33tick.c"
#include "k33http.c"
#include "k33json.c"

/* codol_quick_native_room_context for the shared movie's Quick Play room checks: see quick_room.c. */
#if !CODOL_KERNEL_ROOM_CONTEXT
#include "rooms/quick_room.c"
#endif
/* The THEATRE screen (codol-classic's theatre playback): see theatre.c. */
#include "theatre.c"
/* UTF-8 player names and an optional display-name service: see name_display.c. */
#if !CODOL_KERNEL_NAME_DISPLAY
#include "name_display.c"
#endif
/* Local verified SDK -> immutable hosted-script source handoff. */
#include "p2p/p2p_verified_scripts.c"
#include "local_log_flush.c"
#include "gfx/localized_substitution.c"
#include "gfx/localized_numeric.c"
/* Player-hosted matches: the sh1x server as a child of the host's client (p2p.c, docs/P2P_HOSTING.md). */
#include "p2p/p2p_production_kernel.h"
#include "p2p/p2p.c"
/* The sh1x server inside the client, on the game thread (p2p_inproc.c, feat/p2p-inprocess). */
#include "p2p/p2p_inproc.c"
/* The in-process server's assets against C1's own (p2p_assets.c, asset layout stage 2/3). */
#include "p2p/p2p_assets.c"
/* UDP only with lobby-vouched hosts (hostgate.c): the P2P trust requirement. */
#include "p2p/hostgate.c"
#include "p2p/p2p_production_kernel.c"
/* Offline mode: the Demonware lobby inside the client (offline_lobby.c, offline/, docs/OFFLINE_LOBBY.md). */
#include "offline_lobby.c"
/* Reject malformed top-level DemonWare packet lengths in the exact C1 parser. */
#include "guards/bd_packet_guard.c"
/* Reject C1 snapshot entity and delta-field writes beyond their bounds. */
#include "guards/snapshot_guard.c"
/* Bound the exact C1 bulk record loop to its 19-record destination. */
#include "guards/bulk_count_guard.c"
/* Bind the state-3 challenge reply to C1's pending destination. */
#include "guards/challenge_sender_guard.c"
/* Bound exact C1 joinParty counts and party-builder stack records. */
#include "guards/join_party_guard.c"
/* Bound memberJoin's configured slot scan to the exact 19-entry party array. */
#include "guards/member_join_guard.c"
/* Reject impossible server-info slot totals before they reach party state. */
#include "guards/server_info_capacity_guard.c"
/* Bound local session-update slots before they reach party state. */
#include "guards/session_capacity_guard.c"
/* Bound the party refresh dvar loop to the exact 19-entry array. */
#include "guards/party_slot_loop_guard.c"
/* Bound both game and party session creation to the 19-slot table. */
#include "guards/party_session_capacity_guard.c"
/* Bound session refresh counts before they become array limits. */
#include "guards/party_session_refresh_guard.c"

/* Runs in place of TenPlat's start routine (the fault handler redirects there). */
#include "gfx/news_refresh.c"
#include "gfx/gfx_date_time.c"
#include "gfx/gfx_local_operands.c"
#include "launcher_update_bridge.c"
#if CODOL_KERNEL_NAME_DISPLAY
#include "kernel/kernel_name_display.c"
#endif
#if CODOL_KERNEL_ROOM_CONTEXT
#include "kernel/kernel_room_context.c"
#endif
static void __cdecl TakeOver(void)
{
    if (TryLauncherUpgrade()) ExitProcess(0);
    LoadUnpackedImage();
    MatchC1SectionFlags();
    FillImports();
    if (!InstallBoundedLocalizedSubstitution())
        Fatal("Required text safety checks could not be installed. Please repair or update the client.");
    if (!InstallBoundedLocalizedNumeric())
        Fatal("Required numeric text safety checks could not be installed. Please repair or update the client.");
    PatchGfxDateTime();
    PatchGfxLocalOperands();
#if CODOL_OFFLINE_LOBBY
    if (!InstallOfflineLobby())
        Fatal("The built-in lobby could not start, and this client does not connect to any other lobby. "
              "The reason is in Kernel33.log (next to the game's exe).");
#endif
    if (!InstallHostGate())
        Fatal("Required network safety checks could not be installed. Please repair or update the client.");
    if (!InstallBdPacketLengthGuard())
        Fatal("Required packet length checks could not be installed. Please repair or update the client.");
    if (!InstallSnapshotBoundsGuards())
        Fatal("Required snapshot bounds could not be installed. Please repair or update the client.");
#if CODOL_DIAGNOSTICS
    if (!InstallSnapshotInvalidFrameDiagnostic())
        Fatal("Local snapshot diagnostic could not be installed.");
#endif
    if (!InstallBulkCountGuard())
        Fatal("Required packet record bounds could not be installed. Please repair or update the client.");
    if (!InstallChallengeSenderGuard())
        Fatal("Required challenge sender checks could not be installed. Please repair or update the client.");
    if (!InstallConnectSenderGuard())
        Fatal("Required connection sender checks could not be installed. Please repair or update the client.");
    if (!InstallJoinPartyCountGuard())
        Fatal("Required party packet bounds could not be installed. Please repair or update the client.");
    if (!InstallJoinPartyBuilderGuard())
        Fatal("Required party record bounds could not be installed. Please repair or update the client.");
    if (!InstallJoinPartyFillGuard())
        Fatal("Required party fill bounds could not be installed. Please repair or update the client.");
    if (!InstallMemberJoinSlotGuard())
        Fatal("Required member packet bounds could not be installed. Please repair or update the client.");
    if (!InstallServerInfoCapacityGuard())
        Fatal("Required server slot bounds could not be installed. Please repair or update the client.");
    if (!InstallSessionCapacityGuard())
        Fatal("Required session slot bounds could not be installed. Please repair or update the client.");
    if (!InstallPartySlotLoopGuard())
        Fatal("Required party slot loop bounds could not be installed. Please repair or update the client.");
    if (!InstallPartySessionCapacityGuard())
        Fatal("Required party session bounds could not be installed. Please repair or update the client.");
    if (!InstallPartySessionRefreshGuard())
        Fatal("Required party refresh bounds could not be installed. Please repair or update the client.");
    PatchRenderGate();
    PatchActiveHandFiringMarker();
    PatchMinigunPairCoexistence();
    PatchDvarRegistrations();
    PatchPlainConfig();
    PatchGpuRating();
    PatchFrameCap();
    PatchFov();
    PatchBorderless();
    if (!QbgStartupReady(g_bdInstalled)) ExitProcess(0x51424701u);
    PatchAltTab();
    PatchMultiInstance();
    PatchRawMouse();
    PatchGbkInput();
    PatchScoresOpen();
    PatchInfectedScoreboard();
    PatchManagedQuickJoin();
    PatchQuickStartGate();
    PatchQuickCollectionDefault();
    PatchTrainingPreviewPublish();
    PatchRoomJoinError();
    PatchCapacityQueueWait();
    PatchInitialRoomOwner();
    PatchRoomRosterRoles();
    PatchNewsRefresh();
    PatchLoopFxCatchUp();
    PatchPatchDir();
    PatchLocalizePool();
    PatchUserMaps();
    PatchSurvivalCategory();
    InstallSoundDiag();
#if CODOL_DIAGNOSTICS
    InstallParityLogger();
#endif
#if CODOL_DIAGNOSTICS && !defined(K33_NO_CONSOLE)
    InstallConsole();
    InstallShot();
#endif
#if CODOL_KERNEL_NAME_DISPLAY
    if(!InstallKernelNameDisplay())Fatal("Kernel33 name-display migration checks failed. Restore the qualified client package.");
#endif
    InstallVoice();
    InstallController();
    InstallDiscord();
#if CODOL_KERNEL_ROOM_CONTEXT
    if(!InstallKernelRoomContext())Fatal("Kernel33 native room migration checks failed. Restore the qualified client package.");
#else
    InstallQuickRoom();
#endif
    InstallTheatre();
#if !CODOL_KERNEL_NAME_DISPLAY
    InstallNameDisplay();
#endif
#if CODOL_LOCAL_TESTING
    InstallP2PInprocDiagnostics(); /* QA commands only; no frame/IAT/VEH activation at startup. */
#endif
    InstallP2PHost();
#if CODOL_LOCAL_TESTING
    if (!K33OnMainTick(VoiceCmdFile))
        Fatal("Local command-file tick could not be installed.");
#endif
#if CODOL_DIAGNOSTICS
    K33OnMainTick(SnapshotGuardReport);
#endif
#if CODOL_LOCAL_TESTING
    InstallK33LogFlush();
#endif
    /* Full migration must not enter C1 after silently dropping its ported
     * Theatre callback or the dispatcher. Defaults retain the legacy path. */
#if CODOL_KERNEL_NAME_DISPLAY
    if (!K33HasMainTick(TheatreTick) || !K33InstallMainTicks())
        Fatal("Kernel33 main-thread migration checks failed. Restore the qualified client package.");
#else
    K33InstallMainTicks(); /* last: chains in front of the retained client tick hooks */
#endif
    Log("entering game at 0x%08X", GAME_ENTRY_VA);
    ((void(*)(void))(DWORD_PTR)GAME_ENTRY_VA)();
    ExitProcess(0); /* the CRT entry ends in exit(); not reached */
}

/* ---- TenProtect quarantine -------------------------------------------------------------- */

static BOOL InTen(DWORD_PTR a)
{
    return g_tenBase && a >= (DWORD_PTR)g_tenBase && a < (DWORD_PTR)g_tenBase + g_tenSize;
}

static DWORD NoExec(DWORD prot)
{
    switch (prot & 0xFF) {
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:      return (prot & ~0xFF) | PAGE_READONLY;
    case PAGE_EXECUTE_READWRITE: return (prot & ~0xFF) | PAGE_READWRITE;
    case PAGE_EXECUTE_WRITECOPY: return (prot & ~0xFF) | PAGE_WRITECOPY;
    }
    return prot;
}

static LPVOID WINAPI Hook_VirtualAlloc(LPVOID a, SIZE_T n, DWORD t, DWORD p)
{
    BOOL ten = g_supportedBuild && !g_tenBase && n >= 0x100000 && p == PAGE_EXECUTE_READWRITE;
    LPVOID r = ((LPVOID(WINAPI *)(LPVOID, SIZE_T, DWORD, DWORD))g_virtualAlloc)(a, n, t, ten ? NoExec(p) : p);
    if (ten && r) {
        g_tenSize = n;
        g_tenBase = (BYTE *)r;
        Log("tenprotect: image allocation %p size 0x%lX held non-executable", r, (unsigned long)n);
    }
    return r;
}

static BOOL WINAPI Hook_VirtualProtect(LPVOID a, SIZE_T n, DWORD p, PDWORD old)
{
    if (InTen((DWORD_PTR)a)) p = NoExec(p);
    return ((BOOL(WINAPI *)(LPVOID, SIZE_T, DWORD, PDWORD))g_virtualProtect)(a, n, p, old);
}

static LONG CALLBACK FaultHandler(EXCEPTION_POINTERS *ep)
{
#if CODOL_DIAGNOSTICS
    static volatile LONG logged;
#endif
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *c = ep->ContextRecord;
    if (er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;

    DWORD *sp = (DWORD *)c->Esp;
    if (er->ExceptionInformation[0] == 8 && InTen(c->Eip) && Readable(sp, 16)) {
        DWORD exe = (DWORD)(DWORD_PTR)GetModuleHandleA(NULL);
        DWORD ten = (DWORD)(DWORD_PTR)g_tenBase;
        DWORD off = c->Eip - ten;
        if (sp[1] == ten && sp[2] == DLL_PROCESS_ATTACH) {
            /* DllMain(hTen, DLL_PROCESS_ATTACH, 0): report success. */
            Log("tenprotect: DllMain (+0x%lX) skipped", (unsigned long)off);
            c->Eax = TRUE;
            c->Eip = sp[0];
            c->Esp += 16;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (sp[0] == exe && sp[1] == exe && sp[3] == ten) {
            /* Start routine (hExe, param block, hTen): do its job ourselves. */
            Log("tenprotect: start routine (+0x%lX) replaced; param block at 0x%08lX",
                (unsigned long)off, (unsigned long)sp[2]);
            c->Eip = (DWORD)(DWORD_PTR)TakeOver;
            c->Esp = (c->Esp + 16) & ~0xFu;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        Log("tenprotect: unexpected call +0x%lX from 0x%08lX (%08lX %08lX %08lX)", (unsigned long)off,
            (unsigned long)sp[0], (unsigned long)sp[1], (unsigned long)sp[2], (unsigned long)sp[3]);
        return EXCEPTION_CONTINUE_SEARCH;
    }

#if CODOL_DIAGNOSTICS
    if (InterlockedIncrement(&logged) <= 4) {
        DWORD ret = Readable(sp, 4) ? sp[0] : 0;
        Log("AV %s 0x%08lX at eip=0x%08lX [esp]=0x%08lX (thread %lu)",
            er->ExceptionInformation[0] == 8 ? "exec" : er->ExceptionInformation[0] ? "write" : "read",
            (unsigned long)er->ExceptionInformation[1], (unsigned long)c->Eip, (unsigned long)ret,
            GetCurrentThreadId());
        {
            MEMORY_BASIC_INFORMATION mbi;
            if (ret && VirtualQuery((LPCVOID)(DWORD_PTR)ret, &mbi, sizeof(mbi)))
                Log("  [esp] region base 0x%08lX alloc 0x%08lX type 0x%lX protect 0x%lX",
                    (unsigned long)(DWORD_PTR)mbi.BaseAddress, (unsigned long)(DWORD_PTR)mbi.AllocationBase,
                    (unsigned long)mbi.Type, (unsigned long)mbi.Protect);
        }
        /* Name the module behind eip and the top stack words, so a crash in a foreign DLL says which. */
        for (int k = -1; k < 8; k++) {
            DWORD a = k < 0 ? c->Eip : Readable(sp + k, 4) ? sp[k] : 0;
            HMODULE mod;
            char name[MAX_PATH];
            if (a && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                        (LPCSTR)(DWORD_PTR)a, &mod) &&
                GetModuleFileNameA(mod, name, sizeof(name)))
                Log("  %s 0x%08lX = %s+0x%lX", k < 0 ? "eip" : "stack", (unsigned long)a, name,
                    (unsigned long)(a - (DWORD)(DWORD_PTR)mod));
        }
    }
#endif
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ---- the Kernel32.dll stand-in ---------------------------------------------------------- */

HMODULE WINAPI K33_LoadLibraryA(LPCSTR name)
{
    HMODULE m = ((HMODULE(WINAPI *)(LPCSTR))g_loadLibraryA)(name);
    if (!m) Log("LoadLibraryA(\"%s\") FAILED (%lu)", name ? name : "(null)", GetLastError());
    return m;
}

FARPROC WINAPI K33_GetProcAddress(HMODULE module, LPCSTR name)
{
    FARPROC p = ((FARPROC(WINAPI *)(HMODULE, LPCSTR))g_getProcAddress)(module, name);
    if (p && g_supportedBuild && (DWORD_PTR)name >> 16) {
        if (lstrcmpA(name, "VirtualAlloc") == 0) { g_virtualAlloc = p; return (FARPROC)Hook_VirtualAlloc; }
        if (lstrcmpA(name, "VirtualProtect") == 0) { g_virtualProtect = p; return (FARPROC)Hook_VirtualProtect; }
    }
    return p;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = inst;
        DisableThreadLibraryCalls(inst);
        HMODULE k32 = GetModuleHandleA("kernel32.dll");
        g_loadLibraryA = GetProcAddress(k32, "LoadLibraryA");
        g_getProcAddress = GetProcAddress(k32, "GetProcAddress");
        if (!g_loadLibraryA || !g_getProcAddress) return FALSE;

        BYTE *base = (BYTE *)GetModuleHandleA(NULL);
        IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
        g_supportedBuild = base == (BYTE *)0x400000 && nt->FileHeader.TimeDateStamp == BUILD_TIMESTAMP;

#if CODOL_DIAGNOSTICS
        char exe[MAX_PATH];
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        Log("---- attach pid %lu %s (build %08lX%s) [kernel33 diag/import-crash 2]", GetCurrentProcessId(), exe,
            (unsigned long)nt->FileHeader.TimeDateStamp,
            g_supportedBuild ? "" : ", NOT 1.1.1.13: passing through untouched");
#endif
        if (g_supportedBuild) AddVectoredExceptionHandler(1, FaultHandler);
    }
    return TRUE;
}
