// teleporter_zm -- the "random teleporter", special of zm1 (lb).
//
// C1 IS THE SPECIFICATION.  SF_UI_PLACEABLE_TELEPORTER_ZM_DESC (GBK): "Tears
// space open into a rift; a player who goes through it is teleported at
// random to an area already explored."  (docs/CODOL_ZM_EQUIPMENT_EVIDENCE_20260924.md
// section 2.)  Weapon data: type/inventory/class 1/1/5, offhand class 2,
// world model t5_tp_bomb_world, damage 0 -- the engine throws it; the rift
// and the move are this module's.
//
// BEHAVIOUR REFERENCE, T5 (D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps):
//   _zombiemode_weap_black_hole_bomb.gsc:100-121  follow the thrown grenade
//       until its per-frame movement is zero, then open the device there;
//   :907 black_hole_bomb_teleport_init   a 64 x 70 volume at the rest point;
//   :952 black_hole_time_before_teleport  a player in it is sent to a random
//       destination that is in an ACTIVE zone and not the zone they are in
//       (:989-996); BulletTracePassed from the eye to the device first;
//       zmb_gersh_teleporter_out on the device (:1005);
//   :1020 black_hole_teleport / _zombiemode_weap_quantum_bomb.gsc:645
//       quantum_bomb_teleport  freeze, disable weapons, the 2D
//       zmb_gersh_teleporter_go_2d to the player, setOrigin +
//       setPlayerAngles, re-enable, player effect;
//   :1186 black_hole_bomb_create_exit_portal  zmb_gersh_teleporter_go at the
//       exit.
// C1 ships the matching assets in common_gametype_zm: maps/zombie/
// fx_portable_teleporter (the rift) and fx_portable_teleporter_player (on the
// player), and the three zmb_gersh_teleporter_* aliases.
//
// "AN AREA ALREADY EXPLORED".  T5 answers it with the zone manager
// (check_point_in_active_zone over struct_black_hole_teleport structs).
// Neither exists on CoDOL's zm maps: the zone manager is not ported
// (_zombiemode_spawner.gsc init_spawn_points header), no zm map authors
// struct_black_hole_teleport, and a zombie_door / zombie_debris trigger marks
// the doorway, not the room behind it.  So explored space is recorded
// directly: this module keeps level.zombie_explored, a set of standing
// positions -- the player spawn points (the start room, explored by
// definition) plus every spot a valid, grounded player has actually stood
// on, sampled once a second and kept only when it is at least
// EXPLORED_SPACING units from every recorded spot.  A room behind a door
// joins the set the moment somebody walks into it, which is exactly when T5
// would have activated its zone.  The destination is a random explored spot
// at least TELEPORT_MIN_DISTANCE from the rift (T5's "not your current
// zone"), and one positionWouldTelefrag() does not reject.
//
// CoDOL INTERPRETATION (C1 gives no numbers): the rift stays open
// RIFT_LIFETIME seconds and any number of players may use it; its volume is
// T5's 64 x 70.

#include maps\mp\gametypes\_zombiemode_utility;

TELEPORTER_WEAPON = "teleporter_zm";
RIFT_RADIUS = 64;
RIFT_HEIGHT = 70;
RIFT_LIFETIME = 20;
EXPLORED_SPACING = 256;
EXPLORED_MAX = 128;
TELEPORT_MIN_DISTANCE = 512;

init()
{
	level.zombie_explored = [];
	seed_explored_from_spawns();

	// The explored set is useful whether or not this map loads the device.
	level thread explored_tracker();

	if ( getWeaponIndexFromName( TELEPORTER_WEAPON ) <= 0 )
	{
		codolprintln( "CoDOL zm teleporter: " + TELEPORTER_WEAPON + " is not loaded on this map" );
		return;
	}

	level._effect[ "teleporter_rift" ] = loadfx( "maps/zombie/fx_portable_teleporter" );
	level._effect[ "teleporter_player" ] = loadfx( "maps/zombie/fx_portable_teleporter_player" );
	precacheModel( "t5_tp_bomb_world" );

	maps\mp\gametypes\_zombiemode_equipment::register_equipment( TELEPORTER_WEAPON, ::teleporter_fired );
	codolprintln( "CoDOL zm teleporter: ready, " + level.zombie_explored.size + " explored spots seeded from spawn points" );
}

// ---------------------------------------------------------------------------
// Explored space.
// ---------------------------------------------------------------------------

seed_explored_from_spawns()
{
	names = [];
	names[ 0 ] = "mp_tdm_spawn_allies_start";
	names[ 1 ] = "mp_dm_spawn";
	names[ 2 ] = "mp_tdm_spawn";
	names[ 3 ] = "info_player_start";

	for ( i = 0; i < names.size; i++ )
	{
		points = getEntArray( names[ i ], "classname" );
		for ( j = 0; j < points.size; j++ )
		{
			angles = ( 0, 0, 0 );
			if ( isDefined( points[ j ].angles ) )
				angles = points[ j ].angles;
			add_explored( points[ j ].origin, angles, "spawn" );
		}
	}
}

// Returns true when the spot was new.
add_explored( origin, angles, source )
{
	if ( level.zombie_explored.size >= EXPLORED_MAX )
		return false;

	min_dsq = EXPLORED_SPACING * EXPLORED_SPACING;
	for ( i = 0; i < level.zombie_explored.size; i++ )
	{
		if ( distanceSquared( level.zombie_explored[ i ].origin, origin ) < min_dsq )
			return false;
	}

	spot = spawnStruct();
	spot.origin = origin;
	spot.angles = ( 0, angles[ 1 ], 0 );
	level.zombie_explored[ level.zombie_explored.size ] = spot;
	codolprintln( "CoDOL zm teleporter: explored spot " + level.zombie_explored.size + " at " + origin + " (" + source + ")" );
	return true;
}

