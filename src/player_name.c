/*
 * player_name.c - the console's `name`, readable AND writable (branch feat/console-name-writable). Included by
 * kernel33.c after offline_lobby.c. The pure parts (which names are accepted, the names file) are in
 * player_name_policy.h and tests/player_name_policy_test.c.
 *
 * How the name flows in C1 (MEASURED in C1.exe, image base 0x400000):
 *   - CL_Init registers "name" once: 0x498843 looks up "dw_username" (the profile name, registered at 0x6FE435),
 *     then 0x498887 call Dvar_RegisterString 0x624740("name", <dw_username's value>, 0x2000, ""); the dvar_t* goes
 *     to 0x16FC2FC. 0x2000 is read-only: Dvar_SetVariant 0x622D30 refuses a console or script value at 0x622EAE
 *     ("%s is read only."), so `name X` / `set name X` did nothing.
 *   - 0x725760 reads "name" (with a "[clan] " prefix when there is one) for: the connect packet's userinfo
 *     (0x495C66 -> Info_SetValueForKey "name" at 0x495D72; in Chinese it is re-encoded UTF-8 -> CP936 first), the
 *     party member record (0x4AD670, 63 bytes at +0x5A) and the party "ident" update (0x4A518C compares the record
 *     with the current name and sends the update when they differ), and the party UI rows (0x4B45EF).
 *   - The front end's own name (AS_GetPlayerName, 0x5AA777) reads dw_username, not "name". The session descriptor
 *     C1 sends the lobby keeps the profile name too (21/1 createSession fields 3 and 4 were "edtest" after
 *     `name NewName`, MEASURED in lobby.log; where C1 takes it from was not traced).
 *   - Every logon sets "name" to the account name again (Dvar_SetStringByName 0x6252F0 called from C1's protected
 *     code at 0x85EC590, right after "Account logged in with username"; MEASURED by the frame chain of that set).
 *   - The match server (server/sh1x) takes a player's name ONLY when the player connects: SV_DirectConnect cleans
 *     the userinfo "name", but first replaces it with the lobby's admission gamertag when the roster has one
 *     (sv_client.c, CODOL_NetworkAdmission -> Info_SetValueForKey(userinfo, "name", admission.gamertag)). The lobby's
 *     gamertag is the name in the client's session descriptor (src/offline/dw_session.cpp, LivePlayerName).
 *     SV_UserinfoChanged has the name code commented out and ClientUserinfoChanged (g_sv_client.cpp) reads only
 *     cg_predictItems, so a "userinfo" sent during a match cannot rename anyone; the scoreboard and overhead names
 *     are CS 1280+slot, built from svs.clients[slot].name (sv_main.c, CODOL_RefreshServerPlayerNames).
 *
 * What this file changes:
 *   1. 0x498887: the registration goes through PnRegister, which drops the 0x2000 flag, registers the profile's
 *      saved name (players\k33_names.txt) instead of dw_username when there is a valid one, and gives the dvar a
 *      domain function (dvar_t +0x44, called only by Dvar_SetVariant at 0x622E3A, which refuses the value and prints
 *      "'%s' is not a valid value for dvar 'name'" when it returns 0). The domain function is PnDomain: the rules of
 *      PnNameProblem, with the reason printed to the console.
 *   2. 0x5AA777: AS_GetPlayerName reads "name" instead of dw_username, so the menus read the new name from then on
 *      (not seen on screen in the live test: a tips popup covered the front end).
 *   3. The lobby (src/offline, in this process) asks PnLobbyDisplayName for the profile's chosen name wherever it
 *      would use the descriptor's name (dw_session.cpp LivePlayerName): the admission gamertag, so the name the
 *      match server gives this player when the player connects, the room's member names and chat sender.
 *   4. 0x85EC590 (C1's protected code): the logon's Dvar_SetStringByName("name", <account name>) goes through
 *      PnLoginSet, which sets the profile's chosen name instead when there is one (else the account name, as before).
 *   5. A main-thread tick (PnTick) notices a change from any path (`name X`, `set name X`, `seta`, `reset name`),
 *      saves it for the profile, clears the archive bit `seta` adds (the name is per profile, config_mp.cfg is not),
 *      and says what takes effect when.
 * dw_username is never changed: it is the career (C1 derives the player id from it; it names players\<profile>\).
 *
 * Every site is byte-checked first and nothing is written when one differs: then `name` stays read-only (or the
 * menus / lobby keep dw_username) and Kernel33.log / the console say so. All are patched in TakeOver, before C1 runs.
 */

#include "player_name_policy.h"

