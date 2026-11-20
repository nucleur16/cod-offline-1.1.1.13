/* Native actor records, including coherent overlapping field storage. */
#include "actors.h"
#include <string.h>
static const entity_field_t fields[24] = {
#include "actor_fields.inc"
};
const entity_field_t *ActorField(unsigned index) { return index < 24 ? &fields[index] : NULL; }
static uint32_t word(const actor_state_t *s, unsigned i)
{
    const uint8_t *p;
    if (!s) return 0;
    p = s->data + fields[i].offset;
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static int valid(const actor_state_t *s, size_t n)
{
    size_t i;
    if (n > 32 || (n && !s)) return 0;
    for (i = 0; i < n; ++i) if (s[i].number >= 32 || (i && s[i].number <= s[i-1].number)) return 0;
    return 1;
}
static void number(ol_msg_t *m, unsigned value, int last)
{
    WriteBits(m, 1, 1); WriteBits(m, value == (unsigned)(last+1), 1);
    if (value != (unsigned)(last+1)) WriteBits(m, value, 5);
}
static int payload(ol_msg_t *m, const actor_state_t *f,
                    const actor_state_t *t, delta_context_t *ctx)
{
    int i, lc = -1, previous = -1;
    for (i = 0; i < 24; ++i) if (word(f, i) != word(t, i)) lc = i;
    WriteBits(m, lc >= 0, 1);
    if (lc < 0) return !m->failed;
    WriteBits(m, lc, 5);
    for (i = 0; i <= lc; ++i) {
        unsigned gap;
        uint32_t from = word(f, i), to = word(t, i);
        if (from == to) continue;
        gap = i - previous; previous = i;
        WriteBits(m, gap == 1, 1);
        if (gap != 1) {
            --gap;
            while (gap >= 7) { WriteBits(m, 7, 3); gap -= 7; }
            WriteBits(m, gap, 3);
        }
        if (!WriteDeltaValue(m, fields[i].bits, from, to, ctx)) return 0;
    }
    return !m->failed;
}
int WriteActors(ol_msg_t *m, const actor_state_t *from, size_t nf,
                      const actor_state_t *to, size_t nt, delta_context_t *ctx)
{
    size_t fi = 0, ti = 0;
    int last = -1;
    if (!m) return 0;
    if (!ctx || !valid(from, nf) || !valid(to, nt)) goto fail;
    while (fi < nf || ti < nt) {
        const actor_state_t *f = fi < nf ? &from[fi] : NULL;
        const actor_state_t *t = ti < nt ? &to[ti] : NULL;
        if (f && (!t || f->number < t->number)) {
            number(m, f->number, last); last = f->number;
            WriteBits(m, 1, 1); ++fi; continue;
        }
        if (f && f->number == t->number) {
            ++fi;
            if (!memcmp(f->data+4, t->data+4, 0x58)) { ++ti; continue; }
        } else f = NULL;
        number(m, t->number, last); last = t->number;
        WriteBits(m, 0, 1);
        if (!payload(m, f, t, ctx)) goto fail;
        ++ti;
    }
    WriteBits(m, 0, 1); return !m->failed;
fail:
    m->failed = 1; return 0;
}
