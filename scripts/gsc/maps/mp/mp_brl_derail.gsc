#include maps\mp\_utility;

// PROVENANCE (2026-10-22): mp_brl_derail is CODOL 3.5.6.2's (build 3.5.2.6) DLC 255 Derail,
// ported to 1.1.1.13 by CODOL-AssetExplorer port-map. Its MapEnts carry the regular mp spawns and
// objectives; its DMZ supply drops (hunger_package_ammo/_food structs) and the m1a1 tank
// (m1a1_turret_mp, which 1.1.1.13 does not define) are not used. The zone ships
// maps/mp/mp_brl_derail_fx.gsc and maps/createfx/mp_brl_derail_fx.gsc but no level script, so
// this follows IW4 mp_derail.gsc:
//   * fx: the zone's mp_brl_derail_fx; precache and createart: mp_derail's (vision mp_derail and
//     ambient_mp_snow ship in common_mp).
//   * compass: compass_map_mp_derail ships in the zone.
main()
{
	maps\mp\mp_derail_precache::main();
	maps\mp\mp_brl_derail_fx::main();
	maps\createart\mp_derail_art::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_derail" );

	ambientPlay( "ambient_mp_snow" );

	game[ "attackers" ] = "axis";
	game[ "defenders" ] = "allies";

	setdvar( "r_specularcolorscale", "2.3" );
	setdvar( "compassmaxrange", "4000" );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1 );
	setdvar( "r_lightGridContrast", .4 );

	thread killTrigger( (2077, -91, 0), 75, 20 );
}
