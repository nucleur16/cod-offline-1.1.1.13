#ifndef PLAYERSTATE_H
#define PLAYERSTATE_H
#include "delta.h"
#include "player_arrays.h"
#define PLAYER_FIELDS 244
/* Semantic state must not inherit engine-header packing. */
#pragma pack(push, 8)
typedef struct {
    uint16_t offset;
    int16_t bits;
    uint8_t hint;
} player_field_t;
typedef struct { uint32_t field[PLAYER_FIELDS]; } player_fields_t;
const player_field_t *OL_PlayerField(unsigned index);
int ReachPlayerFields(player_fields_t *held, const player_fields_t *base,
                            const player_fields_t *requested, int read_origin_velocity,
                            const delta_context_t *context);
/* Writes rov, lc, gaps and the mandatory hint-2 second pass. Snapshot assembly
 * must follow this with the seven player-state array/HUD sections. */
int WritePlayerFields(ol_msg_t *msg, const player_fields_t *from,
                            const player_fields_t *to, int read_origin_velocity,
                            delta_context_t *context);
int WritePlayerState(ol_msg_t *msg, const player_fields_t *from,
                           const player_fields_t *to,
                           const player_array_updates_t *updates,
                           const hud_state_t *hud_from, const hud_state_t *hud_to,
                           int read_origin_velocity, delta_context_t *context);
#pragma pack(pop)
#endif
