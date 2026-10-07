// coop mission opening: the player vehicle ride, driven by the map's own
// entities.
//
// Two coop missions open with the squad riding in:
//
//   coop_training_mp_killhouse_sh (user report 2026-10-07, "a scripted
//   driving event in a jeep with captain price giving some dialogue"):
//     script_vehicle_hummer_viewmodel targetname humvee_player, model
//     vehicle_hummer_viewmodel, origin (6365 -1864 92), target me0_auto3003;
//     info_vehicle_node chain me0_auto3003 (6348, speed 25) -> me0_auto3020
//     (2870, 15) -> me0_auto3015 (2349, 10) -> me0_auto3018 (1933 -1748),
//     the road west into camp, ending beside the player_in_camp trigger;
//     info_player_start_so non_CQB_spawn_point (5560 -1866 16) on that road.
//
//   coop_prolog_mp_crash_intro:
//     script_vehicle targetname seaknightPlayer (vehicletype
//     seaknight_airlift, model vehicle_ch46e_opened_door_interior) at
//     (-1480 4329 1282), target chopper_start; a script_struct flight path
//     chopper_start -> ... -> me0_auto3393 (907 718 185, script_flag_set
//     player_heli_crash, speed 150) over the crash site, setting the
//     player_heli_a..f flags on the way; the trigger_multiple_spawn volumes
//     that release enemy_a..enemy_e sit on that path at z 731..1067, so the
//     enemies are released as the helicopter passes them.
//
// (MEASURED, docs/CODOL_CAMPAIGN_MODE.md section 8.)  IW4 would spawn the
// vehicle and drive it with the SP vehicle system; this server spawns no SP
// vehicle from a map entity, so the ride is a script_model mover of the
// vehicle's own model following its authored path (_coopvehicle.gsc), with
// the riders linked to it.  On the jeep the riders' weapons are lowered; in
// the helicopter they keep them, as the door gunners of the prologue.  At the
// end of the path the riders are set down beside the vehicle (the CH-46 is
// then hidden: the prologue's own crashed-helicopter models stand there).
//
// THE JEEP DIALOGUE IS INFERRED.  pit_pri_* are Price's lines on the training
// map, but no shipped script or string says which belong to the ride or in
// what order, so the first lines in alias order are played at a fixed
// spacing.  level.coop_intro_lines is the one place to correct that, and
// +set scr_coop_intro_dialogue 0 turns it off.  +set scr_coop_intro_ride 0
// turns the whole ride off (players then simply start at the map's start).
//
// Coop category root only; names no multiplayer module.

// The player's vehicle: an authored script_vehicle* whose targetname names
// the player (humvee_player, seaknightPlayer).
find_player_vehicle()
{
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( !isDefined( e.classname ) || e.classname.size < 14 || getSubStr( e.classname, 0, 14 ) != "script_vehicle" )
			continue;
		if ( !isDefined( e.targetname ) || !isSubStr( toLower( e.targetname ), "player" ) || !isDefined( e.target ) )
			continue;
		return e;
	}
	return undefined;
}

init()
{
	level.coop_intro = undefined;

	if ( getDvar( "scr_coop_intro_ride" ) != "" && getDvarInt( "scr_coop_intro_ride" ) == 0 )
		return;

	vehicle = find_player_vehicle();
	if ( !isDefined( vehicle ) )
		return;

	model = maps\mp\gametypes\_coopvehicle::vehicle_model( vehicle );
	if ( !isDefined( model ) )
	{
		codolprintln( "CoDOL coop intro: " + vehicle.targetname + " has no model; no ride" );
		return;
	}

	path = maps\mp\gametypes\_coopvehicle::get_path( vehicle.target );
	if ( path.size < 2 )
	{
		codolprintln( "CoDOL coop intro: " + vehicle.targetname + " has no vehicle path; no ride" );
		return;
	}

	// The precache window is Callback_StartGameType, where this runs.
	precacheModel( model );

	intro = spawnStruct();
	intro.vehicle = vehicle;
	intro.model = model;
	intro.path = path;
	intro.ground = maps\mp\gametypes\_coopvehicle::is_ground_path( path );
	// A ground vehicle's authored origin sits this far above its first node;
	// a helicopter path is the vehicle's own positions.
	intro.height = 0;
	if ( intro.ground )
		intro.height = vehicle.origin[ 2 ] - path[ 0 ].origin[ 2 ];
	level.coop_intro = intro;

	level.coop_intro_lines = [];
	if ( intro.ground )
	{
		level.coop_intro_lines[ 0 ] = "pit_pri_001";
		level.coop_intro_lines[ 1 ] = "pit_pri_002_1";
		level.coop_intro_lines[ 2 ] = "pit_pri_002_2";
		level.coop_intro_lines[ 3 ] = "pit_pri_002_3";
		level.coop_intro_lines[ 4 ] = "pit_pri_003";
	}

	codolprintln( "CoDOL coop intro: " + vehicle.targetname + " (" + model + ") ride over " + path.size +
		" path points from " + vehicle.origin + " to " + path[ path.size - 1 ].origin + ", ground " + intro.ground );
}

