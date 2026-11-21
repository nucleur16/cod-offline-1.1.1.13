/*
 * maplist_filter.c - the map lists show only maps whose zone is installed. Included by kernel33.c after k33tick.c.
 *
 * Every map list of the front end (Create Room's carousel, the room's map choice, co-op / Survival / Zombies) is
 * the rows of the string table mp/comaplist.csv (docs/MAPS_31926_PORTS.md), which the patch zones carry for every
 * map any build has. A row whose zone is not on disk can be picked and then fails to load. This removes those rows
 * from C1's copy of the table: column 4 is the level name, and a row stays when
 *   client_shipRetail\<language folder>\<level>.ff or usermaps\<level>\<level>.ff exists (usermaps.c), or
 *   for coop_<mission>_mp_<map> (co-op, Survival), the same for its MP base level mp_<map> (P2PParseRoomContext).
 * The kept rows move up in place (C1 StringTable, 16 bytes: name, columns, rows, {string, hash} cells; see
 * p2p_assets.c) and the row count shrinks; nothing is allocated. The table is looked up in C1's asset hash directly
 * (DB_FindXAssetHeader can block, p2p_assets.c), on the main thread, never while the database thread links a zone,
 * and again when the table changes (a language switch reloads the patch zones). K33_MAPLIST_ALL=1 turns it off.
 *
 * The tick alone came too late: codo.gfx's MatchService builds every map list once (listMap caches its rows per
 * match type in _mapsWithFilter, from the movie's init) and the Create Room carousel, the room's map choice, the
 * playlist room creator and the vote all read that cache. When the movie read the table before the tick's first
 * pass (the tick waits for the database thread, which is still loading ui_viewer_mp then), every map stayed
 * listed for the whole session. So the table is also filtered on its way out of C1's lookup: every reader (the
 * ActionScript StringTable* natives, C1's own code) gets it from DB_FindXAssetHeader 0x4CB8E0, which is hooked at
 * its entry; the first lookup of a new mp/comaplist.csv filters it before the caller sees a row.
 *
 * The same table names the supported Survival maps: a row coop_survival_<level> (usermaps_patch.ff carries one for
 * each level with authored placements, zonekit/shipping.py). Each new table's list is copied here, whether or not
 * the filter is on; K33SurvivalSupported answers the embedded host (p2p.c P2PCheck) and the lobby from that copy.
 */

#define ML_DB_BUSY_VA       0x02007A60u
#define ML_DB_HASHTABLE_VA  0x018FD9D8u
#define ML_DB_ENTRIES_VA    0x01C402C8u
#define ML_DB_NAMEFN_VA     0x00CD5660u
#define ML_ASSET_STRINGTABLE 37u
#define ML_MAP_COLUMN       4

static BYTE *g_mlTable;     /* the table filtered last, and the row count it was left with */
static int g_mlRows = -1;
static unsigned g_mlFrame;
static BOOL g_mlFilterOn;
static CRITICAL_SECTION g_mlLock;
static char g_mlSurvival[64][64]; /* the coop_survival_<level> rows' levels; count -1 until a table was seen */
static int g_mlSurvivalCount = -1;

/* 1 when the zones list `level` as a Survival map, 0 when not, -1 before mp/comaplist.csv is loaded. Any thread. */
static int K33SurvivalSupported(const char *level)
{
    int r;
    EnterCriticalSection(&g_mlLock);
    r = g_mlSurvivalCount < 0 ? -1 : 0;
    for (int i = 0; level && i < g_mlSurvivalCount && r != 1; i++)
        if (!_stricmp(g_mlSurvival[i], level)) r = 1;
    LeaveCriticalSection(&g_mlLock);
    return r;
}

