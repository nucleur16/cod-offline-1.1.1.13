/* CoDOL perk slots. SPDX-License-Identifier: AGPL-3.0-or-later */
#include "perks.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* C1 40eef0 indexes these eight names at cd915c. */
static const char *const slots[PERK_SLOTS] = {
    "perk1", "perk2", "perk3", "perk4",
    "upgrade1", "upgrade2", "upgrade3", "equipment"
};
static int same_name(const char *a, const char *b)
{
    unsigned x, y;
    if (!a || !b) return 0;
    do {
        x=(unsigned char)*a++; y=(unsigned char)*b++;
        if (x>='A' && x<='Z') x+='a'-'A';
        if (y>='A' && y<='Z') y+='a'-'A';
        if (x!=y) return 0;
    } while (x);
    return 1;
}
int ReadPerkDefinition(const engine_table_t *table, const char *name,
                            perk_definition_t *out)
{
    int row;
    if (!table || !name || !*name || !out || table->rowCount<0 || table->columnCount<6 ||
        (table->rowCount && !table->values) ||
        (size_t)table->rowCount>SIZE_MAX/sizeof(*table->values)/(size_t)table->columnCount) return -1;
    for (row=table->rowCount-1; row>=0; --row) {
        const char *const *cells=table->values+(size_t)row*table->columnCount;
        perk_definition_t result;
        char *end; long long id;
        if (!same_name(cells[1], name)) continue;
        if (!cells[0] || !*cells[0] || !cells[5] || !*cells[5]) return -1;
        errno=0; id=strtoll(cells[0], &end, 10);
        /* C1 uses atoi; reject malformed/overflow data instead of allowing
         * a wrapped index. Zero is the empty slot sentinel. */
        if (errno==ERANGE || end==cells[0] || *end || id<=0 || id>INT32_MAX) return -1;
        result.id=(uint32_t)id;
        for (result.slot=0; result.slot<PERK_SLOTS; ++result.slot)
            if (same_name(cells[5], slots[result.slot])) break;
        if (result.slot==PERK_SLOTS && !same_name(cells[5], "no_slot")) return -1;
        *out=result; return 1;
    }
    return 0;
}
static int valid(const perk_definition_t *definition)
{
    return definition && definition->slot<PERK_SLOTS &&
        definition->id>0 && definition->id<=INT32_MAX;
}
int SetPerk(perk_state_t *state, const perk_definition_t *definition)
{
    if (!state || !valid(definition)) return 0;
    state->id[definition->slot]=definition->id; return 1;
}
int HasPerk(const perk_state_t *state, const perk_definition_t *definition, int *out)
{
    if (!state || !out || !valid(definition)) return 0;
    *out=state->id[definition->slot]==definition->id; return 1;
}
int UnsetPerk(perk_state_t *state, const perk_definition_t *definition)
{
    if (!state || !valid(definition)) return 0;
    state->id[definition->slot]=0; return 1;
}
static int slotless(const perk_definition_t *definition)
{
    return definition && definition->slot==PERK_NO_SLOT &&
        definition->id>0 && definition->id<SLOTLESS_PERK_IDS;
}
int SetSlotlessPerk(slotless_perks_t *set, const perk_definition_t *definition)
{
    if (!set || !slotless(definition)) return 0;
    set->bits[definition->id>>5]|=UINT32_C(1)<<(definition->id&31); return 1;
}
int HasSlotlessPerk(const slotless_perks_t *set, const perk_definition_t *definition, int *out)
{
    if (!set || !out || !slotless(definition)) return 0;
    *out=!!(set->bits[definition->id>>5]&(UINT32_C(1)<<(definition->id&31))); return 1;
}
int UnsetSlotlessPerk(slotless_perks_t *set, const perk_definition_t *definition)
{
    if (!set || !slotless(definition)) return 0;
    set->bits[definition->id>>5]&=~(UINT32_C(1)<<(definition->id&31)); return 1;
}
void ClearPerks(perk_state_t *state)
{
    if (state) memset(state, 0, sizeof(*state));
}
/* perks_info_hd (common_mp) draws the perkN and upgradeN name and icon at
 * the same rect, each visible when C1's getperk (0x5de6db, the slot's name or
 * "specialty_null" for 0) is not specialty_null, so a slot with its Pro set
 * must reach the client with perkN empty or the Pro draws over the base perk.
 * upgradeN holds only Pros of perkN perks (coPerkStatsTable column 8). The
 * server keeps both IDs: abilities and HasPerk read the selection itself. */
static uint32_t published_id(const perk_state_t *state, unsigned slot)
{
    if (slot<3 && state->id[4+slot]) return 0;
    return state->id[slot];
}
int OverlayPlayerPerks(player_fields_t *out, const perk_state_t *state)
{
    player_fields_t next;
    unsigned slot, field;
    if (!out || !state) return 0;
    next=*out;
    for (slot=0; slot<PERK_SLOTS; ++slot) {
        uint32_t id=published_id(state, slot);
        if (id>255) return 0;
        for (field=0; field<PLAYER_FIELDS; ++field) {
            const player_field_t *definition=OL_PlayerField(field);
            if (definition->offset!=0x4b0+4*slot) continue;
            if (definition->bits!=8 || definition->hint!=0) return 0;
            next.field[field]=id;
            break;
        }
        if (field==PLAYER_FIELDS) return 0;
    }
    *out=next; return 1;
}
