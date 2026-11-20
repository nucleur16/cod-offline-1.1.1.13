/* Post-VM room control cleanup, default-off native prototype.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "persistent_room.h"
#include "persistent_cleanup.h"
#include "persistent_runtime.h"
#include "radar_types.h"
#include "script_player_builtins.h"
#include "script_startup_extra.h"
#include "remote_missile.h"
#include "g_shared.h"
#include "entity.h"

int PersistentRoomOwnersEmpty(void) {
    return PersistentRadarTypesEmpty() && PersistentPlayerBuiltinsEmpty() &&
        PersistentPlayerLinksEmpty() && PersistentRemoteDriversEmpty() &&
        PersistentVehicleControlsEmpty() && PersistentRemoteMissilesEmpty() &&
        PersistentSoundOwnersEmpty();
}
int PersistentRoomOwnersReset(void) {
    unsigned i;
    if(!PersistentRuntimeEnabled()||!PersistentGameVMStopped()||
       level.num_entities||level.firstFreeEnt||level.lastFreeEnt)return 0;
    for(i=0;i<MAX_GENTITIES;++i)if(g_entities[i].r.inuse)return 0;
    /* Sound strings must already have been released by the actual
     * SV_ShutdownGameVM path, while its VM/string owner was still alive.
     * Never call a string release against a destroyed VM to force clean. */
    if(!PersistentSoundOwnersEmpty())return 0;
    if(!PersistentVehicleControlsReset())return 0;
    ResetAllRadarTypes();
    ResetPlayerBuiltinsAll();
    PlayerLinkResetAll();
    ResetMissileRemoteStates();
    return PersistentRoomOwnersEmpty();
}
