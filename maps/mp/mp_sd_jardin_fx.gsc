main()
{
	level._effect[ "mp_jardin_butterfly_radial" ] = loadfx( "maps/mp_jardin/mp_jardin_butterfly_radial" );
	level._effect[ "mp_jardin_waterfall_02" ] = loadfx( "maps/mp_jardin/mp_jardin_waterfall_02" );
	level._effect[ "hallway_smoke_light" ] = loadfx( "smoke/hallway_smoke_light" );
	level._effect[ "leaves_fall_gentlewind_codo" ] = loadfx( "misc/leaves_fall_gentlewind_codo" );
	level._effect[ "water_fountain_spout_impact" ] = loadfx( "maps/mp_jardin/water_fountain_spout_impact" );
	level._effect[ "water_fountain_01" ] = loadfx( "maps/mp_jardin/water_fountain_01" );
	level._effect[ "room_smoke_200" ] = loadfx( "smoke/room_smoke_200" );
	level._effect[ "mp_jardin_waterfall_01" ] = loadfx( "maps/mp_jardin/mp_jardin_waterfall_01" );
	level._effect[ "water_fountain_splash_impact_02" ] = loadfx( "maps/mp_jardin/water_fountain_splash_impact_02" );
	level._effect[ "water_fountain_splash_impact" ] = loadfx( "water/water_fountain_splash_impact" );
	level._effect[ "brazier" ] = LoadFX( "fire/firelp_med_pm" );

/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_sd_jardin_fx::main();
#/

}
