// PROVENANCE (2026-11-13): mp_nightshift_sh is CODOL 3.19.26.22's port, converted to 1.1.1.13 by CODOL-AssetExplorer port-map.
// Level script: IW4 mp_nightshift ("Skidrow"; COD-GSC-Source/IW4-GSC/maps/mp/mp_nightshift.gsc; this tree has it as maps/mp/mp_nightshift.gsc), translated to the 1.1.1.13 script set.
// The zone's own maps/mp/mp_nightshift_sh_fx.gsc and maps/createfx/mp_nightshift_sh_fx.gsc (3.19.26.22) are used as they ship.
// Precache: maps/mp/mp_nightshift_sh_precache.gsc names the modules the zone's MapEnts give in precache_script keys.
// Adaptations:
//   * ambientPlay( "ambient_mp_urban" ) left out: the zone has no such alias
//   * compass: compass_map_mp_nightshift_sh (the zone's)
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\mp\mp_nightshift_sh_precache::main();
	maps\mp\mp_nightshift_sh_fx::main();
	maps\createart\mp_nightshift_art::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_nightshift_sh" );
	VisionSetNaked( "mp_nightshift_sh" );
	setdvar( "compassmaxrange", "2400" );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.2 );
	setdvar( "r_lightGridContrast", 1 );

	game[ "attackers" ] = "axis";
	game[ "defenders" ] = "allies";

	level.airstrikeHeightScale = 1.5;
	level._effect[ "explosive_fx" ] = loadfx( "explosions/tanker_explosion" );
	level._effect[ "train_dust" ] = loadfx( "dust/train_dust" );
	level._effect[ "train_dust_linger" ] = loadfx( "dust/train_dust_linger" );
	thread hallwayPlunger();
}

hallwayPlunger()
{
	plunger = getEnt( "plunger", "targetname" );
	if ( !isDefined( plunger ) || !isDefined( plunger.target ) )
		return;

	plunger waittill( "trigger" );
	explosives = getEntArray( plunger.target, "targetname" );
	foreach ( explosive in explosives )
	{
		wait( 0.25 );
		rot = randomfloat( 360 );
		explosionEffect = spawnFx( level._effect[ "explosive_fx" ], explosive.origin + ( 0, 0, 0 ), ( 0, 0, 1 ), ( cos( rot ), sin( rot ), 0 ) );
		triggerFx( explosionEffect );
		radiusDamage( explosive.origin, 384, 200, 30 );
		thread playSoundinSpace( "exp_suitcase_bomb_main", explosive.origin );
		explosive delete();
	}
	plunger delete();
}
