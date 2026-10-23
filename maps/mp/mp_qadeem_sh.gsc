// PROVENANCE (2026-10-22): mp_qadeem_sh is CoDOL 3.5.6.2's (build 3.5.2.6) Qadeem, a CoDOL-only map,
// ported to 1.1.1.13 by CODOL-AssetExplorer (port-map --game "3.5.6.2\DLC255",
// ff_dlc22_mp_qadeem_sh_V4). No IW/T level script exists for it, and the zone ships
// maps/mp/mp_qadeem_sh_fx.gsc and maps/createfx/mp_qadeem_sh_fx.gsc (extracted verbatim here)
// but not its level script, so this is the minimal IW4 level-script template (as mp_mideast):
//   * precache module: maps/mp/mp_qadeem_sh_precache.gsc (the ceiling fan its MapEnts name).
//   * no createart: the zone ships vision/mp_qadeem_sh.vision, which _load already sets
//     (VisionSetNaked( mapname )); no fog values exist for this map.
//   * compass: compass_map_mp_qadeem_sh ships in the zone.
//   * ambient: ambient_mp_qadeem_sh_ext ships in the zone (its _int twin is for the
//     audio_zone triggers, which this server does not run).
//   * IW5-only entities are inert here: delete_on_load / no_drop triggers, the pool's
//     trigger_underwater (vision/hillside_pool.vision) and the chopper-boss structs.
main()
{
	maps\mp\mp_qadeem_sh_precache::main();
	maps\mp\mp_qadeem_sh_fx::main();

	maps\mp\_load::main();

	// turn the light grid tweaks off explicitly: another map's level script may have left them on
	setdvar( "r_lightGridEnableTweaks", 0 );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_qadeem_sh" );

	ambientPlay( "ambient_mp_qadeem_sh_ext" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
