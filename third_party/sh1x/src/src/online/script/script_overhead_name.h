/* Overhead names for coop characters that are not players.
 *
 * C1 1.1.1.13 draws no name for an actor: every overhead-name path in its
 * cgame ends in 0x439520 (the IW3 CG_DrawOverheadNames equivalent), which
 * takes a client number 0..18 and prints CS 1280+client over an entity. Its
 * script-mover caller 0x43a3b0 (gated by client dvar "772", default 0) walks
 * the snapshot's eType 6 entities and, for one whose entityState +0x64 is
 * 1..0x13, prints the name of client (value - 1) over that mover: at its
 * DObj's tag + 10 when it has one, else origin + 82 (C1 0xc84548 / 0xc84fe0).
 * MEASURED by static disassembly of C1.exe; see
 * docs/CAMPAIGN_MODE.md section 10.
 *
 * So a friendly AI's name is shown by a script_model that follows the AI and
 * carries +0x64 = k + 1, where k is a player-name slot no client occupies and
 * CS 1280+k holds the name. +0x64 is the same field C1 uses for a character
 * clone (script_character_clone.h), so the model is an invisible one
 * and the slot is one no client has: C1 then has no character to compose.
 *
 * <script_model> codol_setOverheadName( <name> ) reserves the slot (the
 * highest free one, shared by every model with the same name) and sets the
 * field; codol_setOverheadName() clears it. Coop matches only. A slot a
 * client later connects into is released at once and its models cleared, so
 * a mover never shows or composes a real player. */
#ifndef SCRIPT_OVERHEAD_NAME_H
#define SCRIPT_OVERHEAD_NAME_H
#ifdef __cplusplus
extern "C" {
#endif

/* The reserved name for player-name slot `slot`, or NULL. Borrowed. */
const char *OverheadNameForSlot(unsigned slot);
/* A client occupies `slot`: drop its reservation and clear its movers. */
void OverheadNameSlotTaken(unsigned slot);
/* Map change: forget every reservation. */
void OverheadNamesReset(void);

#ifdef __cplusplus
}
#endif
#endif
