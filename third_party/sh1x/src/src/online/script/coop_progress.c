#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Coop (campaign / Spec Ops) mission end.
 *
 * MISSION RESULT. _coopmission.gsc ends a mission complete or failed. For each
 * player still in the match it calls
 *
 *     player codol_reportCoopMission( getDvar( "sv_sessionMap" ), success );
 *
 * which records the playlist name (coop_training_mp_killhouse_sh, ...) on the
 * player's match-report row (match_report.c). The report carries it as
 * progression.coop_mission_completed or progression.coop_mission_failed, and
 * the lobby (alpha0022 career/progression.py) records a completion in the
 * player's profile and unlocks the next mission. The builtin only records in a
 * coop match, so an mp or zm script cannot touch campaign progress.
 *
 * HOW RETAIL CODOL KEEPS CAMPAIGN PROGRESS (MEASURED in the shipped tables):
 * a mission's completion is its storymission challenge in the secure
 * profile's challenges[] (mp/allChallengesTable.csv: 800
 * so_coop_training_mp_killhouse_sh, 801 so_coop_take_mp_fav_tropical, 802
 * so_coop_defense_mp_crash_tropical, 803 so_coop_take_mp_estate_tropical, ...;
 * Spec Ops missions have so_<mission>_2 / _3, one per difficulty), and the
 * next mission is unlocked by the mp/unlockFlags.csv rows gated on that
 * challenge (652 uf_ch_coop_take_mp_fav_tropical and 662
 * uf_ch_coop_spec1_mp_killhouse_sh on training, 653 on take fav, 654 on
 * defense crash, ...). C1's coop map menu tests the flag named in
 * mp/comaplist.csv's unlock column through IsUnlockFlagSet (0x0048D220).
 * The lobby derives the flags from the challenge it credits; but C1 copies
 * the lobby's task-30 flag set only once per process (CODOL_UNLOCK_REFRESH_
 * 20260929.md), so a completion also sends the earned flags to the player
 * right away as `s cg_unlockFlagSet "<id>|<id>"` (CG_SetUnlockFlagsFromString
 * 0x00468900, append-only), ahead of the leave on the same reliable stream,
 * and the campaign menu the player returns to already shows the next mission
 * unlocked.
 *
 * LOBBY RETURN. exitLevel already returns every C1 client to the lobby with the
 * reliable `t "EXE_MATCHENDED"` (script_exitlevel_trace.c), exactly as an
 * mp match ends. What it did next was ExitLevel's map rotation, which for a
 * coop match is SV_CategoryRotate: with sv_mapRotation_coop empty it
 * replayed the same mission for nobody, and the server went on loading coop
 * levels nobody had asked for. A coop mission is chosen in the lobby's
 * campaign menu, one mission per Ready press, so in a coop match with the
 * lobby return on:
 *
 *   1. the match report is written before the leave (the mission results must
 *      be in the lobby's spool before the client is back in the front end and
 *      fetches its profile; a parked server has no map change to write it at);
 *   2. the clients get the same `t "EXE_MATCHENDED"` as an mp match;
 *   3. ExitLevel loads nothing: the finished level stays as it is, and
 *      sh1x_status reports map=none running=0 parked=1, so the lobby's
 *      placement re-points this server with a fresh `map` for whoever presses
 *      Ready next -- the same mission again, the next one, or an mp match.
 *
 * exitToLobby 0 (a dedicated rotation, clients stay connected) keeps the
 * coop rotation unchanged. mp and zm never reach any of this. */
#include "coop_progress.h"

#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "server.h"
#include "g_shared.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include "match_report.h"

#include "assets.h"
#include <stdlib.h>
#include <string.h>

const char *SV_AssetCategory(void);
extern void StringTable_GetAsset(const char *name, const engine_table_t **table);

#define UNLOCK_FLAG_TABLE "mp/unlockFlags.csv"

static int coop_park_armed;

static int coop_match(void)
{
    const char *category = SV_AssetCategory();
    return category && (!Q_stricmp(category, "coop") || !Q_stricmp(category, "survival"));
}

void Scr_ReportZombiesResult(scr_entref_t ref)
{
    const char *category = SV_AssetCategory();
    int recorded;
    if (ref.classnum || ref.entnum >= MAX_CLIENTS || !g_entities[ref.entnum].client ||
        !g_entities[ref.entnum].r.inuse) {
        Scr_Error("codol_reportZombiesResult: entity must be a player entity");
        return;
    }
    int count = Scr_GetNumParam();
    if (count != 4 && count != 7 && count != 8) {
        Scr_Error("codol_reportZombiesResult requires result fields, optional downs/revives/headshots and extraction event");
        return;
    }
    if (!category || Q_stricmp(category, "zm")) {
        Scr_AddInt(0);
        return;
    }
    recorded = MatchReportZombiesResult((unsigned)ref.entnum,
        Scr_GetInt(0), Scr_GetInt(1), Scr_GetInt(2), Scr_GetInt(3),
        count >= 7 ? Scr_GetInt(4) : -1, count >= 7 ? Scr_GetInt(5) : -1,
        count >= 7 ? Scr_GetInt(6) : -1, count == 8 ? Scr_GetInt(7) : 0);
    Scr_AddInt(recorded);
}

