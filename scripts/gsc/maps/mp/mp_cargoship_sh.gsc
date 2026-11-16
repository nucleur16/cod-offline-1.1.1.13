// PROVENANCE (2026-09-01): CODOL-original level script.  No IW4 counterpart
// (Wet Work's cargoship is a CoD4 map; vendor/iw4 ships no mp_cargoship*),
// and no CODOL zone ships a maps/mp/mp_cargoship_sh.gsc rawfile -- only the
// fx pair, carried verbatim in this overlay (its fx main threads its own
// precache_createfx_fx(): snow + cargoship rain-noise runners).  Minimal IW4
// level-script template (shape of vendor/iw4/maps/mp/mp_rust.gsc).
// Deliberately absent because not derivable from the zone: ambientPlay,
// killTrigger/minefields (none in the MapEnts harvest; note the real CoD4
// map kills on water contact -- if falling off the ship becomes reachable,
// that needs a trigger_hurt harvested from the real MapEnts, not a guess),
// expfog.
main()
{
	maps\mp\mp_cargoship_sh_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_cargoship_sh" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
