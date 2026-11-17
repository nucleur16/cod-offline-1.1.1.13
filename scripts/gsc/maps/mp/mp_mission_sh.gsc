// PROVENANCE (2026-11-13): mp_mission_sh is CODOL 3.19.26.22's port, converted to 1.1.1.13 by CODOL-AssetExplorer port-map.
// Level script: IW5 mp_bravo (compiled ScriptFile, mp_bravo.ff; rebuilt from its string/const table), translated to the 1.1.1.13 script set.
// The zone's own maps/mp/mp_mission_sh_fx.gsc and maps/createfx/mp_mission_sh_fx.gsc (3.19.26.22) are used as they ship.
// Precache: maps/mp/mp_mission_sh_precache.gsc names the modules the zone's MapEnts give in precache_script keys.
// Adaptations:
//   * ambientPlay( "ambient_mp_bravo" ) left out: the zone has no such alias
//   * audio_settings() (maps\mp\_audio::add_reverb "mountains") left out: this tree has no maps\mp\_audio
//   * compass: compass_map_mp_mission_sh (the zone's)
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\mp\mp_mission_sh_precache::main();
	maps\createart\mp_mission_sh_art::main();
	maps\mp\mp_mission_sh_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_mission_sh" );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.33 );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
