main()
{
	level._effect[ "ab_mk_firecracker_explosion_runner" ] 			= loadfx( "explosions/ab_mk_firecracker_explosion_runner" );
	level._effect[ "bumper_car_sparks" ] 							= loadfx( "maps/mp_abandon_sh_mk/bumper_car_sparks" );
	level._effect[ "rocketship_launch" ] 							= loadfx( "maps/mp_abandon_sh_mk/rocketship_launch" );
	level._effect[ "fireworks_01_launch" ] 							= loadfx( "misc/fireworks/fireworks_01_launch" );
	level._effect[ "fireworks_02_launch" ] 							= loadfx( "misc/fireworks/fireworks_02_launch" );	
	level._effect[ "fireworks_03_launch" ] 							= loadfx( "misc/fireworks/fireworks_03_launch" );
	level._effect[ "ab_mk_dragon_smoke" ] 							= loadfx( "smoke/ab_mk_dragon_smoke" );
	level._effect[ "ab_dragon_eyes" ] 								= loadfx( "props/ab_dragon_eyes" );
	level._effect[ "ab_interior_dust" ] 							= loadfx( "dust/ab_interior_dust" );
	level._effect[ "ab_birds_runner" ] 								= loadfx( "misc/ab_birds_runner" );
	level._effect[ "ab_dust_wind_fast_paper" ] 						= loadfx( "dust/ab_dust_wind_fast_paper" );
	level._effect[ "dust_spray_detail_oriented_runner" ] 			= loadfx( "dust/dust_spray_detail_oriented_runner" );
	level._effect[ "firework_winner" ] 								= loadfx( "explosions/firework_winner" );
	level._effect[ "rocket_smoke" ] 								= loadfx( "maps/mp_abandoned_sh/rocket_smoke" );
	level._effect[ "moth_runner" ]									= loadfx( "misc/moth_runner" );
	level._effect[ "trash_spiral_runner" ]							= loadfx( "misc/ab_trash_spiral_runner" );
	level._effect[ "insect_trail_runner_icbm" ]						= loadfx( "misc/insect_trail_runner_icbm" );
	level._effect[ "insects_carcass_runner" ]						= loadfx( "misc/insects_carcass_runner" );
	level._effect[ "light_glow_worldoftomorrow" ] 					= loadfx( "maps/mp_abandoned_sh/light_glow_worldoftomorrow" );

/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_abandon_sh_mk_fx::main();
#/

}
