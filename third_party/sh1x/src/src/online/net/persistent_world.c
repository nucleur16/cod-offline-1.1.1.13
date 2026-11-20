/* Release actual slot-owned physical resources after real empty VM shutdown.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "persistent_world.h"
#include "persistent_room.h"
#include "persistent_cleanup.h"
#include "persistent_runtime.h"
#include "vehicle_assets.h"
#include "g_shared.h"
#include "entity.h"
int PersistentWorldOwnersEmpty(void) {
    return PersistentVehicleAssetsEmpty() && PersistentVehicleDefinitionsEmpty() &&
        PersistentGroundWorldEmpty() && PersistentRCLifecycleEmpty() &&
        PersistentWheelOwnersEmpty() && PersistentVehicleTreesEmpty() &&
        PersistentConfigBanksEmpty() && PersistentAssetBindingsEmpty() &&
        PersistentWeaponRegistryEmpty();
}
int PersistentWorldOwnersReset(void) {
    unsigned i;
    if(!PersistentRuntimeEnabled()||!PersistentGameVMStopped()||
       !PersistentRoomOwnersEmpty()||level.num_entities||level.firstFreeEnt||level.lastFreeEnt)return 0;
    for(i=0;i<MAX_GENTITIES;++i)if(g_entities[i].r.inuse)return 0;
    /* These real existing owners must already have released their VM/string/
     * registry resources. Never erase live refs or a running physics world. */
    if(!PersistentConfigBanksEmpty()||!PersistentAssetBindingsEmpty()||
       !PersistentWeaponRegistryReleased()||!PersistentWheelResourcesReleased()||
       !PersistentGroundWorldCanReset())return 0;
    ResetVehicleAssets();
    if(!PersistentWeaponBindingsReset())return 0;
    return PersistentWorldOwnersEmpty();
}
