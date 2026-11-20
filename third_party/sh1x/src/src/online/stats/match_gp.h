/* GP (account currency 2) a player earns by PLAYING a match - the scaled part
 * of a match's GP. Fixed awards (field orders, challenge tiers) never come
 * through here; match_report.c keeps them in their own bucket.
 *
 * Formula (chosen by the user 2026-10-13 from C1's retail tables; retail's
 * own GSC never shipped, so the combination is a decision, not a recovery):
 *
 *   GP = matchreward_perminute
 *        x min(minutes played, matchreward_timecap)
 *        x matchreward_scale_win | _tie | _loss
 *        x (1 + matchreward_levelscale x level)
 *        x GP league scaler            (mp/gpleaguescaler.csv)
 *        x activity gp_scale           (activities.info, C1 0x508120/0x508BC0)
 *
 * truncated to an integer, as _rank.gsc truncates XP with int().
 * matchreward_playernum is deliberately unused.
 *
 * Pure: no engine state. The tables are passed in, the activity file is text.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef MATCH_GP_H
#define MATCH_GP_H

#include <stddef.h>
#include <time.h>
#include "assets.h"

#pragma pack(push, 8)

#define GP_OUTCOME_LOSS 0
#define GP_OUTCOME_TIE  1
#define GP_OUTCOME_WIN  2

typedef struct {
    double timecap_minutes;
    double scale_win, scale_loss, scale_tie;
    double perminute;
    double levelscale;
} match_gp_params_t;

/* The matchreward_* columns of mp/gametypescptable.csv for `mode` (column 0,
 * case-insensitive). Columns are found by their header names in row 0, so a
 * reordered table still reads right. 1 found, 0 no such row or columns. */
int MatchGPParams(const engine_table_t *table, const char *mode,
                        match_gp_params_t *out);

/* The mp/gpleaguescaler.csv factor of row `row` at league index `league`
 * (0..4 = columns 1..5, C1 GPLeagueScalerTable.LEAGUE_0..4). "ranked" is 1
 * whatever the table says, as C1's MatchController.getGPLeagueFactor does for
 * a ranked playlist. A missing row, column or empty cell is 1. */
double MatchGPLeagueScale(const engine_table_t *table, const char *row, int league);

/* The amount. play_ms is the player's time in the match, level its displayed
 * rank level (rank index + 1). */
int MatchGPAmount(const match_gp_params_t *params, int play_ms, int outcome,
                        int level, double league_scale, double activity_scale);

/* The product of the gp_scale of every activities.info event active at `now`
 * for this map and gametype, exactly as C1 0x508BC0 multiplies them (1.0 when
 * none is). `text` is the INI file C1 fetches as a DW pubfile (format: alpha0022
 * docs/pubfiles_recovery.md section 3; reader C1 0x508120, test 0x508A50).
 * `playlist` is 1 for a playlist match, 0 for a custom room. An event with
 * daily_once = yes is skipped, as C1's selector skips it (0x508C16: the
 * once-a-day events are a separate per-account reward path, not a scale).
 * `events` (optional) receives the number of active events. */
double ActivityGPScale(const char *text, size_t length, const struct tm *now,
                             const char *map, const char *gametype, int playlist,
                             int *events);

#pragma pack(pop)
#endif
