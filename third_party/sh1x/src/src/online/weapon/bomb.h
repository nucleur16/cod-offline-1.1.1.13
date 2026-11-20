#ifndef BOMB_H
#define BOMB_H
#include "scr_vm.h"
#include "entity.h"
#include "cm_public.h"
struct WeaponDef;
typedef struct {
    unsigned type, guidance;
    float lifetime, acceleration, curvature, destabilization;
    int destabilization_distance;
} bomb_weapon_t;
void Scr_LaunchMortarBomb(scr_entref_t ref);
void Scr_GetProjectileGravity(void);
/* Partial native LaunchBomb: verified type4/guidance4 constructor only. */
void Scr_LaunchBomb(scr_entref_t ref);
gentity_t *SpawnMortarBomb(gentity_t *,unsigned,const float [3],const float [3]);
/* Called for actual missile/world collision; returns1 when bomb consumed. */
int BombImpact(gentity_t *ent,const trace_t *trace,const float *position);
int BombNotifyImpact(gentity_t *ent);
/* Constructor-owned ballistic bombs bypass retained guided-rocket integration. */
int BombUsesBallisticTrajectory(gentity_t *ent);
void ResetBombState(unsigned entity);
void ResetBombStates(void);
int GetNativeBombWeapon(const struct WeaponDef *,bomb_weapon_t *);
#endif
