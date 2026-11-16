// PROVENANCE (2026-11-13): mp_lookout_sh is CODOL 3.19.26.22's port, converted to 1.1.1.13 by CODOL-AssetExplorer port-map.
// Level script: IW5 mp_restrepo_ss (compiled ScriptFile, patch_mp_restrepo_ss.ff; rebuilt from its string/const table), translated to the 1.1.1.13 script set.
// The zone's own maps/mp/mp_lookout_sh_fx.gsc and maps/createfx/mp_lookout_sh_fx.gsc (3.19.26.22) are used as they ship.
// Precache: maps/mp/mp_lookout_sh_precache.gsc names the modules the zone's MapEnts give in precache_script keys.
// Adaptations:
//   * no mp_restrepo_ss_fog/art source: no createart
//   * ambientPlay( "ambient_mp_outpost" ) left out: the zone has no such alias
//   * audio_settings() (maps\mp\_audio::add_reverb "mountains") left out: this tree has no maps\mp\_audio
//   * compass: compass_map_mp_lookout_sh (the zone's)
//   * tdmSpawnAdjustments() (extra TDM spawns through IW5's maps\mp\gametypes\faceoff::createScriptSpawnPoint) is not ported: this tree has no faceoff gametype; the map's own mp_tdm_spawn points are used
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\mp\mp_lookout_sh_precache::main();
	maps\mp\mp_lookout_sh_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_lookout_sh" );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.33 );
	setdvar( "sm_sunShadowScale", "0.8" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
