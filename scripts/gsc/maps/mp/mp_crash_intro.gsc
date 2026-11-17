// PROVENANCE (2026-09-01): adapted from vendor/iw4/maps/mp/mp_crash.gsc.
// mp_crash_intro is the coop-prolog variant of CODOL's Crash (its zone ships
// sun/mp_crash_tropical.sun -- the SAME sun file as mp_crash_tropical -- and
// its createfx payload is byte-identical in size to crash_tropical's, so the
// base is the same Crash geometry).  Changes from the vendor file:
//   * fx module: mp_crash_fx -> mp_crash_intro_fx (zone-shipped, in this
//     overlay; extracted verbatim from mp_crash_intro.ff).
//   * compass material renamed to the CODOL map (INFERRED naming; the vendor
//     value was compass_map_mp_crash_dlc).
//   * precache/art stay the vendor mp_crash modules (same base map).
main()
{
	maps\mp\mp_crash_precache::main();
	maps\mp\mp_crash_intro_fx::main();
	maps\createart\mp_crash_art::main();
	maps\mp\_load::main();

	// compass_map_mp_crash_intro ships in no zone; crash_intro has crash's geometry and
	// corners, and compass_map_mp_crash_dlc is in patch_common_mp (always mounted).
	maps\mp\_compass::setupMiniMap( "compass_map_mp_crash_dlc" );

	AmbientPlay( "ambient_mp_crash" );

	game["attackers"] = "axis";
	game["defenders"] = "allies";

	SetDvar( "r_specularcolorscale", "1" );
	SetDvar( "compassmaxrange", "1600" );

	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.10 );
	setdvar( "r_lightGridContrast", 1 );
}
