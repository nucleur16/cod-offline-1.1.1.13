main()
{
	level._effect[ "awp_snow_blizzard_small" ]										 = loadfx( "snow/awp_snow_blizzard_small" );

/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_awp_arena_fx::main();
#/

}
