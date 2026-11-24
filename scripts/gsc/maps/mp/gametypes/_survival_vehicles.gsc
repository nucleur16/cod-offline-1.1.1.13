// survival: the enemy helicopters -- the chopper bosses and the juggernaut
// drop.
//
// Ported from Infinity Ward's Modern Warfare 3 Spec Ops Survival:
//   * spawn_chopper_boss / maps\_chopperboss chopper_boss_behavior_little_bird
//     (IW5 scripts\1564.gsc, 1561.gsc): the attack chopper comes in on the
//     level's so_chopper_boss_path_struct network, then hunts the players: it
//     hovers where it sees one, faces him, fires and moves on.  IW5 chose among
//     the linked nodes; here the air around the player is searched too (the
//     chopper bosses, below).  The missile chopper fires rockets as well.  IW5
//     chopper_boss_sentient makes it a priority target (threatbias 10000); here
//     it is in level.helis on the axis team, so the multiplayer SAM turret,
//     stinger lock-on and EMP find it.
//   * spawn_juggernaut / drop_jug_by_chopper (1564.gsc): the Mi-17 flies a
//     drop_path_start chain to the node with script_unload, the juggernaut
//     gets out under it, and the chopper flies on and leaves.  IW5 fast-ropes
//     him down; this server's actor has no rope animation, so he is set on
//     the ground under the hover.
// The helicopters are this server's ownerless spawnHelicopter (the native
// helicopter path, G_SpawnHelicopter) on the multiplayer vehicle definitions
// the multiplayer killstreaks use (cobra_mp for both attack choppers,
// pavelow_mp for the drop), with the models the survival zone and common_mp
// carry.  C1's minimap icon comes from the definition, so the little bird
// showed the attack helicopter's: it asks for littlebird_mp's instead.  It
// stays on cobra_mp for the turret, which reaches 20 to 85 degrees down
// (littlebird_mp's stops at 30: between buildings a spot that low sees a
// player on the street half as often as one at 40 to 60).  The path data is the level's
// maps\mp\survival\<level>.gsc (level.survival_heli_nodes).

precache()
{
	precacheVehicle( "cobra_mp" );
	precacheVehicle( "littlebird_mp" );
	precacheVehicle( "pavelow_mp" );

	level.survival_chopper_model = pick_model( "vehicle_little_bird_armed", "vehicle_little_bird_armed" );
	level.survival_missile_chopper_model = pick_model( "vehicle_mi24p_hind_desert", "vehicle_mi24p_hind_mp" );
	level.survival_drop_chopper_model = pick_model( "vehicle_mi17_woodland_fly_cheap", "vehicle_pavelow" );

	level.survival_chopper_gun = "cobra_20mm_mp";
	level.survival_chopper_rocket = "rpg_mp";
	if ( codol_isWeaponMounted( level.survival_chopper_gun ) )
		precacheItem( level.survival_chopper_gun );
	if ( codol_isWeaponMounted( level.survival_chopper_rocket ) )
		precacheItem( level.survival_chopper_rocket );
	level._effect[ "survival_chopper_explode" ] = loadFx( "explosions/tanker_explosion" );
	level._effect[ "survival_drop_smoke" ] = loadFx( "smoke/signal_smoke_airdrop" );
}

// The later build's model when the survival zone carries it, else the
// multiplayer one.
pick_model( preferred, fallback )
{
	if ( codescripts\character::modelMounted( preferred ) )
	{
		precacheModel( preferred );
		return preferred;
	}
	precacheModel( fallback );
	return fallback;
}

// IW5 chopper_boss_locs_populate / survival_drop_chopper_init over the
// level's helicopter structs.
init()
{
	level.survival_choppers = [];
	level.survival_boss_nodes = [];
	level.survival_drop_starts = [];
	if ( !isDefined( level.helis ) )
		level.helis = [];
	nodes = [];
	if ( isDefined( level.survival_heli_nodes ) )
		nodes = level.survival_heli_nodes;
	byLink = [];
	byName = [];
	for ( i = 0; i < nodes.size; i++ )
	{
		n = nodes[ i ];
		if ( isDefined( n.script_linkname ) )
			byLink[ n.script_linkname ] = n;
		if ( isDefined( n.targetname ) && n.targetname != "drop_path_start" && n.targetname != "chopper_boss_path_start" )
			byName[ n.targetname ] = n;
		if ( isDefined( n.script_noteworthy ) && n.script_noteworthy == "so_chopper_boss_path_struct" )
			level.survival_boss_nodes[ level.survival_boss_nodes.size ] = n;
		if ( isDefined( n.targetname ) && n.targetname == "drop_path_start" )
			level.survival_drop_starts[ level.survival_drop_starts.size ] = n;
	}
	for ( i = 0; i < level.survival_boss_nodes.size; i++ )
	{
		n = level.survival_boss_nodes[ i ];
		n.links = [];
		if ( !isDefined( n.script_linkto ) )
			continue;
		names = strTok( n.script_linkto, " " );
		for ( j = 0; j < names.size; j++ )
		{
			if ( isDefined( byLink[ names[ j ] ] ) )
				n.links[ n.links.size ] = byLink[ names[ j ] ];
		}
	}
	for ( i = 0; i < nodes.size; i++ )
	{
		n = nodes[ i ];
		if ( isDefined( n.target ) && isDefined( byName[ n.target ] ) )
			n.next = byName[ n.target ];
	}
	codolprintln( "CoDOL survival: " + level.survival_boss_nodes.size + " chopper boss nodes, " +
		level.survival_drop_starts.size + " drop paths" );
}

