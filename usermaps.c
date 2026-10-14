/* User maps: <game>\usermaps\<map>\<zone>.ff and <game>\usermaps\<map>\<name>.iwd, loaded as if installed.
 *
 * Installing a map's zones in client_shipRetail\chinese fails the client's pure check, which counts the
 * zone folder against a built-in list ("Client pure check failed!" / "ff file count: 92, check count: 90",
 * main\console_mp.log, 2026-10-14), and main\ is checked the same way for iw_image_*.iwd. So nothing is
 * added to either folder: C1's file imports are wrapped instead, and only names that exist under
 * usermaps\ are affected.
 *   - zones: a <zone>.ff that C1 opens or tests under client_shipRetail\ and that is not there is taken
 *     from usermaps\<any map>\<zone>.ff (DB_TryLoadXFileInternal opens zones with CreateFileA at
 *     0x4CD44E; the zone dir is client_shipRetail\<language>). Stock zones are never shadowed.
 *   - iwds: C1 searches <game>\main_shared (FS_Startup's "%s_shared" format at 0x86D768) and lists it
 *     with FindFirstFileA; the listing of that folder gets each usermaps\*\*.iwd added (the folder need
 *     not exist), and opening <game>\main_shared\<name>.iwd opens the usermaps file. The pure check lists
 *     main\*iw_image_*.iwd only, so it still sees the stock count.
 * An iwd keeps the name it was written with (the name is part of its encryption, and C1 accepts
 * iw_image_00..23 only), so two maps must not use the same iw_image_NN, nor one main\ has.
 * "+set codol_usermaps 0" on the command line turns it off. */

#define USERMAPS_MAX_FILES 512
#define USERMAP_CREATEFILEA_SLOT     0x00B421D8u
#define USERMAP_GETFILEATTRIBUTES_SLOT 0x00B421C0u
#define USERMAP_FINDFIRSTFILEA_SLOT  0x00B42234u
#define USERMAP_FINDNEXTFILEA_SLOT   0x00B4225Cu
#define USERMAP_FINDCLOSE_SLOT       0x00B42238u
#define USERMAP_FINDFIRSTFILEEXA_SLOT 0x00B42294u

typedef struct { char name[MAX_PATH]; char path[MAX_PATH]; BOOL iwd; } UserMapFile;

static UserMapFile g_userMapFiles[USERMAPS_MAX_FILES];
static int g_userMapFileCount;
static char g_gameDir[MAX_PATH];   /* lower case, no trailing backslash */
static char g_sharedDir[MAX_PATH]; /* <game>\main_shared, lower case */

typedef struct UserMapFind {
    DWORD magic;
    struct UserMapFind *next;
    WIN32_FIND_DATAA *entries;
    int count, cursor;
} UserMapFind;
#define USERMAP_FIND_MAGIC 0x50414D55u /* "UMAP" */
static UserMapFind *g_userMapFinds;
static CRITICAL_SECTION g_userMapLock;

static BOOL UserMapsEnabled(void)
{
    const char *p = GetCommandLineA(), *q;
    BOOL on = TRUE;
    while ((q = strstr(p, "codol_usermaps")) != NULL) {
        q += 14;
        while (*q == ' ' || *q == '"') q++;
        on = *q != '0';
        p = q;
    }
    return on;
}

/* full, lower-case path; FALSE when it does not fit */
static BOOL UserMapNormalize(const char *in, char *out)
{
    char *file;
    DWORD n = GetFullPathNameA(in, MAX_PATH, out, &file);
    if (!n || n >= MAX_PATH) return FALSE;
    CharLowerA(out);
    return TRUE;
}

static const UserMapFile *UserMapFind_(const char *name, BOOL iwd)
{
    for (int i = 0; i < g_userMapFileCount; i++)
        if (g_userMapFiles[i].iwd == iwd && lstrcmpiA(g_userMapFiles[i].name, name) == 0) return &g_userMapFiles[i];
    return NULL;
}

