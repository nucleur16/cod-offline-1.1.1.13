// PROVENANCE (2026-09-01): adapted from vendor/iw4/maps/mp/mp_estate.gsc.
// mp_estate_tropical is CODOL's tropical reskin of MW2 Estate (base identity
// confirmed from the zone's own fx script, which loads the estate waterfall
// set: water/waterfall_drainage_splash_estate, waterfall_splash_large_estate,
// waterfall_splash_large_drops_estate).  Changes from the vendor file:
//   * fx module: mp_estate_fx -> mp_estate_tropical_fx (zone-shipped, in this
//     overlay; extracted verbatim from mp_estate_tropical.ff).
//   * compass material renamed to the CODOL map (INFERRED naming).
//   * precache/art stay the vendor mp_estate modules (same base map).
//   * everything else verbatim, including the level.ps3 branch: level.ps3 is
//     set by _globallogic::init (line 8), which runs AFTER this script, so the
//     read is undefined -> falsy -> the PC path, same as stock ordering.
main()
{

	maps\mp\mp_estate_precache::main();
	maps\createart\mp_estate_art::main();
	maps\mp\mp_estate_tropical_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_estate_tropical" );

	ambientPlay( "ambient_mp_estate" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	setdvar( "r_specularcolorscale", "1.17" );
	setdvar( "compassmaxrange", "3500" );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.3 );
	setdvar( "r_lightGridContrast", 0 );

	if ( level.ps3 )
		setdvar( "sm_sunShadowScale", "0.5" ); // ps3 optimization
	else
		setdvar( "sm_sunShadowScale", "0.7" ); // optimization
}