living_choppers()
{
	n = 0;
	out = [];
	for ( i = 0; i < level.survival_choppers.size; i++ )
	{
		c = level.survival_choppers[ i ];
		if ( isDefined( c ) && !( isDefined( c.survival_dead ) && c.survival_dead ) )
			out[ out.size ] = c;
	}
	level.survival_choppers = out;
	return out.size;
}

// --------------------------------------------------------------------------
// Helicopters
// --------------------------------------------------------------------------

spawn_heli( origin, angles, vehicle, model, iconOf )
{
	heli = spawnHelicopter( undefined, origin, angles, vehicle, model );
	if ( !isDefined( heli ) )
		return undefined;
	heli.team = "axis";
	heli setVehicleTeam( "axis" );
	// The native ownerless factory has no player whose team could choose the
	// hostile icon. Resolve this actual VehicleDef's enemy material explicitly.
	if ( isDefined( iconOf ) )
		heli codol_vehicleSetCompassIcon( "enemy", iconOf );
	else
		heli codol_vehicleSetCompassIcon( "enemy" );
	// Reuse the original IW4-style vehicles-tree rotor loop (b625a14).
	heli common_scripts\_heli_rotors::spin_rotors();
	heli.survival_enemy = true;
	heli setMaxPitchRoll( 25, 35 );
	heli Vehicle_SetSpeed( 60, 25 );
	heli setYawSpeed( 90, 45, 45 );
	heli setHoverParams( 40, 20, 15 );
	return heli;
}

// Fly to `origin`; true when it got there before the timeout.
fly_to( origin, timeout, stop )
{
	self endon( "death" );
	self endon( "survival_heli_dead" );
	if ( !isDefined( stop ) )
		stop = 1;
	self setVehGoalPos( origin, stop );
	self.survival_at_goal = false;
	self thread goal_watch();
	for ( t = 0; t < timeout && !self.survival_at_goal; t += 0.25 )
		wait( 0.25 );
	return self.survival_at_goal;
}

goal_watch()
{
	self endon( "death" );
	self endon( "survival_heli_dead" );
	self notify( "survival_goal_watch" );
	self endon( "survival_goal_watch" );
	self waittill( "goal" );
	self.survival_at_goal = true;
}

// --------------------------------------------------------------------------
// The chopper bosses (AI table rows chopper / missile_chopper)
//
// A boss hunts one player at a time and hovers where it can shoot him:
//   * the target is the nearest standing player, a visible one before a
//     hidden one, the current one before an equal one, and another than the
//     other choppers' when there is a choice (hunt_target, before every move);
//   * a hover spot is an air point around him -- a ring point at the type's
//     fighting distance and height, or one of the level's own path nodes --
//     that sees him inside the type's turret arc, with clear air around it
//     and a clear way there, straight or over the roofs (find_spot).  The
//     path network alone was too thin for that: its nodes stand around the
//     map, and the boss flew from node to linked node whether they were near
//     him or not;
//   * at a spot it strafes on after a few seconds, and at once when it loses
//     him, when he leaves the range or when it is shot up.  A flight is
//     re-planned when he moves away from where it was planned for.
// The gun fires whenever the turret has a line to him, moving or not.
// --------------------------------------------------------------------------

chopper_kind( ref )
{
	k = spawnStruct();
	k.vehicle = "cobra_mp";
	k.pitch_min = 24;
	k.pitch_max = 66;
	if ( ref == "missile_chopper" )
	{
		k.model = level.survival_missile_chopper_model;
		k.best = 1250;
		k.near = 450;
		k.far = 2200;
		return k;
	}
	k.model = level.survival_chopper_model;
	// The little bird's own minimap icon; another model keeps the Cobra's.
	if ( isSubStr( k.model, "little_bird" ) )
		k.icon = "littlebird_mp";
	k.best = 850;
	k.near = 300;
	k.far = 1700;
	return k;
}

