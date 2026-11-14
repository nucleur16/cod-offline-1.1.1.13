// PROVENANCE (2026-11-13): mp_lockdown_sh is CODOL 3.19.26.22's port, converted to 1.1.1.13 by CODOL-AssetExplorer port-map.
// Level script: IW5 mp_alpha (compiled ScriptFile, mp_alpha.ff; rebuilt from its string/const table), translated to the 1.1.1.13 script set.
// The zone's own maps/mp/mp_lockdown_sh_fx.gsc and maps/createfx/mp_lockdown_sh_fx.gsc (3.19.26.22) are used as they ship.
// Adaptations:
//   * ambientPlay( "ambient_mp_alpha" ) left out: the zone has no such alias
//   * audio_settings() (maps\mp\_audio::add_reverb "city") left out: this tree has no maps\mp\_audio
//   * compass: compass_map_mp_lockdown_sh (the zone's)
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\createart\mp_lockdown_sh_art::main();
	maps\mp\mp_lockdown_sh_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_lockdown_sh" );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.33 );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
