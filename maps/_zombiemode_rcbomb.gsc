// zm RC car: the ride the kis_rcbomb_zm power-up hands out.
//
// C1 catalogue row 55 rcbomb_zm names rcbomb_mp, the multiplayer RC-XD, and
// common_gametype_zm ships its whole kit (MEASURED, docs/zone_assets):
// vehicle rc_car_medium_mp (ALL_vehicle.txt), models t5_veh_rcbomb_allies /
// _axis (ALL_xmodel.txt), weapons rcbomb_mp / rcbomb_zm (ALL_weapon.txt:794-795),
// the explosion maps/mp_maps/fx_mp_exp_rc_bomb, the rcbomb lights and every
// fx_treadfx_rcbomb_* surface effect (ALL_fx.txt), and the mpl_veh_rc_*
// engine sounds (ALL_sound.txt).  T5 zombies has no RC module; the behaviour
// is T5's multiplayer RC-XD (T5-STEAM-GSC maps/mp/_rcbomb.gsc), reached
// through the same native vehicle path the multiplayer port uses.
//
// WHY NOT maps\mp\killstreaks\_rcbomb.  It includes maps\mp\_utility and
// common_scripts\utility and threads _rcbomb_rules / _rcbomb_watchers /
// _killstreaks / _hostmigration: the multiplayer chain zm's _callbacksetup.gsc
// keeps out of a zm compile.  What IS reused is
// maps\mp\killstreaks\_rcbomb_presentation, which names no other module
// (MEASURED: no #include and no :: reference in it) and is therefore safe to
// compile under zm; it plays T5 _rcbomb.csc's light, blink and tread FX.
//
// NATIVE PATH, unchanged from multiplayer: spawnVehicle builds the native
// rc_car_medium_mp (src/codol/vehicle), useVehicle( player, 0 ) hands the
// player the remote driver seat (codol_remote_vehicle.cpp
// CODOL_Scr_UseVehicle, which Scr_Errors -- i.e. shuts the server down -- on
// an occupied seat or a missing tag_player, so both are checked first here),
// and the player's unlink() releases it with exit_vehicle.
//
// TAKEN from the multiplayer port (T5 maps/mp/_rcbomb.gsc), by function:
//   spawnRCBomb          -- model, targetname "rcbomb", owner, team.
//   useRCBomb            -- the 0.9 s antenna pull, then control.
//   carDetonateWaiter    -- the attack button detonates.
//   exitCarWaiter        -- leaving the car ends the ride.
//   carTimer             -- 20 s, countdown flag, 6 s, timeout flag, 4 s, boom.
//   blowup               -- 256 radius, 350..25 damage, physics, quake, FX.
//   calculateSpawnOrigin / testWheelLocations / testSpawnOrigin -- T5's own
//                           placement, with the native trace adapters.
// NOT TAKEN: _rcbomb_rules (the one-per-team killstreak cap: zombies has no
// enemy team; one car per player is enforced instead), the watcher /
// scrambler / flash stun (no zm equipment jams it), spawn influencers (zm
// spawns zombies from fixed structs, not the influencer system), the
// killstreak award / dialog / fade-to-black presentation, and the
// friendly-fire override (zombies do not damage the car).
//
// C1 INTERPRETATION: the grabber's body stays where it stood and stays
// damageable, as the multiplayer owner's does; a down (player_downed) or a
// disconnect detonates the car.  The kill is credited to the driver: blowup's
// radiusDamage names the player as attacker, which _zombiemode_spawner's
// zombie_death_event pays.

#include maps\_zombiemode_utility;

init()
{
	level.zm_rcbomb_ok = false;

	precacheModel( "t5_veh_rcbomb_allies" );
	precacheVehicle( "rc_car_medium_mp" );

	// maps\mp\killstreaks\_rcbomb.gsc init, the effects its presentation
	// module plays.  Every one ships in common_gametype_zm.
	load_tread_fx( "dust" );
	load_tread_fx( "concrete" );
	load_tread_fx( "snow" );
	level._effect[ "rcbomb_blinky" ]   = loadfx( "vehicle/light/fx_rcbomb_blinky_light" );
	level._effect[ "rcbomb_solid" ]    = loadfx( "vehicle/light/fx_rcbomb_solid_light" );
	level._effect[ "rcbomb_green" ]    = loadfx( "vehicle/light/fx_rcbomb_light_green_os" );
	level._effect[ "rcbomb_red" ]      = loadfx( "vehicle/light/fx_rcbomb_light_red_os" );
	level._effect[ "rcbombexplosion" ] = loadfx( "maps/mp_maps/fx_mp_exp_rc_bomb" );

	// T5 _clientflags.gsc, the values _rcbomb_presentation::monitor reads.
	level.const_flag_countdown = 1;
	level.const_flag_timeout = 2;
	level.const_flag_stunned = 9;

	level.zm_rcbomb_ok = true;
}

