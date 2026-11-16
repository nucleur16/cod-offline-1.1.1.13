// PROVENANCE (2026-11-13): mp_awp_arena is CODOL 3.19.26.22's AWP Arena, ported to 1.1.1.13 by CODOL-AssetExplorer
// port-map (ae_port31926_wt, CODOL-31926.md). The zone ships maps/mp/mp_awp_arena_fx.gsc and
// maps/createfx/mp_awp_arena_fx.gsc but no level script, so this is a minimal one in the style of the other ports:
//   * fx: the zone's own maps/mp/mp_awp_arena_fx.gsc and maps/createfx/mp_awp_arena_fx.gsc (3.19.26.22), as they ship.
//   * NO ORIGINAL LEVEL SCRIPT: none is in 3.19.26.22 or in any IW3/IW4/IW5/T5 source on this PC (a CoDOL-only map),
//     so this is a faithful minimal one: _load, compass, the map's own fx and precache.
//   * compass: compass_map_mp_awp_arena ships in the zone.
main()
{
	maps\mp\mp_awp_arena_fx::main();
	maps\mp\_load::main();

	// turn the light grid tweaks off explicitly: another map's level script may have left them on
	setdvar( "r_lightGridEnableTweaks", 0 );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_awp_arena" );
	setdvar( "compassmaxrange", "3000" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
