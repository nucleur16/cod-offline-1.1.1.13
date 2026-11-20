/* The outgoing gamestate sanity check: a valid gamestate passes; each kind of
 * malformed gamestate fails with an error naming the offending item. */
#include "gamestate_check.h"
#include "configstrings.h"
#include "ol_msg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIMIT 131072u
static uint8_t buffer[200000];

typedef struct { unsigned index; const char *value; } entry_t;

/* A hand-built gamestate in WriteGamestate's layout, so entries the
 * bank refuses (out-of-range, duplicate) can still be expressed. */
static size_t raw_gamestate(const entry_t *entries, unsigned count, unsigned declared,
                            uint32_t client_number)
{
    ol_msg_t m;
    unsigned i;
    MsgWriteInit(&m, buffer, sizeof(buffer));
    WriteByte(&m, 2);
    WriteLong(&m, 7);
    OL_WriteString(&m, "mp_killhouse_sh");
    OL_WriteString(&m, "mp");
    WriteLong(&m, 0xad1e1713u);
    m.bit = 0;
    WriteByte(&m, 3);
    WriteShort(&m, declared);
    for (i = 0; i < count; ++i) {
        WriteBits(&m, 0, 1);
        WriteBits(&m, entries[i].index, 13);
        OL_WriteString(&m, entries[i].value);
    }
    WriteByte(&m, 5);
    WriteLong(&m, client_number);
    WriteLong(&m, 0x1234);
    WriteByte(&m, 5);
    assert(!m.failed);
    return m.size;
}

static void expect_fail(const uint8_t *data, size_t size, const configstrings_t *inherited,
                        size_t message, const char *needle)
{
    gamestate_check_t check;
    int ok = CheckGamestate(data, size, inherited, 12, message, LIMIT, &check);
    if (ok || !strstr(check.error, needle)) {
        fprintf(stderr, "expected failure containing '%s', got ok=%d error='%s'\n",
                needle, ok, check.error);
        assert(0);
    }
    printf("rejected: %s\n", check.error);
}

