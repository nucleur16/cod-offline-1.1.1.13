// PROVENANCE (2026-10-14): CoDOL mp_ambush_sh is a 3.3.3.3 map (CoD4 Ambush, mp_convoy)
// ported to 1.1.1.13 by CODOL-AssetExplorer (port-map). The zone ships
// maps/mp/mp_ambush_sh_fx.gsc and maps/createfx/mp_ambush_sh_fx.gsc (extracted verbatim
// here) but not its level script, so this is IW3 maps/mp/mp_convoy.gsc (COD-GSC-Source
// IW3-GSC) in the IW4 MP level-script shape:
//   * precache module: maps/mp/mp_ambush_sh_precache.gsc (ceiling fan destructible anims,
//     animated birch trees and hanging clothes).
//   * art: maps/createart/mp_ambush_sh_art.gsc carries mp_convoy's fog.
//   * compass: the zone ships compass_map_mp_convoy (and _cha), not compass_map_mp_ambush_sh.
//   * ambient: ambient_mp_ambush (a C1 alias; IW3 used ambient_convoy).
//   * IW3's game[ "allies" ] / [ "axis" ] / soldiertype keys are IW3 team setup and are left
//     to the gametype; attackers/defenders and the dvars are kept.
//   * the two misc_turret entities (weaponinfo turret_minigun_mp, targetname
//     charge_remove) are spawned by the engine from the map entities, not by this script.
main()
{
	maps\mp\mp_ambush_sh_precache::main();
	maps\createart\mp_ambush_sh_art::main();
	maps\mp\mp_ambush_sh_fx::main();

	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_convoy" );

	ambientPlay( "ambient_mp_ambush" );

	game[ "attackers" ] = "axis";
	game[ "defenders" ] = "allies";

	setdvar( "r_specularcolorscale", "1" );

	setdvar( "compassmaxrange", "2000" );
}
