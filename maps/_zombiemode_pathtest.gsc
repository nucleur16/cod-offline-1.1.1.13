// Developer-only test verbs for the zombie entrance walk (spawner -> window
// -> boards -> traversal -> inside).
//
// Runs only on a developer server (the live server runs developer 0).  Drive it
// through the dvar scr_codol_pathtest:
//
//   addbot                    add one test client and keep every player alive
//   hold                      stop the round's own spawning and remove the
//                             zombies it already made (the soak owns the actors)
//   openzones                 enable every authored zone, as if every door and
//                             debris had been bought
//   openall                   buy every door and debris (scr_codol_blockertest
//                             "fire" on each trigger) and enable every zone
//   soak <seconds> <alive> <all|live>
//                             spawn zombies for <seconds>, at most <alive> at a
//                             time, cycling through every authored spawner
//                             ("all") or the round's active ones ("live").  Each
//                             zombie is watched until it is inside (traversal
//                             done, or hunting inside a player volume); then it
//                             is removed.  Stuck = no progress for 10 s before
//                             entering while not tearing or traversing.
//   soak <seconds> <alive> <all|live> keep
//                             as above, but a zombie that gets inside is kept
//                             and hunts the players (a steady chase load of
//                             <alive> actors, for frame-cost profiling).
//   report                    print the running tallies
//   botto <x> <y> <z>         move the first player (the test client) there
//   zones                     print every zone's volumes and spawners, and the
//                             zone each window opens into
//   list                      print the soak's spawner list
//   spawnat <x> <y> <z> <i>   one watched zombie at a point, walking to the
//                             window of spawner <i> of the soak's list
//
// Written for "sometimes the zombies get stuck when pathing to the windows".

#include maps\_zombiemode_utility;

init()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;

	setDvar( "scr_codol_pathtest", "" );
	level thread pathtest_watch();
}

pathtest_watch()
{
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_codol_pathtest" );
		if ( request == "" )
			continue;
		setDvar( "scr_codol_pathtest", "" );

		tokens = strTok( request, " " );
		switch ( tokens[ 0 ] )
		{
		case "addbot":
			addtestclient();
			{}
			level thread pathtest_god();
			break;

		case "hold":
			flag_clear( "spawn_zombies" );
			pathtest_clear();
			{}
			break;

		case "openzones":
			pathtest_open_zones();
			break;

		case "openall":
			level thread pathtest_open_all();
			break;

		case "soak":
			seconds = 300;
			alive = 16;
			mode = "all";
			if ( tokens.size > 1 )
				seconds = int( tokens[ 1 ] );
			if ( tokens.size > 2 )
				alive = int( tokens[ 2 ] );
			if ( tokens.size > 3 )
				mode = tokens[ 3 ];
			level.pathtest_keep = tokens.size > 4 && tokens[ 4 ] == "keep";
			level thread pathtest_soak( seconds, alive, mode );
			break;

		case "report":
			pathtest_report();
			break;

		case "shoottear":
			percent = 25;
			if ( tokens.size > 1 )
				percent = int( tokens[ 1 ] );
			level thread pathtest_shoot_tear( percent );
			break;

		case "repair":
			level thread pathtest_repair( 6 );
			break;

		case "botto":
			players = getPlayers();
			if ( tokens.size >= 4 && players.size > 0 )
			{
				players[ 0 ] setOrigin( ( int( tokens[ 1 ] ), int( tokens[ 2 ] ), int( tokens[ 3 ] ) ) );
				{}
			}
			break;

		case "zones":
			pathtest_dump_zones();
			break;

		case "list":
			spawners = pathtest_all_spawners();
			for ( i = 0; i < spawners.size; i++ )
				{}
			break;

		case "spawnat":
			if ( tokens.size < 5 )
				break;
			level thread pathtest_spawn_at( ( int( tokens[ 1 ] ), int( tokens[ 2 ] ), int( tokens[ 3 ] ) ), int( tokens[ 4 ] ) );
			break;
		}
	}
}

