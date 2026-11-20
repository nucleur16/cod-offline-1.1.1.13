#ifndef PLAYER_ARRAYS_H
#define PLAYER_ARRAYS_H
#include "delta.h"
#define HUD_FIELDS 42
#define HUD_SLOTS 31
/* Semantic state must not inherit engine-header packing. */
#pragma pack(push, 8)
typedef struct {
    uint8_t stats_mask;
    int32_t stats[4];
    uint16_t clip_mask, weapons_mask, weapon_index_mask, reserve_mask;
    int16_t clip_index[16];
    uint8_t clip_presence[16];
    uint16_t clip_ammo[16][2];
    uint16_t weapon_index[15];
    uint8_t weapon_flags[15];
    uint16_t reserve_type[16], reserve_ammo[16];
} player_array_updates_t;
typedef struct { uint32_t field[HUD_FIELDS]; } hud_element_t;
/* CoDOL hudelem-relative offset for the wire-order field; UINT_MAX if invalid. */
unsigned HudFieldOffset(unsigned index);
const char *ReachHudFailure(void);
/* Player-state section 5, the objective and waypoint array at ps+0x5D0.
 *
 * Measured in C1: MSG_ReadDeltaPlayerstate gates the section with one bit at
 * 0x5a1f79, then loops a hard-coded 32 slots (0x5a1f9c) of stride 0x24
 * (0x5a1fd6). Each slot reads a raw 3-bit state (0x5a1fa6) that sits outside
 * the field table, then a six-field delta struct through the generic reader
 * 0x5a0a40 against the table at .rdata:0xc39818. The consumers are the compass
 * drawer 0x4321b0 and the on-screen drawer 0x432790.
 *
 * The field table is not sorted by offset; this is its order, and the indices
 * below are what the gap coder counts in:
 *   0 origin.x (0x04, float)    1 origin.y (0x08, float)
 *   2 origin.z (0x0c, float)    3 icon     (0x20, 12 bits)
 *   4 entNum   (0x10, 11 bits)  5 unknown  (0x14, 4 bits, unread by both drawers)
 *
 * state 0 is the free slot and is how a deletion is expressed; only 1 and 4
 * draw (0x4328b5). entNum 0x7ff means use the world origin (0x4328c5); any
 * other value is read as a client number, not a general entity number. icon
 * indexes CS[4275 + icon] (0x453e72), the same material block hudelems use,
 * and icon 0 falls back to the client's built-in "objective" material. */
#define OBJECTIVE_SLOTS 32
#define OBJECTIVE_FIELDS 6
#define OBJECTIVE_NONE 0x7ffu
typedef struct {
    uint32_t state;                          /* raw 3 bits, outside the table */
    uint32_t field[OBJECTIVE_FIELDS];  /* table order, see above */
} ol_objective_t;

/* The trailing per-player sections: 5 (objectives) and 6 (HUD elements). They
 * share one carrier because they share the same from/to delta plumbing. */
typedef struct {
    unsigned count[2]; /* archival first, then current */
    hud_element_t elements[2][HUD_SLOTS];
    ol_objective_t objectives[OBJECTIVE_SLOTS];
} hud_state_t;
int ReachHud(hud_state_t *held, const hud_state_t *base,
                   const hud_state_t *requested, const delta_context_t *context);
/* Arrays are updates: omitted slots retain their base values. HUD states are
 * complete packed arrays: reducing count deletes the tail. NULL means empty. */
int WritePlayerArrays(ol_msg_t *msg, const player_array_updates_t *updates,
                            const hud_state_t *hud_from, const hud_state_t *hud_to,
                            delta_context_t *context);
#pragma pack(pop)
#endif
