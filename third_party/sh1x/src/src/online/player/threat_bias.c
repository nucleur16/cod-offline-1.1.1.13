/* Native threat bias groups; see threat_bias.h for the T5/IW4 evidence. */

#include "threat_bias.h"

#include <math.h>
#include <string.h>

static char s_groupName[THREATBIAS_MAX_GROUPS][THREATBIAS_NAME_LEN];
static int s_table[THREATBIAS_MAX_GROUPS][THREATBIAS_MAX_GROUPS];
static int s_count = 1;

void ThreatBias_Reset(void)
{
    memset(s_groupName, 0, sizeof(s_groupName));
    memset(s_table, 0, sizeof(s_table));
    s_count = 1;
}

int ThreatBias_Find(const char *name)
{
    int i;

    if (!name || !*name)
        return -1;
    for (i = 1; i < s_count; ++i)
        if (!strcmp(s_groupName[i], name))
            return i;
    return -1;
}

int ThreatBias_Create(const char *name)
{
    int index = ThreatBias_Find(name);

    if (index >= 0)
        return index;
    if (!name || !*name || s_count >= THREATBIAS_MAX_GROUPS)
        return -1;
    strncpy(s_groupName[s_count], name, THREATBIAS_NAME_LEN - 1);
    s_groupName[s_count][THREATBIAS_NAME_LEN - 1] = 0;
    return s_count++;
}

int ThreatBias_Count(void)
{
    return s_count;
}

const char *ThreatBias_Name(int group)
{
    if (group <= 0 || group >= s_count)
        return "";
    return s_groupName[group];
}

int ThreatBias_Get(int group1, int group2)
{
    if (group1 < 0 || group1 >= s_count || group2 < 0 || group2 >= s_count)
        return 0;
    return s_table[group1][group2];
}

void ThreatBias_Set(int group1, int group2, int bias)
{
    if (group1 < 0 || group1 >= s_count || group2 < 0 || group2 >= s_count)
        return;
    s_table[group1][group2] = bias;
}

int ThreatBias_Ignores(int enemyGroup, int selfGroup)
{
    return ThreatBias_Get(enemyGroup, selfGroup) == THREATBIAS_IGNORE;
}

int ThreatFromDistance(float distance)
{
    /* T5 AI_THREAT_DISTANCE_RATE: the float reciprocal times the double
     * 5000.0, rounded once to float; then (rate * r) * r in float. The
     * decompiled "+ 2^-30" before the int cast is the x87 FISTP conversion,
     * which rounds to nearest (OpenBLOPS notes the same FISTP pattern at
     * g_scr_main_mp.cpp GScr_ChangeLevelMsec_SP), so d = 0 scores 5000. */
    static const float rate = (float)((double)(float)(1.0 / (2500.0 * 2500.0)) * 5000.0);
    float remaining;

    if (!(distance < 2500.0f))
        return 0;
    if (distance < 0.0f)
        distance = 0.0f;
    remaining = 2500.0f - distance;
    return (int)lrint((double)(float)((float)(rate * remaining) * remaining) + 9.313225746154785e-10);
}

int ThreatBias_Score(int enemyBias, int enemyGroup, int selfGroup, float distance)
{
    int groupBias = ThreatBias_Get(enemyGroup, selfGroup);
    long long total;

    if (groupBias == THREATBIAS_IGNORE)
        return THREATBIAS_IGNORE;
    total = (long long)enemyBias + groupBias + ThreatFromDistance(distance);
    if (total > 0x7ffffffdLL)
        total = 0x7ffffffdLL;
    if (total <= (long long)THREATBIAS_IGNORE)
        total = (long long)THREATBIAS_IGNORE + 1;
    return (int)total;
}