spawn_chopper_boss( ref, scale )
{
	if ( ref == "jug_drop_chopper" )
		return undefined;
	if ( !level.survival_boss_nodes.size )
	{
		codolprintln( "CoDOL survival: " + ref + " has no chopper boss path on " + getDvar( "mapname" ) );
		return undefined;
	}
	air_init();
	t = level.survival_ai[ ref ];
	start = boss_start();
	kind = chopper_kind( ref );
	// IW5 brings it in from far out along the path start's facing.
	entry = start.origin + ( 0, 0, 1500 );
	heli = spawn_heli( entry, start.angles, kind.vehicle, kind.model, kind.icon );
	if ( !isDefined( heli ) )
	{
		codolprintln( "CoDOL survival: " + ref + " could not be spawned" );
		return undefined;
	}
	if ( !isDefined( scale ) )
		scale = 1.0;
	health = 1100;
	xp = 600;
	if ( isDefined( t ) )
	{
		health = t.health;
		xp = t.xp;
	}
	heli.maxhealth = int( health * scale );
	heli.damageTaken = 0;
	heli.survival_ref = ref;
	heli.survival_kind = kind;
	heli.survival_xp = xp;
	heli.survival_missiles = ( ref == "missile_chopper" );
	heli.survival_shots = 0;
	heli.survival_rockets = 0;
	heli.survival_moves = 0;
	heli setCanDamage( true );
	heli.damageCallback = ::chopper_damage;
	level.helis[ heli getEntityNumber() ] = heli;
	level.survival_choppers[ level.survival_choppers.size ] = heli;
	heli thread chopper_hunt( start );
	heli thread chopper_attack();
	codolprintln( "CoDOL survival: boss " + ref + " spawned, health " + heli.maxhealth + " (" + kind.vehicle + ")" );
	return heli;
}

// A path start, not one much farther from the players than the nearest (some
// levels have one ten thousand units out).
boss_start()
{
	starts = [];
	near = undefined;
	for ( i = 0; i < level.survival_boss_nodes.size; i++ )
	{
		n = level.survival_boss_nodes[ i ];
		if ( !isDefined( n.targetname ) || n.targetname != "chopper_boss_path_start" || !n.links.size )
			continue;
		starts[ starts.size ] = n;
		d = maps\mp\gametypes\_survival_utility::nearest_player_distance( n.origin );
		if ( isDefined( d ) && ( !isDefined( near ) || d < near ) )
			near = d;
	}
	if ( !starts.size )
		return level.survival_boss_nodes[ randomInt( level.survival_boss_nodes.size ) ];
	pick = [];
	for ( i = 0; i < starts.size; i++ )
	{
		d = maps\mp\gametypes\_survival_utility::nearest_player_distance( starts[ i ].origin );
		if ( !isDefined( near ) || d <= near + 2500 )
			pick[ pick.size ] = starts[ i ];
	}
	return pick[ randomInt( pick.size ) ];
}

// The air the bosses may use: the level's path nodes (one per place: a
// generic level's loop names each several times), the box around them and the
// walkable ground, and the height that clears the roofs.
air_init()
{
	if ( isDefined( level.survival_air ) )
		return;
	air = spawnStruct();
	air.nodes = [];
	air.lo = undefined;
	for ( i = 0; i < level.survival_boss_nodes.size; i++ )
	{
		p = level.survival_boss_nodes[ i ].origin;
		known = false;
		for ( j = 0; j < air.nodes.size && !known; j++ )
			known = distanceSquared( p, air.nodes[ j ] ) < 128 * 128;
		if ( !known )
			air.nodes[ air.nodes.size ] = p;
		air.ceiling = heli_max( air.ceiling, p[ 2 ] );
	}
	if ( isDefined( level.survival_waypoints ) )
	{
		for ( i = 0; i < level.survival_waypoints.size; i++ )
			air box_add( level.survival_waypoints[ i ] );
	}
	if ( !isDefined( air.lo ) )
	{
		for ( i = 0; i < air.nodes.size; i++ )
			air box_add( air.nodes[ i ] );
	}
	level.survival_air = air;
	codolprintln( "CoDOL survival: chopper air " + air.nodes.size + " nodes, ceiling " + int( air.ceiling ) + ", box " + air.lo + " to " + air.hi );
}

heli_max( a, b )
{
	if ( !isDefined( a ) || b > a )
		return b;
	return a;
}

