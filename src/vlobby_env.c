/*
 * vlobby_env.c - the lobby environment switch: which scene of the virtual lobby (ui_viewer_mp's mp_ui_viewer map)
 * the main lobby's character view shows. Included by kernel33.c after maplist_filter.c (its DB_FindXAssetHeader
 * hook and stringtable lookup) and hud_style.c (Dvar_RegisterString).
 *
 * The virtual lobby is one map holding every scene; a row of mp/cameraPosTable.csv names a scene's character stand
 * and camera (script_origin targetnames) and its vision and light. C1 picks the row from the viewer mode
 * (0x00610D80): the main lobby's character view is "character_loadout_center" ("character_loadout_zombie" in the
 * Zombies lobby), a room "team_<arena environment>", the weapon page "weapon_loadout_<weapon>". 1.1.1.13 still has
 * the earlier builds' character_loadout_urb / _woo / _des / _arc rows and the movie's terrain switch
 * (CharacterLoadOutController.onChangeTerrainHandle -> UIViewer_SetTerrain), but no control sends that event and the
 * character view always takes the _center row.
 *
 *   ui_vlobbyEnv    string, archived: the scene the main menu's and the Multiplayer lobby's character view shows,
 *                   a row character_loadout_<name> of the table ("" or "center": C1's own). Read on the main tick.
 *   vlobby_env      console command: no argument lists the menu scenes (the character_loadout_* rows but the
 *                   environment scenes urb, woo, des, arc, zombie, which are the rooms': a room shows its map's
 *                   environment, team_<environment>) and the current one; <name>, "next", "prev" or "default" sets
 *                   ui_vlobbyEnv. Bind it to a key to cycle, e.g. bind F6 "vlobby_env next".
 *
 * The scenes of the other builds (tools/vlobby_merge: 3.19.26.22, 2.6.9.7, 0.0.2.2) are merged into the stock world by
 * client_shipRetail\chinese\ui_viewer_vlobby_mp.ff, which C1 opens for ui_viewer_mp (VlobbyZoneOpen): every stock
 * asset of ui_viewer_mp, with the merged world and MapEnts. The same zone carries the camera table
 * with their rows as mp/camerapostable_vlobby.csv; the lookup of mp/cameraPosTable.csv returns that table while it is
 * loaded (VlobbyFindSubstitute, called from maplist_filter.c's hook), and the scene position dvars take the merged
 * scenes' x (VlWidenDomains). Without the zone everything is stock and the switch offers the stock rows only.
 *
 * C1 sites (1.1.1.13), byte-checked before use; on a mismatch one line is logged and the switch stays off:
 *   0x0060F2D0  the viewer's camera row: bool __thiscall (viewer, const char *row); finds the row in the table it
 *               caches at viewer + 0x142A90 (first call: DB_FindXAssetHeader(stringtable, mp/cameraPosTable.csv)),
 *               false when the table has no such row. Prologue 55 8B EC 83 E4 F8 81 EC A4 01 00 00; its first six
 *               bytes move to a trampoline.
 *   viewer + 8  the viewer mode (1 character loadout, 2 weapon, 3 team, 4 creation), + 0x10 the game (2 Zombies),
 *               + 0xE5 set: the viewer update 0x0060B043 runs the mode function 0x00610D80 again (and clears it).
 */

#define VL_CAMROW_VA       0x0060F2D0u
#define VL_DVAR            "ui_vlobbyEnv"
#define VL_TABLE           "mp/cameraPosTable.csv"
#define VL_TABLE_MERGED    "mp/camerapostable_vlobby.csv"
#define VL_ROW_PREFIX      "character_loadout_"
#define VL_ROW_DEFAULT     "character_loadout_center"
#define VL_VIEWER_TABLE    0x142A90u   /* the viewer's cached camera table (0x0060F2F2) */
#define VL_VIEWER_DIRTY    0xE5u       /* the mode is applied again on the next update (0x0060B043) */

static void *VlobbyFindSubstitute(int type, const char *name, void *h);

#define VL_ZONE            "ui_viewer_mp"
#define VL_ZONE_FILE       "client_shipRetail\\chinese\\ui_viewer_vlobby_mp.ff"

