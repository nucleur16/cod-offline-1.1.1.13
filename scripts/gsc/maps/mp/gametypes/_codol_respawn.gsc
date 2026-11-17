#include maps\mp\_utility;
#include common_scripts\utility;

// PROVENANCE: ADAPTED, not verbatim.  Authored 2026-09-02 for this build.
// See docs/respawn_path_fix.md.
//
// WHY THIS EXISTS ----------------------------------------------------------
// On this server the SERVER (damage_events.G4State) owns respawn timing: it
// authoritatively puts the player back in the world CODOL_G4_RESPAWN_MS after
// death and mirrors that into the VM through runtime.set_liveness() ->
// on_player_spawn().  The stock async respawn chain that WOULD apply the
// loadout is
//
//   CodeCallback_PlayerKilled -> _damage::PlayerKilled_internal
//        -> _killcam::killcam()            (blocks the whole death thread)
//        -> _playerlogic::spawnClient()
//        -> _playerlogic::waitAndSpawnClient()
//        -> _playerlogic::spawnPlayer()    <- setClass/giveLoadout/getSpawnPoint
//
// and it is demonstrably NOT reaching spawnPlayer() on respawns live: the
// heartbeat's stock_spawns counter stays pinned at the two connect-time
// spawns while kills climb, and the spawn watchdog forges the four notifies
// (arming the per-life watchers but running NO loadout, NO perks, NO spawn
// point) -- "the stock respawn path is broken for this client".  Because the
// forge only fixes the NOTIFIES, every respawn kept the previous life's
// loadout state and never re-applied perks/class/spawnpoint.
//
// THE FIX: drive the REAL spawnPlayer() on the authoritative respawn edge.
// This is the same function stock's chain would have reached and does the
// same work in stock's own order; running it here just removes the dependence
// on that chain's fragile timing.  spawnPlayer() itself raises "spawned" and
// "end_respawn", which reap any parked _killcam::killcam()/waitAndSpawnClient
// left over from the death thread, so this never double-spawns.

codolServerRespawn()
{
	self endon( "disconnect" );

	// A class must be selected or spawnPlayer()'s own giveLoadout() asserts
	// and bails half-run.  self.class is set by the first spawn's setClass();
	// self.pers["class"] is the persistent choice runtime._mk_player() seeds.
	if ( !isDefined( self.class ) || !isValidClass( self.class ) )
	{
		if ( isDefined( self.pers ) && isDefined( self.pers["class"] ) )
			self.class = self.pers["class"];
	}
	if ( !isDefined( self.class ) || !isValidClass( self.class ) )
		return;

	if ( !isDefined( self.team ) )
		return;

	// Reap a stuck stock respawn thread (waitAndSpawnClient endon("end_respawn"))
	// and clear the gate spawnClient() sets, so the death thread cannot also
	// reach spawnPlayer() and so the next natural death is not blocked by a
	// stale self.waitingToSpawn left true by a reaped waitAndSpawnClient.
	self notify( "end_respawn" );
	self.waitingToSpawn = false;

	// Threaded, exactly as _playerlogic::waitAndSpawnClient() does at :243.
	self thread maps\mp\gametypes\_playerlogic::spawnPlayer();
}
