// zm points of interest: something that outranks flesh for a zombie.
//
// Port of T5's POI machinery, maps\_zombiemode_utility.gsc:
//   create_zombie_point_of_interest                 :287
//   create_zombie_point_of_interest_attractor_positions :333
//   generated_radius_attract_positions              :436
//   get_zombie_point_of_interest                    :523
//   activate / deactivate_zombie_point_of_interest  :631 / :641
//   remove_poi_attractor / add_poi_attractor        :683 / :702
//   can_attract / update_poi_on_death               :801 / :825
//   invalidate_attractor_pos                        :855
//   add / remove_poi_to/from_ignore_list            :900
// and the consumer half of T5 maps\_zombiemode_spawner.gsc find_flesh :3984
// (get_zombie_point_of_interest each pass, enemyoverride outranks
// favoriteenemy) and zombie_follow_enemy :4305 (ignoreall while walking to
// the POI, face it on arrival, SetGoalPos( enemyoverride[0] )).
//
// Any item can use it (cymbal monkey, black hole bomb, and later anything
// else that lures): an ENTITY or a STRUCT with an .origin calls
// create_zombie_point_of_interest on itself.  Consumers:
//   _zombiemode_spawner::zombie_think asks zombie_follow_poi() once per chase
//   pass; true means the POI owns this zombie's goal for that pass.
//
// DIFFERENCES FROM T5, each for a reason:
//   * The registry is level.zombie_pois, not getEntArray( "zombie_poi",
//     "script_noteworthy" ): a struct can be a POI, and a freed entity drops
//     out of the array by itself (isDefined false) without a classname scan.
//   * An attractor position is a spawnStruct { origin, poi } rather than
//     T5's two-element array.  T5 compares those arrays with ==
//     (claimed_attractor_positions, invalidate_attractor_pos); two structs
//     compare by identity here, which is what T5 means.
//   * position[1] is always the POI.  T5's attract_to_origin arm returns the
//     ZOMBIE in [1] (:602), which zombie_follow_enemy then faces.
//   * server_safe_ground_trace is T5's throttled groundpos; this throttles
//     by waiting a frame every 8 traces.
//   * T5 re-threads initial/arrival_attract_func on every find_flesh pass and
//     relies on "zombie_acquire_enemy" to end the previous arrival thread.
//     This server's chase loop has no such notify, so each is threaded once
//     when a zombie's POI changes, and "zombie_poi_changed" ends them.
//   * The native mover here does not reliably notify "goal" (see
//     _zombiemode_spawner wait_until_at_spot), so zombie_follow_poi and the
//     items' arrival funcs test arrival by distance.
//   * T5's check_point_in_active_zone has no counterpart (no zone system in
//     this port); a POI with no traceable ring positions is walked to at its
//     own ground point instead.

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	if ( !isDefined( level.zombie_pois ) )
		level.zombie_pois = [];
	level.zombie_poi_trace_count = 0;
}

// --------------------------------------------------------------------------
// Producer side.  self = the POI (entity or struct).
// --------------------------------------------------------------------------

// T5 :287.  attract_dist undefined = no radius (attracts every zombie).
create_zombie_point_of_interest( attract_dist, num_attractors, added_poi_value, start_turned_on, initial_attract_func, arrival_attract_func )
{
	if ( !isDefined( level.zombie_pois ) )
		level.zombie_pois = [];

	if ( !isDefined( added_poi_value ) )
		added_poi_value = 0;
	if ( !isDefined( start_turned_on ) )
		start_turned_on = true;

	self.added_poi_value = added_poi_value;
	self.is_zombie_poi = true;
	self.poi_active = start_turned_on;

	if ( isDefined( attract_dist ) )
		self.poi_radius = attract_dist * attract_dist;
	else
		self.poi_radius = undefined;

	self.num_poi_attracts = num_attractors;
	self.attract_to_origin = true;
	self.attractor_array = [];
	self.claimed_attractor_positions = [];
	self.initial_attract_func = initial_attract_func;
	self.arrival_attract_func = arrival_attract_func;

	compact_pois();
	level.zombie_pois[ level.zombie_pois.size ] = self;
}

// T5 :631.
activate_zombie_point_of_interest()
{
	if ( !is_true( self.is_zombie_poi ) )
		return;
	self.poi_active = true;
	level notify( "zombie_poi_changed" );
}

