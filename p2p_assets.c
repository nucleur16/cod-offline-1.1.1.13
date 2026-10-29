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

static struct { char key[96]; unsigned count; } g_paHist[32];
static unsigned g_paHistN;
static void PiHist(const char *what)
{
    char key[96];
    unsigned n = 0;
    while (what[n] && what[n] != ':' && n < sizeof(key) - 1) { key[n] = what[n]; n++; }
    key[n] = 0;
    for (unsigned i = 0; i < g_paHistN; i++) if (!strcmp(g_paHist[i].key, key)) { g_paHist[i].count++; return; }
    if (g_paHistN < 32) { strcpy(g_paHist[g_paHistN].key, key); g_paHist[g_paHistN++].count = 1; }
}
static void PiDiff(PiCheck *k, const char *name, const char *what)
{
    PiHist(what);
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

/* The zone C1's linked (first) definition of (type, name) came from: the entry's zone index +0x10 names a slot of
 * the zone array (0x1F0E108 + index * 0xA8, name at +0x0C). NULL for none, a default asset, or while the database
 * thread links a zone. */
static unsigned g_piFindInLogged;
static const char *PiC1ZoneOf(unsigned type, const char *name)
{
    DWORD h = type;
    if (!name || *(volatile const DWORD *)(DWORD_PTR)0x02007A60u) return NULL;
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
            if (n && PiC1NameEq(n, name)) {
                const char *in;
                if (!e[0x10]) return NULL;
                in = (const char *)(DWORD_PTR)(0x01F0E108u + 0xA8u * e[0x10] + 0x0C);
                if (g_piFindInLogged < 12) { g_piFindInLogged++; Log("p2p in-process: C1 type %u %s is in zone %u %s", type, name, e[0x10], in); }
                return in;
            }
        }
        idx = *(const unsigned short *)(e + 0x12);
    }
    return NULL;
}

/* C1 WeaponDef (loader 0x4c3c20, 0xBFC bytes): the pointer slots of the server's codol_c1_weapon.c / the
 * replay_weapon field table: 1 string, 2 sound, 3 31-entry sound array, 4 asset (compared by the name every
 * C1 asset header starts with), 5 graph points. Script-string handle arrays are skipped (9). */
