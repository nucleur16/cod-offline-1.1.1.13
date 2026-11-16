// PROVENANCE (2026-10-14): maps/mp/mp_rust_long_fx.gsc extracted verbatim from the mp_rust_long zone
// (3.3.3.3 map ported to 1.1.1.13), with one change: C1 stores dvar names as hashes, so the
// zone's getdvarint( "1106" ) is IW4's getdvarint( "clientSideEffects" ) (IW4
// maps/mp/mp_rust_long_fx.gsc). This server registers clientSideEffects (true) under its
// IW4 name and has no dvar "1106", so the hashed name read 0 here and ran the createfx
// ents server-side on top of the client's own copy.
main()
{
	//ambient fx
	level._effect[ "sand_storm_light" ]						= loadfx( "weather/sand_storm_mp_rust" );
	level._effect[ "sand_spray_detail_runner0x400" ]	 	= loadfx( "dust/sand_spray_detail_runner_0x400" );
	level._effect[ "sand_spray_detail_runner400x400" ]	 	= loadfx( "dust/sand_spray_detail_runner_400x400" );
	level._effect[ "sand_spray_detail_oriented_runner" ]	= loadfx( "dust/sand_spray_detail_oriented_runner" );
	level._effect[ "sand_spray_cliff_oriented_runner" ] 	= loadfx( "dust/sand_spray_cliff_oriented_runner" );
	//level._effect[ "fx_rcbomb_light_red_sh" ] 	            = loadfx( "vehicle/light/fx_rcbomb_light_red_sh" );
	level._effect[ "field_fire_distant_01" ] 	            = loadfx( "fire/field_fire_distant_01" );
	level._effect[ "oil_rig_fire_mp" ] 	            = loadfx( "fire/oil_rig_fire_mp" );
	level._effect[ "AC130CloudMask" ]					= loadfx( "misc/clouds_cumulus_mask" );
	
	if ( !getdvarint( "clientSideEffects" ) )
		maps\createfx\mp_rust_long_fx::main();

}