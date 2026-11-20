/* C1's fifteen-slot weapon inventory over the server's 700-wide owned set.
 *
 * C1 (like IW4) keeps a player's weapons in fifteen equipped-weapon records
 * (ps+0x2a0), and every per-weapon wire section follows them: the inventory
 * section, the fifteen {clipKey, ammo[2]} magazine records and the fifteen
 * reserve records. C1's G_GivePlayerWeapon 0x574bf0 asks 0x422c50 for a free
 * record before adding a weapon (and again before each alternate), and when
 * all fifteen are used prints "WARNING: Player already has %i weapons, unable
 * to give them any more" (0x574d7b) and returns 0, so giveWeapon leaves the
 * player as they were and skips the ammo initialisation.
 *
 * The server's owned set is a 700-bit array (weapon_bits.h), so these
 * helpers apply C1's ceiling: GivePlayerWeapon refuses past it, and the
 * snapshot writers choose the fifteen weapons the client can be shown should
 * the set ever exceed it some other way (they never refuse the frame).
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef WEAPON_INVENTORY_H
#define WEAPON_INVENTORY_H
#include "player.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INVENTORY_SLOTS 15u

/* Owned weapons, index 0 excluded. */
unsigned WeaponOwnedCount(const playerState_t *ps);
/* The weapons published on the wire, ascending native index, at most
 * INVENTORY_SLOTS. When more are owned, the held weapon and the
 * equipped offhand come first, then the lowest indices. *owned_out
 * (optional) receives the full owned count, so a caller can tell a clamp
 * happened. */
unsigned WireInventory(const playerState_t *ps, unsigned out[INVENTORY_SLOTS],
                             unsigned *owned_out);
/* Whether weapon is one of WireInventory's choices. */
int WireInventoryHas(const unsigned list[INVENTORY_SLOTS], unsigned count,
                           unsigned weapon);

#ifdef __cplusplus
}
#endif
#endif
