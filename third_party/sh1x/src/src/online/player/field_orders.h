/* SPDX-License-Identifier: AGPL-3.0-or-later
 * C1 field orders: the four per-slot playerState words the client's
 * field_orders HUD menu reads, and the script methods that write them.
 *
 * Evidence (C1.exe 1.1.1.13, base 0x400000):
 *
 *   Writers, four player methods whose names C1 stripped but whose usage
 *   strings survive at 0xc53148..0xc531c4:
 *     SetFOGoal( <index>, <value> )     0x518e80 -> ps+0x1b4 + index*4
 *     SetFOProgress( <index>, <value> ) 0x518f70 -> ps+0x1bc + index*4
 *     SetFOReward( <index>, <value> )   0x519060 -> ps+0x1c4 + index*4
 *     SetFOStatus( <index>, <value> )   0x519150 -> ps+0x1cc + index*4
 *   Each requires a player entity and exactly two arguments, and stores
 *   (Scr_GetInt(1) & 0xff) through ent->client (+0x17c) with no bound on the
 *   index (0x518f3a..0x518f54).
 *
 *   Readers, the menu expression operands 213..217 (name table 0xcd08a8):
 *     getFOGoal(i)     0x5e3120  ps+0x1b4+4i  the row of dvar fo_table
 *     getFOProgress(i) 0x5e3170  ps+0x1bc+4i
 *     getFOPercent(i)  0x5e3260  progress / atoi(tablelookupbyrow(fo_table,
 *                                goal, 5)), smoothed per slot
 *     getFOReward(i)   0x5e31c0  ps+0x1c4+4i  shown as locstring("@" + col 4,
 *                                reward), e.g. FO_XP "&&1 XP"
 *     getFOStatus(i)   0x5e3210  ps+0x1cc+4i  the hint line draws while 0
 *   all through the cg pointer *(0x016d3b58 + 4*localClient) ^ 0x490b400c.
 *
 *   Wire: player_fields.inc carries 0x1b4/0x1b8/0x1bc/0x1c0/0x1c4/0x1c8
 *   at 8 bits and 0x1cc/0x1d0 at 2 bits. Two slots, so index is 0 or 1.
 *
 * The values belong to the playerState (C1 keeps them inside it), so they live
 * in ps_ext_t and travel with every captured, archived and spectator
 * copy, like the killstreak HUD block beside them. */
#ifndef FIELD_ORDERS_H
#define FIELD_ORDERS_H
#include <stdint.h>
#define FIELD_ORDER_SLOTS 2
#pragma pack(push, 8)
typedef struct {
    uint8_t goal[FIELD_ORDER_SLOTS];      /* fo_table row, 0 = none */
    uint8_t progress[FIELD_ORDER_SLOTS];
    uint8_t reward[FIELD_ORDER_SLOTS];
    uint8_t status[FIELD_ORDER_SLOTS];    /* C1 stores a byte; 2 wire bits */
} field_orders_t;
#pragma pack(pop)

#ifdef OL_SERVER
#include "player.h"
#include "scr_vm.h"
#include "playerstate.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
void Scr_SetFOGoal(scr_entref_t ref);
void Scr_SetFOProgress(scr_entref_t ref);
void Scr_SetFOReward(scr_entref_t ref);
void Scr_SetFOStatus(scr_entref_t ref);
/* A connecting or leaving client slot starts with no field orders. */
void ResetClientFieldOrders(unsigned client);
/* Publish the eight words of a captured playerState onto its C1 wire fields.
 * Pure over (wire, block): returns 0 when a wire field is missing. */
int OverlayFieldOrderBlock(player_fields_t *wire, const field_orders_t *block);
int OverlayFieldOrders(player_fields_t *wire, const playerState_t *ps);
/* The recipient's own orders unless it is a spectator, then the shown state's. */
int OverlayFieldOrdersFor(player_fields_t *wire, const playerState_t *ps, unsigned recipient);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif
#endif
