// coop survival: squads of a leader and followers.
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
// Coop module; names no multiplayer module.

// IW5 squad_setup( 1 ): the leader structs, minus any "protector" ones.
setup()
{
	level.survival_leaders = [];
	level.survival_squad_locs = [];
	if ( !isDefined( level.struct ) )
		return;
	for ( i = 0; i < level.struct.size; i++ )
	{
		s = level.struct[ i ];
		if ( !isDefined( s.script_noteworthy ) || s.script_noteworthy != "leader" )
			continue;
		if ( isDefined( s.targetname ) && isSubStr( s.targetname, "protector" ) )
			continue;
		if ( isDefined( s.script_gameobjectname ) && s.script_gameobjectname == "survivalPlus" )
			continue;
		if ( !isDefined( s.angles ) )
			s.angles = ( 0, 0, 0 );
		s.survival_followers = followers_of( s );
		level.survival_squad_locs[ level.survival_squad_locs.size ] = s;
	}
	codolprintln( "CoDOL survival: " + level.survival_squad_locs.size + " squad spawn locations" );
}

followers_of( leader )
{
	out = [];
	if ( !isDefined( leader.target ) )
		return out;
	for ( i = 0; i < level.struct.size; i++ )
	{
		s = level.struct[ i ];
		if ( s == leader )
			continue;
		if ( isDefined( s.targetname ) && s.targetname == leader.target )
		{
			if ( !isDefined( s.angles ) )
				s.angles = leader.angles;
			out[ out.size ] = s;
		}
	}
	return out;
}

// IW5 spawn_far_squad's pick: repeatedly drop the location closest to each
// player (and each living leader) until one is left.  The last few locations
// stand farthest from everyone.
farthest_location()
{
	locs = level.survival_squad_locs;
	if ( !locs.size )
		return undefined;
	avoid = maps\_coopmission::living_players();
	for ( i = 0; i < level.survival_leaders.size; i++ )
	{
		if ( isDefined( level.survival_leaders[ i ] ) && isAlive( level.survival_leaders[ i ] ) )
			avoid[ avoid.size ] = level.survival_leaders[ i ];
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
		leader = maps\_coopsurvival_ai::spawn_type( ref, loc.origin, loc.angles, scale );
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
			ai = maps\_coopsurvival_ai::spawn_type( ref, spot.origin, spot.angles, scale );
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
	for ( i = 0; i < level.survival_leaders.size; i++ )
	{
		if ( isDefined( level.survival_leaders[ i ] ) && isAlive( level.survival_leaders[ i ] ) )
			out[ out.size ] = level.survival_leaders[ i ];
	}
	level.survival_leaders = out;
	return out;
}

// IW5 setup_leader: he hunts; when he dies the first living follower is
// promoted and the rest follow him.
setup_leader( members )
{
	self endon( "survival_demoted" );
	self.squadmembers = members;
	level.survival_leaders[ level.survival_leaders.size ] = self;
	self thread maps\_coopsurvival_ai::hunt();
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

// IW5 follow_leader_regular / setup_follower_advanced: stay near the leader
// (goal radius 200, IW5's follower radius), and spread to 600 once anyone in
// the squad has an enemy (IW5 enlarge_follower_goalradius_upon_seeing_player).
setup_follower( leader )
{
	self endon( "death" );
	self endon( "survival_promoted" );
	self notify( "survival_hunt" );
	self.survival_leader = leader;
	for ( ;; )
	{
		if ( level.coop_mission_over )
			return;
		if ( isDefined( level.survival_aggressive ) && level.survival_aggressive )
			return;
		if ( !isDefined( leader ) || !isAlive( leader ) )
			return;
		self.goalradius = 200;
		if ( isDefined( self.enemy ) || isDefined( leader.enemy ) )
			self.goalradius = 600;
		self setGoalPos( leader.origin );
		wait( 1.5 );
	}
}
