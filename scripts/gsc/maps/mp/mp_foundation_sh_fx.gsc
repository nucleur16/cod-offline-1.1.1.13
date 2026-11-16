main()
{
	level._effect[ "cement_powder_upper_red_sh" ] = loadfx( "misc/cement_powder_upper_red_sh" );
	level._effect[ "cement_powder_falling_splash_red_sh" ] = loadfx( "misc/cement_powder_falling_splash_red_sh" );
	level._effect[ "steam_vent_large_wind_sh" ] = loadfx( "smoke/steam_vent_large_wind_sh" );
	level._effect[ "firelp_small_cheap_nodist_foundation" ] = loadfx( "fire/firelp_small_cheap_nodist_foundation" );
	level._effect[ "smoke_thick_distant_white" ] = loadfx( "smoke/smoke_thick_distant_white" );
//	level._effect[ "steam_thick" ] = loadfx( "smoke/steam_thick" );
	level._effect[ "leaves_fall_gentlewind_codo" ] = loadfx( "misc/leaves_fall_gentlewind_codo" );
	level._effect[ "leaves_ground_gentlewind_codo" ] = loadfx( "misc/leaves_ground_gentlewind_codo" );
	level._effect[ "hawk" ] = loadfx( "weather/hawk" );
	level._effect[ "fx_mp_fumes_vent_sm" ] = loadfx( "maps/mp_maps/fx_mp_fumes_vent_sm" );
	level._effect[ "fire_falling_runner_point_infrequent" ] = loadfx( "fire/fire_falling_runner_point_infrequent" );
	level._effect[ "after_math_embers" ] = loadfx( "fire/after_math_embers" );
	level._effect[ "embers_prague_light" ] = loadfx( "weather/embers_prague_light" );
	level._effect[ "falling_ash_mp" ] = loadfx( "misc/falling_ash_mp" );
	
	level._effect[ "moth_runner" ]									= loadfx( "misc/moth_runner" );
	level._effect[ "insect_trail_runner_icbm" ]						= loadfx( "misc/insect_trail_runner_icbm" );
	level._effect[ "insects_carcass_runner" ]						= loadfx( "misc/insects_carcass_runner" );
	
	level._effect[ "cement_powder_falling" ] 						= loadfx( "misc/cement_powder_falling" );
	level._effect[ "cement_powder_upper" ] 							= loadfx( "misc/cement_powder_upper" );
	level._effect[ "cement_powder_falling_splash" ] 				= loadfx( "misc/cement_powder_falling_splash" );
	
	level._effect[ "water_drips_fat_slow_speed_mp_cement" ] 		= loadfx( "water/water_drips_fat_slow_speed_mp_cement" );
	level._effect[ "waterfall_drainage_mp_cement" ] 				= loadfx( "water/waterfall_drainage_mp_cement" );
	level._effect[ "waterfall_drainage_short_london" ] 				= loadfx( "water/waterfall_drainage_short_london" );
	level._effect[ "waterfall_splash_medium_london" ] 				= loadfx( "water/waterfall_splash_medium_london" );
	level._effect[ "drainpipe_runoff" ] 							= loadfx( "water/drainpipe_runoff" );
	level._effect[ "drainpipe_runoff_splash" ] 						= loadfx( "water/drainpipe_runoff_splash" );

	level._effect[ "room_smoke_200" ] 								= loadfx( "smoke/room_smoke_200" );
	level._effect[ "room_smoke_400" ] 								= loadfx( "smoke/room_smoke_400" );

	level._effect[ "steam_vent_large_wind" ] 						= loadfx( "smoke/steam_vent_large_wind" );
	level._effect[ "steam_vent_small_nomodel_far" ] 				= loadfx( "smoke/steam_vent_small_nomodel_far" );
	
	level._effect[ "mist_drifting" ] 								= loadfx( "smoke/mist_drifting" );
	level._effect[ "mist_drifting_london_docks" ] 					= loadfx( "smoke/mist_drifting_london_docks" );
	level._effect[ "mist_drifting_groundfog" ] 						= loadfx( "smoke/mist_drifting_groundfog" );
	
	level._effect[ "firelp_small_pm" ] 								= loadfx( "fire/firelp_small_pm" );


	level._effect[ "falling_dirt_frequent_runner" ]				 	= loadfx( "dust/falling_dirt_frequent_runner" );
	level._effect[ "falling_dirt_frequent_runner_cement" ]			= loadfx( "dust/falling_dirt_frequent_runner_cement" );
	level._effect[ "falling_dirt_infrequent_runner" ]				= loadfx( "dust/falling_dirt_infrequent_runner" );


/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_foundation_sh_fx::main();
#/

}
