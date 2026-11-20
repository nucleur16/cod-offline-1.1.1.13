#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#ifdef OL_SERVER
#include "persistent_events.h"
#endif
/* See match_report.h for the lobby contract this file implements.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "match_report.h"
#include "quickplay.h"
#include "zombies_mode.h"
#include "async_capture.h"
#include "sys_main.h"
#include "match_gp.h"
#include "localtest.h"
#include "ol_profile.h"
#include "script_profile_vm.h"
#include "ol_stats.h"
#include "scoreboard_stats.h"
#include "matchdata_server.h"
#include "assets.h"
#include "weapon_compose.h"
#include "scr_vm.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include "cvar.h"
#include "filesystem.h"
#include "server.h"
#include "g_shared.h"
#include "dev_cheats.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#define MATCH_REPORT_NAME "match_report.json"

/* The lifetime counters the client's career page reads out of its secure
 * profile. Every one is resolved BY NAME through the mounted
 * mp/playersecuredata.def, never by offset: a name this DDL does not declare is
 * skipped and named once in the console, so the list can be wrong without being
 * dangerous. The scripts are what maintain them - _persistence::statAdd /
 * statSet from _damage.gsc, _gamescore.gsc and _gamelogic.gsc write straight
 * into the session secure blob through setPlayerData - so this file only has to
 * measure, exactly as it already does for experience: value now minus value at
 * profile attach. Nothing here re-counts a kill.
 *
 * The names are this client's, taken from the decoded
 * mp/playersecuredata.def (v11, checksum 158043991, 6665 bytes, 1680 leaves,
 * from the PATCH zone - the base zone ships a different def, checksum
 * 1491960907). IW4 names CODOL does not declare - misses, timePlayedAllies,
 * timePlayedOpfor, timePlayedOther, restXPGoal and the round subtree - are deliberately NOT
 * in this list: setPlayerData routes them to the legacy session store that is
 * discarded at disconnect, so measuring them would always report nothing.
 *
 * Ratios are deliberately absent. `accuracy` is stored x10000 and `winRatio`
 * x10, the scripts recompute them with updatePersRatio, and the delta of a
 * ratio is meaningless; the lobby recomputes both from the lifetime totals.
 * `kdRatio` is not even read by the career page - CareerController divides
 * kills by deaths in ActionScript - so nothing needs to maintain it. */
static const char *const match_report_stats[] = {
    "timePlayedTotal", "timePlayed", "timePlayedOverAll",
    "score", "kills", "deaths", "assists", "headshots", "suicides",
    "totalShots", "hits",
    "wins", "losses", "ties", "escapes",
    "winStreak", "killStreak",
    "objectives", "bombPlants", "bombDefuses", "confirmed", "denied"
};
#define MATCH_REPORT_STATS     (sizeof(match_report_stats)/sizeof(match_report_stats[0]))
#define MATCH_REPORT_DEPARTED_SHARED 32
/* Each admitted native slot reserves one emergency departure owner. Reuse is
 * denied until that owner reaches a durable report; this is bounded storage
 * with admission backpressure, not a larger discard-on-full pool. */
#define MATCH_REPORT_DEPARTED (MATCH_REPORT_DEPARTED_SHARED + MAX_CLIENTS)
/* Challenges one player advanced in one match. CoDOL ships 611 of them, but a
 * single match moves a handful; 64 is far above anything observed and keeps
 * the row small enough to stay static on a 32-bit server. Overflow is
 * reported, never silently dropped. */
#define MATCH_REPORT_CHALLENGES 64
/* mp/playersecuredata.def declares challenges[1000]; the id is column 4 of
 * mp/allChallengesTable.csv and doubles as the storage address. */
#define MATCH_REPORT_CHALLENGE_IDS 1000
#define MATCH_REPORT_NAME_CHARS 36
/* Distinct weapons one player killed with in one match. Overflow is reported,
 * never silently dropped. Names are base weapon names (ak47_mp), which fit. */
#define MATCH_REPORT_WEAPONS 24
#define MATCH_REPORT_WEAPON_CHARS 40
/* Story intel one player picked up in one match. mp/playerdata.def declares
 * collectibles[1024] (bool) and C1's mp/coCollectibles.csv lists 156 rows;
 * a zombies map places 12, so 32 is ample. Overflow is reported. */
#define MATCH_REPORT_COLLECTIBLES 32
#define MATCH_REPORT_COLLECTIBLE_IDS 1024
/* A coop playlist name (coop_training_mp_killhouse_sh is 29 characters). */
#define MATCH_REPORT_COOP_MISSION_CHARS 64
#define COOP_RESULT_NONE 0
#define COOP_RESULT_COMPLETED 1
#define COOP_RESULT_FAILED 2
/* 64 live slots plus 32 frozen rows at roughly 230 bytes each, plus up to
 * ~1 KB of weapon XP per human. Static rather than automatic: the game loop is
 * single threaded and this is not a size to put on a 32-bit server stack. */
#define MATCH_REPORT_BYTES 65536

typedef struct {
    unsigned char used;
    unsigned char bot;
    unsigned char baseline_valid;
    unsigned char slot;          /* the client number this row played in */
    uint64_t uid;
    char name[MATCH_REPORT_NAME_CHARS];
    int team;
    int baseline_xp;
    /* What this row already owns independently of the engine slot: earlier
     * stays in this match, minus whatever a previous report already carried.
     * Counters may go negative here; the sum with the engine never does. */
    int carry_xp, carry_kills, carry_deaths, carry_assists, carry_score;
    /* Departed rows only: the settled counters, engine plus carry, taken at
     * the moment the slot stopped being this player's. */
    int kills, deaths, assists, score;
    /* Career counters, measured the same way as experience: the value at
     * profile attach, plus whatever an earlier stay in this match already
     * settled. stat_valid says the DDL actually declares that member. */
    int stat_base[MATCH_REPORT_STATS];
    int stat_carry[MATCH_REPORT_STATS];
    unsigned char stat_valid[MATCH_REPORT_STATS];
    /* Challenge progress this row earned, id -> delta, carried across a
     * reconnect exactly as stat_carry is. Only earlier stays live here; the
     * current stay is diffed at emit against challenge_base, which a written
     * report advances, so a second report in one match cannot re-count it. */
    unsigned short challenge_id[MATCH_REPORT_CHALLENGES];
    int challenge_carry[MATCH_REPORT_CHALLENGES];
    unsigned char challenges;
    unsigned char challenge_overflow;
    /* Weapon XP this row earned, base weapon name -> amount. Nothing in the
     * secure profile holds it - C1 reads a weapon's level only from the
     * lobby's inventory record (alpha0022 career/weapon_leveling.py) - so it is
     * accumulated here as _damage.gsc credits each kill, carried across a
     * reconnect, and zeroed once a report has carried it. */
    char weapon_name[MATCH_REPORT_WEAPONS][MATCH_REPORT_WEAPON_CHARS];
    int weapon_xp[MATCH_REPORT_WEAPONS];
    unsigned char weapons;
    unsigned char weapon_overflow;
    /* Collectible indices (playerdata collectibles[], mp/coCollectibles.csv
     * row) picked up this match. Recorded, not measured: the public profile
     * they are also written to is the client's own upload, which a re-upload
     * or a disconnect discards. Carried across a reconnect and zeroed once a
     * report has carried them, like the weapon XP above. */
    unsigned short collectible[MATCH_REPORT_COLLECTIBLES];
    unsigned char collectibles;
    unsigned char collectible_overflow;
    /* The coop mission this row finished, by its playlist name
     * (sv_sessionMap), and how: CODOL_COOP_RESULT_*. Recorded by
     * _coopmission.gsc at the mission's end through codol_reportCoopMission,
     * carried across a reconnect, zeroed once a report has carried it. The
     * lobby grants the next mission for a completion; a failure is reported
     * for the record only. */
    char coop_mission[MATCH_REPORT_COOP_MISSION_CHARS];
    unsigned char coop_result;
    /* Exact player outcome supplied by the Zombies gametype at game end. */
    unsigned char zm_reported, zm_extracted, zm_difficulty;
    unsigned zm_round, zm_round_goal;
    unsigned zm_time_ms;
    unsigned char zm_time_valid, zm_result_sent, zm_checkpoint_only;
    int zm_downs, zm_revives, zm_headshots;
    /* Earlier connection stays of this authenticated match owner. The live
     * GSC score fields restart at begin; final metadata is an absolute match
     * total, independent of reward/report deltas and result refresh count. */
    int zm_carry_downs, zm_carry_revives, zm_carry_headshots;
    /* GP (account currency 2) this row earned from FIXED awards, one total
     * per source (CODOL_GP_SOURCE_*): completed field orders whose
     * mp/cofieldorder*.csv reward column 4 is FO_GP, and completed challenge
     * tiers (mp/allChallengesTable.csv columns 24..32, C1's CP1..CP9), and
     * rank-ups (RANKUP_GP_PER_LEVEL x each new level). Fixed
     * means exactly the table amount: no activity gp_scale ever touches these
     * (user rule, 2026-10-13: awards are static, gameplay GP scales). Kept
     * apart from any scaled gameplay GP so a scale cannot leak in.
     * Accumulated like the weapon XP: no profile member holds it, the lobby
     * deposits the sum in the wallet. Carried across a reconnect, zeroed once
     * a report has carried it. */
    int gp_fixed[GP_SOURCES];
    /* GP from PLAYING (match_gp.h): the only GP that scales. It is a
     * function of the time this player spent in the match, so the row keeps
     * a timer: play_ms settled from earlier stays (a reconnect adopts it),
     * plus svs.time - play_since while play_running. A report that carried
     * the match GP sets match_gp_settled, which also stops a later departure
     * (intermission, map change) from paying it again. level is the displayed
     * rank level (rank index + 1) last read from the profile. gp_match is a
     * departed row's play GP, computed at the leaver outcome when it froze. */
    /* Independent connected-participation clock. The GP timer may settle on
     * an earlier report; this one continues until disconnect or terminal end.
     * Reported milliseconds partition stays without losing fractional seconds. */
    uint64_t participation_ms, participation_reported_ms;
    uint32_t participation_since;
    unsigned char participation_running;
    int play_ms, play_since;
    unsigned char play_running, match_gp_settled;
    unsigned char terminal_frozen, terminal_completed, terminal_outcome;
    int level;
    int gp_match;
    /* Rank-up GP (GP_SOURCE_RANKUP) is owed once per displayed level.
     * rankup_level is the highest level this row has been paid for, starting
     * at the level the profile already had at attach. A reconnect keeps the
     * larger of the earlier stay's mark and the re-attached profile's level,
     * so a stay that re-crosses a level from a stale profile pays nothing. It
     * survives a report: the report carries the GP, the levels stay paid. */
    int rankup_level;
} match_row_t;

static const char *const gp_source_names[GP_SOURCES] = {
    "field_orders", "challenges", "rankups"
};

static int row_fixed_gp(const match_row_t *row)
{
    int total = 0;
    unsigned s;
    for (s = 0; s < GP_SOURCES; ++s)
        if (row->gp_fixed[s] > 0)
            total = total > INT_MAX - row->gp_fixed[s] ? INT_MAX : total + row->gp_fixed[s];
    return total;
}

static match_row_t live[MAX_CLIENTS];
static match_row_t departed[MATCH_REPORT_DEPARTED];
static unsigned endless_round_checkpoint;
/* Main-thread ownership: a transferred report is not cleared until the OS
 * worker acknowledges file + directory durability. New earnings remain in the
 * rebased live rows. A failed transaction retains its exact name/body, so retry
 * cannot create a second grant identity or pick up a different map context. */
static struct {
    int active, replace, warned;
    unsigned char blocked_slots[MAX_CLIENTS];
    unsigned retry_at;
    async_capture_t job;
    size_t size;
    char path[AC_PATH_BYTES];
    char resident_scope[65]; /* frozen with immutable transaction, not re-read at ack */
    char bytes[MATCH_REPORT_BYTES];
} pending_report;
static char deferred_report_reason[64];
static int report_path_warned;


static int report_transition_held;

static int participation_match_finished;
static void sync_rankup(unsigned client);
/* Challenge progress a written report already carried, per live slot. Until the
 * first report of a stay this is unset and the attached profile is the
 * baseline; once a report is written, the session values it measured become
 * the baseline, so the next report (a map change after gameended, or a player
 * leaving afterwards) holds only what accrued since. The lobby folds every
 * spooled report, so re-reporting a stay would credit it twice. A negative
 * entry is an id that could not be read then and is skipped. */
static float challenge_base[MAX_CLIENTS][MATCH_REPORT_CHALLENGE_IDS];
static unsigned char challenge_base_valid[MAX_CLIENTS];