int main(void)
{
    configstrings_t bank = {0}, inherited = {0};
    gamestate_check_t check;
    ol_msg_t m;
    char *big;
    size_t size;
    unsigned i;
    const char *rows[] = { "1315", "body_mp_default", "4275", "hud_icon", "100", "constant" };

    /* 1. A valid gamestate written by the real writer passes, with counts. */
    assert(ImportConstantConfigstrings(&inherited, 3, rows));
    assert(OL_SetConfigstring(&bank, 0, "\\g_gametype\\war\\mapname\\mp_killhouse_sh\\2485\\12"));
    assert(OL_SetConfigstring(&bank, 2, "cod"));
    assert(OL_SetConfigstring(&bank, 3, "42"));
    assert(OL_SetConfigstring(&bank, 100, ""));
    assert(OL_SetConfigstring(&bank, OL_MAX_CONFIGSTRINGS - 1, "last"));
    big = malloc(CONFIGSTRING_VALUE_CHARS + 2);
    memset(big, 'a', CONFIGSTRING_VALUE_CHARS);
    big[CONFIGSTRING_VALUE_CHARS] = 0; /* exactly at the limit */
    assert(OL_SetConfigstring(&bank, 680, big));
    MsgWriteInit(&m, buffer, sizeof(buffer));
    assert(WriteGamestate(&m, &bank, "mp_killhouse_sh", "mp", 0xad1e1713u, 9, 3, 77));
    size = m.size;
    assert(CheckGamestate(buffer, size, &inherited, 12, size + 40, LIMIT, &check));
    assert(check.configstrings == 6 && check.last_index == OL_MAX_CONFIGSTRINGS - 1);
    assert(check.client_number == 3 && check.longest_index == 680);
    assert(check.longest == CONFIGSTRING_VALUE_CHARS);
    assert(check.gamestate_bytes == size && check.message_bytes == size + 40);
    /* pool: reserved byte + inherited 1315/4275 + sent nonempty values; the
     * sent "" at 100 clears the inherited "constant". */
    assert(check.pool_bytes == 1 + 16 + 9 + strlen(bank.values[0]) + 1 + 4 + 3 + 5 +
                               CONFIGSTRING_VALUE_CHARS + 1);
    printf("valid: %u configstrings, pool %u, %u bytes\n", check.configstrings,
           (unsigned)check.pool_bytes, (unsigned)check.gamestate_bytes);
    /* An empty block is valid too. */
    { entry_t none[1]; size = raw_gamestate(none, 0, 0, 0); }
    assert(CheckGamestate(buffer, size, NULL, 12, size, LIMIT, &check));

    /* 2. An oversized string. */
    memset(big, 'a', CONFIGSTRING_VALUE_CHARS + 1);
    big[CONFIGSTRING_VALUE_CHARS + 1] = 0;
    assert(OL_SetConfigstring(&bank, 680, big));
    MsgWriteInit(&m, buffer, sizeof(buffer));
    assert(WriteGamestate(&m, &bank, "mp_killhouse_sh", "mp", 0xad1e1713u, 9, 3, 77));
    expect_fail(buffer, m.size, &inherited, m.size, "configstring 680 is 1016 chars");

    /* 3. An out-of-range configstring index. */
    {
        entry_t e[] = { { 3, "42" }, { OL_MAX_CONFIGSTRINGS, "x" } };
        size = raw_gamestate(e, 2, 2, 0);
        expect_fail(buffer, size, NULL, size, "configstring index 6519 is outside");
    }
    /* 4. An out-of-range entity (the recipient's client number). */
    {
        entry_t e[] = { { 3, "42" } };
        size = raw_gamestate(e, 1, 1, 12);
        expect_fail(buffer, size, NULL, size, "client number 12 is outside 0..11");
        size = raw_gamestate(e, 1, 1, 11);
        assert(CheckGamestate(buffer, size, NULL, 12, size, LIMIT, &check));
    }
    /* 5. An oversized total message. */
    {
        entry_t e[] = { { 3, "42" } };
        size = raw_gamestate(e, 1, 1, 0);
        expect_fail(buffer, size, NULL, LIMIT + 1, "message is 131073 bytes; the limit is 131072");
        assert(CheckGamestate(buffer, size, NULL, 12, LIMIT, LIMIT, &check));
    }
    /* 6. Duplicate and out-of-order entries. */
    {
        entry_t dup[] = { { 3, "42" }, { 3, "43" } }, order[] = { { 9, "a" }, { 4, "b" } };
        size = raw_gamestate(dup, 2, 2, 0);
        expect_fail(buffer, size, NULL, size, "configstring 3 is sent twice");
        size = raw_gamestate(order, 2, 2, 0);
        expect_fail(buffer, size, NULL, size, "configstring 4 follows 9");
    }
    /* 7. An unterminated value: the section ends inside the last string. */
    {
        ol_msg_t t;
        MsgWriteInit(&t, buffer, sizeof(buffer));
        WriteByte(&t, 2); WriteLong(&t, 7);
        OL_WriteString(&t, "mp_killhouse_sh"); OL_WriteString(&t, "mp");
        WriteLong(&t, 0xad1e1713u);
        t.bit = 0;
        WriteByte(&t, 3); WriteShort(&t, 1);
        WriteBits(&t, 0, 1); WriteBits(&t, 5, 13);
        OL_WriteData(&t, "abc", 3);
        expect_fail(buffer, t.size, NULL, t.size, "configstring 5 is unterminated");
    }
    /* 8. A declared count larger than the entries, and trailing bytes. */
    {
        entry_t e[] = { { 3, "42" } };
        size = raw_gamestate(e, 1, 2, 0);
        expect_fail(buffer, size, NULL, size, ""); /* any named failure */
        size = raw_gamestate(e, 1, 1, 0);
        buffer[size] = 0;
        expect_fail(buffer, size + 1, NULL, size + 1, "1 stray bytes");
    }
    /* 9. A pool over C1's 128 KiB once the inherited bank is counted. */
    {
        configstrings_t many = {0}, sent = {0};
        memset(big, 'b', 1000); big[1000] = 0;
        for (i = 0; i < 130; ++i) assert(OL_SetConfigstring(&many, 2000 + i, big));
        assert(OL_SetConfigstring(&sent, 1, big));
        assert(OL_SetConfigstring(&sent, 2, big));
        MsgWriteInit(&m, buffer, sizeof(buffer));
        assert(WriteGamestate(&m, &sent, "mp_killhouse_sh", "mp", 0xad1e1713u, 9, 0, 77));
        expect_fail(buffer, m.size, &many, m.size, "configstring pool is 132133 bytes");
        /* The same values pass without the inherited bank. */
        assert(CheckGamestate(buffer, m.size, NULL, 12, m.size, LIMIT, &check));
        ClearConfigstrings(&many); ClearConfigstrings(&sent);
    }
    /* 10. Every category the writer accepts passes; another zone type fails. */
    {
        static const char *zones[] = { "mp", "coop", "zm", "survival" };
        configstrings_t one = {0};
        uint32_t crc;
        assert(OL_SetConfigstring(&one, 3, "42"));
        for (i = 0; i < sizeof(zones) / sizeof(zones[0]); ++i) {
            if (!ConstConfigChecksum(zones[i], &crc)) continue;
            MsgWriteInit(&m, buffer, sizeof(buffer));
            assert(WriteGamestate(&m, &one, "mp_fav_tropical", zones[i], crc, 9, 0, 77));
            assert(CheckGamestate(buffer, m.size, NULL, 12, m.size, LIMIT, &check));
            printf("valid zone type: %s\n", zones[i]);
        }
        MsgWriteInit(&m, buffer, sizeof(buffer));
        WriteByte(&m, 2); WriteLong(&m, 7);
        OL_WriteString(&m, "mp_fav_tropical"); OL_WriteString(&m, "sp");
        WriteLong(&m, 0);
        expect_fail(buffer, m.size, NULL, m.size, "zone type");
        ClearConfigstrings(&one);
    }
    /* 11. A broken header. */
    buffer[0] = 7;
    expect_fail(buffer, 16, NULL, 16, "svc_gamestate");

    free(big);
    ClearConfigstrings(&bank);
    ClearConfigstrings(&inherited);
    puts("test_gamestate_check: ok");
    return 0;
}
