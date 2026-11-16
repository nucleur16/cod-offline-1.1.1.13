// PROVENANCE (2026-11-13): mp_village is CODOL 3.19.26.22's port, converted to 1.1.1.13 by CODOL-AssetExplorer port-map.
// Level script: IW5 mp_village (COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_village.gsc), translated to the 1.1.1.13 script set.
// The zone's own maps/mp/mp_village_fx.gsc and maps/createfx/mp_village_fx.gsc (3.19.26.22) are used as they ship.
// Precache: maps/mp/mp_village_precache.gsc names the modules the zone's MapEnts give in precache_script keys.
// Adaptations:
//   * audio_settings() (maps\mp\_audio::add_reverb "mountains") left out: this tree has no maps\mp\_audio
//   * compass: compass_map_mp_village (the zone's)
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\mp\mp_village_precache::main();
	maps\createart\mp_village_art::main();
	maps\mp\mp_village_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_village" );
	ambientPlay( "ambient_mp_village" );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.33 );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
