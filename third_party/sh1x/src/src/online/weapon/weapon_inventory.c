/* C1's fifteen-slot weapon inventory over the 700-wide owned set; see
 * weapon_inventory.h.
 *
 * Only WeaponBitsOwned is used, so the snapshot writers' unit tests
 * (which stub it) link this file unchanged.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "weapon_inventory.h"
#include "weapon_bits.h"

unsigned WeaponOwnedCount(const playerState_t *ps)
{
    unsigned weapon, count = 0;
    if (!ps) return 0;
    for (weapon = 1; weapon < WEAPON_SLOTS; ++weapon)
        if (WeaponBitsOwned(ps, weapon)) ++count;
    return count;
}

int WireInventoryHas(const unsigned list[INVENTORY_SLOTS], unsigned count,
                           unsigned weapon)
{
    unsigned i;
    for (i = 0; i < count && i < INVENTORY_SLOTS; ++i) if (list[i] == weapon) return 1;
    return 0;
}

static void pick(const playerState_t *ps, unsigned out[INVENTORY_SLOTS], unsigned *count,
                 unsigned weapon)
{
    if (!weapon || weapon >= WEAPON_SLOTS || *count >= INVENTORY_SLOTS ||
        !WeaponBitsOwned(ps, weapon) || WireInventoryHas(out, *count, weapon)) return;
    out[(*count)++] = weapon;
}

unsigned WireInventory(const playerState_t *ps, unsigned out[INVENTORY_SLOTS],
                             unsigned *owned_out)
{
    unsigned owned = 0, count = 0, weapon, i, j;
    if (owned_out) *owned_out = 0;
    if (!ps || !out) return 0;
    for (weapon = 1; weapon < WEAPON_SLOTS; ++weapon) {
        if (!WeaponBitsOwned(ps, weapon)) continue;
        if (count < INVENTORY_SLOTS) out[count++] = weapon;
        ++owned;
    }
    if (owned_out) *owned_out = owned;
    if (owned > INVENTORY_SLOTS) {
        /* Over C1's ceiling (GivePlayerWeapon refuses past it, so this
         * is a defensive path): the weapon in hand and the equipped offhand
         * first, so what the player is using keeps its ammo and its slot,
         * then the lowest indices. */
        count = 0;
        pick(ps, out, &count, ps->weapon);
        if (ps->offHandIndex > 0) pick(ps, out, &count, (unsigned)ps->offHandIndex);
        for (weapon = 1; weapon < WEAPON_SLOTS && count < INVENTORY_SLOTS; ++weapon)
            pick(ps, out, &count, weapon);
        /* Ascending order, as the writers always published it. */
        for (i = 1; i < count; ++i) {
            unsigned v = out[i];
            for (j = i; j > 0 && out[j - 1] > v; --j) out[j] = out[j - 1];
            out[j] = v;
        }
    }
    return count;
}
