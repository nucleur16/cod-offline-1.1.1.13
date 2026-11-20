/* C1 dive end and dive check (src/online/player/dive.c).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * C1 PmoveSingle runs PM_DiveEnd 0x408c00 once per command before it picks a
 * move (0x4168f2); PM_CheckDive 0x408f30 is reached only from PM_WalkMove
 * (0x410957). The retained IW3 Pmove reaches both through
 * DiveGroundFrame from its walk move. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "q_shared.h"
#include "bg_public.h"

void DiveEnd(struct pmove_t *pm);
int DiveCheck(struct pmove_t *pm, struct pml_t *pml);
int DiveGroundFrame(struct pmove_t *pm, struct pml_t *pml);

#define PMF_DIVE 0x40000000
#define EV_LAND 140
#define EV_LAUNCH 142

static int dive_end_time, events, last_event, blocks_prone;

int PlayerStateDiveEndTimeSet(const playerState_t *ps, int value)
{ (void)ps; dive_end_time = value; return 1; }
int PlayerStateDiveEndTimeRead(const playerState_t *ps, int *value)
{ (void)ps; *value = dive_end_time; return 1; }
void PlayerStateExtClearClientDive(unsigned client) { (void)client; dive_end_time = 0; }
int WeaponBlocksProneC1(unsigned weapon) { (void)weapon; return blocks_prone; }
void __cdecl BG_AddPredictableEventToPlayerstate(int event, unsigned int parm, playerState_t *ps)
{ (void)parm; (void)ps; ++events; last_event = event; }
void __cdecl PM_playerTrace(struct pmove_t *pm, trace_t *result, const float *start,
    const float *mins, const float *maxs, const float *end, int pass, int mask)
{
    (void)pm; (void)start; (void)mins; (void)maxs; (void)end; (void)pass; (void)mask;
    memset(result, 0, sizeof(*result)); result->fraction = 1.0f;
}

static int failures;
#define CHECK(cond) do { if(!(cond)) { ++failures; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while(0)

static playerState_t ps;
static struct pmove_t pm;
static struct pml_t pml;

static void reset(int now)
{
    memset(&ps, 0, sizeof(ps)); memset(&pm, 0, sizeof(pm)); memset(&pml, 0, sizeof(pml));
    pm.ps = &ps; pm.cmd.serverTime = now;
    ps.gravity = 800;
    events = 0; last_event = 0; blocks_prone = 0; dive_end_time = 0;
}

/* A sliding dive at the given speed. */
static void sliding(int now, float speed)
{
    reset(now);
    ps.pm_flags = PMF_DIVE; ps.jumpTime = -(now - 50);
    ps.velocity[0] = speed;
}

/* A sprint that ended on this command with the dive bit held. */
static void ready_to_dive(int now)
{
    reset(now);
    pml.walking = 1; pml.groundPlane = 1;
    pm.cmd.buttons = 0x800100;
    ps.sprintState.lastSprintStart = now - 600;
    ps.sprintState.lastSprintEnd = now;
    ps.velocity[0] = 250.0f;
    dive_end_time = now - 5000;
}

int main(void)
{
    /* PM_DiveEnd: a sliding dive ends below the minimum speed (3.16); at the
     * minimum itself it keeps sliding (0x408c60 test ah,0x41). */
    sliding(10000, 3.16f);
    DiveEnd(&pm);
    CHECK(ps.pm_flags & PMF_DIVE);
    sliding(10000, 3.0f);
    DiveEnd(&pm);
    CHECK(!(ps.pm_flags & PMF_DIVE));
    CHECK(dive_end_time == 10000 && ps.jumpTime == 0 && ps.velocity[0] == 0.0f);
    /* All three components count. */
    sliding(10000, 0.0f); ps.velocity[2] = -4.0f;
    DiveEnd(&pm);
    CHECK(ps.pm_flags & PMF_DIVE);
    /* A dive still in the air (jumpTime above zero) never ends here. */
    sliding(10000, 0.0f); ps.jumpTime = 9900;
    DiveEnd(&pm);
    CHECK(ps.pm_flags & PMF_DIVE);

    /* PM_CheckDive touchdown: the first walking command of an airborne dive
     * queues the landing event and marks the slide (-now). */
    reset(20000); pml.walking = 1;
    ps.pm_flags = PMF_DIVE; ps.jumpTime = 19500;
    CHECK(DiveCheck(&pm, &pml) == 1);
    CHECK(ps.jumpTime == -20000 && events == 1 && last_event == EV_LAND);
    /* Sliding: still the air move, nothing queued. */
    events = 0;
    CHECK(DiveCheck(&pm, &pml) == 1 && events == 0);

    /* PM_CheckDive launch. */
    ready_to_dive(30000);
    CHECK(DiveCheck(&pm, &pml) == 1);
    CHECK((ps.pm_flags & PMF_DIVE) && ps.jumpTime == 30000 && last_event == EV_LAUNCH);
    CHECK(fabsf(ps.velocity[2] - (float)sqrt(800.0 * 39.0) * 1.5f) < 0.01f);
    CHECK(ps.groundEntityNum == ENTITYNUM_NONE && !pml.walking);
    /* Cooldown: 1500 ms from the last dive's end, measured strictly. */
    ready_to_dive(30000); dive_end_time = 28500;
    CHECK(DiveCheck(&pm, &pml) == 0 && !(ps.pm_flags & PMF_DIVE));
    ready_to_dive(30000); dive_end_time = 28499;
    CHECK(DiveCheck(&pm, &pml) == 1);
    /* The sprint must have ended on this very command, have lasted more than
     * 250 ms, and the 2D speed must exceed the minimum. */
    ready_to_dive(30000); ps.sprintState.lastSprintEnd = 29992;
    CHECK(DiveCheck(&pm, &pml) == 0);
    ready_to_dive(30000); ps.sprintState.lastSprintStart = 29750;
    CHECK(DiveCheck(&pm, &pml) == 0);
    ready_to_dive(30000); ps.velocity[0] = 0.0f; ps.velocity[2] = 300.0f;
    CHECK(DiveCheck(&pm, &pml) == 0);
    ready_to_dive(30000); pm.cmd.buttons = 0x100;
    CHECK(DiveCheck(&pm, &pml) == 0);
    ready_to_dive(30000); blocks_prone = 1;
    CHECK(DiveCheck(&pm, &pml) == 0);
    /* PM_CanDive reads no pm_type. */
    ready_to_dive(30000); ps.pm_type = 7;
    CHECK(DiveCheck(&pm, &pml) == 1);

    /* The retained IW3 walk move: an airborne player never starts a dive
     * (C1 sends it to PM_AirMove before PM_WalkMove), a dive in the air keeps
     * the air move without touching down, and the end runs first. */
    ready_to_dive(40000); pml.walking = 0;
    CHECK(DiveGroundFrame(&pm, &pml) == 0 && !(ps.pm_flags & PMF_DIVE) && events == 0);
    reset(40000); ps.pm_flags = PMF_DIVE; ps.jumpTime = 39800;
    CHECK(DiveGroundFrame(&pm, &pml) == 1 && ps.jumpTime == 39800 && events == 0);
    sliding(40000, 1.0f); pml.walking = 1;
    CHECK(DiveGroundFrame(&pm, &pml) == 0 && dive_end_time == 40000);

    if(failures) { printf("test_dive: %d failure(s)\n", failures); return 1; }
    printf("PASS: C1 dive end (0x408c00) and dive check (0x408f30): strict thresholds, touchdown, cooldown, ground-only launch\n");
    return 0;
}
