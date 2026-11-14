main()
{
	/#
		if ( common_scripts\utility::shouldRunServerSideEffects() )
		{
			precache_createfx_fx();
			maps\createfx\mp_boomtown_fx::main();
		}
	#/
}


precache_createfx_fx()
{	
	// --------------------------------------------
	// Effects
	// --------------------------------------------
	//Ambient
	level._effect[ "bt_dust_playspace" ]						= loadfx( "maps/mp_boomtown/bt_dust_playspace" );
	level._effect[ "bt_dust_doorway" ]						= loadfx( "maps/mp_boomtown/bt_dust_doorway" );
	level._effect[ "bt_light_flourescent" ]					= loadfx( "maps/mp_boomtown/bt_light_flourescent" );
	level._effect[ "bt_light_hanging_lamp" ]					= loadfx( "maps/mp_boomtown/bt_light_hanging_lamp" );
	level._effect[ "cargo_vl_white_smpl" ]					= loadfx( "misc/cargo_vl_white_smpl" );
	level._effect[ "insect_trail_runner" ]					= loadfx( "misc/insect_trail_runner" );
	level._effect[ "drips_slow" ]							= loadfx( "misc/drips_slow" );
	level._effect[ "aircon" ]								= loadfx( "distortion/heater" );
	
	//Log Ride FX
	level._effect[ "bt_water_log_splash" ]					= loadfx( "maps/mp_boomtown/bt_water_log_splash" );
	level._effect[ "bt_water_logride_canal_01" ]				= loadfx( "maps/mp_boomtown/bt_water_logride_canal_01" );
	level._effect[ "bt_water_logride_canal_02" ]				= loadfx( "maps/mp_boomtown/bt_water_logride_canal_02" );
	level._effect[ "bt_water_logride_base" ]					= loadfx( "maps/mp_boomtown/bt_water_logride_base" );
	level._effect[ "bt_water_minisplash" ]					= loadfx( "maps/mp_boomtown/bt_water_logride_minisplash" );
	
	//Cowboy
	level._effect[ "bt_cowboy_gun_exp" ]					= loadfx( "maps/mp_boomtown/bt_cowboy_gun_exp" );

	
}
