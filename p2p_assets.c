/*
 * p2p_assets.c - the in-process server's assets against C1's own (feat/p2p-inprocess, asset layout stage 2/3).
 *
 * The server (sh1x_host.dll, server branch feat/iw4-asset-layout) converts its assets, one type at a time, to the
 * layout C1 holds in memory, and exports Sh1xHost_FindAsset / Sh1xHost_EnumerateAssets for the converted types.
 * p2passetcheck <type> [name|*] compares the server's record with C1's (DB_FindXAssetHeader 0x4CB8E0, only for
 * names C1 has a real, non-default entry for: DB_IsXAssetDefault 0x4CBDA0 first, so no default asset is created).
 * Pointer words are compared by what they point at; every other byte must be equal. Results go to the console
 * and Kernel33.log. Test clients only (+set k33_cmdFile 1). Types: 11 sound, 36 rawfile, 37 string table.
 */

/* (DB_FindXAssetHeader blocked on some rawfiles; the hash is read directly, see PiC1Find) */
#define C1_DB_FIND_HEADER_VA  0x004CB8E0u   /* XAssetHeader __cdecl (int type, const char *name) */
#define C1_DB_IS_DEFAULT_VA   0x004CBDA0u   /* int __cdecl (int type, const char *name): 1 absent or default */
/* Both lookups wait (Sleep(1) loop at 0x4CBDD0) while this word is nonzero, i.e. while the database thread
 * links a zone. Called from the main thread during such a load that wait can last the whole load, so the check
 * stops instead and reports how far it got. */
#define C1_DB_BUSY_VA         0x02007A60u
/* C1's asset hash, read directly (no lock and no wait: DB_FindXAssetHeader blocked the main thread on
 * scaleform/default/codohud.gfx). DB_IsXAssetDefault 0x4CBDA0 walks it this way: bucket = hash(type, name) % 60000
 * (0x4CB350: h = type, h = h*31 + tolower(c), '\' as '/'), db_hashTable 0x18FD9D8 (uint16 per bucket), entries
 * 0x1C402C8 (24 bytes: +0 type, +8 header, +0x10 zone index (0 = default asset), +0x12 next), names through
 * DB_GetXAssetHeaderName 0xCD5660[type](&header). */
#define C1_DB_HASHTABLE_VA    0x018FD9D8u
#define C1_DB_ENTRIES_VA      0x01C402C8u
#define C1_DB_NAMEFN_VA       0x00CD5660u

typedef const void *(__cdecl *Sh1xFindAssetFn)(unsigned, const char *);
typedef int (__cdecl *Sh1xEnumAssetsFn)(unsigned, int (__cdecl *)(void *, const char *), void *);

typedef struct {
    unsigned checked, equal, differ, c1Missing, serverMissing, reported, busy;
    char firstDiff[256];
} PiCheck;

static BOOL PiStrEq(const char *a, const char *b)
{
    if (!a || !b) return a == b;
    if (!Readable(a, 1) || !Readable(b, 1)) return FALSE;
    return strcmp(a, b) == 0;
}

static void PiDiff(PiCheck *k, const char *name, const char *what)
{
    if (!k->firstDiff[0]) _snprintf(k->firstDiff, sizeof(k->firstDiff) - 1, "%s: %s", name, what);
    if (k->reported < 20) { k->reported++; Log("p2passetcheck: %s differs: %s", name, what); }
}

/* Every byte of [a, a+size) except the pointer words listed in ptrs (offsets). */
static BOOL PiBytesEq(const BYTE *a, const BYTE *b, unsigned size, const unsigned *ptrs, unsigned nptrs, unsigned *at)
{
    for (unsigned i = 0; i < size; i++) {
        BOOL skip = FALSE;
        for (unsigned j = 0; j < nptrs && !skip; j++) skip = i >= ptrs[j] && i < ptrs[j] + 4;
        if (!skip && a[i] != b[i]) { *at = i; return FALSE; }
    }
    return TRUE;
}

/* C1 snd_alias_list_t (12) / snd_alias_t (108) / SoundFile (12) / LoadedSound (48) / SndCurve (136) / SpeakerMap (408):
 * the layouts of the server's xassets/sounds.h, from C1's loaders 0x4b8750, 0x4b84b0, 0x4b8240, 0x4b80b0,
 * 0x4b82b0. */