load_tread_fx( type )
{
	level._effect[ "rcbomb_tread_" + type + "_drift" ]        = loadfx( "vehicle/treadfx/fx_treadfx_rcbomb_" + type + "_drift" );
	level._effect[ "rcbomb_tread_" + type + "_peel" ]         = loadfx( "vehicle/treadfx/fx_treadfx_rcbomb_" + type + "_peel" );
	level._effect[ "rcbomb_tread_" + type + "_first_person" ] = loadfx( "vehicle/treadfx/fx_treadfx_rcbomb_" + type + "_first_person" );
	level._effect[ "rcbomb_tread_" + type + "_reverse" ]      = loadfx( "vehicle/treadfx/fx_treadfx_rcbomb_" + type + "_reverse" );
	level._effect[ "rcbomb_tread_" + type + "_trail" ]        = loadfx( "vehicle/treadfx/fx_treadfx_rcbomb_" + type + "_trail" );
	level._effect[ "rcbomb_tread_" + type + "_slow" ]         = loadfx( "vehicle/treadfx/fx_treadfx_rcbomb_" + type + "_slow" );
}

// A player can take the car only while standing, up and not already driving.
can_use_rcbomb( player )
{
	if ( !is_true( level.zm_rcbomb_ok ) )
		return false;
	if ( !is_player_valid( player ) )
		return false;
	if ( isDefined( player.zm_rcbomb ) )
		return false;
	if ( player isRemoteControlling() )
		return false;
	return true;
}

// The power-up grab.  self is the grabber.
rcbomb_powerup()
{
	self endon( "disconnect" );

	if ( !can_use_rcbomb( self ) )
	{
		codolPrintLn( "CoDOL zm rcbomb: " + self.name + " cannot take a car now" );
		return;
	}

	placement = self calculate_spawn_origin( self.origin, self.angles );
	if ( !isDefined( placement ) )
	{
		// T5 refuses with KILLSTREAK_RCBOMB_NOT_PLACABLE; a power-up has no
		// reward to hand back, so it falls back to the ground at the feet of
		// the grabber rather than being lost.
		placement = spawnStruct();
		placement.origin = groundpos( self.origin + ( 0, 0, 16 ) ) + ( 0, 0, 5 );
		placement.angles = ( 0, self.angles[ 1 ], 0 );
	}

	car = spawnVehicle( "t5_veh_rcbomb_allies", "rcbomb", "rc_car_medium_mp", placement.origin, placement.angles );
	if ( !isDefined( car ) )
	{
		codolPrintLn( "CoDOL zm rcbomb: spawnvehicle rc_car_medium_mp returned nothing" );
		return;
	}

	car makeVehicleUnusable();
	car setOwner( self );
	car.owner = self;
	car setVehicleTeam( "allies" );
	car.team = "allies";
	car thread maps\mp\killstreaks\_rcbomb_presentation::start();

	self.zm_rcbomb = car;
	codolPrintLn( "CoDOL zm rcbomb: car " + car getEntityNumber() + " spawned for " + self.name + " at " + placement.origin );

	// T5 useRCBomb: the antenna pull before control.
	wait( 0.9 );

	if ( !isDefined( car ) )
	{
		self.zm_rcbomb = undefined;
		return;
	}

	if ( !is_player_valid( self ) || self isRemoteControlling() || isDefined( car getSeatOccupant( 0 ) ) )
	{
		codolPrintLn( "CoDOL zm rcbomb: " + self.name + " could not take the wheel; car removed" );
		car delete();
		self.zm_rcbomb = undefined;
		return;
	}

	car useVehicle( self, 0 );
	if ( !self isRemoteControlling() )
	{
		codolPrintLn( "CoDOL zm rcbomb: native remote control refused for " + self.name + "; car removed" );
		car delete();
		self.zm_rcbomb = undefined;
		return;
	}

	codolPrintLn( "CoDOL zm rcbomb: " + self.name + " is driving car " + car getEntityNumber() );

	car.zm_start_origin = car.origin;
	car thread maps\mp\killstreaks\_rcbomb_presentation::monitor();

	self thread car_detonate_waiter( car );
	self thread car_exit_waiter( car );
	self thread car_owner_waiter( car );
	self thread car_timer( car );
}

