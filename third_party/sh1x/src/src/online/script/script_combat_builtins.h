/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef SCRIPT_COMBAT_BUILTINS_H
#define SCRIPT_COMBAT_BUILTINS_H
#include "scr_vm.h"
#include "g_public_mp.h"
#include <stdint.h>
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif

/* ---- Script builtins (registered from Scr_InitFunctions' CODOL block) ---- */

/* Methods on a rocket entity (classname "rocket"). */
void Scr_MissileSetTargetEnt(scr_entref_t ref);
void Scr_MissileSetTargetPos(scr_entref_t ref);
/* Function: glassradiusdamage(pos, radius, maxDamage, minDamage). */
void Scr_GlassRadiusDamage(void);
/* Method on any entity: setcanradiusdamage(bool). */
void Scr_SetCanRadiusDamage(scr_entref_t ref);
/* T5 MP entity method: damage, source, attacker, inflictor, headshot,
 * means of death, flags, weapon. Trailing arguments are optional. */
void Scr_DoDamage(scr_entref_t ref);
void Scr_StartPoisoning(scr_entref_t ref);
void Scr_StopPoisoning(scr_entref_t ref);
void Scr_IsPoisoned(scr_entref_t ref);
/* Method on a script_model: physicslaunchserver(contactPoint, force). */
void Scr_PhysicsLaunchServer(scr_entref_t ref);
/* Player methods (IW4 riot shield model attachment). */
void PlayerCmd_AttachShieldModel(scr_entref_t ref);
void PlayerCmd_DetachShieldModel(scr_entref_t ref);
void PlayerCmd_MoveShieldModel(scr_entref_t ref);
/* CoDOL overlay functions:
 *   codol_fire_nearest(src, range, damage, owner, weapon [, mod]) -> 1 hit / 0
 *   codol_radiusdamage(pos, radius, max, min, owner, weapon [, mod]) -> hit count */
void Scr_FireNearest(void);
void Scr_RadiusDamage(void);

/* ---- Radius-damage opt-out (setcanradiusdamage) ----
 * Stored in gentity_t.flags. The retained IW3 flag set does not use this
 * bit; G_FreeEntity's memset clears it with the entity. */
#define FL_NO_RADIUS_DAMAGE 0x40000000
int EntityCanRadiusDamage(const gentity_t *ent);

/* ---- Server physics launch lifecycle (physics_launch.c) ----
 * Launched script_models are server rigid bodies, published every frame as
 * C1 publishes them (pos and apos TR_INTERPOLATE). This frame function
 * steps every body, detects rest, restores the stationary trajectories and
 * notifies "physics_finished". Call once per simulation frame from
 * G_RunFrame (next to RunSoundFrame). Reset at map change. */
void RunPhysicsLaunchFrame(int time);
/* Nonzero while a launch moves this entity; G_RunFrameForEntity then skips
 * the retained item and mover paths for it. */
int PhysicsLaunchOwnsEntity(const gentity_t *ent);
void ResetPhysicsLaunches(void);

/* ---- Riot shield attachment state ----
 * Tag string index (SL) the shield model is attached to, 0 when none.
 * "Raised" means attached to tag_weapon_left. */
unsigned PlayerShieldTag(unsigned clientNum);
int PlayerShieldRaised(unsigned clientNum);
void ResetPlayerShieldState(unsigned clientNum);

/* ---- C-callable EntHandle setter ----
 * Implemented by the parent in src/enthandle.cpp:
 *   extern "C" void EntHandleSetEnt(EntHandle *h, gentity_t *ent){ h->setEnt(ent); } */
void EntHandleSetEnt(EntHandle *handle, gentity_t *ent);

#ifdef __cplusplus
}
#endif
#pragma pack(pop)
struct gentity_s;
void NotifyWeaponFired(struct gentity_s *ent,int weapon);
#endif