static BOOL PiSoundEq(PiCheck *k, const char *name, const BYTE *s, const BYTE *c)
{
    char what[160];
    unsigned at;
    if (!Readable(s, 12) || !Readable(c, 12)) { PiDiff(k, name, "unreadable list"); return FALSE; }
    if (!PiStrEq(*(const char *const *)s, *(const char *const *)c)) { PiDiff(k, name, "list name"); return FALSE; }
    int count = *(const int *)(s + 8);
    if (count != *(const int *)(c + 8)) {
        _snprintf(what, sizeof(what), "alias count %d vs C1 %d", count, *(const int *)(c + 8));
        PiDiff(k, name, what);
        return FALSE;
    }
    const BYTE *sa = *(const BYTE *const *)(s + 4), *ca = *(const BYTE *const *)(c + 4);
    if (count <= 0 || count > 4096 || !Readable(sa, 108 * count) || !Readable(ca, 108 * count)) {
        PiDiff(k, name, "unreadable aliases");
        return FALSE;
    }
    /* +0x18 is runtime state, not loaded data: C1's picker (0x61ea30) stores the last pick's sequence there
     * (0x61ec9a), so a played alias differs from its loaded value. It is skipped like a pointer word. */
    static const unsigned aliasPtrs[] = { 0, 4, 8, 12, 16, 20, 24, 88, 104 };
    for (int i = 0; i < count; i++) {
        const BYTE *x = sa + 108 * i, *y = ca + 108 * i;
        if (!PiBytesEq(x, y, 108, aliasPtrs, 9, &at)) {
            _snprintf(what, sizeof(what), "alias %d byte +0x%02X: %02X vs C1 %02X", i, at, x[at], y[at]);
            PiDiff(k, name, what);
            return FALSE;
        }
        for (int n = 0; n < 5; n++)
            if (!PiStrEq(((const char *const *)x)[n], ((const char *const *)y)[n])) {
                _snprintf(what, sizeof(what), "alias %d name slot %d", i, n);
                PiDiff(k, name, what);
                return FALSE;
            }
        const BYTE *sf = *(const BYTE *const *)(x + 20), *cf = *(const BYTE *const *)(y + 20);
        if (!sf != !cf) { _snprintf(what, sizeof(what), "alias %d sound file presence", i); PiDiff(k, name, what); return FALSE; }
        if (sf) {
            if (!Readable(sf, 12) || !Readable(cf, 12) || sf[0] != cf[0] || sf[1] != cf[1]) {
                _snprintf(what, sizeof(what), "alias %d sound file type/exists %u/%u vs C1 %u/%u", i, sf[0], sf[1], cf[0], cf[1]);
                PiDiff(k, name, what);
                return FALSE;
            }
            if (sf[0] == 1) {
                const BYTE *sl = *(const BYTE *const *)(sf + 4), *cl = *(const BYTE *const *)(cf + 4);
                static const unsigned loadedPtrs[] = { 0, 8, 40, 44 };
                at = 0;
                if (!Readable(sl, 48) || !Readable(cl, 48) || !PiBytesEq(sl, cl, 48, loadedPtrs, 4, &at) ||
                    !PiStrEq(*(const char *const *)sl, *(const char *const *)cl)) {
                    _snprintf(what, sizeof(what), "alias %d loaded sound %s/%s byte +0x%02X: %02X vs C1 %02X", i,
                              Readable(sl, 4) && Readable(*(const char *const *)sl, 1) ? *(const char *const *)sl : "?",
                              Readable(cl, 4) && Readable(*(const char *const *)cl, 1) ? *(const char *const *)cl : "?",
                              at, Readable(sl, 48) ? sl[at] : 0, Readable(cl, 48) ? cl[at] : 0);
                    PiDiff(k, name, what);
                    return FALSE;
                }
                DWORD len = *(const DWORD *)(sl + 12);
                const BYTE *sd = *(const BYTE *const *)(sl + 44), *cd = *(const BYTE *const *)(cl + 44);
                if (len && sd && cd && Readable(sd, len) && Readable(cd, len) && memcmp(sd, cd, len)) {
                    unsigned d = 0;
                    while (d < len && sd[d] == cd[d]) d++;
                    _snprintf(what, sizeof(what), "alias %d sample bytes (%lu, %s, format %ld) first differ at %u, %lu bytes differ",
                              i, (unsigned long)len, *(const char *const *)sl, (long)*(const int *)(sl + 4), d,
                              (unsigned long)0);
                    { unsigned long nd = 0; for (DWORD q = 0; q < len; q++) nd += sd[q] != cd[q];
                      _snprintf(what, sizeof(what), "alias %d sample bytes (%lu, %s, format %ld) first differ at %u, %lu bytes differ",
                                i, (unsigned long)len, *(const char *const *)sl, (long)*(const int *)(sl + 4), d, nd); }
                    PiDiff(k, name, what);
                    return FALSE;
                }
            } else if (!PiStrEq(*(const char *const *)(sf + 4), *(const char *const *)(cf + 4)) ||
                       !PiStrEq(*(const char *const *)(sf + 8), *(const char *const *)(cf + 8))) {
                _snprintf(what, sizeof(what), "alias %d stream name", i);
                PiDiff(k, name, what);
                return FALSE;
            }
        }
        const BYTE *sc = *(const BYTE *const *)(x + 88), *cc = *(const BYTE *const *)(y + 88);
        if (!sc != !cc) { _snprintf(what, sizeof(what), "alias %d curve presence", i); PiDiff(k, name, what); return FALSE; }
        if (sc) {
            static const unsigned curvePtrs[] = { 0 };
            unsigned knots = *(const unsigned short *)(cc + 4);
            if (knots > 16) knots = 16;
            BOOL nameEq = Readable(sc, 136) && Readable(cc, 136) && PiStrEq(*(const char *const *)sc, *(const char *const *)cc);
            at = 0;
            if (!nameEq || !PiBytesEq(sc, cc, 8 + 8 * knots, curvePtrs, 1, &at)) {
                _snprintf(what, sizeof(what), "alias %d curve %s/%s, byte +0x%02X: %02X vs C1 %02X (knots %u vs C1 %u)", i,
                          Readable(sc, 4) && Readable(*(const char *const *)sc, 1) ? *(const char *const *)sc : "?",
                          Readable(cc, 4) && Readable(*(const char *const *)cc, 1) ? *(const char *const *)cc : "?",
                          at, sc[at], cc[at], *(const unsigned short *)(sc + 4), knots);
                PiDiff(k, name, what);
                return FALSE;
            }
        }
        const BYTE *ss = *(const BYTE *const *)(x + 104), *cs = *(const BYTE *const *)(y + 104);
        if (!ss != !cs) { _snprintf(what, sizeof(what), "alias %d speaker map presence", i); PiDiff(k, name, what); return FALSE; }
        if (ss) {
            static const unsigned spkPtrs[] = { 4 };
            if (!Readable(ss, 408) || !Readable(cs, 408) || !PiBytesEq(ss, cs, 408, spkPtrs, 1, &at) ||
                !PiStrEq(*(const char *const *)(ss + 4), *(const char *const *)(cs + 4))) {
                _snprintf(what, sizeof(what), "alias %d speaker map", i);
                PiDiff(k, name, what);
                return FALSE;
            }
        }
    }
    return TRUE;
}

