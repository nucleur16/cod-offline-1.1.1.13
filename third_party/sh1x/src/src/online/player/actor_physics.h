#ifndef ACTOR_PHYSICS_H
#define ACTOR_PHYSICS_H
struct gentity_s;
/* blocked: the last gravity/step move achieved under half of the horizontal
 * displacement it was asked for (a wall, a lip, another capsule).  unstuck:
 * moves that had to lift the start out of the floor first (see physicsMove). */
/* airRise/airRiseMax: frames (and the largest single-frame rise) in which a
 * gravity move left the actor off the ground and higher than it started --
 * a launch; a step-up lands on the step and is not counted.  The per-second
 * actor diagnostic prints and clears them. */
typedef struct { float velocity[3]; int grounded; int blocked, unstuck; int airRise; float airRiseMax; } actor_physics_t;
#ifdef __cplusplus
extern "C" {
#endif
/* Native capsule movement; wishDelta is actual path/root motion this frame.
 * Caller supplies native gravity and authored step height. */
int ActorPhysicsMove(struct gentity_s *, actor_physics_t *,
                          const float *wishDelta, float seconds, float gravity, float stepHeight);
/* mode 0 gravity/step, 1 noclip, 2 no gravity, 3 vertical physics only. */
int ActorPhysicsMoveMode(struct gentity_s *, actor_physics_t *,
    const float *wishDelta, float seconds, float gravity, float stepHeight, int mode);
#ifdef __cplusplus
}
#endif
#endif
