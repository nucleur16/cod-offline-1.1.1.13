#ifndef MISSILE_STICKINESS_H
#define MISSILE_STICKINESS_H

#include "assets.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Native C1 values; never interpret these with IW3's four-value enum. */
typedef enum {
    STICK_NONE = 0,
    STICK_ALL = 1,
    STICK_ALL_NO_SENTIENTS = 2,
    STICK_GROUND = 3,
    STICK_GROUND_WITH_YAW = 4,
    STICK_FLESH = 5
} missile_stickiness_mode_t;

typedef struct {
    missile_stickiness_mode_t mode;
    unsigned char stick_to_player_or_actor;
} missile_stickiness_t;

/* Atomic read of verified C1 WeaponDef +7c8/+bde. Store with the owning native
 * weapon core, including composed cores; no process pointers or name rules. */
int ReadMissileStickiness(const mounted_weapon_t *source,
    missile_stickiness_t *out);

struct WeaponDef;
int GetNativeWeaponMissileStickiness(const struct WeaponDef *definition,
    missile_stickiness_t *out);

/* C1 52df00 / 52ee31: modes supporting attachment-relative placement. */
int MissileSupportsAttachment(const missile_stickiness_t *definition);
int NativeMissileSupportsAttachment(const struct WeaponDef *definition);

/* C1 AI trajectory prediction helper 52eaa0, not authoritative collision.
 * Velocity is the POST-reflection trajectory delta at 52fcb8.
 * The caller resolves the actual trace target to player/actor (52e740), and
 * supplies current stop-speed dvar "1193" (52e47b), not a guessed constant.
 * This decision does not mutate trajectories, attach entities or set angles. */
int PredictMissileStop(const missile_stickiness_t *definition,
    int target_is_player_or_actor, const float normal[3],
    const float reflected_velocity[3], float stop_speed);

#ifdef __cplusplus
}
#endif
#endif