/* Called on the DB thread (kernel33.c PatchDirOpen) with the name of every zone C1 opens from disk: ui_viewer_mp
 * opens client_shipRetail\chinese\ui_viewer_vlobby_mp.ff when it is there, a complete ui_viewer_mp (every stock
 * asset) whose world and MapEnts hold the other builds' scenes too. A second zone with the same world assets loaded
 * next to ui_viewer_mp leaves duplicate images that C1 never created but still releases when the UI zones go
 * (0x00A921B8 calls a texture's Release through what is still its load definition), so the scenes replace the
 * zone rather than add to it. */
static BOOL g_vlZone;             /* the merged zone was opened: the scene dvars' domains are widened */

static HANDLE VlobbyZoneOpen(const char *name)
{
    if (!name || lstrcmpiA(name, VL_ZONE) != 0) return INVALID_HANDLE_VALUE;
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    while (n && path[n - 1] != '\\') n--;
    lstrcpynA(path + n, VL_ZONE_FILE, MAX_PATH - n);
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL);
    if (f != INVALID_HANDLE_VALUE) g_vlZone = TRUE;
    static BOOL logged;
    if (!logged) {
        logged = TRUE;
        if (f != INVALID_HANDLE_VALUE) Log("vlobby: zone \"%s\" loading from %s", name, path);
        else Log("vlobby: no %s (%lu): the stock lobby", path, GetLastError());
    }
    return f;
}

typedef char(__attribute__((thiscall)) *VlCamRowFn)(void *viewer, const char *row);
static VlCamRowFn g_vlCamRowOriginal;
static void *volatile g_vlViewer;
static char g_vlEnv[64];          /* the applied environment ("" = C1's own row) */
static const BYTE *g_vlDvar;
static BOOL g_vlReady, g_vlTried, g_vlLoggedTable;

/* the table C1 reads: the merged one when the zone is loaded, else the stock one */
static void *VlobbyFindSubstitute(int type, const char *name, void *h)
{
    if (type != (int)ML_ASSET_STRINGTABLE || !name || !Readable(name, 1) || !MlNameEq(name, VL_TABLE)) return h;
    BYTE *merged = MlFindTable(VL_TABLE_MERGED);
    if (!merged || !Readable(merged, 16)) return h;
    if (!g_vlLoggedTable) {
        g_vlLoggedTable = TRUE;
        Log("vlobby: %s stands for %s (%d rows against %d)", VL_TABLE_MERGED, VL_TABLE, *(const int *)(merged + 8),
            h && Readable(h, 16) ? *(const int *)((BYTE *)h + 8) : -1);
    }
    return merged;
}

static const BYTE *VlTable(void)
{
    BYTE *t = MlFindTable(VL_TABLE_MERGED);
    if (!t) t = MlFindTable(VL_TABLE);
    return t && Readable(t, 16) ? t : NULL;
}

/* the environment scenes: the rooms' (team_<environment>); their character_loadout_ rows stay out of the menu list */
static BOOL VlIsRoomEnvName(const char *name)
{
    static const char *const kEnvs[] = { "urb", "woo", "des", "arc", "zombie" };
    for (int i = 0; i < (int)(sizeof(kEnvs) / sizeof(kEnvs[0])); i++)
        if (!_stricmp(name, kEnvs[i])) return TRUE;
    return FALSE;
}

/* the menu scenes: every character_loadout_<name> row but the environment scenes, in table order; returns how many */
static int VlEnvironments(char (*out)[64], int max)
{
    const BYTE *t = VlTable();
    if (!t) return 0;
    int cols = *(const int *)(t + 4), rows = *(const int *)(t + 8);
    const BYTE *cells = *(const BYTE *const *)(t + 12);
    if (cols < 1 || rows < 1 || rows > 4096 || !Readable(cells, (SIZE_T)cols * 8 * rows)) return 0;
    int n = 0;
    for (int r = 0; r < rows && n < max; r++) {
        const char *row = *(const char *const *)(cells + (SIZE_T)cols * 8 * r);
        if (!row || !Readable(row, 1) || _strnicmp(row, VL_ROW_PREFIX, sizeof(VL_ROW_PREFIX) - 1)) continue;
        if (VlIsRoomEnvName(row + sizeof(VL_ROW_PREFIX) - 1)) continue;
        lstrcpynA(out[n++], row + sizeof(VL_ROW_PREFIX) - 1, 64);
    }
    return n;
}

