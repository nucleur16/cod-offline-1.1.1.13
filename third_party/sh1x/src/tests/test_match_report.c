/* Native match-report fixture.
 *
 * Checks the two things the lobby actually depends on: the XP a row reports is
 * exactly the growth of that client's secure progression since its profile was
 * attached, and the JSON is the format-1 document rank_writeback.py accepts,
 * with a per-row uid. The engine side is a fixture: svs/level are defined here,
 * and the secure profile is stubbed down to one integer per client so the
 * arithmetic under test is the report's, not the rank table's.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "server.h"
#include "g_shared.h"
#include "match_report.h"
#include "match_gp.h"
#include "structured_schema.h"
#include "assets.h"
#include "matchdata_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* ---- engine fixture ----------------------------------------------------- */
serverStatic_t svs;
static cvar_t playlist_var, xp_rate_var, gp_rate_var;
level_locals_t level;
cvar_t *sv_maxclients;
static cvar_t maxclients_var, profile_path_var, gametype_var, mapname_var, difficulty_var;
static gclient_t clients[MAX_CLIENTS];
static char profile_directory[512];

/* One "secure profile" per client: the total XP the rank conversion would
 * report. The report never inspects the bytes, only the converted total. */
static int profile_total[MAX_CLIENTS];
static int profile_attached[MAX_CLIENTS];
static structured_schema_t fixture_schema;
static engine_table_t fixture_table;

/* The play-GP settings the report reads; an empty string is unset. */
static cvar_t gametype_var, zone_var, difficulty_var, league_var, league_index_var, mapname_var;
cvar_t *Cvar_FindVar(const char *name)
{
    if (!strcmp(name, "profilePath")) return &profile_path_var;
    if (!strcmp(name, "sv_playlistId")) return &playlist_var;
    if (!strcmp(name, "xpRatePercent")) return &xp_rate_var;
    if (!strcmp(name, "gpRatePercent")) return &gp_rate_var;
    if (!strcmp(name, "g_gametype")) return &gametype_var;
    if (!strcmp(name, "sv_zoneType")) return &zone_var;
    if (!strcmp(name, "g_difficulty")) return &difficulty_var;
    if (!strcmp(name, "gpLeague")) return &league_var;
    if (!strcmp(name, "gpLeagueIndex")) return &league_index_var;
    if (!strcmp(name, "mapname")) return &mapname_var;
    return NULL;
}
static int profile_rank[MAX_CLIENTS];

/* C1's mp/gametypescptable.csv (patch_common_gametype_mp: the matchreward
 * columns; the gameplay columns ship empty) and mp/gpleaguescaler.csv
 * (common_gametype_mp), as the zones ship them. */
static const char *cp_cells[] = {
    "MODE","win","loss","tie","kill","headshot","assist","capture","defend","defend_assist","assault","assault_assist","challenge","plant","defuse","destroy","pickup","return","kill_carrier","suicide","teamkill","matchreward_timecap","matchreward_scale_win","matchreward_scale_loss","matchreward_scale_tie","matchreward_perminute","matchreward_levelscale","matchreward_playernum",
    "NONTEAM","","","","","","","","","","","","","","","","","","","","","10","1.5","1","1.2","10","0.005","11",
    "TEAM","","","","","","","","","","","","","","","","","","","","","10","1.5","1","1.2","10","0.005","11",
    "dm","","","","","","","","","","","","","","","","","","","","","10","1.5","1","1.2","10","0.005","11",
    "coop","","","","","","","","","","","","","","","","","","","","","6","1.5","0.5","1","15","0.005","11",
    "zm","","","","","","","","","","","","","","","","","","","","","60","1.5","0.6","1.2","3","0.005","11",
};
static const char *league_cells[] = {
    "ranked","","1","1.2","1.5","1.5",
    "unranked","","0.1","0.1","0.1","0.1",
    "combat","0.5","0.1","0.1","0.25","0.1",
    "sp","1","1","1.5","","",
    "zm","0.5","1","2","2","1",
};
static engine_table_t cp_table = { "mp/gametypescptable.csv", 28, 6, cp_cells };
static engine_table_t league_table = { "mp/gpleaguescaler.csv", 6, 5, league_cells };
const structured_schema_t *ScriptProfileSchema(int buffer)
{ (void)buffer; return &fixture_schema; }
const uint8_t *ScriptProfileBlob(unsigned client, int buffer)
{
    (void)buffer;
    if (client >= MAX_CLIENTS || !profile_attached[client]) return NULL;
    return (const uint8_t *)&profile_total[client];
}
int LoadEngineStringTable(const char *name, const engine_table_t **out)
{
    if (!strcmp(name, "mp/gametypescptable.csv")) *out = &cp_table;
    else if (!strcmp(name, "mp/gpleaguescaler.csv")) *out = &league_table;
    else *out = &fixture_table;
    return 1;
}
int ProfileExperience(const uint8_t *profile, size_t size, const structured_schema_t *schema,
                            const engine_table_t *table, int *rank, int *experience)
{
    (void)size; (void)schema; (void)table;
    if (!profile) return 0;
    *rank = profile_rank[(const int *)profile - profile_total];
    *experience = *(const int *)profile;
    return 1;
}
void SetServerMatchDataSink(server_matchdata_sink_t handler, void *context)
{ (void)handler; (void)context; }
/* The challenge delta reads the live and the attached secure profile; the
 * fixture hands back the same bytes for both, which the report treats as "no
 * script edited this profile" (no challenge progress). */
const uint8_t *ClientSecureProfile(unsigned client, uint64_t *trusted_uid)
{
    (void)trusted_uid;
    if (client >= MAX_CLIENTS || !profile_attached[client]) return NULL;
    return (const uint8_t *)&profile_total[client];
}
const uint8_t *ClientSecureProfileAttached(unsigned client, uint64_t *trusted_uid)
{ return ClientSecureProfile(client, trusted_uid); }
int ProfileRecord(const uint8_t *profile, size_t size, const structured_schema_t *schema,
                        const structured_definition_t **definition,
                        const uint8_t **record, size_t *capacity)
{ (void)profile; (void)size; (void)schema; (void)definition; (void)record; (void)capacity; return 0; }
/* matchReportSpool: off, so the fixture reads the legacy single name. */
static cvar_t spool_var;
cvar_t *Cvar_RegisterBool(const char *name, qboolean value, unsigned short flags, const char *description)
{ (void)name; (void)value; (void)flags; (void)description; spool_var.boolean = qfalse; return &spool_var; }
int Q_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }

/* A stand-in secure DDL. Members are resolved BY NAME exactly as the mounted
 * mp/playersecuredata.def resolves them, so the fixture can also prove what
 * happens to a name the real DDL would not declare: "misses" is deliberately
 * absent here. logical_index is the slot in profile_stats[client][]. */
static const char *const fixture_stats[] = {
    "timePlayedTotal", "score", "kills", "deaths", "assists", "headshots",
    "totalShots", "hits", "wins", "losses", "ties", "winStreak", "killStreak",
    "escapes", "suicides", "objectives"
};
/* This client's DDL stores `kills` and `wins` as float32 - the two headline
 * numbers on the career page - so the fixture must too, or a reader that only
 * handles integers would pass here and report nothing live. */
