/* Crosshair friend/foe identification for C1's crosshair colouring.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef LOOKAT_H
#define LOOKAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* C1 playerState weapon-flags word ps+0x344, CG_GetCrosshairColor
 * 0x43c4e1..0x43c58c: bit 8 selects the friendly colour, bit 0x10 the enemy
 * colour (behind cg_crosshairEnemyColor and a 250 ms settle). These are
 * IW4's PWF_FRIENDLY_FIRE/PWF_ENEMY_FIRE,
 * produced by the server's per-frame look-at trace. Same bit numbers as the
 * retained IW3 weapFlags word. C1's virtualized prediction recomputes these
 * bits too, so entity identity and packed trace bounds must also be correct. */
#define WEAP_CROSSHAIR_FRIENDLY 0x8u
#define WEAP_CROSSHAIR_ENEMY    0x10u

/* Called from ClientEndFrame beside the other playing-frame publishers with
 * the entity number. Rewrites the two bits on the live playerState. */
void UpdateLookAtEntity(unsigned client);

/* Re-arms the one-shot friendly-crosshair-colour client dvar for a client
 * (C1 dvar "291", registered 0x0052b1ff as a steel blue). */
void ResetLookAtClientState(unsigned client);

#ifdef __cplusplus
}
#endif
#endif