#define PN_REGISTER_SITE_VA   0x00498877u /* push desc ; push 0x2000 ; push eax ; push "name" ; call 0x624740 */
#define PN_REGISTER_CALL_VA   0x00498887u
#define PN_REGISTER_STRING_VA 0x00624740u /* Dvar_RegisterString(name, value, flags, desc) -> dvar_t*, cdecl */
#define PN_SET_DOMAIN_VA      0x00622E23u /* Dvar_SetVariant: cmp [edi+44h], 0 ; je ; ... call [edi+44h] */
#define PN_NAME_DVAR_PTR_VA   0x016FC2FCu
#define PN_USERNAME_PTR_VA    0x06C26318u
#define PN_MENU_READ_VA       0x005AA777u /* AS_GetPlayerName: mov eax, [dw_username] */
#define PN_LOGIN_SET_CALL_VA  0x085EC590u /* the logon: call Dvar_SetStringByName("name", <dw_username>) */
#define PN_SET_BY_NAME_VA     0x006252F0u /* Dvar_SetStringByName(name, value), cdecl */
#define PN_IN_GAME_VA         0x06C44D6Cu /* 1 in a match (name_display.c ND_IN_GAME_VA) */

#define PN_DVAR_FLAGS   0x04
#define PN_DVAR_TYPE    0x08
#define PN_DVAR_STRING  0x0C
#define PN_DVAR_RESET   0x2C
#define PN_DVAR_DOMAIN  0x44
#define PN_FLAG_ARCHIVE 0x0001u
#define PN_FLAG_ROM     0x2000u
#define PN_TYPE_STRING  7         /* 0x624740 passes 7 to Dvar_RegisterVariant 0x6242C0 */

static struct {
    BOOL registerOk, menusOk, loginOk;   /* the sites matched and were patched */
    BOOL registered;                     /* PnRegister ran */
    BOOL seen;                           /* PnTick has taken the first value */
    BOOL toldFailure;
    char profile[64];                    /* dw_username at registration: the profile the name is saved for */
    char last[64];                       /* the value PnTick saw last */
    char file[MAX_PATH];                 /* <players>\k33_names.txt */
    SRWLOCK lock;                        /* guards profile / shown for the lobby thread */
    char shown[64];                      /* the name the lobby is given for the profile; "" = the profile name */
} g_pn = { .lock = SRWLOCK_INIT };

/* The current name for the lobby (any thread). */
static void PnPublish(const char *name)
{
    AcquireSRWLockExclusive(&g_pn.lock);
    lstrcpynA(g_pn.shown, name && strcmp(name, g_pn.profile) ? name : "", sizeof(g_pn.shown));
    ReleaseSRWLockExclusive(&g_pn.lock);
}

/* DwOfflineConfig.displayName (lobby thread): the chosen name of the profile the lobby names, if this is it. */
static int PnLobbyDisplayName(const char *profileName, char *out, int outSize)
{
    int ok = 0;
    if (!profileName || !out || outSize < 2) return 0;
    AcquireSRWLockShared(&g_pn.lock);
    if (g_pn.shown[0] && g_pn.profile[0] && !strcmp(profileName, g_pn.profile)) {
        lstrcpynA(out, g_pn.shown, outSize);
        ok = 1;
    }
    ReleaseSRWLockShared(&g_pn.lock);
    return ok;
}

typedef BYTE *(__cdecl *PnRegisterFn)(const char *name, const char *value, unsigned flags, const char *desc);

static BOOL PnBytes(DWORD va, const BYTE *b, unsigned n)
{
    return Readable((void *)(DWORD_PTR)va, n) && !memcmp((void *)(DWORD_PTR)va, b, n);
}

/* ---- the names file ---------------------------------------------------------------------------------------- */

static void PnFilePath(void)
{
    char dir[MAX_PATH];
#if K33_OFFLINE_LOBBY
    if (g_ofl.players[0]) {
        _snprintf(g_pn.file, MAX_PATH, "%s\\k33_names.txt", g_ofl.players);
        g_pn.file[MAX_PATH - 1] = 0;
        return;
    }
#endif
    DWORD n = GetModuleFileNameA(NULL, dir, MAX_PATH);
    while (n && dir[n - 1] != '\\') n--;
    dir[n] = 0;
    _snprintf(g_pn.file, MAX_PATH, "%splayers\\k33_names.txt", dir);
    g_pn.file[MAX_PATH - 1] = 0;
}

