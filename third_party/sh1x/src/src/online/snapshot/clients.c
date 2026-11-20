/* Native clientState snapshot section. Complete arrays, sorted by client number. */
#include "clients.h"
#include <string.h>
#include <math.h>
static const entity_field_t fields[CLIENT_FIELDS] = {
#include "client_fields.inc"
};
const entity_field_t *ClientField(unsigned index)
{
    return index < CLIENT_FIELDS ? &fields[index] : NULL;
}
int OverlayClientAppearance(client_state_t *out,const client_appearance_t *appearance)
{
    client_state_t next; unsigned i; int empty=0;
    if(!out || !appearance || appearance->model>=2048) return 0;
    for(i=0;i<CLIENT_ATTACHMENTS;++i) {
        unsigned model=appearance->attach_model[i],tag=appearance->attach_tag[i];
        if(model>=2048 || tag>=64 || (!model && tag) || (empty && model)) return 0;
        if(!model) empty=1;
    }
    next=*out;
    for(i=0;i<CLIENT_FIELDS;++i) {
        unsigned offset=fields[i].offset;
        if(offset==0x14) next.field[i]=appearance->model;
        else if(offset>=0x20 && offset<=0x5c)
            next.field[i]=appearance->attach_model[(offset-0x20)/4];
        else if(offset>=0x60 && offset<=0x9c)
            next.field[i]=appearance->attach_tag[(offset-0x60)/4];
    }
    *out=next; return 1;
}
int OverlayClientWorldWeapons(client_state_t *out,const unsigned entities[2])
{
    client_state_t next; unsigned i,found=0;
    if(!out || !entities || entities[0]>=2047 || entities[1]>=2047) return 0;
    next=*out;
    for(i=0;i<CLIENT_FIELDS;++i) {
        if(fields[i].offset==0x12c) { next.field[i]=entities[0]; found|=1; }
        else if(fields[i].offset==0x130) { next.field[i]=entities[1]; found|=2; }
    }
    if(found!=3) return 0;
    *out=next; return 1;
}
int OverlayClientAbilities(client_state_t *out,const client_abilities_t *abilities)
{
    client_state_t next; unsigned i;
    if(!out || !abilities) return 0;
    for(i=0;i<ABILITY_VALUES;++i) if(!isfinite(abilities->values[i])) return 0;
    next=*out;
    /* C1 46c988..46c9ec copies these four words and 23 values into
     * clientInfo and the local ps. Helper 432cc0 copies 2*8+7 floats. */
    for(i=0;i<CLIENT_FIELDS;++i) {
        unsigned offset=fields[i].offset;
        if(offset>=0xc0 && offset<=0xcc)
            next.field[i]=abilities->masks[(offset-0xc0)/4];
        else if(offset>=0xd0 && offset<=0x128)
            memcpy(next.field+i,abilities->values+(offset-0xd0)/4,sizeof(uint32_t));
    }
    *out=next; return 1;
}
int ReachClient(client_state_t *held, const client_state_t *base,
                      const client_state_t *requested, const delta_context_t *ctx)
{
    client_state_t out;
    unsigned i;
    if (!held || !requested || !ctx || requested->number > 31) return 0;
    if (base && base->number != requested->number) return 0;
    out = *requested;
    for (i = 0; i < CLIENT_FIELDS; ++i) {
        uint32_t from = base ? base->field[i] : 0;
        if (from == out.field[i]) continue;
        if (!ReachDeltaValue(fields[i].bits, from, out.field[i], ctx, &out.field[i])) return 0;
    }
    *held = out; return 1;
}
static int valid(const client_state_t *s, size_t n)
{
    size_t i;
    if (n > 32 || (n && !s)) return 0;
    for (i = 0; i < n; ++i)
        if (s[i].number >= 32 || (i && s[i].number <= s[i-1].number)) return 0;
    return 1;
}
static void number(ol_msg_t *m, unsigned value, int last)
{
    WriteBits(m, 1, 1);
    WriteBits(m, value == (unsigned)(last + 1), 1);
    if (value != (unsigned)(last + 1)) WriteBits(m, value, 5);
}
static int payload(ol_msg_t *m, const client_state_t *f,
                    const client_state_t *t, delta_context_t *ctx)
{
    int lc = -1, previous = -1;
    unsigned i;
    for (i = 0; i < CLIENT_FIELDS; ++i)
        if (t->field[i] != (f ? f->field[i] : 0)) lc = i;
    WriteBits(m, lc >= 0, 1);
    if (lc < 0) return !m->failed;
    WriteBits(m, lc, 7);
    for (i = 0; i <= (unsigned)lc; ++i) {
        uint32_t base = f ? f->field[i] : 0;
        int gap;
        if (t->field[i] == base) continue;
        gap = (int)i - previous; previous = i;
        while (--gap) WriteBits(m, 0, 1); /* unary gap */
        WriteBits(m, 1, 1);
        if (!WriteDeltaValue(m, fields[i].bits, base, t->field[i], ctx)) return 0;
    }
    return !m->failed;
}
int WriteClients(ol_msg_t *m, const client_state_t *from, size_t nf,
                       const client_state_t *to, size_t nt, delta_context_t *ctx)
{
    size_t fi = 0, ti = 0;
    int last = -1;
    if (!m) return 0;
    if (!ctx || !valid(from, nf) || !valid(to, nt)) goto fail;
    while (fi < nf || ti < nt) {
        const client_state_t *f = fi < nf ? &from[fi] : NULL;
        const client_state_t *t = ti < nt ? &to[ti] : NULL;
        if (f && (!t || f->number < t->number)) {
            number(m, f->number, last); last = f->number;
            WriteBits(m, 1, 1); ++fi; continue;
        }
        if (f && f->number == t->number) {
            ++fi;
            if (!memcmp(f->field, t->field, sizeof(t->field))) { ++ti; continue; }
        } else f = NULL;
        number(m, t->number, last); last = t->number;
        WriteBits(m, 0, 1);
        if (!payload(m, f, t, ctx)) goto fail;
        ++ti;
    }
    WriteBits(m, 0, 1);
    return !m->failed;
fail:
    m->failed = 1; return 0;
}
