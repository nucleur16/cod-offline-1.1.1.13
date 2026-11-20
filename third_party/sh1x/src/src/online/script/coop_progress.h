/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Coop (campaign / Spec Ops) mission end: the per-player result the lobby
 * turns into the next mission's unlock, and the lobby return that replaces the
 * coop map rotation. See coop_progress.c. */
#ifndef COOP_PROGRESS_H
#define COOP_PROGRESS_H

/* sh1x_status's parked state (match_status.c): a coop server whose
 * mission ended and whose players went back to the lobby. */
void MatchStatusSetParked(int parked);
int MatchStatusParked(void);

#ifdef OL_SERVER
#include "scr_vm.h"
/* self codol_reportCoopMission( <playlist name>, <completed> ) */
void Scr_ReportCoopMission(scr_entref_t ref);
/* self codol_reportZombiesResult(extracted, difficulty, round, round_goal,
 * [downs, revives, headshots, [extraction_event]])
 * Only the successful lift-off/fallback extraction path sets extraction_event.
 * Older scripts remain valid but cannot supply an exact extraction time. */

void Scr_ReportZombiesResult(scr_entref_t ref);
/* Global, non-final authoritative round checkpoint for Endless departures. */
void Scr_CheckpointZombiesRound(void);
/* Called by exitLevel once its lobby leave is decided (exitToLobby on).
 * In a coop match: writes the match report that carries the mission results,
 * before the clients are told to leave, and arms the park below. Returns 1 in
 * a coop match, 0 otherwise (nothing done). */
int CoopPrepareLobbyReturn(void);
/* Called by ExitLevel in place of the map rotation. 1 when the coop lobby
 * return was armed: the server keeps no map rotation going, marks itself
 * parked, and waits for the lobby's next `map`. One-shot. */
int CoopExitLevelParks(void);
/* A new level is loading: forget any armed or active park. */
void CoopResetLobbyReturn(void);
#endif
#endif
