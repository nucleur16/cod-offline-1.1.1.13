// PROVENANCE (2026-11-13): mp_wasteland_sh is CODOL 3.19.26.22's port, converted to 1.1.1.13 by CODOL-AssetExplorer port-map.
// Level script: IW4 mp_brecourt ("Wasteland"; COD-GSC-Source/IW4-GSC/maps/mp/mp_brecourt.gsc; this tree has it as maps/mp/mp_brecourt.gsc), translated to the 1.1.1.13 script set.
// The zone's own maps/mp/mp_wasteland_sh_fx.gsc and maps/createfx/mp_wasteland_sh_fx.gsc (3.19.26.22) are used as they ship.
// Precache: maps/mp/mp_wasteland_sh_precache.gsc names the modules the zone's MapEnts give in precache_script keys.
// Adaptations:
//   * ambientPlay( "ambient_mp_rural" ) left out: the zone has no such alias
//   * compass: compass_map_mp_wasteland_sh (the zone's)
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\mp\mp_wasteland_sh_precache::main();
	maps\mp\mp_wasteland_sh_fx::main();
	maps\createart\mp_brecourt_art::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_wasteland_sh" );
	setdvar( "r_specularcolorscale", "1" );
	setdvar( "compassmaxrange", "4000" );
	setdvar( "sm_sunShadowScale", 0.5 );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.11 );
	setdvar( "r_lightGridContrast", .29 );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
