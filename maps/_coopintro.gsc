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
// THE JEEP DIALOGUE comes from _cooptraining.gsc ride_lines(): Price's
// pit_pri_001..003, whose place in the mission was established by listening
// to the shipped lines (docs/CODOL_CAMPAIGN_MODE.md section 9).  They play
// from the jeep at their own lengths.  +set scr_coop_intro_dialogue 0 turns
// them off; +set scr_coop_intro_ride 0 turns the whole ride off (players then
// simply start at the map's start).
//
// 2026-10-07 user report, second round: "the jeep just floats in air while I
// spawned on foot outside the double car gates".  Fixed here: the jeep stands
// on the ground under its authored origin and path (_coopvehicle
// ground_under), players are seated as they spawn (board(), from
// _coopmode::spawnPlayer) and the ride waits for its riders to look around.
//
// Coop module (maps/, IW4 singleplayer namespace); names no multiplayer module.

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

	model = maps\_coopvehicle::vehicle_model( vehicle );
	if ( !isDefined( model ) )
	{
		codolprintln( "CoDOL coop intro: " + vehicle.targetname + " has no model; no ride" );
		return;
	}

	path = maps\_coopvehicle::get_path( vehicle.target );
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
	intro.ground = maps\_coopvehicle::is_ground_path( path );
	intro.riders = [];
	intro.started = false;
	intro.done = false;
	level.coop_intro = intro;

	// The mover exists from the start, parked where the vehicle is authored,
	// so the first player spawns INTO it.  A ground vehicle stands on the
	// ground under its authored origin: the 2026-10-07 user report "the jeep
	// just floats in air" was the authored origin (z 92, 90 units above the
	// road) used as the model's origin.
	start = vehicle.origin;
	if ( intro.ground )
		start = maps\_coopvehicle::ground_under( start );
	mover = spawn( "script_model", start );
	mover.angles = vehicle.angles;
	mover setModel( model );
	vehicle hide();
	intro.mover = mover;

	level.coop_intro_lines = [];
	if ( intro.ground )
		maps\_cooptraining::ride_lines();

	codolprintln( "CoDOL coop intro: " + vehicle.targetname + " (" + model + ") parked at " + start + ", ride over " +
		path.size + " path points to " + path[ path.size - 1 ].origin + ", ground " + intro.ground );
}

// Threaded from _coopmode::Callback_StartGameType after init().
//
// THE START.  Players are seated as they SPAWN (_coopmode::spawnPlayer calls
// board()), so the first player begins in the vehicle rather than beside it.
// The vehicle then waits until its riders are actually playing: a real C1
// client is spawned when it enters the game but is still fading in from the
// load for several seconds, and the first version drove off 1.5 s after the
// spawn -- the user saw only the end of a ride he never watched (MEASURED,
// VPS codol-match-17733: "ride starts with 1 rider", "ride over", then the
// player walking from camp).  The ride now starts when a rider turns his view
// (the first sign a real client is looking at the world) or after
// scr_coop_intro_wait seconds (default 8).
main()
{
	if ( !isDefined( level.coop_intro ) )
		return;

	level endon( "coop_mission_over" );
	intro = level.coop_intro;

	while ( intro.riders.size <= 0 )
		wait( 0.25 );

	limit = getDvarFloat( "scr_coop_intro_wait" );
	if ( limit <= 0 )
		limit = 8;
	waited = 0;
	while ( waited < limit && !riders_looking() )
	{
		wait( 0.25 );
		waited += 0.25;
	}

	// The training jeep's driver is an actor (_cooptraining seat_driver),
	// placed once the cast can spawn; the jeep does not leave without him.
	for ( w = 0; isDefined( level.coop_intro_wait_driver ) && w < 40; w++ )
		wait( 0.25 );

	ride();
}

// A rider whose view has moved since he was seated is looking at the world.
riders_looking()
{
	riders = level.coop_intro.riders;
	for ( i = 0; i < riders.size; i++ )
	{
		p = riders[ i ];
		if ( !isDefined( p ) || !isDefined( p.coop_seat_angles ) )
			continue;
		a = p getPlayerAngles();
		if ( abs_angle( a[ 1 ] - p.coop_seat_angles[ 1 ] ) > 2 || abs_angle( a[ 0 ] - p.coop_seat_angles[ 0 ] ) > 2 )
			return true;
	}
	return false;
}

abs_angle( a )
{
	while ( a > 180 )
		a -= 360;
	while ( a < -180 )
		a += 360;
	if ( a < 0 )
		return 0 - a;
	return a;
}

