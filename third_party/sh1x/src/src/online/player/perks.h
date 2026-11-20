#ifndef PERKS_H
#define PERKS_H
#include "assets.h"
#include "playerstate.h"
#include <stdint.h>
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
enum { PERK_SLOTS = 8 };
/* C1 40eef0 returns 8 for a slot name outside its eight-entry list. That is
 * the Zombies rows' "no_slot" (ids 43-57, specialty_zombie_*). */
enum { PERK_NO_SLOT = PERK_SLOTS };
/* Slotless perks are held as a set of IDs below this bound. */
enum { SLOTLESS_PERK_IDS = 128 };
typedef struct { uint32_t id[PERK_SLOTS]; } perk_state_t;
typedef struct { uint32_t bits[SLOTLESS_PERK_IDS/32]; } slotless_perks_t;
typedef struct { uint32_t id; unsigned slot; } perk_definition_t;
/* coPerkStatsTable: last ASCII-case-insensitive column1 match supplies the
 * numeric ID in column0 and named slot in column5 (perk1..equipment, or
 * no_slot -> PERK_NO_SLOT). No pointers escape.
 * Returns 1 found,0 absent,-1 malformed; output unchanged on failure. */
int ReadPerkDefinition(const engine_table_t *, const char *, perk_definition_t *);
/* Server adaptation for the no_slot rows. C1 SetPerk (518a10 -> 40f0b0)
 * ends the game on them, so C1 never stores one; a Zombies player can hold
 * any number at once (every perk machine), so they are a set, not a slot.
 * These return 0 for a slotted or out-of-range definition. */
int SetSlotlessPerk(slotless_perks_t *, const perk_definition_t *);
int HasSlotlessPerk(const slotless_perks_t *, const perk_definition_t *, int *);
int UnsetSlotlessPerk(slotless_perks_t *, const perk_definition_t *);
/* C1 518a10/518b80/518ca0/518db0. These change/query perk IDs only,
 * independently of ability masks/values. Unset clears the resolved slot
 * unconditionally, even if another ID currently occupies that slot. */
int SetPerk(perk_state_t *, const perk_definition_t *);
int HasPerk(const perk_state_t *, const perk_definition_t *, int *);
int UnsetPerk(perk_state_t *, const perk_definition_t *);
void ClearPerks(perk_state_t *);
/* Eight measured 8-bit playerState netfields at 4b0..4cc. Reject values
 * outside the wire domain instead of truncating an ID to another perk.
 * Output unchanged on failure; other fields preserved. */
int OverlayPlayerPerks(player_fields_t *, const perk_state_t *);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif
