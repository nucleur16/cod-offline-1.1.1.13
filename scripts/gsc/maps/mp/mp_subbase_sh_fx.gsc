main()
{
	level._effect[ "snow_light" ]		 = loadfx( "snow/snow_light_mp_subbase_sh" );
	level._effect[ "snow_wind" ]		 = loadfx( "snow/snow_wind_subbase" );
	level._effect[ "snow_spiral" ]		 = loadfx( "snow/snow_spiral_runner" );
	level._effect["snow_spray_detail_oriented_large_runner_sh"]		    = loadfx ("snow/snow_spray_detail_oriented_large_runner_sh");
	level._effect["snow_spray_detail_oriented_runner_400x400_sh"]		= loadfx ("snow/snow_spray_detail_oriented_runner_400x400_sh");
	level._effect["snow_spray_detail_oriented"]				            = loadfx ("snow/snow_spray_detail_oriented");

/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_subbase_sh_fx::main();
#/

}
