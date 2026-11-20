/* C1's weapon-change announcement (event 0x1c), queued only where C1 queues it.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * C1 PM_BeginWeaponChange (0x4196a0..) decides between two arms after it has
 * cleared the variable zoom (0x4197c2..0x4197ce):
 *   - the quick arm, 0x4197d4..0x41984c: no current weapon, no equipped
 *     record for it, or ps+0x34 (the retained weaponRestrictKickTime, ps+0x44
 *     here) still above zero. It writes weaponstate 3/4 and returns without
 *     queuing anything;
 *   - the drop arm, 0x41984d..: event 0x1c with the new weapon as parameter
 *     (0x419888), then the alternate (0x1b) or put-away (0x1a) event.
 * The retained PM_BeginWeaponChange (_bg_weapons.asm) used to queue the
 * announcement before that decision, so every quick change gave the server
 * one predicted event C1 never queues; C1 then renumbered and replayed every
 * newer predicted event when the snapshot arrived.
 *
 * The retained code calls this twice per change: once before the decision
 * (drop_arm 0) and once on the drop arm (drop_arm 1). c1EventParity 1
 * (default) queues only on the drop arm; 0 restores the old placement for
 * bisecting. */
#ifdef OL_SERVER
#include "q_shared.h"
#include "bg_public.h"
#include "weapon_state.h"
#include "weapon_bits.h"
#include "weapon_slots.h"
#include "pmove_c1.h"

void __cdecl BG_AddPredictableEventToPlayerstate(int event, unsigned int eventParm,
                                                 playerState_t *ps);
int __cdecl Scr_GetNumParam(void);
int __cdecl Scr_GetInt(unsigned int index);

void __cdecl WeaponChangeAnnounce(playerState_t *ps, int weapon, int drop_arm)
{
    if(!ps) return;
    if((drop_arm != 0) != (C1EventParity() != 0)) return;
    BG_AddPredictableEventToPlayerstate(EV_WEAPON_SWITCH_STARTED, (unsigned)weapon, ps);
}

/* setSpawnWeapon's raised-before mark. C1 PlayerCmd_setSpawnWeapon writes the
 * new weapon's equipped-record byte 0 (0x51b65c) from a flag that starts true
 * (0x51b592) and, when the call has a second argument (0x51b597: parameter
 * count above one), is Scr_GetInt(1) != 0 (0x51b59f -> 0x5cda50). That byte
 * is what PM_FinishWeaponChange reads at 0x419d97 to choose the first raise
 * (event 25) over the ordinary one (24), and the server publishes the
 * weaponold bit as that byte. The retained IW3 builtin always set the bit, so
 * setSpawnWeapon(weapon, false) could never give C1's first raise. */
void __cdecl SetSpawnWeaponUsedBefore(playerState_t *ps, int weapon)
{
    int used = 1;
    if(!ps || weapon <= 0 || weapon >= WEAPON_SLOTS) return;
    if(Scr_GetNumParam() > 1) used = Scr_GetInt(1) != 0;
    WeaponBitsOldSet(ps, (unsigned)weapon, used);
}
#endif