explored_tracker()
{
	level endon( "end_game" );

	for ( ;; )
	{
		wait( 1 );

		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( !is_player_valid( p ) )
				continue;
			if ( !p isOnGround() )
				continue;
			// A player the teleporter just moved stands on a spot that is
			// explored by construction; do not let the move itself count.
			if ( recently_teleported( p ) )
				continue;
			add_explored( p.origin, p.angles, p.name );
		}
	}
}

// A random explored spot at least TELEPORT_MIN_DISTANCE from `from`.
pick_destination( from )
{
	far = [];
	min_dsq = TELEPORT_MIN_DISTANCE * TELEPORT_MIN_DISTANCE;
	for ( i = 0; i < level.zombie_explored.size; i++ )
	{
		spot = level.zombie_explored[ i ];
		if ( distanceSquared( spot.origin, from ) < min_dsq )
			continue;
		far[ far.size ] = spot;
	}

	far = array_randomize( far );
	for ( i = 0; i < far.size; i++ )
	{
		if ( positionWouldTelefrag( far[ i ].origin ) )
			continue;
		return far[ i ];
	}
	return undefined;
}

// ---------------------------------------------------------------------------
// The device.
// ---------------------------------------------------------------------------

// self = the thrown grenade.  T5 black_hole_bomb :100-121: wait for the
// grenade to stop, then open the device where it lies.
teleporter_fired( player, weapon )
{
	grenade = self;
	last = grenade.origin;
	still = 0;
	for ( tick = 0; tick < 200; tick++ )
	{
		wait( 0.05 );
		if ( !isDefined( grenade ) )
			break;
		pos = grenade.origin;
		if ( distanceSquared( pos, last ) < 1 )
			still++;
		else
			still = 0;
		last = pos;
		if ( still >= 2 )
			break;
	}

	// The engine's own grenade (fuse, 0 damage) is not the rift; the rift
	// outlives it, standing on the device model.
	if ( isDefined( grenade ) )
		grenade hide();

	level thread rift_think( last, player );
}

rift_think( origin, owner )
{
	owner_name = "(gone)";
	if ( isDefined( owner ) )
		owner_name = owner.name;
	codolprintln( "CoDOL zm teleporter: " + owner_name + " opened a rift at " + origin + " for " + RIFT_LIFETIME + "s" );

	device = spawn( "script_model", origin );
	device setModel( "t5_tp_bomb_world" );

	fx = spawnFx( level._effect[ "teleporter_rift" ], origin + ( 0, 0, 8 ) );
	triggerFx( fx );

	uses = 0;
	elapsed = 0;
	while ( elapsed < RIFT_LIFETIME )
	{
		wait( 0.1 );
		elapsed += 0.1;

		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( !is_player_valid( p ) )
				continue;
			if ( recently_teleported( p ) )
				continue;
			if ( !in_rift( p, origin ) )
				continue;

			// T5 :955: nothing solid between the player's eye and the device.
			if ( !bulletTracePassed( p getEye(), origin + ( 0, 0, 16 ), false, p ) )
				continue;

			dest = pick_destination( origin );
			if ( !isDefined( dest ) )
			{
				if ( !is_true( p.teleporter_no_dest_logged ) )
				{
					p.teleporter_no_dest_logged = true;
					codolprintln( "CoDOL zm teleporter: " + p.name + " entered the rift, but no explored spot is " +
						TELEPORT_MIN_DISTANCE + "+ units away (" + level.zombie_explored.size + " explored)" );
				}
				continue;
			}

			uses++;
			device playSound( "zmb_gersh_teleporter_out" );
			p thread teleport_player( dest, origin );
		}
	}

	if ( isDefined( fx ) )
		fx delete();
	if ( isDefined( device ) )
		device delete();

	codolprintln( "CoDOL zm teleporter: rift at " + origin + " closed after " + uses + " use(s)" );
}

in_rift( p, origin )
{
	delta = p.origin - origin;
	if ( delta[ 2 ] < -16 || delta[ 2 ] > RIFT_HEIGHT )
		return false;
	return ( delta[ 0 ] * delta[ 0 ] + delta[ 1 ] * delta[ 1 ] ) <= RIFT_RADIUS * RIFT_RADIUS;
}

// self = player.  T5 quantum_bomb_teleport :645 / black_hole_teleport :1020.
teleport_player( dest, rift_origin )
{
	// A timestamp, not a flag: a flag set here would stick if this thread
	// ended early on death or disconnect.
	self.teleporter_moved_at = getTime();
	from = self.origin;

	playFx( level._effect[ "teleporter_player" ], from );

	self freezeControls( true );
	self disableOffhandWeapons();
	self disableWeapons();

	self playSoundToPlayer( "zmb_gersh_teleporter_go_2d", self );

	self setOrigin( dest.origin );
	self setPlayerAngles( dest.angles );

	self enableOffhandWeapons();
	self enableWeapons();
	self freezeControls( false );

	playFx( level._effect[ "teleporter_player" ], dest.origin );
	playSoundAtPosition( "zmb_gersh_teleporter_go", dest.origin );

	codolprintln( "CoDOL zm teleporter: " + self.name + " teleported from " + from + " to " + dest.origin +
		" (" + int( distance( from, dest.origin ) ) + " units, rift at " + rift_origin + ")" );
}

// self = player.  One second of grace after a move, so the rift a player
// lands on (if the destination is near another open rift) cannot bounce them
// straight back, and the arrival spot is not sampled as newly explored.
recently_teleported( p )
{
	if ( !isDefined( p.teleporter_moved_at ) )
		return false;
	return getTime() - p.teleporter_moved_at < 1000;
}
