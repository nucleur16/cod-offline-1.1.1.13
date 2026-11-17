// PROVENANCE (2026-11-13): mp_foundation_sh is CODOL 3.19.26.22's port, converted to 1.1.1.13 by CODOL-AssetExplorer port-map.
// Level script: IW5 mp_cement (COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_cement.gsc), translated to the 1.1.1.13 script set.
// The zone's own maps/mp/mp_foundation_sh_fx.gsc and maps/createfx/mp_foundation_sh_fx.gsc (3.19.26.22) are used as they ship.
// Precache: maps/mp/mp_foundation_sh_precache.gsc names the modules the zone's MapEnts give in precache_script keys.
// Adaptations:
//   * ambientPlay( "ambient_mp_cement" ) left out: the zone has no such alias
//   * compass: compass_map_mp_foundation_sh (the zone's)
//   * common_scripts\_interactive::init() and the level.func[] anim hooks are left out (IW5 interactive props; no _interactive in this tree)
//   * spawn_blocker_collision waits at most 30 s for level.airDropCrateCollision (IW5 waits forever)
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	precachemodel( "tag_origin" );
	maps\mp\mp_foundation_sh_precache::main();
	maps\createart\mp_foundation_sh_art::main();
	maps\mp\mp_foundation_sh_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_foundation_sh" );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.33 );
	setdvar( "sm_sunShadowScale", "0.8" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	thread spawn_blocker_collision( ( 166, -670, 388 ), ( 0, 270, 0 ) );
	thread spawn_blocker_collision( ( 166, -670, 360 ), ( 0, 270, 0 ) );
	thread spawn_blocker_collision( ( 166, -728, 388 ), ( 0, 270, 0 ) );
	thread spawn_blocker_collision( ( 166, -728, 360 ), ( 0, 270, 0 ) );
	thread spawn_blocker_collision( ( 166, -786, 388 ), ( 0, 270, 0 ) );
	thread spawn_blocker_collision( ( 166, -786, 360 ), ( 0, 270, 0 ) );
}

spawn_blocker_collision( origin, angles )
{
	level endon( "game_ended" );
	for ( i = 0; i < 600 && !isDefined( level.airDropCrateCollision ); i++ )
		wait 0.05;
	if ( !isDefined( level.airDropCrateCollision ) )
		return;
	collision = spawn( "script_model", ( 0, 0, 0 ) );
	collision setModel( "tag_origin" );
	collision.origin = origin;
	collision.angles = angles;
	collision cloneBrushmodelToScriptmodel( level.airDropCrateCollision );
}