/* The launcher/master owns these rates. Freeze them for the first attached
 * participant so settings for a subsequent map cannot relabel this report. */
static int reward_context_valid;
static int reward_completed;
static int reward_playlist, reward_xp_rate = 100, reward_gp_rate = 100;
static void reward_context(void)
{
    cvar_t *playlist, *xp, *gp;
    if (reward_context_valid) return;
    playlist = Cvar_FindVar("sv_playlistId");
    xp = Cvar_FindVar("xpRatePercent");
    gp = Cvar_FindVar("gpRatePercent");
    reward_playlist = playlist ? playlist->integer : 0;
    reward_xp_rate = xp ? xp->integer : 100;
    reward_gp_rate = gp ? gp->integer : 100;
    reward_context_valid = 1;
}

static const char *report_directory(void)
{
    /* Never persist results from a testing binary, even when its menu is disabled. */
    if (LocalTestBuild()) return NULL;
    cvar_t *directory = Cvar_FindVar("profilePath");
    return (directory && directory->string && *directory->string) ? directory->string : NULL;
}

/* The XP the client's progression currently stands at, read back through the
 * same recovered C1 conversion _rank.gsc's getRankExperience uses, so a value
 * this file compares was produced by the identical rule that produced the one
 * the script saw. The blob is the session view: setRankExperience edits that,
 * never the attached bytes. */
static int client_total_xp(unsigned client, int *out)
{
    const structured_schema_t *schema;
    const engine_table_t *table;
    const uint8_t *blob;
    int rank, total;
    schema = ScriptProfileSchema(PROFILE_SECURE);
    blob = ScriptProfileBlob(client, PROFILE_SECURE);
    if (!schema || !blob || LoadEngineStringTable("mp/rankTable.csv", &table) != 1) return 0;
    if (!ProfileExperience(blob, STATS_SIZE, schema, table, &rank, &total)) return 0;
    *out = total;
    return 1;
}

/* NA_BOT is zero, and so is NA_BAD (sys_net.h:53-54), so an un-set-up netchan
 * reads as a bot. SV_DirectConnect does not call Netchan_Setup until
 * sv_client.c:569, well after it attaches the secure profile at :505, which
 * makes this answer worthless during a connect and correct only once the
 * client is really connected. Never latch it; ask when the netchan is real. */
/* One named counter out of the client's session secure profile, normalised to
 * an integer. Kinds 0/1/2/9 are int32/byte/bool/short and are the value itself.
 *
 * Kind 8 is float32, and it is NOT an exotic case to refuse: this DDL stores
 * `kills` and `wins` - the two headline numbers on the career page - as floats,
 * exactly as the GSC field for sess.kills is F_FLOAT. StructuredReadScalar
 * hands back the raw bits, so they are reinterpreted and rounded here. A
 * nonfinite or out-of-range float is refused rather than truncated into
 * nonsense. A string or aggregate member is not a counter and is refused. */
static int read_secure_stat(unsigned client, const char *name, int *out)
{
    const structured_definition_t *def;
    const uint8_t *record;
    size_t capacity;
    structured_path_t path;
    structured_field_t field;
    uint32_t bits = 0;
    path.name = name; path.index = 0;
    if (!ScriptProfileView(client, PROFILE_SECURE, &def, &record, &capacity)) return 0;
    if (!ResolveStructuredField(def, &path, 1, &field)) return 0;
    if (field.type.kind != 0 && field.type.kind != 1 && field.type.kind != 2 &&
        field.type.kind != 8 && field.type.kind != 9) return 0;
    if (!StructuredReadScalar(&def->layout, field.type.kind, field.logical_index,
                                    record, capacity, &bits)) return 0;
    if (field.type.kind == 8) {
        float value;
        memcpy(&value, &bits, sizeof(value));
        if (!(value >= 0.0f) || !(value <= 2147483000.0f)) return 0;  /* rejects NaN too */
        *out = (int)(value + 0.5f);
        return 1;
    }
    if (bits > (uint32_t)INT_MAX) return 0;
    *out = (int)bits;
    return 1;
}

/* CHALLENGE EGRESS.
 *
 * CoDOL keeps challenge progress as one cumulative float per challenge in the
 * secure profile's challenges[1000], indexed by the numeric id in column 4 of
 * mp/allChallengesTable.csv. The GSC accessors in _missions.gsc write exactly
 * that member, so a tier earned in this match is the difference between the
 * session blob the scripts edited and the pristine bytes the lobby attached.
 *
 * Diffing those two is the whole mechanism: the attached copy is immutable for
 * the life of the attachment (profile_clients.c keeps `data` and
 * `session` apart precisely so every reader sees edits without altering the
 * attached bytes), so no baseline of our own is needed and the measurement is
 * idempotent. Nothing here writes a profile - the lobby owns persistence, and
 * this file only reports.
 *
 * ROUNDING: the member is a float and the lobby accumulates integers, so a
 * fractional delta is reported only once it reaches a whole unit. Progress is
 * authored in whole events, so this is a guard, not a policy. */
static int challenge_view(const uint8_t *blob, const structured_definition_t **def,
                          const uint8_t **record, size_t *capacity)
{
    const structured_schema_t *schema = ScriptProfileSchema(PROFILE_SECURE);
    if (!schema || !blob) return 0;
    return ProfileRecord(blob, STATS_SIZE, schema, def, record, capacity);
}

static int challenge_value(const structured_definition_t *def, const uint8_t *record,
                           size_t capacity, unsigned id, double *out)
{
    /* Two path steps, as getPlayerData( "challenges", id ) builds them: the
     * member, then the index into it. A single step naming "challenges"
     * resolves to the array itself (the resolver never reads a named step's
     * index), so every id failed here and no report ever carried a challenge. */
    structured_path_t path[2];
    structured_field_t field;
    uint32_t bits = 0;
    float value;
    path[0].name = "challenges";
    path[0].index = 0;
    path[1].name = NULL;
    path[1].index = (int)id;
    if (!ResolveStructuredField(def, path, 2, &field)) return 0;
    if (field.type.kind != 8) return 0;
    if (!StructuredReadScalar(&def->layout, field.type.kind, field.logical_index,
                                    record, capacity, &bits)) return 0;
    memcpy(&value, &bits, sizeof(value));
    if (!(value >= 0.0f) || !(value <= 2147483000.0f)) return 0;   /* rejects NaN too */
    *out = (double)value;
    return 1;
}

/* Merge one id's delta into a row, adding to an id already present. */
static void challenge_merge(match_row_t *row, unsigned id, int delta)
{
    unsigned i;
    if (delta <= 0) return;
    for (i = 0; i < row->challenges; ++i) {
        if (row->challenge_id[i] == (unsigned short)id) {
            row->challenge_carry[i] += delta;
            return;
        }
    }
    if (row->challenges >= MATCH_REPORT_CHALLENGES) { row->challenge_overflow = 1; return; }
    row->challenge_id[row->challenges] = (unsigned short)id;
    row->challenge_carry[row->challenges] = delta;
    ++row->challenges;
}

/* This stay's deltas, session minus attached (or minus what the last written
 * report measured, see challenge_base). `sink` receives each one. Returns the
 * number reported, or -1 when the client has no attached profile (a bot, or a
 * slot whose profile never bound) - not the same as zero. */
static int challenge_stay_deltas(unsigned client, void *context,
                                 void (*sink)(void *, unsigned, int))
{
    const structured_definition_t *now_def, *was_def;
    const uint8_t *now_rec, *was_rec, *now_blob, *was_blob;
    size_t now_cap, was_cap;
    unsigned id;
    int found = 0;
    if (client >= MAX_CLIENTS) return -1;
    now_blob = ClientSecureProfile(client, NULL);
    was_blob = ClientSecureProfileAttached(client, NULL);
    if (!now_blob || !was_blob) return -1;
    if (now_blob == was_blob) return 0;      /* no script ever edited this profile */
    if (!challenge_view(now_blob, &now_def, &now_rec, &now_cap)) return -1;
    if (!challenge_view(was_blob, &was_def, &was_rec, &was_cap)) return -1;
    for (id = 0; id < MATCH_REPORT_CHALLENGE_IDS; ++id) {
        double now_value = 0.0, was_value = 0.0;
        int delta;
        if (!challenge_value(now_def, now_rec, now_cap, id, &now_value)) continue;
        if (challenge_base_valid[client]) {
            if (challenge_base[client][id] < 0.0f) continue;
            was_value = (double)challenge_base[client][id];
        } else if (!challenge_value(was_def, was_rec, was_cap, id, &was_value)) continue;
        if (now_value <= was_value) continue;
        delta = (int)(now_value - was_value);
        if (delta <= 0) continue;
        sink(context, id, delta);
        ++found;
    }
    return found;
}

static void challenge_into_row(void *context, unsigned id, int delta)
{
    challenge_merge((match_row_t *)context, id, delta);
}

static void challenge_count(void *context, unsigned id, int delta)
{
    (void)id;
    *(int *)context += delta;
}

/* Make the client's current challenge values the baseline of its next report. */
static void challenge_rebase(unsigned client)
{
    const structured_definition_t *def;
    const uint8_t *record, *blob;
    size_t capacity;
    unsigned id;
    if (client >= MAX_CLIENTS) return;
    blob = ClientSecureProfile(client, NULL);
    if (!blob || !challenge_view(blob, &def, &record, &capacity)) return;
    for (id = 0; id < MATCH_REPORT_CHALLENGE_IDS; ++id) {
        double value = 0.0;
        challenge_base[client][id] =
            challenge_value(def, record, capacity, id, &value) ? (float)value : -1.0f;
    }
    challenge_base_valid[client] = 1;
}

/* Names the DDL does not declare, once per server run, so a wrong entry in the
 * list above is visible instead of silently reporting nothing. */
static void report_missing_stat(unsigned index)
{
    static unsigned char announced[MATCH_REPORT_STATS];
    if (index >= MATCH_REPORT_STATS || announced[index]) return;
    announced[index] = 1;
    Com_PrintWarning(CON_CHANNEL_SERVER,
        "CoDOL match report: mp/playersecuredata.def declares no scalar '%s'; "
        "that career counter will not be reported\n", match_report_stats[index]);
}

static int client_is_bot(unsigned client)
{
    return svs.clients[client].netchan.remoteAddress.type == NA_BOT;
}

static int client_connected(unsigned client)
{
    if (!sv_maxclients || client >= (unsigned)sv_maxclients->integer) return 0;
    return svs.clients[client].state >= CS_CONNECTED;
}

/* The engine's own per-match counters. sess.kills is a float on this build
 * (the C1 script field is F_FLOAT); the report is an integer scoreboard, so it
 * is truncated exactly once, here. */
static void client_counters(unsigned client, int *kills, int *deaths, int *assists, int *score, int *team)
{
    const clientSession_t *sess;
    *kills = *deaths = *assists = *score = 0;
    *team = TEAM_FREE;
    if (!level.clients) return;
    sess = &level.clients[client].sess;
    *kills = (int)sess->kills;
    *deaths = sess->deaths;
    *assists = sess->assists;
    *score = sess->score;
    *team = sess->cs.team;
}

static match_row_t *find_departed(uint64_t uid)
{
    unsigned i;
    if (!uid) return NULL;
    for (i = 0; i < MATCH_REPORT_DEPARTED; ++i)
        if (departed[i].used && departed[i].uid == uid) return &departed[i];
    return NULL;
}

/* ------------------------------------------------------------------------- */
/* Play GP: the scaled part of a match's GP (match_gp.h)               */
/* ------------------------------------------------------------------------- */

/* The displayed rank level, rank index + 1, read through the same recovered
 * conversion client_total_xp uses. */
static int client_level(unsigned client, int *out)
{
    const structured_schema_t *schema;
    const engine_table_t *table;
    const uint8_t *blob;
    int rank, total;
    if (client >= MAX_CLIENTS) return 0;
    schema = ScriptProfileSchema(PROFILE_SECURE);
    blob = ScriptProfileBlob(client, PROFILE_SECURE);
    if (!schema || !blob || LoadEngineStringTable("mp/rankTable.csv", &table) != 1) return 0;
    if (!ProfileExperience(blob, STATS_SIZE, schema, table, &rank, &total)) return 0;
    *out = rank + 1;
    return 1;
}

static uint64_t row_participation_ms(const match_row_t *row)
{
    uint64_t ms = row->participation_ms;
    /* svs.time is the same monotonic server clock used by play GP. Unsigned
     * subtraction preserves elapsed milliseconds through its signed wrap. */
    if (row->participation_running)
        ms += (uint32_t)svs.time - row->participation_since;
    return ms;
}

static void freeze_participation(match_row_t *row)
{
    row->participation_ms = row_participation_ms(row);
    row->participation_running = 0;
}

