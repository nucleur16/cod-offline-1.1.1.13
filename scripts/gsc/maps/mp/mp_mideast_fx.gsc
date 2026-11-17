main()
{
	level._effect[ "water_fountain_splash_02" ] = loadfx( "water/water_fountain_splash_02" );
	level._effect[ "dust_wind_slow_mpmideast" ] = loadfx( "dust/dust_wind_slow_mpmideast" );
	level._effect[ "dust_ceiling_slow" ] = loadfx( "dust/dust_ceiling_slow" );
	level._effect[ "light_glow_walllight_white" ] 					= loadfx( "misc/light_glow_walllight_white" );
	level._effect[ "cgoshp_drips_a" ] 								= loadfx( "misc/cgoshp_drips_a" );
	level._effect[ "fx_light_godray_md_mideast" ] 					= loadfx( "maps/zombie/fx_light_godray_md_mideast" );
	level._effect[ "test_effect" ]									= loadfx( "misc/moth_runner" );
	level._effect[ "drips_fountain_sm_slow" ]						= loadfx( "misc/drips_fountain_sm_slow" );
	level._effect[ "drips_fountain_sm_fast" ]						= loadfx( "misc/drips_fountain_sm_fast" );
	level._effect[ "water_splash_sm" ]								= loadfx( "water/water_splash_sm" );
	level._effect[ "water_fountain" ]								= loadfx( "water/water_fountain" );
	level._effect[ "water_fountain_splash" ]						= loadfx( "water/water_fountain_splash" );
	level._effect[ "fx_debris_dust_motes" ]							= loadfx( "maps/ber2/fx_debris_dust_motes" );
	level._effect[ "bio/insects/fx_insects_swarm_md_light" ]		= loadfx( "bio/insects/fx_insects_swarm_md_light" );
	
	
/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_mideast_fx::main();
#/
}