box_add( p )
{
	if ( !isDefined( self.lo ) )
	{
		self.lo = p;
		self.hi = p;
		return;
	}
	lo = self.lo;
	hi = self.hi;
	x0 = lo[ 0 ];
	y0 = lo[ 1 ];
	x1 = hi[ 0 ];
	y1 = hi[ 1 ];
	if ( p[ 0 ] < x0 )
		x0 = p[ 0 ];
	if ( p[ 1 ] < y0 )
		y0 = p[ 1 ];
	if ( p[ 0 ] > x1 )
		x1 = p[ 0 ];
	if ( p[ 1 ] > y1 )
		y1 = p[ 1 ];
	self.lo = ( x0, y0, 0 );
	self.hi = ( x1, y1, 0 );
}

in_air_box( p )
{
	air = level.survival_air;
	return p[ 0 ] >= air.lo[ 0 ] - 400 && p[ 0 ] <= air.hi[ 0 ] + 400 && p[ 1 ] >= air.lo[ 1 ] - 400 && p[ 1 ] <= air.hi[ 1 ] + 400;
}

// The gun sits below the hull: what it must see past.
gun_point( p )
{
	return p + ( 0, 0, -64 );
}

sees_from( p, target )
{
	return sightTracePassed( gun_point( p ), target getEye(), false, self );
}

// A straight way for the hull: the middle and the skids.
air_clear( a, b )
{
	return bulletTracePassed( a, b, false, self ) && bulletTracePassed( a + ( 0, 0, -120 ), b + ( 0, 0, -120 ), false, self );
}

// Room for the rotor and the skids.
air_room( p )
{
	if ( !bulletTracePassed( p + ( 0, 0, 60 ), p + ( 0, 0, -170 ), false, self ) )
		return false;
	return bulletTracePassed( p + ( 200, 0, 0 ), p - ( 200, 0, 0 ), false, self ) &&
	       bulletTracePassed( p + ( 0, 200, 0 ), p - ( 0, 200, 0 ), false, self );
}

// The way from `from` to `to`: straight, or up, over and down; undefined when
// neither is clear.
air_route( from, to )
{
	r = [];
	if ( air_clear( from, to ) )
	{
		r[ 0 ] = to;
		return r;
	}
	z = level.survival_air.ceiling;
	if ( from[ 2 ] > z )
		z = from[ 2 ];
	if ( to[ 2 ] > z )
		z = to[ 2 ];
	up = ( from[ 0 ], from[ 1 ], z );
	over = ( to[ 0 ], to[ 1 ], z );
	if ( z - from[ 2 ] > 64 )
	{
		if ( !bulletTracePassed( from, up, false, self ) )
			return undefined;
		r[ r.size ] = up;
	}
	if ( !air_clear( up, over ) )
		return undefined;
	if ( z - to[ 2 ] > 64 )
	{
		if ( !bulletTracePassed( over, to, false, self ) )
			return undefined;
		r[ r.size ] = over;
	}
	r[ r.size ] = to;
	return r;
}

// --------------------------------------------------------------------------
// Whom it hunts
// --------------------------------------------------------------------------

huntable( p )
{
	if ( !isDefined( p ) || !isAlive( p ) || p.sessionstate != "playing" )
		return false;
	if ( isDefined( p.laststand ) && p.laststand )
		return false;
	// still falling in (_survival_intro)
	return !( isDefined( p.survival_in_intro ) && p.survival_in_intro );
}

hunt_target()
{
	players = maps\mp\gametypes\_survival_utility::living_players();
	best = undefined;
	bestScore = 0;
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( !huntable( p ) )
			continue;
		score = distance2d( p.origin, self.origin );
		if ( !self sees_from( self.origin, p ) )
			score += 1200;
		if ( isDefined( self.survival_target ) && self.survival_target == p )
			score -= 400;
		for ( j = 0; j < level.survival_choppers.size; j++ )
		{
			c = level.survival_choppers[ j ];
			if ( isDefined( c ) && c != self && isDefined( c.survival_target ) && c.survival_target == p )
				score += 600;
		}
		if ( !isDefined( best ) || score < bestScore )
		{
			best = p;
			bestScore = score;
		}
	}
	return best;
}

// --------------------------------------------------------------------------
// Where it hovers
// --------------------------------------------------------------------------

// The candidate air points around `target`, best first; no traces.  Twelve
// bearings, each at a shallow and a steep angle down to him.
spot_candidates( target )
{
	k = self.survival_kind;
	// where he will be when it gets there
	base = target.origin + target getVelocity() * 0.8;
	phase = vectorToYaw( self.origin - base );
	out = [];
	for ( i = 0; i < 24; i++ )
	{
		// 25 degrees to either side of where it is, then round to behind him
		yaw = phase + ( 25 + 28 * int( i / 4 ) + randomInt( 20 ) ) * ( 1 - 2 * ( i % 2 ) );
		if ( int( i / 2 ) % 2 )
			pitch = randomFloatRange( 44, k.pitch_max - 2 );
		else
			pitch = randomFloatRange( k.pitch_min + 2, 44 );
		p = base + ( 0, 0, 110 ) + anglesToForward( ( 0 - pitch, yaw, 0 ) ) * ( k.best * randomFloatRange( 0.8, 1.25 ) );
		out = self spot_add( out, p, target, 0 );
	}
	nodes = level.survival_air.nodes;
	for ( i = 0; i < nodes.size; i++ )
		out = self spot_add( out, nodes[ i ], target, 100 );
	return out;
}

spot_add( list, p, target, handicap )
{
	k = self.survival_kind;
	d = distance2d( p, target.origin );
	if ( d < k.near || d > k.far || !in_air_box( p ) )
		return list;
	pitch = atan( ( p[ 2 ] - 64 - target.origin[ 2 ] - 50 ) / d );
	if ( pitch < k.pitch_min || pitch > k.pitch_max )
		return list;
	score = 1000 - 0.5 * abs( distance( p, target.origin ) - k.best ) - handicap + randomInt( 250 );
	if ( pitch > 50 )
		score -= ( pitch - 50 ) * 12;
	// a strafe, not a twitch
	step = distance2d( p, self.origin );
	if ( step < 250 )
		score -= 400;
	for ( j = 0; j < level.survival_choppers.size; j++ )
	{
		c = level.survival_choppers[ j ];
		if ( !isDefined( c ) || c == self )
			continue;
		if ( distance2d( c.origin, p ) < 500 || ( isDefined( c.survival_spot ) && distance2d( c.survival_spot, p ) < 500 ) )
			score -= 600;
	}
	for ( j = 0; isDefined( self.survival_bad_spots ) && j < self.survival_bad_spots.size; j++ )
	{
		if ( distanceSquared( self.survival_bad_spots[ j ], p ) < 300 * 300 )
			score -= 800;
	}
	s = spawnStruct();
	s.origin = p;
	s.score = score;
	// sorted, best first
	at = list.size;
	while ( at > 0 && list[ at - 1 ].score < score )
	{
		list[ at ] = list[ at - 1 ];
		at--;
	}
	list[ at ] = s;
	return list;
}

// A spot it can shoot `target` from: the best candidate that sees him, has
// room and can be flown to; else a place nearer to him, above the roofs.  One
// chopper at a time, a few traces a frame.
find_spot( target )
{
	while ( isDefined( level.survival_chopper_searching ) && level.survival_chopper_searching )
		wait( 0.05 );
	level.survival_chopper_searching = true;
	self thread search_release();
	out = undefined;
	list = self spot_candidates( target );
	tried = 0;
	for ( i = 0; i < list.size && tried < 40 && !isDefined( out ); i++ )
	{
		if ( !huntable( target ) )
			break;
		tried++;
		if ( tried % 4 == 0 )
			wait( 0.05 );
		p = list[ i ].origin;
		if ( !isDefined( target ) || !self sees_from( p, target ) || !self air_room( p ) )
			continue;
		route = self air_route( self.origin, p );
		if ( !isDefined( route ) )
			continue;
		out = spawnStruct();
		out.route = route;
		out.sees = true;
	}
	if ( !isDefined( out ) && huntable( target ) )
		out = self approach_spot( target );
	level.survival_chopper_searching = false;
	self notify( "survival_search_done" );
	return out;
}

search_release()
{
	self endon( "survival_search_done" );
	self waittill_any_death();
	level.survival_chopper_searching = false;
}

waittill_any_death()
{
	self endon( "survival_heli_dead" );
	self waittill( "death" );
}

// No spot sees him (he is inside, or under a roof): close in over him, high
// enough to clear what is there, and circle until one does.
approach_spot( target )
{
	k = self.survival_kind;
	z = level.survival_air.ceiling;
	want = target.origin[ 2 ] + k.best;
	if ( want < z )
		z = want;
	base = target.origin;
	yaw = vectorToYaw( self.origin - base );
	turn = 40 + randomInt( 50 );
	if ( randomInt( 2 ) )
		turn = 0 - turn;
	heights = [];
	heights[ 0 ] = z;
	heights[ 1 ] = level.survival_air.ceiling;
	for ( h = 0; h < heights.size; h++ )
	{
		wait( 0.05 );
		for ( i = 0; i < 4; i++ )
		{
			p = base + anglesToForward( ( 0, yaw + turn + 90 * i, 0 ) ) * ( k.near + 250 );
			p = ( p[ 0 ], p[ 1 ], heights[ h ] );
			if ( !in_air_box( p ) || !self air_room( p ) )
				continue;
			route = self air_route( self.origin, p );
			if ( !isDefined( route ) )
				continue;
			s = spawnStruct();
			s.route = route;
			s.sees = false;
			return s;
		}
	}
	return self node_spot( target );
}