static uint64_t participation_delta(const match_row_t *row, uint64_t now)
{
    return now > row->participation_reported_ms ? now - row->participation_reported_ms : 0;
}

static int row_play_ms(const match_row_t *row)
{
    int ms = row->play_ms > 0 ? row->play_ms : 0;
    if (row->play_running && svs.time > row->play_since) {
        int running = svs.time - row->play_since;
        ms = ms > INT_MAX - running ? INT_MAX : ms + running;
    }
    return ms;
}

static const char *cvar_text(const char *name)
{
    cvar_t *var = Cvar_FindVar(name);
    return var && var->string ? var->string : "";
}

static int cvar_number(const char *name, int fallback)
{
    cvar_t *var = Cvar_FindVar(name);
    return var && var->string && *var->string ? atoi(var->string) : fallback;
}

typedef struct {
    char mode[32], league_row[32];
    int league_index, activity_events;
    double league, activity;
} play_gp_detail_t;

/* Everything but the player: the mp/gametypescptable.csv row, the league
 * factor and the activity gp_scale.
 *
 * Row: "coop" / "zm" by sv_zoneType, else g_gametype when the table has it
 * (dm, sd, dom, ...), else "TEAM" for a team player and "NONTEAM" otherwise
 * (war, CoDOL's team deathmatch, is not listed and uses TEAM).
 *
 * League (mp/gpleaguescaler.csv): gpLeague names the row and
 * gpLeagueIndex the league column (0..4) when set. Otherwise: mp is the
 * lobby's ranked playlist, factor 1 as C1's MatchController.getGPLeagueFactor
 * gives a ranked playlist; coop uses row "sp" and zm row "zm", both at column
 * g_difficulty (the room difficulty, 0..3, _zombiemode.gsc). An empty cell is 1.
 *
 * Activity: <profilePath>/activities.info, the file the lobby serves C1
 * as the "activities.info" pubfile, read with C1's own rules at local time.
 * This server only runs playlist matches, so an event limited to "room"
 * matches never applies. */
static int play_gp_setting(int team, match_gp_params_t *params, play_gp_detail_t *d)
{
    const engine_table_t *cp = NULL, *league = NULL;
    const char *zone = cvar_text("sv_zoneType");
    const char *gametype = cvar_text("g_gametype");
    const char *row_override = cvar_text("gpLeague");
    const char *directory = report_directory();
    int found = 0;
    memset(d, 0, sizeof(*d));
    d->league = d->activity = 1.0;
    if (LoadEngineStringTable("mp/gametypescptable.csv", &cp) != 1 || !cp) return 0;
    /* Survival reports as the coop mode it was in C1's own tables (the
     * coop_survival_* playlists): gametypescptable.csv has no survival row. */
    if (!Q_stricmp(zone, "coop") || !Q_stricmp(zone, "zm") || !Q_stricmp(zone, "survival")) {
        Q_strncpyz(d->mode, !Q_stricmp(zone, "zm") ? "zm" : "coop", sizeof(d->mode));
        found = MatchGPParams(cp, d->mode, params);
    } else if (*gametype && MatchGPParams(cp, gametype, params)) {
        Q_strncpyz(d->mode, gametype, sizeof(d->mode));
        found = 1;
    }
    if (!found) {
        Q_strncpyz(d->mode, team == TEAM_RED || team == TEAM_BLUE ? "TEAM" : "NONTEAM",
                   sizeof(d->mode));
        found = MatchGPParams(cp, d->mode, params);
    }
    if (!found) return 0;

    if (*row_override) Q_strncpyz(d->league_row, row_override, sizeof(d->league_row));
    else if (!Q_stricmp(zone, "coop") || !Q_stricmp(zone, "survival")) Q_strncpyz(d->league_row, "sp", sizeof(d->league_row));
    else if (!Q_stricmp(zone, "zm")) Q_strncpyz(d->league_row, "zm", sizeof(d->league_row));
    else Q_strncpyz(d->league_row, "ranked", sizeof(d->league_row));
    d->league_index = cvar_number("gpLeagueIndex",
        Q_stricmp(d->league_row, "ranked") ?
        ZombiesDefaultLeague(zone, cvar_number("g_difficulty", 0)) : 0);
    if (LoadEngineStringTable("mp/gpleaguescaler.csv", &league) == 1 && league)
        d->league = MatchGPLeagueScale(league, d->league_row, d->league_index);

    if (directory) {
        char path[MAX_OSPATH];
        FILE *file;
        int n = snprintf(path, sizeof(path), "%s/activities.info", directory);
        if (n > 0 && (size_t)n < sizeof(path)) {
            FS_ReplaceSeparators(path);
            file = fopen(path, "rb");
            if (file) {
                static char text[65536];
                size_t length = fread(text, 1, sizeof(text) - 1, file);
                time_t now = time(NULL);
                struct tm *local = localtime(&now);
                fclose(file);
                text[length] = 0;
                if (local)
                    d->activity = ActivityGPScale(text, length, local, cvar_text("mapname"),
                                                        gametype, 1, &d->activity_events);
            }
        }
    }
    return 1;
}

static int match_play_gp(const match_row_t *row, int play_ms, int outcome,
                         play_gp_detail_t *detail)
{
    match_gp_params_t params;
    play_gp_detail_t local;
    play_gp_detail_t *d = detail ? detail : &local;
    if (!row || play_ms <= 0) return 0;
    if (!play_gp_setting(row->team, &params, d)) {
        static int announced;
        if (!announced)
            Com_PrintWarning(CON_CHANNEL_SERVER,
                "CoDOL match report: mp/gametypescptable.csv gives no matchreward row for "
                "this match; no GP is paid for playing\n");
        announced = 1;
        return 0;
    }
    return MatchGPAmount(&params, play_ms, outcome, row->level, d->league, d->activity);
}

/* How a player still in the match finished it. `finished` is 0 when the match
 * did not reach its end (then everyone is paid at the leaver outcome). Teams
 * compare team scores (TEAM_RED is bank 1, as outcome_for); a free-for-all
 * player wins with the strictly highest score of every connected client, ties
 * when sharing it; a spectator is paid as a loss. A coop row uses its mission
 * result: completed wins, otherwise a loss. */
static int live_outcome(unsigned client, const match_row_t *row, int finished)
{
    int team, mine, best = INT_MIN;
    unsigned i;
    if (!finished || !level.clients) return GP_OUTCOME_LOSS;
    if (!Q_stricmp(cvar_text("sv_zoneType"), "coop") || !Q_stricmp(cvar_text("sv_zoneType"), "survival"))
        return row->coop_result == COOP_RESULT_COMPLETED ? GP_OUTCOME_WIN
                                                               : GP_OUTCOME_LOSS;
    team = level.clients[client].sess.cs.team;
    if (team == TEAM_RED || team == TEAM_BLUE) {
        int red = level.teamScores[TEAM_RED], blue = level.teamScores[TEAM_BLUE];
        mine = team == TEAM_RED ? red : blue;
        best = team == TEAM_RED ? blue : red;
    } else if (team == TEAM_FREE) {
        mine = level.clients[client].sess.score;
        for (i = 0; i < MAX_CLIENTS; ++i) {
            if (i == client || !client_connected(i)) continue;
            if (level.clients[i].sess.score > best) best = level.clients[i].sess.score;
        }
        if (best == INT_MIN) return GP_OUTCOME_WIN;
    } else return GP_OUTCOME_LOSS;
    if (mine > best) return GP_OUTCOME_WIN;
    if (mine == best) return GP_OUTCOME_TIE;
    return GP_OUTCOME_LOSS;
}

static const char *outcome_name(int outcome)
{
    return outcome == GP_OUTCOME_WIN ? "win" :
           outcome == GP_OUTCOME_TIE ? "tie" : "loss";
}

/* A live row's play GP if the match ended now; 0 once a report carried it. */
static int live_play_gp(unsigned client, int finished, int *outcome_out, play_gp_detail_t *d)
{
    match_row_t row;
    int outcome;
    if (client >= MAX_CLIENTS || !live[client].used || live[client].match_gp_settled) return 0;
    row = live[client];
    if (level.clients) row.team = level.clients[client].sess.cs.team;
    client_level(client, &row.level);
    outcome = row.terminal_frozen ? row.terminal_outcome : live_outcome(client, &row, finished);
    if (outcome_out) *outcome_out = outcome;
    return match_play_gp(&row, row_play_ms(&row), outcome, d);
}

void MatchReportClientAttached(unsigned client, uint64_t uid)
{
    match_row_t row;
    match_row_t *earlier;
    int total = 0;
    unsigned index;
    if (client >= MAX_CLIENTS) return;
    reward_context();
    memset(&row, 0, sizeof(row));
    row.used = 1;
    row.slot = (unsigned char)client;
    row.uid = uid;
    /* A row exists only because a secure profile attached for a real account:
     * AutoBindClientSecureProfile refuses a zero uid and bots have none.
     * So every stored row is a human. Asking client_is_bot() here is the bug
     * that made a connecting player report as a bot and lose its whole match. */
    row.baseline_valid = (unsigned char)(client_total_xp(client, &total) ? 1 : 0);
    row.baseline_xp = row.baseline_valid ? total : 0;
    for (index = 0; index < MATCH_REPORT_STATS; ++index) {
        int value = 0;
        if (read_secure_stat(client, match_report_stats[index], &value)) {
            row.stat_base[index] = value;
            row.stat_valid[index] = 1;
        } else report_missing_stat(index);
    }
    /* A reconnect inside one match is the same player, not a second row. Adopt
     * what the earlier stay accumulated and retire that departed entry. */
    earlier = find_departed(uid);
    if (earlier) {
        row.carry_xp = earlier->carry_xp;
        row.carry_kills = earlier->kills;
        row.carry_deaths = earlier->deaths;
        row.carry_assists = earlier->assists;
        row.carry_score = earlier->score;
        for (index = 0; index < MATCH_REPORT_STATS; ++index)
            row.stat_carry[index] = earlier->stat_carry[index];
        for (index = 0; index < earlier->challenges; ++index) {
            row.challenge_id[index] = earlier->challenge_id[index];
            row.challenge_carry[index] = earlier->challenge_carry[index];
        }
        row.challenges = earlier->challenges;
        row.challenge_overflow = earlier->challenge_overflow;
        memcpy(row.weapon_name, earlier->weapon_name, sizeof(row.weapon_name));
        memcpy(row.weapon_xp, earlier->weapon_xp, sizeof(row.weapon_xp));
        row.weapons = earlier->weapons;
        row.weapon_overflow = earlier->weapon_overflow;
        memcpy(row.collectible, earlier->collectible, sizeof(row.collectible));
        row.collectibles = earlier->collectibles;
        row.collectible_overflow = earlier->collectible_overflow;
        memcpy(row.coop_mission, earlier->coop_mission, sizeof(row.coop_mission));
        row.coop_result = earlier->coop_result;
        memcpy(row.gp_fixed, earlier->gp_fixed, sizeof(row.gp_fixed));
        /* The earlier stay's minutes count toward this match's play GP; the
         * leaver amount it froze with is dropped, the end pays the whole. */
        row.participation_ms = earlier->participation_ms;
        row.participation_reported_ms = earlier->participation_reported_ms;
        row.play_ms = earlier->play_ms;
        row.match_gp_settled = earlier->match_gp_settled;
        row.rankup_level = earlier->rankup_level;
        row.terminal_frozen = earlier->terminal_frozen;
        row.terminal_completed = earlier->terminal_completed;
        row.terminal_outcome = earlier->terminal_outcome;
        row.zm_reported = earlier->zm_reported;
        row.zm_result_sent = earlier->zm_result_sent;
        row.zm_checkpoint_only = earlier->zm_checkpoint_only;
        row.zm_extracted = earlier->zm_extracted;
        row.zm_difficulty = earlier->zm_difficulty;
        row.zm_round = earlier->zm_round;
        row.zm_round_goal = earlier->zm_round_goal;
        row.zm_time_ms = earlier->zm_time_ms;
        row.zm_time_valid = earlier->zm_time_valid;
        row.zm_downs = earlier->zm_downs;
        row.zm_revives = earlier->zm_revives;
        row.zm_headshots = earlier->zm_headshots;
        row.zm_carry_downs = earlier->zm_carry_downs;
        row.zm_carry_revives = earlier->zm_carry_revives;
        row.zm_carry_headshots = earlier->zm_carry_headshots;
        memset(earlier, 0, sizeof(*earlier));
    }
    row.participation_since = (uint32_t)svs.time;
    row.participation_running = (unsigned char)!participation_match_finished;
    row.play_since = svs.time;
    row.play_running = (unsigned char)(!row.match_gp_settled && !row.terminal_frozen);
    if (!client_level(client, &row.level)) row.level = 0;
    if (row.level > row.rankup_level) row.rankup_level = row.level;
    live[client] = row;
    /* A new attachment is measured from its own bytes again. */
    challenge_base_valid[client] = 0;
    Com_Printf(CON_CHANNEL_SERVER,
        "CoDOL match report: client %u uid %llu baseline %d XP%s\n",
        client, (unsigned long long)uid, row.baseline_xp,
        row.baseline_valid ? "" : " (UNREADABLE - this player's XP cannot be reported)");
}

