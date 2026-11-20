// PROVENANCE (2026-09-01): adapted from vendor/iw4/maps/mp/mp_favela.gsc.
// mp_fav_tropical is CODOL's tropical reskin of MW2 Favela ("fav" is CODOL's
// abbreviation; the zone's own fx script loads the favela ambient set --
// insects_carcass_runner, trash_spiral_runner, firelp_* -- and the coop zones
// name it coop_take_mp_fav_tropical alongside the other MW2 reskins).
// Changes from the vendor file:
//   * fx module: mp_favela_fx -> mp_fav_tropical_fx (zone-shipped, in this
//     overlay; extracted verbatim from mp_fav_tropical.ff).
//   * compass material renamed to the CODOL map (INFERRED naming).
//   * precache uses shipped tropical foliage; art uses recovered IW4x tropical source.
//   * everything else verbatim, including switch(level.gameType): this script
//     runs BEFORE the gametype main sets level.gametype, so the switch takes
//     its default branch (attackers axis) -- same as the vendor ordering.
main()
{
	maps\mp\mp_fav_tropical_precache::main();

	// Original IW4x tropical-map art, recovered from iw4x_00.iwd.
	maps\createart\mp_fav_tropical_art::main();
	maps\mp\mp_fav_tropical_fx::main();

	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_fav_tropical" );
	//setExpFog( 270, 11488, 0.8, 0.8, 0.8, 0.1, 0 );

	// raise up planes to avoid them flying through buildings
	level.airstrikeHeightScale = 1.5;

	ambientPlay( "ambient_mp_favela" );

	switch ( level.gameType )
	{
		case "oneflag":
			game[ "attackers" ] = "allies";
			game[ "defenders" ] = "axis";
			break;
		default:
			game[ "attackers" ] = "axis";
			game[ "defenders" ] = "allies";
			break;
	}

	setdvar( "r_specularcolorscale", "2.8" );
	setdvar( "compassmaxrange", "1500" );
}
