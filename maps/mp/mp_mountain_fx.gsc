main()
{
	level._effect[ "light_glow_red_generic_pulse" ] = loadfx( "misc/light_glow_red_generic_pulse" );
	level._effect[ "bubbles_pipe_mp_mountain" ] = loadfx( "water/bubbles_pipe_mp_mountain" );
	level._effect[ "bubbles_tank_mp_mountain" ] = loadfx( "water/bubbles_tank_mp_mountain" );
	level._effect[ "water_surface_bubbling" ] = loadfx( "water/water_surface_bubbling" );
	level._effect[ "fx_water_drip_light_short" ] = loadfx( "env/water/fx_water_drip_light_short" );
	level._effect[ "fx_mp_snow_gust_ground_lg_thin" ] = loadfx( "maps/mp_maps/fx_mp_snow_gust_ground_lg_thin" );
	level._effect[ "fx_mp_snow_gust_rooftop" ] = loadfx( "maps/mp_maps/fx_mp_snow_gust_rooftop" );
	level._effect[ "fx_mp_snow_wall_hvy_loop_sm" ] = loadfx( "maps/mp_maps/fx_mp_snow_wall_hvy_loop_sm" );
	level._effect[ "fx_pipe_steam_md" ] = loadfx( "env/smoke/fx_pipe_steam_md" );
	level._effect[ "water_surface_bubbling" ] = loadfx( "water/water_surface_bubbling" );
	level._effect[ "snow_clifftop_runner" ] = loadfx( "snow/snow_clifftop_runner" );
	level._effect[ "snow_spray_runner_mp_mountain" ] = loadfx( "snow/snow_spray_runner_mp_mountain" );
	level._effect[ "snow_light_mp_mountain_loop" ] = loadfx( "snow/snow_light_mp_mountain_loop" );
	level._effect[ "steam_vent_mp_mountain" ] = loadfx( "smoke/steam_vent_mp_mountain" );
	level._effect[ "pipe_steam_mp_mountain" ] = loadfx( "smoke/pipe_steam_mp_mountain" );
	level._effect[ "bubbles_med_mp_mountain" ] = loadfx( "water/bubbles_med_mp_mountain" );
	level._effect[ "smoke_med_mp_mountain" ] = loadfx( "smoke/smoke_med_mp_mountain" );
	level._effect[ "smoke_large_mp_mountain" ] = loadfx( "smoke/smoke_large_mp_mountain" );
	level._effect[ "steam_vent_long" ] = loadfx( "smoke/steam_vent_long" );
	level._effect[ "snow_fall_edge_runner" ] = loadfx( "snow/snow_fall_edge_runner" );
	level._effect[ "drips_slow" ] = loadfx( "misc/drips_slow" );
	level._effect[ "transformer_spark_loop" ] = loadfx( "explosions/transformer_spark_loop" );
	level._effect[ "steam_manhole" ] = loadfx( "smoke/steam_manhole" );
	level._effect[ "fx_mp_smk_plume_md_wht_wispy" ] = loadfx( "maps/mp_maps/fx_mp_smk_plume_md_wht_wispy" );
	level._effect[ "snow_updraft_runner" ] = loadfx( "snow/snow_updraft_runner" );
	level._effect[ "snow_spiral_runner" ] = loadfx( "snow/snow_spiral_runner" );
	level._effect[ "snow_clouds" ] = loadfx( "snow/snow_clouds" );
	level._effect[ "snow_blowoff_ledge_runner" ] = loadfx( "snow/snow_blowoff_ledge_runner" );
	level._effect[ "mist_clouds_dense" ] = loadfx( "weather/mist_clouds_dense" );
	level._effect[ "mist_clouds" ] = loadfx( "weather/mist_clouds" );
	
/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_mountain_fx::main();
#/
}