// shoottear <percent>: what players do at a window, every 2 s each zombie
// that is tearing boards (or waiting at its spot) dies with this chance.  A
// zombie shot while it held an attack spot must give the spot back, or the
// window starves (VPS 2026-10-05, zm_asylum_sh).
pathtest_shoot_tear( percent )
{
	level notify( "pathtest_shoot_tear" );
	level endon( "pathtest_shoot_tear" );
	{}
	if ( percent <= 0 )
		return;
	for ( ;; )
	{
		wait( 2 );
		if ( !isDefined( level.zombies ) )
			continue;
		zombies = level.zombies;
		for ( i = 0; i < zombies.size; i++ )
		{
			z = zombies[ i ];
			if ( !isDefined( z ) || !isAlive( z ) || !isDefined( z.attacking_spot ) )
				continue;
			if ( randomInt( 100 ) >= percent )
				continue;
			{}
			z.no_powerup_drop = true;
			z doDamage( z.health + 100, z.origin );
		}
	}
}

// repair: every 6 s rebuild one torn board of every window (the player
// repair loop's replace_chunk, flying board and mid_repair state included).
pathtest_repair( seconds )
{
	level notify( "pathtest_repair" );
	level endon( "pathtest_repair" );
	{}
	for ( ;; )
	{
		wait( seconds );
		if ( !isDefined( level.exterior_goals ) )
			continue;
		for ( i = 0; i < level.exterior_goals.size; i++ )
		{
			node = level.exterior_goals[ i ];
			if ( !isDefined( node.barrier_chunks ) )
				continue;
			chunk = get_random_destroyed_chunk( node.barrier_chunks );
			if ( !isDefined( chunk ) )
				continue;
			node thread maps\_zombiemode_blockers::replace_chunk( chunk, false );
			if ( isDefined( node.clip ) )
			{
				node.clip enable_trigger();
				node.clip disconnectpaths();
				node.clip.paths_open = false;
			}
		}
	}
}

// Every player stays alive: the soak measures the walk, not the fight.
pathtest_god()
{
	level notify( "pathtest_god" );
	level endon( "pathtest_god" );

	for ( ;; )
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( !isAlive( players[ i ] ) )
				continue;
			players[ i ].maxhealth = 100000;
			players[ i ].health = 100000;
		}
		wait( 0.1 );
	}
}

pathtest_clear()
{
	if ( !isDefined( level.zombies ) )
		return;
	zombies = level.zombies;
	for ( i = 0; i < zombies.size; i++ )
	{
		if ( isDefined( zombies[ i ] ) && isAlive( zombies[ i ] ) )
		{
			zombies[ i ].no_powerup_drop = true;
			zombies[ i ] doDamage( zombies[ i ].health + 100, zombies[ i ].origin );
		}
	}
}

pathtest_open_zones()
{
	if ( !isDefined( level.zm_spawn_zones ) )
		return;
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
		level.zm_spawn_zones[ i ].enabled = true;
	{}
}

pathtest_open_all()
{
	players = getPlayers();
	if ( players.size < 1 )
		return;
	client = players[ 0 ] getEntityNumber();
	for ( pass = 0; pass < 4; pass++ )
	{
		trigs = maps\_zombiemode_blockertest::blockertest_triggers();
		if ( trigs.size < 1 )
			break;
		for ( i = 0; i < trigs.size; i++ )
		{
			if ( isDefined( trigs[ i ] ) )
				maps\_zombiemode_blockertest::blockertest_buy( "fire", client, trigs[ i ] getEntityNumber() );
		}
	}
	pathtest_open_zones();
	{}
}

