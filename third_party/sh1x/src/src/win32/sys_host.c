/* sh1x host API: the server inside another process (the CoDOL client, Kernel33 feat/p2p-inprocess).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The same objects as sh1x_dedrun.exe, linked as bin/sh1x_host.dll (makefile target `host-dll`). The host
 * (Kernel33 p2p_inproc.c, docs/P2P_HOSTING.md there) loads the DLL and drives it from its own frame loop:
 *
 *   Sh1xHost_Init(cmdline)   Sys_Main's start-up (Com_Init, the +set/+map line) without its loop
 *   Sh1xHost_Frame()         one Com_Frame; never sleeps (NET_Sleep polls) -> 1 running, 0 ended
 *   Sh1xHost_Command(text)   Cbuf_AddText, run by the next frame (e.g. "quit")
 *   Sh1xHost_ExitCode()      the code the server ended with
 *
 * Why this lives in the server: the engine's own exits (Com_Quit_f -> Sys_Quit -> Sys_Exit -> ExitProcess,
 * Sys_Error, the shutdown and load watchdogs' _exit) end the process, which here is the player's client.
 * Hosted (Sys_IsHosted), each of them ends the server instead: Sys_ExitForOS longjmps back to the host call
 * that is running (Init or Frame). The win32 console window (CON_Init), the fatal-error dialog wait, the
 * watchdog threads and the crash filter (only WinMain installs it) are not used; the host contains faults.
 * The FPU/SSE control words are the host's job too: the server sets its own (Sys_SetFloatEnv) and the host
 * restores the client's around every call.
 *
 * One server per DLL instance: after the server ends its globals are spent; the host loads a fresh copy of
 * the DLL to host again.
 *
 * Linked into the client (makefile target host-lib, Kernel33 feat/server-in-kernel33) there is one instance for the
 * client's whole run: the host starts it once and changes matches with console commands (map, gametype) instead
 * of ending it. Its large state is allocated by Sh1xHost_Init (sys_ondemand.h), so a client that never hosts
 * holds only the remaining .bss; the binary path is then the client's folder (the module that holds this code). */
#include <windows.h>
#include <setjmp.h>
#include <string.h>
#include "../q_shared.h"
#include "../qcommon.h"
#include "../cmd.h"
#include "../sys_main.h"
#include "../sys_thread.h"
#include "../online/assets/asset_host.h"
#include "../online/assets/c1_materialize.h"
#include "../server.h"
#include "../sys_ondemand.h"

void Sys_InitThreadContext(void); /* win32/sys_win32.c */

static int sys_hosted;
static int host_state;              /* 0 not started, 1 running, 2 ended */
static int host_exit_code;
static int host_in_call;
static jmp_buf host_exit;

int Sys_IsHosted(void) { return sys_hosted; }

void __noreturn Sys_HostExit(int exitCode)
{
    host_exit_code = exitCode;
    host_state = 2;
    if (host_in_call && Sys_IsMainThread()) longjmp(host_exit, 1);
    /* An exit from another server thread: park that thread; the host sees the server ended. */
    for (;;) Sleep(INFINITE);
}

__declspec(dllexport) int __cdecl Sh1xHost_Init(const char *cmdline)
{
    static char line[8192];
    static char path[MAX_OSPATH];
    HMODULE self = NULL;
    char *sep;
    if (host_state) return -1;
    if (!cmdline || strlen(cmdline) >= sizeof(line)) return -2;
    Q_strncpyz(line, cmdline, sizeof(line));
    sys_hosted = 1;
    /* The binary path is the DLL's folder, not the client exe's. */
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)(void *)Sh1xHost_Init, &self);
    if (!self || !GetModuleFileNameA(self, path, sizeof(path)) || !(sep = strrchr(path, '\\'))) return -3;
    Sys_SetExeFile(path);
    *sep = 0;
    Sys_SetBinaryPath(path);
    Sys_SetExeFileShort(sep + 1);
    Sys_InitThreadContext();
    host_state = 1;
    host_in_call = 1;
    if (setjmp(host_exit)) { host_in_call = 0; return host_exit_code ? host_exit_code : -4; }
    if (Sys_MainInit(line)) { host_in_call = 0; host_state = 2; return -5; }
    host_in_call = 0;
    return 0;
}

__declspec(dllexport) int __cdecl Sh1xHost_Frame(void)
{
    if (host_state != 1) return 0;
    host_in_call = 1;
    if (setjmp(host_exit)) { host_in_call = 0; return 0; }
    Com_Frame();
    host_in_call = 0;
    return host_state == 1;
}

__declspec(dllexport) void __cdecl Sh1xHost_Command(const char *text)
{
    if (host_state == 1 && text) Cbuf_AddText(text);
}

__declspec(dllexport) int __cdecl Sh1xHost_ExitCode(void) { return host_exit_code; }

/* Bytes of server state Sh1xHost_Init allocated on demand (sys_ondemand.h); 0 before it ran. */
__declspec(dllexport) unsigned __cdecl Sh1xHost_OnDemandBytes(void) { return Sys_OnDemandBytes(); }

/* Levels the server has spawned (each `map`): a host that queued a map change knows the new level is up once
 * this has moved, whatever the status of the replaced level says (a one-shot match keeps its finished level). */
