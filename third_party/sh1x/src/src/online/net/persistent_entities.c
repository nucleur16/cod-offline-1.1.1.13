/* Clear verified post-VM entity intent without discarding live animation refs.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "persistent_entities.h"
#include "persistent_world.h"
#include "persistent_room.h"
#include "persistent_cleanup.h"
#include "persistent_runtime.h"
#include "persistent_build.h"
#include "bomb.h"
#include "knife_attachment.h"
#include "bolt_attachment.h"
#include "player_worldweapon.h"
#include "vehicle_spawner.h"
#include "rc_spawn_influence.h"
#include "weapon_lock.h"
#include "script_combat_builtins.h"
#include "glass_world.h"
#include "model_animation.h"
#include "script_model_tree.h"
#include "script_model_rootmotion.h"
#include "map_shadow.h"
#include "perk_runtime.h"
#include "variable_zoom.h"
#include "g_shared.h"
#include "entity.h"
int PersistentEntityOwnersEmpty(void) {
    return PersistentWaterEmpty()&&PersistentHudHelpEmpty()&&
        PersistentBombsEmpty()&&PersistentKnivesEmpty()&&
        PersistentBoltsEmpty()&&PersistentWorldWeaponsEmpty()&&
        PersistentSpawnersEmpty()&&PersistentSpawnInfluencesEmpty()&&
        PersistentMissilePositionsEmpty()&&PersistentPhysicsLaunchesEmpty()&&
        PersistentGlassEmpty()&&PersistentModelAnimationsEmpty()&&
        PersistentScriptModelsEmpty()&&PersistentRootMotionEmpty()&&
        PersistentMapShadowSettled()&&PersistentPerkSourcesEmpty()&&
        PersistentZoomEmpty()
#if RESIDENT_HAS_TOKENS
        &&PersistentReviveTokensEmpty()
#endif
        ;
}
int PersistentEntityOwnersReset(void) {
    unsigned i;
    if(!PersistentRuntimeEnabled()||!PersistentGameVMStopped()||
       !PersistentRoomOwnersEmpty()||!PersistentWorldOwnersEmpty()||
       level.num_entities||level.firstFreeEnt||level.lastFreeEnt)return 0;
    for(i=0;i<MAX_GENTITIES;++i)if(g_entities[i].r.inuse)return 0;
    if(!PersistentScriptModelResourcesReleased()||!PersistentMapShadowKnown())return 0;
#if RESIDENT_HAS_TOKENS
    /* The actual connect-identity owner already revoked these before
     * SV_ClearServer. Do not silently discard unrevoked token ownership. */
    if(!PersistentReviveTokensEmpty())return 0;
#endif
    PersistentWaterReset();PersistentHudHelpReset();
    ResetBombStates();ResetKnifeAttachments();ResetBoltAttachments();
    ResetPlayerWorldWeapons();ResetVehicleSpawners();ResetRCSpawnInfluencers();
    ResetMissilePositions();ResetPhysicsLaunches();GlassWorldReset();
    ResetScriptModelTrees();ResetModelAnimations();
    ResetMapSunSampleSize();
    for(i=0;i<MAX_CLIENTS;++i)ResetPerkRuntime(i);
    VariableZoomResetAll();
    return PersistentEntityOwnersEmpty();
}