static char __attribute__((thiscall)) VlCamRowHook(void *viewer, const char *row)
{
    g_vlViewer = viewer;
    if (g_vlEnv[0] && row && Readable(row, 1) && MlNameEq(row, VL_ROW_DEFAULT)) {
        char want[96];
        _snprintf(want, sizeof(want), VL_ROW_PREFIX "%s", g_vlEnv);
        want[sizeof(want) - 1] = 0;
        if (g_vlCamRowOriginal(viewer, want)) return 1;
    }
    return g_vlCamRowOriginal(viewer, row);
}

static BOOL VlHookCamRow(void)
{
    static const BYTE kProlog[12] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x81, 0xEC, 0xA4, 0x01, 0x00, 0x00 };
    BYTE *entry = (BYTE *)(DWORD_PTR)VL_CAMROW_VA;
    if (!Readable(entry, sizeof(kProlog)) || memcmp(entry, kProlog, sizeof(kProlog))) return FALSE;
    BYTE *tramp = (BYTE *)VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return FALSE;
    memcpy(tramp, kProlog, 6);
    tramp[6] = 0xE9;
    LONG back = (LONG)(DWORD_PTR)(entry + 6) - (LONG)(DWORD_PTR)(tramp + 11);
    memcpy(tramp + 7, &back, 4);
    FlushInstructionCache(GetCurrentProcess(), tramp, 16);
    g_vlCamRowOriginal = (VlCamRowFn)(void *)tramp;
    BYTE jump[6] = { 0xE9, 0, 0, 0, 0, 0x90 };
    LONG rel = (LONG)(DWORD_PTR)VlCamRowHook - (LONG)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    return TRUE;
}

/* The environment takes effect the next time C1 applies the viewer mode: viewer + 0xE5 set makes the viewer's update
 * (0x0060B043) run the mode function 0x00610D80 again on its own thread and frame, which asks for the camera row
 * (VlCamRowHook). Calling the camera row function from the tick instead moved the camera while the renderer was
 * drawing the old view (a crash in the static model pass), so nothing here calls into the viewer. */
static void VlApply(void)
{
    BYTE *v = (BYTE *)g_vlViewer;
    if (!v || !Readable(v + 8, 12) || !Readable(v + VL_VIEWER_DIRTY, 1)) return;
    if (*(const int *)(v + 8) != 1 || *(const int *)(v + 0x10) == 2) return;   /* character view, not Zombies */
    *(volatile BYTE *)(v + VL_VIEWER_DIRTY) = 1;
}

static void __cdecl VlCmd(void)
{
    char envs[64][64];
    int n = VlEnvironments(envs, 64);
    if (VoiceArgc() < 2) {
        char line[1024];
        int at = 0;
        for (int i = 0; i < n && at < (int)sizeof(line) - 70; i++) at += _snprintf(line + at, sizeof(line) - at, " %s", envs[i]);
        line[at] = 0;
        C1Printf("vlobby_env: %s (ui_vlobbyEnv); environments:%s\n", g_vlEnv[0] ? g_vlEnv : "center", n ? line : " (no camera table yet)");
        C1Printf("vlobby_env <name> | next | prev | default\n");
        return;
    }
    const char *a = VoiceArgv(1);
    char pick[64];
    lstrcpynA(pick, a, sizeof(pick));
    if (!_stricmp(a, "default")) lstrcpynA(pick, "center", sizeof(pick));
    else if ((!_stricmp(a, "next") || !_stricmp(a, "prev")) && n > 0) {
        int cur = 0;
        for (int i = 0; i < n; i++)
            if (!_stricmp(envs[i], g_vlEnv[0] ? g_vlEnv : "center")) cur = i;
        cur = (cur + (!_stricmp(a, "next") ? 1 : n - 1)) % n;
        lstrcpynA(pick, envs[cur], sizeof(pick));
    } else {
        BOOL known = FALSE;
        for (int i = 0; i < n; i++)
            if (!_stricmp(envs[i], a)) known = TRUE;
        if (!known) {
            C1Printf("vlobby_env: no environment %s (vlobby_env lists them)\n", a);
            return;
        }
    }
    C1QueueSet(VL_DVAR, !_stricmp(pick, "center") ? "" : pick);
    C1Printf("vlobby_env: %s\n", pick);
}

static C1Cmd g_vlCmd = { NULL, "vlobby_env", (void *)VlCmd };

