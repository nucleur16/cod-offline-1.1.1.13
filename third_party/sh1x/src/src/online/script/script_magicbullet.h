#ifndef SCRIPT_MAGICBULLET_H
#define SCRIPT_MAGICBULLET_H
#ifdef OL_SERVER

#ifdef __cplusplus
extern "C" {
#endif

/* IW4 0x497ed0: magicbullet(<weaponName>, <start>, <end>[, <attacker>]).
 * Function (not a method). See src/script_magicbullet.c for the
 * traced contract and obj/claude-workers/magicbullet-r1-report.md for the
 * disassembly evidence. */
void Scr_MagicBullet(void);
/* Native handle bridge for the shared projectile return path. */
struct gentity_s;
void MagicBulletSetOwner(struct gentity_s *projectile, struct gentity_s *owner);

#ifdef __cplusplus
}
#endif

#endif
#endif
