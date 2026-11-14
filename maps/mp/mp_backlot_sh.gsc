// PROVENANCE (2026-11-13): mp_backlot_sh is CODOL 3.19.26.22's port, converted to 1.1.1.13 by CODOL-AssetExplorer port-map.
// Level script: IW3 mp_backlot (COD-GSC-Source/IW3-GSC/maps/mp/mp_backlot.gsc), translated to the 1.1.1.13 script set.
// The zone's own maps/mp/mp_backlot_sh_fx.gsc and maps/createfx/mp_backlot_sh_fx.gsc (3.19.26.22) are used as they ship.
// Precache: maps/mp/mp_backlot_sh_precache.gsc names the modules the zone's MapEnts give in precache_script keys.
// Adaptations:
//   * ambientPlay( "ambient_backlot_ext" ) left out: the zone has no such alias
//   * game["allies"]/["axis"] = marines/opfor and the *_soldiertype lines are left out (IW3 team setup; 1.1.1.13 takes its teams from the arena/teams scripts)
//   * no createart: IW3's maps/createart/mp_backlot_art.gsc is not in the dump
//   * compass: compass_map_mp_backlot_sh (the zone's)
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\mp\mp_backlot_sh_precache::main();
	maps\mp\mp_backlot_sh_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_backlot_sh" );
	setdvar( "r_specularcolorscale", "1" );
	setdvar( "r_glowbloomintensity0", ".25" );
	setdvar( "r_glowbloomintensity1", ".25" );
	setdvar( "r_glowskybleedintensity0", ".3" );
	setdvar( "compassmaxrange", "1800" );

	game[ "attackers" ] = "axis";
	game[ "defenders" ] = "allies";
}
