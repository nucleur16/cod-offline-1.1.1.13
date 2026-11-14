main()
{


//	level._effect[ "firelp_med_pm_cheap" ]			 				= loadfx( "fire/firelp_med_pm_cheap" );
//	level._effect[ "firelp_small_pm_a_cheap" ]		 				= loadfx( "fire/firelp_small_pm_a_cheap" );
	level._effect[ "dust_wind_fast" ]				 				= loadfx( "dust/dust_wind_fast_paper" );
//	level._effect[ "dust_wind_fast_no_paper" ]				 		= loadfx( "dust/dust_wind_fast_no_paper" );
	level._effect[ "dust_wind_slow" ]				 				= loadfx( "dust/dust_wind_slow_paper" );
	level._effect[ "dust_wind_spiral" ]				 				= loadfx( "misc/trash_spiral_runner" );
//	level._effect[ "battlefield_smokebank_s_cheap" ] 				= loadfx( "smoke/battlefield_smokebank_s_cheap" );
	level._effect[ "hallway_smoke_light" ]			 				= loadfx( "smoke/hallway_smoke_light" );
//	level._effect[ "falling_brick_runner_line_400" ] 				= loadfx( "misc/falling_brick_runner_line_400" );
	level._effect[ "room_smoke_200" ] 				 				= loadfx( "smoke/room_smoke_200" );
	level._effect[ "room_smoke_400" ] 				 				= loadfx( "smoke/room_smoke_400" );
	level._effect[ "insects_carcass_runner" ] 		 				= loadfx( "misc/insects_carcass_runner" );
	level._effect[ "electrical_transformer_spark_runner_loop" ] 	= loadfx( "explosions/electrical_transformer_spark_runner_loop" );
	level._effect[ "smoke_plume_grey_01" ] 							= loadfx( "smoke/smoke_plume_grey_01" );
	level._effect[ "smoke_plume_grey_02" ] 							= loadfx( "smoke/smoke_plume_grey_02" );
//	level._effect[ "thick_black_smoke_mp" ] 						= loadfx( "smoke/thick_black_smoke_mp" );
//	level._effect[ "falling_dirt_light_1_runner" ] 					= loadfx( "dust/falling_dirt_light_1_runner" );
//	level._effect[ "falling_dirt_light_2_runner" ] 					= loadfx( "dust/falling_dirt_light_2_runner" );
	level._effect["firelp_barrel_pm"]								= loadfx ("fire/firelp_barrel_pm");
	level._effect["airplane_smoke"]									= loadfx ("smoke/fx_sky_boom_smoke_env");
	level._effect["airplane_fly"]									= loadfx ("misc/airplane_ww2");
	level._effect["war_smoke_big"]									= loadfx ("maps/mp_maps/fx_mp_smoke_plume_lg");

/#
    if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_mogadishu_sh_fx::main();
#/
}
