// PROVENANCE (2026-09-01): CODOL-original level script.  No IW4 counterpart
// (Crossfire is a CoD4 map, and CODOL spells it mp_cross_fire; vendor/iw4
// ships no mp_crossfire either), and no CODOL zone ships a
// maps/mp/mp_cross_fire.gsc rawfile -- only the fx pair, carried verbatim in
// this overlay.  Minimal IW4 level-script template (shape of
// vendor/iw4/maps/mp/mp_rust.gsc).  Deliberately absent because not derivable
// from the zone: ambientPlay, killTrigger/minefields (none in the MapEnts
// harvest), expfog.
main()
{
	maps\mp\mp_cross_fire_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_cross_fire" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
