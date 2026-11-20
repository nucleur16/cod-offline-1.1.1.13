#ifndef PERK_QUERY_H
#define PERK_QUERY_H
/* Native read of a live player's selected perks by coPerkStatsTable name,
 * the same lookup the hasPerk script method makes (script_perks.c),
 * for engine consumers of perks that have no ability modifier row.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
struct playerState_s;
#ifdef __cplusplus
extern "C" {
#endif
/* 1 the perk occupies its slot, 0 absent, -1 unknown name, missing table or
 * no playerState extension. Slotted perks only; a no_slot row returns -1. */
int PlayerHasNamedPerk(const struct playerState_s *ps,const char *name);
#ifdef __cplusplus
}
#endif
#endif
