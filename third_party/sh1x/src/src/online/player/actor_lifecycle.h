/* Native actor allocation and script/type lifetime, staged for integration.
 * No GSC spawnactor registration until AI initialization is connected. */
#ifndef ACTOR_LIFECYCLE_H
#define ACTOR_LIFECYCLE_H
#ifdef OL_SERVER
#include "sentient.h"
#include "dog_animation.h"
struct actor_lifetime_s;
typedef struct actor_lifetime_s actor_lifetime_t;
extern "C" {
/* Entity must be a freshly allocated live non-client, not a spawner. Claims
 * an actor slot and sentient together, without evicting unrelated sentients.
 * Capacity refusal does not modify the entity or another live actor. */
actor_lifetime_t *ActorAllocate(gentity_s *entity, int team);
actor_lifetime_t *ActorForEntity(gentity_s *entity);
/* Requires a preloaded original aitype main function handle. Calls the actual
 * entity script; revalidates after it runs because script may delete self.
 * No thread id is retained: Scr_FreeThread releases only the caller reference,
 * and ordinary entity script lifetime owns a yielding thread. */
/* Native perception selects enemy; GSC enemy is read-only. */
gentity_s *ActorEnemy(gentity_s *entity);
unsigned ActorEnemyModel(gentity_s *entity);
void ActorInheritSight(gentity_s *entity);
int ActorSetEnemy(gentity_s *entity, gentity_s *enemy);
int ActorSetGoal(gentity_s *entity, const float *goal);
/* 0 unavailable/blocked, 1 route direction, 2 inside authored goal volume. */
int ActorGoalDirection(gentity_s *entity, float *direction);
int ActorMoveDelta(gentity_s *entity, const float *delta);
/* AI_ANIM_MOVE_CODE root-motion consumer, once after animation advancement.
 * Native capsule gravity/slide/step are applied; traversal remains separate. */
int ActorAdvanceGoalAnimation(gentity_s *entity, int time, int useGoalWeight);
int ActorRunCorpseFrame(gentity_s *entity);
int ActorRunFrame(gentity_s *entity);
int ActorRunTypeMain(gentity_s *entity, int mainHandle);
/* Original T5 animscript entrypoints, resolved during map script loading.
 * No numeric T5 function/animation indices are copied to C1. The native AI
 * selects a descriptor; these are retained VM code handles for this map only. */
typedef struct {
    int init, stop, move, combat, death, pain, flashed, turn, jump;
} actor_script_table_t;
int ActorInitScripts(gentity_s *, const actor_script_table_t *);
/* Call only in the native AI phase with an empty VM operand stack, never from
 * a GSC builtin: T5 transitions run the pending script queue before entry.
 * The caller supplies actual native move/animation modes, not C1 offsets. */
int ActorSwitchScript(gentity_s *, int function, int moveMode, int animMode);
void ActorStopScript(gentity_s *);
int ActorScriptModes(gentity_s *, int *moveMode, int *animMode);
/* Called only after native AI initialization and selected DObj tree creation.
 * Enables capture from that real tree, never an invented idle pose. */
/* Bind real dog tree after aitype model creation, before animscript init. */
struct XAnimTree_s *ActorOwnedTree(gentity_s *entity);
int ActorPrepareAnimation(gentity_s *entity);
int ActorFinishInitialization(gentity_s *entity, unsigned treeSelector);
typedef struct {
    struct gentity_s *entity;
    unsigned number, generation, enemyModel, team;
    const struct XAnimTree_s *tree;
} actor_corpse_info_t;
int ActorCorpseGet(unsigned index, actor_corpse_info_t *out);
int ActorSlotIdle(unsigned slot);
void ActorSlotHold(unsigned slot);
int ActorCaptureSources(dog_anim_capture_source_t *out,
                             unsigned capacity, unsigned *count);
/* Called after the existing engine DObj tick, once per archive frame. */
int ActorArchivePose(int frame, unsigned time);
/* Sentient_Free must call Forget before returning the slot to its allocator.
 * Forget owns neither the entity nor its DObj: G_FreeEntity releases those. */
void ActorForget(gentity_s *entity);
void ActorRelease(gentity_s *entity);
/* Same reset epoch as Sentient_ResetAll; do not release old EntHandles after
 * the shared handle registry has already been reset. */
void ActorLifetimeReset(void);
}
#endif
#endif