// The spawners a match can use: every zone's group when the map authors
// zones (Training also carries Prototype's leftover groups, which no zone
// ever enables), else every authored zombie_spawner.
pathtest_dump_zones()
{
	if ( !isDefined( level.zm_spawn_zones ) )
		return;
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
	{
		zone = level.zm_spawn_zones[ i ];
		{}
		for ( j = 0; j < zone.volumes.size; j++ )
			{}
		for ( j = 0; j < zone.spawners.size; j++ )
		{
			sp = zone.spawners[ j ];
			target = "none";
			if ( isDefined( sp.target ) )
				target = sp.target;
			{}
		}
	}
	if ( !isDefined( level.exterior_goals ) )
		return;
	for ( i = 0; i < level.exterior_goals.size; i++ )
	{
		goal = level.exterior_goals[ i ];
		inside = maps\_zombiemode_zone_manager::entrance_inside_point( goal );
		zone = "none";
		if ( isDefined( inside ) )
		{
			z = maps\_zombiemode_zone_manager::zone_at_point( inside );
			if ( isDefined( z ) )
				zone = z.name;
		}
		{}
	}
}

pathtest_all_spawners()
{
	if ( isDefined( level.zm_spawn_zones ) && level.zm_spawn_zones.size > 0 )
	{
		all = [];
		for ( i = 0; i < level.zm_spawn_zones.size; i++ )
		{
			for ( j = 0; j < level.zm_spawn_zones[ i ].spawners.size; j++ )
				all[ all.size ] = level.zm_spawn_zones[ i ].spawners[ j ];
		}
		return all;
	}
	all = get_struct_array( "zombie_spawner", "script_noteworthy" );
	if ( !isDefined( all ) )
		all = [];
	return all;
}

pathtest_inside_volume( origin )
{
	if ( !isDefined( level.zm_spawn_zones ) )
		return false;
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
	{
		zone = level.zm_spawn_zones[ i ];
		for ( j = 0; j < zone.volumes.size; j++ )
		{
			mins = zone.volumes[ j ] getAbsMins();
			maxs = zone.volumes[ j ] getAbsMaxs();
			if ( origin[ 0 ] >= mins[ 0 ] && origin[ 0 ] <= maxs[ 0 ] &&
				origin[ 1 ] >= mins[ 1 ] && origin[ 1 ] <= maxs[ 1 ] &&
				origin[ 2 ] >= mins[ 2 ] && origin[ 2 ] <= maxs[ 2 ] )
				return true;
		}
	}
	return false;
}

pathtest_soak( seconds, alive, mode )
{
	level notify( "pathtest_soak" );
	level endon( "pathtest_soak" );

	level.pathtest_spawned = 0;
	level.pathtest_entered = 0;
	level.pathtest_stuck = 0;
	level.pathtest_stuck_zombies = 0;
	level.pathtest_timeout = 0;
	level.pathtest_enter_ms = 0;
	level.pathtest_alive = 0;
	level.pathtest_died = 0;

	spawners = pathtest_all_spawners();
	{}

	deadline = getTime() + seconds * 1000;
	next = 0;
	while ( getTime() < deadline )
	{
		if ( level.pathtest_alive >= alive )
		{
			wait( 0.25 );
			continue;
		}

		spawn_point = undefined;
		index = -1;
		if ( mode == "live" )
		{
			if ( isDefined( level.enemy_spawns ) && level.enemy_spawns.size > 0 )
				spawn_point = level.enemy_spawns[ randomInt( level.enemy_spawns.size ) ];
		}
		else if ( spawners.size > 0 )
		{
			index = next % spawners.size;
			spawn_point = spawners[ index ];
			next++;
		}
		if ( !isDefined( spawn_point ) )
		{
			wait( 1 );
			continue;
		}
		if ( index < 0 )
		{
			for ( i = 0; i < spawners.size; i++ )
			{
				if ( spawners[ i ] == spawn_point )
					index = i;
			}
		}

		ai = maps\_zombiemode_spawner::spawn_zombie( spawn_point );
		if ( !isDefined( ai ) )
		{
			wait( 1 );
			continue;
		}
		level.pathtest_spawned++;
		level.pathtest_alive++;
		ai thread pathtest_watch_zombie( index, spawn_point );
		wait( 0.5 );
	}

	// Let the last ones finish (or time out) before the summary.
	for ( i = 0; i < 260 && level.pathtest_alive > 0; i++ )
		wait( 1 );

	pathtest_report();
	{}
}

