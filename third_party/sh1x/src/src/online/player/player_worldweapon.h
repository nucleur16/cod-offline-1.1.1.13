#ifndef PLAYER_WORLDWEAPON_H
#define PLAYER_WORLDWEAPON_H
#ifdef __cplusplus
extern "C" {
#endif
/* C1 native linked script_model world guns. Call after player appearance and
 * playerState->entityState producers, before snapshot capture. */
void UpdatePlayerWorldWeapons(void);
/* Live lifecycle only. Archived entity states retain their captured selector. */
void ResetPlayerWorldWeaponEntity(unsigned entity);
void ResetPlayerWorldWeapons(void);
/* The live world-gun child a client holds in hand 0 (right) or 1 (left), as
 * an entity number, or 0 for none. C1 keeps the same two numbers at
 * gclient+0x3594/+0x3598 (570c4c/570c79/570d47), which is clientState
 * +0x12c/+0x130 (clientState is gclient+0x3468), so they reach every client;
 * its CG reads them as clientInfo+0xaf4/+0xaf8 (copy 46c979/46c982) to find
 * the gun's DObj for the third-person muzzle flash (480180..4802c1) and the
 * muzzle point (CalcMuzzlePoint 482606..48263e). */
unsigned PlayerWorldWeaponEntity(unsigned client,unsigned hand);
/* C1 0x55b5e0's weapon half: link world guns to a character clone. weapons
 * NULL copies the player's current world guns (weapon and hand tag); otherwise
 * one gun per weapon index, on the hand that weapon is held in. Fills out[]
 * and returns how many were linked. */
struct gentity_s;
unsigned LinkCloneWorldWeapons(struct gentity_s *player,struct gentity_s *clone,
    const unsigned *weapons,unsigned count,struct gentity_s **out,unsigned capacity);
/* Re-arms the per-(cause,tag) entity-link warnings. A link that cannot be
 * published is otherwise invisible, and the previous global eight-message cap
 * was exhausted during boot by one repeating tag. */
void ResetEntityLinkReports(void);
#ifdef __cplusplus
}
#endif
#endif
