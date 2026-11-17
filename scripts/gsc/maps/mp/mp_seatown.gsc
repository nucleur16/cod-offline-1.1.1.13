// PROVENANCE (2026-10-14): CoDOL mp_seatown is a 3.3.3.3 map (MW3 Seatown) ported to 1.1.1.13
// by CODOL-AssetExplorer (port-map). COD-GSC-Source has no IW5 mp_seatown level script (only
// unnamed fx/createfx dumps), and the zone ships maps/mp/mp_seatown_fx.gsc and
// maps/createfx/mp_seatown_fx.gsc (extracted verbatim here) but not its level script, so
// this is the minimal IW4 level-script template (shape of IW4 maps/mp/mp_rust.gsc, as
// maps/mp/mp_bloc_sh.gsc):
//   * precache module: maps/mp/mp_seatown_precache.gsc (ceiling fan, chicken and
//     air-conditioner destructible anims; hanging clothes and wire groups).
//   * art: maps/createart/mp_seatown_art.gsc carries IW5's seatown fog
//     (maps/createart/mp_seatown_fog.gsc, from a MW3 raw dump) as setExpFog, and the zone's
//     vision/mp_seatown.vision.
//   * compass: compass_map_mp_seatown ships in the zone.
//   * no ambientPlay: no ambient alias for this map is known.
main()
{
	maps\mp\mp_seatown_precache::main();
	maps\createart\mp_seatown_art::main();
	maps\mp\mp_seatown_fx::main();

	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_seatown" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
