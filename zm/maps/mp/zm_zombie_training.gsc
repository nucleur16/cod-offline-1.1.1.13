// zm_zombie_training level script.
//
// GScr_LoadScripts asks for exactly one module, maps/mp/<mapname>, for every
// asset category (src/scr_vm_main.c, GScr_LoadScripts) -- the CoDOL zones name
// their own rawfiles in that same maps/mp/ namespace even for zm
// (maps/mp/zm_zombie_training_fx.gsc) -- and the per-category source root makes
// this the file a zm match resolves it to.  A sweep of all 90 zones in
// client_shipRetail found 55 distinct .gsc rawfiles and every one is an fx
// pair, so no CoDOL zone ships a level script for any map and the server owns
// this file the same way it owns mp_killhouse_sh.gsc and mp_fav_tropical.gsc.
// Its absence is what let the isSP() crash surface in the first place
// (docs/CODOL_ZOMBIES_STARTUP_20260915.md section 1).
//
// DELIBERATELY NOT HERE, and why:
//
//  * maps\mp\_load::main().  That is the multiplayer level loader: it threads
//    _minefields, _radiation, _shutter, _destructables, _elevator,
//    _dynamic_world, _destructible, _pipes and _tweakables, and naming it would
//    compile the multiplayer chain into a zm match.  Its per-map jobs that zm
//    does need (spawn setup, vision) are either done by _zombiemode or listed
//    below as unavailable.
//
//  * maps\mp\zm_zombie_training_fx::main() and maps\createfx\...  The map's
//    ambient effects (fog banks, fires, lamp glows, sparks, loop and interval
//    sounds) are drawn by C1 itself: on every map load the client reads
//    maps/mp/<mapname>_fx.gsc and maps/createfx/<mapname>_fx.gsc out of the
//    map zone with its own parser and plays them locally (C1.exe 0x42df70,
//    called unconditionally at 0x450f5e and 0x467cdf; formats 0xc41ca4 and
//    0xc41cb8; frame update 0x42e1b0 while connected).  The zone's _fx.gsc
//    runs the createfx module only inside a /# shouldRunServerSideEffects() #/
//    developer block, so the vanilla server never placed them either.  Running
//    it here would draw every effect twice and spend server fx entities on
//    them; docs/CODOL_CLIENT_AMBIENT_EMITTERS_20260915.md has the details.
//
//  * setupMiniMap.  mp/basemaps.arena's zm_zombie_training entry carries only
//    map/longname/gametype/description/mapimage "blank" -- there is no compass
//    material named for this map to set up.

main()
{
	// See _zombiemode::init() for why this is answered rather than inferred:
	// common_scripts\utility::isSP() derives singleplayer from an "mp_" map
	// name prefix, and zm_* maps are not mp_*.  The level script runs before
	// CodeCallback_StartGameType, so the earliest entry point needs it too.
	// Routed through the shared entry point so all three zm maps take the
	// same path.  zm_zombie_training.ff ships no vision rawfile of its own,
	// but C1's startup zone common_mp does: vision/zm_zombie_training.vision
	// (with _interiors and _end beside it, docs/zone_assets/ALL_rawfile.txt),
	// and every client mounts common_mp.  levelMain() sets level.isSP = false.
	maps\mp\gametypes\_zombiemode::levelMain( "zm_zombie_training" );

	// The three actor_zombie_* spawner classnames the map ships
	// (actor_zombie_ger, actor_zombie_juggernaut, actor_zombie_dog), its 51
	// zombie_spawner structs, the treasure_chest_use box, the elec_switch
	// power, the zombie_vending perk machines and the zm_door_* progression are
	// all present in the MapEnts and all belong to the follow-on workstream.
	// Nothing reads them yet, so nothing here touches them.
}