// T5 :641.  Every attractor is released (T5's "kill_poi" ends their death
// watchers) and the claims are cleared.
deactivate_zombie_point_of_interest()
{
	if ( !is_true( self.is_zombie_poi ) )
		return;

	if ( isDefined( self.attractor_array ) )
	{
		attractors = self.attractor_array;
		for ( i = 0; i < attractors.size; i++ )
		{
			if ( isDefined( attractors[ i ] ) )
				attractors[ i ] notify( "kill_poi" );
		}
	}

	self.attractor_array = [];
	self.claimed_attractor_positions = [];
	self.poi_active = false;
	level notify( "zombie_poi_changed" );
}

// Not in T5 (its POIs die with their entity).  Called before an item deletes
// or abandons a POI so no zombie keeps walking at it for another pass.
remove_zombie_point_of_interest()
{
	self deactivate_zombie_point_of_interest();
	self.is_zombie_poi = undefined;
	compact_pois();
}

compact_pois()
{
	if ( !isDefined( level.zombie_pois ) )
	{
		level.zombie_pois = [];
		return;
	}

	live = [];
	for ( i = 0; i < level.zombie_pois.size; i++ )
	{
		p = level.zombie_pois[ i ];
		if ( !isDefined( p ) )
			continue;
		if ( !is_true( p.is_zombie_poi ) )
			continue;
		live[ live.size ] = p;
	}
	level.zombie_pois = live;
}

// T5 :333.  Rings of ground positions around the POI, so a crowd surrounds
// it instead of stacking on one point.  Notifies "attractor_positions_generated"
// on the POI and the level when done.
create_zombie_point_of_interest_attractor_positions( num_attract_dists, diff_per_dist, attractor_width )
{
	if ( !isDefined( self.num_poi_attracts ) || !is_true( self.is_zombie_poi ) )
		return;

	if ( !isDefined( num_attract_dists ) )
		num_attract_dists = 4;
	if ( !isDefined( diff_per_dist ) )
		diff_per_dist = 45;
	if ( !isDefined( attractor_width ) )
		attractor_width = 45;

	origin = self.origin;
	forward = ( 0, 1, 0 );

	self.num_attract_dists = num_attract_dists;

	last_index = [];
	attract_dists = [];
	max_positions = [];
	for ( i = 0; i < num_attract_dists; i++ )
	{
		last_index[ i ] = -1;
		attract_dists[ i ] = diff_per_dist * ( i + 1 );
		max_positions[ i ] = int( 3.14 * 2 * attract_dists[ i ] / attractor_width );
	}

	num_attracts_per_dist = self.num_poi_attracts / num_attract_dists;

	// T5 :393.  What each ring can hold after the bleed from closer rings.
	diff = 0;
	actual_num_positions = [];
	for ( i = 0; i < num_attract_dists; i++ )
	{
		if ( num_attracts_per_dist > ( max_positions[ i ] + diff ) )
		{
			actual_num_positions[ i ] = max_positions[ i ];
			diff += num_attracts_per_dist - max_positions[ i ];
		}
		else
		{
			actual_num_positions[ i ] = num_attracts_per_dist + diff;
			diff = 0;
		}
	}

	positions = [];
	failed = 0;
	angle_offset = 0;
	prev_last_index = -1;
	for ( j = 0; j < num_attract_dists; j++ )
	{
		if ( ( actual_num_positions[ j ] + failed ) < max_positions[ j ] )
		{
			actual_num_positions[ j ] += failed;
			failed = 0;
		}
		else if ( actual_num_positions[ j ] < max_positions[ j ] )
		{
			actual_num_positions[ j ] = max_positions[ j ];
			failed = max_positions[ j ] - actual_num_positions[ j ];
		}

		ring = generated_radius_attract_positions( origin, forward, angle_offset, actual_num_positions[ j ], attract_dists[ j ] );
		for ( k = 0; k < ring.size; k++ )
		{
			pos = spawnStruct();
			pos.origin = ring[ k ];
			positions[ positions.size ] = pos;
		}
		failed += int( actual_num_positions[ j ] ) - ring.size;
		angle_offset += 15;
		last_index[ j ] = int( actual_num_positions[ j ] - failed + prev_last_index );
		prev_last_index = last_index[ j ];

		// The ring traces waited; the POI may be gone (the grenade exploded).
		if ( !isDefined( self ) || !is_true( self.is_zombie_poi ) )
			return;
	}

	for ( k = 0; k < positions.size; k++ )
		positions[ k ].poi = self;

	self.attract_dists = attract_dists;
	self.max_attractor_dist = attract_dists[ attract_dists.size - 1 ] * 1.1;
	self.last_index = last_index;
	self.attractor_positions = positions;
	self.attract_to_origin = false;

	self notify( "attractor_positions_generated" );
	level notify( "attractor_positions_generated" );
}

