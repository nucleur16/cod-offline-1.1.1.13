// PROVENANCE (2026-11-13): mp_underground_sh is CODOL 3.19.26.22's port, converted to 1.1.1.13 by CODOL-AssetExplorer port-map.
// Level script: IW5 mp_underground (COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_underground.gsc), translated to the 1.1.1.13 script set.
// The zone's own maps/mp/mp_underground_sh_fx.gsc and maps/createfx/mp_underground_sh_fx.gsc (3.19.26.22) are used as they ship.
// Precache: maps/mp/mp_underground_sh_precache.gsc names the modules the zone's MapEnts give in precache_script keys.
// Adaptations:
//   * ambientPlay( "ambient_mp_underground" ) left out: the zone has no such alias
//   * audio_settings() (maps\mp\_audio::add_reverb "arena") left out: this tree has no maps\mp\_audio
//   * compass: compass_map_mp_underground_sh (the zone's)
//   * ac130_thread returns when the level has no ac130_overhead_* entities (IW5 would loop over empty groups)
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\mp\mp_underground_sh_precache::main();
	maps\createart\mp_underground_sh_art::main();
	maps\mp\mp_underground_sh_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_underground_sh" );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.33 );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	thread ac130_thread();
}

// the AC-130 silhouettes flying over the map (ac130_overhead_1..3 models moving to their targets)
ac130_thread()
{
	level endon( "game_ended" );
	speed = 1200;
	groups = 3;
	group = 0;

	for ( ;; )
	{
		group = group % groups + 1;
		planes = getEntArray( "ac130_overhead_" + group, "targetname" );
		longest = 0;
		spawned = [];

		foreach ( plane in planes )
		{
			if ( !isDefined( plane.target ) || !isDefined( plane.model ) )
				continue;
			dest = getEnt( plane.target, "targetname" );
			if ( !isDefined( dest ) )
				continue;
			model = spawn( "script_model", plane.origin );
			model.angles = plane.angles;
			model setModel( plane.model );
			dist = distance( dest.origin, plane.origin );
			planeSpeed = speed;

			if ( isDefined( plane.speed ) )
				planeSpeed = plane.speed;
			moveTime = dist / planeSpeed;

			if ( moveTime > longest )
				longest = moveTime;
			model moveTo( dest.origin, moveTime );
			model thread play_plane_fx( moveTime );
			spawned[ spawned.size ] = model;
		}

		if ( spawned.size == 0 )
			return; // the level has no ac130_overhead_* entities

		wait( longest );

		foreach ( model in spawned )
		{
			model notify( "delete_plane" );
			wait 0.05;
			model delete();
		}

		wait( randomfloatrange( 5, 20 ) );
	}
}

play_plane_fx( moveTime )
{
	self endon( "delete_plane" );
}
