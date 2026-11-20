/* C1 Fill_Clip 0x527680 loads BOTH magazines of a dual-wield weapon.
 *
 * Session 10 (zm_prototype_sh, m1911dw_zm at spawn): PARITY "pred hand1+0x8
 * (ps+0x28C) client 714 snapshot 0" at match start. hand1+0x8 is the second
 * hand's weaponDelay; 714 is what is left of the 1100 ms C1 writes for an
 * EMPTY reload (PM_SetReloadingState 0x4192f0 -> 0x419150, def+0x5e4) after
 * 386 ms. The client saw an empty left magazine: the server created it only at
 * the next movement frame, so the snapshots between the give and that frame
 * carried none, and it created it out of nothing (reserve 88, C1 80).
 *
 * This test drives WeaponStateFillLeftClip the way the retained Fill_Clip
 * and Add_Ammo now call it after loading hand 0, and checks C1's arithmetic:
 * add = min(clipSize - clip[1], reserve), reserve -= add, clip[1] += add, with
 * the second magazine published at once. Engine dependencies are stubbed.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "bg.h"
#include "weapon_state.h"
#include "playerstate_ext.h"
#include "abilities.h"
#include "assets.h"
#include "weapon_input.h"
#include "entitystate_ext.h"
#include "weapon_native.h"
#include "xassets/weapondef.h"
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#define AKIMBO 17
#define SINGLE 18

static struct WeaponDef defs[32];
static uint32_t owned[WEAPON_BIT_WORDS];
static ps_hand_t hand_ext;
static ps_ext_t stub_ext;

int BG_GetNumWeapons(void) { return 32; }
struct WeaponDef *BG_GetWeaponDef(int index) { return index >= 0 && index < 32 ? &defs[index] : NULL; }
int WeaponBitsOwned(const playerState_t *ps, unsigned weapon)
{ (void)ps; return weapon < WEAPON_SLOTS && !!(owned[weapon >> 5] & (1u << (weapon & 31))); }
uint32_t WeaponBitsOwnedWord(const playerState_t *ps, unsigned word)
{ (void)ps; return word < WEAPON_BIT_WORDS ? owned[word] : 0; }
const ps_ext_t *PlayerStateExtConst(const playerState_t *ps) { (void)ps; return &stub_ext; }
void PlayerStateExtClearClientHand(unsigned client) { (void)client; memset(&hand_ext, 0, sizeof(hand_ext)); }
void PlayerStateExtClearClientOffhand(unsigned client) { (void)client; }
ps_hand_t *PlayerStateHandMutable(const playerState_t *ps) { (void)ps; return &hand_ext; }
const ps_hand_t *PlayerStateHandRead(const playerState_t *ps) { (void)ps; return &hand_ext; }
ps_offhand_t *PlayerStateOffhandMutable(const playerState_t *ps) { (void)ps; return NULL; }
const ps_offhand_t *PlayerStateOffhandRead(const playerState_t *ps) { (void)ps; return NULL; }
void VariableZoomResetClient(unsigned client) { (void)client; }
void Com_PrintWarning(unsigned channel, const char *fmt, ...) { (void)channel; (void)fmt; }
void Com_Printf(unsigned channel, const char *fmt, ...) { (void)channel; (void)fmt; }

static int dual_resolver(const struct WeaponDef *def, int *out)
{
    *out = def == &defs[AKIMBO];
    return 1;
}

static int left_magazine(const playerState_t *ps, unsigned weapon)
{
    int ammo = -1;
    assert(WeaponStateClipAmmo(ps, weapon, 1, &ammo));
    return ammo;
}

/* The retained G_InitializeAmmo path for a fresh give: ownership first, the
 * reserve from Add_Ammo, then the retained Fill_Clip's hand-0 load. */
static void give(playerState_t *ps, unsigned weapon, int start)
{
    const struct WeaponDef *def = &defs[weapon];
    int add;
    owned[weapon >> 5] |= 1u << (weapon & 31);
    ps->ammo[def->iAmmoIndex] += start;
    add = def->iClipSize - ps->ammoclip[def->iClipIndex];
    if(add > ps->ammo[def->iAmmoIndex]) add = ps->ammo[def->iAmmoIndex];
    ps->ammo[def->iAmmoIndex] -= add;
    ps->ammoclip[def->iClipIndex] += add;
    WeaponStateFillLeftClip(ps, weapon);
}

int main(void)
{
    static playerState_t ps;
    defs[AKIMBO].iClipSize = 8;  defs[AKIMBO].iAmmoIndex = 17; defs[AKIMBO].iClipIndex = 17;
    defs[SINGLE].iClipSize = 30; defs[SINGLE].iAmmoIndex = 18; defs[SINGLE].iClipIndex = 18;
    SetWeaponDualWieldResolver(dual_resolver);
    ps.clientNum = 6;
    ResetClientWeaponState(6);

    /* m1911dw_zm from the session 10 spawn: start 96, clip 8. C1: 8 + 8 + 80. */
    give(&ps, AKIMBO, 96);
    assert(ps.ammoclip[17] == 8);
    assert(ps.ammo[17] == 80);
    assert(left_magazine(&ps, AKIMBO) == 8);
    /* Published by the give itself, before any movement frame runs. */
    assert(hand_ext.clip_index[0] == 17 && hand_ext.magazine[0] == 8);

    /* A second fill on full magazines moves nothing (add 0, 0x52775a). */
    WeaponStateFillLeftClip(&ps, AKIMBO);
    assert(ps.ammo[17] == 80 && left_magazine(&ps, AKIMBO) == 8);

    /* A short reserve fills the second hand only as far as it goes: hand 0
     * takes its share first, exactly as the per-hand loop runs. */
    memset(&ps, 0, sizeof(ps)); ps.clientNum = 6;
    memset(owned, 0, sizeof(owned));
    ResetClientWeaponState(6);
    give(&ps, AKIMBO, 11);
    assert(ps.ammoclip[17] == 8 && left_magazine(&ps, AKIMBO) == 3 && ps.ammo[17] == 0);

    /* Topping up a partly spent left magazine (Add_Ammo with a fill, e.g. an
     * ammo pickup) draws only the missing rounds from the reserve. */
    ps.ammo[17] = 20;
    WeaponStateFillLeftClip(&ps, AKIMBO);
    assert(left_magazine(&ps, AKIMBO) == 8 && ps.ammo[17] == 15);

    /* A single-hand weapon has no second magazine to load (0x5276d7). */
    give(&ps, SINGLE, 120);
    assert(ps.ammoclip[18] == 30 && ps.ammo[18] == 90);
    assert(left_magazine(&ps, AKIMBO) == 8 && ps.ammo[17] == 15);

    puts("PASS: a dual-wield give loads both magazines from the reserve like C1 Fill_Clip 0x527680 (8 + 8 + 80), publishes the left magazine at once, and leaves single-hand weapons alone");
    return 0;
}