/* C1 RawFile (0x4c58a0, 16 bytes): name, compressedLen, len, buffer (compressed, or len bytes + NUL). */
static BOOL PiRawFileEq(PiCheck *k, const char *name, const BYTE *s, const BYTE *c)
{
    char what[160];
    if (!Readable(s, 16) || !Readable(c, 16)) { PiDiff(k, name, "unreadable record"); return FALSE; }
    if (!PiStrEq(*(const char *const *)s, *(const char *const *)c)) { PiDiff(k, name, "name"); return FALSE; }
    int sc = *(const int *)(s + 4), cc = *(const int *)(c + 4), sl = *(const int *)(s + 8), cl = *(const int *)(c + 8);
    if (sc != cc || sl != cl) {
        _snprintf(what, sizeof(what), "compressedLen/len %d/%d vs C1 %d/%d", sc, sl, cc, cl);
        PiDiff(k, name, what);
        return FALSE;
    }
    DWORD n = sc ? (DWORD)sc : (DWORD)sl + 1;
    const BYTE *sb = *(const BYTE *const *)(s + 12), *cb = *(const BYTE *const *)(c + 12);
    if (!sb || !cb || !Readable(sb, n) || !Readable(cb, n)) { PiDiff(k, name, "unreadable buffer"); return FALSE; }
    if (memcmp(sb, cb, n)) {
        DWORD d = 0;
        while (d < n && sb[d] == cb[d]) d++;
        _snprintf(what, sizeof(what), "buffer byte %lu of %lu", (unsigned long)d, (unsigned long)n);
        PiDiff(k, name, what);
        return FALSE;
    }
    return TRUE;
}