static int settle(int engine, int *carry)
{
    /* The carry is negative for everything an earlier report already took, so
     * the settled figure is what this row still owes the lobby. */
    int value = engine + *carry;
    *carry = 0;
    return value > 0 ? value : 0;
}

static void freeze(unsigned client, match_row_t *out)
{
    match_row_t row;
    int total = 0, team = 0;
    unsigned index;
    /* A level the profile reached by a path that never called
     * codol_creditRankUpGP is still owed before the row leaves. */
    sync_rankup(client);
    row = live[client];
    freeze_participation(&row);
    client_counters(client, &row.kills, &row.deaths, &row.assists, &row.score, &team);
    row.team = team;
    row.slot = (unsigned char)client;
    if (svs.clients[client].name[0])
        Q_strncpyz(row.name, svs.clients[client].name, sizeof(row.name));
    /* Fold the carry in now: once the slot is released the engine counters are
     * gone, and a frozen row that ignored the carry would re-report counters a
     * previous report already took. */
    row.kills = settle(row.kills, &row.carry_kills);
    row.deaths = settle(row.deaths, &row.carry_deaths);
    row.assists = settle(row.assists, &row.carry_assists);
    row.score = settle(row.score, &row.carry_score);
    if (row.baseline_valid && client_total_xp(client, &total) && total > row.baseline_xp)
        row.carry_xp += total - row.baseline_xp;
    for (index = 0; index < MATCH_REPORT_STATS; ++index) {
        int value = 0;
        if (!row.stat_valid[index]) continue;
        if (read_secure_stat(client, match_report_stats[index], &value) &&
            value > row.stat_base[index])
            row.stat_carry[index] += value - row.stat_base[index];
        row.stat_valid[index] = 0;   /* the slot's profile is about to go */
    }
    /* Settle the challenge deltas the same way: once the slot is released the
     * session blob is freed and the difference is unrecoverable. */
    challenge_stay_deltas(client, &row, challenge_into_row);
    row.baseline_valid = 0;
    /* Play GP for the minutes played, at the LEAVER outcome: loss scale. A
     * player who leaves has not finished the match; scale_loss (1 in every
     * MP row) is the base rate, so a leaver keeps the per-minute GP and only
     * forgoes the win/tie bonus. A reconnect discards this and is paid in
     * full at the end. */
    client_level(client, &row.level);
    row.play_ms = row_play_ms(&row);
    row.play_running = 0;
    row.gp_match = row.match_gp_settled ? 0 :
        match_play_gp(&row, row.play_ms,
            row.terminal_frozen ? row.terminal_outcome : GP_OUTCOME_LOSS, NULL);
    *out = row;
}

int MatchReportTransitionHeld(void) { return report_transition_held; }

int MatchReportCanReuseSlot(unsigned client)
{
    unsigned i;
    if (client >= MAX_CLIENTS || report_transition_held ||
        pending_report.blocked_slots[client] ||
        departed[MATCH_REPORT_DEPARTED_SHARED + client].used) return 0;
    if (!live[client].used) return 1;
    /* Do not freeze/kick an existing player merely because a reconnect arrives
     * while the shared pool is full: their emergency slot is for departure. */
    for (i=0;i<MATCH_REPORT_DEPARTED_SHARED;++i)
        if (!departed[i].used) return 1;
    return 0;
}

static int zombie_counter_total(int prior, int current)
{
    if (prior < 0) prior = 0;
    if (current < 0) current = 0;
    return current > INT_MAX - prior ? INT_MAX : prior + current;
}

void MatchReportClientDropped(unsigned client)
{
    unsigned i;
    if (client >= MAX_CLIENTS || !live[client].used) return;
    for (i=0;i<MATCH_REPORT_DEPARTED_SHARED;++i)
        if (!departed[i].used) break;
    if (i == MATCH_REPORT_DEPARTED_SHARED)
        i = MATCH_REPORT_DEPARTED_SHARED + client;
    /* Slot admission guarantees this reservation. Never clear a live owner
     * before its frozen destination exists. A violated invariant is fatal,
     * rather than silently overwriting somebody's rewards. */
    if (departed[i].used) {
        Com_Error(ERR_FATAL, "Match report emergency departure ownership violated");
        return;
    }
    /* These native session fields are cleared by ResetClientInput.
     * SV_DropClient must freeze the owner first. Transfer this stay once;
     * memset(live) below prevents a duplicate drop from adding it twice. */
    if (!Q_stricmp(cvar_text("sv_zoneType"), "zm")) {
        live[client].zm_carry_downs = zombie_counter_total(live[client].zm_carry_downs,
            GetScoreStat(client, SCORE_DOWNS));
        live[client].zm_carry_revives = zombie_counter_total(live[client].zm_carry_revives,
            GetScoreStat(client, SCORE_REVIVES));
        live[client].zm_carry_headshots = zombie_counter_total(live[client].zm_carry_headshots,
            GetScoreStat(client, SCORE_HEADSHOTS));
    }
    /* Snapshot the world-owned round before the secure profile and game
     * entity are detached. This is non-final progress: it must not consume
     * a later genuine defeat after reconnect or invent an extraction. */
    if (endless_round_checkpoint && !live[client].zm_reported && !live[client].zm_result_sent) {
        live[client].zm_checkpoint_only = 1;
        live[client].zm_difficulty = 3;
        live[client].zm_round = endless_round_checkpoint;
        live[client].zm_round_goal = 0;
        live[client].zm_downs = live[client].zm_revives = live[client].zm_headshots = -1;
    }
    freeze(client, &departed[i]);
    memset(&live[client], 0, sizeof(live[client]));
    challenge_base_valid[client] = 0;
}

int MatchReportHumansConnected(void)
{
    unsigned i;
    if (!sv_maxclients) return 0;
    for (i = 0; i < (unsigned)sv_maxclients->integer && i < MAX_CLIENTS; ++i) {
        if (!client_connected(i)) continue;
        /* An attached account is a human whatever its netchan currently says;
         * the netchan test only has to decide the slots that own no row. */
        if (live[i].used || !client_is_bot(i)) return 1;
    }
    return 0;
}

/* The weapon a kill is credited to, keyed the way the lobby keys weapon XP:
 * the base weapon name. A C1 loadout gun is a composed name
 * ("*a 32 5 14 4 ...", see weapon_compose.h) whose base index names the
 * gun the level belongs to; attachments and camo do not level separately.
 * Anything else - a grenade, a killstreak - is already its own name and is
 * passed through; the lobby ignores the ones its weapon table does not list.
 * 1 resolved, 0 nothing to credit. */
static int weapon_credit_key(const char *weapon, char *out, size_t size)
{
    composed_weapon_t composed;
    const char *base = NULL;
    size_t n;
    if (!weapon || !*weapon || !Q_stricmp(weapon, "none")) return 0;
    switch (ParseComposedWeaponName(weapon, &composed)) {
    case 1:
        if (ComposedWeaponBaseName(composed.base, &base) != 1 || !base || !*base)
            return 0;
        weapon = base;
        break;
    case 0:
        break;
    default:
        return 0;
    }
    Q_strncpyz(out, weapon, (int)size);
    for (n = 0; out[n]; ++n)
        if (out[n] >= 'A' && out[n] <= 'Z') out[n] = (char)(out[n] - 'A' + 'a');
    return 1;
}

int MatchReportCreditWeaponXP(unsigned client, const char *weapon, int amount)
{
    match_row_t *row;
    char key[MATCH_REPORT_WEAPON_CHARS];
    unsigned i;
    if (client >= MAX_CLIENTS || amount <= 0) return 0;
    row = &live[client];
    /* Only an attached account owns a row; bots and unauthenticated slots
     * have nowhere for weapon XP to go. */
    if (!row->used) return 0;
    if (!weapon_credit_key(weapon, key, sizeof(key))) return 0;
    for (i = 0; i < row->weapons; ++i) {
        if (strcmp(row->weapon_name[i], key)) continue;
        row->weapon_xp[i] = row->weapon_xp[i] > INT_MAX - amount ? INT_MAX
                                                                : row->weapon_xp[i] + amount;
        return 1;
    }
    if (row->weapons >= MATCH_REPORT_WEAPONS) {
        if (!row->weapon_overflow)
            Com_PrintWarning(CON_CHANNEL_SERVER,
                "CoDOL match report: client %u killed with more than %u weapons; %s earns no "
                "weapon XP this match\n", client, (unsigned)MATCH_REPORT_WEAPONS, key);
        row->weapon_overflow = 1;
        return 0;
    }
    Q_strncpyz(row->weapon_name[row->weapons], key, sizeof(row->weapon_name[0]));
    row->weapon_xp[row->weapons] = amount;
    ++row->weapons;
    return 1;
}

/* self codol_creditWeaponXP(<weapon name>, <amount>)
 *
 * _damage.gsc handleNormalDeath calls this once per enemy kill with the
 * killing weapon and the gametype's flat "kill" score value - deliberately not
 * the giveRankXP amount, which also carries headshot, last-stand, streak and
 * xpScaler bonuses. Returns 1 when the XP was recorded for the lobby. */
void Scr_CreditWeaponXP(scr_entref_t ref)
{
    if (ref.classnum || ref.entnum >= MAX_CLIENTS || !g_entities[ref.entnum].client ||
        !g_entities[ref.entnum].r.inuse) {
        Scr_Error("codol_creditWeaponXP: entity must be a player entity");
        return;
    }
    if (Scr_GetNumParam() != 2) {
        Scr_Error("codol_creditWeaponXP requires a weapon name and an amount");
        return;
    }
    Scr_AddInt(MatchReportCreditWeaponXP((unsigned)ref.entnum, Scr_GetString(0),
                                               Scr_GetInt(1)));
}

int MatchReportCollectible(unsigned client, unsigned index)
{
    match_row_t *row;
    unsigned i;
    if (client >= MAX_CLIENTS || index >= MATCH_REPORT_COLLECTIBLE_IDS) return 0;
    row = &live[client];
    if (!row->used) return 0;
    for (i = 0; i < row->collectibles; ++i)
        if (row->collectible[i] == (unsigned short)index) return 1;
    if (row->collectibles >= MATCH_REPORT_COLLECTIBLES) {
        if (!row->collectible_overflow)
            Com_PrintWarning(CON_CHANNEL_SERVER,
                "CoDOL match report: client %u collected more than %u items this match; "
                "collectible %u is not in the report\n", client,
                (unsigned)MATCH_REPORT_COLLECTIBLES, index);
        row->collectible_overflow = 1;
        return 0;
    }
    row->collectible[row->collectibles++] = (unsigned short)index;
    return 1;
}

int MatchReportCreditFixedGP(unsigned client, int source, int amount)
{
    match_row_t *row;
    int *bucket;
    if (client >= MAX_CLIENTS || amount <= 0) return 0;
    if (source < 0 || source >= GP_SOURCES) return 0;
    row = &live[client];
    if (!row->used) return 0;
    bucket = &row->gp_fixed[source];
    *bucket = *bucket > INT_MAX - amount ? INT_MAX : *bucket + amount;
    ((void)0);
    return 1;
}

int MatchReportCreditGP(unsigned client, int amount)
{
    return MatchReportCreditFixedGP(client, GP_SOURCE_FIELD_ORDER, amount);
}

static int clamp_level(int level)
{
    if (level < 1) return 1;
    if (level > RANKUP_MAX_LEVEL) return RANKUP_MAX_LEVEL;
    return level;
}

int RankUpGP(int from_level, int to_level)
{
    long long total = 0;
    int level;
    from_level = clamp_level(from_level);
    to_level = clamp_level(to_level);
    for (level = from_level + 1; level <= to_level; ++level)
        total += (long long)RANKUP_GP_PER_LEVEL * level;
    return total > INT_MAX ? INT_MAX : (int)total;
}

int MatchReportCreditRankUp(unsigned client, int old_level, int new_level)
{
    match_row_t *row;
    int from, amount;
    if (client >= MAX_CLIENTS) return 0;
    row = &live[client];
    if (!row->used) return 0;
    old_level = clamp_level(old_level);
    new_level = clamp_level(new_level);
    /* Pay only above both the caller's old level and this row's mark: the
     * mark stops a repeat, the old level stops an unreadable attach level
     * (mark 0) from paying the whole ladder. */
    from = old_level > row->rankup_level ? old_level : row->rankup_level;
    if (new_level <= from) return 0;
    amount = RankUpGP(from, new_level);
    if (!MatchReportCreditFixedGP(client, GP_SOURCE_RANKUP, amount)) return 0;
    ((void)0);
    row->rankup_level = new_level;
    return amount;
}

