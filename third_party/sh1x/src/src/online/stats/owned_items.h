/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef OWNED_ITEMS_H
#define OWNED_ITEMS_H
#include <stddef.h>
#include <stdint.h>
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
/* What a player has bought in the lobby store.
 *
 * The lobby's marketplace inventory (resources/lobby.sqlite) is the authority
 * for purchases, and nothing in the client-uploaded playerdata says what was
 * bought: a class can name any item id. So the lobby exports every purchased
 * row it holds for a player beside that player's mpdata_s, in the directory
 * this server already reads profiles from (profilePath):
 *
 *     owned_items.<uid>.txt   when the lobby keys data per uid
 *     owned_items.txt         the shared file otherwise
 *
 * One row per line, '#' starts a comment:
 *
 *     itemId  expiryEpoch  qty
 *
 * expiryEpoch 0 (or 0xFFFFFFFF) is a permanent purchase, anything else is the
 * end of a rental; qty 0 is a spent stack. These are the inventory record's
 * own fields (dw_marketplace_buy.save_owned), so a row the client reads as
 * owned (qty > 0, permanent or not yet expired) reads as owned here too.
 *
 * The export holds purchases only -- not the lobby's starter inventory or the
 * items it grants because a class already names them -- so it answers "did
 * this player buy it", which is what a Pro perk upgrade is gated on. */

/* Look item up in one manifest's text. Returns 1 owned, 0 not owned (no row,
 * spent, expired). A malformed line is skipped, never an error. */
int OwnedItemsLookup(const char *text,size_t length,unsigned item,uint32_t now);

/* Read the manifest for uid from directory (per-uid file first, then the
 * shared one, as ReadSecureProfile walks mpdata_s) and look item up.
 * Returns 1 owned, 0 not owned or no manifest (nothing bought), -1 when the
 * directory is unset or a manifest exists but cannot be read. */
int ReadOwnedItem(const char *directory,uint64_t uid,unsigned item,uint32_t now);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif
