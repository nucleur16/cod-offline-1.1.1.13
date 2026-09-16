main()
{

	level._effect[ "ground_smoke_launch_a" ]								 = loadfx( "smoke/ground_smoke_launch_a" );
	level._effect[ "dust_ceiling_ash_large_mp_vacant" ]			             = loadfx( "dust/dust_ceiling_ash_large_mp_killhouse_sh");
	level._effect[ "floodlight_white" ]					                     = loadfx( "misc/floodlight_white_sh" );
	level._effect[ "light_shaft_dust_large_mp_vacant_sidewall" ]             = loadfx( "dust/light_shaft_dust_large_mp_vacant_sidewall" );
	
 /#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_killhouse_sh_fx::main();
#/

}