/* C1 StringTable (0x4c5b90, 16 bytes): name, columns, rows, {string, hash} cells. */
static BOOL PiStringTableEq(PiCheck *k, const char *name, const BYTE *s, const BYTE *c)
{
    char what[160];
    if (!Readable(s, 16) || !Readable(c, 16)) { PiDiff(k, name, "unreadable record"); return FALSE; }
    if (!PiStrEq(*(const char *const *)s, *(const char *const *)c)) { PiDiff(k, name, "name"); return FALSE; }
    int cols = *(const int *)(s + 4), rows = *(const int *)(s + 8);
    if (cols != *(const int *)(c + 4) || rows != *(const int *)(c + 8)) {
        _snprintf(what, sizeof(what), "%dx%d vs C1 %dx%d", cols, rows, *(const int *)(c + 4), *(const int *)(c + 8));
        PiDiff(k, name, what);
        return FALSE;
    }
    unsigned count = (unsigned)cols * (unsigned)rows;
    const BYTE *sv = *(const BYTE *const *)(s + 12), *cv = *(const BYTE *const *)(c + 12);
    if (count && (!Readable(sv, 8 * count) || !Readable(cv, 8 * count))) { PiDiff(k, name, "unreadable cells"); return FALSE; }
    for (unsigned i = 0; i < count; i++) {
        const char *ss = *(const char *const *)(sv + 8 * i), *cs = *(const char *const *)(cv + 8 * i);
        if (*(const int *)(sv + 8 * i + 4) != *(const int *)(cv + 8 * i + 4) || !PiStrEq(ss, cs)) {
            _snprintf(what, sizeof(what), "cell %u (row %u col %u)", i, i / (unsigned)cols, i % (unsigned)cols);
            PiDiff(k, name, what);
            return FALSE;
        }
    }
    return TRUE;
}

static int PiC1NameEq(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = (unsigned char)*a, y = (unsigned char)*b;
        if (x == 92) x = '/';
        if (y == 92) y = '/';
        x = tolower(x); y = tolower(y);
        if (x != y) return 0;
        if (!x) return 1;
    }
}

/* C1's header for (type, name), or NULL when C1 has no real (non-default) entry. */
static const void *PiC1Find(unsigned type, const char *name)
{
    DWORD h = type;
    for (const char *p = name; *p; p++) {
        int c = tolower((unsigned char)*p);
        if (c == 92) c = '/';
        h = h * 31 + (DWORD)c;
    }
    unsigned idx = ((const unsigned short *)(DWORD_PTR)C1_DB_HASHTABLE_VA)[h % 60000u];
    typedef const char *(__cdecl *NameFn)(const void *);
    NameFn nameOf = ((const NameFn *)(DWORD_PTR)C1_DB_NAMEFN_VA)[type];
    for (unsigned guard = 0; idx && guard < 65536; guard++) {
        const BYTE *e = (const BYTE *)(DWORD_PTR)(C1_DB_ENTRIES_VA + 24u * idx);
        if (*(const unsigned *)e == type && nameOf) {
            const char *n = nameOf(e + 8);
            if (n && PiC1NameEq(n, name)) return e[0x10] ? *(const void *const *)(e + 8) : NULL;
        }
        idx = *(const unsigned short *)(e + 0x12);
    }
    return NULL;
}

static Sh1xFindAssetFn g_paFind;
static unsigned g_paType;

/* A name repeats once per archive that defines it; each is checked once (FNV-1a set of 32768 slots). */
static DWORD g_paSeen[32768];
static BOOL PiSeen(const char *name)
{
    DWORD h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) h = (h ^ (*p | 0x20)) * 16777619u;
    if (!h) h = 1;
    for (DWORD i = h & 32767, n = 0; n < 32768; i = (i + 1) & 32767, n++) {
        if (g_paSeen[i] == h) return TRUE;
        if (!g_paSeen[i]) { g_paSeen[i] = h; return FALSE; }
    }
    return FALSE;
}

