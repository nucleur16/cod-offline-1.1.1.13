main()
{
	level._effect[ "ab_dragon_smoke" ] 								= loadfx( "smoke/ab_dragon_smoke" );
	level._effect[ "ab_dragon_eyes" ] 								= loadfx( "props/ab_dragon_eyes" );
	level._effect[ "ab_godray" ] 									= loadfx( "env/light/ab_godray" );
	level._effect[ "ab_interior_dust" ] 							= loadfx( "dust/ab_interior_dust" );
	level._effect[ "ab_birds_runner" ] 								= loadfx( "misc/ab_birds_runner" );
	level._effect[ "ab_dust_wind_fast_paper" ] 						= loadfx( "dust/ab_dust_wind_fast_paper" );
	level._effect[ "dust_spray_detail_oriented_runner" ] 			= loadfx( "dust/dust_spray_detail_oriented_runner" );
	level._effect[ "firework_winner" ] 								= loadfx( "explosions/firework_winner" );
	level._effect[ "ab_smoke_grenade_red" ] 						= loadfx( "smoke/ab_smoke_grenade_red" );
	level._effect[ "smoke_grenade_red" ] 							= loadfx( "smoke/smoke_grenade_red" );
	level._effect[ "firework_explosion_runner" ] 					= loadfx( "explosions/firework_explosion_runner" );
	level._effect[ "rocket_smoke" ] 								= loadfx( "maps/mp_abandoned_sh/rocket_smoke" );

	level._effect[ "firelp_large_pm" ]								= loadfx( "fire/firelp_large_pm" );
	level._effect[ "firelp_med_pm" ]								= loadfx( "fire/firelp_med_pm" );
	level._effect[ "firelp_small_pm" ]								= loadfx( "fire/firelp_small_pm" );

	level._effect[ "traffic" ]										= loadfx( "props/traffic_mp_abandon" );

	level._effect[ "moth_runner" ]									= loadfx( "misc/moth_runner" );
	level._effect[ "insect_trail_runner_icbm" ]						= loadfx( "misc/insect_trail_runner_icbm" );
	level._effect[ "insects_carcass_runner" ]						= loadfx( "misc/insects_carcass_runner" );

	level._effect[ "leaves_fall_gentlewind" ]						= loadfx( "misc/leaves_fall_gentlewind" );
	level._effect[ "leaves_ground_gentlewind" ]						= loadfx( "misc/leaves_ground_gentlewind" );
	level._effect[ "fog_ground_200" ]								= loadfx( "smoke/fog_ground_200" );
	level._effect[ "trash_spiral_runner" ]							= loadfx( "misc/ab_trash_spiral_runner" );

	level._effect[ "hallway_smoke_light" ]							= loadfx( "smoke/hallway_smoke_light" );
	level._effect[ "room_smoke_200" ]								= loadfx( "smoke/room_smoke_200" );
	level._effect[ "ground_fog_600_abandon" ]						= loadfx( "smoke/ground_fog_600_abandon" );
	
	level._effect["fx_mp_ray_light_sm_yellow_01"]						= loadfx("env/light/fx_light_godray_overcast_sm_yellow_01");
	level._effect["fx_mp_ray_light_sm_yellow_02"]						= loadfx("env/light/fx_light_godray_overcast_sm_yellow_02");

/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_abandon_sh_fx::main();
#/

}
