// maps/mp/survival/_generic -- Spec Ops Survival's data for a level without
// authored placements.
//
// maps\mp\gametypes\survival::onStartGameType calls data() when
// maps\mp\gametypes\_survival_levels has no maps\mp\survival\<level>.gsc for
// the running level.  It fills the same level fields an authored level file
// does (level.survival_player_starts, _leaders with .followers,
// _kiosk_spots, _claymore_spots, _carepackage_spots, _heli_nodes), derived
// from what every MP level carries:
//   * the actors' navigation graph (the level's node_* records or its bot
//     waypoint CSV, bot_nav.c; codol_getWalkableWaypoints, findpath,
//     codol_botNavPointNear) -- a level whose actors cannot route between its
//     spawn areas is refused;
//   * the MP spawn points (every gametype's classname): the players start at
//     the best-connected one, the squads come from the others the actors can
//     route to the players from;
//   * the attack helicopter's path (heli_loop_start / heli_start / heli_leave
//     script_origins of maps\mp\killstreaks\_helicopter): the chopper boss
//     flies the loop, the juggernaut drop comes in on a start path, unloads
//     over open ground near the players and leaves.
// Kiosks, claymore and care package spots are walkable waypoints around the
// players' start.  The waves are the shared tier table
// (maps\mp\gametypes\_survival::wave_table_for_map).
//
// Survival is listed for a level by its coop_survival_<level> row in
// mp/comaplist.csv (zones/usermaps_patch), kept in a shipping build for the
// levels zonekit/zonelist.py names in survival_levels.

spot( origin, angles )
{
	s = spawnStruct();
	s.origin = origin;
	s.angles = angles;
	return s;
}

kiosk( kind, origin, angles )
{
	s = spot( origin, angles );
	s.kind = kind;
	return s;
}

// Every MP gametype's spawn classname: some levels carry only a few
// war spawns and keep the rest under sd / cha / dom names.
spawn_classnames()
{
	names = [];
	names[ names.size ] = "mp_tdm_spawn_allies_start";
	names[ names.size ] = "mp_tdm_spawn_axis_start";
	names[ names.size ] = "mp_tdm_spawn";
	names[ names.size ] = "mp_dm_spawn";
	names[ names.size ] = "mp_dom_spawn";
	names[ names.size ] = "mp_dom_spawn_allies_start";
	names[ names.size ] = "mp_dom_spawn_axis_start";
	names[ names.size ] = "mp_sd_spawn_attacker";
	names[ names.size ] = "mp_sd_spawn_defender";
	names[ names.size ] = "mp_sab_spawn_allies";
	names[ names.size ] = "mp_sab_spawn_axis";
	names[ names.size ] = "mp_sab_spawn_allies_start";
	names[ names.size ] = "mp_sab_spawn_axis_start";
	names[ names.size ] = "mp_ctf_spawn_allies";
	names[ names.size ] = "mp_ctf_spawn_axis";
	names[ names.size ] = "mp_ctf_spawn_allies_start";
	names[ names.size ] = "mp_ctf_spawn_axis_start";
	names[ names.size ] = "mp_dd_spawn_attacker";
	names[ names.size ] = "mp_dd_spawn_defender";
	names[ names.size ] = "mp_dd_spawn_attacker_start";
	names[ names.size ] = "mp_dd_spawn_defender_start";
	names[ names.size ] = "mp_cha_spawn_allies";
	names[ names.size ] = "mp_cha_spawn_axis";
	names[ names.size ] = "mp_cha_spawn_allies_start";
	names[ names.size ] = "mp_cha_spawn_axis_start";
	return names;
}

near_any( list, origin, radius )
{
	r2 = radius * radius;
	for ( i = 0; i < list.size; i++ )
	{
		if ( distanceSquared( list[ i ].origin, origin ) < r2 )
			return true;
	}
	return false;
}

