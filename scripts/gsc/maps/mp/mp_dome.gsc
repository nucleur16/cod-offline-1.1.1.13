// PROVENANCE (2026-10-22): mp_dome is CoDOL 3.5.6.2's (build 3.5.2.6) Dome (MW3 Dome), ported to
// 1.1.1.13 by CODOL-AssetExplorer (port-map --game 3.5.6.2). The zone ships
// maps/mp/mp_dome_fx.gsc and maps/createfx/mp_dome_fx.gsc (extracted verbatim here) but not its
// level script, so this follows IW5 scripts/maps/mp/mp_dome.gsc (COD-GSC-Source/IW5-GSC):
//   * precache module: maps/mp/mp_dome_precache.gsc registers the animated props the entities
//     name in their precache_script keys (fence tarps, desert brush, oil pump jacks, windmill,
//     large windsock) and the air-conditioner destructible anims.
//   * art: maps/createart/mp_dome_art.gsc carries IW5's mp_dome fog and the zone's vision.
//   * ambient: ambient_mp_dome_ext (3.5.6.2 common_mp, carried into the ported zone). IW5 plays
//     "ambient_mp_dome"; 3.5.6.2 names its aliases _ext/_int, and the outdoor one is the base.
//   * IW5's audio_settings() (maps\mp\_audio::add_reverb "quarry") is left out: this tree has
//     no maps\mp\_audio. The map's audio_zone triggers are left unused for the same reason.
main()
{
	maps\mp\mp_dome_precache::main();
	maps\createart\mp_dome_art::main();
	maps\mp\mp_dome_fx::main();
	maps\mp\_explosive_barrels::main();

	maps\mp\_load::main();

	ambientPlay( "ambient_mp_dome_ext" );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_dome" );

	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.33 );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	// IW5: a kill volume under the dome's broken floor
	thread maps\mp\_utility::killTrigger( ( 0, 263, -223 ), 24, 10 );
}
