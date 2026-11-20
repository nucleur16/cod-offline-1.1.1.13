/* Looping entity sound: the wire half. See loop_sound.h for the C1
 * provenance of the field, the bank and the consumer.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "loop_sound.h"

/* Field lookup by measured offset, like the other overlays: the tables are
 * per-eType and the same offset sits at a different index in each of them. */
static int put_entity(entity_state_t *out, unsigned offset, uint32_t value)
{
    unsigned i;
    const entity_field_t *field;
    for (i = 0; i < ENTITY_FIELDS; ++i) {
        field = EntityField(out->type, i);
        if (!field) return 0;
        if (field->offset == offset) { out->field[i] = value; return 1; }
    }
    return 0;
}

static int put_player(player_fields_t *out, unsigned offset, uint32_t value)
{
    unsigned i;
    const player_field_t *field;
    for (i = 0; i < PLAYER_FIELDS; ++i) {
        field = OL_PlayerField(i);
        if (!field) return 0;
        if (field->offset == offset) { out->field[i] = value; return 1; }
    }
    return 0;
}

int LoopSoundToWire(int loop_sound, uint32_t *out)
{
    if (!out || loop_sound < 0 || loop_sound > LOOP_SOUND_MAX) return 0;
    *out = (uint32_t)loop_sound;
    return 1;
}

int OverlayEngineLoopSound(entity_state_t *wire, const entityState_t *es)
{
    uint32_t index;
    if (!wire || !es) return 0;
    /* Every producer of the retained field reaches the wire here - the script
     * methods, the turret fire loop, the vehicle and death clears - so there is
     * one translation and no per-producer hook. Zero is a real value: it is how
     * C1's stop arrives, and the client stops re-adding the sound when it does. */
    if (!LoopSoundToWire(es->loopSound, &index)) return 0;
    return put_entity(wire, LOOP_SOUND_ENTITY_FIELD, index);
}

int OverlayPlayerLoopSound(player_fields_t *wire, unsigned sound)
{
    player_fields_t out;
    if (!wire || sound > LOOP_SOUND_MAX) return 0;
    out = *wire;
    /* Eight bits, like the entity half and like IW4's own netfield entry. A
     * state that carries no loop sound publishes 0, which is what the field
     * means: the client stops re-adding the looping alias. */
    if (!put_player(&out, LOOP_SOUND_PLAYER_FIELD, sound)) return 0;
    *wire = out;
    return 1;
}