// The level's node nearest to him that it can fly to.
node_spot( target )
{
	nodes = level.survival_air.nodes;
	best = undefined;
	bestDist = 0;
	for ( i = 0; i < nodes.size; i++ )
	{
		d = distance2d( nodes[ i ], target.origin );
		if ( distance2d( nodes[ i ], self.origin ) < 200 )
			d += 2000;
		if ( !isDefined( best ) || d < bestDist )
		{
			best = nodes[ i ];
			bestDist = d;
		}
	}
	if ( !isDefined( best ) )
		return undefined;
	s = spawnStruct();
	s.sees = false;
	s.route = self air_route( self.origin, best );
	if ( !isDefined( s.route ) )
	{
		// boxed in: the authored network is flyable from node to node
		s.route = [];
		s.route[ 0 ] = best;
	}
	return s;
}

// --------------------------------------------------------------------------
// The hunt
// --------------------------------------------------------------------------

chopper_hunt( start )
{
	self endon( "death" );
	self endon( "survival_heli_dead" );
	self setNearGoalNotifyDist( 160 );
	self.survival_bad_spots = [];
	d = maps\mp\gametypes\_survival_utility::nearest_player_distance( self.origin );
	if ( !isDefined( d ) || d < 4500 )
		self fly_to( start.origin, 8, 1 );
	for ( ;; )
	{
		target = self hunt_target();
		self.survival_target = target;
		if ( !isDefined( target ) )
		{
			// nobody standing: hold off
			self clearLookAtEnt();
			wait( 1 );
			continue;
		}
		self setLookAtEnt( target );
		spot = self find_spot( target );
		if ( !isDefined( spot ) || !huntable( target ) )
		{
			wait( 0.5 );
			continue;
		}
		self.survival_moves++;
		self fly_route( spot, target );
		self hold_spot( target );
	}
}

// Fly the route to a spot.  Ends early when the player moved away from where
// it was planned for, went down, or a blind approach found him.
fly_route( spot, target )
{
	planned = target.origin;
	last = spot.route.size - 1;
	self.survival_spot = spot.route[ last ];
	for ( i = 0; i <= last; i++ )
	{
		goal = spot.route[ i ];
		leg = distance( self.origin, goal );
		if ( leg > 3500 )
			self Vehicle_SetSpeed( 90, 40 );
		else if ( leg > 1400 )
			self Vehicle_SetSpeed( 60, 30 );
		else
			self Vehicle_SetSpeed( 42, 28 );
		self setVehGoalPos( goal, i == last );
		limit = leg / 350 + 4;
		still = 0;
		from = self.origin;
		for ( t = 0; t < limit; t += 0.25 )
		{
			wait( 0.25 );
			if ( distanceSquared( self.origin, goal ) < 160 * 160 )
				break;
			if ( !huntable( target ) )
				return;
			if ( t > 1 && distance2d( target.origin, planned ) > 450 )
				return;
			if ( !spot.sees && t > 1 && i == last && self can_fight( target ) )
				return;
			if ( distanceSquared( self.origin, from ) < 12 * 12 )
				still++;
			else
				still = 0;
			from = self.origin;
			if ( still >= 8 )
			{
				self bad_spot( goal );
				return;
			}
		}
	}
}

bad_spot( p )
{
	if ( self.survival_bad_spots.size >= 6 )
		self.survival_bad_spots = [];
	self.survival_bad_spots[ self.survival_bad_spots.size ] = p;
}

// It sees him from inside its fighting range.
can_fight( target )
{
	k = self.survival_kind;
	d = distance2d( self.origin, target.origin );
	if ( d < k.near * 0.5 || d > k.far )
		return false;
	return self sees_from( self.origin, target );
}

// Hover and shoot for a few seconds, then move on.
hold_spot( target )
{
	blind = 0;
	hurt = self.damageTaken;
	limit = 2 + randomFloat( 2 );
	for ( t = 0; t < limit; t += 0.25 )
	{
		if ( !huntable( target ) )
			return;
		if ( self can_fight( target ) )
			blind = 0;
		else
			blind++;
		if ( blind >= 3 || self.damageTaken - hurt > self.maxhealth * 0.12 )
			return;
		wait( 0.25 );
	}
	// another player may be nearer by now; hunt_target decides
}

