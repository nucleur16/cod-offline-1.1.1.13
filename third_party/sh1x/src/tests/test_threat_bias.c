/* Native threat bias groups (src/online/player/threat_bias.c): the
 * table, the ignore sentinel and the T5 Actor_UpdateSingleThreat bias +
 * distance score the native actor acquisition ranks candidates by. The VM
 * glue and the sight traces are outside this fixture. */
#include "threat_bias.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char name[32];
    int sentry, allies, axis, boss, dogs, i;

    ThreatBias_Reset();
    assert(ThreatBias_Count() == 1);
    assert(ThreatBias_Find("sentry") < 0);
    assert(ThreatBias_Find("") < 0);
    assert(!strcmp(ThreatBias_Name(0), ""));

    /* IW5 survival ai_init (scripts/1564.gsc). */
    sentry = ThreatBias_Create("sentry");
    allies = ThreatBias_Create("allies");
    axis = ThreatBias_Create("axis");
    boss = ThreatBias_Create("boss");
    dogs = ThreatBias_Create("dogs");
    assert(sentry == 1 && allies == 2 && axis == 3 && boss == 4 && dogs == 5);
    assert(ThreatBias_Create("boss") == boss);
    assert(ThreatBias_Find("axis") == axis);
    assert(!strcmp(ThreatBias_Name(dogs), "dogs"));
    assert(ThreatBias_Find("Axis") < 0);

    ThreatBias_Set(sentry, dogs, THREATBIAS_IGNORE); /* setignoremegroup("sentry","dogs") */
    ThreatBias_Set(sentry, boss, 50);
    ThreatBias_Set(boss, allies, 2000);
    ThreatBias_Set(dogs, allies, 1000);

    /* setignoremegroup(group1, group2): group2 ignores group1, not the
     * other way round. */
    assert(ThreatBias_Ignores(sentry, dogs));
    assert(!ThreatBias_Ignores(dogs, sentry));
    assert(ThreatBias_Get(sentry, dogs) < -10000); /* IW4 combat_utility test */
    assert(ThreatBias_Score(0, sentry, dogs, 10.0f) == THREATBIAS_IGNORE);

    /* setthreatbias(group1, group2, x): group2 wants group1 x more. */
    assert(ThreatBias_Get(boss, allies) == 2000);
    assert(ThreatBias_Get(allies, boss) == 0);

    /* T5 Actor_ThreatFromDistance. */
    assert(ThreatFromDistance(0.0f) == 5000);
    assert(ThreatFromDistance(1250.0f) == 1250);
    assert(ThreatFromDistance(2500.0f) == 0);
    assert(ThreatFromDistance(4000.0f) == 0);
    for (i = 1; i < 2500; ++i)
        assert(ThreatFromDistance((float)i) <= ThreatFromDistance((float)(i - 1)));

    /* An allied (squad) actor picks the boss at 2000 units over a plain axis
     * soldier at 500: 2000 + 200 against 0 + 3200 -- distance still wins
     * there. At 1500 vs 1000 the bias decides. */
    assert(ThreatBias_Score(0, boss, allies, 2000.0f) == 2000 + 200);
    assert(ThreatBias_Score(0, axis, allies, 500.0f) == 3200);
    assert(ThreatBias_Score(0, boss, allies, 1500.0f) >
           ThreatBias_Score(0, axis, allies, 1000.0f));
    /* The per-entity threatbias adds on top of the group bias. */
    assert(ThreatBias_Score(-1000, axis, allies, 0.0f) == 4000);
    assert(ThreatBias_Score(5000, dogs, allies, 2500.0f) == 6000);
    /* No groups / no bias: the score orders purely by distance. */
    assert(ThreatBias_Score(0, 0, 0, 100.0f) > ThreatBias_Score(0, 0, 0, 200.0f));
    /* Saturation below the favourite enemy's 0x7ffffffe and above ignore. */
    assert(ThreatBias_Score(INT_MAX, boss, allies, 0.0f) == 0x7ffffffd);
    assert(ThreatBias_Score(INT_MIN, axis, allies, 3000.0f) == INT_MIN + 1);

    /* Out of range indices read 0 and are not written. */
    ThreatBias_Set(-1, 2, 7);
    ThreatBias_Set(2, 99, 7);
    assert(ThreatBias_Get(-1, 2) == 0 && ThreatBias_Get(2, 99) == 0);

    /* 16 groups including the unnamed one (T5 threat_bias_t). */
    for (i = ThreatBias_Count(); i < THREATBIAS_MAX_GROUPS; ++i)
    {
        snprintf(name, sizeof(name), "g%d", i);
        assert(ThreatBias_Create(name) == i);
    }
    assert(ThreatBias_Create("one_too_many") < 0);
    assert(ThreatBias_Count() == THREATBIAS_MAX_GROUPS);

    /* Map change. */
    ThreatBias_Reset();
    assert(ThreatBias_Count() == 1);
    assert(ThreatBias_Find("boss") < 0);
    assert(ThreatBias_Create("boss") == 1);
    assert(ThreatBias_Get(1, 2) == 0);
    assert(!ThreatBias_Ignores(1, 1));

    puts("PASS: threat bias groups, ignore groups and the T5 bias + distance score");
    return 0;
}
