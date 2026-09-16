// Native level adaptation (2026-09-01); shadow setting from IW4x (2026-09-15).
// No original IW4 counterpart
// exists (Killhouse is a CoD4 map; vendor/iw4 ships no mp_killhouse*), and no
// CODOL zone ships a maps/mp/mp_killhouse_sh.gsc rawfile -- only the fx pair,
// which this overlay carries verbatim.  This is the minimal IW4 level-script
// template (shape of vendor/iw4/maps/mp/mp_rust.gsc): the zone-shipped fx
// module, the standard _load chain, minimap setup, team sides.  Deliberately
// absent because not derivable from the zone: killTrigger/minefields (no such entities in the MapEnts harvest,
// resources/map_spawns.proposed.json), expfog (no authored values available).
main()
{
	maps\mp\mp_killhouse_sh_fx::main();
	maps\mp\_load::main();
	// IW4x authored Killhouse correction; C1 numeric renderer publication.
	codol_setmapsunsamplesize( 0.35 );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_killhouse_sh" );
	// Authored IW4x Killhouse ambience; exact alias supplied by C1 common_mp.
	ambientPlay( "ambient_scoutsniper_ext0" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
