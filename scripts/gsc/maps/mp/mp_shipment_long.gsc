// PROVENANCE (2026-10-14): CoDOL mp_shipment_long is a 3.3.3.3 map ported to 1.1.1.13 by
// CODOL-AssetExplorer (port-map). The zone ships maps/mp/mp_shipment_long_fx.gsc and
// maps/createfx/mp_shipment_long_fx.gsc (extracted here) but not its level script, so this
// is IW4 maps/mp/mp_shipment_long.gsc (COD-GSC-Source IW4-GSC) adapted:
//   * no direct maps\createfx\mp_shipment_long_fx::main() call: the zone's fx file runs it
//     only when clientSideEffects is off, and the client plays those ents itself.
//   * IW4's deleteChaModels / deleteBarrels are dropped. They delete per-gametype copies of
//     objectives and props by targetname ("cha", "charge_remove", second
//     mp_global_intermission, ...), none of which the CoDOL map places, and they index
//     arrays the decompiled source never fills, which would be a script error here.
//   * compass: compass_map_mp_shipment_long ships in the zone (IW4 switched to it for every
//     gametype but koth).
#include maps\mp\_utility;

main()
{
	maps\mp\mp_shipment_long_fx::main();
	maps\createart\mp_shipment_long_art::main();

	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_shipment_long" );

	ambientPlay( "ambient_mp_rain" );

	game[ "attackers" ] = "axis";
	game[ "defenders" ] = "allies";

	setDvar( "r_specularcolorscale", "1" );
	setDvar( "r_diffusecolorscale", "0.8" );

	setDvar( "compassmaxrange", "1400" );
}
