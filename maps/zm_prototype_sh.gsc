// zm_prototype_sh level script.
//
// GScr_LoadScripts asks for exactly one level module per match, maps/<mapname>
// for a zm match (CODOL_ScriptEntryPoints, src/codol/script/codol_script_source.c;
// T5 keeps its zombie level scripts in maps/ too).  Without this file the server
// prints "no level script for zm_prototype_sh" and the map runs with no per-map
// setup at all.
//
// mp/basemaps.arena gives zm_prototype_sh gametype "zm", the same as zm_zombie_training,
// so the zm rules script plus this file is the whole per-map contract.
//
// MEASURED: zm_prototype_sh.ff ships sun/zm_prototype_sh.sun and vision/zm_prototype_sh.vision, where
// zm_zombie_training.ff ships neither.  That is why the vision name is passed
// here and omitted there.
//
// DELIBERATELY NOT HERE, and why:
//
//  * maps/mp/_load::main() -- the multiplayer level loader; naming it compiles
//    the whole multiplayer chain (100 scripts) into a zm match.
//  * maps/mp/zm_prototype_sh_fx::main() and maps/createfx/zm_prototype_sh_fx.gsc -- the map's
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
	maps\_zombiemode::levelMain( "zm_prototype_sh" );
	hideNavigationArrows();
}

// The yellow floor arrows.  MEASURED from the zone MapEnts
// (alpha0022 resources/map_spawns.proposed.json, zm_prototype_sh): 14
// script_brushmodels, script_noteworthy "navigation_arrows", under 13
// arrows_to_* targetnames (brush models *12..*25; arrows_to_windows has
// two).  They are the zombie-training tutorial's objective pointers, shared
// with zm_zombie_training's layout; nothing on this map drives them, so they
// are hidden for the whole match (as zm_zombie_training.gsc
// training_setup() does with its own set), not deleted.
hideNavigationArrows()
{
	// getEntArray answers only the engine's own keys (targetname, classname,
	// ...); a script_noteworthy query returns undefined, so the arrows are
	// found by their targetnames.
	names = strTok( "arrows_to_first arrows_to_second arrows_to_third arrows_to_fourth arrows_to_fifth arrows_to_windows arrows_to_stairs arrows_to_power arrows_to_door arrows_to_box arrows_to_speedcola arrows_to_m14 arrows_to_m16", " " );
	hidden = 0;
	for ( i = 0; i < names.size; i++ )
	{
		arrows = getEntArray( names[ i ], "targetname" );
		if ( !isDefined( arrows ) )
			continue;
		for ( j = 0; j < arrows.size; j++ )
		{
			arrows[ j ] notSolid();
			arrows[ j ] hide();
			hidden++;
		}
	}
	codolprintln( "CoDOL zm_prototype_sh: " + hidden + " navigation arrows hidden" );
}
