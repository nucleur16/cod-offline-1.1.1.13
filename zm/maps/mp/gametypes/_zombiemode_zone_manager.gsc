// Zombies spawn-zone selection from the C1 map's info_volume entities.
// T5 maps/_zombiemode_zone_manager.gsc zone_init/manage_zones: each
// player_volume targets its own script_struct spawner group.  CoDOL's
// prototype and training maps also carry T5's three door flags exactly.

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	level.zm_spawn_zones = [];
	level.zm_zone_mode = getDvar( "mapname" );

	// This VM's filtered getEntArray only accepts native ent_fields; C1's
	// script_noteworthy is dynamic and returns undefined as a filter key.
	// Enumerate once and test the authored keys on each live entity.
	entities = getEntArray();
	if ( !isDefined( entities ) )
		entities = [];
	for ( i = 0; i < entities.size; i++ )
	{
		volume = entities[ i ];
		if ( !isDefined( volume.classname ) || volume.classname != "info_volume" ||
			!isDefined( volume.script_noteworthy ) || volume.script_noteworthy != "player_volume" )
			continue;
		if ( !isDefined( volume.targetname ) || !isDefined( volume.target ) )
			continue;

		zone = zone_by_name( volume.targetname );
		if ( !isDefined( zone ) )
		{
			zone = spawnStruct();
			zone.name = volume.targetname;
			zone.spawner_target = volume.target;
			zone.volumes = [];
			zone.spawners = [];
			zone.enabled = false;

			group = get_struct_array( volume.target, "targetname" );
			if ( !isDefined( group ) )
				group = [];
			for ( j = 0; j < group.size; j++ )
			{
				if ( isDefined( group[ j ].script_noteworthy ) && group[ j ].script_noteworthy == "zombie_spawner" )
					zone.spawners[ zone.spawners.size ] = group[ j ];
			}
			level.zm_spawn_zones[ level.zm_spawn_zones.size ] = zone;
		}
		zone.volumes[ zone.volumes.size ] = volume;
	}

	// The first room is authored as start_zone on Prototype and Training.
	// Asylum has split starts; player occupancy selects the correct side.
	if ( level.zm_zone_mode == "zm_prototype_sh" || level.zm_zone_mode == "zm_zombie_training" )
	{
		zone = zone_by_name( "start_zone" );
		if ( isDefined( zone ) )
			zone.enabled = true;
	}

	level.enemy_spawns = [];
	if ( level.zm_spawn_zones.size < 1 )
	{
		// Some server builds do not instantiate C1's info_volume entities.
		// Keep the match playable until native entity spawning is corrected.
		level.enemy_spawns = get_struct_array( "zombie_spawner", "script_noteworthy" );
		if ( !isDefined( level.enemy_spawns ) )
			level.enemy_spawns = [];
		codolprintln( "CoDOL zm zones: no live player_volume; using all authored spawners" );
		return;
	}
	refresh_spawners();
	level thread zone_watch();
	codolprintln( "CoDOL zm zones: " + level.zm_spawn_zones.size + " map-authored player volumes/groups on " + level.zm_zone_mode );
	if ( isDefined( level.exterior_goals ) )
	{
		available = 0;
		for ( i = 0; i < level.exterior_goals.size; i++ )
		{
			if ( entrance_available( level.exterior_goals[ i ] ) )
				available++;
		}
		codolprintln( "CoDOL zm zones: entrances in open zones " + available + "/" + level.exterior_goals.size );
	}
}

zone_by_name( name )
{
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
	{
		if ( level.zm_spawn_zones[ i ].name == name )
			return level.zm_spawn_zones[ i ];
	}
	return undefined;
}

enable_zone( name )
{
	zone = zone_by_name( name );
	if ( isDefined( zone ) && !zone.enabled )
	{
		zone.enabled = true;
		codolprintln( "CoDOL zm zones: enabled " + name + " (" + zone.spawners.size + " spawners)" );
	}
}

zone_watch()
{
	level endon( "intermission" );
	for ( ;; )
	{
		refresh_spawners();
		wait( 1 );
	}
}

