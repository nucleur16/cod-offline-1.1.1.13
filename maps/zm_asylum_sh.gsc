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
// Colour is T5's warm brown (fog_scale 2 and the sun colour have no C1
// equivalent).
// zm maps do not run maps\mp\_load, whose codolFogDvar sends C1's r_fog
// (dvar 1464); a previous map may have turned it off, so it is sent here.
// --------------------------------------------------------------------------

asylum_fog()
{
	level thread asylum_fog_dvar();

	setExpFog( 244.829, 582.3, 0.419608, 0.321569, 0.25098, 1, 0 );
}

asylum_fog_dvar()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player setClientDvar( "1464", 1 );
	}
}