/* Catch-up from the profile itself: _rank.gsc credits an MP promotion the
 * moment it happens, but coop (maps/_rank.gsc) and zombies (setRankExperience)
 * move the same secure rank without calling codol_creditRankUpGP. Whatever
 * level the session profile now shows above this row's mark is paid here, at
 * the same mark, so the two routes cannot pay one level twice. A row whose
 * attach level was unreadable takes its first readable level as the mark and
 * pays nothing for it. */
static void sync_rankup(unsigned client)
{
    match_row_t *row;
    int now;
    if (client >= MAX_CLIENTS) return;
    row = &live[client];
    if (!row->used || !client_level(client, &now)) return;
    if (row->rankup_level <= 0) {
        row->rankup_level = now;
        return;
    }
    if (now > row->rankup_level)
        MatchReportCreditRankUp(client, row->rankup_level, now);
}

/* self codol_creditRankUpGP(<old level>, <new level>)
 *
 * _rank.gsc calls this when a giveRankXP award promotes the player, with the
 * displayed levels (rank index + 1) before and after. Every level crossed pays
 * RANKUP_GP_PER_LEVEL x that level, once per match row (see
 * MatchReportCreditRankUp). Returns the GP credited (0 for a bot, a
 * level already paid, or a non-promotion). */
void Scr_CreditRankUpGP(scr_entref_t ref)
{
    if (ref.classnum || ref.entnum >= MAX_CLIENTS || !g_entities[ref.entnum].client ||
        !g_entities[ref.entnum].r.inuse) {
        Scr_Error("codol_creditRankUpGP: entity must be a player entity");
        return;
    }
    if (Scr_GetNumParam() != 2) {
        Scr_Error("codol_creditRankUpGP requires the old and the new level");
        return;
    }
    Scr_AddInt(MatchReportCreditRankUp((unsigned)ref.entnum, Scr_GetInt(0), Scr_GetInt(1)));
}

/* What this row's "gp" will be when the "gameended" report carries it: the
 * fixed-award total plus the play GP as if the match ended now. The AAR's
 * sendFinal reads this in the same server frame as the report is written,
 * with the same clock, scores and level, so the end-of-game summary and the
 * lobby's deposit are the same number (aar's ui_eog_r7 is this minus
 * the fixed sources: the play GP). */
static int row_report_gp(unsigned client, const match_row_t *row)
{
    int fixed = row_fixed_gp(row);
    int play = live_play_gp(client, 1, NULL, NULL);
    return fixed > INT_MAX - play ? INT_MAX : fixed + play;
}

int MatchReportGP(unsigned client, int source)
{
    const match_row_t *row;
    if (client >= MAX_CLIENTS) return 0;
    row = &live[client];
    if (!row->used) return 0;
    sync_rankup(client);
    if (source < 0) return row_report_gp(client, row);
    if (source >= GP_SOURCES) return 0;
    return row->gp_fixed[source] > 0 ? row->gp_fixed[source] : 0;
}

/* self codol_matchGP( [<source>] )
 *
 * The GP this player's row holds now, before a report carries (and zeroes)
 * it: "fieldorder", "challenge" or "rankup" for one fixed-award source, "total" (the
 * default) for what progression.gp will deposit. The end-of-game summary
 * (_codol_aar.gsc) reads it just before processLobbyData writes the
 * "gameended" report. A player without an account row reads 0. */
void Scr_MatchGP(scr_entref_t ref)
{
    int source = -1;
    unsigned params;
    if (ref.classnum || ref.entnum >= MAX_CLIENTS || !g_entities[ref.entnum].client ||
        !g_entities[ref.entnum].r.inuse) {
        Scr_Error("codol_matchGP: entity must be a player entity");
        return;
    }
    params = Scr_GetNumParam();
    if (params > 1) {
        Scr_Error("codol_matchGP takes at most a source");
        return;
    }
    if (params == 1) {
        const char *name = Scr_GetString(0);
        if (Q_stricmp(name, "total")) {
            source = MatchReportGPSource(name);
            if (source < 0 || !*name) {
                Scr_Error("codol_matchGP: source must be \"total\", \"fieldorder\", \"challenge\" or \"rankup\"");
                return;
            }
        }
    }
    Scr_AddInt(MatchReportGP((unsigned)ref.entnum, source));
}

int MatchReportGPSource(const char *name)
{
    if (!name || !*name || !Q_stricmp(name, "fieldorder") || !Q_stricmp(name, "field_orders"))
        return GP_SOURCE_FIELD_ORDER;
    if (!Q_stricmp(name, "challenge") || !Q_stricmp(name, "challenges"))
        return GP_SOURCE_CHALLENGE;
    if (!Q_stricmp(name, "rankup") || !Q_stricmp(name, "rankups"))
        return GP_SOURCE_RANKUP;
    return -1;
}

/* self codol_creditGP(<amount> [, <source>])
 *
 * A FIXED GP award, recorded for the lobby exactly as given. <source> is
 * "fieldorder" (the default: _fieldorders.gsc, an FO_GP field order reward)
 * or "challenge" (_missions.gsc, a completed tier's CP column). Returns 1
 * when the GP was recorded. */
void Scr_CreditGP(scr_entref_t ref)
{
    int source = GP_SOURCE_FIELD_ORDER;
    unsigned params;
    if (ref.classnum || ref.entnum >= MAX_CLIENTS || !g_entities[ref.entnum].client ||
        !g_entities[ref.entnum].r.inuse) {
        Scr_Error("codol_creditGP: entity must be a player entity");
        return;
    }
    params = Scr_GetNumParam();
    if (params != 1 && params != 2) {
        Scr_Error("codol_creditGP requires an amount and optionally a source");
        return;
    }
    if (params == 2) {
        source = MatchReportGPSource(Scr_GetString(1));
        /* Rank-up GP has its own builtin, which keeps the per-level mark. */
        if (source < 0 || source == GP_SOURCE_RANKUP) {
            Scr_Error("codol_creditGP: source must be \"fieldorder\" or \"challenge\" "
                      "(rank-ups use codol_creditRankUpGP)");
            return;
        }
    }
    Scr_AddInt(MatchReportCreditFixedGP((unsigned)ref.entnum, source, Scr_GetInt(0)));
}

