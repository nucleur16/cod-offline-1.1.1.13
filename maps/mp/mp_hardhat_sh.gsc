// PROVENANCE (2026-10-22): mp_hardhat_sh is CoDOL 3.5.6.2's (build 3.5.2.6) Hardhat (MW3 Hardhat),
// ported to 1.1.1.13 by CODOL-AssetExplorer (port-map --game 3.5.6.2). The zone ships
// maps/mp/mp_hardhat_sh_fx.gsc and maps/createfx/mp_hardhat_sh_fx.gsc (extracted verbatim here)
// but not its level script, and COD-GSC-Source has no IW5 mp_hardhat level script, so this is
// the IW5 level-script shape of maps/mp/mp_dome.gsc:
//   * precache module: maps/mp/mp_hardhat_sh_precache.gsc (fence tarps, plastic fences,
//     air-conditioner and generator destructible anims).
//   * art: maps/createart/mp_hardhat_sh_art.gsc, the zone's vision/mp_hardhat_sh.vision. No fog
//     values for this map are known, so none is set.
//   * explodable barrels: 20 explodable_barrel entities.
//   * ambient: ambient_mp_hardhat_sh (3.5.6.2 common_mp, carried into the ported zone).
//   * compass: compass_map_mp_hardhat_sh ships in the zone.
main()
{
	maps\mp\mp_hardhat_sh_precache::main();
	maps\createart\mp_hardhat_sh_art::main();
	maps\mp\mp_hardhat_sh_fx::main();
	maps\mp\_explosive_barrels::main();

	maps\mp\_load::main();

	// turn the light grid tweaks off explicitly: another map's level script may have left them on
	setdvar( "r_lightGridEnableTweaks", 0 );

	ambientPlay( "ambient_mp_hardhat_sh" );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_hardhat_sh" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
