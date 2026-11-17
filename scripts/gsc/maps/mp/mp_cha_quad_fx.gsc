main()
{
	level._effect[ "fire_candle_01" ] 					= loadfx( "fire/fire_candle_01" );
	level._effect[ "light_glow_streetlight_02" ] 		= loadfx( "misc/light_glow_streetlight_02" );
	level._effect[ "fx_light_godray_md" ] 				= loadfx( "maps/zombie/fx_light_godray_md" );
	level._effect[ "fx_light_godray_overcast_lg" ] 		= loadfx( "maps/zombie/fx_light_godray_overcast_lg" );
	level._effect[ "fx_light_godray_md_mideast" ] 		= loadfx( "maps/zombie/fx_light_godray_md_mideast" );
	level._effect[ "fx_insects_swarm_md_light" ] 		= loadfx( "bio/insects/fx_insects_swarm_md_light" );
	level._effect[ "fx_insects_butterfly_flutter" ] 	= loadfx( "bio/insects/fx_insects_butterfly_flutter" );
	level._effect[ "dust_wind_medium" ] 				= loadfx( "dust/dust_wind_medium" );
	level._effect[ "dust_ceiling_slow" ] 				= loadfx( "dust/dust_ceiling_slow" );
	level._effect[ "bird" ] 							= loadfx( "misc/bird" );
	level._effect[ "test_effect" ]						= loadfx( "misc/moth_runner" );
	level._effect[ "AC130CloudMask" ]					= loadfx( "misc/clouds_cumulus_mask" );
	
	//if ( !getdvarint( "1106" ) )
	//	maps\createfx\mp_cha_quad_fx::main();
	
	/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
			maps\createfx\mp_cha_quad_fx::main();
	#/
}