void Scr_CheckpointZombiesRound(void)
{
    const char *category = SV_AssetCategory();
    char difficulty[16], enabled[16];
    if (Scr_GetNumParam() != 3) {
        Scr_Error("codol_checkpointZombiesRound requires difficulty, round and goal");
        return;
    }
    Cvar_VariableStringBuffer("g_difficulty",difficulty,sizeof(difficulty));
    Cvar_VariableStringBuffer("zombiesEndlessEnabled",enabled,sizeof(enabled));
    if (!category || strcmp(category,"zm") || atoi(difficulty)!=3 || strcmp(enabled,"1")) {
        Scr_AddInt(0);
        return;
    }
    Scr_AddInt(MatchReportZombiesCheckpoint(Scr_GetInt(0),Scr_GetInt(1),Scr_GetInt(2)));
}

/* The unlock flags C1's mp/unlockFlags.csv grants on this mission's
 * challenge (column 3 = the challenge ref, column 4 = its tier, column 1 = the
 * flag id), as "id|id|...". The challenge is so_<mission> for a story mission
 * and so_<mission>_2 (the first difficulty) for a Spec Ops mission; whichever
 * the table names. 0 when the table is not mounted or nothing is gated. */
static int coop_mission_flags(const char *mission, char *out, size_t size)
{
    const engine_table_t *table = NULL;
    char story[96], spec[96];
    size_t used = 0;
    int row, count = 0;
    out[0] = 0;
    if (snprintf(story, sizeof(story), "so_%s", mission) >= (int)sizeof(story)) return 0;
    if (snprintf(spec, sizeof(spec), "so_%s_2", mission) >= (int)sizeof(spec)) return 0;
    StringTable_GetAsset(UNLOCK_FLAG_TABLE, &table);
    if (!table || !table->values || table->columnCount < 5) return 0;
    for (row = 0; row < table->rowCount; ++row) {
        const char *const *cells = (const char *const *)table->values + (size_t)row * table->columnCount;
        const char *challenge = cells[3], *id = cells[1], *tier = cells[4];
        int n;
        if (!challenge || !id || atoi(id) <= 0) continue;
        if (Q_stricmp(challenge, story) && Q_stricmp(challenge, spec)) continue;
        /* Every storymission challenge has one tier of target 1. */
        if (tier && atoi(tier) > 1) continue;
        n = snprintf(out + used, size - used, "%s%d", used ? "|" : "", atoi(id));
        if (n < 0 || (size_t)n >= size - used) break;
        used += (size_t)n;
        ++count;
    }
    return count;
}

void Scr_ReportCoopMission(scr_entref_t ref)
{
    const char *mission;
    int completed, recorded;
    if (ref.classnum || ref.entnum >= MAX_CLIENTS || !g_entities[ref.entnum].client ||
        !g_entities[ref.entnum].r.inuse) {
        Scr_Error("codol_reportCoopMission: entity must be a player entity");
        return;
    }
    if (Scr_GetNumParam() != 2) {
        Scr_Error("codol_reportCoopMission requires a mission name and a completed flag");
        return;
    }
    mission = Scr_GetString(0);
    completed = Scr_GetInt(1) != 0;
    if (!coop_match()) {
        Com_PrintWarning(CON_CHANNEL_SERVER,
            "CoDOL coop: codol_reportCoopMission outside a coop match is ignored\n");
        Scr_AddInt(0);
        return;
    }
    recorded = MatchReportCoopMission((unsigned)ref.entnum, mission, completed);
    Com_Printf(CON_CHANNEL_SERVER, "CoDOL coop: client %d mission %s %s (%s)\n",
        ref.entnum, mission, completed ? "completed" : "failed",
        recorded ? "in the match report" : "no account row - not reported");
    if (completed) {
        char flags[256];
        int count = coop_mission_flags(mission, flags, sizeof(flags));
        if (count && svs.clients[ref.entnum].netchan.remoteAddress.type != NA_BOT) {
            SV_GameSendServerCommand(ref.entnum, 1, va("s cg_unlockFlagSet \"%s\"", flags));
            ((void)0);
        } else {
            ((void)0);
        }
    }
    Scr_AddInt(recorded);
}

int CoopPrepareLobbyReturn(void)
{
    if (!coop_match()) return 0;
    WriteMatchReport("coopmission");
    coop_park_armed = 1;
    return 1;
}

int CoopExitLevelParks(void)
{
    if (!coop_park_armed) return 0;
    coop_park_armed = 0;
    MatchStatusSetParked(1);
    ((void)0);
    return 1;
}

void CoopResetLobbyReturn(void)
{
    coop_park_armed = 0;
    MatchStatusSetParked(0);
}
#endif

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}
extern int MatchStatusParked(void);
int PersistentTailCoopEmpty(void){return !coop_park_armed&&!MatchStatusParked();}
void PersistentTailCoopReset(void){CoopResetLobbyReturn();}
#endif
