// PROVENANCE (2026-10-22): mp_abandon_sh is CODOL 2.6.9.7's amusement-park Abandon, ported to
// 1.1.1.13 by CODOL-AssetExplorer (port-map --game CODOL_2.6.9.7). It is not mp_abandon_sh_mk
// (the 3.3.3.3 remake, a separate usermap). The zone ships maps/mp/mp_abandon_sh_fx.gsc and
// maps/createfx/mp_abandon_sh_fx.gsc (extracted verbatim here) but not its level script, so this
// follows mp_abandon_sh_mk.gsc / IW4 mp_abandon.gsc:
//   * fx/art modules: the mp_abandon_sh ones.
//   * precache module: maps/mp/mp_abandon_sh_precache.gsc registers the animated props the
//     entities name in their precache_script keys (ferris wheel, dragon windsocks, banners, birch).
//     The destructible toys (toy_trashcan_pig, toy_arcade_machine_1/2, toy_pinball_machine_chi,
//     toy_electricbox4) are in common_scripts/_destructible_types.gsc.
//   * carnival games: maps/mp/mp_abandon_sh_carnival.gsc (animal race, pitching game, lights).
//   * compass: compass_map_mp_abandon_sh ships in the zone.
//   * ambient alias: ambient_mp_abandon (common_mp), as mp_abandon_sh_mk.
main()
{
	maps\mp\mp_abandon_sh_precache::main();
	maps\createart\mp_abandon_sh_art::main();
	maps\mp\mp_abandon_sh_fx::main();
	maps\mp\mp_abandon_sh_carnival::precache();

	maps\mp\_load::main();

	// turn the light grid tweaks off explicitly: another map's level script (e.g. mp_terminal_cls) may have left them on
	setdvar( "r_lightGridEnableTweaks", 0 );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_abandon_sh" );

	ambientPlay( "ambient_mp_abandon" );

	setdvar( "compassmaxrange", "1800" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	// carnival games (animal race, pitching game) and their lights; no fireworks
	maps\mp\mp_abandon_sh_carnival::main();
}
