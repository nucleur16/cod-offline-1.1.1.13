main()
{
	level._effect[ "fx_mp_light_gray_warm_sm" ] = loadfx( "maps/mp_maps/fx_mp_light_gray_warm_sm" );
	level._effect[ "fx_mp_light_gray_warm_md" ] = loadfx( "maps/mp_maps/fx_mp_light_gray_warm_md" );
	level._effect[ "fx_mp_light_gray_warm_lrg" ] = loadfx( "maps/mp_maps/fx_mp_light_gray_warm_lrg" );
	level._effect[ "fx_mp_light_dust_motes_md" ] = loadfx( "maps/mp_maps/fx_mp_light_dust_motes_md" );
	level._effect[ "light_shaft_motes_ow" ] = loadfx( "dust/light_shaft_motes_ow" );
	level._effect[ "fx_light_godray_md_mideast" ] = loadfx( "maps/zombie/fx_light_godray_md_mideast" );

	level._effect["firelp_med_pm"]					= loadfx ("fire/firelp_med_pm_nodistort");	
	level._effect["firelp_small_pm"]				= loadfx ("fire/firelp_small_pm");
	level._effect["firelp_small_pm_a"]				= loadfx ("fire/firelp_small_pm_a");
	level._effect["dust_wind_fast"]					= loadfx ("dust/dust_wind_fast");
	level._effect["dust_wind_slow"]					= loadfx ("dust/dust_wind_slow_yel_loop");
	level._effect["dust_wind_spiral"]				= loadfx ("dust/dust_spiral_runner");
	level._effect["hawk"]							= loadfx ("weather/hawk");
	level._effect["bird_seagull_flock_large"]		= loadfx ("misc/bird_seagull_flock_large");
	level._effect[ "sand_storm_skybox" ] 			= loadfx( "weather/sand_storm_skybox_backlot" );

/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_backlot_sh_fx::main();
#/		
}