refresh_spawners()
{
	// These are the map's actual purchased-door/debris flags.  The exact
	// prototype graph is T5 zombie_cod5_prototype::prototype_zone_init.
	if ( level.zm_zone_mode == "zm_prototype_sh" || level.zm_zone_mode == "zm_zombie_training" )
	{
		if ( flag( "start_2_upstairs" ) || flag( "box_2_upstairs" ) )
			enable_zone( "upstairs_zone" );
		if ( flag( "start_2_box" ) || flag( "box_2_upstairs" ) )
			enable_zone( "box_zone" );
	}

	// Asylum has no recoverable map-specific adjacency script.  An authored
	// zone becomes reachable when a live player actually enters its volume;
	// purchased doors govern physical access to that volume.
	players = getPlayers();
	occupied = [];
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
	{
		zone = level.zm_spawn_zones[ i ];
		occupied[ i ] = false;
		for ( j = 0; j < players.size && !occupied[ i ]; j++ )
		{
			if ( !isAlive( players[ j ] ) )
				continue;
			for ( k = 0; k < zone.volumes.size; k++ )
			{
				if ( players[ j ] isTouching( zone.volumes[ k ] ) )
				{
					occupied[ i ] = true;
					// Prototype and Training have authored purchase flags.  A
					// volume may overlap a closed doorway, so touching it alone
					// must never unlock its spawners.
					if ( level.zm_zone_mode != "zm_prototype_sh" && level.zm_zone_mode != "zm_zombie_training" )
						zone.enabled = true;
					break;
				}
			}
		}
	}

	new_spawns = [];
	active_names = "";
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
	{
		zone = level.zm_spawn_zones[ i ];
		active = occupied[ i ];
		if ( !active && ( level.zm_zone_mode == "zm_prototype_sh" || level.zm_zone_mode == "zm_zombie_training" ) )
			active = occupied_adjacent_zone( zone.name, occupied );
		if ( !zone.enabled || !active )
			continue;
		active_names = active_names + zone.name + ",";
		for ( j = 0; j < zone.spawners.size; j++ )
			new_spawns[ new_spawns.size ] = zone.spawners[ j ];
	}

	// The T5 manager retains the initial room when nobody occupies an
	// eligible volume (loading, downed players, or a zone without spawners).
	if ( new_spawns.size < 1 )
	{
		for ( i = 0; i < level.zm_spawn_zones.size; i++ )
		{
			zone = level.zm_spawn_zones[ i ];
			if ( !zone.enabled )
				continue;
			active_names = active_names + zone.name + ",";
			for ( j = 0; j < zone.spawners.size; j++ )
				new_spawns[ new_spawns.size ] = zone.spawners[ j ];
		}
	}

	level.enemy_spawns = new_spawns;
	if ( !isDefined( level.zm_last_active_zone_names ) || active_names != level.zm_last_active_zone_names )
	{
		level.zm_last_active_zone_names = active_names;
		codolprintln( "CoDOL zm zones: active " + active_names + " spawners " + new_spawns.size );
	}
}

entrance_available( node )
{
	if ( !isDefined( level.zm_spawn_zones ) || level.zm_spawn_zones.size < 1 )
		return true;
	if ( !isDefined( node.target ) )
		return false;

	// The exterior_goal targets a node_negotiation_end just inside its
	// window.  Classify that endpoint against the authored player volumes,
	// then allow only the groups opened by the matching purchase flags.
	ends = getEntArray( node.target, "targetname" );
	if ( !isDefined( ends ) || ends.size < 1 )
		return false;
	point = ends[ 0 ].origin;
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
	{
		zone = level.zm_spawn_zones[ i ];
		for ( j = 0; j < zone.volumes.size; j++ )
		{
			mins = zone.volumes[ j ] getAbsMins();
			maxs = zone.volumes[ j ] getAbsMaxs();
			if ( point[ 0 ] < mins[ 0 ] || point[ 0 ] > maxs[ 0 ] ||
				point[ 1 ] < mins[ 1 ] || point[ 1 ] > maxs[ 1 ] ||
				point[ 2 ] < mins[ 2 ] || point[ 2 ] > maxs[ 2 ] )
				continue;
			if ( zone.enabled )
				return true;
		}
	}
	return false;
}

occupied_adjacent_zone( name, occupied )
{
	for ( z = 0; z < level.zm_spawn_zones.size; z++ )
	{
		if ( !occupied[ z ] )
			continue;
		other = level.zm_spawn_zones[ z ].name;
		if ( ( ( name == "start_zone" && other == "upstairs_zone" ) || ( name == "upstairs_zone" && other == "start_zone" ) ) && flag( "start_2_upstairs" ) )
			return true;
		if ( ( ( name == "start_zone" && other == "box_zone" ) || ( name == "box_zone" && other == "start_zone" ) ) && flag( "start_2_box" ) )
			return true;
		if ( ( ( name == "box_zone" && other == "upstairs_zone" ) || ( name == "upstairs_zone" && other == "box_zone" ) ) && flag( "box_2_upstairs" ) )
			return true;
	}
	return false;
}