static const struct { unsigned offset, count, kind; } kWeaponSlots[] = {
    {0,4,1},{0x10,11,4},{0x7c,144,1},{0x35c,2,4},{0x364,53,2},{0x438,2,3},{0x440,4,4},{0x450,2,4},{0x52c,6,4},
    {0x548,1,4},{0x550,1,4},{0x564,1,1},{0x56c,1,1},{0x57c,1,1},{0x684,4,4},{0x758,1,4},{0x7b0,1,4},{0x7b8,2,4},
    {0x7c0,2,2},{0x8cc,2,4},{0x8ec,1,4},{0x8f0,1,2},{0x9a0,1,1},{0x9a8,1,5},{0x9a4,1,1},{0x9ac,1,5},{0x9fc,2,1},
    {0xa20,2,1},{0xaa0,2,1},{0xaa8,1,4},{0xac4,1,2},{0xac8,1,4},{0xacc,1,1},{0xadc,9,2},{0xb00,2,2},{0xb48,6,2},
    {0xb78,1,1},{0xb84,2,1},{0xb90,3,4},{0xbb4,2,5}
};
static unsigned PiWeaponKind(unsigned off)
{
    /* script-string handles, which belong to the holder's string table: the 32 hide tags at +0x3c and the four
     * 16-entry name maps at +0x2bc..+0x33b (C1 converts them to its own handles at load) */
    if ((off >= 0x3c && off < 0x7c) || (off >= 0x2bc && off < 0x33c)) return 9;
    /* Runtime registrations C1 writes into the pool record once a match registers its weapons (measured in a
     * match, 0 before): the ammo index +0x568, clip index +0x570 and shared ammo cap index +0x580, each beside
     * its name slot (+0x564/+0x56c/+0x57c). Loaded data is 0 there. */
    if (off == 0x568 || off == 0x570 || (off >= 0x580 && off < 0x584)) return 8;
    for (unsigned i = 0; i < sizeof(kWeaponSlots) / sizeof(kWeaponSlots[0]); i++)
        if (off >= kWeaponSlots[i].offset && off < kWeaponSlots[i].offset + 4 * kWeaponSlots[i].count) return kWeaponSlots[i].kind;
    return 0;
}
/* The name a C1 asset header starts with (both sides hold records whose first word is the name). */
static const char *PiAssetName(const void *p)
{
    if (!p || !Readable(p, 4)) return NULL;
    const char *n = *(const char *const *)p;
    return n && Readable(n, 1) ? n : NULL;
}
static BOOL PiWeaponEq(PiCheck *k, const char *name, const BYTE *s, const BYTE *c)
{
    char what[200];
    if (!Readable(s, 0xbfc) || !Readable(c, 0xbfc)) { PiDiff(k, name, "unreadable record"); return FALSE; }
    for (unsigned off = 0; off < 0xbfc; ) {
        unsigned kind = PiWeaponKind(off);
        if (!kind) {
            if (s[off] != c[off]) {
                unsigned a = off & ~3u;
                _snprintf(what, sizeof(what), "byte +0x%03X: %02X vs C1 %02X (dword +0x%03X %08lX vs C1 %08lX)", off, s[off], c[off],
                          a, (unsigned long)*(const DWORD *)(s + a), (unsigned long)*(const DWORD *)(c + a));
                if (k->reported < 20) { char ranges[400]; int used = 0; unsigned n = 0, q = off;
                    ranges[0] = 0;
                    while (q < 0xbfc) {
                        if (PiWeaponKind(q) || s[q] == c[q]) { q++; continue; }
                        unsigned st = q; while (q < 0xbfc && !PiWeaponKind(q) && s[q] != c[q]) q++;
                        n += q - st;
                        if (used < 360) used += _snprintf(ranges + used, sizeof(ranges) - used, " %03X-%03X", st, q - 1);
                    }
                    Log("p2passetcheck: %s: %u scalar bytes differ:%s", name, n, ranges); }
                PiDiff(k, name, what);
                return FALSE;
            }
            off++;
            continue;
        }
        if (kind == 9) { off += 2; continue; }
        if (kind == 8) { off += 1; continue; }
        const void *sp = *(const void *const *)(s + off), *cp = *(const void *const *)(c + off);
        BOOL eq = TRUE;
        const char *sn = NULL, *cn = NULL;
        if (kind == 1) { sn = sp && Readable(sp, 1) ? sp : NULL; cn = cp && Readable(cp, 1) ? cp : NULL; eq = PiStrEq(sn, cn); }
        else if (kind == 2 || kind == 4) { sn = PiAssetName(sp); cn = PiAssetName(cp); eq = (!sp == !cp) && PiStrEq(sn, cn); }
        else if (kind == 3) {
            if (!sp != !cp) eq = FALSE;
            else if (sp && Readable(sp, 124) && Readable(cp, 124))
                for (unsigned i = 0; i < 31 && eq; i++) {
                    const void *a = ((const void *const *)sp)[i], *b = ((const void *const *)cp)[i];
                    sn = PiAssetName(a); cn = PiAssetName(b);
                    eq = (!a == !b) && PiStrEq(sn, cn);
                }
        } else if (kind == 5) {
            unsigned count = *(const unsigned short *)(c + (off == 0x9a8 || off == 0xbb4 ? 0xbb0 : 0xbb2));
            if (!sp != !cp) eq = FALSE;
            else if (sp && count && (!Readable(sp, 8 * count) || !Readable(cp, 8 * count) || memcmp(sp, cp, 8 * count))) eq = FALSE;
        }
        if (!eq) {
            _snprintf(what, sizeof(what), "slot +0x%03X kind %u: %s vs C1 %s", off, kind, sn ? sn : (sp ? "(set)" : "null"),
                      cn ? cn : (cp ? "(set)" : "null"));
            PiDiff(k, name, what);
            return FALSE;
        }
        off += 4;
    }
    return TRUE;
}

static Sh1xFindAssetFn g_paFind;
static BOOL g_paGeneric;
static unsigned g_paType;

/* ---- Generic comparison of C1-layout header copies (server codol_c1Fixups 1) ------------------------------------
 * The server replays each zone the way C1's loader does and records every pointer word C1 relocates (a fix-up):
 * Sh1xHost_FindC1Header gives the header as C1 holds it after DB_AddXAsset, with pointer words still serialized,
 * Sh1xHost_C1PointersIn lists the pointer words of a range and Sh1xHost_ResolveC1Pointer what C1 stores in each:
 * loaded data (compared recursively, byte for byte), an asset header (compared by name) or a sound alias name
 * (C1 stores the snd_alias_list_t it finds by it). Every other byte must equal C1's. Any type the replay catalogues. */
typedef const void *(__cdecl *Sh1xFindC1HeaderFn)(unsigned, const char *, unsigned *);
typedef const void *(__cdecl *Sh1xResolveC1Fn)(const void *, unsigned *, unsigned *);
typedef int (__cdecl *Sh1xPointersInFn)(const void *, unsigned, unsigned *, unsigned);
static Sh1xFindC1HeaderFn g_paHeader;
static Sh1xResolveC1Fn g_paResolve;
static Sh1xPointersInFn g_paPointers;
typedef const void *(__cdecl *Sh1xFindC1RecordFn)(unsigned, const char *);
static Sh1xFindC1RecordFn g_paRecord;
static BOOL g_paMaterialized;
static unsigned g_paPooled, g_paPointerWords, g_paBytes, g_paScript, g_paRuntime;

static BOOL PiReadableSpan(const void *p, SIZE_T n)
{
    const BYTE *a = p, *end = a + n;
    if (!p || end < a) return FALSE;
    while (a < end) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(a, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            return FALSE;
        a = (const BYTE *)mbi.BaseAddress + mbi.RegionSize;
    }
    return TRUE;
}
/* The name field of a C1 asset header: GfxImage keeps it at +32, LocalizeEntry at +4, every other type at +0. */
static unsigned PiNameOffset(unsigned type) { return type == 10 ? 32 : type == 27 ? 4 : 0; }
static const char *PiSkipComma(const char *n) { return n && *n == ',' ? n + 1 : n; }
static BOOL PiCString(const BYTE *p, unsigned max)
{
    for (unsigned i = 0; i < max; i++) {
        /* one VirtualQuery per page, not per byte */
        if ((!i || !((DWORD_PTR)(p + i) & 0xfff)) && !Readable(p + i, 1)) return FALSE;
        if (!p[i]) return i > 0;
        if (p[i] < 0x20 && p[i] != '\t' && p[i] != '\n' && p[i] != '\r') return FALSE;
    }
    return FALSE;
}
/* m: the server's materialized record (real pointers) walked beside the header copy s, which gives the pointer
 * words; NULL compares the header copy itself. */
static BOOL PiGenEq(PiCheck *k, const char *name, const BYTE *s, const BYTE *m, const BYTE *c, unsigned size, unsigned depth, const char *path);
/* Data reached more than once (pooled pointers, e.g. every brush side's plane) is compared once per asset: the
 * server addresses already compared, tagged with the asset's generation. */
