/* Native CoDOL entity section. SPDX-License-Identifier: AGPL-3.0-or-later */
#include "entities.h"
#include <string.h>
#include <stdio.h>

static char reach_failure[160];
const char *ReachEntityFailure(void) { return reach_failure; }

static const entity_field_t tables[22][ENTITY_FIELDS] = {
#include "entity_fields.inc"
};
const entity_field_t *EntityField(unsigned type, unsigned index)
{
    return type <= 255 && index < ENTITY_FIELDS ? &tables[type < 21 ? type : 21][index] : NULL;
}
static void index_code(ol_msg_t *m, unsigned number, int last)
{
    unsigned delta = number - last;
    WriteBits(m, delta == 1, 1);
    if (delta == 1) return;
    WriteBits(m, delta >= 512, 1);
    WriteBits(m, delta < 512 ? delta : number, delta < 512 ? 9 : 11);
}
static void gap(ol_msg_t *m, unsigned delta)
{
    WriteBits(m, delta == 1, 1);
    if (delta == 1) return;
    --delta;
    while (delta >= 7) { WriteBits(m, 7, 3); delta -= 7; }
    WriteBits(m, delta, 3);
}
static int protected_position(int bits) { return bits >= -83 && bits <= -78; }
int ReachEntity(entity_state_t *held, const entity_state_t *base,
                      const entity_state_t *requested, const delta_context_t *ctx)
{
    entity_state_t out;
    const entity_field_t *table;
    unsigned i;
    int same;
    if (!held || !requested || !ctx || requested->type > 255) {
        snprintf(reach_failure,sizeof(reach_failure),"invalid arguments");
        return 0;
    }
    table = tables[requested->type < 21 ? requested->type : 21];
    same = base && base->number == requested->number && base->type == requested->type;
    out = *requested; out.field[0] = requested->type;
    if (same && !memcmp(base->field+1, requested->field+1, 65*sizeof(uint32_t))) {
        *held = out; return 1;
    }
    for (i = 1; i < 66; ++i) {
        uint32_t from = same && !protected_position(table[i].bits) ? base->field[i] : 0;
        if (same && !protected_position(table[i].bits) && from == out.field[i]) continue;
        if (!same && !out.field[i]) continue;
        if (!ReachDeltaValue(table[i].bits, from, out.field[i], ctx, &out.field[i])) {
            snprintf(reach_failure,sizeof(reach_failure),
                "field %u offset 0x%x codec %d from 0x%08x to 0x%08x",
                i,table[i].offset,table[i].bits,(unsigned)from,(unsigned)out.field[i]);
            return 0;
        }
    }
    *held = out; return 1;
}
static int payload(ol_msg_t *m, const entity_state_t *f,
                    const entity_state_t *t, delta_context_t *ctx)
{
    const entity_field_t *table = tables[t->type < 21 ? t->type : 21];
    uint32_t base[66] = {0};
    unsigned char selected[66] = {1};
    unsigned i, last = 0, lc = 0;
    int same_type = f && f->type == t->type;
    for (i = 1; i < 66; ++i) {
        base[i] = same_type && !protected_position(table[i].bits) ? f->field[i] : 0;
        if ((f && !same_type) || t->field[i] != base[i] || (same_type && t->field[i] != f->field[i])) {
            selected[i] = 1; lc = i;
        }
    }
    WriteBits(m, 0, 1); /* not removed */
    WriteBits(m, 1, 1); /* changed */
    WriteBits(m, same_type, 1); /* noDeltaPos */
    WriteBits(m, lc, 7);
    gap(m, 1);
    if (!WriteDeltaValue(m, -77, 0, t->type, ctx)) return 0;
    for (i = 1; i <= lc; ++i) if (selected[i] && !(table[i].hint == 2 && i < lc)) {
        gap(m, i - last); last = i;
        if (!WriteDeltaValue(m, table[i].bits, base[i], t->field[i], ctx)) return 0;
    }
    /* C1 0x5A0DD9..0x5A0E3B visits exactly [0,lc), with the same
     * type/noDeltaPos base rule as the gap pass. The lc slot must stay in
     * the gap pass, including when it is itself hint-2. */
    for (i = 0; i < lc; ++i) if (table[i].hint == 2)
        if (!WriteDeltaValue(m, table[i].bits, base[i], t->field[i], ctx)) return 0;
    return !m->failed;
}
static int valid(const entity_state_t *entities, size_t count)
{
    size_t i;
    if (count > OL_ENTITY_SENTINEL || (count && !entities)) return 0;
    for (i = 0; i < count; ++i)
        if (entities[i].number >= OL_ENTITY_SENTINEL || entities[i].type > 255 ||
            (i && entities[i].number <= entities[i-1].number)) return 0;
    return 1;
}
int WriteEntities(ol_msg_t *m, const entity_state_t *from, size_t nf,
                        const entity_state_t *to, size_t nt, delta_context_t *ctx)
{
    size_t fi = 0, ti = 0;
    int last = -1;
    if (!m) return 0;
    if (!ctx || !valid(from, nf) || !valid(to, nt)) goto fail;
    while (fi < nf || ti < nt) {
        const entity_state_t *f = fi < nf ? &from[fi] : NULL;
        const entity_state_t *t = ti < nt ? &to[ti] : NULL;
        if (f && (!t || f->number < t->number)) {
            index_code(m, f->number, last); last = f->number;
            WriteBits(m, 1, 1); ++fi; continue;
        }
        if (f && f->number == t->number) {
            ++fi;
            if (f->type == t->type && !memcmp(f->field+1, t->field+1, 65*sizeof(uint32_t))) { ++ti; continue; }
        } else f = NULL;
        index_code(m, t->number, last); last = t->number;
        if (!payload(m, f, t, ctx)) goto fail;
        ++ti;
    }
    index_code(m, OL_ENTITY_SENTINEL, last);
    return !m->failed;
fail:
    m->failed = 1; return 0;
}