// T5 :436.  Returns the ground points of one ring; a point more than 60 units
// above or below the POI (a ledge, a wall top, the far side of a floor) is
// dropped, as in T5.
generated_radius_attract_positions( origin, forward, offset, num_positions, attract_radius )
{
	result = [];
	if ( num_positions < 1 )
		return result;

	degs_per_pos = 360 / num_positions;
	for ( i = offset; i < 360 + offset; i += degs_per_pos )
	{
		altforward = vector_scale( forward, attract_radius );
		rotated_forward = ( ( cos( i ) * altforward[ 0 ] - sin( i ) * altforward[ 1 ] ), ( sin( i ) * altforward[ 0 ] + cos( i ) * altforward[ 1 ] ), altforward[ 2 ] );

		pos = poi_ground_trace( origin + rotated_forward + ( 0, 0, 100 ) );
		if ( !isDefined( pos ) )
			continue;

		if ( abs_val( pos[ 2 ] - origin[ 2 ] ) < 60 )
			result[ result.size ] = pos;
	}
	return result;
}

// T5 maps\_zombiemode_server_throttle::server_safe_ground_trace: a ground
// trace that yields a frame every few calls so one monkey cannot spend a
// frame on 96 traces.
poi_ground_trace( pos )
{
	level.zombie_poi_trace_count++;
	if ( level.zombie_poi_trace_count >= 8 )
	{
		level.zombie_poi_trace_count = 0;
		wait( 0.05 );
	}
	return groundpos( pos );
}

abs_val( v )
{
	if ( v < 0 )
		return 0 - v;
	return v;
}

// --------------------------------------------------------------------------
// Consumer side.  self = the zombie.
// --------------------------------------------------------------------------

// T5 :523.  The best live POI within its radius that can still take this
// zombie, as a position struct { origin, poi }, or undefined.
get_zombie_point_of_interest( origin )
{
	if ( is_true( self.ignore_all_poi ) )
		return undefined;
	if ( !isDefined( level.zombie_pois ) || level.zombie_pois.size == 0 )
		return undefined;

	pois = level.zombie_pois;
	best_poi = undefined;
	best_dist = 10000 * 10000;

	for ( i = 0; i < pois.size; i++ )
	{
		p = pois[ i ];
		if ( !isDefined( p ) || !is_true( p.is_zombie_poi ) || !is_true( p.poi_active ) )
			continue;
		if ( self is_ignoring_poi( p ) )
			continue;

		dist = distanceSquared( origin, p.origin ) - p.added_poi_value;

		if ( isDefined( p.poi_radius ) && dist >= p.poi_radius )
			continue;
		if ( dist >= best_dist )
			continue;
		if ( !p can_attract( self ) )
			continue;

		best_poi = p;
		best_dist = dist;
	}

	if ( !isDefined( best_poi ) )
		return undefined;

	// T5 :595-:612.  The black hole traces fresh ground under itself
	// (_new_ground_trace); a POI still in flight is walked to directly
	// (attract_to_origin); otherwise the zombie claims a ring position.
	if ( is_true( best_poi._new_ground_trace ) || is_true( best_poi.attract_to_origin ) )
	{
		position = spawnStruct();
		position.origin = groundpos( best_poi.origin + ( 0, 0, 100 ) );
		// T5 traces from 100 above.  Under a low ceiling that start is inside
		// the ceiling and the "ground" comes back above the POI (MEASURED on
		// zm_prototype_sh: a monkey at z 24 gave a goal at z 138); then the
		// trace is taken from the POI itself.
		if ( position.origin[ 2 ] > best_poi.origin[ 2 ] + 60 )
			position.origin = groundpos( best_poi.origin );
		position.poi = best_poi;
		return position;
	}

	return self add_poi_attractor( best_poi );
}

is_ignoring_poi( poi )
{
	if ( !isDefined( self.ignore_poi ) )
		return false;
	for ( j = 0; j < self.ignore_poi.size; j++ )
	{
		if ( isDefined( self.ignore_poi[ j ] ) && self.ignore_poi[ j ] == poi )
			return true;
	}
	return false;
}

