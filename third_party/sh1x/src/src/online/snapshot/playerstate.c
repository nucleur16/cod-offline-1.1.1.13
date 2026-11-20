/* Native playerState field ordering; no runtime hooks or engine ABI changes. */
#include "playerstate.h"
#include <string.h>

static const player_field_t fields[PLAYER_FIELDS] = {
#include "player_fields.inc"
};

const player_field_t *OL_PlayerField(unsigned index)
{
    return index < PLAYER_FIELDS ? &fields[index] : NULL;
}

static uint32_t player_value(const player_fields_t *state, uint16_t offset)
{
    unsigned i;
    for (i = 0; i < PLAYER_FIELDS; ++i)
        if (fields[i].offset == offset) return state->field[i];
    return 0;
}

static int values_differ(const player_field_t *field,
                         const player_fields_t *from,
                         const player_fields_t *to)
{
    unsigned i = (unsigned)(field - fields);
    return from->field[i] != to->field[i];
}

/* Exact live-snapshot predicate at C1 0x5a44b0..0x5a4561.  Force cases
 * deliberately return true even when the field itself is unchanged. */
static int field_is_sent(const player_field_t *field,
                         const player_fields_t *from,
                         const player_fields_t *to, int rov,
                         const delta_context_t *context)
{
    if (field->bits == -87) {
        /* Exact C1 5A450E permits changed -87 fields when ROV is true.
         * Restrict this additional arm to replay capture until live prediction
         * has its own separately verified migration. Reader ROV=1 bypasses
         * 5A1C42..5A1CF5, which can otherwise replace explicit angles. */
        if (context->replay_view_angles && rov && values_differ(field, from, to)) return 1;
        uint32_t to_other = player_value(to, 0x010);
        uint32_t from_other = player_value(from, 0x010);
        uint32_t to_lock = player_value(to, 0x1b0);
        if (((player_value(to, 0x0ec) ^ player_value(from, 0x0ec)) & 2u) != 0) return 1;
        if (to_other & 0x800u)
            return !(from_other & 0x800u) || values_differ(field, from, to);
        if (to_lock != 2047u)
            return to_lock != player_value(from, 0x1b0) || values_differ(field, from, to);
        if (player_value(to, 0x004) == 6u || player_value(to, 0x004) == 1u ||
            (player_value(to, 0x014) & 4u))
            return values_differ(field, from, to);
        return 0;
    }
    if (field->bits == -70) {
        uint32_t word = player_value(to, 0x348);
        float value;
        memcpy(&value, &word, sizeof(value));
        if (value != 0.0f) return 1;
    }
    if (field->hint == 3) return rov;
    return values_differ(field, from, to);
}

int ReachPlayerFields(player_fields_t *held, const player_fields_t *base,
                            const player_fields_t *requested, int rov,
                            const delta_context_t *context)
{
    player_fields_t result;
    unsigned i;
    if (!held || !base || !requested || !context || (rov != 0 && rov != 1)) return 0;
    result = *base;
    for (i = 0; i < PLAYER_FIELDS; ++i) {
        uint32_t from;
        /* Omitted predicted values are reconstructed from C1's command-time
         * prediction ring, not from the delta base.  ROV=0 is selected only
         * after that prediction agrees with the requested server state, so
         * retain the authoritative value as the next server delta base. */
        if (!field_is_sent(&fields[i], base, requested, rov, context)) {
            /* Except the -87 view angles: C1 does not predict those into its
             * snapshot playerState. An omitted one keeps the delta base's
             * value there (measured live 2026-09-28: both snapshot copies
             * held viewangles 0 while the server's view was 8.69), and C1's
             * first usercmd base is built from exactly that state
             * (DefaultWireUsercmdFromFields). The -87 codec is an
             * absolute short, so holding the base value cannot desynchronise
             * a later delta. */
            result.field[i] = fields[i].bits == -87 ? base->field[i] : requested->field[i];
            continue;
        }
        from = rov && fields[i].hint == 3 ? 0 : base->field[i];
        if (!ReachDeltaValue(fields[i].bits, from, requested->field[i], context, &result.field[i])) return 0;
    }
    *held = result; return 1;
}

int WritePlayerState(ol_msg_t *msg, const player_fields_t *from,
                           const player_fields_t *to,
                           const player_array_updates_t *updates,
                           const hud_state_t *hud_from, const hud_state_t *hud_to,
                           int rov, delta_context_t *context)
{
    return WritePlayerFields(msg, from, to, rov, context) &&
        WritePlayerArrays(msg, updates, hud_from, hud_to, context);
}

static void gap(ol_msg_t *msg, unsigned delta)
{
    WriteBits(msg, delta == 1, 1);
    if (delta == 1) return;
    --delta;
    while (delta >= 15) { WriteBits(msg, 15, 4); delta -= 15; }
    WriteBits(msg, delta, 4);
}

int WritePlayerFields(ol_msg_t *msg, const player_fields_t *from,
                            const player_fields_t *to, int rov,
                            delta_context_t *context)
{
    unsigned i, lc = 0;
    int previous = -1;
    if (!msg || !from || !to || !context || (rov != 0 && rov != 1)) {
        if (msg) msg->failed = 1;
        return 0;
    }
    for (i = 0; i < PLAYER_FIELDS; ++i)
        if (field_is_sent(&fields[i], from, to, rov, context)) lc = i + 1;
    WriteBits(msg, rov, 1);
    WriteBits(msg, lc, 8);
    for (i = 0; i < lc; ++i) {
        uint32_t base;
        /* Hint-2 slots below lc-1 are consumed in a separate implicit pass.
         * If the highest changed slot is hint-2, it remains in the gap loop:
         * this guarantees it is delivered even when no ordinary field changes. */
        if (!field_is_sent(&fields[i], from, to, rov, context) ||
            (fields[i].hint == 2 && i + 1 < lc)) continue;
        gap(msg, (unsigned)((int)i - previous)); previous = i;
        base = rov && fields[i].hint == 3 ? 0 : from->field[i];
        if (!WriteDeltaValue(msg, fields[i].bits, base, to->field[i], context)) return 0;
    }
    for (i = 0; i + 1 < lc; ++i) {
        if (fields[i].hint != 2) continue;
        if (!WriteDeltaValue(msg, fields[i].bits, from->field[i], to->field[i], context)) return 0;
    }
    return !msg->failed;
}
