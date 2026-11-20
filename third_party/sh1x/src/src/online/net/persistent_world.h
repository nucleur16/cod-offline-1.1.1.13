/* Specific native world resources/bindings, not full MATCH_STATICS authority.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PERSISTENT_WORLD_H
#define PERSISTENT_WORLD_H
struct vehicle_ode_world_s;
#ifdef __cplusplus
extern "C" {
#endif
int PersistentVehicleAssetsEmpty(void);
int PersistentVehicleDefinitionsEmpty(void);
int PersistentGroundWorldEmpty(void);
int PersistentGroundWorldCanReset(void);
int PersistentPhysicsWorldQuiet(const struct vehicle_ode_world_s *,unsigned *);
int PersistentRCLifecycleEmpty(void);
int PersistentWheelOwnersEmpty(void);
int PersistentWheelResourcesReleased(void);
int PersistentVehicleTreesEmpty(void);
int PersistentConfigBanksEmpty(void);
int PersistentAssetBindingsEmpty(void);
int PersistentWeaponRegistryEmpty(void);
int PersistentWeaponRegistryReleased(void);
int PersistentWeaponBindingsReset(void);
int PersistentWorldOwnersReset(void);
int PersistentWorldOwnersEmpty(void);
#ifdef __cplusplus
}
#endif
#endif