static int fixture_stat_is_float(const char *name)
{ return !strcmp(name, "kills") || !strcmp(name, "wins"); }
#define FIXTURE_STATS (sizeof(fixture_stats)/sizeof(fixture_stats[0]))
static uint32_t profile_stats[MAX_CLIENTS][FIXTURE_STATS];
static structured_definition_t fixture_def;

static int fixture_stat_index(const char *name)
{
    unsigned i;
    for (i = 0; i < FIXTURE_STATS; ++i)
        if (!strcmp(name, fixture_stats[i])) return (int)i;
    return -1;
}
static void set_stat(unsigned client, const char *name, uint32_t value)
{
    int index = fixture_stat_index(name);
    if (index < 0) return;
    if (fixture_stat_is_float(name)) {
        float f = (float)value;
        memcpy(&profile_stats[client][index], &f, sizeof(f));
    } else profile_stats[client][index] = value;
}
int ScriptProfileView(unsigned client, int buffer,
                            const structured_definition_t **definition,
                            const uint8_t **record, size_t *capacity)
{
    (void)buffer;
    if (client >= MAX_CLIENTS || !profile_attached[client]) return 0;
    *definition = &fixture_def;
    *record = (const uint8_t *)profile_stats[client];
    *capacity = sizeof(profile_stats[client]);
    return 1;
}
int ResolveStructuredField(const structured_definition_t *definition,
                                 const structured_path_t *path, size_t count,
                                 structured_field_t *out)
{
    int index;
    (void)definition; (void)count;
    if (!path || !path->name) return 0;
    index = fixture_stat_index(path->name);
    if (index < 0) return 0;
    out->type.kind = fixture_stat_is_float(path->name) ? 8 : 0;
    out->type.index = 0;
    out->logical_index = (uint32_t)index; out->mapped_address = 0;
    return 1;
}
int StructuredReadScalar(const structured_layout_t *layout, unsigned type,
                               unsigned property, const uint8_t *record, size_t capacity,
                               uint32_t *out)
{
    (void)layout; (void)type;
    if ((size_t)(property + 1) * sizeof(uint32_t) > capacity) return 0;
    *out = ((const uint32_t *)record)[property];
    return 1;
}
time_t Com_GetRealtime(void) { return 1789500000; }
void QDECL Com_Printf(conChannel_t channel, const char *fmt, ...) { (void)channel; (void)fmt; }
void QDECL Com_PrintWarning(conChannel_t channel, const char *fmt, ...) { (void)channel; (void)fmt; }
void FS_ReplaceSeparators(char *path) { (void)path; }
void Q_strncpyz(char *dest, const char *src, int size)
{
    if (size <= 0) return;
    strncpy(dest, src, (size_t)size - 1);
    dest[size - 1] = 0;
}

/* ---- harness ------------------------------------------------------------ */
static int failures;
static void check(int condition, const char *what, const char *detail)
{
    if (condition) { printf("  ok   %s\n", what); return; }
    printf("  FAIL %s %s\n", what, detail ? detail : "");
    ++failures;
}

static char report_text[65536];
static size_t read_report(void)
{
    char path[640];
    FILE *file;
    size_t n;
    snprintf(path, sizeof(path), "%s/match_report.json", profile_directory);
    file = fopen(path, "rb");
    if (!file) return 0;
    n = fread(report_text, 1, sizeof(report_text) - 1, file);
    fclose(file);
    report_text[n] = 0;
    return n;
}
static void remove_report(void)
{
    char path[640];
    snprintf(path, sizeof(path), "%s/match_report.json", profile_directory);
    remove(path);
}
static int has(const char *needle) { return strstr(report_text, needle) != NULL; }
static void save_case(const char *name)
{
    char path[640]; FILE *f;
    snprintf(path, sizeof(path), "%s/%s.json", profile_directory, name);
    f = fopen(path, "wb");
    if (!f) { ++failures; return; }
    fwrite(report_text, 1, strlen(report_text), f); fclose(f);
}
/* Search only inside one named row's "lifetime" object: the row itself
 * already carries this-match "kills"/"deaths"/"score" keys with the same
 * names, and every other row has a lifetime object of its own. */
static int lifetime_has(const char *who, const char *needle)
{
    char key[64];
    const char *row, *start, *end;
    char window[4096];
    size_t n;
    snprintf(key, sizeof(key), "\"name\": \"%s\"", who);
    row = strstr(report_text, key);
    if (!row) return 0;
    start = strstr(row, "\"lifetime\": {");
    if (!start) return 0;
    end = strchr(start, '}');
    if (!end) return 0;
    n = (size_t)(end - start);
    if (n >= sizeof(window)) n = sizeof(window) - 1;
    memcpy(window, start, n);
    window[n] = 0;
    return strstr(window, needle) != NULL;
}

static void connect_client(unsigned slot, const char *name, uint64_t uid, int bot,
                           int total, int team)
{
    svs.clients[slot].state = CS_ACTIVE;
    svs.clients[slot].netchan.remoteAddress.type = bot ? NA_BOT : NA_IP;
    Q_strncpyz(svs.clients[slot].name, name, sizeof(svs.clients[slot].name));
    clients[slot].sess.cs.team = team;
    clients[slot].sess.kills = 0;
    clients[slot].sess.deaths = 0;
    clients[slot].sess.assists = 0;
    clients[slot].sess.score = 0;
    profile_total[slot] = total;
    profile_attached[slot] = !bot;
    /* Only an account attaches a profile. A bot never reaches
     * AutoBindClientSecureProfile (it has no uid), so the report has to
     * synthesise its row from the connected slot at write time - which is also
     * the only moment the NA_BOT test can be trusted, because NA_BOT is zero
     * and SV_DirectConnect does not set the netchan up until after the attach. */
    if (!bot)
        MatchReportClientAttached(slot, uid);
}


static int row_has(const char *who, const char *needle)
{
    const char *row = strstr(report_text, who), *end, *found;
    if (!row) return 0;
    end = strstr(row + strlen(who), "{\"slot\":");
    found = strstr(row, needle);
    return found && (!end || found < end);
}

static void participation_reset(const char *mode)
{
    unsigned i;
    ResetMatchReport();
    for (i = 0; i < MAX_CLIENTS; ++i) {
        svs.clients[i].state = CS_FREE;
        profile_attached[i] = 0;
    }
    memset(profile_stats, 0, sizeof(profile_stats));
    svs.time = 1000;
    level.time = 20000;
    level.startTime = 20000;
    gametype_var.string = (char *)mode;
    zone_var.string = (char *)mode;
    mapname_var.string = (char *)(!strcmp(mode, "zm") ? "zm_asylum_sh" : "mp_killhouse_sh");
    remove_report();
}

static void participation_advance(unsigned ms)
{
    svs.time = (int)((uint32_t)svs.time + ms);
    level.time += (int)ms;
}

