#ifndef PERK_EFFECTS_H
#define PERK_EFFECTS_H
#include "abilities.h"
#include "perks.h"

typedef struct {
    /* All perk-owned modifier bits; only declared wire value slots are used. */
    client_abilities_t abilities;
    uint32_t server_masks[4];
    /* Includes slotless numeric parameters of client-visible flag effects. */
    uint32_t server_value_masks[4];
    float server_values[128];
} perk_effects_t;

/* Rebuild from the currently selected eight perk IDs plus the optional set of
 * slotless (no_slot) perk IDs, never from pro-link
 * columns. Policy: sum every selected row's deltas, add a declared default
 * once, then clamp to declared bounds. An absent default contributes no term.
 * This is table-consistent native policy, not a recovered C1 script producer.
 * Boolean effects only set bits. Server-only numeric values use their ability
 * ID as index, independently of C1's 23 wire value slots.
 * Returns 1 success, 0 missing referenced row, -1 malformed input. Output is
 * unchanged on failure. No global state, script-owned effects or VM semantics
 * are changed. Parent merges this independently owned contribution. */
int BuildPerkEffects(const perk_state_t *selection,
    const slotless_perks_t *slotless,
    const engine_table_t *perk_table,
    const engine_table_t *ability_table, perk_effects_t *out);
#endif