static struct { const void *at; unsigned gen; } g_paVisited[1 << 18];
static unsigned g_paGen, g_paRevisits;
static BOOL PiVisited(const void *at)
{
    unsigned h = (unsigned)((DWORD_PTR)at * 2654435761u) >> 14;
    for (unsigned n = 0; n < 64; n++, h = (h + 1) & ((1 << 18) - 1)) {
        if (g_paVisited[h].gen != g_paGen) { g_paVisited[h].at = at; g_paVisited[h].gen = g_paGen; return FALSE; }
        if (g_paVisited[h].at == at) { g_paRevisits++; return TRUE; }
    }
    return FALSE;
}
static BOOL PiGenPointerEq(PiCheck *k, const char *name, const BYTE *slot, const BYTE *mslot, const BYTE *cslot, unsigned depth, const char *path)
{
    char what[256];
    unsigned kind = 0, size = 0;
    const BYTE *t = g_paResolve(slot, &kind, &size);
    const BYTE *cp = *(const BYTE *const *)cslot;
    const BYTE *mp = mslot ? *(const BYTE *const *)mslot : NULL;
    g_paPointerWords++;
    if (!t) { _snprintf(what, sizeof(what), "%s: unresolved pointer (kind %u)", path, kind); PiDiff(k, name, what); return FALSE; }
    if (!cp) { _snprintf(what, sizeof(what), "%s: null in C1 (kind %u)", path, kind & 0xff); PiDiff(k, name, what); return FALSE; }
    if (mslot && !mp) { _snprintf(what, sizeof(what), "%s: null in the server record (kind %u)", path, kind & 0xff); PiDiff(k, name, what); return FALSE; }
    if (mslot && (kind & 0xff) != 1) t = NULL; /* names below come from the record's own pointers */
    switch (kind & 0xff) {
    case 1: /* data */
        if (size) {
            if (depth >= 16 || PiVisited(t)) return TRUE;
            return PiGenEq(k, name, t, mp, cp, size, depth + 1, path);
        }
        /* a pooled (deduplicated) reference: its extent is not recorded; strings are compared, other data is
         * compared where it was loaded */
        g_paPooled++;
        if (mp) t = mp;
        if (PiCString(t, 4096)) {
            if (!PiCString(cp, 4096) || strcmp((const char *)t, (const char *)cp)) {
                _snprintf(what, sizeof(what), "%s: pooled string \"%.60s\" vs C1 \"%.60s\"", path, (const char *)t,
                          PiCString(cp, 4096) ? (const char *)cp : "?");
                PiDiff(k, name, what);
                return FALSE;
            }
        }
        return TRUE;
    case 2: { /* asset header: by name */
        unsigned type = (kind >> 8) & 0xff, off = type == 0xff ? 0 : PiNameOffset(type), nk = 0, ns = 0;
        const char *sn = mp ? (Readable(mp + off, 4) ? *(const char *const *)(mp + off) : NULL) : g_paResolve(t + off, &nk, &ns);
        const char *cn = Readable(cp + off, 4) ? *(const char *const *)(cp + off) : NULL;
        if (!cn || !Readable(cn, 1) || !sn || !Readable(sn, 1) || _stricmp(PiSkipComma(sn), PiSkipComma(cn))) {
            _snprintf(what, sizeof(what), "%s: asset (type %u) %s vs C1 %s", path, type, sn && Readable(sn, 1) ? sn : "?",
                      cn && Readable(cn, 1) ? cn : "?");
            PiDiff(k, name, what);
            return FALSE;
        }
        return TRUE;
    }
    case 7: { /* a name C1 resolved to the header of that type */
        unsigned type = (kind >> 8) & 0xff, off = PiNameOffset(type);
        const char *cn = Readable(cp + off, 4) ? *(const char *const *)(cp + off) : NULL;
        if (mp) t = Readable(mp + off, 4) ? *(const BYTE *const *)(mp + off) : NULL;
        if (!t || !Readable(t, 1)) { _snprintf(what, sizeof(what), "%s: unnamed server record", path); PiDiff(k, name, what); return FALSE; }
        if (!cn || !Readable(cn, 1) || _stricmp(PiSkipComma((const char *)t), PiSkipComma(cn))) {
            _snprintf(what, sizeof(what), "%s: named asset (type %u) %s vs C1 %s", path, type, (const char *)t,
                      cn && Readable(cn, 1) ? cn : "?");
            PiDiff(k, name, what);
            return FALSE;
        }
        return TRUE;
    }
    case 4: { /* sound alias name; C1 holds the snd_alias_list_t */
        const char *cn = Readable(cp, 4) ? *(const char *const *)cp : NULL;
        if (mp) t = Readable(mp, 4) ? *(const BYTE *const *)mp : NULL;
        if (!t || !Readable(t, 1)) { _snprintf(what, sizeof(what), "%s: unnamed server alias list", path); PiDiff(k, name, what); return FALSE; }
        if (!cn || !Readable(cn, 1) || _stricmp(PiSkipComma((const char *)t), PiSkipComma(cn))) {
            _snprintf(what, sizeof(what), "%s: sound %s vs C1 %s", path, (const char *)t, cn && Readable(cn, 1) ? cn : "?");
            PiDiff(k, name, what);
            return FALSE;
        }
        return TRUE;
    }
    default:
        _snprintf(what, sizeof(what), "%s: pointer kind %u", path, kind);
        PiDiff(k, name, what);
        return FALSE;
    }
}
static BOOL PiGenEq(PiCheck *k, const char *name, const BYTE *s, const BYTE *m, const BYTE *c, unsigned size, unsigned depth, const char *path)
{
    char what[256], sub[160];
    if (!size) return TRUE;
    if (!PiReadableSpan(s, size) || !PiReadableSpan(c, size) || (m && !PiReadableSpan(m, size))) {
        _snprintf(what, sizeof(what), "%s: unreadable %u bytes", path, size); PiDiff(k, name, what); return FALSE;
    }
    int n = g_paPointers(s, size, NULL, 0);
    unsigned *offs = n > 0 ? HeapAlloc(GetProcessHeap(), 0, n * sizeof(unsigned)) : NULL;
    if (n > 0 && !offs) { PiDiff(k, name, "out of memory"); return FALSE; }
    if (n > 0) g_paPointers(s, size, offs, (unsigned)n);
    if (n < 0) n = 0;
    /* width of each listed word: 4 for a pointer; an annotation (5 script strings, 6 written by C1 itself) covers
     * its size and is skipped */
    unsigned *width = n > 0 ? HeapAlloc(GetProcessHeap(), 0, n * sizeof(unsigned)) : NULL;
    if (n > 0 && !width) { HeapFree(GetProcessHeap(), 0, offs); PiDiff(k, name, "out of memory"); return FALSE; }
    for (int i = 0; i < n; i++) {
        unsigned kind = 0, sz = 0;
        g_paResolve(s + offs[i], &kind, &sz);
        width[i] = (kind & 0xff) == 5 || (kind & 0xff) == 6 ? (sz & 0x3fffffffu) | 0x40000000u : 4;
        if ((kind & 0xff) == 5) g_paScript += sz; else if ((kind & 0xff) == 6) g_paRuntime += sz;
    }
    /* a listed word that runs past the end (a window into an array) ends the comparison before it */
    while (n > 0 && offs[n - 1] + (width[n - 1] & 0x3fffffffu) > size) size = offs[--n];
    g_paBytes += size;
    const BYTE *v = m ? m : s;
    BOOL eq = TRUE;
    unsigned prev = 0;
    for (int i = 0; i <= n && eq; i++) {
        if (i < n && offs[i] < prev) continue;
        unsigned end = i < n ? offs[i] : size;
        if (end > size) end = size;
        if (end > prev && memcmp(v + prev, c + prev, end - prev)) {
            unsigned at = prev;
            while (v[at] == c[at]) at++;
            /* the differing ranges of this block, listed words excluded */
            char ranges[300]; int used = 0; unsigned total = 0, q = at; int j = i;
            ranges[0] = 0;
            while (q < size) {
                while (j < n && offs[j] + (width[j] & 0x3fffffffu) <= q) j++;
                if (j < n && q >= offs[j]) { q = offs[j] + (width[j] & 0x3fffffffu); continue; }
                if (v[q] == c[q]) { q++; continue; }
                unsigned st = q;
                while (q < size && v[q] != c[q] && !(j < n && q >= offs[j])) q++;
                total += q - st;
                if (used < 260) used += _snprintf(ranges + used, sizeof(ranges) - used, " %X-%X", st, q - 1);
            }
            _snprintf(what, sizeof(what), "%s+0x%X: %02X vs C1 %02X (dword %08lX vs C1 %08lX; %u bytes differ:%s)", path, at,
                      v[at], c[at], (unsigned long)*(const DWORD *)(v + (at & ~3u)), (unsigned long)*(const DWORD *)(c + (at & ~3u)),
                      total, ranges);
            PiDiff(k, name, what);
            eq = FALSE;
            break;
        }
        if (i < n && (width[i] & 0x40000000u)) { prev = offs[i] + (width[i] & 0x3fffffffu); continue; }
        if (i < n && offs[i] + 4 <= size) {
            _snprintf(sub, sizeof(sub), "%.120s+0x%X>", path, offs[i]);
            if (!PiGenPointerEq(k, name, s + offs[i], m ? m + offs[i] : NULL, c + offs[i], depth, sub)) { eq = FALSE; break; }
            prev = offs[i] + 4;
        }
    }
    if (width) HeapFree(GetProcessHeap(), 0, width);
    if (offs) HeapFree(GetProcessHeap(), 0, offs);
    return eq;
}

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
    if (g_paGeneric) {
        unsigned length = 0;
        const void *hs = g_paHeader(g_paType, name, &length);
        if (!hs) { k->serverMissing++; return 1; }
        const void *hc = PiC1Find(g_paType, name);
        if (!hc) { k->c1Missing++; return 1; }
        const void *hm = NULL;
        if (g_paMaterialized && !(hm = g_paRecord(g_paType, name))) { k->serverMissing++; return 1; }
        if (!++g_paGen) g_paGen = 1;
        if (PiGenEq(k, name, hs, hm, hc, length, 0, "")) k->equal++; else k->differ++;
        return 1;
    }
    const void *s = g_paFind(g_paType, name);
    if (!s) { k->serverMissing++; return 1; }
    const void *c = PiC1Find(g_paType, name);
    if (!c) { k->c1Missing++; return 1; }
    BOOL eq = g_paType == 11 ? PiSoundEq(k, name, s, c) : g_paType == 36 ? PiRawFileEq(k, name, s, c) :
              g_paType == 37 ? PiStringTableEq(k, name, s, c) : g_paType == 28 ? PiWeaponEq(k, name, s, c) : FALSE;
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
    /* p2passetcheck <type> <name|*> c1: the generic comparison of the server's C1-layout header copies (any type
     * the replay catalogues; the server needs +set codol_c1Fixups 1). Without it, the converted types' records. */
    /* c1: the header copies; m: the materialized records (real pointers) walked beside them */
    g_paMaterialized = VoiceArgc() > 3 && !strcmp(VoiceArgv(3), "m");
    g_paGeneric = VoiceArgc() > 3 && (!strcmp(VoiceArgv(3), "c1") || g_paMaterialized);
    g_paRecord = (Sh1xFindC1RecordFn)(void *)GetProcAddress(m, "Sh1xHost_FindC1Record");
    if (g_paMaterialized && !g_paRecord) { C1Printf("p2passetcheck: the server DLL has no C1 record API\n"); return; }
    g_paHeader = (Sh1xFindC1HeaderFn)(void *)GetProcAddress(m, "Sh1xHost_FindC1Header");
    g_paResolve = (Sh1xResolveC1Fn)(void *)GetProcAddress(m, "Sh1xHost_ResolveC1Pointer");
    g_paPointers = (Sh1xPointersInFn)(void *)GetProcAddress(m, "Sh1xHost_C1PointersIn");
    if (g_paGeneric && (!g_paHeader || !g_paResolve || !g_paPointers)) { C1Printf("p2passetcheck: the server DLL has no C1 header API\n"); return; }
    if (!g_paGeneric && type != 11 && type != 28 && type != 36 && type != 37) { C1Printf("p2passetcheck: type %u is not converted yet (11 sound, 28 weapon, 36 rawfile, 37 stringtable; or add c1)\n", type); return; }
    g_paType = type;
    g_paPooled = g_paPointerWords = g_paBytes = g_paScript = g_paRuntime = 0;
    g_paHistN = 0;
    g_paRevisits = 0;
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
              "%u not in the server (%lu ms)%s%s%s%s", type, k.checked, k.equal, k.differ, k.c1Missing, k.serverMissing,
              GetTickCount() - t0, g_paMaterialized ? " [c1 records]" : g_paGeneric ? " [c1 headers]" : "", k.busy ? "; STOPPED: C1's database is loading a zone, run it again" : "",
              k.firstDiff[0] ? "; first: " : "", k.firstDiff);
    line[sizeof(line) - 1] = 0;
    Log("%s", line);
    for (unsigned i = 0; i < g_paHistN; i++) Log("p2passetcheck: first difference at %s: %u assets", g_paHist[i].key, g_paHist[i].count);
    if (g_paGeneric) Log("p2passetcheck: compared %u bytes, %u pointer words (%u pooled), skipped %u script-string and %u C1-written bytes, %u repeat visits",
                         g_paBytes, g_paPointerWords, g_paPooled, g_paScript, g_paRuntime, g_paRevisits);
    C1Printf("%s\n", line);
}

