/* CoDOL vehicle / turret / remote-view script builtins.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef SCRIPT_VEHICLE_BUILTINS_H
#define SCRIPT_VEHICLE_BUILTINS_H
#ifdef OL_SERVER
#include "scr_vm.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
struct gentity_s;

/* ---- script vehicle methods (receiver: entity with scr_vehicle) ---- */
/* attachpath(<vehicle node>)      IW4 0x67f120, C1 0x685490 */
void Scr_AttachPath(scr_entref_t ref);
/* startpath([<vehicle node>])     IW4 0x67f360, C1 0x6858c0 */
void Scr_StartPath(scr_entref_t ref);
/* vehicle_canturrettargetpoint(<point>, [<doSightTrace>], [<ignoreEnt>])
 * IW4 0x67fef0; returns int. */
void Scr_VehicleCanTurretTargetPoint(scr_entref_t ref);
/* vehicle_finishdamage(inflictor, attacker, damage, dFlags, meansOfDeath,
 * weapon, point, dir, hitLoc, timeOffset, modelIndex, partName)
 * IW4 0x67f4d0 -> 0x481bd0; no return value. */
void Scr_VehicleFinishDamage(scr_entref_t ref);
/* vehicleturretcontrolon(<player>) / vehicleturretcontroloff(<player>)
 * IW4 0x4d1b60 -> 0x421d30 and 0x4634e0 -> 0x49a8a0. */
void Scr_VehicleTurretControlOn(scr_entref_t ref);
void Scr_VehicleTurretControlOff(scr_entref_t ref);

/* ---- vehicle spawner method (receiver: entity created with spawnflag 2) ---- */
/* vehicle_dospawn([<vehicleType>], [<owner>])  IW4 0x680280 -> 0x420670;
 * returns the spawned vehicle entity, or undefined when the spawner already
 * produced a vehicle this server frame. */
void Scr_VehicleDoSpawn(scr_entref_t ref);

/* ---- player methods ---- */
/* startac130() / stopac130()      IW4 0x48f850 / 0x450b40 */
void Scr_StartAC130(scr_entref_t ref);
void Scr_StopAC130(scr_entref_t ref);
/* remotecamerasoundscapeoff()     IW4 0x5f54d0 */
void Scr_RemoteCameraSoundscapeOff(scr_entref_t ref);
/* canplayerplacesentry()          IW4 0x5dca90 -> 0x489fe0; returns an
 * array with keys "result", "origin", "angles". */
void Scr_CanPlayerPlaceSentry(scr_entref_t ref);

/* ---- turret methods (receiver: entity with pTurretInfo) ---- */
/* maketurretsolid()               IW4 0x5ed520 -> 0x4038e0 */
void Scr_MakeTurretSolid(scr_entref_t ref);
/* setturretminimapvisible(<bool>) IW4 0x5ed0d0 -> 0x4b1310 */
void Scr_SetTurretMinimapVisible(scr_entref_t ref);

/* ---- native state and lifecycle consumed by the engine ---- */
/* Path following. Reset at level start (beside InitVehicleNodes) and
 * when a vehicle entity is freed. Think returns 1 when the vehicle is on a
 * running path and this call already produced its position, angles and
 * velocity for the frame; the retained physics integration is then skipped
 * for that vehicle. RunFrame walks the vehicle pool for a caller that cannot
 * reach the per-vehicle think. */
void VehiclePath_Reset(void);
void VehiclePath_EntityFreed(unsigned entnum);
int VehiclePath_Think(struct gentity_s *vehicle);
void VehiclePath_RunFrame(void);
/* Vehicle damage script callback (IW4 Scr_VehicleDamage lineage).
 * LoadCallback resolves maps/mp/gametypes/_callbacksetup::CodeCallback_VehicleDamage
 * once the gametype scripts are loaded. Callback takes G_Damage's argument
 * list and returns 1 when the script thread was run instead of applying the
 * damage natively; vehicle_finishdamage re-enters G_Damage under a gate. */
void VehicleDamage_LoadCallback(void);
int VehicleDamageCallback(struct gentity_s *targ, struct gentity_s *inflictor,
    struct gentity_s *attacker, const float *dir, const float *point, int damage,
    int dflags, int mod, int weapon, int hitLoc, unsigned int modelIndex,
    unsigned int partName, int timeOffset);
/* Turret controller bookkeeping (gunner ride slot owner). */
void VehicleTurretControl_Reset(void);
void VehicleTurretControl_ClientLeft(int clientNum);
struct gentity_s *VehicleTurretController(const struct gentity_s *vehicle);
/* Helicopter think: aim a vehicleTurretControlOn turret along its
 * controller's view and notify "turret_fire" while he holds attack. */
void VehicleTurretControllerThink(struct gentity_s *vehicle);
/* Per-client remote view flags produced by the player methods above. */
int PlayerAC130Active(int clientNum);
int PlayerRemoteCameraSoundscape(int clientNum);
void PlayerRemoteView_ResetClient(int clientNum);
void PlayerRemoteView_ResetAll(void);
/* Turret minimap visibility produced by setturretminimapvisible. */
int TurretMinimapVisible(const struct gentity_s *turret);
void TurretMinimap_EntityFreed(unsigned entnum);
void TurretMinimap_ResetAll(void);

/* Vehicle node record accessors. Implemented by the owner of the class-3
 * vehicle node store (src/script_vehicle_nodes.c); see the parent
 * patch in the worker report. Count is the number of retained records,
 * Value returns the raw map key text of record index or NULL. */
unsigned VehicleNodeCount(void);
const char *VehicleNodeValue(unsigned index, const char *key);

#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif
#endif