static void test_participation_clock(void)
{
    printf("match report: exact independent participation clock\n");
    participation_reset("mp");
    connect_client(0, "ClockMP", 90001ULL, 0, 1000, TEAM_BLUE);
    participation_advance(1250);
    connect_client(1, "LateMP", 90002ULL, 0, 1000, TEAM_RED);
    check(WriteMatchReport("partial") == 1, "time alone can publish an exact partial report", NULL);
    read_report();
    check(row_has("ClockMP", "\"play_time_ms\": 1250") && row_has("LateMP", "\"play_time_ms\": 0"),
          "per-player joins are measured independently and valid zero is explicit", report_text);
    check(row_has("ClockMP", "\"play_time_source\": \"native_participation_clock\""),
          "participation provenance is explicit", report_text);
    check(!lifetime_has("ClockMP", "\"timePlayedTotal\""), "MP never gets a potentially overlapping lifetime fallback", report_text);
    participation_advance(750);
    check(WriteMatchReport("partial") == 1, "a second partial report carries only new time", NULL);
    read_report();
    check(row_has("ClockMP", "\"play_time_ms\": 750") && row_has("LateMP", "\"play_time_ms\": 750"),
          "partial reports partition milliseconds even after the GP timer settles", report_text);
    participation_advance(1000);
    check(WriteMatchReport("gameended") == 1, "final report freezes participation", NULL);
    read_report();
    check(row_has("ClockMP", "\"play_time_ms\": 1000") && row_has("LateMP", "\"play_time_ms\": 1000"),
          "terminal deltas complete each connected interval once", report_text);
    save_case("participation-mp-final");
    participation_advance(5000);
    ++profile_total[0];
    check(WriteMatchReport("gameended") == 1, "a later counter delta can still be exported", NULL);
    read_report();
    check(row_has("ClockMP", "\"play_time_ms\": 0") && row_has("LateMP", "\"play_time_ms\": 0"),
          "postgame and repeated terminal callbacks add zero participation", report_text);
    save_case("participation-mp-postgame");

    participation_reset("zm");
    connect_client(0, "ClockZM", 90003ULL, 0, 1000, TEAM_BLUE);
    participation_advance(600);
    check(WriteMatchReport("partial") == 1, "subsecond Zombies participation is not dropped", NULL);
    read_report();
    check(row_has("ClockZM", "\"play_time_ms\": 600") && lifetime_has("ClockZM", "\"timePlayedTotal\": 0"),
          "Zombies carries exact milliseconds and a valid whole-second zero", report_text);
    participation_advance(700);
    check(WriteMatchReport("partial") == 1, "Zombies fractional carry reaches the next second", NULL);
    read_report();
    check(row_has("ClockZM", "\"play_time_ms\": 700") && lifetime_has("ClockZM", "\"timePlayedTotal\": 1"),
          "Zombies lifetime fallback carries fractional seconds across reports", report_text);
    participation_advance(800);
    check(WriteMatchReport("gameended") == 1, "Zombies final fallback is written", NULL);
    read_report();
    check(row_has("ClockZM", "\"play_time_ms\": 800") && lifetime_has("ClockZM", "\"timePlayedTotal\": 1"),
          "Zombies lifetime seconds total floor(2100ms), with no double-credit", report_text);
    save_case("participation-zm-final");

    participation_reset("zm");
    connect_client(0, "SecureZM", 90004ULL, 0, 1000, TEAM_BLUE);
    participation_advance(2100);
    set_stat(0, "timePlayedTotal", 5);
    check(WriteMatchReport("gameended") == 1, "a real secure timer delta is still accepted", NULL);
    read_report();
    check(lifetime_has("SecureZM", "\"timePlayedTotal\": 5"), "an existing positive secure delta is preserved exactly", report_text);

    participation_reset("zm");
    connect_client(0, "RejoinZM", 90005ULL, 0, 1000, TEAM_BLUE);
    participation_advance(1100);
    MatchReportClientDropped(0);
    svs.clients[0].state = CS_FREE;
    participation_advance(900);
    connect_client(0, "RejoinZM", 90005ULL, 0, 1000, TEAM_BLUE);
    participation_advance(900);
    check(WriteMatchReport("gameended") == 1, "rejoined Zombies participant writes final result", NULL);
    read_report();
    check(row_has("RejoinZM", "\"play_time_ms\": 2000") && lifetime_has("RejoinZM", "\"timePlayedTotal\": 2"),
          "reconnect adopts connected time but excludes the disconnected interval", report_text);
    save_case("participation-zm-reconnect");

    participation_reset("mp");
    connect_client(0, "RetryMP", 90006ULL, 0, 1000, TEAM_BLUE);
    participation_advance(1500);
    profile_path_var.string = (char *)"";
    check(WriteMatchReport("gameended") == 0, "unavailable spool cannot deliver yet", NULL);
    participation_advance(9999);
    profile_path_var.string = profile_directory;
    check(WriteMatchReport("gameended") == 1, "terminal spool can retry after recovery", NULL);
    read_report();
    check(row_has("RetryMP", "\"play_time_ms\": 1500"), "failed delivery does not extend participation into postgame", report_text);

    participation_reset("mp");
    svs.time = 2147483500;
    connect_client(0, "WrapMP", 90007ULL, 0, 1000, TEAM_BLUE);
    participation_advance(500);
    check(WriteMatchReport("gameended") == 1, "signed server-clock wrap still reports", NULL);
    read_report();
    check(row_has("WrapMP", "\"play_time_ms\": 500"), "signed wrap preserves exact elapsed milliseconds", report_text);
}

