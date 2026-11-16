main()
{

	// ambient fx
	level._effect[ "dust_wind_fast" ]					= loadfx( "dust/dust_wind_fast" );
	level._effect[ "dust_wind_spiral" ]					= loadfx( "dust/dust_spiral_runner" );
	level._effect[ "firelp_small_pm" ]					= loadfx( "fire/firelp_small_pm" );
	level._effect[ "firelp_small_pm_a" ]				= loadfx( "fire/firelp_small_pm_a" );
	level._effect[ "thin_black_smoke_L" ]				= loadfx( "smoke/thin_black_smoke_L" );
	level._effect[ "battlefield_smokebank_S" ]	 		= loadfx( "smoke/battlefield_smokebank_S" );
	level._effect[ "tank_fire_turret_abrams" ]	 		= loadfx( "fire/tank_fire_turret_abrams" );
	level._effect[ "tank_fire_hatch" ]			 		= loadfx( "fire/tank_fire_hatch" );
	level._effect[ "firelp_vhc_farview" ]			 	= loadfx( "fire/firelp_vhc_lrg_pm_farview" );
	level._effect[ "paper_falling" ]					= loadfx( "misc/paper_falling" );
	level._effect[ "damage_smoke_loop" ]			 	= loadfx( "smoke/damage_smoke_loop" );
	level._effect[ "paper_blowing_burnt_loop" ]			= loadfx( "misc/paper_blowing_burnt_loop" );
	level._effect[ "codo_godray_afgan" ]			 	= loadfx( "env/light/codo_godray_afgan" );
	level._effect["firelp_m1a1_cookoff"]				= loadfx ("fire/firelp_m1a1_cookoff");
	level._effect[ "AC130CloudMask" ]					= loadfx( "misc/clouds_cumulus_mask" );

	

/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_ambush_sh_fx::main();
#/
}