/* C1 hands the row's stand and camera origins to the scene through float dvars, and two of them hold x within the
 * stock lobby's extent: "2110" (the stand's x, -1024 .. 7680) and "2104" (the camera's x, -1024 .. 10000). The merged
 * scenes lie at x -16384 and beyond (the light grid's rows run along x and hold one scene each), so set to such a
 * row's x those dvars refused the value ("not a valid value for dvar") and kept the previous scene's: the view stood
 * at the old x with the new y and z (black, or the stock sky). With the merged zone loaded their domains are widened
 * to the light grid's whole extent. The domain is found by its stock bounds next to the value, and left alone when
 * they are not there. */
static void VlWidenDomains(void)
{
    static const struct { const char *name; float lo, hi; } kDvars[] = { { "2110", -1024.0f, 7680.0f }, { "2104", -1024.0f, 10000.0f } };
    for (int i = 0; i < (int)(sizeof(kDvars) / sizeof(kDvars[0])); i++) {
        BYTE *d = (BYTE *)C1DvarFind(kDvars[i].name);
        if (!d || !Readable(d, 0x60) || d[8] != 1) { Log("vlobby: dvar %s missing or not a float; its domain stays", kDvars[i].name); continue; }
        int at = -1;
        for (int o = 0x10; o + 8 <= 0x60; o += 4)
            if (*(const float *)(d + o) == kDvars[i].lo && *(const float *)(d + o + 4) == kDvars[i].hi) { at = o; break; }
        if (at < 0) { Log("vlobby: dvar %s: no domain %g .. %g found; left as it is", kDvars[i].name, kDvars[i].lo, kDvars[i].hi); continue; }
        DWORD old;
        if (!VirtualProtect(d + at, 8, PAGE_READWRITE, &old)) continue;
        *(float *)(d + at) = -131072.0f;
        *(float *)(d + at + 4) = 131072.0f;
        Log("vlobby: dvar %s domain (+0x%X) %g .. %g widened to -131072 .. 131072", kDvars[i].name, at, kDvars[i].lo, kDvars[i].hi);
    }
}

static void VlobbyTick(void)
{
    if (!g_vlReady) {
        if (g_vlTried || !C1DvarsReady()) return;
        g_vlTried = TRUE;
        static const BYTE kRegString[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x10, 0x8B, 0x55, 0x14,
                                           0x8B, 0x4D, 0x0C, 0x52 };
        typedef const BYTE *(__cdecl *RegStr)(const char *, const char *, unsigned, const char *);
        if (!Readable((void *)(DWORD_PTR)HUD_STYLE_REGISTER_STRING_VA, sizeof(kRegString)) ||
            memcmp((void *)(DWORD_PTR)HUD_STYLE_REGISTER_STRING_VA, kRegString, sizeof(kRegString))) {
            Log("vlobby: Dvar_RegisterString 0x%08X differs; no environment switch", HUD_STYLE_REGISTER_STRING_VA);
            return;
        }
        g_vlDvar = ((RegStr)(DWORD_PTR)HUD_STYLE_REGISTER_STRING_VA)(VL_DVAR, "", 0x1,
            "Kernel33: the virtual lobby environment of the main lobby (vlobby_env lists them)");
        C1AddCommand(&g_vlCmd);
        g_vlReady = g_vlDvar && Readable(g_vlDvar, 0x10);
        Log("vlobby: %s %s, command vlobby_env", VL_DVAR, g_vlReady ? "registered" : "not registered");
        if (!g_vlReady) return;
    }
    static BOOL widened;
    if (g_vlZone && !widened && C1DvarFind("2110")) {
        widened = TRUE;
        VlWidenDomains();
    }
    /* a viewer that cached the stock table (looked up before the merged one was linked) reads the merged one */
    BYTE *vw = (BYTE *)g_vlViewer;
    if (vw && Readable(vw + VL_VIEWER_TABLE, 4) && !*(volatile const DWORD *)(DWORD_PTR)ML_DB_BUSY_VA) {
        BYTE *merged = MlFindTable(VL_TABLE_MERGED);
        BYTE **cached = (BYTE **)(vw + VL_VIEWER_TABLE);
        if (merged && *cached && *cached != merged) {
            *cached = merged;
            Log("vlobby: the viewer's camera table is now %s", VL_TABLE_MERGED);
        }
    }
    const char *v = *(const char *const *)(g_vlDvar + 0xC);
    char want[64] = "";
    if (v && Readable(v, 1)) lstrcpynA(want, v, sizeof(want));
    if (!_stricmp(want, "center") || !_stricmp(want, "default")) want[0] = 0;
    if (strcmp(want, g_vlEnv)) {
        lstrcpynA(g_vlEnv, want, sizeof(g_vlEnv));
        Log("vlobby: environment %s", g_vlEnv[0] ? g_vlEnv : "center (stock)");
        VlApply();
    }
}