static void UserMapIndexFolder(const char *dir, const char *pattern, BOOL iwd)
{
    char search[MAX_PATH];
    WIN32_FIND_DATAA fd;
    _snprintf(search, sizeof(search), "%s\\%s", dir, pattern);
    search[sizeof(search) - 1] = 0;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (g_userMapFileCount == USERMAPS_MAX_FILES) { Log("usermaps: more than %d files, the rest are ignored", USERMAPS_MAX_FILES); break; }
        const UserMapFile *dup = UserMapFind_(fd.cFileName, iwd);
        if (dup) { Log("usermaps: skipped %s\\%s: %s has the same name", dir, fd.cFileName, dup->path); continue; }
        if (iwd) {
            char stock[MAX_PATH];
            _snprintf(stock, sizeof(stock), "%s\\main\\%s", g_gameDir, fd.cFileName);
            stock[sizeof(stock) - 1] = 0;
            if (GetFileAttributesA(stock) != INVALID_FILE_ATTRIBUTES) {
                Log("usermaps: skipped %s\\%s: main\\ has an iwd of that name", dir, fd.cFileName);
                continue;
            }
        }
        UserMapFile *f = &g_userMapFiles[g_userMapFileCount++];
        lstrcpynA(f->name, fd.cFileName, sizeof(f->name));
        _snprintf(f->path, sizeof(f->path), "%s\\%s", dir, fd.cFileName);
        f->path[sizeof(f->path) - 1] = 0;
        f->iwd = iwd;
        Log("usermaps: %s (%lu bytes) as %s", f->path, (unsigned long)fd.nFileSizeLow, iwd ? "a main_shared iwd" : "a zone");
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static void UserMapIndex(void)
{
    char maps[MAX_PATH], search[MAX_PATH];
    WIN32_FIND_DATAA fd;
    _snprintf(maps, sizeof(maps), "%s\\usermaps", g_gameDir);
    maps[sizeof(maps) - 1] = 0;
    _snprintf(search, sizeof(search), "%s\\*", maps);
    search[sizeof(search) - 1] = 0;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) { Log("usermaps: no %s folder", maps); return; }
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
        char dir[MAX_PATH];
        _snprintf(dir, sizeof(dir), "%s\\%s", maps, fd.cFileName);
        dir[sizeof(dir) - 1] = 0;
        UserMapIndexFolder(dir, "*.ff", FALSE);
        UserMapIndexFolder(dir, "*.iwd", TRUE);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/* the usermaps file standing in for `path`, or NULL */
static const char *UserMapRedirect(const char *path)
{
    char full[MAX_PATH];
    if (!path || !g_userMapFileCount || !UserMapNormalize(path, full)) return NULL;
    char *slash = strrchr(full, '\\');
    if (!slash) return NULL;
    const char *name = slash + 1;
    size_t len = strlen(name);
    if (len > 4 && strcmp(name + len - 4, ".iwd") == 0) {
        *slash = 0;
        BOOL shared = strcmp(full, g_sharedDir) == 0;
        *slash = '\\';
        const UserMapFile *f = shared ? UserMapFind_(name, TRUE) : NULL;
        return f ? f->path : NULL;
    }
    if (len > 3 && strcmp(name + len - 3, ".ff") == 0 && strstr(full, "\\client_shipretail\\")) {
        const UserMapFile *f = UserMapFind_(name, FALSE);
        if (!f || GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) return NULL; /* never shadow a stock zone */
        return f->path;
    }
    return NULL;
}

static void UserMapLogOnce(const char *what, const char *from, const char *to)
{
    static char logged[64][MAX_PATH];
    static int count;
    for (int i = 0; i < count; i++) if (lstrcmpiA(logged[i], to) == 0) return;
    if (count < 64) lstrcpynA(logged[count++], to, MAX_PATH);
    Log("usermaps: %s %s -> %s", what, from, to);
}

static HANDLE WINAPI UserMapCreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa,
                                        DWORD disposition, DWORD flags, HANDLE templ)
{
    const char *to = disposition == OPEN_EXISTING ? UserMapRedirect(name) : NULL;
    if (to) { UserMapLogOnce("open", name, to); name = to; }
    return CreateFileA(name, access, share, sa, disposition, flags, templ);
}

