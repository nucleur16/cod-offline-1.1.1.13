/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Story intel (C1 "collectibles") and the objective info strings that list it.
 *
 * WHAT C1 SHIPS (all MEASURED in the 1.1.1.13 fastfiles and C1.exe):
 *
 *  - zm_prototype_sh and zm_asylum_sh each place twelve trigger_use entities
 *    with targetname "intel_collectible", a "script_index" and a "target"
 *    naming the script_model that is the item (notebook_zm_sh,
 *    envolope_zm_sh, scribbled_papers_zm_sh, photo_frame_zm_sh). script_index
 *    is 0..11 on prototype and 12..23 on asylum. spawnflags 1536/1280/768 are
 *    SP's NOT_NORMAL|NOT_HARD, NOT_EASY|NOT_HARD and NOT_EASY|NOT_NORMAL, so
 *    each map carries four items per difficulty.
 *
 *  - mp/coCollectibles.csv (code_post_gfx_mp, 156 x 3: name, description,
 *    icon) is indexed by that script_index: row 0 ZOMBIE_STORY_INTEL_PROTO_E1
 *    .. row 11 PROTO_H4, row 12 ASYLUM_E1 .. row 23 ASYLUM_H4, then the coop
 *    COOP_STORY_INTEL_* rows.
 *
 *  - mp/playerdata.def (the client's public "mpdata" record) declares
 *    collectibles[1024] of bool. 0x463b20 reads collectibles[intel - 1] from
 *    the LOCAL client's live public record ([0x6c267dc]+0x4c, or 0x722d06c).
 *
 *  - The scoreboard (CG_DrawScoreboard, 0x4602d6 -> 0x463c60, drawn under the
 *    player list) walks all 32 objective info configstrings (CS 482 + i,
 *    infostrings), sorts them by "sort" (else by slot), and for an entry
 *    whose "intel" key is > 0 shows coCollectibles.csv row intel-1 (name,
 *    desc, icon) when collectibles[intel-1] is set, else MPUI_FIND_INTEL with
 *    hud_intel_unknown. C1's server writes those keys with the setters at
 *    0x56a7b0 ("intel" = argument, %d) and 0x56a6b0 ("sort" = argument + 32),
 *    and clears a slot with 0x56a530.
 *
 *  - C1's 'J' server command (0x469aba -> 0x468bd0 with secure=0) writes
 *    bytes straight into that same live public record. That is how a pickup
 *    reaches the scoreboard mid-match; the lobby keeps it (match report
 *    progression.collectibles) for the next login.
 *
 * The builtin names are this server's: C1 stripped its builtin names
 * (tools/c1_builtin_tables.py), so only the behaviour is C1's.
 */
#include <stdio.h>
#include <string.h>
#include "script_collectibles.h"

void ProfilePushByteToken(uint8_t value, char out[3])
{
    static const char hex[] = "0123456789ABCDEF";
    if (value == 0) { out[0] = 'Z'; out[1] = 0; return; }
    if (value == 1) { out[0] = 'O'; out[1] = 0; return; }
    out[0] = hex[value >> 4];
    out[1] = hex[value & 15];
    out[2] = 0;
}

size_t FormatPublicProfilePush(char *out, size_t size, unsigned offset, uint8_t value)
{
    char token[3];
    int n;
    /* The handler refuses offsets at or past the 0x7ffc-byte record. */
    if (!out || !size || offset >= 0x7ffcu) return 0;
    ProfilePushByteToken(value, token);
    n = snprintf(out, size, "J %u %s", offset, token);
    if (n < 0 || (size_t)n >= size) return 0;
    return (size_t)n;
}

#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "server.h"
#include "g_shared.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include "cvar.h"
#include "zlib/zlib.h"
#include "sv_state.h"
#include "script_profile_vm.h"
#include "ol_profile.h"
#include "profile_updates.h"
#include "ol_stats.h"
#include "match_report.h"

static cvar_t *push_public_profile;

/* ------------------------------------------------------------------------- */
/* Objective info configstrings                                              */
/* ------------------------------------------------------------------------- */

/* C1's setters all read parameter 0 the same way and print this text. */
static int objective_info_index(void)
{
    int index = Scr_GetInt(0);
    if (index < 0 || index >= OBJECTIVE_INFO_SLOTS) {
        Scr_ParamError(0, va("index %i is an illegal objective index. Valid indexes are 0 to %i\n",
                             index, OBJECTIVE_INFO_SLOTS - 1));
        return -1;
    }
    return index;
}

/* Replace (key != NULL) one key of slot `index`'s infostring, or clear the
 * whole string (key == NULL), and deliver the result as C1's own 'd' update,
 * the way setslowmotion publishes CS 6132. A late joiner gets it from the
 * gamestate, which carries every override. */
static void objective_info_publish(int index, const char *key, const char *value)
{
    configstrings_t *bank = ServerConfigstrings();
    unsigned cs = CS_OBJECTIVE_INFO + (unsigned)index;
    const char *old = GetEffectiveConfigstring(bank, ServerInheritedConfigstrings(), cs);
    char info[MAX_INFO_STRING];
    int i;
    if (key) {
        Q_strncpyz(info, old ? old : "", sizeof(info));
        Info_SetValueForKey(info, key, value);
    } else
        info[0] = 0;
    if (old && !strcmp(old, info)) return;
    if (!OL_SetConfigstring(bank, cs, info)) {
        Scr_Error(va("Could not set objective info configstring %u", cs));
        return;
    }
    if (!sv_maxclients) return;
    for (i = 0; i < sv_maxclients->integer; ++i)
        if (svs.clients[i].state >= CS_PRIMED && svs.clients[i].gamestateSent)
            SV_SendServerCommandNoLoss(&svs.clients[i], "d %u %s", cs, info);
}

void Scr_ObjectiveSetIntel(void)
{
    char value[16];
    int index = objective_info_index();
    if (index < 0) return;
    Com_sprintf(value, sizeof(value), "%d", Scr_GetInt(1));
    objective_info_publish(index, "intel", value);
}

void Scr_ObjectiveSetSort(void)
{
    char value[16];
    int index = objective_info_index();
    if (index < 0) return;
    /* 0x56a72a: lea edi, [eax + 0x20]. The scoreboard falls back to the slot
     * number for a sort <= 0, so the offset keeps script sorts above it. */
    Com_sprintf(value, sizeof(value), "%d", Scr_GetInt(1) + 32);
    objective_info_publish(index, "sort", value);
}

void Scr_ObjectiveClearInfo(void)
{
    int index = objective_info_index();
    if (index < 0) return;
    objective_info_publish(index, NULL, NULL);
}

/* ------------------------------------------------------------------------- */
/* collectibles[] in the public profile                                      */
/* ------------------------------------------------------------------------- */

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* collectibles[index] in definition d: a bool, whose offset-map entry is a
 * record-relative BIT address (structured_data.h). */
static int collectible_field(const structured_definition_t *d, int index,
                             structured_field_t *field, uint32_t *bit)
{
    structured_path_t path[2];
    uint32_t address;
    if (!d || index < 0) return 0;
    path[0].name = "collectibles"; path[0].index = 0;
    path[1].name = NULL; path[1].index = index;
    if (!ResolveStructuredField(d, path, 2, field) || field->type.kind != 2) return 0;
    if (!d->layout.offset_map || field->logical_index >= d->layout.offset_map_size / 4) return 0;
    address = le32(d->layout.offset_map + (size_t)field->logical_index * 4);
    if ((address >> 3) >= d->layout.record_size || (address >> 3) >= 0x7ffcu) return 0;
    if (bit) *bit = address;
    return 1;
}

/* Is `index` a collectibles[] element in any mounted playerdata definition?
 * Used when the player has no upload to pick a definition from. */
static int collectible_index_valid(int index)
{
    const structured_schema_t *schema = ScriptProfileSchema(PROFILE_PUBLIC);
    structured_field_t field;
    uint32_t i;
    if (!schema || !schema->definitions) return 0;
    for (i = 0; i < schema->count; ++i)
        if (collectible_field(schema->definitions + i, index, &field, NULL)) return 1;
    return 0;
}

/* -1 no public profile upload to read, else the stored bit. */
static int public_collected(unsigned client, int index)
{
    const structured_definition_t *d;
    const uint8_t *record;
    size_t capacity;
    structured_field_t field;
    uint32_t value = 0;
    if (!ScriptProfileView(client, PROFILE_PUBLIC, &d, &record, &capacity)) return -1;
    if (!collectible_field(d, index, &field, NULL) ||
        !StructuredReadScalar(&d->layout, 2, field.logical_index, record, capacity, &value))
        return -1;
    return value ? 1 : 0;
}

static unsigned collectible_player(scr_entref_t ref, const char *method)
{
    if (ref.classnum || ref.entnum >= MAX_CLIENTS || !g_entities[ref.entnum].client ||
        !g_entities[ref.entnum].r.inuse) {
        Scr_Error(va("%s: entity must be a player entity", method));
        return MAX_CLIENTS;
    }
    return ref.entnum;
}

/* Set collectibles[index] in the session copy of the upload, exactly as
 * setPlayerData does (dirty tracking, CRC reseal), then push the byte that
 * holds the bit to the client with 'J'. -1 no upload, 0 already set, 1 set. */
static int public_collect(unsigned client, int index, int *pushed)
{
    const structured_schema_t *schema = ScriptProfileSchema(PROFILE_PUBLIC);
    const structured_definition_t *d;
    const uint8_t *record;
    size_t capacity;
    structured_field_t field;
    uint32_t bit, value = 0, crc;
    uint8_t *blob;
    unsigned i;
    int marking;
    char command[64];
    *pushed = 0;
    if (!schema || !ClientStats(client)) return -1;
    blob = ClientStatsWritable(client);
    if (!blob || !ProfileRecord(blob, STATS_SIZE, schema, &d, &record, &capacity) ||
        !collectible_field(d, index, &field, &bit) ||
        !StructuredReadScalar(&d->layout, 2, field.logical_index, record, capacity, &value))
        return -1;
    if (value) return 0;
    marking = BeginProfileUpdate(client);
    if (!StructuredWriteScalar(&d->layout, 2, field.logical_index, blob + 4, capacity, 1))
        return -1;
    if (marking) MarkProfileField(client, &d->layout, &field, 0);
    crc = (uint32_t)crc32(0, blob + 4, STATS_SIZE - 4);
    for (i = 0; i < 4; ++i) blob[i] = (uint8_t)(crc >> (8 * i));

    if (!push_public_profile)
        push_public_profile = Cvar_RegisterBool("pushPublicProfile", qtrue, CVAR_ARCHIVE,
            "Send a picked-up collectible to the C1 client's live playerdata with its 'J' command");
    if (push_public_profile->boolean && svs.clients[client].state == CS_ACTIVE &&
        svs.clients[client].netchan.remoteAddress.type != NA_BOT &&
        FormatPublicProfilePush(command, sizeof(command), bit >> 3, blob[4 + (bit >> 3)])) {
        SV_GameSendServerCommand((int)client, 1, command);
        *pushed = 1;
    }
    return 1;
}

void Scr_HasCollectible(scr_entref_t ref)
{
    unsigned client = collectible_player(ref, "codol_hasCollectible");
    int index, stored;
    if (client >= MAX_CLIENTS) return;
    index = Scr_GetInt(0);
    if (!collectible_index_valid(index)) {
        Scr_ParamError(0, va("codol_hasCollectible: %i is not a collectibles index", index));
        return;
    }
    stored = public_collected(client, index);
    Scr_AddInt(stored > 0 || MatchReportHasCollectible(client, (unsigned)index) ? 1 : 0);
}

void Scr_Collect(scr_entref_t ref)
{
    unsigned client = collectible_player(ref, "codol_collect");
    int index, stored, reported, pushed = 0, had;
    if (client >= MAX_CLIENTS) return;
    index = Scr_GetInt(0);
    if (!collectible_index_valid(index)) {
        Scr_ParamError(0, va("codol_collect: %i is not a collectibles index", index));
        return;
    }
    had = public_collected(client, index) > 0 || MatchReportHasCollectible(client, (unsigned)index);
    stored = public_collect(client, index, &pushed);
    reported = MatchReportCollectible(client, (unsigned)index);
    ((void)0);
    Scr_AddInt(had ? 0 : 1);
}
#endif