// T5 :801.
can_attract( attractor )
{
	if ( !isDefined( self.attractor_array ) )
		self.attractor_array = [];

	if ( array_contains( self.attractor_array, attractor ) )
		return true;

	if ( isDefined( self.num_poi_attracts ) && self.attractor_array.size >= self.num_poi_attracts )
		return false;

	return true;
}

// T5 :702.  Claims the closest free position in the innermost ring that
// still has room, or returns the one this zombie already holds.
add_poi_attractor( zombie_poi )
{
	if ( !isDefined( zombie_poi ) )
		return undefined;
	if ( !isDefined( zombie_poi.attractor_array ) )
		zombie_poi.attractor_array = [];
	if ( !isDefined( zombie_poi.claimed_attractor_positions ) )
		zombie_poi.claimed_attractor_positions = [];

	held = self.zombie_poi_claim;
	if ( isDefined( held ) && isDefined( held.poi ) && held.poi == zombie_poi && array_contains( zombie_poi.attractor_array, self ) )
		return held;

	if ( !isDefined( zombie_poi.attractor_positions ) || zombie_poi.attractor_positions.size <= 0 )
		return undefined;
	if ( !isDefined( zombie_poi.last_index ) )
		return undefined;

	start = -1;
	end = -1;
	last_index = -1;
	for ( i = 0; i < zombie_poi.last_index.size; i++ )
	{
		if ( zombie_poi.claimed_attractor_positions.size < zombie_poi.last_index[ i ] )
		{
			start = last_index + 1;
			end = zombie_poi.last_index[ i ];
			break;
		}
		last_index = zombie_poi.last_index[ i ];
	}

	if ( start < 0 )
		start = 0;
	if ( end < 0 )
		return undefined;
	if ( end >= zombie_poi.attractor_positions.size )
		end = zombie_poi.attractor_positions.size - 1;

	best_dist = 10000 * 10000;
	best_pos = undefined;
	for ( i = int( start ); i <= int( end ); i++ )
	{
		pos = zombie_poi.attractor_positions[ i ];
		if ( !isDefined( pos ) )
			continue;
		if ( array_contains( zombie_poi.claimed_attractor_positions, pos ) )
			continue;
		dist = distanceSquared( pos.origin, self.origin );
		if ( !isDefined( best_pos ) || dist < best_dist )
		{
			best_dist = dist;
			best_pos = pos;
		}
	}

	if ( !isDefined( best_pos ) )
		return undefined;

	zombie_poi.attractor_array[ zombie_poi.attractor_array.size ] = self;
	zombie_poi.claimed_attractor_positions[ zombie_poi.claimed_attractor_positions.size ] = best_pos;
	self.zombie_poi_claim = best_pos;
	self thread update_poi_on_death( zombie_poi );

	return best_pos;
}

// T5 :683.  Drops this zombie and its claim from the POI.
remove_poi_attractor( zombie_poi )
{
	if ( !isDefined( zombie_poi ) || !isDefined( zombie_poi.attractor_array ) )
		return;

	attractors = [];
	for ( i = 0; i < zombie_poi.attractor_array.size; i++ )
	{
		a = zombie_poi.attractor_array[ i ];
		if ( isDefined( a ) && a != self )
			attractors[ attractors.size ] = a;
	}
	zombie_poi.attractor_array = attractors;

	claim = self.zombie_poi_claim;
	if ( isDefined( claim ) && isDefined( zombie_poi.claimed_attractor_positions ) )
		zombie_poi.claimed_attractor_positions = array_without( zombie_poi.claimed_attractor_positions, claim );
	self.zombie_poi_claim = undefined;
}

// T5 :825.  A dead zombie hands its position back.  zombie_poi is held by
// this thread's frame; it is checked before use because it may have been
// deleted in the meantime.
update_poi_on_death( zombie_poi )
{
	self endon( "kill_poi" );

	claim = self.zombie_poi_claim;
	self waittill( "death" );

	if ( !isDefined( zombie_poi ) || !isDefined( zombie_poi.attractor_array ) )
		return;

	attractors = [];
	for ( i = 0; i < zombie_poi.attractor_array.size; i++ )
	{
		a = zombie_poi.attractor_array[ i ];
		if ( isDefined( a ) && isAlive( a ) )
			attractors[ attractors.size ] = a;
	}
	zombie_poi.attractor_array = attractors;

	if ( isDefined( claim ) && isDefined( zombie_poi.claimed_attractor_positions ) )
		zombie_poi.claimed_attractor_positions = array_without( zombie_poi.claimed_attractor_positions, claim );
}