static DWORD WINAPI UserMapGetFileAttributesA(LPCSTR name)
{
    const char *to = UserMapRedirect(name);
    if (to) { UserMapLogOnce("test", name, to); name = to; }
    return GetFileAttributesA(name);
}

static BOOL UserMapHasWildcard(const char *s) { return strchr(s, '*') || strchr(s, '?'); }

static HANDLE WINAPI UserMapFindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA out);

/* the CRT's _findfirst lists folders with FindFirstFileExA; a plain name match is the same listing */
static HANDLE WINAPI UserMapFindFirstFileExA(LPCSTR pattern, FINDEX_INFO_LEVELS level, LPVOID out,
                                             FINDEX_SEARCH_OPS op, LPVOID filter, DWORD flags)
{
    if (op == FindExSearchNameMatch && !filter && (level == FindExInfoStandard || level == FindExInfoBasic))
        return UserMapFindFirstFileA(pattern, (LPWIN32_FIND_DATAA)out);
    return FindFirstFileExA(pattern, level, out, op, filter, flags);
}

static HANDLE WINAPI UserMapFindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA out)
{
    char full[MAX_PATH];
    if (!g_userMapFileCount || !pattern || !UserMapNormalize(pattern, full)) return FindFirstFileA(pattern, out);
    char *slash = strrchr(full, '\\');
    if (!slash) return FindFirstFileA(pattern, out);
    if (!UserMapHasWildcard(slash + 1)) {
        const char *to = UserMapRedirect(pattern);
        if (to) { UserMapLogOnce("find", pattern, to); pattern = to; }
        return FindFirstFileA(pattern, out);
    }
    *slash = 0;
    BOOL shared = strcmp(full, g_sharedDir) == 0;
    const char *spec = slash + 1;
    if (!shared) return FindFirstFileA(pattern, out);

    /* main_shared: its own entries, then the usermaps iwds the pattern matches */
    UserMapFind *find = calloc(1, sizeof(*find));
    if (!find) return FindFirstFileA(pattern, out);
    find->entries = calloc(USERMAPS_MAX_FILES + 64, sizeof(WIN32_FIND_DATAA));
    if (!find->entries) { free(find); return FindFirstFileA(pattern, out); }
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { if (find->count < 64) find->entries[find->count++] = fd; } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    for (int i = 0; i < g_userMapFileCount; i++) {
        const UserMapFile *f = &g_userMapFiles[i];
        if (!f->iwd || !PathMatchSpecA(f->name, spec)) continue;
        BOOL dup = FALSE;
        for (int j = 0; j < find->count; j++) dup |= lstrcmpiA(find->entries[j].cFileName, f->name) == 0;
        HANDLE one = dup ? INVALID_HANDLE_VALUE : FindFirstFileA(f->path, &fd);
        if (one == INVALID_HANDLE_VALUE) continue;
        FindClose(one);
        lstrcpynA(fd.cFileName, f->name, sizeof(fd.cFileName));
        fd.cAlternateFileName[0] = 0;
        find->entries[find->count++] = fd;
        UserMapLogOnce("list", pattern, f->path);
    }
    if (!find->count) {
        free(find->entries); free(find);
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    find->magic = USERMAP_FIND_MAGIC;
    *out = find->entries[0];
    find->cursor = 1;
    EnterCriticalSection(&g_userMapLock);
    find->next = g_userMapFinds;
    g_userMapFinds = find;
    LeaveCriticalSection(&g_userMapLock);
    return (HANDLE)find;
}

/* the listing `h` is, when it is one of ours */
static UserMapFind *UserMapOwn(HANDLE h, BOOL unlink)
{
    UserMapFind **p, *found = NULL;
    EnterCriticalSection(&g_userMapLock);
    for (p = &g_userMapFinds; *p; p = &(*p)->next) {
        if ((HANDLE)*p == h) {
            found = *p;
            if (unlink) *p = found->next;
            break;
        }
    }
    LeaveCriticalSection(&g_userMapLock);
    return found;
}

static BOOL WINAPI UserMapFindNextFileA(HANDLE h, LPWIN32_FIND_DATAA out)
{
    UserMapFind *find = UserMapOwn(h, FALSE);
    if (!find) return FindNextFileA(h, out);
    if (find->cursor >= find->count) { SetLastError(ERROR_NO_MORE_FILES); return FALSE; }
    *out = find->entries[find->cursor++];
    return TRUE;
}

static BOOL WINAPI UserMapFindClose(HANDLE h)
{
    UserMapFind *find = UserMapOwn(h, TRUE);
    if (!find) return FindClose(h);
    free(find->entries);
    free(find);
    return TRUE;
}

static void PatchUserMaps(void)
{
    static const struct { DWORD slot; void *hook; } kSlots[] = {
        { USERMAP_CREATEFILEA_SLOT, (void *)UserMapCreateFileA },
        { USERMAP_GETFILEATTRIBUTES_SLOT, (void *)UserMapGetFileAttributesA },
        { USERMAP_FINDFIRSTFILEA_SLOT, (void *)UserMapFindFirstFileA },
        { USERMAP_FINDNEXTFILEA_SLOT, (void *)UserMapFindNextFileA },
        { USERMAP_FINDCLOSE_SLOT, (void *)UserMapFindClose },
        { USERMAP_FINDFIRSTFILEEXA_SLOT, (void *)UserMapFindFirstFileExA },
    };
    if (!UserMapsEnabled()) { Log("usermaps: off (codol_usermaps 0)"); return; }
    DWORD n = GetModuleFileNameA(NULL, g_gameDir, MAX_PATH);
    while (n && g_gameDir[n - 1] != '\\') n--;
    if (!n) return;
    g_gameDir[n - 1] = 0;
    CharLowerA(g_gameDir);
    _snprintf(g_sharedDir, sizeof(g_sharedDir), "%s\\main_shared", g_gameDir);
    g_sharedDir[sizeof(g_sharedDir) - 1] = 0;
    UserMapIndex();
    if (!g_userMapFileCount) return;
    InitializeCriticalSection(&g_userMapLock);
    for (unsigned i = 0; i < sizeof(kSlots) / sizeof(kSlots[0]); i++) {
        /* the slots FillImports filled with the kernel32 functions; anything else means the table moved */
        void *const *slot = (void *const *)(DWORD_PTR)kSlots[i].slot;
        if (!Readable(slot, sizeof(*slot))) { Log("usermaps: import slot 0x%08lX unreadable, not installed", (unsigned long)kSlots[i].slot); return; }
    }
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    if (*(FARPROC *)(DWORD_PTR)USERMAP_CREATEFILEA_SLOT != GetProcAddress(k32, "CreateFileA") ||
        *(FARPROC *)(DWORD_PTR)USERMAP_FINDFIRSTFILEA_SLOT != GetProcAddress(k32, "FindFirstFileA") ||
        *(FARPROC *)(DWORD_PTR)USERMAP_FINDFIRSTFILEEXA_SLOT != GetProcAddress(k32, "FindFirstFileExA")) {
        Log("usermaps: import slots do not hold CreateFileA/FindFirstFileA(Ex), not installed");
        return;
    }
    for (unsigned i = 0; i < sizeof(kSlots) / sizeof(kSlots[0]); i++)
        WriteCode((void *)(DWORD_PTR)kSlots[i].slot, &kSlots[i].hook, sizeof(void *));
    Log("usermaps: %d file(s) indexed, zone and main_shared lookups redirected", g_userMapFileCount);
}
