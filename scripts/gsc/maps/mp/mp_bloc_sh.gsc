// PROVENANCE (2026-09-01): CODOL-original level script.  No IW4 counterpart
// (Bloc is a CoD4 map; vendor/iw4 ships no mp_bloc*), and no CODOL zone ships
// a maps/mp/mp_bloc_sh.gsc rawfile -- only the fx pair, carried verbatim in
// this overlay.  Minimal IW4 level-script template (shape of
// vendor/iw4/maps/mp/mp_rust.gsc).  Deliberately absent because not derivable
// from the zone: ambientPlay, killTrigger/minefields (none in the MapEnts
// harvest), expfog.
main()
{
	maps\mp\mp_bloc_sh_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_bloc_sh" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
