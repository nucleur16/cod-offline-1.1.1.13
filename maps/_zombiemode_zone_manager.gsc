// Zombies spawn-zone selection from the C1 map's info_volume entities.
// T5 maps/_zombiemode_zone_manager.gsc zone_init/manage_zones: each
// player_volume targets its own script_struct spawner group.  CoDOL's
// prototype and training maps also carry T5's three door flags exactly.

#include maps\_zombiemode_utility;

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

	// Spawners that cannot put a zombie on the map are never used: no floor
	// under them (zm_zombie_training carries Prototype's spawner groups at
	// Prototype's coordinates; several drop to z -2031).
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
		level.zm_spawn_zones[ i ].spawners = spawners_with_floor( level.zm_spawn_zones[ i ] );

	if ( level.zm_zone_mode == "zm_zombie_training" )
		training_group_spawners_by_window();

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
		{}
		return;
	}
	refresh_spawners();
	level thread zone_watch();
	{}
	if ( isDefined( level.exterior_goals ) )
	{
		available = 0;
		for ( i = 0; i < level.exterior_goals.size; i++ )
		{
			if ( entrance_available( level.exterior_goals[ i ] ) )
				available++;
		}
		{}
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
		{}
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
	// Training carries these flag names only on Prototype's leftover debris
	// triggers; its own doors (door2..door5_tutorial) set no flag.
	if ( level.zm_zone_mode == "zm_prototype_sh" )
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
					if ( level.zm_zone_mode != "zm_prototype_sh" )
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
		if ( !active && level.zm_zone_mode == "zm_prototype_sh" )
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
		{}
	}
}

// T5 _zombiemode_zone_manager.gsc deactivate_initial_barrier_goals (:138)
// and enable_zone (:300): an exterior_goal WITHOUT a script_noteworthy is
// always a valid entrance; one WITH a script_noteworthy is off until the zone
// "<zone>_barriers" names is enabled.  T5's zombie_think itself searches every
// exterior_goal (:471, :508, :666).
//
// The test this replaced classified the first entity the goal targets -- a
// board or the clip brushmodel, whose origin sits on the window line -- against
// the player volumes, and accepted the window only when that point fell inside
// an enabled zone.  The window line is the volume's edge, so 8 of Prototype's
// 12 windows always failed (4/12 at round start with every zone enabled).
// No CoDOL zm map authors a script_noteworthy on an exterior_goal (Prototype,
// Asylum, Training), so every window is available, as in T5.
entrance_available( node )
{
	if ( !isDefined( node ) )
		return false;
	if ( !isDefined( node.script_noteworthy ) || node.script_noteworthy == "" )
		return true;
	if ( !isDefined( level.zm_spawn_zones ) )
		return true;
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
	{
		zone = level.zm_spawn_zones[ i ];
		if ( zone.enabled && node.script_noteworthy == zone.name + "_barriers" )
			return true;
	}
	return false;
}

// The point a zombie lands on after climbing through this window: the
// node_negotiation_end of the window's traversal (the begin node sits 15-20
// units outside the exterior_goal, the end 60-90 units inside it).
entrance_inside_point( goal )
{
	if ( isDefined( goal.zm_inside_point ) )
		return goal.zm_inside_point;

	nodes = getAllNodes();
	best = undefined;
	best_dist = 160 * 160;
	for ( i = 0; isDefined( nodes ) && i < nodes.size; i++ )
	{
		if ( !isDefined( nodes[ i ].classname ) || nodes[ i ].classname != "node_negotiation_end" )
			continue;
		d = distanceSquared( nodes[ i ].origin, goal.origin );
		if ( d < best_dist )
		{
			best = nodes[ i ].origin;
			best_dist = d;
		}
	}
	goal.zm_inside_point = best;
	return best;
}

// The zone whose player volume contains point, else undefined.  Bounds test
// with the same slack T5's volume check gets from the player's own hull.
zone_at_point( point )
{
	if ( !isDefined( point ) || !isDefined( level.zm_spawn_zones ) )
		return undefined;
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
	{
		zone = level.zm_spawn_zones[ i ];
		for ( j = 0; j < zone.volumes.size; j++ )
		{
			mins = zone.volumes[ j ] getAbsMins();
			maxs = zone.volumes[ j ] getAbsMaxs();
			if ( point[ 0 ] >= mins[ 0 ] - 16 && point[ 0 ] <= maxs[ 0 ] + 16 &&
				point[ 1 ] >= mins[ 1 ] - 16 && point[ 1 ] <= maxs[ 1 ] + 16 &&
				point[ 2 ] >= mins[ 2 ] - 16 && point[ 2 ] <= maxs[ 2 ] + 16 )
				return zone;
		}
	}
	return undefined;
}

spawners_with_floor( zone )
{
	kept = [];
	for ( i = 0; i < zone.spawners.size; i++ )
	{
		sp = zone.spawners[ i ];
		trace = bulletTrace( sp.origin + ( 0, 0, 16 ), sp.origin - ( 0, 0, 256 ), false, undefined );
		if ( trace[ "fraction" ] < 1 )
			kept[ kept.size ] = sp;
		else
			{}
	}
	return kept;
}

// The zone a window's inside point belongs to: the volume that contains it,
// else the nearest volume within max_dist (bounds distance).
zone_for_point( point, max_dist )
{
	zone = zone_at_point( point );
	if ( isDefined( zone ) )
		return zone;
	best = undefined;
	best_dist = max_dist * max_dist;
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
	{
		for ( j = 0; j < level.zm_spawn_zones[ i ].volumes.size; j++ )
		{
			d = distance_to_volume_sq( point, level.zm_spawn_zones[ i ].volumes[ j ] );
			if ( d < best_dist )
			{
				best = level.zm_spawn_zones[ i ];
				best_dist = d;
			}
		}
	}
	return best;
}