pathtest_spawn_at( origin, index )
{
	spawners = pathtest_all_spawners();
	if ( index < 0 || index >= spawners.size )
		return;
	spawn_point = spawnStruct();
	spawn_point.origin = origin;
	spawn_point.angles = ( 0, 0, 0 );
	spawn_point.target = spawners[ index ].target;
	ai = maps\_zombiemode_spawner::spawn_zombie( spawn_point );
	if ( !isDefined( ai ) )
		return;
	if ( !isDefined( level.pathtest_spawned ) )
	{
		level.pathtest_spawned = 0;
		level.pathtest_entered = 0;
		level.pathtest_stuck = 0;
		level.pathtest_stuck_zombies = 0;
		level.pathtest_timeout = 0;
		level.pathtest_enter_ms = 0;
		level.pathtest_alive = 0;
		level.pathtest_died = 0;
	}
	level.pathtest_spawned++;
	level.pathtest_alive++;
	{}
	ai thread pathtest_watch_zombie( index, spawn_point );
}

pathtest_watch_zombie( index, spawn_point )
{
	self endon( "pathtest_done" );

	self thread pathtest_watch_death();
	self thread pathtest_watch_traverse();

	start = getTime();
	num = self getEntityNumber();
	samples = [];
	stuck_reported = false;
	window = "?";

	for ( ;; )
	{
		wait( 1 );
		if ( !isDefined( self ) || !isAlive( self ) )
			return;

		if ( isDefined( self.first_node ) )
			window = "" + self.first_node.origin;

		phase = "entrance";
		if ( isDefined( self.zm_path_phase ) )
			phase = self.zm_path_phase;

		entered = is_true( self.pathtest_traversed ) ||
			( phase == "hunt" && pathtest_inside_volume( self.origin ) );
		if ( entered )
		{
			level.pathtest_entered++;
			level.pathtest_enter_ms += getTime() - start;
			codolprintln( "CoDOL pathtest: ent " + num + " spawner " + index + " ENTERED via " + window + " after " +
				( getTime() - start ) + " ms at " + self.origin );
			if ( is_true( level.pathtest_keep ) )
				return;
			self pathtest_finish();
			return;
		}

		samples[ samples.size ] = self.origin;
		busy = phase == "tear" || is_true( self.pathtest_traversing );
		if ( busy )
			samples = [];

		if ( samples.size > 10 )
		{
			old = samples[ samples.size - 11 ];
			if ( distanceSquared( old, self.origin ) < 32 * 32 )
			{
				level.pathtest_stuck++;
				if ( !stuck_reported )
				{
					stuck_reported = true;
					level.pathtest_stuck_zombies++;
				}
				{}
				samples = [];
			}
		}

		if ( getTime() - start > 240000 )
		{
			level.pathtest_timeout++;
			{}
			self pathtest_finish();
			return;
		}
	}
}

pathtest_watch_death()
{
	self endon( "pathtest_done" );
	self waittill( "death" );
	level.pathtest_alive--;
	level.pathtest_died++;
	self notify( "pathtest_done" );
}

pathtest_watch_traverse()
{
	self endon( "death" );
	self endon( "pathtest_done" );
	self waittill( "zombie_start_traverse" );
	self.pathtest_traversing = true;
	self waittill( "zombie_end_traverse" );
	self.pathtest_traversing = false;
	self.pathtest_traversed = true;
}

// Called from the watcher thread, which ends on "pathtest_done": count and
// remove first, notify last.
pathtest_finish()
{
	level.pathtest_alive--;
	self.no_powerup_drop = true;
	self thread pathtest_remove();
	self notify( "pathtest_done" );
}

pathtest_remove()
{
	wait( 0.05 );
	if ( isDefined( self ) && isAlive( self ) )
		self doDamage( self.health + 100, self.origin );
}

pathtest_report()
{
	if ( !isDefined( level.pathtest_spawned ) )
		return;
	avg = 0;
	if ( level.pathtest_entered > 0 )
		avg = int( level.pathtest_enter_ms / level.pathtest_entered );
	{}
}
