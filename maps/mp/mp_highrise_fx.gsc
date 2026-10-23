main()
{
	level._effect[ "falling_dust_light" ] = loadfx( "dust/falling_dust_light" );
	level._effect[ "light_glow_walllight_white" ] = loadfx( "misc/light_glow_walllight_white" );
	level._effect[ "mp_highrise_godray_white" ] = loadfx( "env/light/mp_highrise_godray_white" );
	level._effect[ "ab_dust_wind_fast_paper" ] = loadfx( "dust/ab_dust_wind_fast_paper" );
	level._effect[ "ab_birds_runner" ] = loadfx( "misc/ab_birds_runner" );
	level._effect[ "fx_debris_dust_motes" ] = loadfx( "maps/ber2/fx_debris_dust_motes" );
	level._effect[ "transformer_spark_loop" ] = loadfx( "explosions/transformer_spark_loop" );
	level._effect[ "drips_slow" ] = loadfx( "misc/drips_slow" );
	level._effect[ "fx_light_god_ray_sm_single" ] = loadfx( "env/light/fx_light_god_ray_sm_single" );
	level._effect[ "mist_clouds" ] = loadfx( "weather/mist_clouds" );

	level._effect[ "ground_fog_mp_highrise_far" ]		= loadfx( "smoke/ground_fog_mp_highrise_far" );
	level._effect[ "fog_highrise_night" ]				= loadfx( "smoke/fog_highrise_night" );

	level._effect[ "hallway_smoke" ]					= loadfx( "smoke/hallway_smoke_light" );
	level._effect[ "dust_wind_fast_light" ] 			= loadfx( "dust/dust_wind_fast_light" );
	level._effect[ "room_smoke_200" ]					= loadfx( "smoke/room_smoke_200" );
	level._effect[ "room_smoke_400" ]					= loadfx( "smoke/room_smoke_400" );
	level._effect[ "steam_vent_long" ]					= loadfx( "smoke/steam_vent_long" );
	level._effect[ "fx_mp_fog_rolling_small" ]			= loadfx( "smoke/fx_mp_fog_rolling_small" );
	level._effect[ "fx_dust_blow_fanvent" ]			= loadfx( "env/dirt/fx_dust_blow_fanvent" );


 /#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_highrise_fx::main();
#/
}
