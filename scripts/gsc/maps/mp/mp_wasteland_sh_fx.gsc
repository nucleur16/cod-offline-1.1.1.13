main()
{
	level._effect[ "smoke_large_ow2" ] = loadfx( "smoke/smoke_large_ow2" );
	level._effect[ "thin_black_smoke_m" ] = loadfx( "smoke/thin_black_smoke_m" );
	level._effect[ "fire_car_sm" ] = loadfx( "fire/fire_car_sm" );
	level._effect[ "fx_mp_fire_small_detail" ] = loadfx( "maps/mp_maps/fx_mp_fire_small_detail" );
	level._effect[ "fx_mp_fire_rubble_small" ] = loadfx( "maps/mp_maps/fx_mp_fire_rubble_small" );
	level._effect[ "smoke_crater_sh" ] 								= loadfx( "smoke/smoke_crater_sh" );
	level._effect[ "fx_mp_fire_medium" ] 							= loadfx( "maps/mp_maps/fx_mp_fire_medium" );
	level._effect[ "fx_water_drip_light_long_wasteland_sh" ] 		= loadfx( "env/water/fx_water_drip_light_long_wasteland_sh" );
	level._effect[ "fx_mp_light_gray_warm_lrg" ] 					= loadfx( "maps/mp_maps/fx_mp_light_gray_warm_lrg" );
	level._effect[ "fx_mp_light_gray_warm_sm" ] 					= loadfx( "maps/mp_maps/fx_mp_light_gray_warm_sm" );
	level._effect[ "fx_mp_light_gray_warm_md" ] 					= loadfx( "maps/mp_maps/fx_mp_light_gray_warm_md" );
	level._effect[ "drip_single_point" ] 							= loadfx( "misc/drip_single_point" );
	//ambient fx
	level._effect[ "insect_trail_runner_icbm" ]						 = loadfx( "misc/insect_trail_runner_icbm" );
	level._effect[ "leaves_fall_gentlewind" ]						 = loadfx( "misc/leaves_fall_gentlewind" );
	level._effect[ "leaves_ground_gentlewind" ]						 = loadfx( "misc/leaves_ground_gentlewind" );
	level._effect[ "ground_fog1200x1200_brecourt" ]					 = loadfx( "smoke/ground_fog1200x1200_wasteland_sh" );
	level._effect[ "fog_ground_200" ]								 = loadfx( "smoke/fog_ground_200" );


/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_wasteland_sh_fx::main();
#/

}