static void MlReadSurvival(const BYTE *t)
{
    int cols = *(const int *)(t + 4), rows = *(const int *)(t + 8);
    const BYTE *cells = *(const BYTE *const *)(t + 12);
    if (cols <= ML_MAP_COLUMN || cols > 256 || rows <= 0 || rows > 4096 || !Readable(cells, (SIZE_T)cols * 8 * rows)) return;
    char found[64][64];
    int n = 0;
    for (int r = 0; r < rows && n < 64; r++) {
        const char *level = *(const char *const *)(cells + (SIZE_T)cols * 8 * r + 8 * ML_MAP_COLUMN);
        if (!level || !Readable(level, 1) || _strnicmp(level, "coop_survival_", 14) || strlen(level + 14) >= 64) continue;
        lstrcpynA(found[n++], level + 14, 64);
    }
    EnterCriticalSection(&g_mlLock);
    memcpy(g_mlSurvival, found, sizeof(found));
    g_mlSurvivalCount = n;
    LeaveCriticalSection(&g_mlLock);
    Log("maplist: %d Survival maps in mp/comaplist.csv", n);
}

static BOOL MlNameEq(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = tolower((unsigned char)*a), y = tolower((unsigned char)*b);
        if (x == '\\') x = '/';
        if (y == '\\') y = '/';
        if (x != y) return FALSE;
        if (!x) return TRUE;
    }
}

static BYTE *MlFindTable(const char *name)
{
    DWORD h = ML_ASSET_STRINGTABLE;
    for (const char *p = name; *p; p++) {
        int c = tolower((unsigned char)*p);
        if (c == '\\') c = '/';
        h = h * 31 + (DWORD)c;
    }
    unsigned idx = ((const unsigned short *)(DWORD_PTR)ML_DB_HASHTABLE_VA)[h % 60000u];
    typedef const char *(__cdecl *NameFn)(const void *);
    NameFn nameOf = ((const NameFn *)(DWORD_PTR)ML_DB_NAMEFN_VA)[ML_ASSET_STRINGTABLE];
    for (unsigned guard = 0; idx && guard < 65536; guard++) {
        const BYTE *e = (const BYTE *)(DWORD_PTR)(ML_DB_ENTRIES_VA + 24u * idx);
        if (*(const unsigned *)e == ML_ASSET_STRINGTABLE && nameOf) {
            const char *n = nameOf(e + 8);
            if (n && Readable(n, 1) && MlNameEq(n, name)) return e[0x10] ? *(BYTE *const *)(e + 8) : NULL;
        }
        idx = *(const unsigned short *)(e + 0x12);
    }
    return NULL;
}