// T5 carDetonateWaiter: the attack button, or a forced detonation.
car_detonate_waiter( car )
{
	self endon( "disconnect" );
	car endon( "death" );
	car endon( "zm_rc_blown" );

	while ( !self attackButtonPressed() && !is_true( car.force_detonation ) )
		wait( 0.05 );

	car thread blowup( self, "detonated" );
}

// T5 exitCarWaiter.  Leaving the seat any other way blows the car up too,
// because zombies has no rule slot to hand back and no way to retake it.
car_exit_waiter( car )
{
	self endon( "disconnect" );
	car endon( "death" );
	car endon( "zm_rc_blown" );

	self waittill( "exit_vehicle" );

	car thread blowup( self, "driver left" );
}

// A down or a disconnect ends the ride.  Two waiters, because an endon on
// the player would end the waiting thread without detonating.
car_owner_waiter( car )
{
	self endon( "disconnect" );
	car endon( "death" );
	car endon( "zm_rc_blown" );

	car thread car_owner_disconnect_waiter( self );

	self waittill( "player_downed" );

	car thread blowup( self, "driver down" );
}

car_owner_disconnect_waiter( owner )
{
	self endon( "death" );
	self endon( "zm_rc_blown" );

	owner waittill( "disconnect" );

	self thread blowup( undefined, "driver gone" );
}

// T5 carTimer: 20 s, countdown flag, 6 s, timeout flag, 4 s.
car_timer( car )
{
	self endon( "disconnect" );
	car endon( "death" );
	car endon( "zm_rc_blown" );

	wait( 20 );
	car setClientFlag( level.const_flag_countdown );
	wait( 6 );
	car setClientFlag( level.const_flag_timeout );
	wait( 4 );

	car thread blowup( self, "timeout" );
}

// T5 blowup.  self is the car.
blowup( driver, reason )
{
	if ( is_true( self.zm_blown ) )
		return;
	self.zm_blown = true;
	self notify( "zm_rc_blown" );

	origin = self.origin + ( 0, 0, 10 );
	explosion_origin = self.origin;

	moved = 0;
	if ( isDefined( self.zm_start_origin ) )
		moved = int( distance( self.zm_start_origin, explosion_origin ) );

	before = level.total_zombies_killed;

	attacker = driver;
	if ( !isDefined( attacker ) )
		attacker = self;

	self radiusDamage( origin, 256, 350, 25, attacker, "MOD_EXPLOSIVE", "rcbomb_mp" );
	physicsExplosionSphere( origin, 256, 256, 1 );
	earthquake( 0.5, 0.5, origin, 512 );
	playFx( level._effect[ "rcbombexplosion" ], explosion_origin );

	self notify( "codol_rc_hidden" );
	self hide();

	wait( 0.05 );

	name = "(gone)";
	if ( isDefined( driver ) )
		name = driver.name;
	codolPrintLn( "CoDOL zm rcbomb: car of " + name + " blew up (" + reason + ") " + moved +
		" units from where it started; zombies killed by the blast so far: " + ( level.total_zombies_killed - before ) );

	wait( 0.95 );

	// T5: the owner's unlink releases the remote seat (exit_vehicle).
	if ( isDefined( driver ) )
	{
		if ( driver isRemoteControlling() )
			driver unlink();
		driver.zm_rcbomb = undefined;
	}

	if ( isDefined( self ) )
		self delete();
}

// ---------------------------------------------------------------------------
// T5 maps/mp/_rcbomb.gsc placement, as adapted in
// maps\mp\killstreaks\_rcbomb.gsc (native typed trace adapters).  self is the
// player.  The debug drawing is left out.

get_placement_start_height()
{
	startheight = 50;

	switch ( self getStance() )
	{
	case "crouch":
		startheight = 30;
		break;
	case "prone":
		startheight = 15;
		break;
	}
	return startheight;
}