// Threaded from _coopmode::Callback_StartGameType after init().
main()
{
	if ( !isDefined( level.coop_intro ) )
		return;

	level endon( "coop_mission_over" );

	// The ride starts when the first player is in; everyone in by then rides.
	riders = [];
	while ( riders.size <= 0 )
	{
		wait( 0.5 );
		riders = ride_candidates();
	}
	wait( 1.5 );
	riders = ride_candidates();

	ride( riders );
}

ride_candidates()
{
	riders = [];
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( !isAlive( p ) || !isDefined( p.sessionstate ) || p.sessionstate != "playing" )
			continue;
		riders[ riders.size ] = p;
	}
	return riders;
}

// Seats: offsets from the vehicle origin in its own frame (forward, right,
// up).  The humvee viewmodel's origin is MEASURED 92 units above the road at
// its spawn (checkstarts ground trace, road at z 0), so its riders' feet go
// 62 below it; the CH-46 interior model's origin is its cabin floor
// (INFERRED), so riders stand on it.
seat_offset( index, ground )
{
	seats = [];
	if ( ground )
	{
		seats[ 0 ] = ( -8, 18, -62 );
		seats[ 1 ] = ( -8, -18, -62 );
		seats[ 2 ] = ( -48, 18, -62 );
		seats[ 3 ] = ( -48, -18, -62 );
	}
	else
	{
		seats[ 0 ] = ( 0, 24, 8 );
		seats[ 1 ] = ( 0, -24, 8 );
		seats[ 2 ] = ( 48, 24, 8 );
		seats[ 3 ] = ( 48, -24, 8 );
	}
	return seats[ index % seats.size ];
}

local_to_world( origin, angles, offset )
{
	return origin + anglesToForward( angles ) * offset[ 0 ] + anglesToRight( angles ) * offset[ 1 ] + ( 0, 0, offset[ 2 ] );
}

ride( riders )
{
	intro = level.coop_intro;

	vehicle = intro.vehicle;
	mover = spawn( "script_model", vehicle.origin );
	mover.angles = vehicle.angles;
	mover setModel( intro.model );
	vehicle hide();
	intro.mover = mover;

	codolprintln( "CoDOL coop intro: ride starts with " + riders.size + " rider(s)" );
	level.coop_intro_riding = true;

	for ( i = 0; i < riders.size; i++ )
	{
		p = riders[ i ];
		p.coop_in_intro = true;
		if ( intro.ground )
			p disableWeapons();
		p setStance( "stand" );
		p setOrigin( local_to_world( mover.origin, mover.angles, seat_offset( i, intro.ground ) ) );
		p setPlayerAngles( mover.angles );
		p playerLinkToDelta( mover, "tag_origin", 1 );
	}

	if ( intro.ground )
		mover playLoopSound( "hummer_player_engine_low" );
	level thread intro_dialogue( riders );

	defaultMph = 15;
	if ( !intro.ground )
		defaultMph = 30;
	mover maps\mp\gametypes\_coopvehicle::follow_path( intro.path, intro.height, defaultMph );

	if ( intro.ground )
	{
		mover stopLoopSound();
		mover playSound( "veh_humvee_engine_stop" );
	}
	wait( 0.5 );

	// Set the riders down beside the vehicle, on the ground.
	for ( i = 0; i < riders.size; i++ )
	{
		p = riders[ i ];
		if ( !isDefined( p ) || !isAlive( p ) )
			continue;
		p unlink();
		side = 1;
		if ( i % 2 )
			side = -1;
		spot = mover.origin + anglesToRight( mover.angles ) * ( 64 * side ) - anglesToForward( mover.angles ) * ( 24 * int( i / 2 ) );
		trace = bulletTrace( spot + ( 0, 0, 16 ), spot - ( 0, 0, 1024 ), false, mover );
		if ( trace[ "fraction" ] < 1 )
			spot = trace[ "position" ] + ( 0, 0, 1 );
		p setOrigin( spot );
		p enableWeapons();
		p.coop_in_intro = undefined;
	}

	if ( !intro.ground )
		mover hide();

	level.coop_intro_riding = undefined;
	codolprintln( "CoDOL coop intro: ride over at " + mover.origin );
	level notify( "coop_intro_done" );
}

intro_dialogue( riders )
{
	if ( getDvar( "scr_coop_intro_dialogue" ) != "" && getDvarInt( "scr_coop_intro_dialogue" ) == 0 )
		return;
	lines = level.coop_intro_lines;
	wait( 1.5 );
	for ( l = 0; l < lines.size; l++ )
	{
		for ( i = 0; i < riders.size; i++ )
		{
			if ( isDefined( riders[ i ] ) )
				riders[ i ] playLocalSound( lines[ l ] );
		}
		wait( 3.5 );
	}
}