distance_to_volume_sq( point, volume )
{
	mins = volume getAbsMins();
	maxs = volume getAbsMaxs();
	total = 0;
	for ( k = 0; k < 3; k++ )
	{
		gap = 0;
		if ( point[ k ] < mins[ k ] )
			gap = mins[ k ] - point[ k ];
		else if ( point[ k ] > maxs[ k ] )
			gap = point[ k ] - maxs[ k ];
		total += gap * gap;
	}
	return total;
}

// zm_zombie_training (CoDOL's own map; no T5 counterpart, and no CoDOL zone
// ships a level script for it) authors its zombie spawners per WINDOW:
// window_spawners (2) target nodes inside the window at (565 1335 272), and
// window1..3_wave1..3 (18) target nodes inside the three final-room windows.
// None of those groups is the target of a player_volume, so the volume-based
// grouping never used them.  The map's player_volumes instead target
// upstairs_zone_spawners / box_zone_spawners, which are Prototype's groups
// left in the map at Prototype's coordinates (every one of them is 800-1750
// units outside Training's volumes; none can reach a Training window).
//
// So, as T5 groups spawners by the zone their window opens into: every
// targeted spawner joins the zone its window's inside point lies in (the
// window at (565 1335 272) opens 124 units past upstairs_zone's last volume,
// the nearest), and an untargeted spawner stays in its authored zone only
// when it stands within 192 units of that zone's volumes (start_zone's own
// spawner (125 -1246 272) is 148 away; the nearest Prototype leftover is 270).
training_group_spawners_by_window()
{
	groups = [];
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
	{
		zone = level.zm_spawn_zones[ i ];
		kept = [];
		for ( j = 0; j < zone.spawners.size; j++ )
		{
			sp = zone.spawners[ j ];
			if ( isDefined( sp.target ) && sp.target != "" )
				continue;
			near = false;
			for ( k = 0; k < zone.volumes.size && !near; k++ )
				near = distance_to_volume_sq( sp.origin, zone.volumes[ k ] ) <= 192 * 192;
			if ( near )
				kept[ kept.size ] = sp;
			else
				{}
		}
		zone.spawners = kept;
	}

	if ( !isDefined( level.exterior_goals ) )
		return;

	all = get_struct_array( "zombie_spawner", "script_noteworthy" );
	added = 0;
	for ( i = 0; i < all.size; i++ )
	{
		sp = all[ i ];
		if ( !isDefined( sp.target ) || sp.target == "" )
			continue;
		node = getNode( sp.target, "targetname" );
		if ( !isDefined( node ) )
			continue;
		window = get_closest( node.origin, level.exterior_goals );
		if ( !isDefined( window ) )
			continue;
		zone = zone_for_point( entrance_inside_point( window ), 256 );
		if ( !isDefined( zone ) )
			continue;
		trace = bulletTrace( sp.origin + ( 0, 0, 16 ), sp.origin - ( 0, 0, 256 ), false, undefined );
		if ( trace[ "fraction" ] >= 1 )
			continue;
		zone.spawners[ zone.spawners.size ] = sp;
		added++;
	}
	{}
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

// T5 _zombiemode_zone_manager.gsc add_adjacent_zone, for a level script that
// declares its zone graph (zm_sumpf_sh, from T5 zombie_cod5_sumpf.gsc
// sumpf_zone_init).  Call it from the level script's main(): data only, the
// zones themselves are built later by init().  Two-way, as T5's default;
// one_way (T5's fourth argument) makes zone_b active from zone_a only.
add_adjacent_zone( zone_a, zone_b, flag_name, one_way )
{
	if ( !isDefined( level.zm_zone_adjacency ) )
		level.zm_zone_adjacency = [];
	link = spawnStruct();
	link.zone_a = zone_a;
	link.zone_b = zone_b;
	link.flag_name = flag_name;
	link.one_way = is_true( one_way );
	level.zm_zone_adjacency[ level.zm_zone_adjacency.size ] = link;
}

// T5 enable_zone on the adjacency flag: both ends open.
adjacency_enable_zones()
{
	for ( i = 0; i < level.zm_zone_adjacency.size; i++ )
	{
		link = level.zm_zone_adjacency[ i ];
		if ( !flag( link.flag_name ) )
			continue;
		enable_zone( link.zone_a );
		enable_zone( link.zone_b );
	}
}

// T5 manage_zones: a zone is active when a player is in it or in a zone
// joined to it by a set adjacency flag.
adjacency_occupied_neighbour( name, occupied )
{
	for ( i = 0; i < level.zm_zone_adjacency.size; i++ )
	{
		link = level.zm_zone_adjacency[ i ];
		other = undefined;
		// T5 one_way: only zone_a lists zone_b as adjacent, so an occupied
		// zone_b does not wake zone_a.
		if ( link.zone_a == name && !link.one_way )
			other = link.zone_b;
		else if ( link.zone_b == name )
			other = link.zone_a;
		if ( !isDefined( other ) || !flag( link.flag_name ) )
			continue;
		for ( z = 0; z < level.zm_spawn_zones.size; z++ )
		{
			if ( occupied[ z ] && level.zm_spawn_zones[ z ].name == other )
				return true;
		}
	}
	return false;
}