calculate_spawn_origin( origin, angles )
{
	distance_from_player = 70;

	startheight = get_placement_start_height();

	mins = ( -5, -5, 0 );
	maxs = ( 5, 5, 10 );

	startPoints = [];
	startAngles = [];
	wheelCounts = [];
	testCheck = [];
	largestCount = 0;
	largestCountIndex = 0;

	testangles = [];
	testangles[ 0 ] = ( 0, 0, 0 );
	testangles[ 1 ] = ( 0, 20, 0 );
	testangles[ 2 ] = ( 0, -20, 0 );
	testangles[ 3 ] = ( 0, 45, 0 );
	testangles[ 4 ] = ( 0, -45, 0 );

	heightoffset = 5;

	for ( i = 0; i < testangles.size; i++ )
	{
		testCheck[ i ] = false;
		wheelCounts[ i ] = 0;

		startAngles[ i ] = ( 0, angles[ 1 ], 0 );
		startPoint = origin + vector_scale( anglesToForward( startAngles[ i ] + testangles[ i ] ), distance_from_player );
		endPoint = startPoint - ( 0, 0, 100 );
		startPoint = startPoint + ( 0, 0, startheight );

		// T5 PhysicsTraceMaskPhysics | Vehicle.
		trace = codol_rcPlacementTrace( startPoint, endPoint, mins, maxs, self, 3 );

		if ( isDefined( trace[ "entity" ] ) && isPlayer( trace[ "entity" ] ) )
			continue;

		startPoints[ i ] = trace[ "position" ] + ( 0, 0, heightoffset );
		wheelCounts[ i ] = test_wheel_locations( startPoints[ i ], startAngles[ i ], heightoffset );

		if ( positionWouldTelefrag( startPoints[ i ] ) )
			continue;

		if ( largestCount < wheelCounts[ i ] )
		{
			largestCount = wheelCounts[ i ];
			largestCountIndex = i;
		}

		if ( wheelCounts[ i ] >= 3 )
		{
			testCheck[ i ] = true;

			if ( test_spawn_origin( startPoints[ i ], startAngles[ i ] ) )
			{
				placement = spawnStruct();
				placement.origin = startPoints[ i ];
				placement.angles = startAngles[ i ];
				return placement;
			}
		}
	}

	for ( i = 0; i < testangles.size; i++ )
	{
		if ( testCheck[ i ] || !isDefined( startPoints[ i ] ) )
			continue;

		if ( wheelCounts[ i ] >= 2 && test_spawn_origin( startPoints[ i ], startAngles[ i ] ) )
		{
			placement = spawnStruct();
			placement.origin = startPoints[ i ];
			placement.angles = startAngles[ i ];
			return placement;
		}
	}

	return undefined;
}

test_wheel_locations( origin, angles, heightoffset )
{
	forward = 13;
	side = 10;

	wheels = [];
	wheels[ 0 ] = ( forward, side, 0 );
	wheels[ 1 ] = ( forward, -1 * side, 0 );
	wheels[ 2 ] = ( -1 * forward, -1 * side, 0 );
	wheels[ 3 ] = ( -1 * forward, side, 0 );

	height = 5;
	touchCount = 0;

	yawangles = ( 0, angles[ 1 ], 0 );

	for ( i = 0; i < 4; i++ )
	{
		wheel = rotatePoint( wheels[ i ], yawangles );
		startPoint = origin + wheel;
		endPoint = startPoint + ( 0, 0, ( -1 * height ) - heightoffset );
		startPoint = startPoint + ( 0, 0, height - heightoffset );

		trace = bulletTrace( startPoint, endPoint, false, self );
		if ( trace[ "fraction" ] < 1 )
			touchCount++;
	}

	return touchCount;
}

test_spawn_origin( origin, angles )
{
	liftedorigin = origin + ( 0, 0, 5 );
	size = 12;
	height = 15;
	mins = ( -1 * size, -1 * size, 0 );
	maxs = ( size, size, height );
	absmins = liftedorigin + mins;
	absmaxs = liftedorigin + maxs;

	if ( codol_rcBoundsWouldTelefrag( absmins, absmaxs ) )
		return false;

	startheight = get_placement_start_height();

	// T5 PhysicsTraceMaskPhysics | Vehicle | Water.
	trace = codol_rcPlacementTrace( liftedorigin, ( origin + ( 0, 0, 1 ) ), mins, maxs, self, 7 );
	if ( trace[ "fraction" ] < 1 )
		return false;

	size = 2.5;
	height = size * 2;
	mins = ( -1 * size, -1 * size, 0 );
	maxs = ( size, size, height );

	sweeptrace = codol_rcPlacementTrace( ( self.origin + ( 0, 0, startheight ) ), liftedorigin, mins, maxs, self, 7 );
	if ( sweeptrace[ "fraction" ] < 1 )
		return false;

	return true;
}
