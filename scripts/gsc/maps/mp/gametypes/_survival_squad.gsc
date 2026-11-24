// survival: squads of a leader and followers.
//
// Ported from Infinity Ward's Modern Warfare 3 maps\_squad_enemies.gsc (IW5;
// D:\CoD\COD-GSC-Source\IW5-GSC\scripts\1552.gsc): squad_setup reads the
// "leader" structs (script_noteworthy leader; the CoDOL survival addons author
// 16 on fav_tropical, each targeting its "follower" structs), spawn_far_squad
// picks the leader location farthest from the players, the followers stand at
// the leader's targets, and setup_leader / setup_follower keep the followers
// on their leader.  IW5 spawns through the authored spawner of the squad's
// classname; here the actor comes from _coopsurvival_ai::spawn_type.
//
// Names no multiplayer and no coop module.

// IW5 squad_setup( 1 ): the leader locations.  The CoDOL survival addons
// authored them as script_structs (script_noteworthy leader, each targeting
// its followers); maps/<level>_survival.gsc carries them as
// level.survival_leaders, each with .origin, .angles and .followers.
setup()
{
	level.survival_leaders_alive = [];
	level.survival_squad_locs = [];
	if ( isDefined( level.survival_leaders ) )
		level.survival_squad_locs = level.survival_leaders;
	for ( i = 0; i < level.survival_squad_locs.size; i++ )
	{
		s = level.survival_squad_locs[ i ];
		if ( !isDefined( s.angles ) )
			s.angles = ( 0, 0, 0 );
		if ( !isDefined( s.followers ) )
			s.followers = [];
		s.survival_followers = s.followers;
	}
	codolprintln( "CoDOL survival: " + level.survival_squad_locs.size + " squad spawn locations" );
}

// IW5 spawn_far_squad's pick: repeatedly drop the location closest to each
// player (and each living leader) until one is left.  The last few locations
// stand farthest from everyone.
farthest_location()
{
	locs = level.survival_squad_locs;
	if ( !locs.size )
		return undefined;
	avoid = maps\mp\gametypes\_survival_utility::living_players();
	for ( i = 0; i < level.survival_leaders_alive.size; i++ )
	{
		if ( isDefined( level.survival_leaders_alive[ i ] ) && isAlive( level.survival_leaders_alive[ i ] ) )
			avoid[ avoid.size ] = level.survival_leaders_alive[ i ];
	}
	if ( !avoid.size )
		return locs[ randomInt( locs.size ) ];
	pool = [];
	for ( i = 0; i < locs.size; i++ )
		pool[ i ] = locs[ i ];
	while ( pool.size > 1 )
	{
		for ( a = 0; a < avoid.size && pool.size > 1; a++ )
		{
			closest = 0;
			best = distanceSquared( pool[ 0 ].origin, avoid[ a ].origin );
			for ( i = 1; i < pool.size; i++ )
			{
				d = distanceSquared( pool[ i ].origin, avoid[ a ].origin );
				if ( d < best )
				{
					best = d;
					closest = i;
				}
			}
			rest = [];
			for ( i = 0; i < pool.size; i++ )
			{
				if ( i != closest )
					rest[ rest.size ] = pool[ i ];
			}
			pool = rest;
		}
	}
	return pool[ 0 ];
}

// IW5 get_furthest_from_these( locations, players, count ): the location
// closest to each player in turn is dropped until `count` are left; one of
// those at random, another than the last one when there is a choice.
far_location( count )
{
	pool = [];
	for ( i = 0; i < level.survival_squad_locs.size; i++ )
		pool[ i ] = level.survival_squad_locs[ i ];
	players = maps\mp\gametypes\_survival_utility::living_players();
	if ( !players.size || pool.size <= count )
		return farthest_location();
	while ( pool.size > count )
	{
		for ( a = 0; a < players.size && pool.size > count; a++ )
		{
			closest = 0;
			best = distanceSquared( pool[ 0 ].origin, players[ a ].origin );
			for ( i = 1; i < pool.size; i++ )
			{
				d = distanceSquared( pool[ i ].origin, players[ a ].origin );
				if ( d < best )
				{
					best = d;
					closest = i;
				}
			}
			rest = [];
			for ( i = 0; i < pool.size; i++ )
			{
				if ( i != closest )
					rest[ rest.size ] = pool[ i ];
			}
			pool = rest;
		}
	}
	pick = randomInt( pool.size );
	if ( isDefined( level.survival_last_far ) && pool[ pick ] == level.survival_last_far )
		pick = ( pick + 1 ) % pool.size;
	level.survival_last_far = pool[ pick ];
	return pool[ pick ];
}