/* The whole file (it is a few lines), NUL-terminated, into buf. -> FALSE when there is none or it is too long. */
static BOOL PnReadFile(char *buf, DWORD cap)
{
    HANDLE h = CreateFileA(g_pn.file, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD got = 0;
    buf[0] = 0;
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    BOOL ok = ReadFile(h, buf, cap - 1, &got, NULL) && got < cap - 1;
    CloseHandle(h);
    buf[ok ? got : 0] = 0;
    return ok;
}

/* `name` for the profile (NULL: forget it), written to a temporary file and moved over the old one. */
static BOOL PnSave(const char *name)
{
    char old[8192], text[8192], tmp[MAX_PATH + 8];
    if (!PnProfileStorable(g_pn.profile) || !g_pn.file[0]) return FALSE;
    if (!PnReadFile(old, sizeof(old)) && GetFileAttributesA(g_pn.file) != INVALID_FILE_ATTRIBUTES) {
        Log("name: %s could not be read; not saved", g_pn.file);
        return FALSE;
    }
    size_t n = PnNamesEdit(old, g_pn.profile, name, text, sizeof(text));
    if (n == (size_t)-1) { Log("name: %s is full; not saved", g_pn.file); return FALSE; }
    _snprintf(tmp, sizeof(tmp), "%s.tmp", g_pn.file);
    tmp[sizeof(tmp) - 1] = 0;
    HANDLE h = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { Log("name: %s could not be created (%lu)", tmp, GetLastError()); return FALSE; }
    DWORD wrote = 0;
    BOOL ok = WriteFile(h, text, (DWORD)n, &wrote, NULL) && wrote == (DWORD)n;
    ok = FlushFileBuffers(h) && ok;
    CloseHandle(h);
    if (!ok || !MoveFileExA(tmp, g_pn.file, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        Log("name: %s could not be written (%lu)", g_pn.file, GetLastError());
        DeleteFileA(tmp);
        return FALSE;
    }
    return TRUE;
}

/* ---- C1 ----------------------------------------------------------------------------------------------------- */

static const char *PnDvarString(const BYTE *d)
{
    if (!d || !Readable(d, PN_DVAR_DOMAIN + 4)) return NULL;
    const char *s = *(const char *const *)(d + PN_DVAR_STRING);
    return s && Readable(s, 1) ? s : NULL;
}

/* Dvar_SetVariant's domain check for "name" (0x622E3A: push the 16-byte value, push the dvar, call, add esp 14h;
 * al = 0 refuses). The reset value (the saved name or the profile name) is always accepted, so `reset name` works. */
static unsigned char __cdecl PnDomain(const BYTE *dvar, const char *value, DWORD v1, DWORD v2, DWORD v3)
{
    char copy[128];
    (void)v1; (void)v2; (void)v3;
    if (!value || !Readable(value, 1)) return 0;
    lstrcpynA(copy, value, sizeof(copy));
    const char *reset = dvar && Readable(dvar, PN_DVAR_DOMAIN + 4) ? *(const char *const *)(dvar + PN_DVAR_RESET) : NULL;
    if (reset && Readable(reset, 1) && !strcmp(copy, reset)) return 1;
    const char *why = strlen(value) >= sizeof(copy) ? "at most 31 bytes (a Chinese character takes 2 or 3)" : PnNameProblem(copy);
    if (!why) return 1;
    C1Printf("name: \"%s\" was not taken: %s\n", copy, why);
    Log("name: refused \"%s\": %s", copy, why);
    return 0;
}

/* The logon's Dvar_SetStringByName (0x85EC590, in C1's protected code, after "Account logged in with username"; it
 * sets "name" to the account name on every logon, MEASURED by the call chain of the change). A profile with a chosen
 * name keeps it; every other dvar and value passes through unchanged. */
static DWORD __cdecl PnLoginSet(const char *dvarName, const char *value)
{
    char shown[64];
    shown[0] = 0;
    if (dvarName && Readable(dvarName, sizeof("name")) && !strcmp(dvarName, "name")) {
        AcquireSRWLockShared(&g_pn.lock);
        lstrcpynA(shown, g_pn.shown, sizeof(shown));
        ReleaseSRWLockShared(&g_pn.lock);
        if (shown[0]) Log("name: the logon sets \"%s\"; the profile keeps its chosen \"%s\"", value && Readable(value, 1) ? value : "", shown);
    }
    return ((DWORD(__cdecl *)(const char *, const char *))(DWORD_PTR)PN_SET_BY_NAME_VA)(dvarName, shown[0] ? shown : value);
}

static BYTE *__cdecl PnRegister(const char *name, const char *value, unsigned flags, const char *desc)
{
    char saved[64];
    const char *use = value;
    g_pn.registered = TRUE;
    AcquireSRWLockExclusive(&g_pn.lock);
    lstrcpynA(g_pn.profile, value && Readable(value, 1) ? value : "", sizeof(g_pn.profile));
    ReleaseSRWLockExclusive(&g_pn.lock);
    PnFilePath();
    if (PnProfileStorable(g_pn.profile)) {
        char text[8192];
        if (PnReadFile(text, sizeof(text)) && PnNamesLookup(text, g_pn.profile, saved, sizeof(saved))) {
            const char *why = PnNameProblem(saved);
            if (!why) use = saved;
            else Log("name: the saved name \"%s\" of profile %s is not used: %s", saved, g_pn.profile, why);
        }
    }
    PnPublish(use);
    BYTE *d = ((PnRegisterFn)(DWORD_PTR)PN_REGISTER_STRING_VA)(name, use, flags & ~PN_FLAG_ROM, desc);
    if (d && Readable(d, PN_DVAR_DOMAIN + 4) && d[PN_DVAR_TYPE] == PN_TYPE_STRING) {
        DWORD f = *(volatile DWORD *)(d + PN_DVAR_FLAGS);
        *(volatile DWORD *)(d + PN_DVAR_FLAGS) = f & ~PN_FLAG_ROM; /* also when the dvar already existed */
        *(void *volatile *)(d + PN_DVAR_DOMAIN) = (void *)PnDomain;
        Log("name: registered writable (flags 0x%lX -> 0x%lX), \"%s\" for profile %s%s", (unsigned long)f,
            (unsigned long)(f & ~PN_FLAG_ROM), use ? use : "", g_pn.profile, use == saved ? " (saved name)" : "");
    } else {
        Log("name: Dvar_RegisterString returned %p (not a string dvar); the read-only flag was only dropped from the request", (void *)d);
    }
    return d;
}

/* Main thread, every frame: a new value from any path is saved and explained. */
static void PnTick(void)
{
    if (!g_pn.registerOk) {
        if (!g_pn.toldFailure && C1DvarsReady()) {
            g_pn.toldFailure = TRUE;
            C1Printf("name: C1's name registration differs from the measured bytes; name stays read-only (Kernel33.log)\n");
        }
        return;
    }
    BYTE *d = *(BYTE *volatile *)(DWORD_PTR)PN_NAME_DVAR_PTR_VA;
    const char *v = PnDvarString(d);
    if (!v) return;
    DWORD f = *(volatile DWORD *)(d + PN_DVAR_FLAGS);
    if (f & PN_FLAG_ARCHIVE) *(volatile DWORD *)(d + PN_DVAR_FLAGS) = f & ~PN_FLAG_ARCHIVE; /* seta: saved per profile */
    char now[64];
    lstrcpynA(now, v, sizeof(now));
    if (!g_pn.seen) {
        g_pn.seen = TRUE;
        lstrcpynA(g_pn.last, now, sizeof(g_pn.last));
        PnPublish(now);
        /* a value that did not pass PnDomain (+set name on the command line, before the dvar existed) */
        if (PnNameProblem(now) && strcmp(now, g_pn.profile)) {
            C1Printf("name: \"%s\" is not a usable name (%s); back to the saved one\n", now, PnNameProblem(now));
            C1CbufAddText("reset name\n");
        }
        return;
    }
    if (!strcmp(now, g_pn.last)) return;
    char was[64];
    lstrcpynA(was, g_pn.last, sizeof(was));
    lstrcpynA(g_pn.last, now, sizeof(g_pn.last));
    PnPublish(now);
    BOOL saved = PnSave(strcmp(now, g_pn.profile) ? now : NULL);
    BOOL inGame = *(volatile const BYTE *)(DWORD_PTR)PN_IN_GAME_VA != 0;
    Log("name: \"%s\" -> \"%s\" (profile %s, %s, %s)", was, now, g_pn.profile, saved ? "saved" : "NOT saved", inGame ? "in a match" : "front end");
    C1Printf("name: now \"%s^7\" (was \"%s^7\")%s\n", now, was,
             saved ? (strcmp(now, g_pn.profile) ? "; kept for the next launch of this profile" : "; the profile's own name again")
                   : "; NOT saved for the next launch (Kernel33.log)");
    C1Printf("name: %s; the match server uses it from the next match you start or join\n",
             g_pn.menusOk ? "the menus read it from now on" : "the menus keep the profile name");
    if (inGame) C1Printf("name: this match keeps \"%s^7\": the server reads a player's name only when the player connects\n", was);
}

/* ---- install ------------------------------------------------------------------------------------------------ */

static void InstallPlayerName(void)
{
    static const BYTE kRegister[] = { 0x68, 0xAD, 0xFE, 0xC2, 0x00, 0x68, 0x00, 0x20, 0x00, 0x00, 0x50,
                                      0x68, 0xA0, 0x45, 0xC4, 0x00, 0xE8, 0xB4, 0xBE, 0x18, 0x00 };
    static const BYTE kRegisterFn[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x10, 0x8B, 0x55, 0x14 };
    /* cmp dword [edi+44h], 0 ; je +6Dh ; sub esp, 10h ; ... ; call [edi+44h] ; add esp, 14h ; test al, al */
    static const BYTE kDomain[] = { 0x83, 0x7F, 0x44, 0x00, 0x74, 0x6D };
    static const BYTE kDomainCall[] = { 0xFF, 0x57, 0x44, 0x83, 0xC4, 0x14, 0x84, 0xC0 };
    static const BYTE kStore[] = { 0xA3, 0xFC, 0xC2, 0x6F, 0x01 }; /* 0x49889D mov [0x16FC2FC], eax */
    /* mov eax, [dw_username] ; cmp eax, esi ; je ; mov ecx, [eax+0Ch] */
    static const BYTE kMenu[] = { 0xA1, 0x18, 0x63, 0xC2, 0x06, 0x3B, 0xC6, 0x74, 0x1A, 0x8B, 0x48, 0x0C };
    static const BYTE kReadName[] = { 0xA1, 0xFC, 0xC2, 0x6F, 0x01 }; /* mov eax, [0x16FC2FC] */

    C1GameInit();
#if K33_OFFLINE_LOBBY
    g_oflDisplayName = PnLobbyDisplayName;
#endif
    BOOL reg = PnBytes(PN_REGISTER_SITE_VA, kRegister, sizeof(kRegister)) && PnBytes(PN_REGISTER_STRING_VA, kRegisterFn, sizeof(kRegisterFn)) &&
               PnBytes(PN_SET_DOMAIN_VA, kDomain, sizeof(kDomain)) && PnBytes(0x00622E3Au, kDomainCall, sizeof(kDomainCall)) &&
               PnBytes(0x0049889Du, kStore, sizeof(kStore)) && CallsTarget(PN_REGISTER_CALL_VA, PN_REGISTER_STRING_VA);
    if (!reg) {
        Log("name: C1's name registration (0x%08X), Dvar_RegisterString or Dvar_SetVariant's domain check differ; name stays read-only",
            PN_REGISTER_SITE_VA);
    } else {
        RedirectCall(PN_REGISTER_CALL_VA, (void *)PnRegister);
        g_pn.registerOk = TRUE;
    }
    /* The menu read only makes sense with a writable name; without it they would change nothing anyway. */
    if (reg && PnBytes(PN_MENU_READ_VA, kMenu, sizeof(kMenu))) {
        WriteCode((void *)(DWORD_PTR)PN_MENU_READ_VA, kReadName, sizeof(kReadName));
        g_pn.menusOk = TRUE;
    } else if (reg) Log("name: AS_GetPlayerName's dw_username read (0x%08X) differs; the menus keep the profile name", PN_MENU_READ_VA);
    /* call Dvar_SetStringByName ; add esp, 8 */
    static const BYTE kLoginSetTail[] = { 0x81, 0xC4, 0x08, 0x00, 0x00, 0x00 };
    static const BYTE kSetByName[] = { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0x75, 0x08, 0x56, 0xE8, 0xC3, 0xDF, 0xFF, 0xFF };
    if (reg && CallsTarget(PN_LOGIN_SET_CALL_VA, PN_SET_BY_NAME_VA) && PnBytes(PN_LOGIN_SET_CALL_VA + 5, kLoginSetTail, sizeof(kLoginSetTail)) &&
        PnBytes(PN_SET_BY_NAME_VA, kSetByName, sizeof(kSetByName))) {
        RedirectCall(PN_LOGIN_SET_CALL_VA, (void *)PnLoginSet);
        g_pn.loginOk = TRUE;
    } else if (reg) Log("name: the logon's name set (0x%08X) differs; a chosen name is replaced by the profile name at every logon",
                        PN_LOGIN_SET_CALL_VA);
    if (!K33OnMainTick(PnTick)) Log("name: no main-thread tick left; changes are not saved or explained");
    Log("name: %s, menus %s, logon %s", g_pn.registerOk ? "writable" : "READ-ONLY", g_pn.menusOk ? "follow it" : "keep dw_username",
        g_pn.loginOk ? "keeps a chosen name" : "resets it");
}