// The gun (IW4 _helicopter attack_primary's burst) and the missile chopper's
// rockets at the player it hunts, whenever the turret has a line to him.
chopper_attack()
{
	self endon( "death" );
	self endon( "survival_heli_dead" );
	gun = codol_isWeaponMounted( level.survival_chopper_gun );
	rockets = self.survival_missiles && codol_isWeaponMounted( level.survival_chopper_rocket );
	for ( ;; )
	{
		wait( 0.5 );
		target = self.survival_target;
		if ( !huntable( target ) )
			continue;
		self setTurretTargetEnt( target, ( 0, 0, 40 ) );
		if ( !self has_shot( target ) )
			continue;
		if ( gun )
		{
			self setVehWeapon( level.survival_chopper_gun );
			wait( 0.5 );
			shots = 10 + randomInt( 10 );
			lost = 0;
			for ( i = 0; i < shots && huntable( target ) && lost < 2; i++ )
			{
				if ( i % 3 == 2 )
				{
					if ( self has_shot( target ) )
						lost = 0;
					else
						lost++;
				}
				self setTurretTargetEnt( target, ( randomFloatRange( -48, 48 ), randomFloatRange( -48, 48 ), 40 ) );
				self fireWeapon( "tag_flash" );
				self.survival_shots++;
				wait( 0.1 );
			}
		}
		if ( rockets && huntable( target ) && self has_shot( target ) )
		{
			// Fired by the chopper, not by the world (magicBullet): its
			// rockets passed for nobody's, so they did a player's full damage
			// and blew up the choppers they left from.
			self setVehWeapon( level.survival_chopper_rocket );
			wait( 0.2 );
			for ( i = 0; i < 2 && huntable( target ); i++ )
			{
				self setTurretTargetEnt( target, ( randomFloatRange( -64, 64 ), randomFloatRange( -64, 64 ), 0 ) );
				wait( 0.2 );
				if ( !self has_shot( target ) )
					break;
				self fireWeapon( "tag_flash" );
				self.survival_rockets++;
				wait( 0.2 );
			}
		}
		wait( 1.5 + randomFloat( 1.5 ) );
	}
}

has_shot( target )
{
	return self Vehicle_CanTurretTargetPoint( target.origin + ( 0, 0, 40 ), 1, target );
}

// CodeCallback_VehicleDamage for an enemy chopper: the players' damage counts
// against the AI table's health (the native helicopter keeps 99999).
chopper_damage( inflictor, attacker, damage, dFlags, meansOfDeath, weapon, point, dir, hitLoc, timeOffset, modelIndex, partName )
{
	if ( isDefined( self.survival_dead ) && self.survival_dead )
		return;
	if ( isDefined( attacker ) && !isPlayer( attacker ) && ( isAI( attacker ) || ( isDefined( attacker.survival_enemy ) && attacker.survival_enemy ) ) )
		return;
	if ( isDefined( attacker ) && attacker == self )
		return;
	// IW5's choppers take explosives harder (rockets, the SAM).
	if ( meansOfDeath == "MOD_PROJECTILE" || meansOfDeath == "MOD_PROJECTILE_SPLASH" || meansOfDeath == "MOD_EXPLOSIVE" )
		damage = damage * 2;
	self.damageTaken += damage;
	if ( isDefined( attacker ) && isPlayer( attacker ) )
		attacker thread maps\mp\gametypes\_damagefeedback::updateDamageFeedback( "standard" );
	self Vehicle_FinishDamage( inflictor, attacker, damage, dFlags, meansOfDeath, weapon, point, dir, hitLoc, timeOffset, modelIndex, partName );
	if ( self.damageTaken >= self.maxhealth )
		self thread chopper_die( attacker );
}

// The kill: credits and a kill to whoever brought it down (or owns what did),
// a spin and an explosion.
chopper_die( attacker )
{
	if ( isDefined( self.survival_dead ) && self.survival_dead )
		return;
	self.survival_dead = true;
	self notify( "survival_heli_dead" );
	self notify( "crashing" );
	level.helis[ self getEntityNumber() ] = undefined;
	level notify( "survival_axis_died" );
	killer = maps\mp\gametypes\_survival_ai::credit_owner( attacker );
	if ( isDefined( killer ) )
	{
		killer maps\mp\gametypes\_survival::add_credits( self.survival_xp, "chopper" );
		killer maps\mp\gametypes\_survival_ai::award_kill();
	}
	codolprintln( "CoDOL survival: " + self.survival_ref + " destroyed" );
	self clearTargetYaw();
	self setYawSpeed( 400, 100, 100 );
	self setTargetYaw( self.angles[ 1 ] + 170 );
	self Vehicle_SetSpeed( 25, 10 );
	self setVehGoalPos( self.origin + ( randomFloatRange( -300, 300 ), randomFloatRange( -300, 300 ), -400 ), 0 );
	wait( 2 );
	if ( isDefined( level._effect[ "survival_chopper_explode" ] ) )
		playFx( level._effect[ "survival_chopper_explode" ], self.origin );
	playSoundAtPosition( "cobra_helicopter_crash", self.origin );
	radiusDamage( self.origin, 256, 120, 20 );
	self delete();
}

