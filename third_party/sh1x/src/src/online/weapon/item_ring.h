/* Dropped-item ring support. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef ITEM_RING_H
#define ITEM_RING_H
struct gentity_s;

/* C1 G_InitGentity 0x5724e9..0x5724ee stamps every new entity with the level
 * time (gentity+0x2c8). The retained gentity has no such field, so the
 * retained G_InitGentity (_g_utils_mp.asm) records it here by number. */
void RecordEntitySpawnTime(unsigned number);
/* C1's dropped-item slot picker 0x528680 treats an item spawned within the
 * last 50 ms (0x52879d..0x5287b2) as if a player stood on it, so it is not
 * the item reclaimed. Called by the retained Drop_Item for each candidate. */
int DroppedItemIsRecent(const struct gentity_s *item);

#endif
