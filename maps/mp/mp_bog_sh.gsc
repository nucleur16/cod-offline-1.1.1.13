// PROVENANCE (2026-09-01): CODOL-original level script.  No IW4 counterpart
// (Bog is a CoD4 map; vendor/iw4 ships no mp_bog*), and no CODOL zone ships a
// maps/mp/mp_bog_sh.gsc rawfile -- only the fx pair, carried verbatim in this
// overlay (its fx main loads the bog night set: antiair_runner_night,
// fog_bog_a, mp_bog_horizon_flash).  Minimal IW4 level-script template (shape
// of vendor/iw4/maps/mp/mp_rust.gsc).  Deliberately absent because not
// derivable from the zone: ambientPlay, killTrigger/minefields (none in the
// MapEnts harvest), expfog.
main()
{
	maps\mp\mp_bog_sh_fx::main();
	// toy_airconditioner destructibles spin their fan with ex_airconditioner_fan
	// (shipped in this map's zone). IW4 registers destructible anims from the
	// map's precache script before _load runs _destructible::init().
	common_scripts\_destructible_types_anim_airconditioner::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_bog_sh" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
