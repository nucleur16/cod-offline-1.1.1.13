// PROVENANCE (2026-10-22): mp_hunger is CODOL 3.5.6.2's (build 3.5.2.6) DLC 23 map, a DMZ map
// there (mp/loadingscreensetting.csv: "mp_hunger,dmz"), ported to 1.1.1.13 by CODOL-AssetExplorer
// port-map and played here in the regular mp modes. The zone ships maps/mp/mp_hunger_fx.gsc,
// maps/createfx/mp_hunger_fx.gsc and vision/mp_hunger.vision but no level script, so this follows
// the other usermaps:
//   * fx: the zone's mp_hunger_fx (one moth effect); createart: maps/createart/mp_hunger_art.gsc.
//   * precache: the hanging-clothes animated models its MapEnts name.
//   * compass: compass_map_mp_hunger ships in the zone and its _load zone.
//   * ambient alias: ambient_mp_rural (common_mp); the zone ships no ambient alias of its own.
//   * mode spawns and objectives: maps/mp/mp_hunger.d3dbsp.ents (the zone parks them off the level).
main()
{
	maps\mp\mp_hunger_precache::main();
	maps\createart\mp_hunger_art::main();
	maps\mp\mp_hunger_fx::main();

	maps\mp\_load::main();

	// turn the light grid tweaks off explicitly: another map's level script may have left them on
	setdvar( "r_lightGridEnableTweaks", 0 );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_hunger" );

	ambientPlay( "ambient_mp_rural" );

	setdvar( "compassmaxrange", "4000" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
