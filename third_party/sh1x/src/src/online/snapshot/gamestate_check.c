/* Sanity check of a serialized CoDOL gamestate; see gamestate_check.h.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "gamestate_check.h"
#include "ol_msg.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int fail(gamestate_check_t *out, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(out->error, sizeof(out->error), format, args);
    va_end(args);
    return 0;
}

/* A printable excerpt of a value for the error line. */
static void excerpt(char out[44], const uint8_t *text, size_t length)
{
    size_t i, n = length < 40 ? length : 40;
    for (i = 0; i < n; ++i)
        out[i] = text[i] >= 32 && text[i] < 127 && text[i] != '"' ? (char)text[i] : '?';
    if (length > n) { out[n++] = '.'; out[n++] = '.'; out[n++] = '.'; }
    out[n] = 0;
}

/* One byte-aligned NUL-terminated string at the cursor. Returns its length,
 * or -1 (unterminated within the section) / -2 (longer than limit). */
static long read_string(ol_msg_t *m, size_t limit, const uint8_t **text)
{
    const uint8_t *start, *end;
    size_t remaining;
    if (m->failed || m->cursor > m->size) return -1;
    start = m->data + m->cursor;
    remaining = m->size - m->cursor;
    end = memchr(start, 0, remaining);
    *text = start;
    if (!end) return -1;
    if ((size_t)(end - start) > limit) return -2;
    m->cursor += (size_t)(end - start) + 1;
    return (long)(end - start);
}

static size_t pool_cost(const char *value)
{
    return value && *value ? strlen(value) + 1 : 0;
}

int CheckGamestate(const uint8_t *data, size_t size,
                         const configstrings_t *inherited, unsigned max_clients,
                         size_t message_bytes, size_t message_limit,
                         gamestate_check_t *out)
{
    ol_msg_t m;
    const uint8_t *text;
    char shown[44], zone[16];
    uint32_t crc;
    unsigned count, i, previous = 0;
    long length;
    int have_previous = 0;
    size_t pool = 1;

    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    out->gamestate_bytes = size;
    out->message_bytes = message_bytes;
    out->message_limit = message_limit;
    if (!data || !size) return fail(out, "gamestate is empty");
    if (message_bytes < size)
        return fail(out, "message size %u is smaller than its %u-byte gamestate",
                    (unsigned)message_bytes, (unsigned)size);
    if (message_bytes > message_limit)
        return fail(out, "message is %u bytes; the limit is %u (send buffer and C1's fragment capacity)",
                    (unsigned)message_bytes, (unsigned)message_limit);
    if (!max_clients || max_clients > MAX_GAME_CLIENTS)
        return fail(out, "max clients %u is outside C1's 1..%u", max_clients, MAX_GAME_CLIENTS);

    MsgReadInit(&m, data, size);
    if (ReadByte(&m) != 2) return fail(out, "gamestate does not start with svc_gamestate (2)");
    ReadLong(&m); /* server command sequence */
    length = read_string(&m, GAMESTATE_MAP_CHARS, &text);
    if (length == -1) return fail(out, "map name is unterminated");
    if (length == -2) return fail(out, "map name is longer than %u chars", GAMESTATE_MAP_CHARS);
    if (!length) return fail(out, "map name is empty");
    /* The categories the writer itself accepts: those with a constant table. */
    length = read_string(&m, sizeof(zone) - 1, &text);
    if (length >= 0) memcpy(zone, text, (size_t)length + 1);
    if (length < 0 || !ConstConfigChecksum(zone, &crc))
        return fail(out, "zone type is not a category with a constant configstring table");
    ReadLong(&m); /* constant table checksum */
    if (m.failed) return fail(out, "gamestate header is truncated");

    if (inherited)
        for (i = 0; i < OL_MAX_CONFIGSTRINGS; ++i) pool += pool_cost(inherited->values[i]);

    /* The configstring block is its own bit substream (WriteConfigstrings). */
    m.bit = 0;
    if (ReadByte(&m) != 3) return fail(out, "configstring block marker (3) is missing");
    count = ReadShort(&m);
    if (m.failed) return fail(out, "configstring block is truncated");
    if (count > OL_MAX_CONFIGSTRINGS)
        return fail(out, "configstring count %u exceeds C1's %u", count, OL_MAX_CONFIGSTRINGS);
    for (i = 0; i < count; ++i) {
        unsigned relative = ReadBits(&m, 1), index = ReadBits(&m, 13);
        if (m.failed) return fail(out, "configstring entry %u of %u is truncated", i + 1, count);
        if (relative) return fail(out, "configstring entry %u uses a relative index", i + 1);
        if (index >= OL_MAX_CONFIGSTRINGS)
            return fail(out, "configstring index %u is outside C1's 0..%u", index,
                        OL_MAX_CONFIGSTRINGS - 1);
        if (have_previous && index == previous)
            return fail(out, "configstring %u is sent twice", index);
        if (have_previous && index < previous)
            return fail(out, "configstring %u follows %u (out of order)", index, previous);
        length = read_string(&m, CONFIGSTRING_VALUE_CHARS, &text);
        if (length == -1) return fail(out, "configstring %u is unterminated", index);
        if (length == -2) {
            const uint8_t *end = memchr(text, 0, m.size - (size_t)(text - m.data));
            size_t real = end ? (size_t)(end - text) : 0;
            excerpt(shown, text, real);
            return fail(out, "configstring %u is %u chars; C1's limit is %u (\"%s\")", index,
                        (unsigned)real, CONFIGSTRING_VALUE_CHARS, shown);
        }
        if (inherited) pool -= pool_cost(inherited->values[index]);
        if (length) pool += (size_t)length + 1;
        if ((size_t)length > out->longest || !out->configstrings) {
            out->longest = (size_t)length;
            out->longest_index = index;
        }
        previous = index; have_previous = 1;
        out->configstrings = i + 1;
        out->last_index = index;
    }
    if (ReadByte(&m) != 5 || m.failed)
        return fail(out, "configstring block terminator (5) is missing after %u entries", count);
    out->client_number = ReadLong(&m);
    ReadLong(&m); /* checksum feed */
    if (ReadByte(&m) != 5 || m.failed) return fail(out, "gamestate terminator (5) is missing");
    if (out->client_number >= max_clients)
        return fail(out, "client number %u is outside 0..%u", out->client_number, max_clients - 1);
    if (m.cursor != size)
        return fail(out, "%u stray bytes follow the gamestate", (unsigned)(size - m.cursor));
    /* Checked once the block is read: a later empty override can shrink it. */
    out->pool_bytes = pool;
    if (pool > CONFIGSTRING_POOL_BYTES)
        return fail(out, "configstring pool is %u bytes; C1's limit is %u (longest: configstring %u, %u chars)",
                    (unsigned)pool, CONFIGSTRING_POOL_BYTES, out->longest_index,
                    (unsigned)out->longest);
    return 1;
}
