// coop vehicle paths: script_model movers along the map's authored vehicle
// paths.
//
// The coop addons author IW4 SP vehicles (script_vehicle, script_vehicle_
// blackhawk, _ch46e, _littlebird_bench, _hummer_viewmodel, ...) whose
// `target` starts a path: info_vehicle_node chains for ground vehicles, and
// script_struct / script_origin chains for helicopters (MEASURED,
// docs/CODOL_CAMPAIGN_MODE.md section 8).  This server spawns no SP vehicle
// from a map entity, so a vehicle is shown as a script_model of its own
// `model` and moved node to node:
//
//   * speed: a node's `speed` (IW4 vehicle speeds are mph, 1 mph = 17.6
//     units/s) holds until a later node sets another;
//   * a node's script_flag_set is set when the vehicle reaches it, and a
//     node's script_flag (IW4: wait for this flag before continuing) is
//     honoured for at most script_flag_wait_max seconds, because the level
//     scripts that would set those flags do not ship;
//   * ground vehicles keep the authored height of the vehicle above its
//     first node; helicopter paths are the vehicle's own positions.
//
// Coop module (maps/, IW4 singleplayer namespace); names no multiplayer module.

mph_to_units()
{
	return 17.6;
}

// --------------------------------------------------------------------------
// Struct lookup (codescripts\struct puts every script_struct in level.struct).
// --------------------------------------------------------------------------

struct_by_targetname( name )
{
	if ( !isDefined( level.struct ) )
		return undefined;
	if ( !isDefined( level.coop_struct_index ) )
	{
		level.coop_struct_index = [];
		for ( i = 0; i < level.struct.size; i++ )
		{
			s = level.struct[ i ];
			if ( isDefined( s.targetname ) && !isDefined( level.coop_struct_index[ s.targetname ] ) )
				level.coop_struct_index[ s.targetname ] = s;
		}
	}
	return level.coop_struct_index[ name ];
}

// A path point by targetname: a vehicle node, a script_struct or an entity
// (script_origin).
path_point( name )
{
	if ( !isDefined( name ) || name == "" )
		return undefined;
	node = getVehicleNode( name, "targetname" );
	if ( isDefined( node ) )
		return node;
	s = struct_by_targetname( name );
	if ( isDefined( s ) )
		return s;
	ents = getEntArray( name, "targetname" );
	if ( ents.size > 0 )
		return ents[ 0 ];
	return undefined;
}

// The whole chain from a vehicle's (or node's) target, at most 64 points,
// stopping at a loop.
get_path( start )
{
	path = [];
	name = start;
	seen = [];
	while ( isDefined( name ) && path.size < 64 )
	{
		if ( isDefined( seen[ name ] ) )
			break;
		seen[ name ] = true;
		p = path_point( name );
		if ( !isDefined( p ) )
			break;
		path[ path.size ] = p;
		name = p.target;
	}
	return path;
}

is_ground_path( path )
{
	return path.size > 0 && isDefined( path[ 0 ].classname ) && isSubStr( path[ 0 ].classname, "info_vehicle_node" );
}

vehicle_model( vehicle )
{
	if ( isDefined( vehicle.model ) && vehicle.model != "" )
		return vehicle.model;
	return undefined;
}

// Spawn the mover for an authored vehicle.  The authored entity is hidden.
spawn_mover( vehicle )
{
	model = vehicle_model( vehicle );
	if ( !isDefined( model ) )
		return undefined;
	mover = spawn( "script_model", vehicle.origin );
	mover.angles = vehicle.angles;
	mover setModel( model );
	vehicle hide();
	mover.coop_vehicle = vehicle;
	// A helicopter's rotors turn from the moment it exists (IW4 _vehicle
	// build_drive); common_scripts\_heli_rotors picks the model's clip.
	mover common_scripts\_heli_rotors::spin_rotors();
	return mover;
}

// The ground under a point (world only), or the point itself when there is
// none within 512 units.  IW4 drives ground vehicles on the ground under their
// vehicle nodes, which sit 40-90 units above it on the training road
// (MEASURED: road z 0, nodes z 40-44, the authored humvee origin z 92); a
// script_model placed at the node height floats.
ground_under( point )
{
	trace = bulletTrace( point + ( 0, 0, 64 ), point - ( 0, 0, 512 ), false, undefined );
	if ( trace[ "fraction" ] < 1 )
		return trace[ "position" ];
	return point;
}

// Move the mover along the path.  `height` is added to every point; with
// `snapGround` each point is first dropped to the ground under it (ground
// vehicles); flags are handled as described above.
follow_path( path, height, defaultMph, snapGround )
{
	self endon( "death" );
	speed = defaultMph * mph_to_units();
	flagWaitMax = getDvarInt( "scr_coop_vehicle_flag_wait_max" );
	if ( flagWaitMax <= 0 )
		flagWaitMax = 4;

	for ( n = 0; n < path.size; n++ )
	{
		node = path[ n ];
		if ( isDefined( node.speed ) && node.speed > 0 )
			speed = node.speed * mph_to_units();

		dest = node.origin;
		if ( isDefined( snapGround ) && snapGround )
			dest = ground_under( dest );
		dest = dest + ( 0, 0, height );
		dist = distance( self.origin, dest );
		if ( dist >= 1 )
		{
			dir = vectorToAngles( dest - self.origin );
			self rotateTo( ( 0, dir[ 1 ], 0 ), 0.5 );
			time = dist / speed;
			if ( time < 0.1 )
				time = 0.1;
			self moveTo( dest, time, 0, 0 );
			wait( time );
		}

		if ( isDefined( node.script_flag_set ) )
			maps\_coopmission::flag_set( node.script_flag_set );
		if ( isDefined( node.script_flag ) && !maps\_coopmission::flag( node.script_flag ) )
		{
			waited = 0;
			while ( waited < flagWaitMax && !maps\_coopmission::flag( node.script_flag ) )
			{
				wait( 0.5 );
				waited += 0.5;
			}
		}
		self notify( "coop_path_node", node );
	}
	self notify( "coop_path_end" );
}
