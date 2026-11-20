/* Dropped-item ring support. SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Drop_Item keeps the last g_maxDroppedWeapons dropped items (weapons and
 * scavenger bags alike) and, when the ring is full, frees the one farthest
 * from every playing client. C1's picker 0x528680 does the same, with one
 * addition the retained IW3 picker lacks: an item younger than 50 ms counts
 * as distance 0 (0x5287a5 compares the item's spawn time, gentity+0x2c8,
 * with level.time - 50 and, when newer, takes the zero it keeps beside the
 * running minimum). A death drops its scavenger bag and then its weapon in
 * the same frame (_damage.gsc dropScavengerForDeath, dropWeaponForDeath);
 * without the guard the weapon's drop could reclaim the bag that had just
 * landed in the ring whenever no player stood nearer to any older item. */
#ifdef OL_SERVER
#include "item_ring.h"
#include "g_shared.h"

static int spawn_time[MAX_GENTITIES];

void RecordEntitySpawnTime(unsigned number)
{
    if(number < MAX_GENTITIES) spawn_time[number] = level.time;
}

int DroppedItemIsRecent(const gentity_t *item)
{
    unsigned number;
    if(!item) return 0;
    number = (unsigned)item->s.number;
    if(number >= MAX_GENTITIES) return 0;
    return spawn_time[number] >= level.time - 50;
}
#endif