static BOOL MlWritable(const void *p, SIZE_T n)
{
    MEMORY_BASIC_INFORMATION mbi;
    const BYTE *a = (const BYTE *)p, *end = a + n;
    while (a < end) {
        if (!VirtualQuery(a, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) return FALSE;
        if (!(mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) || (mbi.Protect & PAGE_GUARD)) return FALSE;
        a = (const BYTE *)mbi.BaseAddress + mbi.RegionSize;
    }
    return TRUE;
}

static BOOL MlZoneOnDisk(const char *root, const char *level)
{
    char file[96], path[MAX_PATH];
    if (!level[0] || strlen(level) > 64) return FALSE;
    _snprintf(file, sizeof(file), "%s.ff", level);
    file[sizeof(file) - 1] = 0;
    _snprintf(path, sizeof(path), "%sclient_shipRetail\\", root);
    path[sizeof(path) - 1] = 0;
    if (StockZoneExists(path, file)) return TRUE;
    _snprintf(path, sizeof(path), "%susermaps\\%s\\%s", root, level, file);
    path[sizeof(path) - 1] = 0;
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static BOOL MlMapInstalled(const char *root, const char *level)
{
    if (MlZoneOnDisk(root, level)) return TRUE;
    const char *base = !_strnicmp(level, "coop_", 5) ? strstr(level, "_mp_") : NULL;
    return base && MlZoneOnDisk(root, base + 1);
}

static void MlFilter(BYTE *t)
{
    int cols = *(const int *)(t + 4), rows = *(const int *)(t + 8);
    BYTE *cells = *(BYTE *const *)(t + 12);
    if (cols <= ML_MAP_COLUMN || cols > 256 || rows <= 0 || rows > 4096) return;
    SIZE_T rowBytes = (SIZE_T)cols * 8;
    if (!Readable(cells, rowBytes * rows)) return;
    if (!MlWritable(t, 16) || !MlWritable(cells, rowBytes * rows)) {
        Log("maplist: mp/comaplist.csv is not writable; every map stays listed");
        g_mlTable = t, g_mlRows = rows;
        return;
    }
    char root[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, root, MAX_PATH);
    while (n && root[n - 1] != '\\') n--;
    root[n] = 0;
    int kept = 0, dropped = 0;
    char list[512] = "";
    BOOL more = FALSE;
    for (int r = 0; r < rows; r++) {
        BYTE *row = cells + rowBytes * r;
        const char *level = *(const char *const *)(row + 8 * ML_MAP_COLUMN);
        BOOL keep = !level || !Readable(level, 1) || !level[0] || MlMapInstalled(root, level);
        if (keep) {
            if (kept != r) memmove(cells + rowBytes * kept, row, rowBytes);
            kept++;
        } else {
            dropped++;
            size_t len = strlen(list);
            if (len + strlen(level) + 2 < sizeof(list)) _snprintf(list + len, sizeof(list) - len, "%s%s", len ? " " : "", level);
            else more = TRUE;
        }
    }
    *(int *)(t + 8) = kept;
    g_mlTable = t, g_mlRows = kept;
    if (dropped) Log("maplist: %d of %d maps listed; not installed: %s%s", kept, rows, list, more ? " ..." : "");
    else Log("maplist: all %d maps are installed", rows);
}

/* A table not seen yet (the first, or a new one after a language switch): read its Survival rows, then filter
 * it. Any thread; g_mlTableLock keeps the lookup hook and the tick from taking the same table twice. */
static CRITICAL_SECTION g_mlTableLock;
static void MlTakeTable(BYTE *t, DWORD caller)
{
    EnterCriticalSection(&g_mlTableLock);
    if (!(t == g_mlTable && *(const int *)(t + 8) == g_mlRows)) {
        if (caller) Log("maplist: new mp/comaplist.csv %p (%d rows), first looked up by 0x%08lX on thread %lu", (void *)t,
                        *(const int *)(t + 8), (unsigned long)caller, GetCurrentThreadId());
        else Log("maplist: new mp/comaplist.csv %p (%d rows), found by the tick", (void *)t, *(const int *)(t + 8));
        MlReadSurvival(t); /* before the filter drops rows of maps that are not installed */
        if (g_mlFilterOn) MlFilter(t);
        g_mlTable = t, g_mlRows = *(const int *)(t + 8);
    }
    LeaveCriticalSection(&g_mlTableLock);
}

static void MapListTick(void)
{
    if (++g_mlFrame % 30) return; /* twice a second at 60 fps */
    if (*(volatile const DWORD *)(DWORD_PTR)ML_DB_BUSY_VA) return;
    BYTE *t = MlFindTable("mp/comaplist.csv");
    if (!t || !Readable(t, 16)) return;
    if (t == g_mlTable && *(const int *)(t + 8) == g_mlRows) return;
    MlTakeTable(t, 0);
}

/* DB_FindXAssetHeader 0x4CB8E0: XAssetHeader __cdecl (XAssetType type, const char *name). Its first six bytes
 * (push ebp; mov ebp, esp; and esp, -8) move to a trampoline, and the entry jumps to MlFindHook. */
#define ML_DB_FIND_VA 0x004CB8E0u
static const BYTE kMlFindPrologue[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 };
typedef void *(__cdecl *MlFindFn)(int type, const char *name);
static MlFindFn g_mlFindOriginal;

static void *__cdecl MlFindHook(int type, const char *name)
{
    void *h = g_mlFindOriginal(type, name);
    if (type == (int)ML_ASSET_STRINGTABLE && h && name && Readable(name, 1) && MlNameEq(name, "mp/comaplist.csv") &&
        Readable(h, 16) && !(h == g_mlTable && *(const int *)((BYTE *)h + 8) == g_mlRows))
        MlTakeTable((BYTE *)h, (DWORD)(DWORD_PTR)__builtin_return_address(0));
    return h;
}

static BOOL MlHookLookup(void)
{
    BYTE *entry = (BYTE *)(DWORD_PTR)ML_DB_FIND_VA;
    if (!Readable(entry, sizeof(kMlFindPrologue)) || memcmp(entry, kMlFindPrologue, sizeof(kMlFindPrologue))) return FALSE;
    BYTE *tramp = (BYTE *)VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return FALSE;
    memcpy(tramp, kMlFindPrologue, sizeof(kMlFindPrologue));
    tramp[6] = 0xE9; /* jmp back to the instruction after the moved prologue */
    LONG back = (LONG)(DWORD_PTR)(entry + sizeof(kMlFindPrologue)) - (LONG)(DWORD_PTR)(tramp + 11);
    memcpy(tramp + 7, &back, 4);
    FlushInstructionCache(GetCurrentProcess(), tramp, 16);
    g_mlFindOriginal = (MlFindFn)(void *)tramp;
    BYTE jump[6] = { 0xE9, 0, 0, 0, 0, 0x90 };
    LONG rel = (LONG)(DWORD_PTR)MlFindHook - (LONG)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    return TRUE;
}

/* ---- the room lobby's scene for coop levels without an arena entry (Survival) ---------------------------------
 * The room's 3D lobby (the "vlobby" behind the room UI: the map's environment scene and the players' characters)
 * comes from the room's map. 0x00610F4C copies the room info (0x611180; the map string at +8, which room.changemap
 * sets), puts that map in ui_mapname and asks the map info lookup 0x005ED910 for its "environment"; the scene is
 * the one of that name (0x611A30: urban, forest, desert, arctic; zm_ maps take "zombie" without the lookup) and the
 * characters are "<team>_<environment>". 0x005ED910 (eax = map, one stack argument = the key, the caller pops it;
 * eax = the value) reads the map list C1 builds from mp/basemaps.arena (0x005EA420: count 0x02837598, 0xB30-byte
 * entries from 0x0283759C, the map name at +0x30) and returns "" (0x00C2FEAD) for a map that has none. Every co-op
 * row of mp/comaplist.csv has an arena entry except Survival's coop_survival_<level>: those rooms got "", so no
 * scene (the cloud animation) and no characters. The arena cannot carry one per Survival level (C1 reads 128
 * entries, zonekit/shipping.py), so the lookup answers for a coop_<mode>_mp_<map> that has no arena entry of its
 * own with its MP base level mp_<map>'s value (the level the match runs on, as MlMapInstalled reads it). The other
 * callers (the load screen's and the menus' map info of ui_mapname) get the same answer for such a level. */
#define MI_LOOKUP_VA    0x005ED910u
#define MI_MAP_COUNT_VA 0x02837598u
#define MI_MAPS_VA      0x0283759Cu
#define MI_MAP_STRIDE   0xB30u
#define MI_MAP_NAME     0x30u
#define MI_EMPTY_VA     0x00C2FEADu
static const BYTE kMiPrologue[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x18 }; /* push ebp; mov ebp, esp; sub esp, 0x18 */
__attribute__((used)) void *g_miOriginal; /* the moved prologue, then a jump back */

const char *__cdecl MiCallOriginal(const char *map, const char *key);
void MiLookupHook(void);
__asm__(
    ".text\n"
    ".globl _MiCallOriginal\n"
    "_MiCallOriginal:\n"            /* cdecl (map, key) -> C1's convention */
    "    movl 4(%esp), %eax\n"
    "    pushl 8(%esp)\n"
    "    call *_g_miOriginal\n"
    "    addl $4, %esp\n"
    "    ret\n"
    ".globl _MiLookupHook\n"
    "_MiLookupHook:\n"              /* C1's convention -> cdecl MiLookup(map, key) */
    "    pushl 4(%esp)\n"
    "    pushl %eax\n"
    "    call _MiLookup\n"
    "    addl $8, %esp\n"
    "    ret\n");

static BOOL MiArenaHas(const char *map)
{
    int count = *(volatile const int *)(DWORD_PTR)MI_MAP_COUNT_VA;
    if (count <= 0 || count > 0x80) return FALSE;
    for (int i = 0; i < count; i++) {
        const char *name = (const char *)(DWORD_PTR)(MI_MAPS_VA + MI_MAP_STRIDE * (DWORD)i + MI_MAP_NAME);
        if (!_stricmp(name, map)) return TRUE;
    }
    return FALSE;
}

__attribute__((used)) const char *__cdecl MiLookup(const char *map, const char *key)
{
    static char logged[64];
    const char *v = MiCallOriginal(map, key); /* it builds the map list first */
    if (v && v != (const char *)(DWORD_PTR)MI_EMPTY_VA && *v) return v;
    if (!map || !Readable(map, 1) || _strnicmp(map, "coop_", 5)) return v;
    const char *base = strstr(map, "_mp_");
    if (!base || MiArenaHas(map)) return v;
    const char *b = MiCallOriginal(base + 1, key);
    if (_stricmp(logged, map)) {
        lstrcpynA(logged, map, sizeof(logged));
        Log("maplist: %s has no arena entry; its lobby scene and map info are %s's (%s \"%s\")", map, base + 1,
            key && Readable(key, 1) ? key : "?", b && Readable(b, 1) ? b : "");
    }
    return b;
}

static BOOL MiHookLookup(void)
{
    BYTE *entry = (BYTE *)(DWORD_PTR)MI_LOOKUP_VA;
    if (!Readable(entry, sizeof(kMiPrologue)) || memcmp(entry, kMiPrologue, sizeof(kMiPrologue))) return FALSE;
    BYTE *tramp = (BYTE *)VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return FALSE;
    memcpy(tramp, kMiPrologue, sizeof(kMiPrologue));
    tramp[6] = 0xE9;
    LONG back = (LONG)(DWORD_PTR)(entry + sizeof(kMiPrologue)) - (LONG)(DWORD_PTR)(tramp + 11);
    memcpy(tramp + 7, &back, 4);
    FlushInstructionCache(GetCurrentProcess(), tramp, 16);
    g_miOriginal = tramp;
    BYTE jump[6] = { 0xE9, 0, 0, 0, 0, 0x90 };
    LONG rel = (LONG)(DWORD_PTR)MiLookupHook - (LONG)(DWORD_PTR)(entry + 5);
    memcpy(jump + 1, &rel, 4);
    WriteCode(entry, jump, sizeof(jump));
    return TRUE;
}

static void InstallMapListFilter(void)
{
    char v[8];
    InitializeCriticalSection(&g_mlLock);
    InitializeCriticalSection(&g_mlTableLock);
    g_mlFilterOn = !(GetEnvironmentVariableA("K33_MAPLIST_ALL", v, sizeof(v)) && v[0] == '1');
    if (!g_mlFilterOn) Log("maplist: K33_MAPLIST_ALL=1, every map stays listed");
    else if (MlHookLookup()) Log("maplist: mp/comaplist.csv is filtered on its first lookup (DB_FindXAssetHeader 0x%08X hooked)", ML_DB_FIND_VA);
    else Log("maplist: DB_FindXAssetHeader 0x%08X differs; the table is filtered by the tick only", ML_DB_FIND_VA);
    if (MiHookLookup()) Log("maplist: coop levels without an arena entry take their MP level's (0x%08X hooked)", MI_LOOKUP_VA);
    else Log("maplist: the map info lookup 0x%08X differs; Survival rooms keep the lobby's default scene", MI_LOOKUP_VA);
    if (!K33OnMainTick(MapListTick)) Log("maplist: no main-thread tick left; every map stays listed, no Survival map is known");
}
