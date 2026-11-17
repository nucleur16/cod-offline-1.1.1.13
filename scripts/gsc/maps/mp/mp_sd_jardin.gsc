// PROVENANCE (2026-10-22): mp_sd_jardin is CoDOL 3.5.6.2's (build 3.5.2.6) "Pillars" garden map
// (MPUI_SD_PILLARS), a Search and Destroy map: 3.5.6.2's comaplist offers it for sd and sdp only,
// and its MapEnts carry sd spawns and bomb sites (one placeholder dm/tdm/dom spawn each).
// Ported to 1.1.1.13 by CODOL-AssetExplorer (port-map --game 3.5.6.2). The zone ships
// maps/mp/mp_sd_jardin_fx.gsc and maps/createfx/mp_sd_jardin_fx.gsc (extracted verbatim here)
// but not its level script, so this is the IW4 level-script template:
//   * precache: the locker destructible anims (3 toy_locker_double).
//   * art: maps/createart/mp_sd_jardin_art.gsc, the zone's vision/mp_sd_jardin.vision. No fog
//     values are known, so none is set.
//   * no ambientPlay: 3.5.6.2 defines no ambient alias for this map.
//   * compass: compass_map_mp_sd_jardin ships in the zone.
main()
{
	common_scripts\_destructible_types_anim_lockers::main();
	maps\createart\mp_sd_jardin_art::main();
	maps\mp\mp_sd_jardin_fx::main();

	maps\mp\_load::main();

	// turn the light grid tweaks off explicitly: another map's level script may have left them on
	setdvar( "r_lightGridEnableTweaks", 0 );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_sd_jardin" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
