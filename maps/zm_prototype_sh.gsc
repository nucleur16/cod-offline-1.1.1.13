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
}
