// PROVENANCE (2026-09-01): CODOL-original level script.  No IW4 counterpart
// (vendor/iw4 ships no mp_nuked), and no CODOL zone ships a
// maps/mp/mp_nuked.gsc rawfile -- only the fx pair, carried verbatim in this
// overlay.  NOTE the zone's fx script is structured like a BO-era one: it
// defines main() itself (calling precache_util_fx / precache_createfx_fx /
// precache_scripted_fx / precache_fx_prop_anims) and #includes
// maps\mp\_utility, so calling its main() here is exactly what its author
// wired.  Minimal IW4 level-script template otherwise (shape of
// vendor/iw4/maps/mp/mp_rust.gsc).  Deliberately absent because not derivable
// from the zone: ambientPlay, killTrigger/minefields (none in the MapEnts
// harvest), expfog.
main()
{
	maps\mp\mp_nuked_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_nuked" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
