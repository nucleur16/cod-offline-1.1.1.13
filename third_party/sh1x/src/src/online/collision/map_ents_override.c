/* Server-side map entity overrides: maps/mp/<map>.d3dbsp.ents in the script
 * roots (bin/home/codol_rawfiles, the same search the level scripts use).
 *
 * Some usermaps ported from CoDOL 3.3.3.3 ship objective prefabs the level
 * designer never placed: mp_mideast keeps its sd, sab and hq objectives at
 * z -816, 1200+ units under the level, mp_cha_quad parks every objective and
 * its mode spawns about 11000 units off the map.  Every CoDOL build that
 * ships the two maps (2.6.9.7, 3.3.3.3, 3.5.2.6) carries the same entities,
 * so there is no correct placement to recover.  The override is the level's
 * whole entity string with those entities placed in the playable area; the
 * zone stays untouched.  It replaces the entity string before anything parses
 * it -- the path node table, the bot graph's spawn and objective coverage
 * check, G_SpawnEntitiesFromString -- so every consumer sees one placement.
 *
 * The file is the entity string plus "//" comment lines.  It is used only when
 *   - MP or Survival rules use the base level (coop composes its own addon);
 *   - its "// base_crc32 xxxxxxxx" line names the CRC-32 of the entity string
 *     the mounted zone carries, so a re-ported zone never runs a stale copy;
 *   - every "?N" trigger reference names one of the zone's trigger models
 *     (those are the zone's hulls; a moved trigger keeps its model and takes
 *     a new origin -- hull bounds are relative to the entity origin);
 *   - it opens with the worldspawn entity and its braces balance.
 * Anything else is reported and the zone's own string is kept. */
#include "q_shared.h"
#include "qcommon_io.h"
#include "cvar.h"
#include "script_source.h"
#include "map_entity_policy.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char *ServerZoneType(void);

static uint32_t ents_crc32(const char *data, size_t size)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    int k;
    for (i = 0; i < size; ++i) {
        crc ^= (uint8_t)data[i];
        for (k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static const char *root_value(const char *name)
{
    cvar_t *var = Cvar_FindVar(name);
    return var && var->string ? var->string : "";
}

/* Copies `text` without its "//" comment lines (a line whose first non-blank
 * characters are "//").  Records the base_crc32 value when one is present. */
static char *strip_comments(const char *text, size_t length, size_t *out_length, int *has_crc, uint32_t *crc)
{
    char *out = malloc(length + 1);
    size_t i = 0, w = 0;
    if (!out) return NULL;
    while (i < length) {
        size_t end = i, s = i;
        while (end < length && text[end] != '\n') ++end;
        while (s < end && (text[s] == ' ' || text[s] == '\t' || text[s] == '\r')) ++s;
        if (s + 1 < end && text[s] == '/' && text[s + 1] == '/') {
            unsigned value;
            if (sscanf(text + s, "// base_crc32 %8x", &value) == 1) { *has_crc = 1; *crc = value; }
        } else {
            memcpy(out + w, text + i, end - i);
            w += end - i;
            if (end < length) out[w++] = '\n';
        }
        i = end < length ? end + 1 : end;
    }
    out[w] = 0;
    *out_length = w;
    return out;
}

/* Structural checks on the stripped body (see the header comment). */
static const char *check_body(const char *body, size_t length, uint32_t trigger_models)
{
    static const char model_key[] = "\"model\" \"?";
    const char *p = body, *first_close;
    int depth = 0, quoted = 0;
    size_t i;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != '{') return "does not open with an entity";
    first_close = strchr(p, '}');
    if (!first_close) return "has no complete entity";
    {
        const char *world = strstr(p, "\"classname\" \"worldspawn\"");
        if (!world || world > first_close) return "does not open with the worldspawn entity";
    }
    for (i = 0; i < length; ++i) {
        char c = body[i];
        if (c == '"') { quoted = !quoted; continue; }
        if (quoted) continue;
        if (c == '{') { if (depth++) return "nests an entity"; }
        else if (c == '}') { if (--depth < 0) return "closes an entity it never opened"; }
    }
    if (depth || quoted) return "ends inside an entity or a string";
    for (p = strstr(body, model_key); p; p = strstr(p + 1, model_key)) {
        const char *digits = p + sizeof(model_key) - 1;
        char *end;
        unsigned long index = strtoul(digits, &end, 10);
        if (end == digits || *end != '"') return "has a malformed \"?N\" trigger reference";
        if (index >= trigger_models) return "names a trigger model the zone does not have";
    }
    return NULL;
}

char *LoadMapEntsOverride(const char *map, const char *zone_text, size_t zone_text_size,
                                uint32_t trigger_models, size_t *out_size)
{
    char path[MAX_QPATH], *raw = NULL, *body;
    size_t raw_length = 0, body_length = 0;
    const char *zone = ServerZoneType(), *problem;
    uint32_t want = 0, have;
    int status, has_crc = 0;
    if (!map || !*map || !zone_text || !zone_text_size || !out_size) return NULL;
    if (!MapEntityUsesMpBase(zone)) return NULL;
    Com_sprintf(path, sizeof(path), "maps/mp/%s.d3dbsp.ents", map);
    status = ReadScriptSourceWithNativeOverlay(root_value("scriptPath"), root_value("scriptOverlay"),
        root_value("scriptNativeOverlay"), path, &raw, &raw_length);
    if (status == 0) return NULL;
    if (status < 0 || !raw) {
        free(raw);
        Com_PrintWarning(CON_CHANNEL_SERVER, "CoDOL map ents: %s is unreadable; keeping the zone's entities\n", path);
        return NULL;
    }
    body = strip_comments(raw, raw_length, &body_length, &has_crc, &want);
    free(raw);
    if (!body) return NULL;
    have = ents_crc32(zone_text, zone_text_size - 1);
    if (!has_crc || want != have) {
        Com_PrintWarning(CON_CHANNEL_SERVER,
            "CoDOL map ents: %s %s (zone entity string crc32 %08x); keeping the zone's entities\n",
            path, has_crc ? va("was made for base_crc32 %08x", want) : "has no base_crc32 line", have);
        free(body);
        return NULL;
    }
    problem = check_body(body, body_length, trigger_models);
    if (problem) {
        Com_PrintWarning(CON_CHANNEL_SERVER, "CoDOL map ents: %s %s; keeping the zone's entities\n", path, problem);
        free(body);
        return NULL;
    }
    Com_Printf(CON_CHANNEL_SERVER, "CoDOL map ents: %s replaces the zone's entity string (%u -> %u bytes)\n",
        path, (unsigned)(zone_text_size - 1), (unsigned)body_length);
    *out_size = body_length + 1;
    return body;
}
