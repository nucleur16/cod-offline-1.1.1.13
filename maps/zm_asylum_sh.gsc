// zm_asylum_sh level script.
//
// GScr_LoadScripts asks for exactly one level module per match, maps/<mapname>
// for a zm match (CODOL_ScriptEntryPoints, src/codol/script/codol_script_source.c;
// T5 keeps its zombie level scripts in maps/ too).  Without this file the server
// prints "no level script for zm_asylum_sh" and the map runs with no per-map
// setup at all.
//
// mp/basemaps.arena gives zm_asylum_sh gametype "zm", the same as zm_zombie_training,
// so the zm rules script plus this file is the whole per-map contract.
//
// MEASURED: zm_asylum_sh.ff ships sun/zm_asylum_sh.sun and vision/zm_asylum_sh.vision, where
// zm_zombie_training.ff ships neither.  That is why the vision name is passed
// here and omitted there.
//
// DELIBERATELY NOT HERE, and why:
//
//  * maps/mp/_load::main() -- the multiplayer level loader; naming it compiles
//    the whole multiplayer chain (100 scripts) into a zm match.
//  * maps/mp/zm_asylum_sh_fx::main() and maps/createfx/zm_asylum_sh_fx.gsc -- the map's
//    ambient effects (fog banks, fires, light glows, sparks, loop and interval
//    sounds) are drawn by C1 itself: on every map load the client reads both
//    rawfiles out of the map zone with its own parser and plays them locally
//    (C1.exe 0x42df70; see zm_zombie_training.gsc and
//    docs/CODOL_CLIENT_AMBIENT_EMITTERS_20260915.md).  The zone's _fx.gsc runs
//    the createfx module only inside a /# #/ developer block, so the vanilla
//    server never placed them; running it here would draw everything twice.
//  * maps/mp/_compass::setupMiniMap -- _compass is a multiplayer module, and the
//    arena entry for every zm map carries mapimage "blank".

main()
{
	maps\_zombiemode::levelMain( "zm_asylum_sh" );

	asylum_zone_init();
}

// --------------------------------------------------------------------------
// Zones.  T5 zombie_cod5_asylum.gsc:85-88 (manage_zones with the two west
// starting rooms) and asylum_zone_init :131, verbatim.  MEASURED on a
// zm_asylum_sh probe (scr_codol_pathtest zones): the map's player volumes
// carry T5's zone names and its doors / debris T5's flags
// (north_door1, north_upstairs_blocker, upstairs_north_door1/2,
// magic_box_north/south, south_upstairs_blocker, south_access_1); the two
// flagless doors between the west rooms are T5's power doors ("power_on").
// Without the graph a room only spawned while a player stood in it, so the
// rooms next to the players' never fed zombies.
// --------------------------------------------------------------------------

asylum_zone_init()
{
	level.zm_zone_initial = [];
	level.zm_zone_initial[ 0 ] = "west_downstairs_zone";
	level.zm_zone_initial[ 1 ] = "west2_downstairs_zone";

	maps\_zombiemode_zone_manager::add_adjacent_zone( "west_downstairs_zone", "west2_downstairs_zone", "power_on" );

	// path north to power
	maps\_zombiemode_zone_manager::add_adjacent_zone( "west2_downstairs_zone", "north_downstairs_zone", "north_door1" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "north_downstairs_zone", "north_upstairs_zone", "north_upstairs_blocker" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "north_upstairs_zone", "north2_upstairs_zone", "upstairs_north_door1" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "north2_upstairs_zone", "kitchen_upstairs_zone", "upstairs_north_door2" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "kitchen_upstairs_zone", "power_upstairs_zone", "magic_box_north" );

	// path south to power
	maps\_zombiemode_zone_manager::add_adjacent_zone( "west_downstairs_zone", "south_upstairs_zone", "south_upstairs_blocker" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "south_upstairs_zone", "south2_upstairs_zone", "south_access_1" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "south2_upstairs_zone", "power_upstairs_zone", "magic_box_south" );
}
