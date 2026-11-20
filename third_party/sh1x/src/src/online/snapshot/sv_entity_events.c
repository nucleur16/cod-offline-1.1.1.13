#ifdef OL_SERVER
/* Entity event-generation resets.
 *
 * These live apart from sv_entities.c on purpose. Clearing a slot's
 * event history has to reach every subsystem that keeps per-entity state -
 * vehicles, model animation, missiles, glass - so this file drags in most of
 * the server. The snapshot collectors next door depend on none of that, and
 * keeping them in their own translation unit is what lets them be linked
 * against on their own by tests and tools. */
#include "server.h"
#include "sv_entities.h"
#include "playerstate_ext.h"
#include "entitystate_ext.h"
#include "remote_missile.h"
#include "weapon_lock.h"
#include "model_animation.h"
#include "vehicle_spawner.h"
#include "script_combat_builtins.h"
#include "glass_world.h"
#include "bomb.h"
#include "player_worldweapon.h"
#include "knife_attachment.h"
#include "bolt_attachment.h"
#include "rc_spawn_influence.h"
#include "script_model_tree.h"

void ResetEntityEventGeneration(unsigned number)
{
    ResetRCSpawnInfluencer(number);
    ResetKnifeAttachment(number);
    ResetBoltAttachment(number);
    ResetPlayerWorldWeaponEntity(number);
    ResetBombState(number);
    ResetVehicleSpawner(number);
    ResetEntityModelAnimation(number);
    ResetScriptModelTree(number);
    ResetMissilePosition(number);
    ResetMissileRemoteState(number);
    /* A reused entity slot starts a new event history; captured snapshots and
     * archived frames keep the epoch they were taken with. A player entity
     * going away takes its event epochs with it; the retained counter is
     * zeroed by the gclient memset on the next spawn. */
    ResetEntityEventEpoch(number);
}

void ResetEntityEventGenerations(void)
{
    ResetRCSpawnInfluencers();
    ResetKnifeAttachments();
    ResetBoltAttachments();
    ResetPlayerWorldWeapons();
    ResetBombStates();
    ResetVehicleSpawners();
    ResetMissilePositions();
    ResetPhysicsLaunches();
    GlassWorldReset();
    ResetMissileRemoteStates();
    ResetEntityEventEpochs();
    /* A teardown, unlike a slot recycle: the ring and the archive are
     * about to be rebuilt, so their captured histories go too. */
    EntityStateExtClearAll();
}
#endif