// T5 :855, as zombie_pathing :4102 uses it: the claimed position could not
// be pathed to, so it is struck from the POI for everyone, and the zombie
// picks again next pass.  A POI whose ORIGIN cannot be reached (nothing
// left to invalidate) goes on the zombie's ignore list after the second
// failure so it stops walking into the same wall.
invalidate_attractor_pos( position )
{
	if ( !isDefined( position ) )
		return;
	poi = position.poi;
	if ( !isDefined( poi ) )
		return;

	if ( isDefined( poi.attractor_positions ) && array_contains( poi.attractor_positions, position ) )
	{
		index = 0;
		for ( i = 0; i < poi.attractor_positions.size; i++ )
		{
			if ( poi.attractor_positions[ i ] == position )
				index = i;
		}
		if ( isDefined( poi.last_index ) )
		{
			for ( i = 0; i < poi.last_index.size; i++ )
			{
				if ( index <= poi.last_index[ i ] )
					poi.last_index[ i ] = poi.last_index[ i ] - 1;
			}
		}
		poi.attractor_positions = array_without( poi.attractor_positions, position );
		self remove_poi_attractor( poi );
		return;
	}

	if ( !isDefined( self.poi_bad_path_count ) )
		self.poi_bad_path_count = 0;
	self.poi_bad_path_count++;
	if ( self.poi_bad_path_count >= 2 )
	{
		self.poi_bad_path_count = 0;
		self add_poi_to_ignore_list( poi );
		codolprintln( "CoDOL zm poi: ent " + self getEntityNumber() + " cannot path to the poi at " + poi.origin + "; ignoring it" );
	}
}

// T5 :900.
add_poi_to_ignore_list( poi )
{
	if ( !isDefined( self.ignore_poi ) )
		self.ignore_poi = [];
	if ( !array_contains( self.ignore_poi, poi ) )
		self.ignore_poi[ self.ignore_poi.size ] = poi;
}

remove_poi_from_ignore_list( poi )
{
	if ( isDefined( self.ignore_poi ) )
		self.ignore_poi = array_without( self.ignore_poi, poi );
}

// --------------------------------------------------------------------------
// The chase hook.  self = a zombie in _zombiemode_spawner::zombie_think's
// chase loop.  Returns true when a POI owns the goal for this pass.
//
// T5 zombie_follow_enemy :4305: while a zombie walks to a POI it is
// ignoreall (on this server that is also what stops the native chase from
// re-targeting the goal at a player every frame -- see tear_into_building),
// and its goal is the claimed position.  When the POI goes away the zombie
// is handed back to flesh with ignoreall restored, but only if this module
// was the one that raised it.
// --------------------------------------------------------------------------

zombie_follow_poi()
{
	position = self get_zombie_point_of_interest( self.origin );

	if ( !isDefined( position ) || !isDefined( position.poi ) || !isDefined( position.origin ) )
	{
		self zombie_release_poi();
		return false;
	}

	poi = position.poi;
	current = self.zombie_poi_current;
	if ( !isDefined( current ) || current != poi )
	{
		self notify( "zombie_poi_changed" );
		self.zombie_poi_current = poi;
		self.zombie_poi_arrived = false;
		if ( !isDefined( level.zombie_poi_retargets ) )
			level.zombie_poi_retargets = 0;
		level.zombie_poi_retargets++;
		codolprintln( "CoDOL zm poi: ent " + self getEntityNumber() + " leaves flesh for the poi at " + poi.origin + " (goal " + position.origin + ", " + int( distance( self.origin, poi.origin ) ) + " units away)" );

		if ( isDefined( poi.initial_attract_func ) )
			self thread [[ poi.initial_attract_func ]]( poi );
		if ( isDefined( poi.arrival_attract_func ) )
			self thread [[ poi.arrival_attract_func ]]( poi );

		self thread poi_bad_path_watch( poi );
	}

	if ( !is_true( self.ignoreall ) )
	{
		self.ignoreall = true;
		self.zombie_poi_raised_ignoreall = true;
	}

	if ( !isDefined( self.zombie_poi_goal ) || distanceSquared( self.zombie_poi_goal, position.origin ) > 1 )
	{
		if ( !isDefined( self.zombie_poi_saved_goalradius ) && isDefined( self.goalradius ) )
			self.zombie_poi_saved_goalradius = self.goalradius;
		self.zombie_poi_goal = position.origin;
		self.goalradius = 32;
		self setGoalPos( position.origin );
	}

	// T5 :4307-:4314: face motion on the way, face the POI once there.
	if ( distanceSquared( self.origin, position.origin ) < 48 * 48 )
	{
		if ( !is_true( self.zombie_poi_arrived ) )
		{
			self.zombie_poi_arrived = true;
			self orientMode( "face point", poi.origin );
		}
	}

	return true;
}

