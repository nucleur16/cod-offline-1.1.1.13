// PROVENANCE (2026-11-13): mp_district is CODOL 3.19.26.22's port, converted to 1.1.1.13 by CODOL-AssetExplorer port-map.
// Level script: IW3 mp_citystreets ("District"; COD-GSC-Source/IW3-GSC/maps/mp/mp_citystreets.gsc), translated to the 1.1.1.13 script set.
// The zone's own maps/mp/mp_district_fx.gsc and maps/createfx/mp_district_fx.gsc (3.19.26.22) are used as they ship.
// Precache: maps/mp/mp_district_precache.gsc names the modules the zone's MapEnts give in precache_script keys.
// Adaptations:
//   * ambientPlay( "ambient_citystreets_day" ) left out: the zone has no such alias
//   * game["allies"]/["axis"] = marines/opfor and the *_soldiertype lines are left out (IW3 team setup; 1.1.1.13 takes its teams from the arena/teams scripts)
//   * no createart: IW3's maps/createart/mp_citystreets_art.gsc is not in the dump
//   * compass: compass_map_mp_citystreets (the zone's)
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\mp\mp_district_precache::main();
	maps\mp\mp_district_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_citystreets" );
	VisionSetNaked( "mp_district" );
	setdvar( "r_specularcolorscale", "1" );
	setdvar( "compassmaxrange", "2000" );

	game[ "attackers" ] = "axis";
	game[ "defenders" ] = "allies";

	maps\mp\_explosive_barrels::main();
}
