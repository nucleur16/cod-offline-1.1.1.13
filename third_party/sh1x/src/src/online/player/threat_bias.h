#ifndef THREAT_BIAS_H
#define THREAT_BIAS_H

/* Native threat bias groups (IW4 SP / T5 threat_bias_t).
 *
 * MEASURED, OpenBLOPS Game/Server/game/actor_threat.h:30: threat_bias_t is
 * 16 group names and a 16x16 int table plus a count; group 0 is the unnamed
 * default every sentient starts in (Actor_InitThreatBiasGroups sets the
 * count to 1, actor_threat.cpp:46-58). The table is indexed
 * [threatener][threatened] by setthreatbias (g_scr_main_mp.cpp:18211-18218)
 * and read as Actor_GetThreatBias(enemyGroup, selfGroup) by the threat pass
 * (actor_threat.cpp:422), so entry [g1][g2] is how much more a member of g2
 * wants to attack a member of g1 -- the IW4 doc for setthreatbias(<group1>,
 * <group2>, <threat>): "group2 will be this much more or less likely to
 * attack group1". The value INT_MIN (0x80000000) is "ignore": Actor_CheckIgnore
 * (actor_threat.cpp:36-44) and Actor_UpdateSingleThreat (:423) both drop the
 * candidate, which is what IW4's setignoremegroup(<group1>, <group2>) ("group2
 * will ignore group1") writes.
 *
 * This module is the table only; it has no VM dependency, so the scoring is
 * unit-testable (tests/test_threat_bias.c). The script surface lives in
 * sentient.cpp, beside the per-sentient group and bias it reads. */

#include <limits.h>

#define THREATBIAS_MAX_GROUPS 16
#define THREATBIAS_NAME_LEN 64
#define THREATBIAS_IGNORE INT_MIN

#ifdef __cplusplus
extern "C" {
#endif

/* Level init: only the unnamed group 0 remains and every bias is zero. */
void ThreatBias_Reset(void);
/* Index of a named group, -1 when there is none. The empty name is never a
 * created group (T5 groupName[0] is the null string, which script cannot pass
 * to createthreatbiasgroup as a distinct group). */
int ThreatBias_Find(const char *name);
/* T5 GScr_CreateThreatBiasGroup_SP: an existing name returns its index, a
 * full table returns -1 (the caller prints T5's warning). */
int ThreatBias_Create(const char *name);
int ThreatBias_Count(void);
/* "" for group 0 or an out-of-range index (T5 GScr_GetThreatBiasGroup_SP). */
const char *ThreatBias_Name(int group);
/* table[group1][group2]: how much group2 wants to attack group1. Out-of-range
 * indices read 0 and are not written. */
int ThreatBias_Get(int group1, int group2);
void ThreatBias_Set(int group1, int group2, int bias);
/* Actor_CheckIgnore's group half: does a member of selfGroup ignore a member
 * of enemyGroup? */
int ThreatBias_Ignores(int enemyGroup, int selfGroup);

/* T5 Actor_ThreatFromDistance (actor_threat.cpp:586-598):
 * 5000 * (2500 - d)^2 / 2500^2 below 2500 units, else 0. */
int ThreatFromDistance(float distance);

/* The bias half of T5 Actor_UpdateSingleThreat (actor_threat.cpp:422-455):
 * enemy->iThreatBias + Actor_GetThreatBias(enemyGroup, selfGroup) + the
 * distance term. Returns THREATBIAS_IGNORE when the groups ignore.
 * The sum saturates instead of wrapping (T5 adds plain ints; a wrap could
 * only turn a huge bias into "ignore"), and stays below 0x7ffffffe, the
 * favourite enemy's score (actor_threat.cpp:317-321). */
int ThreatBias_Score(int enemyBias, int enemyGroup, int selfGroup, float distance);

#ifdef __cplusplus
}
#endif

#endif