// Hands the zombie back to flesh.  Safe to call every pass.
zombie_release_poi()
{
	if ( isDefined( self.zombie_poi_current ) || isDefined( self.zombie_poi_goal ) )
	{
		self notify( "zombie_poi_changed" );
		codolprintln( "CoDOL zm poi: ent " + self getEntityNumber() + " returns to flesh" );
	}

	old = self.zombie_poi_current;
	if ( isDefined( old ) )
		self remove_poi_attractor( old );

	self.zombie_poi_current = undefined;
	self.zombie_poi_goal = undefined;
	self.zombie_poi_claim = undefined;

	if ( isDefined( self.zombie_poi_saved_goalradius ) )
	{
		self.goalradius = self.zombie_poi_saved_goalradius;
		self.zombie_poi_saved_goalradius = undefined;
	}

	if ( is_true( self.zombie_poi_arrived ) )
	{
		self.zombie_poi_arrived = false;
		self orientMode( "face default" );
	}

	if ( is_true( self.zombie_poi_raised_ignoreall ) )
	{
		self.zombie_poi_raised_ignoreall = undefined;
		self.ignoreall = false;
	}
}

// T5 zombie_pathing :4097: "bad_path" while walking to a POI strikes the
// claimed position (or, walking to the POI's origin, counts toward ignoring
// it).  Ends when the zombie's POI changes.
poi_bad_path_watch( poi )
{
	self endon( "death" );
	self endon( "zombie_poi_changed" );

	for ( ;; )
	{
		self waittill( "bad_path" );
		if ( !isDefined( poi ) )
			return;

		position = self.zombie_poi_claim;
		if ( !isDefined( position ) )
		{
			position = spawnStruct();
			position.poi = poi;
		}
		self invalidate_attractor_pos( position );
		self.zombie_poi_goal = undefined;
	}
}

// --------------------------------------------------------------------------
// Array helpers (T5 common_scripts\utility: array_check_for_dupes is the
// negation of array_contains; array_remove).
// --------------------------------------------------------------------------

array_contains( array, item )
{
	if ( !isDefined( array ) || !isDefined( item ) )
		return false;
	for ( i = 0; i < array.size; i++ )
	{
		if ( isDefined( array[ i ] ) && array[ i ] == item )
			return true;
	}
	return false;
}

array_without( array, item )
{
	result = [];
	if ( !isDefined( array ) )
		return result;
	for ( i = 0; i < array.size; i++ )
	{
		if ( !isDefined( array[ i ] ) )
			continue;
		if ( isDefined( item ) && array[ i ] == item )
			continue;
		result[ result.size ] = array[ i ];
	}
	return result;
}

// How many of a captured list of zombies are dead or freed now.
count_dead( list )
{
	n = 0;
	if ( !isDefined( list ) )
		return n;
	for ( i = 0; i < list.size; i++ )
	{
		if ( !isDefined( list[ i ] ) || !isAlive( list[ i ] ) )
			n++;
	}
	return n;
}

// Zombies within radius of origin, from the spawner's live list (no
// getaiarray on this server, see _zombiemode_utility get_enemy_count).
zombies_in_radius( origin, radius )
{
	result = [];
	if ( !isDefined( level.zombies ) )
		return result;
	r2 = radius * radius;
	zombies = level.zombies;
	for ( i = 0; i < zombies.size; i++ )
	{
		z = zombies[ i ];
		if ( !isDefined( z ) || !isAlive( z ) )
			continue;
		if ( distanceSquared( z.origin, origin ) <= r2 )
			result[ result.size ] = z;
	}
	return result;
}