// _coopmode::spawnPlayer hook: true when the player was seated.
board()
{
	intro = level.coop_intro;
	if ( !isDefined( intro ) || intro.done || !isDefined( intro.mover ) )
		return false;

	seat = intro.riders.size;
	intro.riders[ seat ] = self;
	self.coop_in_intro = true;
	if ( intro.ground )
		self disableWeapons();
	self setStance( "stand" );

	mover = intro.mover;
	offset = seat_offset( seat, intro.ground );
	// The humvee seats are the model's own tags (MEASURED, _cooptest
	// jeeptags on vehicle_hummer_viewmodel: tag_driver front left,
	// tag_passenger front right, tag_guy0 / tag_guy1 rear left / right,
	// tag_player the middle, all about 36 units up).  Price has the driver's
	// tag (_cooptraining seat_driver); the riders sit, crouched, on the rest.
	tag = seat_tag( seat, intro.ground );
	if ( isDefined( tag ) )
	{
		offset = mover getTagOrigin( tag ) - mover.origin;
		// The seat tag is the cushion (about 36 above the jeep's origin on
		// the ground).  A crouched player's eye is 40 above his feet.  With
		// the feet 12 below the tag the user's view sat at Price's elbow,
		// under the dash line (his recording, 2026-10-08); the retail
		// recording's rider looks over the dash with his eye level with
		// Price's head, about 10 higher, so the feet go 2 below the tag.
		offset = ( vectorDot( offset, anglesToForward( mover.angles ) ), vectorDot( offset, anglesToRight( mover.angles ) ), offset[ 2 ] - 2 );
		self setStance( "crouch" );
		self seat_lock( true );
	}
	self setOrigin( local_to_world( mover.origin, mover.angles, offset ) );
	self setPlayerAngles( mover.angles );
	self.coop_seat_angles = mover.angles;
	self playerLinkToDelta( mover, "tag_origin", 1 );
	// The training's title card over a black screen that lifts as the ride
	// starts (the retail recording, 1:19-1:26).
	if ( isDefined( level.coop_training_flow ) && level.coop_training_flow )
		self thread maps\_cooptraininghud::title_card_player();
	tagName = "none";
	if ( isDefined( tag ) )
		tagName = tag;
	codolprintln( "CoDOL coop intro: " + self.name + " seated in seat " + seat + " (" + tagName + ") offset " + offset +
		", stance " + self getStance() + ", linked " + self isLinked() + ", ride started " + intro.started );
	return true;
}

// A seated rider keeps the crouched (sitting) stance until he gets out:
// C1's stance permissions (allowStand / allowProne: pm_flags 0x800000 /
// 0x2000000, which its own prediction honours while linked, C1 0x412e42)
// refuse standing and going prone, and jumping and sprinting are off.  The
// link (PM_NORMAL_LINKED) carries him with the jeep and leaves him no
// walking of his own.
seat_lock( locked )
{
	self allowStand( !locked );
	self allowProne( !locked );
	self allowJump( !locked );
	self allowSprint( !locked );
	self.coop_seat_locked = locked;
}

// Seats: offsets from the vehicle origin in its own frame (forward, right,
// up).  The humvee stands on the ground (see init), so a seated rider's feet
// are a little above it; the CH-46 interior model's origin is its cabin floor
// (INFERRED), so riders stand on it.
seat_offset( index, ground )
{
	seats = [];
	if ( ground )
	{
		seats[ 0 ] = ( -8, 18, 24 );
		seats[ 1 ] = ( -40, -18, 24 );
		seats[ 2 ] = ( -40, 18, 24 );
		seats[ 3 ] = ( -64, 0, 24 );
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

seat_tag( index, ground )
{
	if ( !ground )
		return undefined;
	tags = [];
	tags[ 0 ] = "tag_passenger";
	tags[ 1 ] = "tag_guy1";
	tags[ 2 ] = "tag_guy0";
	tags[ 3 ] = "tag_player";
	return tags[ index % tags.size ];
}

local_to_world( origin, angles, offset )
{
	return origin + anglesToForward( angles ) * offset[ 0 ] + anglesToRight( angles ) * offset[ 1 ] + ( 0, 0, offset[ 2 ] );
}

ride()
{
	intro = level.coop_intro;
	mover = intro.mover;
	intro.started = true;
	level.coop_intro_riding = true;
	codolprintln( "CoDOL coop intro: ride starts with " + intro.riders.size + " rider(s)" );
	level notify( "coop_intro_started" );

	if ( intro.ground )
		mover playLoopSound( "hummer_player_engine_low" );
	level thread intro_dialogue();

	defaultMph = 15;
	if ( !intro.ground )
		defaultMph = 30;
	mover maps\_coopvehicle::follow_path( intro.path, 0, defaultMph, intro.ground );

	if ( intro.ground )
	{
		mover stopLoopSound();
		mover playSound( "veh_humvee_engine_stop" );
	}
	wait( 0.5 );

	intro.done = true;
	riders = intro.riders;
	for ( i = 0; i < riders.size; i++ )
	{
		p = riders[ i ];
		if ( !isDefined( p ) || !isAlive( p ) )
			continue;
		p unlink();
		side = 1;
		if ( i % 2 )
			side = -1;
		spot = mover.origin + anglesToRight( mover.angles ) * ( 72 * side ) - anglesToForward( mover.angles ) * ( 24 * int( i / 2 ) );
		trace = bulletTrace( spot + ( 0, 0, 32 ), spot - ( 0, 0, 1024 ), false, mover );
		if ( trace[ "fraction" ] < 1 )
			spot = trace[ "position" ] + ( 0, 0, 1 );
		if ( isDefined( p.coop_seat_locked ) && p.coop_seat_locked )
			p seat_lock( false );
		p setOrigin( spot );
		p setStance( "stand" );
		p enableWeapons();
		p.coop_in_intro = undefined;
	}

	if ( !intro.ground )
		mover hide();

	level.coop_intro_riding = undefined;
	codolprintln( "CoDOL coop intro: ride over at " + mover.origin );
	level notify( "coop_intro_done" );
}

// The ride's lines, from the vehicle's driver when the map authors one
// (_cooptraining gives the training humvee Price at the wheel), else from the
// vehicle.
intro_dialogue()
{
	if ( getDvar( "scr_coop_intro_dialogue" ) != "" && getDvarInt( "scr_coop_intro_dialogue" ) == 0 )
		return;
	lines = level.coop_intro_lines;
	speaker = level.coop_intro.mover;
	if ( isDefined( level.coop_intro_driver ) )
		speaker = level.coop_intro_driver;
	wait( 1.0 );
	for ( l = 0; l < lines.size; l++ )
	{
		if ( !isDefined( speaker ) )
			return;
		speaker playSound( lines[ l ].alias );
		codolprintln( "CoDOL coop intro: line " + lines[ l ].alias );
		wait( lines[ l ].time );
	}
}
