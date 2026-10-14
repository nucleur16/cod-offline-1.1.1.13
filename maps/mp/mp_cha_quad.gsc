// PROVENANCE (2026-10-14): CoDOL mp_cha_quad is a 3.3.3.3, CoDOL-only map ported to 1.1.1.13
// by CODOL-AssetExplorer (port-map). No IW/T level script exists for it, and the zone ships
// maps/mp/mp_cha_quad_fx.gsc and maps/createfx/mp_cha_quad_fx.gsc (extracted verbatim here)
// but not its level script, so this is the minimal IW4 level-script template (shape of
// IW4 maps/mp/mp_rust.gsc, as maps/mp/mp_bloc_sh.gsc):
//   * precache module: maps/mp/mp_cha_quad_precache.gsc (the seven animated_models scripts
//     the map's entities name).
//   * no createart: the zone ships vision/mp_cha_quad.vision, which _load already sets
//     (VisionSetNaked( mapname )); no fog values exist for this map.
//   * compass: compass_map_mp_cha_quad ships in the zone.
//   * no ambientPlay: no ambient alias for this map is known.
main()
{
	maps\mp\mp_cha_quad_precache::main();
	maps\mp\mp_cha_quad_fx::main();

	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_cha_quad" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