int MatchReportCoopMission(unsigned client, const char *mission, int completed)
{
    match_row_t *row;
    const char *p;
    if (client >= MAX_CLIENTS || !mission || !*mission) return 0;
    if (strlen(mission) >= MATCH_REPORT_COOP_MISSION_CHARS) return 0;
    /* A playlist name only (letters, digits, '_'): the lobby uses it as a key. */
    for (p = mission; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_'))
            return 0;
    row = &live[client];
    if (!row->used) return 0;
    /* A completion is never downgraded by a later failure in the same match. */
    if (row->coop_result == COOP_RESULT_COMPLETED && !completed &&
        !Q_stricmp(row->coop_mission, mission))
        return 1;
    Q_strncpyz(row->coop_mission, mission, sizeof(row->coop_mission));
    row->coop_result = (unsigned char)(completed ? COOP_RESULT_COMPLETED
                                                 : COOP_RESULT_FAILED);
    Com_Printf(CON_CHANNEL_SERVER,
        "CoDOL match report: client %u uid %llu coop mission %s %s\n",
        client, (unsigned long long)row->uid, row->coop_mission,
        completed ? "completed" : "failed");
    return 1;
}

int MatchReportZombiesCheckpoint(int difficulty, int round, int round_goal)
{
    if (difficulty != 3 || round < 1 || round_goal != 0 ||
        (unsigned)round < endless_round_checkpoint) return 0;
    endless_round_checkpoint = (unsigned)round;
    return 1;
}

int MatchReportZombiesResult(unsigned client, int extracted, int difficulty,
                                  int round, int round_goal, int downs,
                                  int revives, int headshots, int extraction_event)
{
    match_row_t *row;
    if (client >= MAX_CLIENTS || difficulty < 0 || difficulty > 3 ||
        round < 1 || (difficulty != 3 && round > 999) ||
        round_goal < 0 || round_goal > 999 ||
        (difficulty == 3 && (round_goal != 0 || extracted || extraction_event)) ||
        downs < -1 || revives < -1 || headshots < -1 ||
        (extraction_event != 0 && extraction_event != 1) ||
        (extraction_event && !extracted))
        return 0;
    row = &live[client];
    if (!row->used || !row->uid || row->bot) return 0;
    if (row->zm_result_sent) return 1;
    if (!row->zm_reported) {
        row->zm_downs = row->zm_revives = row->zm_headshots = -1;
    }
    row->zm_reported = 1;
    row->zm_checkpoint_only = 0;
    /* Once extracted, end-game settlement cannot downgrade the result. */
    row->zm_extracted |= extracted != 0;
    row->zm_difficulty = (unsigned char)difficulty;
    row->zm_round = (unsigned)round;
    row->zm_round_goal = (unsigned)round_goal;
    if (downs >= 0) row->zm_downs = zombie_counter_total(row->zm_carry_downs, downs);
    if (revives >= 0) row->zm_revives = zombie_counter_total(row->zm_carry_revives, revives);
    if (headshots >= 0) row->zm_headshots = zombie_counter_total(row->zm_carry_headshots, headshots);
    /* First successful lift-off is the endpoint. Subsequent end-game calls
     * refresh counters, never add the outbound flight or result-screen delay.
     * level.startTime is the same authoritative epoch as report duration_s. */
    if (extraction_event && !row->zm_time_valid && level.time >= level.startTime) {
        row->zm_time_ms = (unsigned)(level.time - level.startTime);
        row->zm_time_valid = 1;
        ((void)0);
    }
    return 1;
}

int MatchReportHasCollectible(unsigned client, unsigned index)
{
    const match_row_t *row;
    unsigned i;
    if (client >= MAX_CLIENTS || index >= MATCH_REPORT_COLLECTIBLE_IDS) return 0;
    row = &live[client];
    if (!row->used) return 0;
    for (i = 0; i < row->collectibles; ++i)
        if (row->collectible[i] == (unsigned short)index) return 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* JSON writing                                                              */
/* ------------------------------------------------------------------------- */

typedef struct { char *data; size_t size, used; int overflow; } buffer_t;

static void append(buffer_t *b, const char *text, size_t length)
{
    if (b->overflow || b->used + length >= b->size) { b->overflow = 1; return; }
    memcpy(b->data + b->used, text, length);
    b->used += length;
    b->data[b->used] = 0;
}

static void append_text(buffer_t *b, const char *text) { append(b, text, strlen(text)); }

static void append_u64(buffer_t *b, uint64_t value)
{
    char text[32];
    snprintf(text, sizeof(text), "%llu", (unsigned long long)value);
    append_text(b, text);
}

static void append_int(buffer_t *b, int value)
{
    char text[16];
    int n = snprintf(text, sizeof(text), "%d", value);
    if (n <= 0 || (size_t)n >= sizeof(text)) { b->overflow = 1; return; }
    append(b, text, (size_t)n);
}

/* A player name is arbitrary client text, including C1 colour escapes and
 * whatever bytes survived the name filter. Emit it as a JSON string the lobby's
 * json.loads accepts: quote/backslash escaped, every control byte as \u00XX,
 * and every byte above 0x7f as \u00XX too, which is the latin-1 reading the
 * lobby already uses for names elsewhere. */
static void append_string(buffer_t *b, const char *text)
{
    const unsigned char *p;
    append_text(b, "\"");
    for (p = (const unsigned char *)(text ? text : ""); *p; ++p) {
        char escape[8];
        if (*p == '"' || *p == '\\') { escape[0] = '\\'; escape[1] = (char)*p; append(b, escape, 2); }
        else if (*p >= 0x20 && *p < 0x7f) append(b, (const char *)p, 1);
        else { snprintf(escape, sizeof(escape), "\\u%04x", (unsigned)*p); append(b, escape, 6); }
    }
    append_text(b, "\"");
}

static void append_uid(buffer_t *b, uint64_t uid)
{
    char text[24];
    int n = snprintf(text, sizeof(text), "%llu", (unsigned long long)uid);
    if (n <= 0 || (size_t)n >= sizeof(text)) { b->overflow = 1; return; }
    append_text(b, "\"");
    append(b, text, (size_t)n);
    append_text(b, "\"");
}

static const char *outcome_for(int team, const int scores[2])
{
    /* The team a row played for against the other one. TEAM_RED is bank 1 and
     * TEAM_BLUE bank 2, the same pairing PublishTeamScores uses. */
    int mine, theirs;
    if (team == TEAM_RED) { mine = scores[0]; theirs = scores[1]; }
    else if (team == TEAM_BLUE) { mine = scores[1]; theirs = scores[0]; }
    else return "tie";
    if (mine > theirs) return "win";
    if (mine < theirs) return "loss";
    return "tie";
}

/* `client` is the engine slot this row is still playing in, or -1 for a
 * departed row or a bot. A live row's challenge deltas are diffed here, from
 * the baseline the previous report left (challenge_base), rather than stored. */
static void append_row(buffer_t *b, int slot, const match_row_t *row,
                       int kills, int deaths, int assists, int score, int earned,
                       const int *stats, const int scores[2], int first, int client,
                       int play_gp, uint64_t play_time_ms)
{
    match_row_t merged;
    if (client >= 0) {
        merged = *row;
        challenge_stay_deltas((unsigned)client, &merged, challenge_into_row);
        row = &merged;
    }
    if (!first) append_text(b, ",\n  ");
    append_text(b, "{\"slot\": ");
    append_int(b, slot);
    append_text(b, ", \"name\": ");
    append_string(b, row->name[0] ? row->name : "player");
    append_text(b, ", \"bot\": ");
    append_text(b, row->bot ? "true" : "false");
    if (!row->bot && row->uid) { append_text(b, ", \"uid\": "); append_uid(b, row->uid); }
    if (!row->bot && row->uid) {
        append_text(b, ", \"completed\": ");
        append_text(b, !row->zm_result_sent && (row->terminal_completed ||
            (reward_completed && client >= 0 && client_connected((unsigned)client)))
                       ? "true" : "false");
    }
    if (!row->bot && row->uid) {
        append_text(b, ", \"play_time_ms\": ");
        append_u64(b, play_time_ms);
        append_text(b, ", \"play_time_source\": \"native_participation_clock\"");
    }
    append_text(b, ", \"team\": ");
    append_int(b, row->team);
    append_text(b, ", \"kills\": ");
    append_int(b, kills);
    append_text(b, ", \"deaths\": ");
    append_int(b, deaths);
    append_text(b, ", \"assists\": ");
    append_int(b, assists);
    append_text(b, ", \"score\": ");
    append_int(b, score);
    append_text(b, ", \"outcome\": ");
    append_string(b, row->terminal_frozen ?
        (row->terminal_outcome == GP_OUTCOME_WIN ? "win" :
         row->terminal_outcome == GP_OUTCOME_TIE ? "tie" : "loss") : outcome_for(row->team, scores));
    if (!row->bot && (row->zm_reported || row->zm_result_sent || row->zm_checkpoint_only)) {
        /* A later XP/counter delta is not another extraction. Keep an explicit
         * non-final marker so older outcome-only fallbacks cannot count it. */
        append_text(b, ", \"zombies\": {\"final\": ");
        append_text(b, row->zm_reported ? "true" : "false");
        append_text(b, ", \"extracted\": ");
        append_text(b, row->zm_extracted ? "true" : "false");
        append_text(b, ", \"difficulty\": ");
        append_int(b, row->zm_difficulty);
        append_text(b, ", \"round\": ");
        append_int(b, row->zm_round);
        append_text(b, ", \"round_goal\": ");
        append_int(b, row->zm_round_goal);
        append_text(b, ", \"time_s\": ");
        if (row->zm_time_valid) append_int(b, row->zm_time_ms / 1000);
        else append_text(b, "null");
        append_text(b, ", \"time_ms\": ");
        if (row->zm_time_valid) append_int(b, row->zm_time_ms);
        else append_text(b, "null");
        append_text(b, ", \"time_source\": ");
        if (row->zm_time_valid) append_string(b, "extraction_liftoff");
        else append_text(b, "null");
        append_text(b, ", \"downs\": ");
        if (row->zm_downs >= 0) append_int(b, row->zm_downs);
        else append_text(b, "null");
        append_text(b, ", \"revives\": ");
        if (row->zm_revives >= 0) append_int(b, row->zm_revives);
        else append_text(b, "null");
        append_text(b, ", \"headshots\": ");
        if (row->zm_headshots >= 0) append_int(b, row->zm_headshots);
        else append_text(b, "null");
        append_text(b, "}");
    }
    if (!row->bot && stats) {
        /* The career counters this match added, measured out of the secure
         * profile the scripts maintain. Absent members are simply absent from
         * the object; the lobby credits what it is given. */
        unsigned n; int first = 1, has_secure_time = 0;
        append_text(b, ", \"lifetime\": {");
        for (n = 0; n < MATCH_REPORT_STATS; ++n) {
            if (stats[n] <= 0) continue;
            if (!strcmp(match_report_stats[n], "timePlayedTotal")) has_secure_time = 1;
            if (!first) append_text(b, ", ");
            first = 0;
            append_string(b, match_report_stats[n]);
            append_text(b, ": ");
            append_int(b, stats[n]);
        }
        /* Zombies scripts do not maintain secure timePlayedTotal. Supply its
         * missing per-report seconds for the existing lobby career fold, never
         * alter the secure blob or replace an existing positive secure delta.
         * Restrict this to Zombies: MP may flush its secure timer later, which
         * would overlap a fallback credited by an earlier partial report. */
        if (!has_secure_time && (!Q_stricmp(cvar_text("sv_zoneType"), "zm") ||
                                 !Q_stricmp(cvar_text("g_gametype"), "zm"))) {
            uint64_t now = row->participation_reported_ms + play_time_ms;
            uint64_t seconds = now / 1000 - row->participation_reported_ms / 1000;
            if (!first) append_text(b, ", ");
            append_text(b, "\"timePlayedTotal\": ");
            append_u64(b, seconds);
        }
        append_text(b, "}");
    }
    if (!row->bot) {
        /* kill_xp/match_bonus are the retired Python writer's split of the same
         * total. This server has no separate end-of-match bonus stage: the
         * vendor scripts award it through giveRankXP like any other event, so
         * it is already inside the measured delta and is not re-derived here.
         * applied_live stays 0 - nothing was persisted outside this report. */
        append_text(b, ", \"xp\": {\"kill_xp\": ");
        append_int(b, earned);
        append_text(b, ", \"match_bonus\": 0, \"total\": ");
        append_int(b, earned);
        append_text(b, ", \"applied_live\": 0}");
        /* The lobby folds this into players/<uid>.json and re-renders the
         * secure blob from it; see alpha0022 rank_writeback._merge_progression
         * and progression.credit(). Keys are the numeric challenge ids, the
         * same addresses these deltas were measured at. Absent when nothing
         * moved, so an ordinary match adds nothing to the file. */
        /* "weapons" is the base weapon name -> weapon XP this match, which the
         * lobby adds to the profile and serves as the inventory record's xp
         * field, the only thing C1 derives a weapon level from. */
        /* "collectibles" is the set of collectibles[] indices picked up this
         * match, as {"<index>": 1}: the lobby keeps their union in the profile
         * and sets those bits in the mpdata (mp/playerdata.def) blob it serves,
         * which is the record C1's scoreboard intel list reads. */
        /* "coop_mission_completed" / "coop_mission_failed" name the coop
         * playlist (sv_sessionMap) this player finished. The lobby records
         * a completion in the profile and unlocks the next mission; a failure
         * unlocks nothing. */
        /* "gp" is the GP (account currency 2) this row is owed for the
         * match; the lobby deposits exactly that in this uid's wallet.
         * "gp_play" is the scaled GP for playing the match (match_gp.h)
         * and "gp_fixed" the fixed awards by source (field_orders,
         * challenges, rankups). The lobby deposits "gp", except that it pays
         * rank-ups itself off its own per-account level mark (the levels its
         * XP fold actually crossed), so it swaps gp_fixed.rankups for that;
         * the rest is for the journal. */
        if (row->challenges || row->weapons || row->collectibles || row->coop_result ||
            row_fixed_gp(row) > 0 || play_gp > 0) {
            unsigned c;
            int any = 0;
            append_text(b, ", \"progression\": {");
            if (row->challenges) {
                any = 1;
                append_text(b, "\"challenges\": {");
                for (c = 0; c < row->challenges; ++c) {
                    if (c) append_text(b, ", ");
                    append_text(b, "\"");
                    append_int(b, (int)row->challenge_id[c]);
                    append_text(b, "\": ");
                    append_int(b, row->challenge_carry[c]);
                }
                append_text(b, "}");
            }
            if (row->weapons) {
                if (any) append_text(b, ", ");
                any = 1;
                append_text(b, "\"weapons\": {");
                for (c = 0; c < row->weapons; ++c) {
                    if (c) append_text(b, ", ");
                    append_string(b, row->weapon_name[c]);
                    append_text(b, ": ");
                    append_int(b, row->weapon_xp[c]);
                }
                append_text(b, "}");
            }
            if (row->coop_result) {
                if (any) append_text(b, ", ");
                any = 1;
                append_text(b, row->coop_result == COOP_RESULT_COMPLETED
                                   ? "\"coop_mission_completed\": "
                                   : "\"coop_mission_failed\": ");
                append_string(b, row->coop_mission);
            }
            if (row_fixed_gp(row) > 0 || play_gp > 0) {
                unsigned s;
                int first_source = 1;
                int fixed = row_fixed_gp(row);
                if (any) append_text(b, ", ");
                any = 1;
                append_text(b, "\"gp\": ");
                append_int(b, fixed > INT_MAX - play_gp ? INT_MAX : fixed + play_gp);
                if (play_gp > 0) {
                    append_text(b, ", \"gp_play\": ");
                    append_int(b, play_gp);
                }
                if (fixed > 0) {
                    append_text(b, ", \"gp_fixed\": {");
                    for (s = 0; s < GP_SOURCES; ++s) {
                        if (row->gp_fixed[s] <= 0) continue;
                        if (!first_source) append_text(b, ", ");
                        first_source = 0;
                        append_string(b, gp_source_names[s]);
                        append_text(b, ": ");
                        append_int(b, row->gp_fixed[s]);
                    }
                    append_text(b, "}");
                }
            }
            if (row->collectibles) {
                if (any) append_text(b, ", ");
                append_text(b, "\"collectibles\": {");
                for (c = 0; c < row->collectibles; ++c) {
                    if (c) append_text(b, ", ");
                    append_text(b, "\"");
                    append_int(b, (int)row->collectible[c]);
                    append_text(b, "\": 1");
                }
                append_text(b, "}");
            }
            append_text(b, "}");
        }
        if (row->challenge_overflow)
            Com_PrintWarning(CON_CHANNEL_SERVER,
                "CoDOL match report: %s advanced more than %u challenges; the rest "
                "are not in the report\n", row->name[0] ? row->name : "a player",
                (unsigned)MATCH_REPORT_CHALLENGES);
    }
    append_text(b, "}");
}

static int write_file(const char *directory, const char *name, const char *text, size_t length)
{
    char path[MAX_OSPATH], temporary[MAX_OSPATH];
    FILE *file;
    int n;
    n = snprintf(path, sizeof(path), "%s/%s", directory, name);
    if (n <= 0 || (size_t)n >= sizeof(path)) return 0;
    n = snprintf(temporary, sizeof(temporary), "%s/%s.tmp", directory, name);
    if (n <= 0 || (size_t)n >= sizeof(temporary)) return 0;
    FS_ReplaceSeparators(path);
    FS_ReplaceSeparators(temporary);
    file = fopen(temporary, "wb");
    if (!file) return 0;
    if (fwrite(text, 1, length, file) != length) { fclose(file); remove(temporary); return 0; }
    if (fclose(file)) { remove(temporary); return 0; }
    /* POSIX rename replaces in one step. Windows refuses a rename onto an
     * existing name, so the old report is removed first; a reader can then see
     * the complete old file, the complete new file, or no file, but never a
     * half-written one - which is all the lobby's consume-by-rename needs. */
    if (rename(temporary, path)) {
        remove(path);
        if (rename(temporary, path)) { remove(temporary); return 0; }
    }
    return 1;
}

/* Where this report goes.
 *
 * match_report.json is ONE slot and the lobby empties it only when a client
 * asks it for mpdata_s or its marketplace record - that is, when a player is
 * back in the front end. With a map rotation (exitToLobby 0) nobody goes
 * back between matches, so every gameended report replaced the unconsumed one
 * before it: on 2026-09-27 the lobby ledger holds only the last report of each
 * rotation run and 16700 + 10940 XP of earlier matches never arrived. A second
 * match server sharing the resources directory (sh1x-rollout.sh) races for the
 * same name as well.
 *
 * So, by default, every report gets its own name that sorts in write order:
 * match_report.q.<epoch>.<port>.<seq>.json. The lobby folds them oldest
 * first. matchReportSpool 0 restores the single legacy file for a lobby
 * that does not read the spool yet. */
static int report_file_name(char *out, size_t size)
{
    static cvar_t *spool;
    static unsigned sequence;
    cvar_t *port;
    int n;
    if (!spool)
        spool = Cvar_RegisterBool("matchReportSpool", qtrue, CVAR_ARCHIVE,
            "Write every match report under its own name for the lobby to fold in order");
    if (!spool->boolean) {
        Q_strncpyz(out, MATCH_REPORT_NAME, (int)size);
        return 1;
    }
    port = Cvar_FindVar("net_port");
    n = snprintf(out, size, "match_report.q.%010lu.%05d.%06u.json",
                 (unsigned long)Com_GetRealtime(), port ? port->integer : 0,
                 ++sequence % 1000000u);
    return n > 0 && (size_t)n < size;
}

static void defer_report(const char *reason)
{
    /* Never downgrade a completed outcome to a later disconnect/mapchange. */
    if (!strcmp(deferred_report_reason, "gameended") ||
        !strcmp(deferred_report_reason, "coopmission")) return;
    Q_strncpyz(deferred_report_reason, reason && *reason ? reason : "ended",
               sizeof(deferred_report_reason));
}

void MatchReportFrame(void)
{
    int status, error = 0;
    unsigned now = Sys_Milliseconds();
    if (!pending_report.active) return;
    if (pending_report.job) {
        status = AsyncCapturePublishStatus(pending_report.job, &error);
        if (!status) return;
        AsyncCapturePublishRelease(pending_report.job);
        pending_report.job = 0;
        if (status == 1) {
            PersistentRuntimeReportPublished(pending_report.resident_scope,pending_report.path,
                pending_report.bytes,pending_report.size);
            memset(&pending_report, 0, sizeof(pending_report));
            return;
        }
        pending_report.retry_at = now + 1000u;
        if (!pending_report.warned) {
            Com_PrintError(CON_CHANNEL_SERVER,
                "CoDOL match report: durable publication failed (%d); immutable result retained, transition held\n", error);
            pending_report.warned = 1;
        }
    }
    if ((int)(now - pending_report.retry_at) < 0) return;
    pending_report.job = AsyncCapturePublish(pending_report.path,
        pending_report.bytes, pending_report.size, pending_report.replace);
    if (!pending_report.job) {
        pending_report.retry_at = now + 1000u;
        if (!pending_report.warned) {
            Com_PrintError(CON_CHANNEL_SERVER,
                "CoDOL match report: persistence queue unavailable; immutable result retained, transition held\n");
            pending_report.warned = 1;
        }
    }
}

static int report_absolute_path(const char *directory, const char *name,
                                char *out, size_t size)
{
    const char *cwd;
    int n;
    if (!directory || !*directory || !name || !*name) return 0;
    if (directory[0] == '/' ||
        (directory[0] && directory[1] == ':' &&
         (directory[2] == '/' || directory[2] == '\\')))
        n = snprintf(out, size, "%s/%s", directory, name);
    else {
        /* Preserve legacy relative profile-directory configurations, but freeze
         * their process-relative meaning on the engine thread before enqueue. */
        cwd = Sys_Cwd();
        if (!cwd || !*cwd) return 0;
        n = snprintf(out, size, "%s/%s/%s", cwd, directory, name);
    }
    if (n <= 0 || (size_t)n >= size) return 0;
    FS_ReplaceSeparators(out); /* legacy profile-path normalization, main only */
    return 1;
}

int WriteMatchReport(const char *reason)
{
    if (LocalTestBuild()) return 0; /* Local testing binaries never pay. */
    static char storage[MATCH_REPORT_BYTES];
    buffer_t b;
    const char *directory = report_directory();
    int scores[2], slot, rows = 0, humans = 0, activity = 0, earned_total = 0;
    int live_earned[MAX_CLIENTS], live_kills[MAX_CLIENTS], live_deaths[MAX_CLIENTS];
    int live_assists[MAX_CLIENTS], live_score[MAX_CLIENTS], live_total[MAX_CLIENTS];
    int live_play[MAX_CLIENTS], live_outcome_of[MAX_CLIENTS];
    uint64_t live_participation[MAX_CLIENTS];
    play_gp_detail_t live_detail[MAX_CLIENTS];
    unsigned char live_selected[MAX_CLIENTS] = {0}, live_owed[MAX_CLIENTS] = {0};
    unsigned char departed_selected[MATCH_REPORT_DEPARTED] = {0};
    int selected = 0;
    /* Only a match that reached its end pays the win/tie scale; any other
     * report pays its live rows as leavers. */
    int finished = reason && (!strcmp(reason, "gameended") || !strcmp(reason, "coopmission"));
    static int live_stats[MAX_CLIENTS][MATCH_REPORT_STATS];
    static int departed_stats[MATCH_REPORT_DEPARTED][MATCH_REPORT_STATS];
    char file_name[96];
    unsigned i, n;
    file_name[0] = 0;
    /* The first terminal event ends participation even if spool delivery must
     * retry. Result screens, later reports, and reconnects cannot add time. */
    if (reason && (!strcmp(reason, "gameended") || !strcmp(reason, "coopmission") ||
                   !strcmp(reason, "timelimit") || !strcmp(reason, "scorelimit") ||
                   !strcmp(reason, "mapchange"))) {
        participation_match_finished = 1;
        for (i = 0; i < MAX_CLIENTS; ++i) if (live[i].used) {
            freeze_participation(&live[i]);
            if (!live[i].terminal_frozen) {
                live[i].play_ms = row_play_ms(&live[i]);
                live[i].play_running = 0;
                live[i].terminal_outcome = live_outcome(i, &live[i], finished);
                live[i].terminal_completed = !strcmp(reason,"gameended") ||
                    !strcmp(reason,"timelimit") || !strcmp(reason,"scorelimit");
                live[i].terminal_frozen = 1;
            }
        }
    }
    MatchReportFrame();
    if (pending_report.active) {
        defer_report(reason);
        return REPORT_PENDING;
    }
    if (deferred_report_reason[0]) {
        /* Stable copied reason; callers may pass a temporary command buffer. */
        static char resumed_reason[64];
        Q_strncpyz(resumed_reason, deferred_report_reason, sizeof(resumed_reason));
        deferred_report_reason[0] = 0;
        reason = resumed_reason;
        finished = !strcmp(reason, "gameended") || !strcmp(reason, "coopmission");
    }

    if (!level.clients || !sv_maxclients) return 0;
    /* Cheats on in this process (dev_cheats.c): no rewards, no stats. */
    if (CheatsBlockProgression("match report")) return 0;
    scores[0] = level.teamScores[TEAM_RED];
    scores[1] = level.teamScores[TEAM_BLUE];
    memset(live_total, 0, sizeof(live_total));

    for (i = 0; i < MAX_CLIENTS; ++i) {
        int team = 0, total = 0, before_activity = activity;
        live_earned[i] = live_kills[i] = live_deaths[i] = live_assists[i] = live_score[i] = 0;
        live_play[i] = 0;
        live_participation[i] = 0;
        live_outcome_of[i] = GP_OUTCOME_LOSS;
        if (!live[i].used || !client_connected(i)) continue;
        live_participation[i] = row_participation_ms(&live[i]);
        activity += participation_delta(&live[i], live_participation[i]) > 0 ? 1 : 0;
        /* Before anything below resets the coop result it may depend on. */
        client_level(i, &live[i].level);
        /* Rank-up GP still owed for the profile's level, before it counts. */
        sync_rankup(i);
        live_play[i] = live_play_gp(i, finished, &live_outcome_of[i], &live_detail[i]);
        client_counters(i, &live_kills[i], &live_deaths[i], &live_assists[i], &live_score[i], &team);
        live[i].team = team;
        if (svs.clients[i].name[0]) Q_strncpyz(live[i].name, svs.clients[i].name, sizeof(live[i].name));
        live_kills[i] = live_kills[i] + live[i].carry_kills > 0 ? live_kills[i] + live[i].carry_kills : 0;
        live_deaths[i] = live_deaths[i] + live[i].carry_deaths > 0 ? live_deaths[i] + live[i].carry_deaths : 0;
        live_assists[i] = live_assists[i] + live[i].carry_assists > 0 ? live_assists[i] + live[i].carry_assists : 0;
        live_score[i] = live_score[i] + live[i].carry_score > 0 ? live_score[i] + live[i].carry_score : 0;
        live_earned[i] = live[i].carry_xp;
        for (n = 0; n < MATCH_REPORT_STATS; ++n) {
            int value = 0;
            live_stats[i][n] = live[i].stat_carry[n];
            if (live[i].stat_valid[n] &&
                read_secure_stat(i, match_report_stats[n], &value) &&
                value > live[i].stat_base[n])
                live_stats[i][n] += value - live[i].stat_base[n];
            if (live_stats[i][n] < 0) live_stats[i][n] = 0;
        }
        if (live[i].baseline_valid && client_total_xp(i, &total)) {
            live_total[i] = total;
            if (total > live[i].baseline_xp) live_earned[i] += total - live[i].baseline_xp;
        }
        ++rows;
        ++humans;
        earned_total += live_earned[i];
        activity += live_kills[i] + live_deaths[i] + live_assists[i] + live_score[i];
        /* The lobby credits challenge progress too, so it is activity. */
        for (n = 0; n < live[i].challenges; ++n)
            if (live[i].challenge_carry[n] > 0) activity += live[i].challenge_carry[n];
        challenge_stay_deltas(i, &activity, challenge_count);
        /* So is a picked-up collectible. */
        activity += live[i].collectibles;
        /* And a finished coop mission. */
        activity += live[i].coop_result ? 1 : 0;
        activity += live[i].zm_reported ? 1 : 0;
        /* And GP, fixed or for playing: a player who only played some
         * minutes is still owed them. */
        activity += row_fixed_gp(&live[i]) > 0 ? 1 : 0;
        activity += live_play[i] > 0 ? 1 : 0;
        live_owed[i] = live_earned[i] > 0 || activity != before_activity;
    }
    for (i = 0; i < MATCH_REPORT_DEPARTED; ++i) {
        if (!departed[i].used) continue;
        for (n = 0; n < MATCH_REPORT_STATS; ++n)
            departed_stats[i][n] = departed[i].stat_carry[n] > 0 ? departed[i].stat_carry[n] : 0;
        ++rows;
        ++humans;
        earned_total += departed[i].carry_xp;
        activity += departed[i].kills + departed[i].deaths + departed[i].assists +
                    departed[i].score;
        for (n = 0; n < departed[i].challenges; ++n)
            if (departed[i].challenge_carry[n] > 0) activity += departed[i].challenge_carry[n];
        activity += departed[i].collectibles;
        activity += departed[i].coop_result ? 1 : 0;
        activity += departed[i].zm_reported ? 1 : 0;
        activity += departed[i].zm_checkpoint_only ? 1 : 0;
        activity += row_fixed_gp(&departed[i]) > 0 ? 1 : 0;
        activity += departed[i].gp_match > 0 ? 1 : 0;
        activity += participation_delta(&departed[i], row_participation_ms(&departed[i])) > 0 ? 1 : 0;
    }
    /* "Activity" is deliberately what the lobby can be credited for - XP, and
     * the kills/deaths/assists/score that become lifetime counters - and
     * deliberately NOT time played. A report carrying nothing but a duration
     * would still replace a report the lobby has not consumed yet, which is
     * how a real match's XP would get thrown away by an idle reconnect.
     * Say the outcome out loud either way: "no report" and "a report with
     * nothing in it" look identical from the console otherwise, and the usual
     * cause is upstream (the scripts never called setRankExperience). */
    if (!rows || !humans || (earned_total <= 0 && activity <= 0)) {
        ((void)0);
        for (i = 0; i < MAX_CLIENTS; ++i) {
            if (!live[i].used || !client_connected(i)) continue;
            ((void)0);
        }
        for (i = 0; i < MATCH_REPORT_DEPARTED; ++i) {
            if (!departed[i].used) continue;
            ((void)0);
        }
        return 0;
    }

    b.data = storage; b.size = sizeof(storage); b.used = 0; b.overflow = 0;
    reward_completed = reason && (!Q_stricmp(reason, "gameended") ||
        !Q_stricmp(reason, "timelimit") || !Q_stricmp(reason, "scorelimit"));
    storage[0] = 0;
    append_text(&b, "{\n \"format\": 1,\n \"created\": ");
    append_int(&b, (int)(long)Com_GetRealtime());
    reward_context();
    append_text(&b, ",\n \"playlist_id\": ");
    append_int(&b, reward_playlist);
    append_text(&b, ",\n \"xp_rate_percent\": ");
    append_int(&b, reward_xp_rate);
    append_text(&b, ",\n \"gp_rate_percent\": ");
    append_int(&b, reward_gp_rate);
    if (QuickPlayEnabled()) {
        append_text(&b, ",\n \"quick_session\": ");
        append_string(&b, QuickPlaySession());
        append_text(&b, ",\n \"quick_epoch\": ");
        append_u64(&b, QuickPlayEpoch());
    }
    append_text(&b, ",\n \"reason\": ");
    append_string(&b, reason && *reason ? reason : "ended");
    append_text(&b, ",\n \"server_time_ms\": ");
    append_int(&b, level.time);
    append_text(&b, ",\n \"duration_s\": ");
    append_int(&b, level.time > level.startTime ? (level.time - level.startTime) / 1000 : 0);
    {
        cvar_t *game = Cvar_FindVar("g_gametype");
        cvar_t *map = Cvar_FindVar("mapname");
        cvar_t *difficulty = Cvar_FindVar("g_difficulty");
        append_text(&b, ",\n \"gametype\": ");
        append_string(&b, game && game->string ? game->string : "");
        append_text(&b, ",\n \"map\": ");
        append_string(&b, map && map->string ? map->string : "");
        if (game && game->string && !Q_stricmp(game->string, "zm") &&
            difficulty && difficulty->integer >= 0 && difficulty->integer <= 3) {
            append_text(&b, ",\n \"zombies\": {\"difficulty\": ");
            append_int(&b, difficulty->integer);
            append_text(&b, "}");
        }
    }
    { const char *resident=PersistentRuntimeReportEnvelope();
      if(resident) {append_text(&b, ",\n \"resident\": ");append_text(&b,resident);} }
    append_text(&b, ",\n \"team_scores\": [");
    append_int(&b, scores[0]);
    append_text(&b, ", ");
    append_int(&b, scores[1]);
    append_text(&b, "],\n \"players\": [\n  ");
    slot = 0;
    /* Departed owners first. Each whole row is admitted atomically into the
     * bounded JSON body; unselected owners remain for the next transaction. */
    for (i = 0; i < MATCH_REPORT_DEPARTED; ++i) {
        size_t before;
        if (!departed[i].used) continue;
        before = b.used;
        append_row(&b, departed[i].slot, &departed[i], departed[i].kills, departed[i].deaths,
                   departed[i].assists, departed[i].score, departed[i].carry_xp,
                   departed_stats[i], scores, !slot, -1, departed[i].gp_match,
                   participation_delta(&departed[i], row_participation_ms(&departed[i])));
        if (b.overflow || b.used + 8 >= b.size) {
            b.used=before;b.data[before]=0;b.overflow=0;break;
        }
        departed_selected[i]=1;++slot;++selected;
    }
    for (i = 0; i < MAX_CLIENTS; ++i) {
        size_t before;
        if (!live[i].used || !client_connected(i) || !live_owed[i]) continue;
        before=b.used;
        append_row(&b, live[i].slot, &live[i], live_kills[i], live_deaths[i], live_assists[i],
                   live_score[i], live_earned[i], live_stats[i], scores, !slot, (int)i, live_play[i],
                   participation_delta(&live[i], live_participation[i]));
        if (b.overflow || b.used + 8 >= b.size) {
            b.used=before;b.data[before]=0;b.overflow=0;break;
        }
        live_selected[i]=1;++slot;++selected;
    }
    if (!selected) {
        Com_PrintError(CON_CHANNEL_SERVER,
            "CoDOL match report: one result row exceeds bounded report capacity; rows retained\n");
        return -1;
    }
    /* Preserve zero-delta human context when space permits (including the
     * explicit non-final marker after an earlier Zombies extraction). It must
     * never crowd out an owed owner or settle an unselected result. */
    for (i = 0; i < MAX_CLIENTS; ++i) {
        size_t before;
        if (!live[i].used || !client_connected(i) || live_owed[i]) continue;
        before=b.used;
        append_row(&b, live[i].slot, &live[i], live_kills[i], live_deaths[i], live_assists[i],
                   live_score[i], live_earned[i], live_stats[i], scores, !slot, (int)i, live_play[i],
                   participation_delta(&live[i], live_participation[i]));
        if (b.overflow || b.used + 8 >= b.size) {
            b.used=before;b.data[before]=0;b.overflow=0;break;
        }
        ++slot;
    }
    /* The bots, for the scoreboard context the lobby's format carries. They own
     * no stored row - nothing attaches a profile for them - so they are built
     * here, where the netchan is real and the NA_BOT test actually answers. */
    for (i = 0; i < MAX_CLIENTS; ++i) {
        match_row_t bot;
        int team = 0, kills, deaths, assists, score;
        if (live[i].used || !client_connected(i) || !client_is_bot(i)) continue;
        memset(&bot, 0, sizeof(bot));
        bot.bot = 1;
        bot.slot = (unsigned char)i;
        client_counters(i, &kills, &deaths, &assists, &score, &team);
        bot.team = team;
        if (svs.clients[i].name[0]) Q_strncpyz(bot.name, svs.clients[i].name, sizeof(bot.name));
        {
            size_t before=b.used;
            append_row(&b, bot.slot, &bot, kills, deaths, assists, score, 0, NULL, scores, !slot, -1, 0, 0);
            if (b.overflow || b.used + 8 >= b.size) {
                b.used=before;b.data[before]=0;b.overflow=0;break;
            }
            ++slot;
        }
    }
    append_text(&b, "\n ]\n}\n");
    if (b.overflow) {
        Com_PrintWarning(CON_CHANNEL_SERVER,
            "CoDOL match report: %d rows do not fit in %u bytes; the match result is NOT reported\n",
            rows, (unsigned)sizeof(storage));
        return -1;
    }
    if (!report_file_name(file_name, sizeof(file_name)) ||
        !report_absolute_path(directory, file_name, pending_report.path,
                              sizeof(pending_report.path))) {
        if (!report_path_warned) {
            Com_PrintError(CON_CHANNEL_SERVER,
                "CoDOL match report: missing/invalid absolute profile path; rows retained, transition held\n");
            report_path_warned = 1;
        }
        return -1;
    }
    report_path_warned = 0;
    memcpy(pending_report.bytes, b.data, b.used);
    pending_report.size = b.used;
    pending_report.replace = !strcmp(file_name, MATCH_REPORT_NAME);
    Q_strncpyz(pending_report.resident_scope,PersistentRuntimeReportScope(),sizeof(pending_report.resident_scope));
    pending_report.active = 1;
    pending_report.retry_at = Sys_Milliseconds();
    /* Ownership transfers to the retained transaction BEFORE row rebasing.
     * Queue admission is retryable and is not a durability acknowledgement. */
    ((void)0);
    /* One line per human row, so a report the lobby never folds can still be
     * credited by hand from the console log alone. */
    for (i = 0; i < MAX_CLIENTS; ++i) {
        if (!live[i].used || !client_connected(i)) continue;
        ((void)0);
        ((void)0);
    }
    for (i = 0; i < MATCH_REPORT_DEPARTED; ++i) {
        if (!departed[i].used) continue;
        ((void)0);
        ((void)0);
    }
    /* Consecutive reports must partition the match, never repeat it: everything
     * just reported becomes the new baseline, and the frozen rows are done. */
    for (i = 0; i < MAX_CLIENTS; ++i) {
        if (!live_selected[i]) continue;
        live[i].participation_reported_ms = live_participation[i];
        live[i].carry_xp = 0;
        /* live_* already included the old carry, so subtracting it leaves the
         * negative of what the engine counters read at this instant: the next
         * report's row is then only what accrued after this one. */
        live[i].carry_kills -= live_kills[i];
        live[i].carry_deaths -= live_deaths[i];
        live[i].carry_assists -= live_assists[i];
        live[i].carry_score -= live_score[i];
        if (live[i].baseline_valid && live_total[i] > live[i].baseline_xp)
            live[i].baseline_xp = live_total[i];
        /* Challenges are measured against a baseline: this report took the
         * earlier stays' carry and everything up to the values it just read. */
        memset(live[i].challenge_id, 0, sizeof(live[i].challenge_id));
        memset(live[i].challenge_carry, 0, sizeof(live[i].challenge_carry));
        live[i].challenges = 0;
        live[i].challenge_overflow = 0;
        if (client_connected(i)) challenge_rebase(i);
        /* Weapon XP is accumulated, not measured, so the report owns it now. */
        memset(live[i].weapon_name, 0, sizeof(live[i].weapon_name));
        memset(live[i].weapon_xp, 0, sizeof(live[i].weapon_xp));
        live[i].weapons = 0;
        live[i].weapon_overflow = 0;
        /* Collectibles are recorded, not measured: the report owns them now. */
        memset(live[i].collectible, 0, sizeof(live[i].collectible));
        live[i].collectibles = 0;
        live[i].collectible_overflow = 0;
        /* So is the coop mission result. */
        memset(live[i].coop_mission, 0, sizeof(live[i].coop_mission));
        live[i].coop_result = COOP_RESULT_NONE;
        if (live[i].zm_reported) live[i].zm_result_sent = 1;
        live[i].zm_reported = 0;
        /* And the fixed GP. */
        memset(live[i].gp_fixed, 0, sizeof(live[i].gp_fixed));
        /* The play GP is paid once per match: a carried row stops its timer,
         * and a later departure or report pays it nothing more. */
        if (client_connected(i) && !live[i].match_gp_settled) {
            if (live_play[i] > 0)
                ((void)0);
            live[i].play_ms = row_play_ms(&live[i]);
            live[i].play_running = 0;
            live[i].match_gp_settled = 1;
        }
        for (n = 0; n < MATCH_REPORT_STATS; ++n) {
            int value = 0;
            live[i].stat_carry[n] = 0;
            if (live[i].stat_valid[n] && read_secure_stat(i, match_report_stats[n], &value))
                live[i].stat_base[n] = value;
        }
    }
    for (i=0;i<MATCH_REPORT_DEPARTED;++i) if (departed_selected[i]) {
        if (i>=MATCH_REPORT_DEPARTED_SHARED)
            pending_report.blocked_slots[i-MATCH_REPORT_DEPARTED_SHARED]=1;
        memset(&departed[i],0,sizeof(departed[i]));
    }
    MatchReportFrame();
    return REPORT_PENDING;
}

/* ------------------------------------------------------------------------- */
/* Server match record delivery (setMatchDataDef/setMatchData/sendMatchData)  */
/* ------------------------------------------------------------------------- */

/* C1 hands the prepared {record, definition size} pair to its Live upload queue
 * (0x551d80 -> 0x5cf180 -> 0x58eba0). This server has no Demonware backend of
 * its own: the lobby is the backend, and the boundary between the two processes
 * is the same resources directory the rank report crosses. So deliver the
 * record there, beside match_report.json, under the definition's own name.
 * Nothing in the lobby consumes it yet - this closes the "retained, no delivery
 * sink" path and gives the end-game record a real destination, it is not a
 * claim that the lobby reads it. */
static int matchdata_sink(void *context, const server_matchdata_submission_t *submission)
{
    const char *directory = report_directory();
    (void)context;
    if (!submission || !submission->record || !submission->size) return 0;
    if (!directory) return 0;
    if (CheatsBlockProgression("matchdata record")) return 0;
    if (!write_file(directory, "match_data.mdr", (const char *)submission->record, submission->size))
        return 0;
    ((void)0);
    return 1;
}

void InstallMatchDataSink(void)
{
    SetServerMatchDataSink(matchdata_sink, NULL);
}

/* Called before old-world context/cvars/VM can be replaced. Nonblocking. */
int MatchReportPrepareReset(void)
{
    int result;
    report_transition_held = 1;
    result = WriteMatchReport("mapchange");
    PersistentEventReportBarrier(result == 0 || result == 1);
    return result == 0 || result == 1;
}

int MatchReportShutdown(unsigned timeout_ms)
{
    unsigned started = Sys_Milliseconds();
    do {
        if (MatchReportPrepareReset()) return 1;
        if ((unsigned)(Sys_Milliseconds() - started) >= timeout_ms) break;
        Sys_SleepMSec(1);
    } while (1);
    Com_PrintError(CON_CHANNEL_SERVER,
        "CoDOL match report: exit drain timed out; result NOT acknowledged durable\n");
    return 0;
}

int ResetMatchReport(void)
{
    /* No ClientDropped loop: it cleared live rows before a bounded departed
     * slot was available. Preflight reports directly from live + departed. */
    if (!MatchReportPrepareReset()) return 0;
    memset(live, 0, sizeof(live));
    memset(departed, 0, sizeof(departed));
    endless_round_checkpoint = 0;
    participation_match_finished = 0;
    report_transition_held = 0;
    memset(challenge_base_valid, 0, sizeof(challenge_base_valid));
    reward_context_valid = 0;
    return 1;
}
#endif
