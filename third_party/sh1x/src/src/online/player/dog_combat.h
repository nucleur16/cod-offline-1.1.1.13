#ifndef DOG_COMBAT_H
#define DOG_COMBAT_H
struct gentity_s;
/* Native dog combat geometry. This is an implementation component, not a
 * callable dog actor: animation, movement and damage producers must call it. */
typedef struct {
    float enemyPosition[3], goal[3];
    int goalSet, inAttackRange;
} dog_attack_goal_t;
void DogCombatReset(void);
void DogCombatRelease(struct gentity_s *dog);
void DogCombatGoal(struct gentity_s *dog, struct gentity_s *enemy,
                        float attackDistance, int enemyInSyncedMelee,
                        dog_attack_goal_t *result);
/* Every other species (the zm zombie): T5 humanoid melee range, no attack
 * spots; result->goalSet is always 0. */
void ActorMeleeRangeGoal(struct gentity_s *actor, struct gentity_s *enemy,
                               float attackDistance, dog_attack_goal_t *result);
/* The zm hellhound: T5 zombie mode's hounds run in at their player and queue
 * behind each other; no attack spots. result->goalSet is always 0. */
void ZmHoundCombatGoal(struct gentity_s *dog, struct gentity_s *enemy,
                             float attackDistance, dog_attack_goal_t *result);
#endif
