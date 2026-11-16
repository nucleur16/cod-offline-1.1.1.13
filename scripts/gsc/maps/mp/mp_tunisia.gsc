// PROVENANCE (2026-11-13): mp_tunisia is CODOL 3.19.26.22's Tunisia, ported to 1.1.1.13 by CODOL-AssetExplorer
// port-map (ae_port31926_wt, CODOL-31926.md). The zone ships maps/mp/mp_tunisia_fx.gsc and
// maps/createfx/mp_tunisia_fx.gsc but no level script, so this is a minimal one in the style of the other ports:
//   * fx: the zone's own maps/mp/mp_tunisia_fx.gsc and maps/createfx/mp_tunisia_fx.gsc (3.19.26.22), as they ship.
//   * NO ORIGINAL LEVEL SCRIPT: none is in 3.19.26.22 or in any IW3/IW4/IW5/T5 source on this PC (a CoDOL-only map),
//     so this is a faithful minimal one: _load, compass, the map's own fx and precache.
//   * precache: maps/mp/mp_tunisia_precache.gsc registers the modules its MapEnts name in precache_script keys.
//   * compass: compass_map_mp_tunisia ships in the zone.
//   * ambient: ambient_mp_tunisia ships in the zone.
main()
{
	maps\mp\mp_tunisia_precache::main();
	maps\mp\mp_tunisia_fx::main();
	maps\mp\_load::main();

	// turn the light grid tweaks off explicitly: another map's level script may have left them on
	setdvar( "r_lightGridEnableTweaks", 0 );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_tunisia" );
	ambientPlay( "ambient_mp_tunisia" );
	setdvar( "compassmaxrange", "3000" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
