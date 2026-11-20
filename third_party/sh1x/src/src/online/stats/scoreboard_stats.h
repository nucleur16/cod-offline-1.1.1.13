/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef SCOREBOARD_STATS_H
#define SCOREBOARD_STATS_H
#include <limits.h>
/* C1 ae0410 converts the float kill count to a signed integer by truncation.
 * Match the x86 invalid-conversion result without undefined C float casts. */
static inline int ScoreInteger(float value)
{
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return INT_MIN;
    return (int)value;
}
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    SCORE_DOWNS, SCORE_REVIVES, SCORE_HEADSHOTS,
    SCORE_OBJECTIVES, SCORE_STAT_COUNT
} score_stat_t;
int GetScoreStat(unsigned client, unsigned stat);
void SetScoreStat(unsigned client, unsigned stat, int value);
void ResetScoreStats(unsigned client);
void ResetAllScoreStats(void);
/* Read C1's skills/tdm column from the public session profile. Failure leaves
 * out unchanged; the sender owns C1's online/client-state gating. */
int ReadScoreboardSkill(unsigned client, int *out);
#ifdef __cplusplus
}
#endif
#endif
