main()
{
	level._effect[ "mist_clouds_slow" ] = loadfx( "weather/mist_clouds_slow" );
	level._effect[ "dust_spray_detail_oriented_runner" ] = loadfx( "dust/dust_spray_detail_oriented_runner" );
	level._effect[ "water_pipe_pour" ] = loadfx( "water/water_pipe_pour" );
	level._effect[ "bird_seagull_flock_large" ] = loadfx( "misc/bird_seagull_flock_large" );
	level._effect[ "fx_seagulls_shore_distant" ] = loadfx( "bio/animals/fx_seagulls_shore_distant" );
	level._effect[ "dust_ceiling_slow" ] = loadfx( "dust/dust_ceiling_slow" );
	level._effect[ "fx_debris_dust_motes" ]			= loadfx( "maps/ber2/fx_debris_dust_motes" );
	level._effect[ "room_smoke_200" ] 						= loadfx( "smoke/room_smoke_200" );
	level._effect[ "room_smoke_400" ] 						= loadfx( "smoke/room_smoke_400" );
	level._effect[ "drips_fast" ]	 						= loadfx( "misc/drips_fast" );
	level._effect[ "light_glow_white_bulb" ]			 	= loadfx( "misc/light_glow_white_bulb" );
	
/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_tunisia_fx::main();
#/

}