static int __cdecl PiCheckOne(void *ctx, const char *name)
{
    PiCheck *k = ctx;
    if (!name || !*name || *name == ',') return 1;
    if (PiSeen(name)) return 1;
    if (k->checked && k->checked % 1000 == 0) Log("p2passetcheck: progress %u, next %s", k->checked, name);
    if (*(volatile const DWORD *)(DWORD_PTR)C1_DB_BUSY_VA) { k->busy = 1; return 0; }
    k->checked++;
    const void *s = g_paFind(g_paType, name);
    if (!s) { k->serverMissing++; return 1; }
    const void *c = PiC1Find(g_paType, name);
    if (!c) { k->c1Missing++; return 1; }
    BOOL eq = g_paType == 11 ? PiSoundEq(k, name, s, c) : g_paType == 36 ? PiRawFileEq(k, name, s, c) :
              g_paType == 37 ? PiStringTableEq(k, name, s, c) : FALSE;
    if (eq) k->equal++; else k->differ++;
    return 1;
}

/* On the server's fiber (PiRunOnServer): the server's lookups run in the server's own context. */
static Sh1xEnumAssetsFn g_paEnum;
static const char *g_paWhich;
static int g_paResult;
static void PiCheckOnServer(void *ctx)
{
    PiCheck *k = ctx;
    if (strcmp(g_paWhich, "*")) { PiCheckOne(k, g_paWhich); g_paResult = 1; }
    else g_paResult = g_paEnum(g_paType, PiCheckOne, k);
}

static void __cdecl PiCmdAssetCheck(void)
{
    const char *cl = GetCommandLineA();
    if (!cl || !strstr(cl, "k33_cmdFile 1")) { C1Printf("p2passetcheck: test clients only (+set k33_cmdFile 1)\n"); return; }
    if (g_piState != PI_RUNNING || !g_piBase) { C1Printf("p2passetcheck: no in-process server running\n"); return; }
    unsigned type = VoiceArgc() > 1 ? (unsigned)atoi(VoiceArgv(1)) : 11;
    const char *which = VoiceArgc() > 2 ? VoiceArgv(2) : "*";
    HMODULE m = (HMODULE)(DWORD_PTR)g_piBase;
    g_paFind = (Sh1xFindAssetFn)(void *)GetProcAddress(m, "Sh1xHost_FindAsset");
    Sh1xEnumAssetsFn enumerate = (Sh1xEnumAssetsFn)(void *)GetProcAddress(m, "Sh1xHost_EnumerateAssets");
    if (!g_paFind || !enumerate) { C1Printf("p2passetcheck: the server DLL has no asset API (needs feat/iw4-asset-layout)\n"); return; }
    if (type != 11 && type != 36 && type != 37) { C1Printf("p2passetcheck: type %u is not converted yet (11 sound, 36 rawfile, 37 stringtable)\n", type); return; }
    g_paType = type;
    PiCheck k;
    memset(&k, 0, sizeof(k));
    memset(g_paSeen, 0, sizeof(g_paSeen));
    DWORD t0 = GetTickCount();
    g_paEnum = enumerate;
    g_paWhich = which;
    g_paResult = 0;
    if (!PiRunOnServer(PiCheckOnServer, &k)) { C1Printf("p2passetcheck: could not run on the server's fiber\n"); return; }
    if (g_paResult < 0) { C1Printf("p2passetcheck: the server cannot enumerate type %u\n", type); return; }
    char line[512];
    _snprintf(line, sizeof(line) - 1, "p2passetcheck type %u: %u checked, %u equal to C1, %u differ, %u not loaded in C1, "
              "%u not in the server (%lu ms)%s%s%s", type, k.checked, k.equal, k.differ, k.c1Missing, k.serverMissing,
              GetTickCount() - t0, k.busy ? "; STOPPED: C1's database is loading a zone, run it again" : "",
              k.firstDiff[0] ? "; first: " : "", k.firstDiff);
    line[sizeof(line) - 1] = 0;
    Log("%s", line);
    C1Printf("%s\n", line);
}