// The spawn points, one per 96 units, that stand on the actors' graph (a
// walkable waypoint within 256: placeholder spawns parked off the level drop
// out).  .start marks the allies' war starts.
collect_spawns()
{
	out = [];
	names = spawn_classnames();
	for ( n = 0; n < names.size; n++ )
	{
		ents = getEntArray( names[ n ], "classname" );
		for ( i = 0; i < ents.size; i++ )
		{
			e = ents[ i ];
			if ( !isDefined( e.origin ) || near_any( out, e.origin, 96 ) )
				continue;
			if ( !isDefined( codol_botNavPointNear( e.origin, 256 ) ) )
				continue;
			angles = ( 0, 0, 0 );
			if ( isDefined( e.angles ) )
				angles = e.angles;
			s = spot( e.origin, angles );
			s.start = ( names[ n ] == "mp_tdm_spawn_allies_start" );
			out[ out.size ] = s;
		}
	}
	return out;
}

// The spawns an actor at each one can route to `home` from.
reachable_from( spawns, home )
{
	out = [];
	for ( i = 0; i < spawns.size; i++ )
	{
		s = spawns[ i ];
		if ( s == home || findpath( s.origin, home.origin ) )
			out[ out.size ] = s;
	}
	return out;
}

// The players' start: of the allies' war starts and a spread of the other
// spawns, the one the most spawns can route to (the main part of the graph).
pick_home( spawns )
{
	cands = [];
	for ( i = 0; i < spawns.size && cands.size < 6; i++ )
	{
		if ( spawns[ i ].start )
			cands[ cands.size ] = spawns[ i ];
	}
	step = int( spawns.size / 8 );
	if ( step < 1 )
		step = 1;
	for ( i = 0; i < spawns.size && cands.size < 14; i += step )
	{
		if ( !spawns[ i ].start )
			cands[ cands.size ] = spawns[ i ];
	}
	best = undefined;
	bestScore = -1;
	for ( c = 0; c < cands.size; c++ )
	{
		score = reachable_from( spawns, cands[ c ] ).size * 2;
		if ( cands[ c ].start )
			score++;
		if ( score > bestScore )
		{
			best = cands[ c ];
			bestScore = score;
		}
	}
	return best;
}

center_of( list )
{
	sum = ( 0, 0, 0 );
	for ( i = 0; i < list.size; i++ )
		sum = sum + list[ i ].origin;
	if ( !list.size )
		return sum;
	return ( sum[ 0 ] / list.size, sum[ 1 ] / list.size, sum[ 2 ] / list.size );
}

facing( from, to )
{
	a = vectorToAngles( to - from );
	return ( 0, a[ 1 ], 0 );
}

// Up to `count` walkable waypoints between minRadius and maxRadius of
// `anchor` on its part of the graph, `spacing` apart and clear of `avoid`.
nav_points( anchor, minRadius, maxRadius, count, spacing, avoid )
{
	out = [];
	for ( tries = 0; tries < count * 6 && out.size < count; tries++ )
	{
		p = codol_botNavPointNear( anchor, maxRadius, minRadius );
		if ( !isDefined( p ) )
			continue;
		if ( near_any( out, p, spacing ) )
			continue;
		if ( isDefined( avoid ) && near_any( avoid, p, 64 ) )
			continue;
		out[ out.size ] = spot( p, facing( p, anchor ) );
	}
	return out;
}

