/* survival_category_policy.h: the category rule, the zone set and the checksum rule.
 *   survival_category_test.exe [table.csv]   with a CSV: prints its rows and checksum (tests/test_survival_category.py) */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../survival_category_policy.h"

static void categories(void)
{
    static const char *const yes[] = { "survival", "SURVIVAL", "Survival", "sUrViVaL" };
    static const char *const no[] = { "", "mp", "combat", "war", "coop", "zm", "coop_survival", "survival_", "survivals",
                                      "surviva", "xsurvival", " survival", "survival ", "evac", "cyborg" };
    for (unsigned i = 0; i < sizeof(yes) / sizeof(yes[0]); i++) assert(K33IsSurvivalCategory(yes[i]));
    for (unsigned i = 0; i < sizeof(no) / sizeof(no[0]); i++) assert(!K33IsSurvivalCategory(no[i]));
    assert(!K33IsSurvivalCategory(NULL));
}

static void zones(void)
{
    /* C1's order for zm/coop/mp (0x4CCEFD..0x4CCF60) and 2.6.9.7's helper 0x4D1020 */
    assert(!strcmp(kK33SurvivalZones[0], "patch_" K33_SURVIVAL_GAMETYPE_ZONE));
    assert(!strcmp(kK33SurvivalZones[1], K33_SURVIVAL_GAMETYPE_ZONE));
    assert(!strcmp(kK33SurvivalZones[2], "patch_localized_" K33_SURVIVAL_GAMETYPE_ZONE));
    assert(!strcmp(kK33SurvivalZones[3], "localized_" K33_SURVIVAL_GAMETYPE_ZONE));
    assert(K33_SURVIVAL_ZONE_ALLOC_FLAGS == 4);
    assert(K33SurvEqualI(K33_SURVIVAL_CONST_TABLE, "mp/constconfigstrings_survival.csv"));
}

static void checksum(void)
{
    static const K33SurvStringTableCell cells[] = { { "2", 50 }, { "cod", 0 }, { NULL, 0 }, { "x", 0 } };
    assert(K33SurvCrcUpdate(0, (const unsigned char *)"123456789", 9) == 0xCBF43926u); /* CRC-32/ISO-HDLC check */
    assert(K33SurvTableChecksum(cells, 4) == 0x2E66357Eu); /* zlib.crc32 over "2\0cod\0\0x\0" */
    assert(K33SurvTableChecksum(cells, 0) == 0);
}

static int table(const char *path)
{
    static char line[4096];
    FILE *f = fopen(path, "rb");
    unsigned crc = 0, rows = 0;
    if (!f) return 1;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n'), *comma = strchr(line, ',');
        if (nl) *nl = 0;
        if (!comma || strchr(comma + 1, ',')) { fclose(f); return 2; }
        *comma = 0;
        crc = K33SurvCrcString(crc, line);
        crc = K33SurvCrcString(crc, comma + 1);
        rows++;
    }
    fclose(f);
    printf("rows %u checksum 0x%08X\n", rows, crc);
    return 0;
}

int main(int argc, char **argv)
{
    categories();
    zones();
    checksum();
    if (argc > 1) return table(argv[1]);
    puts("survival category policy: ok");
    return 0;
}
