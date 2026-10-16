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
	level thread asylum_fog();

	asylum_zone_init();
}

// --------------------------------------------------------------------------
// Fog.  T5 maps\createart\zombie_cod5_asylum_art.gsc fog_settings (called
// once from zombie_cod5_asylum.gsc main) sets height fog (setVolFog); no
// other T5 script changes it on this map (no hounds; turning the power on
// changes no fog).  C1 has exponential fog only, so the setting is the T5
// density at the ground floor where players spawn (z 72, the eight
// initial_spawn_points; the downstairs path nodes lie at z 64-96, upstairs
// at z 240-256, which would give 1042):
// halfway = half_dist * 2 ^ ( ( 72 - base_height ) / half_height ).
//   start 244.829  half_dist 697.262  half_height 200  base 124  -> 582.3
// Colour.  Not T5's brown 0.4196 0.3216 0.2510 any more: the map's
// vision/zm_asylum_sh.vision (MEASURED, zone rawfile) grades after the fog
// is drawn.  Its film block is in IW5's order (see zm_prototype_sh.gsc):
// desaturation 523 -0.805 (negative = extra saturation, x1.2 at the fog's
// luminance, applied before the tint), light tint 526 "1.5 1.57888 1.5",
// medium 527 "1.15 1.37 1.37", dark 528 "0.701083 0.895233 1.01064" -- every
// tint lifts green and blue over red.  Modelled per pixel (Rec.709
// luminance; tint lerped dark -> medium -> light; saturation lerped
// desaturationDark -> desaturation), the brown grades to an over-saturated
// yellow-brown 0.44 0.39 0.29 and any greyer fog drifts toward cyan.  The
// colour below is that model solved backwards for a slightly warm grey
// 0.42 0.40 0.37 after grading -- roughly target / tint per channel, with
// the warm offset shrunk by the saturation boost -- at the same luminance as
// T5's fog: 0.403 0.330 0.303.  (Under the other reading of the tint ids,
// 527 dark / 528 medium, it grades to a near-neutral 0.35 0.34 0.33.)
// Distance is unchanged.
// zm maps do not run maps\mp\_load, whose codolFogDvar sends C1's r_fog
// (dvar 1464); a previous map may have turned it off, so it is sent here.
// --------------------------------------------------------------------------

asylum_fog()
{
	level thread asylum_fog_dvar();

	setExpFog( 244.829, 582.3, 0.403, 0.330, 0.303, 1, 0 );
}

asylum_fog_dvar()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player setClientDvar( "1464", 1 );
	}
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
