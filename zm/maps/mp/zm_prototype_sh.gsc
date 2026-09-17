// zm_prototype_sh level script.
//
// GScr_LoadScripts asks for exactly one module, maps/mp/<mapname>, for every
// asset category (src/scr_vm_main.c), and the per-category source root is what
// makes that file zm's rather than multiplayer's.  Without this file the server
// prints "no level script for zm_prototype_sh" and the map runs with no per-map setup at
// all -- and because the shared tree has no maps/mp/zm_prototype_sh.gsc either, there was
// nothing for it to fall back to.
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
//  * maps/mp/zm_prototype_sh_fx::main() -- the effects module exists as a rawfile inside
//    the map zone, but whether Scr_ReadFile can reach a CoDOL zone rawfile is
//    unproven, and a file::function call is a compile-time dependency, so being
//    wrong about it is a fatal load error rather than a missing effect.
//  * maps/mp/_compass::setupMiniMap -- _compass is a multiplayer module, and the
//    arena entry for every zm map carries mapimage "blank".

main()
{
	maps\mp\gametypes\_zombiemode::levelMain( "zm_prototype_sh" );
}
