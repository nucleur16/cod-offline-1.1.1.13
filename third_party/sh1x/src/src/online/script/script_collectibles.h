/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Story intel (collectibles) and the objective info strings C1's scoreboard
 * lists them from. See script_collectibles.c for the C1 evidence. */
#ifndef SCRIPT_COLLECTIBLES_H
#define SCRIPT_COLLECTIBLES_H
#include <stddef.h>
#include <stdint.h>

/* C1 objective info configstrings: one infostring per objective slot at
 * CS 482 + index (0..31). Its server setters (0x56a3c0.., 0x56a530, 0x56a5b0,
 * 0x56a6b0, 0x56a7b0) all end in SV_SetConfigstring(0x1e2 + index); the
 * scoreboard objective list (0x463c60) reads them through the configstring
 * offsets at cl.gameState + 0x1e2*4 (0x6d0d8b8 = 0x6d0d130 + 482*4). */
#define CS_OBJECTIVE_INFO 482
#define OBJECTIVE_INFO_SLOTS 32

/* Encode one record byte as C1's 'J'/'K' profile-push data token: 'Z' for 0,
 * 'O' for 1, else two upper-case hex digits (handler 0x468bd0). out needs 3
 * bytes. Pure; unit-testable. */
void ProfilePushByteToken(uint8_t value, char out[3]);
/* "J <offset> <token>": write `value` at record-relative `offset` of the
 * client's live public playerdata record. Returns the length written, or 0
 * when it does not fit / offset is outside C1's 0x7ffc record. Pure. */
size_t FormatPublicProfilePush(char *out, size_t size, unsigned offset, uint8_t value);

#ifdef OL_SERVER
#include "scr_vm.h"
/* codol_objective_setintel( <index 0..31>, <intel> ) -- C1 0x56a7b0. */
void Scr_ObjectiveSetIntel(void);
/* codol_objective_setsort( <index 0..31>, <sort> ) -- C1 0x56a6b0 (+32). */
void Scr_ObjectiveSetSort(void);
/* codol_objective_clearinfo( <index 0..31> ) -- C1 0x56a530. */
void Scr_ObjectiveClearInfo(void);
/* <player> codol_hasCollectible( <collectibles index> ) -> 0/1. */
void Scr_HasCollectible(scr_entref_t ref);
/* <player> codol_collect( <collectibles index> ) -> 1 newly collected, 0 had it. */
void Scr_Collect(scr_entref_t ref);
#endif
#endif