// IW5 spawn_far_squad + spawn_enemy_group: a leader of AI type `ref` and up to
// size - 1 followers.  Returns the spawned actors.
spawn_squad( ref, size, scale )
{
	ais = [];
	loc = farthest_location();
	if ( !isDefined( loc ) )
	{
		codolprintln( "CoDOL survival: no squad location; the map's addon has no leader structs" );
		return ais;
	}
	leader = undefined;
	for ( tries = 0; !isDefined( leader ) && tries < 10; tries++ )
	{
		leader = maps\mp\gametypes\_survival_ai::spawn_type( ref, loc.origin, loc.angles, scale );
		if ( !isDefined( leader ) )
			wait( 1 );
	}
	if ( !isDefined( leader ) )
		return ais;
	ais[ 0 ] = leader;
	followers = loc.survival_followers;
	for ( i = 0; i < size - 1; i++ )
	{
		spot = loc;
		if ( followers.size )
			spot = followers[ i % followers.size ];
		ai = undefined;
		for ( tries = 0; !isDefined( ai ) && tries < 5; tries++ )
		{
			ai = maps\mp\gametypes\_survival_ai::spawn_type( ref, spot.origin, spot.angles, scale );
			if ( !isDefined( ai ) )
				wait( 1 );
		}
		if ( isDefined( ai ) )
			ais[ ais.size ] = ai;
		wait( 0.05 );
	}
	leader thread setup_leader( ais );
	for ( i = 1; i < ais.size; i++ )
		ais[ i ] thread setup_follower( leader );
	return ais;
}

living_leaders()
{
	out = [];
	for ( i = 0; i < level.survival_leaders_alive.size; i++ )
	{
		if ( isDefined( level.survival_leaders_alive[ i ] ) && isAlive( level.survival_leaders_alive[ i ] ) )
			out[ out.size ] = level.survival_leaders_alive[ i ];
	}
	level.survival_leaders_alive = out;
	return out;
}

// IW5 setup_leader: he hunts; when he dies the first living follower is
// promoted and the rest follow him.
setup_leader( members )
{
	self endon( "survival_demoted" );
	self.squadmembers = members;
	level.survival_leaders_alive[ level.survival_leaders_alive.size ] = self;
	self thread maps\mp\gametypes\_survival_ai::hunt();
	self waittill( "death" );
	living_leaders();
	next = undefined;
	rest = [];
	for ( i = 0; i < members.size; i++ )
	{
		m = members[ i ];
		if ( !isDefined( m ) || !isAlive( m ) || m == self )
			continue;
		if ( !isDefined( next ) )
			next = m;
		rest[ rest.size ] = m;
	}
	if ( !isDefined( next ) )
		return;
	next notify( "survival_promoted" );
	next thread setup_leader( rest );
	for ( i = 0; i < rest.size; i++ )
	{
		if ( rest[ i ] != next )
			rest[ i ] thread setup_follower( next );
	}
}

// Follow a living leader into the fight. Do not stop another 600 units
// behind him merely because one squad member can see a player: native
// soldier combat independently retains each actor's real firing range.
setup_follower( leader )
{
	self endon( "death" );
	self endon( "survival_promoted" );
	self notify( "survival_hunt" );
	// The chase watchdog (_survival_ai::chase_watch) sends a follower whose
	// squad made no progress hunting on his own: hunt() notifies this.
	self endon( "survival_hunt" );
	self.survival_hunting = undefined;
	self.survival_leader = leader;
	for ( ;; )
	{
		if ( level.survival_over )
			return;
		if ( isDefined( level.survival_aggressive ) && level.survival_aggressive )
			return;
		if ( !isDefined( leader ) || !isAlive( leader ) )
			return;
		if ( !isDefined( self.survival_dash ) )
		{
			self.goalradius = 96;
			self setGoalPos( leader.origin );
		}
		wait( 0.75 );
	}
}
