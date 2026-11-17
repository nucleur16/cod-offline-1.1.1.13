main()
{
	
	//ambient fx
	//level._effect[ "gas_coop_storm" ] 			= loadfx( "weather/gas_coop_storm" );
	level._effect[ "dust_ceiling" ] 				= loadfx( "dust/dust_ceiling_ash_large_mp_killhouse_sh" );
	level._effect[ "heater_big" ] 				            = loadfx( "distortion/heater_big" );
	level._effect[ "insects_carcass_flies_c" ] 				= loadfx( "misc/insects_carcass_flies_c" );
	level._effect[ "oxygen_leak" ] 				    = loadfx( "distortion/oxygen_leak" );
	//level._effect[ "drips_fast" ]                           = loadfx( "smoke/drips_fast" );
	//level._effect[ "smoke_grenade_11sec_mp" ]               = loadfx( "smoke/smoke_grenade_11sec_mp" );
	
	level._effect[ "light_c4_blink_nodlight" ] 			= loadfx( "misc/light_c4_blink_nodlight" );
	level._effect["bombexplosion"] = loadfx("explosions/tanker_explosion");
	
/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_storm_spring_fx::main();
#/

}
