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

static void MapListTick(void)
{
    if (++g_mlFrame % 30) return; /* twice a second at 60 fps */
    if (*(volatile const DWORD *)(DWORD_PTR)ML_DB_BUSY_VA) return;
    BYTE *t = MlFindTable("mp/comaplist.csv");
    if (!t || !Readable(t, 16)) return;
    if (t == g_mlTable && *(const int *)(t + 8) == g_mlRows) return;
    MlReadSurvival(t); /* before the filter drops rows of maps that are not installed */
    if (g_mlFilterOn) MlFilter(t);
    else g_mlTable = t, g_mlRows = *(const int *)(t + 8);
}

static void InstallMapListFilter(void)
{
    char v[8];
    InitializeCriticalSection(&g_mlLock);
    g_mlFilterOn = !(GetEnvironmentVariableA("K33_MAPLIST_ALL", v, sizeof(v)) && v[0] == '1');
    if (!g_mlFilterOn) Log("maplist: K33_MAPLIST_ALL=1, every map stays listed");
    if (!K33OnMainTick(MapListTick)) Log("maplist: no main-thread tick left; every map stays listed, no Survival map is known");
}
