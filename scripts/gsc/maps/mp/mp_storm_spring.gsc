// PROVENANCE (2026-09-01): adapted from vendor/iw4/maps/mp/mp_storm.gsc.
// mp_storm_spring is CODOL's re-lit "spring" variant of MW2 Storm (the zone
// ships sun/mp_storm_spring.sun and a second createfx payload,
// mp_storm_spring_ex_fx.gsc, on top of the storm layout; its fx script keeps
// storm's tanker_explosion bombexplosion hook).  Changes from the vendor file:
//   * fx module: mp_storm_fx -> mp_storm_spring_fx (zone-shipped, in this
//     overlay; extracted verbatim from mp_storm_spring.ff).
//   * compass material renamed to the CODOL map (INFERRED naming).
//   * precache/art stay the vendor mp_storm modules (same base map).
//   * ambientPlay alias kept as vendor's ambient_mp_storm (host-recorded).
main()
{
	maps\mp\mp_storm_precache::main();
	maps\mp\mp_storm_spring_fx::main();
	maps\createart\mp_storm_art::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_storm_spring" );

	ambientPlay( "ambient_mp_storm" );

	game[ "attackers" ] = "axis";
	game[ "defenders" ] = "allies";

	setdvar( "r_specularcolorscale", "1.5" );
	setdvar( "compassmaxrange", "2300" );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.3 );
	//setdvar( "r_lightGridContrast", .5 );
}