data()
{
	map = getDvar( "mapname" );
	nav = codol_getWalkableWaypoints();
	if ( nav.size < 64 )
	{
		codolprintln( "CoDOL survival: generic " + map + " refused: " + nav.size + " walkable waypoints (no AI navigation)" );
		return false;
	}
	spawns = collect_spawns();
	if ( spawns.size < 2 )
	{
		codolprintln( "CoDOL survival: generic " + map + " refused: " + spawns.size + " spawn points on the AI graph" );
		return false;
	}
	home = pick_home( spawns );
	reach = reachable_from( spawns, home );
	if ( reach.size < 4 )
	{
		codolprintln( "CoDOL survival: generic " + map + " refused: the actors reach the start at " + home.origin +
			" from " + ( reach.size - 1 ) + " of " + ( spawns.size - 1 ) + " spawn points" );
		return false;
	}
	center = center_of( reach );

	// The players' starts: the home spawn and the reachable spawns around it,
	// topped up with waypoints beside it.
	level.survival_player_starts = [];
	level.survival_player_starts[ 0 ] = spot( home.origin, facing( home.origin, center ) );
	for ( i = 0; i < reach.size && level.survival_player_starts.size < 6; i++ )
	{
		s = reach[ i ];
		if ( s != home && distance( s.origin, home.origin ) < 700 )
			level.survival_player_starts[ level.survival_player_starts.size ] = spot( s.origin, facing( s.origin, center ) );
	}
	extra = nav_points( home.origin, 48, 320, 6 - level.survival_player_starts.size, 64, level.survival_player_starts );
	for ( i = 0; i < extra.size; i++ )
		level.survival_player_starts[ level.survival_player_starts.size ] = spot( extra[ i ].origin, facing( extra[ i ].origin, center ) );

	// The squads: IW5 _squad_enemies picks the leader location farthest from
	// the players; every reachable spawn away from the start is one, with
	// waypoints out on the map when the level has few spawns.
	leaders = [];
	for ( i = 0; i < reach.size; i++ )
	{
		if ( distance( reach[ i ].origin, home.origin ) >= 800 )
			leaders[ leaders.size ] = reach[ i ];
	}
	if ( leaders.size < 8 )
	{
		far = nav_points( home.origin, 1000, 4000, 16 - leaders.size, 400, leaders );
		for ( i = 0; i < far.size; i++ )
		{
			if ( findpath( far[ i ].origin, home.origin ) )
				leaders[ leaders.size ] = far[ i ];
		}
	}
	if ( leaders.size < 4 )
	{
		for ( i = 0; i < reach.size; i++ )
		{
			if ( reach[ i ] != home && !near_any( leaders, reach[ i ].origin, 64 ) )
				leaders[ leaders.size ] = reach[ i ];
		}
	}
	level.survival_leaders = [];
	for ( i = 0; i < leaders.size; i++ )
	{
		l = spot( leaders[ i ].origin, facing( leaders[ i ].origin, home.origin ) );
		l.followers = nav_points( l.origin, 40, 192, 3, 40, undefined );
		level.survival_leaders[ level.survival_leaders.size ] = l;
	}

	// The two CoDOL kiosks (the locker: ammo, armor and perks; the support
	// crate: the bought support) near the start.
	level.survival_kiosk_spots = [];
	near = nav_points( home.origin, 140, 480, 2, 160, level.survival_player_starts );
	if ( near.size < 2 )
		near = nav_points( home.origin, 64, 900, 2, 96, undefined );
	if ( near.size > 0 )
		level.survival_kiosk_spots[ 0 ] = kiosk( "kiosk_locker", near[ 0 ].origin, facing( near[ 0 ].origin, home.origin ) );
	if ( near.size > 1 )
		level.survival_kiosk_spots[ 1 ] = kiosk( "kiosk_killstreak", near[ 1 ].origin, facing( near[ 1 ].origin, home.origin ) );

	// The claymore planter's spots: on the approaches, clear of the start.
	level.survival_claymore_spots = nav_points( home.origin, 350, 2200, 32, 160, undefined );
	level.survival_carepackage_spots = nav_points( home.origin, 256, 1200, 6, 256, undefined );

	heli_nodes( home );

	codolprintln( "CoDOL survival: generic " + map + ": " + nav.size + " walkable waypoints; " + reach.size + " of " +
		spawns.size + " spawn points reach the start at " + home.origin + "; " + level.survival_player_starts.size +
		" player starts, " + level.survival_leaders.size + " squad locations, " + level.survival_kiosk_spots.size +
		" kiosks, " + level.survival_claymore_spots.size + " claymore spots, " + level.survival_heli_boss_count +
		" chopper boss nodes, " + level.survival_heli_drop_count + " drop paths" );
	return true;
}

// --------------------------------------------------------------------------
// Helicopter paths from the level's attack helicopter path
// --------------------------------------------------------------------------

first_ent( name )
{
	ents = getEntArray( name, "targetname" );
	if ( ents.size )
		return ents[ 0 ];
	return undefined;
}

