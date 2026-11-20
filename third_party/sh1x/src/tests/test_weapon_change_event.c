/* C1 weapon-change event placement and setSpawnWeapon's raised-before mark.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * weapon_change_event.c is called twice by the retained
 * PM_BeginWeaponChange: before its quick/drop decision (drop_arm 0) and on the
 * drop arm (drop_arm 1). C1 queues event 0x1c only on the drop arm
 * (0x419888); its quick arm (0x4197d4..0x41984c) queues nothing. A change
 * must therefore queue exactly one announcement on the drop arm and none on
 * the quick arm, and c1EventParity 0 must restore the old placement. */
#include <stdio.h>
#include <string.h>
#include "q_shared.h"
#include "bg_public.h"
#include "weapon_state.h"

void __cdecl WeaponChangeAnnounce(playerState_t *ps, int weapon, int drop_arm);
void __cdecl SetSpawnWeaponUsedBefore(playerState_t *ps, int weapon);

static int parity = 1, queued, last_event, last_parm;
static int params = 1, param1;
static int old_weapon = -1, old_value = -1;

int C1EventParity(void) { return parity; }
void __cdecl BG_AddPredictableEventToPlayerstate(int event, unsigned int parm, playerState_t *ps)
{
    (void)ps;
    ++queued; last_event = event; last_parm = (int)parm;
}
int __cdecl Scr_GetNumParam(void) { return params; }
int __cdecl Scr_GetInt(unsigned int index) { return index == 1 ? param1 : 0; }
void WeaponBitsOldSet(const playerState_t *ps, unsigned weapon, int old)
{
    (void)ps;
    old_weapon = (int)weapon; old_value = old;
}

static int failures;
#define CHECK(cond) do { if(!(cond)) { ++failures; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while(0)

/* One change as the retained code runs it: the pre-decision call always, the
 * drop-arm call only when the change drops the weapon. */
static int change(playerState_t *ps, int weapon, int drop)
{
    queued = 0;
    WeaponChangeAnnounce(ps, weapon, 0);
    if(drop) WeaponChangeAnnounce(ps, weapon, 1);
    return queued;
}

int main(void)
{
    static playerState_t ps;

    parity = 1;
    CHECK(change(&ps, 7, 1) == 1);
    CHECK(last_event == EV_WEAPON_SWITCH_STARTED && last_parm == 7);
    CHECK(change(&ps, 7, 0) == 0);   /* quick arm: C1 queues nothing */

    parity = 0;                      /* bisecting: the old placement */
    CHECK(change(&ps, 9, 1) == 1);
    CHECK(change(&ps, 9, 0) == 1);
    CHECK(last_event == EV_WEAPON_SWITCH_STARTED && last_parm == 9);
    parity = 1;

    queued = 0;
    WeaponChangeAnnounce(NULL, 7, 1);
    CHECK(queued == 0);

    /* setSpawnWeapon(weapon): raised before (C1 0x51b592). */
    params = 1; param1 = 0;
    SetSpawnWeaponUsedBefore(&ps, 12);
    CHECK(old_weapon == 12 && old_value == 1);
    /* setSpawnWeapon(weapon, false): the next raise is the first raise. */
    params = 2; param1 = 0;
    SetSpawnWeaponUsedBefore(&ps, 13);
    CHECK(old_weapon == 13 && old_value == 0);
    params = 2; param1 = 1;
    SetSpawnWeaponUsedBefore(&ps, 14);
    CHECK(old_weapon == 14 && old_value == 1);
    /* No weapon: nothing written. */
    old_weapon = -1;
    SetSpawnWeaponUsedBefore(&ps, 0);
    CHECK(old_weapon == -1);

    if(failures) { printf("test_weapon_change_event: %d failure(s)\n", failures); return 1; }
    printf("test_weapon_change_event: ok\n");
    return 0;
}