extern unsigned sv_levelSerial; /* sv_main.c */
__declspec(dllexport) unsigned __cdecl Sh1xHost_LevelSerial(void) { return host_state == 1 ? sv_levelSerial : 0; }

/* End the match for its players before the host unloads the level: every human in the game gets the reliable
 * t "EXE_MATCHENDED", the same return to the lobby a finished match sends (script_exitlevel_trace.c), after
 * their profiles are flushed. Returns how many were told; the host keeps running frames so it reaches them. */
void FlushSecureProfiles(void);
__declspec(dllexport) int __cdecl Sh1xHost_ReturnPlayersToLobby(void)
{
    int i, sent = 0;
    if (host_state != 1 || !sv_maxclients) return 0;
    FlushSecureProfiles();
    for (i = 0; i < sv_maxclients->integer; ++i) {
        client_t *client = &svs.clients[i];
        if (client->state < CS_PRIMED || !client->gamestateSent) continue;
        if (client->netchan.remoteAddress.type == NA_BOT) continue;
        SV_SendServerCommandNoLoss(client, "t \"EXE_MATCHENDED\"");
        ++sent;
    }
    return sent;
}

/* The server's record for a C1 asset type in C1's memory layout (converted types only, see asset_host.c),
 * for the host to compare with or replace by C1's own. Call between frames, on the host's game thread. */
__declspec(dllexport) const void *__cdecl Sh1xHost_FindAsset(unsigned type, const char *name)
{
    return host_state == 1 ? HostFindAsset(type, name) : NULL;
}

__declspec(dllexport) int __cdecl Sh1xHost_EnumerateAssets(unsigned type, int (__cdecl *visit)(void *, const char *), void *context)
{
    return host_state == 1 ? HostEnumerateAssets(type, visit, context) : -1;
}

/* The server's C1-layout header copy of an asset (pointer words still serialized) and the pointers C1 stores in
 * it; the generic comparison in Kernel33's p2passetcheck walks both. Needs +set c1Fixups 1. */
__declspec(dllexport) const void *__cdecl Sh1xHost_FindC1Header(unsigned type, const char *name, unsigned *length)
{
    return host_state == 1 ? FindC1Header(type, name, length) : NULL;
}

__declspec(dllexport) const void *__cdecl Sh1xHost_ResolveC1Pointer(const void *addr, unsigned *kind, unsigned *size)
{
    return host_state == 1 ? ResolveC1Pointer(addr, kind, size) : NULL;
}

__declspec(dllexport) int __cdecl Sh1xHost_C1PointersIn(const void *addr, unsigned length, unsigned *offsets, unsigned max)
{
    return host_state == 1 ? C1PointersIn(addr, length, offsets, max) : -1;
}

/* The server's record of an asset as C1 holds it: real pointers, assets linked by name (stage 2 step 1). */
__declspec(dllexport) const void *__cdecl Sh1xHost_FindC1Record(unsigned type, const char *name)
{
    return host_state == 1 ? C1Record(type, name) : NULL;
}

/* Stage 3: the host's lookup into C1's asset database. Records of zones C1 loads once at startup are taken from it
 * instead of the server's own zones. Call before or after Sh1xHost_Init; NULL uninstalls. */
__declspec(dllexport) void __cdecl Sh1xHost_SetC1AssetProvider(const void *(__cdecl *find)(unsigned, const char *))
{
    SetC1AssetProvider(find);
}

__declspec(dllexport) unsigned __cdecl Sh1xHost_C1ProvidedCount(unsigned type)
{
    return C1ProvidedCount(type);
}

/* In-process host: C1's zone loading (see asset_host.h). The host's functions run on the server's fiber
 * and may yield to C1's thread while they wait. */
__declspec(dllexport) void __cdecl Sh1xHost_SetC1ZoneHost(int (__cdecl *await_zones)(const char *, const char *, unsigned),
    int (__cdecl *zone_held)(const char *))
{
    const c1_zone_host_t *old=C1ZoneHost();
    c1_zone_host_t host={await_zones,zone_held,old ? old->zone_of : NULL,old ? old->script_string : NULL};
    SetC1ZoneHost(await_zones || zone_held ? &host : NULL);
}
/* Stage 3: zones C1 holds are not mounted; the server asks C1 which zone defines an asset (zone_of). */
__declspec(dllexport) void __cdecl Sh1xHost_SetC1ZoneFind(const char *(__cdecl *zone_of)(unsigned, const char *))
{
    const c1_zone_host_t *old=C1ZoneHost();
    c1_zone_host_t host={old ? old->await_zones : NULL,old ? old->zone_held : NULL,zone_of,
        old ? old->script_string : NULL};
    SetC1ZoneHost(&host);
}
/* Stage 3: C1's script strings (SL_ConvertToString), for the handles in C1's own records. */
__declspec(dllexport) void __cdecl Sh1xHost_SetC1ScriptStrings(const char *(__cdecl *script_string)(unsigned))
{
    const c1_zone_host_t *old=C1ZoneHost();
    c1_zone_host_t host={old ? old->await_zones : NULL,old ? old->zone_held : NULL,old ? old->zone_of : NULL,
        script_string};
    SetC1ZoneHost(&host);
}
