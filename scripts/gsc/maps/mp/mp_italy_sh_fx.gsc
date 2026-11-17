main()
{
	//ambient fx

	//original fx names marked as follows: !! doesn't work, ** works, but isn't added, ++ has been added

//	**	level._effect[ "sand_spray_detail_oriented_runner_mp_dome" ]					= loadfx( "dust/sand_spray_detail_oriented_runner_mp_dome" );
//
//	++  level._effect[ "room_dust_100" ]												= loadfx( "dust/room_dust_100_blend" );	
//	**  level._effect[ "battlefield_smokebank_S_warm" ]									= loadfx( "smoke/battlefield_smokebank_S_warm" );
//	++  level._effect[ "dust_wind_fast_paper" ]											= loadfx( "dust/dust_wind_fast_paper" );
//	++  level._effect[ "trash_spiral_runner" ]											= loadfx( "misc/trash_spiral_runner" );
//	++  level._effect[ "room_dust_200_mp_seatown" ]										= loadfx( "dust/room_dust_200_blend_mp_seatown" );	
//		
	
//	!!  level._effect[ "ceiling_smoke_seatown" ]										= loadfx( "weather/ceiling_smoke_seatown" );
//	**  level._effect[ "insects_carcass_flies" ] 										= loadfx( "misc/insects_carcass_flies" );
//	**  level._effect[ "spark_fall_runner_mp" ] 			                  			= loadfx( "explosions/spark_fall_runner_mp" );
//	!!  level._effect[ "seatown_lookout_splash_runner" ] 								= loadfx( "water/seatown_lookout_splash_runner" );
//	!!  level._effect[ "splash_lookout_runner" ] 										= loadfx( "water/splash_lookout_runner" );
//	!!  level._effect[ "seatown_pillar_mist" ] 											= loadfx( "water/seatown_pillar_mist" );
//	!!  level._effect[ "palm_leaves" ] 		         									= loadfx( "misc/palm_leaves" );
//	!!  level._effect[ "falling_dirt_frequent_runner" ] 								= loadfx( "dust/falling_dirt_frequent_runner" );
//	!!  level._effect[ "flocking_birds_mp" ] 					    					= loadfx( "misc/flocking_birds_mp" );
//	!!  ??  level._effect[ "insects_light_hunted_a_mp" ] 		  							= loadfx( "misc/insects_light_hunted_a_mp" );
//	**  level._effect[ "firelp_small_cheap_mp" ] 		      							= loadfx( "fire/firelp_small_cheap_mp" );
//	!!  level._effect[ "car_fire_mp" ] 		                							= loadfx( "fire/car_fire_mp" );
//	!!  ??level._effect[ "bg_smoke_plume" ] 		            							= loadfx( "smoke/bg_smoke_plume" );
//		(bg_smoke_plume_mp works)
//	!!  level._effect[ "room_dust_200_blend_seatown_wind_fast" ]						= loadfx( "dust/room_dust_200_blend_seatown_wind_fast" );	
//
//
//	++  level._effect[ "leaves_fall_gentlewind_green" ] 		                		= loadfx( "misc/leaves_fall_gentlewind_green" );
//	++  level._effect[ "leaves_spiral_runner" ] 		                				= loadfx( "misc/leaves_spiral_runner" );
//	++  level._effect[ "leaves_ground_gentlewind" ] 		                			= loadfx( "misc/leaves_ground_gentlewind" );
//
//
//	!!  ??level._effect[ "waterfall_drainage_splash_small" ] 		                		= loadfx( "water/waterfall_drainage_splash_small" );
//		(_mp works)
	
//	!!  level._effect[ "water_drips_point_infrequent" ] 		                		= loadfx( "water/water_drips_point_infrequent" );
//	!!  level._effect[ "water_drips_fat_fast_speed" ] 		                			= loadfx( "water/water_drips_fat_fast_speed" );
//	!!  ??level._effect[ "waterfall_drainage_short_london" ] 		                		= loadfx( "water/waterfall_drainage_short_london" );
//		(_short works)
//	!!  level._effect[ "waterfall_splash_medium_london" ] 		                		= loadfx( "water/waterfall_splash_medium_london" );
//	!!  level._effect[ "water_fountain_spout" ] 		                				= loadfx( "water/water_fountain_spout" );

	level._effect[ "water_fountain_splash_impact" ] 								= loadfx( "water/water_fountain_splash_impact" );
	level._effect[ "water_fountain_splash" ] 										= loadfx( "water/water_fountain_splash" );
	level._effect[ "water_fountain_splash_sm" ] 									= loadfx( "water/water_fountain_splash_sm" );
	level._effect[ "water_fountain_splash_ring" ] 									= loadfx( "water/water_fountain_splash_impact_ring" );
	level._effect[ "water_fountain_splash_ring_sm" ] 								= loadfx( "water/water_fountain_splash_impact_ring_sm" );
	level._effect[ "mp_italy_fountain" ]			 								= loadfx( "water/mp_italy_fountain" );
	level._effect[ "mp_italy_fountain_splash_top" ]	 								= loadfx( "water/mp_italy_fountain_splash_top" );
	level._effect[ "water_surface_bubbling" ] 										= loadfx( "water/water_surface_bubbling_fast" );
	level._effect[ "water/creek_splash" ]											= loadfx( "water/creek_splash" );

	
//resurrected using either original or CODO versions, from the commented out fx above
	
	level._effect[ "leaves_fall_gentlewind_codo" ] 		                			= loadfx( "misc/leaves_fall_gentlewind_codo" );
	level._effect[ "leaves_ground_gentlewind_codo" ] 		                		= loadfx( "misc/leaves_ground_gentlewind_codo" );
	level._effect[ "leaves_spiral_runner" ] 		                				= loadfx( "misc/leaves_spiral_runner" );

	level._effect[ "trash_spiral_runner" ] 		                					= loadfx( "misc/trash_spiral_runner" );
	level._effect[ "dust_wind_fast_paper" ]											= loadfx( "dust/dust_wind_fast_paper" );
	level._effect[ "room_dust_200_blend" ]											= loadfx( "dust/room_dust_200_blend_mp_creek" );	
	level._effect[ "room_dust_100_blend" ]											= loadfx( "dust/room_dust_100_blend" );	

	level._effect[ "battlefield_smokebank_S_warm" ]									= loadfx( "smoke/battlefield_smokebank_S_warm" );
    level._effect[ "insects_moths" ] 												= loadfx( "misc/insects_moths" );
	level._effect["hawk"]															= loadfx ("weather/hawk");
	level._effect[ "drip_single" ] 													= loadfx( "misc/drip_single_point" );
	
	
	
	
/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )	
		maps\createfx\mp_italy_sh_fx::main();
#/
	
///#
//    if ( getdvar( "574" ) != "1" )
//        maps\createfx\mp_italy_sh_fx::main();
//#/

}
