// PROVENANCE (2026-11-13): mp_offshore is CODOL 3.19.26.22's port, converted to 1.1.1.13 by CODOL-AssetExplorer port-map.
// Level script: IW5 mp_roughneck ("Off Shore"; COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_roughneck.gsc), translated to the 1.1.1.13 script set.
// The zone's own maps/mp/mp_offshore_fx.gsc and maps/createfx/mp_offshore_fx.gsc (3.19.26.22) are used as they ship.
// Precache: maps/mp/mp_offshore_precache.gsc names the modules the zone's MapEnts give in precache_script keys.
// Adaptations:
//   * ambient ambient_mp_roughneck -> ambient_mp_offshore (the zone's alias)
//   * audio_settings() (maps\mp\_audio::add_reverb "quarry") left out: this tree has no maps\mp\_audio
//   * compass: compass_map_mp_offshore (the zone's)
//   * the remote_uav / remote_tank checks in watchPlayerEnterWater are left out (IW5 killstreaks this tree does not have)
//   * sm_sunShadowScale takes the PC value (IW5 picks 0.5 on PS3)
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\mp\mp_offshore_precache::main();
	maps\createart\mp_offshore_art::main();
	maps\mp\mp_offshore_fx::main();
	maps\mp\_explosive_barrels::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_offshore" );
	ambientPlay( "ambient_mp_offshore" );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.33 );
	setdvar( "r_diffuseColorScale", 1.25 );
	setdvar( "r_specularColorScale", 2.75 );
	setdvar( "sm_sunShadowScale", "0.75" );

	game[ "attackers" ] = "axis";
	game[ "defenders" ] = "allies";

	water = getEnt( "trigger_underwater", "targetname" );
	if ( isDefined( water ) )
	{
		water thread watchPlayerEnterWater();
		level thread clearWaterVarsOnSpawn( water );
	}
}

watchPlayerEnterWater()
{
	level endon( "game_ended" );

	for ( ;; )
	{
		self waittill( "trigger", player );

		if ( !isPlayer( player ) )
			continue;

		if ( !isAlive( player ) )
			continue;

		if ( !isDefined( player.inWater ) )
		{
			player.inWater = true;
			player thread playerUnderWater( self );
		}
	}
}

playerUnderWater( trigger )
{
	level endon( "game_ended" );
	self endon( "disconnect" );

	if ( isDefined( level._effect[ "body_splash" ] ) )
		playFX( level._effect[ "body_splash" ], self.origin );

	if ( !self isUsingRemote() )
		self shellShock( "default", 8 );
}

clearWaterVarsOnSpawn( trigger )
{
	level endon( "game_ended" );

	for ( ;; )
	{
		level waittill( "player_spawned", player );

		if ( !player isTouching( trigger ) )
		{
			player.inWater = undefined;
			player stopShellShock();
		}
	}
}
