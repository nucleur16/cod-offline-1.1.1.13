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
//  * maps\mp\zm_zombie_training_fx::main().  The effects module does exist, as
//    a rawfile inside zm_zombie_training.ff.  Whether Scr_ReadFile can reach a
//    CoDOL zone rawfile is unproven: its fastfile fallback goes through the
//    retained DB_FindXAssetHeader path, not the CoDOL archive registry, and the
//    only rawfile category wired to the registry in Scr_ReadFile is
//    animtrees/ (src/cscr_parser.cpp).  A `file::function` call is a
//    compile-time dependency, so getting that wrong is a fatal load error, not
//    a missing effect.  Effects are cosmetic; this is left for the pass that
//    can verify the rawfile path live.
//
//  * VisionSetNaked / VisionSetPain.  zm_zombie_training.ff ships neither
//    vision/zm_zombie_training.vision nor sun/zm_zombie_training.sun, where
//    zm_prototype_sh.ff, zm_asylum_sh.ff and every mp zone ship both.  Asking
//    for a vision name that is in no mounted zone is the open risk recorded as
//    audit row 18 of the same document, and the multiplayer _load.gsc:70,74 is
//    where it would have been asked.
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
	// same path.  undefined is deliberate: zm_zombie_training.ff ships no
	// vision and no sun rawfile, unlike zm_prototype_sh and zm_asylum_sh,
	// so nothing is asked for.  levelMain() sets level.isSP = false.
	maps\mp\gametypes\_zombiemode::levelMain( undefined );

	// The three actor_zombie_* spawner classnames the map ships
	// (actor_zombie_ger, actor_zombie_juggernaut, actor_zombie_dog), its 51
	// zombie_spawner structs, the treasure_chest_use box, the elec_switch
	// power, the zombie_vending perk machines and the zm_door_* progression are
	// all present in the MapEnts and all belong to the follow-on workstream.
	// Nothing reads them yet, so nothing here touches them.
}
