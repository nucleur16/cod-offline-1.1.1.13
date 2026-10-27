/* The survival match category: names and rules shared by survival_category.c (Kernel33), its tests and the server.
 * Pure C, no Windows and no C library (the x86 fixture links with -nostdlib).
 *
 * C1 1.1.1.13 knows three categories. Its zone-list builder (0x4CCDF0) loads "patch_common_gametype_<cat>",
 * "common_gametype_<cat>", "patch_localized_common_gametype_<cat>" and "localized_common_gametype_<cat>" (alloc flags
 * 4) for a category that starts "zm" or "coop" and the mp set for anything else; its const configstring selector
 * (0x612850) picks mp/constConfigStrings_{mp,coop,zm}.csv the same way. CoDOL 2.6.9.7 adds common_gametype_survival
 * with the same four-zone pattern (its builder 0x4D1020 formats patch_%s, %s, patch_localized_%s, localized_%s).
 *
 * Kernel33 adds the category "survival" (exact, ignoring case, as C1 compares categories with Q_strnicmp). Every other
 * category string takes C1's own code unchanged. */
#ifndef K33_SURVIVAL_CATEGORY_POLICY_H
#define K33_SURVIVAL_CATEGORY_POLICY_H

#define K33_SURVIVAL_CATEGORY "survival"
#define K33_SURVIVAL_GAMETYPE_ZONE "common_gametype_survival"
/* C1 asks for mp/constConfigStrings_<cat>.csv; asset names are case-insensitive. The zone stores it lower case, as
 * code_post_gfx_mp stores the stock three. */
#define K33_SURVIVAL_CONST_TABLE "mp/constConfigStrings_survival.csv"
#define K33_SURVIVAL_ZONE_ALLOC_FLAGS 4 /* the gametype zones' flags in 0x4CCDF0 (and 2.6.9.7's 0x4D1020) */

/* In C1's order: patch_, the zone, patch_localized_, localized_. A missing patch_ zone only prints (0x8308DBD). */
static const char *const kK33SurvivalZones[4] = {
    "patch_common_gametype_survival",
    "common_gametype_survival",
    "patch_localized_common_gametype_survival",
    "localized_common_gametype_survival",
};

static int K33SurvLower(int c) { return c >= 'A' && c <= 'Z' ? c + 0x20 : c; }

/* a == b ignoring ASCII case; NULL never matches. */
static int K33SurvEqualI(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && K33SurvLower((unsigned char)*a) == K33SurvLower((unsigned char)*b)) a++, b++;
    return *a == 0 && *b == 0;
}

static int K33IsSurvivalCategory(const char *category)
{
    return K33SurvEqualI(category, K33_SURVIVAL_CATEGORY);
}

/* The config string checksum of the survival table. C1's own checksum (0x83B094E) is a TenProtect VM stub nobody has
 * reproduced (alpha0022 zones/zone_crc.py tried ~40 algorithms), so the server could only learn it from a client's
 * "Our config string checksum <want> doesn't match" line. For the survival table, which this project ships to client
 * and server alike, Kernel33 stores a checksum both sides can compute instead:
 *     CRC-32 (ISO-HDLC: zlib's crc32, reflected 0xEDB88320, init and final xor 0xFFFFFFFF) over every cell's bytes
 *     followed by one NUL, row-major (row 0 col 0, row 0 col 1, row 1 col 0, ...); a NULL cell counts as "".
 * The stock mp/coop/zm tables keep C1's native value. */
static unsigned K33SurvCrcUpdate(unsigned crc, const unsigned char *p, unsigned long n)
{
    crc = ~crc;
    while (n--) {
        int k;
        crc ^= *p++;
        for (k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static unsigned K33SurvCrcString(unsigned crc, const char *s)
{
    static const unsigned char nul = 0;
    unsigned long n = 0;
    if (s) {
        while (s[n]) n++;
        crc = K33SurvCrcUpdate(crc, (const unsigned char *)s, n);
    }
    return K33SurvCrcUpdate(crc, &nul, 1);
}

/* C1's StringTableCell (alpha0022 zone_crc.py, measured at 0x72AFB7): {const char *string; int hash;}. */
typedef struct { const char *string; int hash; } K33SurvStringTableCell;
typedef struct { const char *name; int columnCount; int rowCount; const K33SurvStringTableCell *values; } K33SurvStringTable;

static unsigned K33SurvTableChecksum(const K33SurvStringTableCell *cells, unsigned long count)
{
    unsigned crc = 0;
    unsigned long i;
    for (i = 0; i < count; i++) crc = K33SurvCrcString(crc, cells[i].string);
    return crc;
}

#endif