int main(int argc, char **argv)
{
    const char *directory = argc > 1 ? argv[1] : ".";
    int written;

    snprintf(profile_directory, sizeof(profile_directory), "%s", directory);
    profile_path_var.string = profile_directory;
    playlist_var.integer = 6;
    xp_rate_var.integer = 25;
    gp_rate_var.integer = 10;
    gametype_var.string = (char *)"mp";
    mapname_var.string = (char *)"mp_crash";
    difficulty_var.integer = 0;
    maxclients_var.integer = MAX_CLIENTS;
    sv_maxclients = &maxclients_var;
    level.clients = clients;
    level.time = 620000;
    level.startTime = 20000;
    level.teamScores[TEAM_RED] = 750;
    level.teamScores[TEAM_BLUE] = 1000;
    remove_report();

    printf("match report: two humans and a bot\n");
    connect_client(0, "Alpha", 11354582479263516619ULL, 0, 484072, TEAM_BLUE);
    connect_client(1, "Bra^1vo\"\\", 2758876527760990337ULL, 0, 1000, TEAM_RED);
    connect_client(2, "bot0", 0, 1, 0, TEAM_RED);
    /* A subsequent playlist selection must not relabel the current match. */
    playlist_var.integer = 94;
    xp_rate_var.integer = 100;
    gp_rate_var.integer = 100;

    check(WriteMatchReport("gameended") == 0,
          "an idle match with no XP and no activity writes nothing", NULL);

    /* A match happens. _rank.gsc commits through setRankExperience, which is
     * what moves these totals. */
    clients[0].sess.kills = 9; clients[0].sess.deaths = 3;
    clients[0].sess.assists = 2; clients[0].sess.score = 1000;
    clients[1].sess.kills = 4; clients[1].sess.deaths = 7;
    clients[1].sess.assists = 1; clients[1].sess.score = 450;
    clients[2].sess.kills = 2; clients[2].sess.deaths = 10; clients[2].sess.score = 200;
    profile_total[0] = 484072 + 1500;
    profile_total[1] = 1000 + 620;

    written = WriteMatchReport("gameended");
    check(written == 1, "a match with XP is reported", NULL);
    check(read_report() > 0, "the report file exists", NULL);
    check(has("\"format\": 1"), "format 1", report_text);
    check(has("\"reason\": \"gameended\""), "the reason is carried", NULL);
    check(has("\"playlist_id\": 6") && has("\"xp_rate_percent\": 25") &&
          has("\"gp_rate_percent\": 10"), "server reward context reaches the report", report_text);
    save_case("completed-mp");
    check(has("\"team_scores\": [750, 1000]"), "team scores", NULL);
    check(has("\"uid\": \"11354582479263516619\""), "row 0 names its own account", NULL);
    check(has("\"uid\": \"2758876527760990337\""), "row 1 names its own account", NULL);
    check(has("\"total\": 1500"), "row 0 reports exactly the XP it gained", report_text);
    check(has("\"total\": 620"), "row 1 reports exactly the XP it gained", report_text);
    check(has("\"applied_live\": 0"),
          "nothing was persisted behind the lobby's back", NULL);
    check(has("\"kills\": 9") && has("\"deaths\": 3") && has("\"assists\": 2"),
          "row 0 carries its own counters", NULL);
    check(has("\"kills\": 4") && has("\"deaths\": 7"),
          "row 1 carries its own counters", NULL);
    check(has("\"name\": \"Bra^1vo\\\"\\\\\""),
          "a name with a quote and a backslash is escaped", report_text);
    check(has("\"outcome\": \"win\""), "the blue row won", NULL);
    check(has("\"outcome\": \"loss\""), "the red rows lost", NULL);
    check(has("\"bot\": true"), "the bot is present", NULL);
    check(strstr(report_text, "\"bot\": true, \"team\"") != NULL,
          "and carries no uid key", report_text);
    check(has("\"name\": \"bot0\", \"bot\": true"),
          "the bot row is synthesised from the connected slot, not from an attach",
          report_text);
    {   /* the bot row must have no xp object: count them */
        const char *p = report_text; int xp = 0;
        while ((p = strstr(p, "\"xp\":")) != NULL) { ++xp; ++p; }
        check(xp == 2, "exactly the two human rows carry xp", NULL);
    }

    printf("match report: consecutive reports partition the match\n");
    check(WriteMatchReport("timelimit") == 0,
          "an immediate second report has nothing to add", NULL);
    profile_total[0] += 250;
    clients[0].sess.kills = 11;
    check(WriteMatchReport("timelimit") == 1, "more XP reports again", NULL);
    read_report();
    check(has("\"total\": 250"), "and reports only the new XP, never the first 1500",
          report_text);
    check(has("\"kills\": 2"), "and only the kills since the last report", report_text);

    printf("match report: a departed player is still reported\n");
    profile_total[1] += 80;
    MatchReportClientDropped(1);
    svs.clients[1].state = CS_FREE;
    profile_attached[1] = 0;
    check(MatchReportHumansConnected() == 1, "one human is still in", NULL);
    check(WriteMatchReport("disconnect") == 1, "the leaver is reported", NULL);
    read_report();
    check(has("\"uid\": \"2758876527760990337\""), "with its own uid", report_text);
    check(has("\"total\": 80"), "and the XP it earned after the last report", report_text);

    printf("match report: a reconnect is one row, not two\n");
    connect_client(1, "Bravo", 2758876527760990337ULL, 0, 1700, TEAM_RED);
    profile_total[1] = 1700 + 90;
    clients[1].sess.kills = 1;
    check(WriteMatchReport("gameended") == 1, "the rejoined player is reported", NULL);
    read_report();
    {
        const char *first = strstr(report_text, "\"uid\": \"2758876527760990337\"");
        check(first && !strstr(first + 1, "\"uid\": \"2758876527760990337\""),
              "exactly one row for that account", report_text);
    }
    check(has("\"total\": 90"), "carrying only the new XP", report_text);

    printf("match report: career counters are measured, not re-counted\n");
    {
        /* The scripts maintain the secure profile through setPlayerData; this
         * module only measures it, the same way it measures experience. */
        unsigned slot = 5;
        svs.clients[slot].state = CS_ACTIVE;
        svs.clients[slot].netchan.remoteAddress.type = NA_IP;
        Q_strncpyz(svs.clients[slot].name, "Career", sizeof(svs.clients[slot].name));
        clients[slot].sess.cs.team = TEAM_BLUE;
        clients[slot].sess.kills = 0; clients[slot].sess.deaths = 0;
        clients[slot].sess.assists = 0; clients[slot].sess.score = 0;
        profile_total[slot] = 100000;
        profile_attached[slot] = 1;
        set_stat(slot, "timePlayedTotal", 3600);
        set_stat(slot, "kills", 400);
        set_stat(slot, "score", 90000);
        set_stat(slot, "hits", 2000);
        set_stat(slot, "totalShots", 8000);
        set_stat(slot, "killStreak", 7);
        set_stat(slot, "wins", 40);   /* float32 in this DDL, like kills */
        MatchReportClientAttached(slot, 1122334455667788990ULL);

        /* A match happens: the scripts move the profile. */
        set_stat(slot, "timePlayedTotal", 3600 + 540);
        set_stat(slot, "kills", 400 + 9);
        set_stat(slot, "score", 90000 + 1250);
        set_stat(slot, "hits", 2000 + 31);
        set_stat(slot, "totalShots", 8000 + 120);
        set_stat(slot, "killStreak", 11);
        set_stat(slot, "wins", 41);
        clients[slot].sess.kills = 9; clients[slot].sess.score = 1250;
        profile_total[slot] = 100000 + 900;

        check(WriteMatchReport("gameended") == 1, "the career match is reported", NULL);
        read_report();
        check(has("\"lifetime\": {"), "the row carries a lifetime object", report_text);
        check(has("\"timePlayedTotal\": 540"), "time played is the delta, not the total",
              report_text);
        check(has("\"kills\": 9") && has("\"score\": 1250"),
              "kills and score are deltas", report_text);
        check(has("\"hits\": 31") && has("\"totalShots\": 120"),
              "accuracy inputs are deltas", report_text);
        check(has("\"killStreak\": 4"),
              "a best-streak member reports its rise this match", report_text);
        check(lifetime_has("Career", "\"wins\": 1") &&
              lifetime_has("Career", "\"kills\": 9"),
              "float32 members (kills, wins) are read, not refused as non-integers",
              report_text);
        check(!has("\"bombPlants\""),
              "a member this DDL does not declare is simply absent", report_text);
        check(!lifetime_has("Career", "\"deaths\""),
              "and an unchanged counter is omitted rather than sent as zero", report_text);

        /* Partitioning applies to the counters too. */
        check(WriteMatchReport("timelimit") == 0,
              "an immediate second report owes nothing", NULL);
        set_stat(slot, "kills", 400 + 12);
        clients[slot].sess.kills = 12;
        check(WriteMatchReport("timelimit") == 1, "more kills report again", NULL);
        read_report();
        check(lifetime_has("Career", "\"kills\": 3"),
              "carrying only the three since the last report", report_text);
        MatchReportClientDropped(slot);
        svs.clients[slot].state = CS_FREE;
        profile_attached[slot] = 0;
    }

    printf("match report: an attach before Netchan_Setup is still a human\n");
    {
        /* THE 18:55 LIVE BUG. NA_BOT is zero and so is NA_BAD, and
         * SV_DirectConnect attaches the secure profile at sv_client.c:505 but
         * does not call Netchan_Setup until :569, so a connecting human reads
         * as a bot for the whole of the attach. The server logged
         * "nothing owed ... 1 row(s), 0 human" and never wrote a report. */
        unsigned slot = 4;
        svs.clients[slot].state = CS_ACTIVE;
        svs.clients[slot].netchan.remoteAddress.type = NA_BOT;   /* not set up yet */
        Q_strncpyz(svs.clients[slot].name, "Late", sizeof(svs.clients[slot].name));
        clients[slot].sess.cs.team = TEAM_BLUE;
        clients[slot].sess.kills = 0; clients[slot].sess.deaths = 0;
        clients[slot].sess.assists = 0; clients[slot].sess.score = 0;
        profile_total[slot] = 5000;
        profile_attached[slot] = 1;
        MatchReportClientAttached(slot, 8765909783858705198ULL);
        check(MatchReportHumansConnected() == 1,
              "an attached account counts as human even with a bot-looking netchan", NULL);
        svs.clients[slot].netchan.remoteAddress.type = NA_IP;    /* Netchan_Setup ran */
        clients[slot].sess.kills = 3; clients[slot].sess.score = 300;
        profile_total[slot] = 5000 + 410;
        check(WriteMatchReport("gameended") == 1,
              "and its match is reported, not discarded as a bot", NULL);
        read_report();
        check(has("\"uid\": \"8765909783858705198\"") &&
              has("\"total\": 410"),
              "with its own uid and its own XP", report_text);
        MatchReportClientDropped(slot);
        svs.clients[slot].state = CS_FREE;
        profile_attached[slot] = 0;
    }

    printf("match report: a map change cannot clobber an unconsumed report\n");
    {
        /* The lobby has not fetched the last report yet. SV_ClearServer resets
         * the store on the way into the next map; nothing is owed, so the
         * document on disk must survive untouched. */
        static char before[sizeof(report_text)];
        check(read_report() > 0, "a report is waiting", NULL);
        memcpy(before, report_text, sizeof(before));
        ResetMatchReport();
        read_report();
        check(!strcmp(before, report_text),
              "the waiting report is byte-identical after the reset", report_text);
    }
    connect_client(0, "Alpha", 11354582479263516619ULL, 0, 700000, TEAM_BLUE);

    printf("match report: a verified Zombies extraction\n");
    {
        unsigned slot = 6;
        gametype_var.string = (char *)"zm";
        mapname_var.string = (char *)"zm_prototype_sh";
        difficulty_var.integer = 1;
        connect_client(slot, "Zombies", 1122334455667788990ULL, 0, 1000, TEAM_FREE);
        level.time = 620123;
        check(MatchReportZombiesResult(slot, 1, 1, 13, 13, 4, 3, 42, 1) == 1,
              "a finished extraction is recorded for the player", NULL);
        check(MatchReportZombiesResult(slot, 1, 4, 13, 13, 4, 3, 42, 1) == 0,
              "invalid difficulty cannot be recorded", NULL);
        level.time = 626999;
        check(MatchReportZombiesResult(slot, 1, 1, 13, 13, 4, 9, 43, 0) == 1,
              "end-game refreshes final counters after the flight", NULL);
        check(MatchReportZombiesResult(slot, 0, 1, 13, 13, 4, 9, 43, 1) == 0,
              "a loss cannot supply extraction timing", NULL);
        check(MatchReportZombiesResult(slot, 1, 1, 13, 13, -2, 9, 43, 0) == 0,
              "invalid counters are refused", NULL);
        check(WriteMatchReport("gameended") == 1,
              "the final result is reported even without XP", NULL);
        read_report();
        check(has("\"gametype\": \"zm\"") && has("\"map\": \"zm_prototype_sh\""),
              "mode and map identify the leaderboard", report_text);
        check(has("\"zombies\": {\"final\": true, \"extracted\": true, \"difficulty\": 1, \"round\": 13, \"round_goal\": 13, \"time_s\": 600, \"time_ms\": 600123, \"time_source\": \"extraction_liftoff\", \"downs\": 4, \"revives\": 9, \"headshots\": 43}"),
              "first extraction time is frozen and final counters are carried", report_text);
        save_case("zombies-extraction-counters");
        check(MatchReportZombiesResult(slot, 1, 1, 13, 13, 4, 9, 43, 0) == 1,
              "a post-report final callback is safely accepted", NULL);
        check(WriteMatchReport("gameended") == 0,
              "the extraction cannot be counted twice after repeated final callbacks", NULL);
        ++clients[0].sess.kills;
        check(WriteMatchReport("gameended") == 1,
              "another participant can still publish later counter deltas", NULL);
        read_report();
        check(has("\"zombies\": {\"final\": false, \"extracted\": true"),
              "the earlier extraction is explicitly non-final in later mixed-player reports", report_text);
        save_case("zombies-later-partial-delta");
        MatchReportClientDropped(slot);
        svs.clients[slot].state = CS_FREE;
        profile_attached[slot] = 0;
        remove_report();
        connect_client(slot, "LegacyZM", 1122334455667788991ULL, 0, 1000, TEAM_FREE);
        check(MatchReportZombiesResult(slot, 1, 1, 13, 13, -1, -1, -1, 0) == 1,
              "legacy result is accepted without fabricated timing or counters", NULL);
        check(WriteMatchReport("gameended") == 1, "legacy result writes", NULL);
        read_report();
        check(has("\"time_s\": null, \"time_ms\": null, \"time_source\": null, \"downs\": null, \"revives\": null, \"headshots\": null"),
              "unknown fields are null, not invented zeroes or report-end times", report_text);
        save_case("zombies-legacy-unknown");
        MatchReportClientDropped(slot);
        svs.clients[slot].state = CS_FREE;
        profile_attached[slot] = 0;
        level.time = 620000;
        gametype_var.string = (char *)"mp";
        mapname_var.string = (char *)"mp_crash";
    }

    printf("match report: a coop mission result\n");
    {
        /* _coopmission.gsc records the result per player at the mission's
         * end; exitLevel then writes the report before the lobby leave. A
         * mission with no XP and no kills is still owed: the completion is
         * what unlocks the next mission in the lobby. */
        unsigned slot = 5;
        connect_client(slot, "Coop", 18293260893631942602ULL, 0, 9000, TEAM_FREE);
        check(MatchReportCoopMission(slot, "coop_training_mp_killhouse_sh", 1) == 1,
              "a completion is recorded on an account row", NULL);
        check(MatchReportCoopMission(slot, "coop_training_mp_killhouse_sh", 0) == 1,
              "a later failure of the same mission is accepted", NULL);
        check(MatchReportCoopMission(slot, "bad name\"", 1) == 0,
              "a name that is not a playlist name is refused", NULL);
        check(MatchReportCoopMission(2, "coop_training_mp_killhouse_sh", 1) == 0,
              "a bot has no row to record on", NULL);
        check(WriteMatchReport("coopmission") == 1,
              "a mission result alone is worth a report", NULL);
        read_report();
        check(has("\"reason\": \"coopmission\""), "the coop reason is carried", report_text);
        check(has("\"uid\": \"18293260893631942602\""), "the coop row names its account", report_text);
        check(has("\"coop_mission_completed\": \"coop_training_mp_killhouse_sh\""),
              "the completion is in the row's progression, not downgraded by the failure",
              report_text);
        check(!has("coop_mission_failed"), "no failure beside the completion", report_text);
        check(WriteMatchReport("coopmission") == 0,
              "a second report does not repeat the completion", report_text);

        check(MatchReportCoopMission(slot, "coop_prolog_mp_crash_intro", 0) == 1,
              "a failure is recorded", NULL);
        check(WriteMatchReport("coopmission") == 1, "and reported", NULL);
        read_report();
        check(has("\"coop_mission_failed\": \"coop_prolog_mp_crash_intro\"") &&
              !has("coop_mission_completed"),
              "as coop_mission_failed only", report_text);
        MatchReportClientDropped(slot);
        svs.clients[slot].state = CS_FREE;
        profile_attached[slot] = 0;
    }

    printf("match report: field order GP\n");
    {
        /* _fieldorders.gsc credits an FO_GP reward through codol_creditGP;
         * the lobby deposits progression.gp in the GP wallet. GP alone is
         * owed, survives a reconnect, and is carried exactly once. */
        unsigned slot = 5;
        connect_client(slot, "Golf", 1895515831414337566ULL, 0, 9000, TEAM_FREE);
        check(MatchReportCreditGP(slot, 50) == 1, "GP is recorded on an account row", NULL);
        check(MatchReportCreditGP(slot, 0) == 0 && MatchReportCreditGP(slot, -5) == 0,
              "a zero or negative amount is refused", NULL);
        check(MatchReportCreditGP(2, 50) == 0, "a bot has no row to record on", NULL);
        MatchReportClientDropped(slot);
        connect_client(slot, "Golf", 1895515831414337566ULL, 0, 9000, TEAM_FREE);
        check(MatchReportCreditGP(slot, 20) == 1, "and more after a reconnect", NULL);
        check(WriteMatchReport("gameended") == 1, "GP alone is worth a report", NULL);
        read_report();
        check(has("\"progression\": {\"gp\": 70, \"gp_fixed\": {\"field_orders\": 70}}"),
              "the row's progression carries the match's GP across the reconnect", report_text);
        check(WriteMatchReport("gameended") == 0, "a second report does not repeat it", NULL);
        MatchReportClientDropped(slot);
        svs.clients[slot].state = CS_FREE;
        profile_attached[slot] = 0;
    }

    printf("match report: fixed GP by source\n");
    {
        /* Challenge tier GP (mp/allChallengesTable.csv CP1..CP9) is a fixed
         * award like field order GP: credited at its table amount, summed into
         * progression.gp for the deposit, and kept apart by source. A player
         * who leaves takes it with them ("left" reports it). */
        unsigned slot = 6;
        connect_client(slot, "Hotel", 8769660084610037209ULL, 0, 9000, TEAM_FREE);
        check(MatchReportGPSource("challenge") == GP_SOURCE_CHALLENGE &&
              MatchReportGPSource("fieldorder") == GP_SOURCE_FIELD_ORDER &&
              MatchReportGPSource(NULL) == GP_SOURCE_FIELD_ORDER &&
              MatchReportGPSource("bonus") == -1,
              "source names resolve, unknown ones are refused", NULL);
        check(MatchReportCreditFixedGP(slot, GP_SOURCE_CHALLENGE, 150) == 1,
              "challenge GP is recorded", NULL);
        check(MatchReportCreditFixedGP(slot, GP_SOURCES, 10) == 0 &&
              MatchReportCreditFixedGP(slot, -1, 10) == 0,
              "an out-of-range source is refused", NULL);
        check(MatchReportCreditGP(slot, 25) == 1, "field order GP alongside it", NULL);
        /* The end-of-game summary (_codol_aar.gsc, codol_matchGP) reads the
         * live row before the report carries it: the total is what
         * progression.gp will deposit, and each fixed source on its own. */
        check(MatchReportGP(slot, -1) == 175, "the summary reads the deposit total", NULL);
        check(MatchReportGP(slot, GP_SOURCE_FIELD_ORDER) == 25 &&
              MatchReportGP(slot, GP_SOURCE_CHALLENGE) == 150,
              "and each fixed source", NULL);
        check(MatchReportGP(slot, GP_SOURCES) == 0 && MatchReportGP(2, -1) == 0 &&
              MatchReportGP(MAX_CLIENTS, -1) == 0,
              "an unknown source, a bot and a bad slot read 0", NULL);
        MatchReportClientDropped(slot);
        svs.clients[slot].state = CS_FREE;
        profile_attached[slot] = 0;
        check(WriteMatchReport("left") == 1, "a departed player's GP is owed", NULL);
        read_report();
        check(has("\"gp\": 175, \"gp_fixed\": {\"field_orders\": 25, \"challenges\": 150}}"),
              "the deposit is the sum, split by source", report_text);
        check(WriteMatchReport("gameended") == 0, "and carried once", NULL);
    }

    printf("match report: GP for playing (the scaled part)\n");
    {
        /* GP = perminute x min(minutes, timecap) x outcome scale
         *      x (1 + levelscale x level) x league x activity gp_scale,
         * truncated. MP row: 10 / 10 min / win 1.5 tie 1.2 loss 1 / 0.005. */
        match_gp_params_t params;
        char path[600];
        FILE *file;
        int gp, events;
        struct tm now;
        level.teamScores[TEAM_RED] = 750;
        level.teamScores[TEAM_BLUE] = 1000;
        gametype_var.string = (char *)"war";       /* not in the table: TEAM */
        zone_var.string = (char *)"mp";
        mapname_var.string = (char *)"mp_killhouse_sh";

        /* The user's match: 369 s, a win, level 1. */
        svs.time = 1000;
        connect_client(7, "India", 2758876527760990337ULL, 0, 9000, TEAM_BLUE);
        svs.time += 369000;
        check(MatchReportGP(7, -1) == 92,
              "the summary reads 10 x 6.15 min x 1.5 x 1.005 = 92 before the report", NULL);
        check(MatchReportCreditFixedGP(7, GP_SOURCE_CHALLENGE, 25) == 1,
              "a fixed challenge award alongside", NULL);
        check(MatchReportGP(7, -1) == 117 && MatchReportGP(7, GP_SOURCE_CHALLENGE) == 25,
              "the total is play plus fixed, so ui_eog_r7 (total - fixed) is the play GP", NULL);
        check(WriteMatchReport("gameended") == 1, "the match is reported", NULL);
        read_report();
        check(has("\"gp\": 117, \"gp_play\": 92, \"gp_fixed\": {\"challenges\": 25}}"),
              "the deposit equals what the summary showed", report_text);
        check(MatchReportGP(7, -1) == 0, "a carried match pays nothing more", NULL);
        svs.time += 30000;
        MatchReportClientDropped(7);
        svs.clients[7].state = CS_FREE;
        profile_attached[7] = 0;
        check(WriteMatchReport("disconnect") == 0,
              "leaving in the intermission does not pay the match twice", report_text);

        /* Timecap, level and the loss scale: 15 minutes, level 10, a loss. */
        svs.time = 5000;
        profile_rank[8] = 9;
        connect_client(8, "Juliet", 8769660084610037209ULL, 0, 9000, TEAM_RED);
        svs.time += 15 * 60000;
        check(MatchReportGP(8, -1) == 105, "10 x 10 (capped) x 1 x 1.05 = 105", NULL);
        WriteMatchReport("gameended");
        MatchReportClientDropped(8);
        svs.clients[8].state = CS_FREE;
        profile_attached[8] = 0;
        profile_rank[8] = 0;
        WriteMatchReport("disconnect");

        /* A leaver is paid for its minutes at the loss scale, and fixed GP is
         * never scaled: an active x2 event doubles only the play part. */
        snprintf(path, sizeof(path), "%s/activities.info", profile_directory);
        file = fopen(path, "wb");
        fputs("; test\n[double]\nvalid_from=20000101\nvalid_until=29991231\n"
              "daily_begin=0:0\ndaily_end=23:59\nxp_scale=2\ngp_scale=2\nmaps=all\n"
              "modes=all\nmatch=all\n", file);
        fclose(file);
        svs.time = 1000;
        connect_client(9, "Kilo", 1895515831414337566ULL, 0, 9000, TEAM_BLUE);
        MatchReportCreditGP(9, 30);
        svs.time += 120000;
        MatchReportClientDropped(9);
        svs.clients[9].state = CS_FREE;
        profile_attached[9] = 0;
        check(WriteMatchReport("disconnect") == 1, "the leaver is reported", NULL);
        read_report();
        check(has("\"gp\": 70, \"gp_play\": 40, \"gp_fixed\": {\"field_orders\": 30}}"),
              "10 x 2 min x 1 (loss) x 1.005 x 2 = 40 play, 30 fixed unscaled", report_text);
        remove(path);

        /* A reconnect pays the whole stay once, at the real outcome. */
        svs.time = 1000;
        connect_client(10, "Lima", 1127913377208771177ULL, 0, 9000, TEAM_BLUE);
        svs.time += 60000;
        MatchReportClientDropped(10);
        svs.clients[10].state = CS_FREE;
        svs.time += 30000;
        connect_client(10, "Lima", 1127913377208771177ULL, 0, 9000, TEAM_BLUE);
        svs.time += 60000;
        check(WriteMatchReport("gameended") == 1, "reported at the end", NULL);
        read_report();
        check(has("\"gp\": 30, \"gp_play\": 30}"),
              "2 min in two stays x 1.5 x 1.005 = 30, the first stay's leaver GP dropped",
              report_text);
        MatchReportClientDropped(10);
        svs.clients[10].state = CS_FREE;
        profile_attached[10] = 0;
        WriteMatchReport("disconnect");

        /* Free-for-all: the strictly best score wins. */
        gametype_var.string = (char *)"dm";
        svs.time = 1000;
        connect_client(11, "Mike", 13502897353324629401ULL, 0, 9000, TEAM_FREE);
        connect_client(12, "botdm", 0, 1, 0, TEAM_FREE);
        clients[11].sess.score = 500;
        clients[12].sess.score = 500;
        svs.time += 60000;
        check(MatchReportGP(11, -1) == 12, "a shared best score is a tie: 10 x 1.2 x 1.005", NULL);
        clients[11].sess.score = 600;
        check(MatchReportGP(11, -1) == 15, "the best score wins: 10 x 1.5 x 1.005", NULL);
        WriteMatchReport("gameended");
        MatchReportClientDropped(11);
        svs.clients[11].state = CS_FREE;
        svs.clients[12].state = CS_FREE;
        profile_attached[11] = 0;
        WriteMatchReport("disconnect");

        /* Coop: its own row, league row "sp" at the room difficulty, and the
         * mission result as the outcome. 4 min x 15 x 1.5 x 1.005 x 1.5. */
        zone_var.string = (char *)"coop";
        difficulty_var.string = (char *)"2";
        svs.time = 1000;
        connect_client(13, "November", 15574075864353841164ULL, 0, 9000, TEAM_BLUE);
        svs.time += 4 * 60000;
        check(MatchReportGP(13, -1) == 45, "an unfinished mission pays the loss scale (0.5)", NULL);
        MatchReportCoopMission(13, "coop_spec1_mp_killhouse_sh", 1);
        check(MatchReportGP(13, -1) == 135, "a completed mission pays the win scale", NULL);
        WriteMatchReport("coopmission");
        MatchReportClientDropped(13);
        svs.clients[13].state = CS_FREE;
        profile_attached[13] = 0;
        WriteMatchReport("disconnect");
        zone_var.string = (char *)"mp";
        difficulty_var.string = NULL;
        gametype_var.string = NULL;
        svs.time = 0;

        /* The pure pieces. */
        check(MatchGPParams(&cp_table, "zm", &params) && params.timecap_minutes == 60 &&
              params.perminute == 3, "a mode's row is read by header name", NULL);
        check(!MatchGPParams(&cp_table, "war", &params), "an unlisted mode has no row", NULL);
        check(MatchGPLeagueScale(&league_table, "ranked", 0) == 1.0 &&
              MatchGPLeagueScale(&league_table, "zm", 2) == 2.0 &&
              MatchGPLeagueScale(&league_table, "sp", 3) == 1.0 &&
              MatchGPLeagueScale(&league_table, "nothere", 1) == 1.0,
              "league: ranked is 1, a cell by index, an empty cell or missing row is 1", NULL);
        gp = MatchGPAmount(&params, 0, GP_OUTCOME_WIN, 1, 1.0, 1.0);
        check(gp == 0, "no time, no GP", NULL);
        memset(&now, 0, sizeof(now));
        now.tm_year = 2026 - 1900; now.tm_mon = 9; now.tm_mday = 13;
        now.tm_hour = 20; now.tm_min = 30;
        {
            static const char ini[] =
                "[evening]\nvalid_from=20261001\nvalid_until=20261031\ndaily_begin=20:00\n"
                "daily_end=21:00\ngp_scale=1.5\nmaps=mp_rust,mp_killhouse_sh\nmodes=all\nmatch=playlist\n"
                "[morning]\nvalid_from=20261001\nvalid_until=20261031\ndaily_begin=8:00\n"
                "daily_end=9:00\ngp_scale=3\nmaps=all\nmodes=all\nmatch=all\n"
                "[rooms]\nvalid_from=20261001\nvalid_until=20261031\ndaily_begin=0:00\n"
                "daily_end=23:59\ngp_scale=3\nmaps=all\nmodes=all\nmatch=room\n"
                "[once]\nvalid_from=20261001\nvalid_until=20261031\ndaily_begin=0:00\n"
                "daily_end=23:59\ngp_scale=3\nmaps=all\nmodes=all\nmatch=all\ndaily_once=yes\n"
                "[dmonly]\nvalid_from=20261001\nvalid_until=20261031\ndaily_begin=0:00\n"
                "daily_end=23:59\ngp_scale=3\nmaps=all\nmodes=dm\nmatch=all\n"
                "[expired]\nvalid_from=20250101\nvalid_until=20251231\ndaily_begin=0:00\n"
                "daily_end=23:59\ngp_scale=3\nmaps=all\nmodes=all\nmatch=all\n";
            double scale = ActivityGPScale(ini, sizeof(ini) - 1, &now, "mp_killhouse_sh",
                                                 "war", 1, &events);
            check(scale == 1.5 && events == 1,
                  "only the in-window playlist event for this map applies; room-only, "
                  "daily_once, another mode and an expired event do not", NULL);
            check(ActivityGPScale(";only a comment\n", 16, &now, "mp_rust", "war", 1, &events)
                  == 1.0 && events == 0, "no events is scale 1", NULL);
        }
    }

    printf("match report: fixed rank-up GP\n");
    {
        /* 200 x the new displayed level, for every level crossed, once. */
        unsigned slot = 14;
        check(RankUpGP(1, 2) == 400 && RankUpGP(9, 10) == 2000 &&
              RankUpGP(4, 6) == 200 * 5 + 200 * 6 && RankUpGP(6, 6) == 0 &&
              RankUpGP(7, 3) == 0,
              "level 2 pays 400, level 10 2000, 4 -> 6 pays both levels", NULL);
        check(MatchReportGPSource("rankup") == GP_SOURCE_RANKUP &&
              MatchReportGPSource("rankups") == GP_SOURCE_RANKUP,
              "codol_matchGP( \"rankup\" ) names the source", NULL);
        svs.time = 0;
        profile_rank[slot] = 3;                       /* level 4 at attach */
        connect_client(slot, "Oscar", 13502897353324629402ULL, 0, 9000, TEAM_BLUE);
        check(MatchReportCreditRankUp(slot, 4, 6) == 2200, "a 4 -> 6 promotion pays 1000 + 1200", NULL);
        check(MatchReportCreditRankUp(slot, 4, 6) == 0 &&
              MatchReportCreditRankUp(slot, 5, 6) == 0,
              "the same levels are not paid again", NULL);
        check(MatchReportCreditRankUp(slot, 1, 7) == 1400,
              "a caller's low old level pays only above the row's mark", NULL);
        check(MatchReportCreditRankUp(slot, 7, 3) == 0 &&
              MatchReportCreditRankUp(MAX_CLIENTS, 1, 5) == 0 &&
              MatchReportCreditRankUp(2, 1, 5) == 0,
              "a demotion, a bad slot and a bot (no row) pay nothing", NULL);
        /* A level the profile reached without codol_creditRankUpGP (coop,
         * zombies) is caught up from the profile when the row is read. */
        profile_rank[slot] = 7;                       /* level 8 */
        check(MatchReportGP(slot, GP_SOURCE_RANKUP) == 2200 + 1400 + 1600,
              "the profile's level 8 is caught up: +1600", NULL);
        check(MatchReportCreditRankUp(slot, 7, 8) == 0,
              "and the script's later credit of it pays nothing", NULL);
        /* Reconnect with a STALE profile (the lobby has not folded yet): the
         * new stay re-crosses levels 5..8 and none of them pays again. */
        MatchReportClientDropped(slot);
        svs.clients[slot].state = CS_FREE;
        profile_attached[slot] = 0;
        profile_rank[slot] = 3;
        connect_client(slot, "Oscar", 13502897353324629402ULL, 0, 9000, TEAM_BLUE);
        check(MatchReportCreditRankUp(slot, 4, 8) == 0,
              "a reconnect keeps the mark: levels 5..8 are paid already", NULL);
        profile_rank[slot] = 8;
        check(MatchReportCreditRankUp(slot, 8, 9) == 1800, "level 9 is new: 1800", NULL);
        check(MatchReportGP(slot, GP_SOURCE_RANKUP) == 7000,
              "the row carried the first stay's rank-up GP", NULL);
        check(WriteMatchReport("gameended") == 1, "the match is reported", NULL);
        read_report();
        check(has("\"rankups\": 7000}"), "gp_fixed.rankups carries it", report_text);
        check(has("\"gp\": 7000, \"gp_fixed\""),
              "and progression.gp, the deposit, includes it unscaled", report_text);
        check(MatchReportCreditRankUp(slot, 8, 9) == 0 &&
              MatchReportGP(slot, GP_SOURCE_RANKUP) == 0,
              "a level stays paid after the report carried its GP", NULL);
        MatchReportClientDropped(slot);
        svs.clients[slot].state = CS_FREE;
        profile_attached[slot] = 0;
        check(WriteMatchReport("disconnect") == 0, "leaving afterwards repays nothing", report_text);

        /* An unreadable attach level never pays the whole ladder: the first
         * level read becomes the mark. */
        slot = 15;
        svs.clients[slot].state = CS_ACTIVE;
        svs.clients[slot].netchan.remoteAddress.type = NA_IP;
        Q_strncpyz(svs.clients[slot].name, "Papa", sizeof(svs.clients[slot].name));
        clients[slot].sess.cs.team = TEAM_RED;
        profile_total[slot] = 9000;
        profile_attached[slot] = 0;
        MatchReportClientAttached(slot, 15574075864353841165ULL);
        profile_attached[slot] = 1;
        profile_rank[slot] = 19;                      /* level 20 */
        check(MatchReportGP(slot, GP_SOURCE_RANKUP) == 0,
              "a row with no readable attach level takes level 20 as its mark", NULL);
        check(MatchReportCreditRankUp(slot, 20, 21) == 4200, "then level 21 pays 4200", NULL);
        MatchReportClientDropped(slot);
        svs.clients[slot].state = CS_FREE;
        profile_attached[slot] = 0;
        profile_rank[slot] = 0;
        profile_rank[14] = 0;
        check(WriteMatchReport("left") == 1, "a leaver takes its rank-up GP", NULL);
        read_report();
        check(has("\"rankups\": 4200}"), "and the report carries it", report_text);
    }

    test_participation_clock();

    printf("match report: no profile path means no report\n");
    ResetMatchReport();
    gametype_var.string = (char *)"zm";
    mapname_var.string = (char *)"zm_prototype_sh";
    connect_client(0, "Survivor", 11354582479263516619ULL, 0, 1000, TEAM_BLUE);
    clients[0].sess.kills = 30;
    check(WriteMatchReport("gameended") == 1, "terminal Zombies report is written", NULL);
    read_report();
    check(has("\"completed\": true"), "terminal participation reaches the player row", report_text);
    check(has("\"playlist_id\": 94") && has("\"gp_rate_percent\": 100"),
          "new match captures the new reward rates", report_text);
    save_case("completed-zm");
    profile_path_var.string = (char *)"";
    check(WriteMatchReport("gameended") == 0,
          "an unset profilePath writes nothing", NULL);
    profile_path_var.string = profile_directory;

    /* Leave the last document on disk: the companion Python check feeds it to
     * the lobby's own rank_writeback.py so both ends agree on the schema. */
    ResetMatchReport();
    if (failures) { printf("\nFAILED: %d check(s)\n", failures); return 1; }
    printf("\nPASS: match report XP deltas, per-row uid, escaping, departure, reconnect and reset\n");
    return 0;
}