/* p2pslscan <xmodel>: find C1's script-string buffer. For each data dword D in C1's image that points at readable
 * memory, test whether D + stride * handle + offset gives a name for the model's first bone handles (strides 8/12/16,
 * offsets 0/4/8); log the candidates with the names they give. A one-off reverse-engineering aid. */
static BOOL PiNamelike(const char *s)
{
    int n = 0;
    if (!s || !Readable(s, 32)) return FALSE;
    if (!Readable(s - 1, 1) || s[-1]) return FALSE; /* a string start */
    if (strncmp(s, "tag_", 4) && strncmp(s, "j_", 2)) return FALSE; /* a bone name */
    for (; n < 32 && s[n]; n++) if (!((s[n] >= 'a' && s[n] <= 'z') || (s[n] >= '0' && s[n] <= '9') || s[n] == '_')) return FALSE;
    return n >= 2 && n < 32;
}
static void __cdecl PiCmdSlScan(void)
{
    static const unsigned strides[] = { 8, 12, 16 }, offsets[] = { 0, 4, 8 };
    const char *name = VoiceArgc() > 1 ? VoiceArgv(1) : "defaultactor";
    const BYTE *model = PiC1Find(4, name), *bones;
    unsigned n, hits = 0;
    if (!model || !Readable(model, 0x30)) { C1Printf("p2pslscan: no C1 xmodel %s\n", name); return; }
    bones = *(const BYTE *const *)(model + 0x2c); n = model[4];
    if (!bones || !Readable(bones, 2 * n) || n < 3) { C1Printf("p2pslscan: %s has no bone names\n", name); return; }
    Log("p2pslscan: %s bones %u, handles %u %u %u", name, n, ((const WORD *)bones)[0], ((const WORD *)bones)[1], ((const WORD *)bones)[2]);
    for (DWORD a = 0x00C00000u; a < 0x02800000u && hits < 20; a += 4) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!(a & 0xffff)) {
            if (!VirtualQuery((void *)(DWORD_PTR)a, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) { a = (DWORD)(DWORD_PTR)mbi.BaseAddress + (DWORD)mbi.RegionSize - 4; continue; }
        }
        if (!Readable((void *)(DWORD_PTR)a, 4)) continue;
        DWORD d = *(const DWORD *)(DWORD_PTR)a;
        if (d < 0x00010000u || d > 0x7fff0000u) continue;
        for (unsigned si = 0; si < 3; si++) for (unsigned oi = 0; oi < 3; oi++) {
            BOOL ok = TRUE;
            for (unsigned b = 0; b < 3 && ok; b++) {
                DWORD h = ((const WORD *)bones)[b];
                ok = h && PiNamelike((const char *)(DWORD_PTR)(d + strides[si] * h + offsets[oi]));
            }
            if (ok) {
                DWORD h0 = ((const WORD *)bones)[0], h1 = ((const WORD *)bones)[1];
                Log("p2pslscan: global 0x%08lX -> 0x%08lX stride %u offset %u: %s %s", (unsigned long)a, (unsigned long)d,
                    strides[si], offsets[oi], (const char *)(DWORD_PTR)(d + strides[si] * h0 + offsets[oi]),
                    (const char *)(DWORD_PTR)(d + strides[si] * h1 + offsets[oi]));
                hits++;
            }
        }
    }
    C1Printf("p2pslscan: %u candidates (Kernel33.log)\n", hits);
}
