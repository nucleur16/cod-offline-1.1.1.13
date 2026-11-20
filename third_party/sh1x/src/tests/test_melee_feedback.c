/* Exercise the actual event producer: a Zombies knife keeps robot feedback
 * for actors and uses body feedback on players, without changing the knife
 * carried by the attacker or weakening shield/team rules. */
#include "q_shared.h"
#include "entity.h"
#include "g_shared.h"
#include "g_public.h"
#include "bg.h"
#include "cm_public.h"
#include "xassets/weapondef.h"
#include "weapon_give.h"
#include "radius_damage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern gentity_t *MeleeHitEvent(gentity_t *, gentity_t *,
    const weaponParms_t *, const trace_t *, const float *);

static gentity_t attacker, target, temporary;
static gclient_t attacker_client, target_client;
static struct WeaponDef definition;
static weaponParms_t parms;
static trace_t trace;
static vec3_t endpos = {100.0f, 200.0f, 300.0f};
static unsigned knife_row, blood_events;
static int same_team, temp_event;

unsigned ServerMeleeRow(void) { return knife_row; }
qboolean OnSameTeam(gentity_t *a, gentity_t *b)
{ assert(a == &attacker && b == &target); return same_team; }
void G_AddEvent(gentity_t *entity, int event, int parameter)
{
    assert(entity == &attacker && event == EV_MELEE_BLOOD && parameter == 0);
    ++blood_events;
}
gentity_t *G_TempEntity(const float *origin, int event)
{
    assert(!memcmp(origin, endpos, sizeof(endpos)));
    memset(&temporary, 0, sizeof(temporary));
    temporary.s.number = 88;
    temp_event = event;
    return &temporary;
}

static void setup(int player_target, int has_knife, unsigned row)
{
    memset(&attacker, 0, sizeof(attacker));
    memset(&target, 0, sizeof(target));
    memset(&definition, 0, sizeof(definition));
    memset(&parms, 0, sizeof(parms));
    memset(&trace, 0, sizeof(trace));
    attacker.client = &attacker_client;
    attacker.s.eType = ET_PLAYER;
    attacker.s.number = 1;
    attacker.s.weapon = 12;
    attacker.s.weaponModel = 3;
    target.s.number = 2;
    target.s.eType = player_target ? ET_PLAYER : ET_ACTOR;
    target.client = player_target ? &target_client : NULL;
    definition.knifeModel = has_knife ? (XModel *)&definition : NULL;
    parms.weapDef = &definition;
    knife_row = row;
    blood_events = 0;
    same_team = 0;
}

static gentity_t *hit(void)
{
    gentity_t saved_attacker = attacker;
    gentity_t *event = MeleeHitEvent(&attacker, &target, &parms, &trace, endpos);
    assert(event == &temporary && event->s.weapon == 12 && event->s.weaponModel == 3);
    assert(!memcmp(&attacker, &saved_attacker, sizeof(attacker)));
    return event;
}

int main(void)
{
    gentity_t *event;
    for (int has_knife = 0; has_knife <= 1; ++has_knife) {
        setup(1, has_knife, 13);
        event = hit();
        assert(temp_event == EV_MELEE_HIT && event->s.otherEntityNum == 2);
        assert(event->s.un2 == 0 && event->s.eventParm == 1 && blood_events == 1);
        assert(knife_row == 13); /* attacker's animation/damage row is untouched */

        setup(1, has_knife, 13);
        same_team = 1;
        event = hit();
        assert(event->s.un2 == 0 && event->s.eventParm == 1 && blood_events == 0);

        setup(1, has_knife, 13);
        trace.partGroup = HITLOC_SHIELD;
        event = hit();
        assert(event->s.un2 == 0 && event->s.eventParm == 3 && blood_events == 0);

        setup(0, has_knife, 13);
        event = hit();
        assert(temp_event == EV_MELEE_HIT && event->s.un2 == 13);
        assert(event->s.eventParm == has_knife && blood_events == 1);

        setup(0, has_knife, 13);
        trace.partGroup = HITLOC_SHIELD;
        event = hit();
        assert(event->s.un2 == 13 && event->s.eventParm == (has_knife | 2));
        assert(blood_events == 0);

        setup(0, has_knife, 13);
        target.s.eType = ET_GENERAL;
        event = hit();
        assert(temp_event == EV_MELEE_MISS && event->s.un2 == 13);
        assert(event->s.eventParm == has_knife && blood_events == 0);

        setup(1, has_knife, 0); /* ordinary MP retains its original hints */
        event = hit();
        assert(event->s.un2 == 0 && event->s.eventParm == has_knife);
    }
    setup(1, 1, 13);
    attacker.client = NULL;
    attacker.s.eType = ET_ACTOR;
    event = hit();
    assert(event->s.un2 == 0 && event->s.eventParm == 0 && blood_events == 0);
    setup(1, 0, 22); /* invalid table rows must not become a knife hint */
    event = hit();
    assert(event->s.un2 == 0 && event->s.eventParm == 0);
    puts("PASS: native melee player/actor/scenery feedback, shield precedence, teams and attacker state");
    return 0;
}
