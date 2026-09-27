// PROVENANCE (2026-09-01): adapted from vendor/iw4/maps/mp/mp_crash.gsc.
// mp_crash_tropical is CODOL's tropical reskin of MW2 Crash (base identity
// confirmed from the zone itself: mp_crash_tropical.ff ships
// sun/mp_crash_tropical.sun and its own maps/mp/mp_crash_tropical_fx.gsc whose
// loadfx lines are the mp_crash set re-pointed at *_tropical fx variants).
// Changes from the vendor file, and nothing else:
//   * fx module: mp_crash_fx -> mp_crash_tropical_fx (the zone-shipped one in
//     this overlay; extracted verbatim from mp_crash_tropical.ff).
//   * compass material: kept as vendor's compass_map_mp_crash_dlc. No
//     compass_map_mp_crash_tropical ships (docs/zone_assets); patch_common_mp
//     adds the compass_map_mp_crash_dlc image for this map, and a name C1
//     cannot resolve leaves the minimap blank.
//   * precache/art stay the vendor mp_crash modules -- same base geometry and
//     props; precache calls are host-recorded on this server, not fatal.
//   * ambientPlay alias kept as vendor's ambient_mp_crash (host-recorded).
main()
{
	maps\mp\mp_crash_precache::main();
	maps\mp\mp_crash_tropical_fx::main();
	maps\createart\mp_crash_art::main();
	maps\mp\_load::main();

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