// The survival EMP: every enemy chopper goes down.
emp_choppers( attacker )
{
	living_choppers();
	choppers = level.survival_choppers;
	for ( i = 0; i < choppers.size; i++ )
		choppers[ i ] thread chopper_die( attacker );
	return choppers.size;
}

// --------------------------------------------------------------------------
// The juggernaut drop
// --------------------------------------------------------------------------

// Launches one drop; false when the map has no drop path.  The boss is
// pending (level.survival_boss_pending) until he is on the ground.
drop_juggernaut( ref, scale )
{
	if ( !isDefined( level.survival_drop_starts ) || !level.survival_drop_starts.size )
		return false;
	start = level.survival_drop_starts[ randomInt( level.survival_drop_starts.size ) ];
	path = [];
	n = start;
	unload = undefined;
	for ( i = 0; isDefined( n ) && i < 32; i++ )
	{
		path[ path.size ] = n;
		if ( !isDefined( unload ) && isDefined( n.script_unload ) )
			unload = path.size - 1;
		n = n.next;
	}
	if ( !isDefined( unload ) )
		return false;
	heli = spawn_heli( start.origin, start.angles, "pavelow_mp", level.survival_drop_chopper_model );
	if ( !isDefined( heli ) )
		return false;
	// IW5 godon: the drop chopper cannot be shot down.
	heli setCanDamage( false );
	heli Vehicle_SetSpeed( 60 + randomInt( 15 ), 30 );
	level.survival_boss_pending++;
	heli thread drop_flight( path, unload, ref, scale );
	return true;
}

drop_flight( path, unload, ref, scale )
{
	ai = undefined;
	for ( i = 1; i < path.size; i++ )
	{
		stop = 0;
		if ( i == unload )
			stop = 1;
		self fly_to( path[ i ].origin, 25, stop );
		if ( i != unload )
			continue;
		// IW5 chopper_drop_smoke_at_unloading and the drop.
		ground = self drop_point( path[ i ].origin );
		if ( isDefined( level._effect[ "survival_drop_smoke" ] ) )
			playFx( level._effect[ "survival_drop_smoke" ], ground );
		wait( 2 );
		for ( tries = 0; !isDefined( ai ) && tries < 10 && !level.survival_over; tries++ )
		{
			ai = maps\mp\gametypes\_survival_ai::spawn_boss( ref, ground, path[ i ].angles, scale );
			if ( !isDefined( ai ) )
				wait( 1 );
		}
		if ( isDefined( ai ) )
			codolprintln( "CoDOL survival: boss " + ref + " dropped at " + ground );
		else
			codolprintln( "CoDOL survival: boss " + ref + " drop at " + ground + " failed" );
		level.survival_boss_pending--;
		self Vehicle_SetSpeed( 80, 40 );
	}
	// IW5 script_vehicle_selfremove: gone at the end of its path.
	self delete();
}

// The ground under `origin`, past any helicopter hovering below it: with two
// drops on one path the second juggernaut was set down on the first transport.
groundpos( origin, ignore )
{
	trace = bulletTrace( origin, origin + ( 0, 0, -10000 ), false, ignore );
	for ( i = 0; i < 6 && isDefined( trace[ "entity" ] ); i++ )
		trace = bulletTrace( trace[ "position" ] + ( 0, 0, -32 ), origin + ( 0, 0, -10000 ), false, trace[ "entity" ] );
	return trace[ "position" ];
}

// Where the juggernaut is set down: the ground under the hover, or the nearest
// walkable point to it when no player can be reached from there.
drop_point( origin )
{
	ground = groundpos( origin, self );
	target = maps\mp\gametypes\_survival_ai::closest_player( ground );
	if ( !isDefined( target ) || findPath( ground, target.origin ) )
		return ground;
	near = maps\mp\gametypes\_survival_ai::nearest_points( level.survival_waypoints, ground, 6, 0, 1200 );
	for ( i = 0; i < near.size; i++ )
	{
		if ( findPath( near[ i ], target.origin ) )
			return near[ i ];
	}
	return ground;
}
