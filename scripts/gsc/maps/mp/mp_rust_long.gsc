// PROVENANCE (2026-10-14): CoDOL mp_rust_long is a 3.3.3.3 map ported to 1.1.1.13 by
// CODOL-AssetExplorer (port-map). The zone ships maps/mp/mp_rust_long_fx.gsc and
// maps/createfx/mp_rust_long_fx.gsc (extracted here) but not its level script, so this is
// IW4 maps/mp/mp_rust_long.gsc (COD-GSC-Source IW4-GSC) with one change: the level script
// no longer calls maps\createfx\mp_rust_long_fx::main() itself. The zone's fx file runs it
// only when clientSideEffects is off, and the client plays those ents itself.
// precache, art, compass (compass_map_mp_rust_long ships in the zone) and ambient alias are
// IW4's.
#include maps\mp\_utility;

main()
{
	maps\mp\mp_rust_long_precache::main();
	maps\createart\mp_rust_long_art::main();
	maps\mp\mp_rust_long_fx::main();

	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_rust_long" );

	setDvar( "compassmaxrange", "1400" );

	ambientPlay( "ambient_mp_duststorm" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	setDvar( "r_diffusecolorscale", "1.4" );
}
