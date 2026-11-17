// PROVENANCE (2026-10-22): mp_strike_sh is CoDOL 3.5.6.2's (build 3.5.2.6) Strike, ported to
// 1.1.1.13 by CODOL-AssetExplorer (port-map --game "3.5.6.2\DLC255", ff_dlc22_mp_strike_sh_V4).
// The zone ships maps/mp/mp_strike_sh_fx.gsc and maps/createfx/mp_strike_sh_fx.gsc (extracted
// verbatim here) but no level script, so this follows IW4 maps/mp/mp_strike.gsc:
//   * precache module: maps/mp/mp_strike_sh_precache.gsc, the precache_script keys its MapEnts
//     name (ceiling fans, fluorescent lights, generators, wall fan, hanging clothes).
//   * art: maps/createart/mp_strike_sh_art.gsc, IW4 mp_strike fog and the zone's
//     vision/mp_strike_sh.vision.
//   * compass: compass_map_mp_strike_sh ships in the zone; ambient_mp_strike is in common_mp.
//   * IW4's BreakGlass() is left out: 1.1.1.13 has no glass_map (the port drops it).
main()
{
	maps\mp\mp_strike_sh_precache::main();
	maps\createart\mp_strike_sh_art::main();
	maps\mp\mp_strike_sh_fx::main();

	maps\mp\_load::main();

	// turn the light grid tweaks off explicitly: another map's level script may have left them on
	setdvar( "r_lightGridEnableTweaks", 0 );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_strike_sh" );

	ambientPlay( "ambient_mp_strike" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	setdvar( "compassmaxrange", "1900" );
}
