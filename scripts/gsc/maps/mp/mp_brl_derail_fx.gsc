// PROVENANCE (2026-10-22): mp_brl_derail_fx.gsc as CODOL 3.5.6.2 (build 3.5.2.6) ships it in
// ff_dlc255_mp_brl_derail, with two changes: the include of common_scripts\shared (not in this
// tree; nothing here uses it) is dropped, and the createfx call is a developer block as in the
// other usermaps (the client runs the zone's own createfx copy).
#include common_scripts\utility;
#include maps\mp\_utility;
//#include maps\_debug;

main()
{
	level._effect[ "cold_breath" ]				 = loadfx( "misc/cold_breath" );
	
	//Ambient fx
//	level._effect[ "tree_snow_dump_runner" ]						= loadfx( "snow/tree_snow_dump_runner" );
	level._effect[ "snow_spray_detail_contingency_runner_0x400" ]	= loadfx( "snow/snow_spray_detail_contingency_runner_0x400" );
	level._effect[ "snow_spray_detail_oriented_runner_0x400" ]		= loadfx( "snow/snow_spray_detail_oriented_runner_0x400" );
	level._effect[ "snow_spray_detail_oriented_runner_400x400" ]	= loadfx( "snow/snow_spray_detail_oriented_runner_400x400" );
	level._effect[ "snow_spray_detail_oriented_runner" ]			= loadfx( "snow/snow_spray_detail_oriented_runner" );
	level._effect[ "snow_spray_detail_oriented_runner_large" ]		= loadfx( "snow/snow_spray_detail_oriented_large_runner" );
	level._effect[ "snow_spray_large_oriented_runner" ]				= loadfx( "snow/snow_spray_large_oriented_runner" );
	level._effect[ "snow_vortex_runner_cheap" ]						= loadfx( "snow/snow_vortex_runner_cheap" );

	level._effect[ "snow_clouds" ]									= loadfx( "snow/snow_clouds_mp_derail" );
//	level._effect[ "room_smoke_200" ] 								= LoadFX( "smoke/room_smoke_200" );
	level._effect[ "room_smoke_200" ] 								= loadfx( "smoke/room_smoke_200" );
	level._effect[ "snow_spiral_runner" ]							= loadfx( "snow/snow_spiral_runner" );


	//Player Footstep fx
	level._effect[ "footstep_snow_small" ]							= loadfx( "impacts/footstep_snow_small" );
	level._effect[ "footstep_snow" ]								= loadfx( "impacts/footstep_snow" );

	//Player snow
	level._effect[ "snow_light" ]								 	= loadfx( "snow/snow_light_contingency" );
	
	level thread playerEffect();
	
/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_brl_derail_fx::main();
#/
}

playerCheckInside()
{
	level endon ( "game_ended" );
	level endon( "special_op_terminated" );
	level endon( "team_extracted" );
	level endon( "extraction_failed" );
	level endon( "stop_snow" );
	self endon( "death" );
	self endon( "disconnect" );

	//just in case
	self notify( "inside_check" );
	self endon( "inside_check" );
	
	while(1)
	{
		self.is_under_cover = false;
		eye = self GetEye();
		ceiling = eye + (0,0,350);
		end = PhysicsTrace( eye, ceiling );
		if ( end[2] < ceiling[2] )
		{
			self.is_under_cover = true;
		}
		wait(1);
	}
}

playerEffect()
{
	level endon( "stop_snow" );
	level waittill( "player_spawned" );
	for ( ;; )
	{
		foreach( player in level.players )
		{
			if ( !IsDefined( player.is_under_cover ) )
			{
				player.is_under_cover = false;
				player thread playerCheckInside();
			}
			else if ( !player.is_under_cover )
			{
				playfx( getfx( "snow_light" ), player.origin + ( 0, 0, 300 ), player.origin + ( 0, 0, 350 ) );
			}
		}
		wait( 0.075 );
	}
}