#if K33_DIAGNOSTICS
/* Diagnostics: a fault in C1's material sort (0x00A87400, after a zone load) names the two materials it compared
 * (edi, esi there) and their technique sets, so a merged lobby zone's bad material can be found. */
static const char *VlName(DWORD p)
{
    const char *const *n = (const char *const *)(DWORD_PTR)p;
    if (!p || !Readable(n, 4) || !*n || !Readable(*n, 1)) return "?";
    return *n;
}

static LONG CALLBACK VlFaultProbe(EXCEPTION_POINTERS *e)
{
    DWORD eip = e->ContextRecord->Eip;
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || eip < 0x00A87400u || eip >= 0x00A87800u)
        return EXCEPTION_CONTINUE_SEARCH;
    DWORD m[2] = { e->ContextRecord->Edi, e->ContextRecord->Esi };
    for (int i = 0; i < 2; i++) {
        DWORD ts = Readable((void *)(DWORD_PTR)(m[i] + 0x50), 4) ? *(DWORD *)(DWORD_PTR)(m[i] + 0x50) : 0;
        DWORD rm = ts && Readable((void *)(DWORD_PTR)(ts + 8), 4) ? *(DWORD *)(DWORD_PTR)(ts + 8) : 0;
        Log("vlobby: material sort fault at 0x%08lX: material %d 0x%08lX \"%s\" techset 0x%08lX \"%s\" remapped 0x%08lX \"%s\"",
            (unsigned long)eip, i, (unsigned long)m[i], VlName(m[i]), (unsigned long)ts, VlName(ts), (unsigned long)rm, VlName(rm));
        DWORD set = rm ? rm : ts;
        for (int k = 0; set && k < 48; k++) {
            DWORD t = Readable((void *)(DWORD_PTR)(set + 0xC + 4 * k), 4) ? *(DWORD *)(DWORD_PTR)(set + 0xC + 4 * k) : 0;
            if (!t) continue;
            DWORD ps = Readable((void *)(DWORD_PTR)(t + 0x10), 4) ? *(DWORD *)(DWORD_PTR)(t + 0x10) : 0;
            Log("vlobby:   technique %d 0x%08lX \"%s\" pass0 decl 0x%08lX vs 0x%08lX ps 0x%08lX", k, (unsigned long)t, VlName(t),
                Readable((void *)(DWORD_PTR)(t + 8), 4) ? (unsigned long)*(DWORD *)(DWORD_PTR)(t + 8) : 0ul,
                Readable((void *)(DWORD_PTR)(t + 12), 4) ? (unsigned long)*(DWORD *)(DWORD_PTR)(t + 12) : 0ul, (unsigned long)ps);
        }
    }
    Log("vlobby: eax 0x%08lX ebx 0x%08lX ecx 0x%08lX edx 0x%08lX ebp 0x%08lX", (unsigned long)e->ContextRecord->Eax,
        (unsigned long)e->ContextRecord->Ebx, (unsigned long)e->ContextRecord->Ecx, (unsigned long)e->ContextRecord->Edx,
        (unsigned long)e->ContextRecord->Ebp);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

/* the merged world's static models are not in C1's material order: its load-time sort must not leave the cell
 * trees naming the wrong ones (installed for every map; smodel_sort.c) */
#include "smodel_sort.c"

static void InstallVlobbyEnv(void)
{
    InstallSmodelSort();
#if K33_DIAGNOSTICS
    AddVectoredExceptionHandler(1, VlFaultProbe);
#endif
    /* the merged camera table is served by maplist_filter.c's DB_FindXAssetHeader hook; with K33_MAPLIST_ALL=1 that
     * hook is not installed, so it is installed here (its map list part then only reads the Survival rows) */
    if (!g_mlFindOriginal && !MlHookLookup()) Log("vlobby: DB_FindXAssetHeader 0x%08X differs; the stock camera table stays", ML_DB_FIND_VA);
    if (!VlHookCamRow()) {
        Log("vlobby: the camera row function 0x%08X differs; no environment switch", VL_CAMROW_VA);
        return;
    }
    if (!K33OnMainTick(VlobbyTick)) Log("vlobby: no main-thread tick left; no environment switch");
    else Log("vlobby: camera rows hooked (0x%08X)", VL_CAMROW_VA);
}
