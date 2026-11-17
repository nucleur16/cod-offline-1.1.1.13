main()
{
	level._effect[ "water_fountain_splash" ] = loadfx( "water/water_fountain_splash" );
	level._effect[ "fx_mp_sand_windy_heavy_sm_slow" ] = loadfx( "maps/mp_maps/fx_mp_sand_windy_heavy_sm_slow" );
	level._effect[ "bg_smoke_plume_mp" ] = loadfx( "smoke/bg_smoke_plume_mp" );
	level._effect[ "oil_rig_fire" ] = loadfx( "fire/oil_rig_fire" );
	level._effect[ "sand_spray_detail_oriented_runner_mp_dome" ] = loadfx( "dust/sand_spray_detail_oriented_runner_mp_dome" );
	level._effect[ "sand_storm_mp_dome_exterior" ]              = loadfx( "weather/sand_storm_mp_dome_exterior" );
	level._effect[ "dust_wind_fast" ]						= loadfx( "dust/dust_wind_fast" );
	level._effect[ "ash_spiral_runner" ]			 			= loadfx( "dust/ash_spiral_runner" );
	level._effect[ "hallway_smoke_light" ]                      = loadfx( "smoke/hallway_smoke_light" );
	level._effect[ "test_effect" ]										 = loadfx( "misc/moth_runner" );

/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_offshore_fx::main();
#/

}