// A target chain of script_origins from `start` (not including it), at
// most `max` long, stopping where it closes on itself.
chain_from( start, max )
{
	out = [];
	e = start;
	for ( i = 0; i < max && isDefined( e.target ); i++ )
	{
		e = first_ent( e.target );
		if ( !isDefined( e ) || e == start )
			break;
		out[ out.size ] = e;
	}
	return out;
}

heli_node( origin, angles )
{
	s = spot( origin, angles );
	level.survival_heli_nodes[ level.survival_heli_nodes.size ] = s;
	return s;
}

heli_nodes( home )
{
	level.survival_heli_nodes = [];
	level.survival_heli_boss_count = 0;
	level.survival_heli_drop_count = 0;

	// The chopper boss network (IW5 so_chopper_boss_path_struct): the loop,
	// each node linked to both neighbours and across, a stop at every other
	// one to fire, starts at opposite sides.
	loopStart = first_ent( "heli_loop_start" );
	loop = [];
	if ( isDefined( loopStart ) )
	{
		loop[ 0 ] = loopStart;
		rest = chain_from( loopStart, 32 );
		for ( i = 0; i < rest.size; i++ )
			loop[ loop.size ] = rest[ i ];
	}
	if ( loop.size >= 3 )
	{
		n = loop.size;
		for ( i = 0; i < n; i++ )
		{
			angles = ( 0, 0, 0 );
			if ( isDefined( loop[ i ].angles ) )
				angles = loop[ i ].angles;
			s = heli_node( loop[ i ].origin, angles );
			s.script_noteworthy = "so_chopper_boss_path_struct";
			s.script_linkname = "k33_boss_" + i;
			s.script_linkto = "k33_boss_" + ( ( i + 1 ) % n ) + " k33_boss_" + ( ( i + n - 1 ) % n ) + " k33_boss_" + ( ( i + int( n / 2 ) ) % n );
			if ( i % 2 == 0 )
				s.script_stopnode = 1;
			if ( i == 0 || i == int( n / 2 ) )
				s.targetname = "chopper_boss_path_start";
		}
		level.survival_heli_boss_count = n;
	}

	// The juggernaut drops (IW5 drop_path_start chains): a start path in,
	// the unload over open walkable ground near the players at the path's
	// height, out to the nearest leave point.
	starts = getEntArray( "heli_start", "targetname" );
	leaves = getEntArray( "heli_leave", "targetname" );
	for ( k = 0; k < starts.size && k < 4; k++ )
	{
		path = chain_from( starts[ k ], 8 );
		last = starts[ k ];
		if ( path.size )
			last = path[ path.size - 1 ];
		height = last.origin[ 2 ];
		unload = undefined;
		for ( tries = 0; tries < 12 && !isDefined( unload ); tries++ )
		{
			p = codol_botNavPointNear( home.origin, 1400, 300 );
			if ( !isDefined( p ) || p[ 2 ] > height - 300 )
				continue;
			if ( bulletTracePassed( ( p[ 0 ], p[ 1 ], height ), p + ( 0, 0, 48 ), false, undefined ) )
				unload = p;
		}
		if ( !isDefined( unload ) )
			continue;
		prefix = "k33_drop_" + k + "_";
		s = heli_node( starts[ k ].origin, facing( starts[ k ].origin, unload ) );
		s.targetname = "drop_path_start";
		s.target = prefix + "0";
		idx = 0;
		for ( i = 0; i < path.size; i++ )
		{
			s = heli_node( path[ i ].origin, ( 0, 0, 0 ) );
			s.targetname = prefix + idx;
			idx++;
			s.target = prefix + idx;
		}
		s = heli_node( ( unload[ 0 ], unload[ 1 ], height ), facing( last.origin, unload ) );
		s.targetname = prefix + idx;
		s.script_unload = "default";
		if ( leaves.size )
		{
			out = leaves[ 0 ];
			for ( i = 1; i < leaves.size; i++ )
			{
				if ( distance2d( leaves[ i ].origin, unload ) < distance2d( out.origin, unload ) )
					out = leaves[ i ];
			}
			idx++;
			s.target = prefix + idx;
			s = heli_node( out.origin, ( 0, 0, 0 ) );
			s.targetname = prefix + idx;
		}
		level.survival_heli_drop_count++;
	}
}
