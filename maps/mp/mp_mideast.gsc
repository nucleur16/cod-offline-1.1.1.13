// PROVENANCE (2026-10-14): CoDOL mp_mideast is a 3.3.3.3, CoDOL-only map ported to 1.1.1.13
// by CODOL-AssetExplorer (port-map). No IW/T level script exists for it, and the zone ships
// maps/mp/mp_mideast_fx.gsc and maps/createfx/mp_mideast_fx.gsc (extracted verbatim here)
// but not its level script, so this is the minimal IW4 level-script template (shape of
// IW4 maps/mp/mp_rust.gsc, as maps/mp/mp_bloc_sh.gsc):
//   * precache module: maps/mp/mp_mideast_precache.gsc (generator, chicken and
//     air-conditioner destructible anims; animated trees, clothes, wires and wall lamps).
//   * no createart: the zone ships vision/mp_mideast.vision, which _load already sets
//     (VisionSetNaked( mapname )); no fog values exist for this map.
//   * compass: compass_map_mp_mideast ships in the zone.
//   * no ambientPlay: no ambient alias for this map is known.
//   * objectives and mode spawns: the zone parks them outside the level; the server reads
//     maps/mp/mp_mideast.d3dbsp.ents in place of the zone's entity string (CoD4x_Server
//     tools/codol_objective_ents.py writes it; src/codol/collision/codol_map_ents_override.c).
main()
{
	maps\mp\mp_mideast_precache::main();
	maps\mp\mp_mideast_fx::main();

	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_mideast" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
