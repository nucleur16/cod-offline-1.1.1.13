// zm extraction: survive the round cap, then reach the rescue helicopter.
//
// CoDOL's zombies ended in a rescue, not T5's endless rounds.  Nothing of the
// Tencent game script survives, so this file is rebuilt from what C1 ships
// for it -- every name below is MEASURED from the client's zones:
//
// THE STRINGS (localized_code_post_gfx_mp, always mounted; again in the
// Prototype and Training map zones).  Chinese text, English meaning:
//   ZOMBIE_ROUNDS_REMAINING  "Rescue arrives in &&1 rounds"
//   ZOMBIE_ROUNDS_FINAL      "Rescue arrives in 1 round"
//   ZOMBIE_RESCUE_BEGIN      "Final round"
//   ZOMBIE_RESCUE_DISTANCE   "Rescue team arriving: &&1 minutes"
//   ZOMBIE_RESCUE_OBJECTIVE  "Go to the extraction point"
//   ZOMBIE_RESCUE_LEAVING    "Rescue team leaving: "   (a timer label)
//   ZOMBIE_RESCUE_HINT       "Sprint and press [{+gostand}] to jump onto the
//                             rescue aircraft"
//   ZOMBIE_DID_NOT_SURVIVE   "You didn't make it out..."
//   ZOMBIE_GAME_OVER, ZOMBIE_SURVIVED_ROUNDS / _ROUND, and the zm gametype
//   zone's SCRIPT_MISSION_COMPLETE.
//   MPUI_TAR_MAP_ZM_ASYLUM_0/1/2 (the lobby's map goals): "Survive 6 / 12 /
//   21 rounds, defeat the BOSS, then escape".
// THE SPLASHES (mp/splashtable.csv rows, C1's own end-of-rescue outcomes):
//   zrescue_all_exfil  SPLASHES_ZRESCUE_ALL   "Mission complete! / Every
//                      teammate was rescued!"            sound zac130_victory
//   zrescue_exfil      SPLASHES_ZRESCUE_EXFIL "Emergency extraction! /
//                      Someone was left behind!"         sound zac130_victory
//   zrescue_left       SPLASHES_ZRESCUE_LEFT  "Left behind! / Your teammates
//                      left without you!"                sound zac130_defeat
// THE HELICOPTER (common_gametype_zm): VehicleDef blackhawk_minigun, xmodel
//   vehicle_blackhawk_minigun_hero, the blackhawk engine/idle sounds, the
//   bh_*_idle rider and fastrope clips; mus_zombie_rescue and
//   mus_zombie_victory.
// THE RADIO (common_gametype_zm, subtitled in localized_common_gametype_zm as
//   SUBTITLE_ZMB_RESCUE_ROUND_*): HQ zmby_evac3mins/2mins/1min_hq_r,
//   zmby_30seconds_hq_r, zmby_onetheway_hq_r, zmby_getout_hq_r,
//   zmby_longer_hq_r; the pilot's zmby_lz_01..03_chpr, zmby_jump_chpr,
//   zmby_liftoff1..3_chpr; and each character's <code>_evac_call.
// THE ROUTES (each zm map's MapEnts, script_structs):
//   rescue_dest (the landing zone on the ground) -> target: the helicopter's
//   first struct (script_noteworthy "blackhawk_minigun", model
//   vehicle_blackhawk_minigun_hero) -> target chain -> the hover struct
//   (script_noteworthy "stop") -> the exit structs.  Prototype and Asylum
//   author two landing zones each; Training authors one rescue_dest with no
//   target plus its own chain from "ending_heli_start".  The round's rescue
//   uses one landing zone, chosen at random among the complete routes.
// THE MARKER: waypoint_extraction (localized_common_mp, like the
//   waypoint_revive the last stand draws) and the smoke/flare_smoke FX with
//   sh_env_roadflare_burn.
//
// THE FLOW
//   rounds 1 .. cap-1  every round opens with "Rescue arrives in N rounds".
//   round cap          "Final round" (C1's round HUD draws MPUI_FINAL_ROUND).
//   cap survived       the extraction: zombies keep coming; a character
//                      radios for evac, HQ answers; one minute later the
//                      helicopter flies the route to its hover point.
//   landed             "Go to the extraction point", the waypoint and a
//                      leave timer.  A player boards by jumping at the
//                      helicopter (C1's own hint), or by standing in the
//                      landing zone when it lifts off.  It leaves when the
//                      timer runs out or as soon as every standing player
//                      is aboard.
//   lift-off           the boarded ride out; the rest are left to the
//                      horde.  Anyone aboard -> the match is a WIN.
//
// Dvars: scr_zm_roundcap (default 13, 0 = endless rounds, no extraction),
// scr_zm_extract_arrive (seconds from the call to the landing, default 60),
// scr_zm_extract_wait (seconds the helicopter waits, default 60).

#include maps\mp\gametypes\_zombiemode_utility;

HELI_VEHICLE = "blackhawk_minigun";
HELI_MODEL = "vehicle_blackhawk_minigun_hero";
LZ_RADIUS = 220;
BOARD_RADIUS = 200;

init()
{
	precacheString( &"ZOMBIE_ROUNDS_REMAINING" );
	precacheString( &"ZOMBIE_ROUNDS_FINAL" );
	precacheString( &"ZOMBIE_RESCUE_BEGIN" );
	precacheString( &"ZOMBIE_RESCUE_DISTANCE" );
	precacheString( &"ZOMBIE_RESCUE_OBJECTIVE" );
	precacheString( &"ZOMBIE_RESCUE_LEAVING" );
	precacheString( &"ZOMBIE_RESCUE_HINT" );
	precacheString( &"ZOMBIE_DID_NOT_SURVIVE" );
	precacheString( &"ZOMBIE_GAME_OVER" );
	precacheString( &"ZOMBIE_SURVIVED_ROUNDS" );
	precacheString( &"ZOMBIE_SURVIVED_ROUND" );
	precacheString( &"SCRIPT_MISSION_COMPLETE" );

	precacheShader( "waypoint_extraction" );
	precacheModel( HELI_MODEL );

	level._effect[ "zm_lz_flare" ] = loadfx( "smoke/flare_smoke" );

	level.zm_extract_arrive = dvar_seconds( "scr_zm_extract_arrive", 60 );
	level.zm_extract_wait = dvar_seconds( "scr_zm_extract_wait", 60 );

	// The helicopter flies as C1's own VehicleDef when the zone carries it
	// (rotors, engine sound and lights from the definition); the script_model
	// mover is the fallback.  scr_zm_extract_vehicle 0 forces the mover.
	level.zm_extract_vehicle = false;
	if ( getDvar( "scr_zm_extract_vehicle" ) == "" || getDvarInt( "scr_zm_extract_vehicle" ) != 0 )
	{
		precacheVehicle( HELI_VEHICLE );
		level.zm_extract_vehicle = true;
	}

	level.zm_extraction_routes = find_routes();
	codolprintln( "CoDOL zm extraction: round cap " + level.zm_round_cap + ", " +
		level.zm_extraction_routes.size + " rescue route(s) on " + getDvar( "mapname" ) +
		", helicopter as " + ternary_string( level.zm_extract_vehicle, "vehicle " + HELI_VEHICLE, "script_model" ) );

	level thread rescue_round_notices();
}

dvar_seconds( name, def )
{
	if ( getDvar( name ) == "" )
		return def;
	value = getDvarFloat( name );
	if ( value < 5 )
		value = 5;
	return value;
}

ternary_string( cond, a, b )
{
	if ( cond )
		return a;
	return b;
}

// --------------------------------------------------------------------------
// Routes.  rescue_dest -> heli chain, from the map's own structs.
// --------------------------------------------------------------------------

find_routes()
{
	routes = [];
	dests = get_struct_array( "rescue_dest", "targetname" );
	starts = get_struct_array( "blackhawk_minigun", "script_noteworthy" );

	for ( i = 0; i < dests.size; i++ )
	{
		dest = dests[ i ];
		start = undefined;
		if ( isDefined( dest.target ) )
			start = get_struct( dest.target, "targetname" );

		// Training: the landing zone names no route; take the helicopter chain
		// whose hover point is nearest to it.
		if ( !isDefined( start ) )
			start = nearest_start( dest, starts );
		if ( !isDefined( start ) )
			continue;

		route = build_route( dest, start );
		if ( isDefined( route ) )
			routes[ routes.size ] = route;
	}
	return routes;
}

nearest_start( dest, starts )
{
	best = undefined;
	best_dist = 0;
	for ( i = 0; i < starts.size; i++ )
	{
		route = build_route( dest, starts[ i ] );
		if ( !isDefined( route ) )
			continue;
		d = distanceSquared( route.hover.origin, dest.origin );
		if ( !isDefined( best ) || d < best_dist )
		{
			best = starts[ i ];
			best_dist = d;
		}
	}
	return best;
}

build_route( dest, start )
{
	route = spawnStruct();
	route.dest = dest;
	route.inbound = [];
	route.outbound = [];
	route.hover = undefined;

	node = start;
	for ( guard = 0; isDefined( node ) && guard < 32; guard++ )
	{
		if ( !isDefined( route.hover ) )
		{
			route.inbound[ route.inbound.size ] = node;
			if ( isDefined( node.script_noteworthy ) && node.script_noteworthy == "stop" )
				route.hover = node;
		}
		else
			route.outbound[ route.outbound.size ] = node;

		if ( !isDefined( node.target ) )
			break;
		node = get_struct( node.target, "targetname" );
	}

	if ( !isDefined( route.hover ) || route.inbound.size < 2 )
		return undefined;

	// The exit: the chain past the hover point, else straight back out the
	// way it came in.
	if ( route.outbound.size < 1 )
		route.outbound[ 0 ] = route.inbound[ 0 ];

	// The landing zone is the ground under the rescue_dest marker.
	trace = bulletTrace( dest.origin + ( 0, 0, 16 ), dest.origin - ( 0, 0, 512 ), false, undefined );
	route.lz = dest.origin;
	if ( trace[ "fraction" ] < 1 )
		route.lz = trace[ "position" ];

	return route;
}

struct_angles( s )
{
	if ( isDefined( s.angles ) )
		return s.angles;
	return ( 0, 0, 0 );
}

// --------------------------------------------------------------------------
// Round-start notices.  C1's round HUD itself carries the n/cap count and
// the final-round label (_zombiemode::updateRoundHud); these are the
// rescue's own lines.
// --------------------------------------------------------------------------

rescue_round_notices()
{
	level endon( "end_game" );

	for ( ;; )
	{
		level waittill( "start_of_round" );

		cap = level.zm_round_cap;
		if ( cap <= 0 )
			continue;

		remaining = cap - level.round_number;
		if ( remaining > 1 )
			notice_all( &"ZOMBIE_ROUNDS_REMAINING", remaining );
		else if ( remaining == 1 )
			notice_all( &"ZOMBIE_ROUNDS_FINAL" );
		else if ( remaining == 0 )
			notice_all( &"ZOMBIE_RESCUE_BEGIN" );
	}
}

notice_all( text, value )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] thread notice( text, value );
}

notice( text, value )
{
	self endon( "disconnect" );

	// One notice at a time: a newer one replaces the one showing.
	self notify( "zm_rescue_notice" );
	self endon( "zm_rescue_notice" );

	elem = newClientHudElem( self );
	elem.alignX = "center";
	elem.alignY = "middle";
	elem.horzAlign = "center";
	elem.vertAlign = "top";
	elem.x = 0;
	elem.y = 80;
	elem.font = "objective";
	elem.fontScale = 1.6;
	elem.color = ( 1, 0.85, 0.3 );
	elem.glowColor = ( 0.6, 0.3, 0.05 );
	elem.glowAlpha = 0.5;
	elem.foreground = true;
	elem.hidewheninmenu = true;
	elem.alpha = 0;
	if ( isDefined( value ) )
	{
		elem.label = text;
		elem setValue( value );
	}
	else
		elem setText( text );

	self thread notice_destroy_on_replace( elem );

	elem fadeOverTime( 0.5 );
	elem.alpha = 1;
	wait( 4.5 );
	elem fadeOverTime( 1 );
	elem.alpha = 0;
	wait( 1 );
	self notify( "zm_rescue_notice_done" );
}

notice_destroy_on_replace( elem )
{
	self waittill_either( "zm_rescue_notice", "zm_rescue_notice_done" );
	if ( isDefined( elem ) )
		elem destroy();
}

// --------------------------------------------------------------------------
// The extraction.  _zombiemode::round_think hands over here once the cap
// round is survived; it does not return: it ends in "end_game".
// --------------------------------------------------------------------------

extraction_start()
{
	level endon( "end_game" );

	if ( is_true( level.zm_extraction_active ) )
		return;
	level.zm_extraction_active = true;
	level notify( "zm_extraction_start" );

	codolprintln( "CoDOL zm extraction: round " + level.round_number + " survived; calling the rescue" );

	if ( level.zm_extraction_routes.size < 1 )
	{
		// A map with no rescue route still finishes as a win: there is nowhere
		// to be picked up, so everyone standing is extracted on the spot.
		codolprintln( "CoDOL zm extraction: no rescue route on " + getDvar( "mapname" ) + "; extracting everyone standing" );
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( is_player_valid( players[ i ] ) )
				players[ i ] mark_extracted();
		}
		extraction_finish();
		return;
	}

	route = pick_route();
	level.zm_extraction_route = route;
	codolprintln( "CoDOL zm extraction: landing zone " + route.lz + ", hover " + route.hover.origin +
		", " + route.inbound.size + " inbound / " + route.outbound.size + " outbound nodes" );

	// The horde does not stop for the rescue.
	level thread extraction_horde();

	// The rescue music, a character's radio call and HQ's answer.
	level thread maps\mp\gametypes\_zombiemode_audio::change_zombie_music( "rescue" );
	level thread evac_call_vo();

	// "Rescue team arriving: N minutes".
	minutes = int( ( level.zm_extract_arrive + 59 ) / 60 );
	if ( minutes < 1 )
		minutes = 1;
	notice_all( &"ZOMBIE_RESCUE_DISTANCE", minutes );

	// The helicopter's own flight is part of the wait: it is launched so it
	// reaches the hover point at the announced time.
	flight = route_flight_time( route );
	pre = level.zm_extract_arrive - flight;
	if ( pre > 0 )
		wait( pre );

	heli = spawn_rescue_heli( route );
	level.zm_rescue_heli = heli;
	radio_all( "zmby_lz_01_chpr", 0 );

	heli heli_fly_inbound( route );

	codolprintln( "CoDOL zm extraction: the helicopter is over the landing zone" );
	extraction_wait( route, heli );

	extraction_leave( route, heli );
	extraction_finish();
}

// The landing zone the players can reach: a route whose landing zone lies in
// a zone the players have opened (the zone manager's player volumes), at
// random among those; any route when none is (or the map has no volumes).
// Prototype's two landing zones are both upstairs, behind the doors.
pick_route()
{
	open = [];
	for ( i = 0; i < level.zm_extraction_routes.size; i++ )
	{
		zone = maps\mp\gametypes\_zombiemode_zone_manager::zone_at_point( level.zm_extraction_routes[ i ].lz );
		if ( isDefined( zone ) && zone.enabled )
			open[ open.size ] = level.zm_extraction_routes[ i ];
	}
	if ( open.size > 0 )
		return open[ randomInt( open.size ) ];
	return level.zm_extraction_routes[ randomInt( level.zm_extraction_routes.size ) ];
}

// T5's round spawner with an endless budget: zombies keep coming until the
// helicopter is gone.  The spawner's own between-round limits (the AI cap,
// the spawn delay) still apply.
extraction_horde()
{
	level endon( "end_game" );

	// A level may run its own (zm_zombie_training's tutorial ends without one).
	if ( isDefined( level.zm_extraction_horde_func ) )
	{
		level thread [[ level.zm_extraction_horde_func ]]();
		return;
	}

	level.zm_extraction_horde = true;
	level.max_zombie_func = ::extraction_max_zombies;
	level.round_spawn_func = maps\mp\gametypes\_zombiemode_spawner::round_spawning;
	level thread [[ level.round_spawn_func ]]();
}

extraction_max_zombies( max )
{
	return 100000;
}

evac_call_vo()
{
	level endon( "end_game" );

	players = getPlayers();
	callers = [];
	for ( i = 0; i < players.size; i++ )
	{
		if ( is_player_valid( players[ i ] ) )
			callers[ callers.size ] = players[ i ];
	}
	if ( callers.size > 0 )
	{
		level.player_is_speaking = false;
		callers[ randomInt( callers.size ) ] maps\mp\gametypes\_zombiemode_audio::create_and_play_dialog( "evac_call", true );
	}

	wait( 4 );
	if ( level.zm_extract_arrive >= 150 )
		radio_all( "zmby_evac3mins_hq_r", 0 );
	else if ( level.zm_extract_arrive >= 90 )
		radio_all( "zmby_evac2mins_hq_r", 0 );
	else
		radio_all( "zmby_evac1min_hq_r", 0 );
}

// The radio: a 2D line on every player.  Aliases that a zone does not carry
// are skipped (_zombiemode_audio's own check).
radio_all( alias, delay )
{
	if ( isDefined( delay ) && delay > 0 )
		wait( delay );
	if ( !maps\mp\gametypes\_zombiemode_audio::alias_exists( alias ) )
		return;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] playLocalSound( alias );
}

// --------------------------------------------------------------------------
// The helicopter.
// --------------------------------------------------------------------------

HELI_SPEED = 50;	// mph, the vehicle's cruise (MP _helicopter uses 30..80)
HELI_ACCEL = 20;
HELI_UPS = 800;		// units/s, the mover's cruise; ~ HELI_SPEED * 17.6

route_flight_time( route )
{
	total = 0;
	for ( i = 1; i < route.inbound.size; i++ )
		total += distance( route.inbound[ i - 1 ].origin, route.inbound[ i ].origin );
	return total / HELI_UPS + 2;
}

spawn_rescue_heli( route )
{
	start = route.inbound[ 0 ];
	angles = struct_angles( start );
	heli = undefined;

	if ( level.zm_extract_vehicle )
	{
		owner = undefined;
		players = getPlayers();
		if ( players.size > 0 )
			owner = players[ 0 ];
		if ( isDefined( owner ) )
			heli = spawnHelicopter( owner, start.origin, angles, HELI_VEHICLE, HELI_MODEL );
		if ( isDefined( heli ) )
		{
			heli.zm_vehicle = true;
			heli Vehicle_SetSpeed( HELI_SPEED, HELI_ACCEL );
			heli setYawSpeed( 60, 30 );
			heli setMaxPitchRoll( 15, 15 );
			heli setHoverParams( 20, 10, 5 );
			heli setCanDamage( false );
			codolprintln( "CoDOL zm extraction: helicopter vehicle " + HELI_VEHICLE + " spawned at " + start.origin );
		}
		else
			codolprintln( "CoDOL zm extraction: spawnHelicopter produced nothing; using the script_model" );
	}

	if ( !isDefined( heli ) )
	{
		heli = spawn( "script_model", start.origin );
		heli setModel( HELI_MODEL );
		heli.angles = angles;
		heli.zm_vehicle = false;
		heli playLoopSound( "blackhawk_engine_high" );
	}

	heli.targetname = "zm_rescue_heli";
	heli.riders = [];
	return heli;
}

// Fly a chain of structs.  The last node of the inbound chain is the hover
// point, where the helicopter stops.
heli_fly_inbound( route )
{
	for ( i = 1; i < route.inbound.size; i++ )
		self heli_fly_to( route.inbound[ i ], i == route.inbound.size - 1 );
}

heli_fly_outbound( route )
{
	for ( i = 0; i < route.outbound.size; i++ )
		self heli_fly_to( route.outbound[ i ], i == route.outbound.size - 1 );
}

heli_fly_to( node, stop )
{
	if ( !isDefined( self ) )
		return;

	if ( is_true( self.zm_vehicle ) )
	{
		self setGoalYaw( struct_angles( node )[ 1 ] );
		self setVehGoalPos( node.origin, stop );
		self heli_wait_goal( node, stop );
		return;
	}

	dist = distance( self.origin, node.origin );
	time = dist / HELI_UPS;
	if ( time < 0.5 )
		time = 0.5;
	accel = 0;
	decel = 0;
	if ( stop )
		decel = time * 0.4;
	self moveTo( node.origin, time, accel, decel );
	self rotateTo( struct_angles( node ), time * 0.8 );
	wait( time );
}

// "goal" (or "near_goal" in passing) -- with a time-out, so a goal the
// vehicle cannot reach never stalls the rescue.
heli_wait_goal( node, stop )
{
	self endon( "death" );

	dist = distance( self.origin, node.origin );
	limit = dist / ( HELI_SPEED * 17.6 ) * 2.5 + 4;
	end = getTime() + int( limit * 1000 );

	if ( !stop )
		self setNearGoalNotifyDist( 300 );

	for ( ;; )
	{
		msg = self waittill_any_timeout_local( 0.25, "goal", "near_goal" );
		if ( msg == "goal" || ( !stop && msg == "near_goal" ) )
			return;
		if ( getTime() > end )
		{
			codolprintln( "CoDOL zm extraction: the helicopter did not reach " + node.origin + " in " + limit + " s; moving on" );
			return;
		}
	}
}

waittill_any_timeout_local( timeout, a, b )
{
	self endon( "death" );
	ent = spawnStruct();
	self thread waittill_string_local( a, ent );
	self thread waittill_string_local( b, ent );
	ent thread timeout_local( timeout );
	ent waittill( "returned", msg );
	ent notify( "die" );
	return msg;
}

waittill_string_local( msg, ent )
{
	self endon( "death" );
	ent endon( "die" );
	self waittill( msg );
	ent notify( "returned", msg );
}

timeout_local( delay )
{
	self endon( "die" );
	wait( delay );
	self notify( "returned", "timeout" );
}

// --------------------------------------------------------------------------
// Over the landing zone: the marker, the timer, boarding.
// --------------------------------------------------------------------------

extraction_wait( route, heli )
{
	level endon( "end_game" );

	lz = route.lz;
	level.zm_extraction_lz = lz;

	// The marker: the flare in the landing zone and the waypoint over it.
	level.zm_lz_flare = spawnFx( level._effect[ "zm_lz_flare" ], lz );
	triggerFx( level.zm_lz_flare );
	level.zm_lz_sound = spawn( "script_origin", lz );
	level.zm_lz_sound playLoopSound( "sh_env_roadflare_burn" );

	wp = newHudElem();
	wp.x = lz[ 0 ];
	wp.y = lz[ 1 ];
	wp.z = lz[ 2 ] + 72;
	wp.alpha = 1;
	wp.hidewheninmenu = true;
	wp setShader( "waypoint_extraction", 8, 8 );
	wp setWaypoint( true, true );
	level.zm_lz_waypoint = wp;

	// "Go to the extraction point" and "Rescue team leaving: m:ss".
	objective = newHudElem();
	objective.alignX = "center";
	objective.alignY = "middle";
	objective.horzAlign = "center";
	objective.vertAlign = "top";
	objective.x = 0;
	objective.y = 80;
	objective.font = "objective";
	objective.fontScale = 1.6;
	objective.color = ( 1, 0.85, 0.3 );
	objective.glowColor = ( 0.6, 0.3, 0.05 );
	objective.glowAlpha = 0.5;
	objective.foreground = true;
	objective.hidewheninmenu = true;
	objective setText( &"ZOMBIE_RESCUE_OBJECTIVE" );
	level.zm_lz_objective = objective;

	timer = newHudElem();
	timer.alignX = "center";
	timer.alignY = "middle";
	timer.horzAlign = "center";
	timer.vertAlign = "top";
	timer.x = 0;
	timer.y = 100;
	timer.font = "objective";
	timer.fontScale = 1.6;
	timer.color = ( 1, 1, 1 );
	timer.foreground = true;
	timer.hidewheninmenu = true;
	timer.label = &"ZOMBIE_RESCUE_LEAVING";
	timer setTimer( level.zm_extract_wait );
	level.zm_lz_timer = timer;

	// Clear any personal notice still showing over the objective.
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] notify( "zm_rescue_notice" );

	radio_all( "zmby_jump_chpr", 1 );

	array_thread_1( getPlayers(), ::board_watch );

	deadline = getTime() + int( level.zm_extract_wait * 1000 );
	warned = false;
	all_aboard_since = undefined;

	while ( getTime() < deadline )
	{
		wait( 0.25 );

		if ( !warned && deadline - getTime() <= 30000 )
		{
			warned = true;
			level thread radio_all( "zmby_30seconds_hq_r", 0 );
		}

		// Everyone who can still move is aboard: go (after a beat, so a
		// teammate a step away is not stranded by a single frame).
		if ( all_valid_extracted() )
		{
			if ( !isDefined( all_aboard_since ) )
				all_aboard_since = getTime();
			else if ( getTime() - all_aboard_since >= 3000 )
				break;
		}
		else
			all_aboard_since = undefined;
	}

	// The landing zone at lift-off: everyone standing in it gets out.
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( is_true( p.zm_extracted ) || !is_player_valid( p ) )
			continue;
		if ( in_landing_zone( p, route, heli ) )
			p board( heli, "in the landing zone at lift-off" );
	}

	level notify( "zm_extraction_liftoff" );
	extraction_hud_clear();
}

all_valid_extracted()
{
	players = getPlayers();
	any = false;
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( is_true( p.zm_extracted ) )
		{
			any = true;
			continue;
		}
		if ( is_player_valid( p ) )
			return false;
	}
	return any;
}

in_landing_zone( p, route, heli )
{
	if ( distance2D( p.origin, route.lz ) <= LZ_RADIUS && abs_f( p.origin[ 2 ] - route.lz[ 2 ] ) <= 160 )
		return true;
	if ( isDefined( heli ) && distance2D( p.origin, heli.origin ) <= BOARD_RADIUS && abs_f( p.origin[ 2 ] - route.lz[ 2 ] ) <= 160 )
		return true;
	return false;
}

abs_f( v )
{
	if ( v < 0 )
		return 0 - v;
	return v;
}

// self = player.  C1's hint: "sprint and press jump to jump onto the rescue
// aircraft".  The jump is the boarding; being near is enough to show the
// hint.
board_watch()
{
	self endon( "disconnect" );
	level endon( "zm_extraction_liftoff" );
	level endon( "end_game" );

	route = level.zm_extraction_route;
	heli = level.zm_rescue_heli;
	hint = undefined;

	for ( ;; )
	{
		wait( 0.05 );

		if ( is_true( self.zm_extracted ) )
			break;

		near = is_player_valid( self ) && in_landing_zone( self, route, heli );
		if ( near && !isDefined( hint ) )
			hint = self board_hint();
		else if ( !near && isDefined( hint ) )
		{
			hint destroy();
			hint = undefined;
		}

		if ( near && self jumpButtonPressed() )
		{
			self board( heli, "jumped aboard" );
			break;
		}
	}

	if ( isDefined( hint ) )
		hint destroy();
}

board_hint()
{
	elem = newClientHudElem( self );
	elem.alignX = "center";
	elem.alignY = "middle";
	elem.horzAlign = "center";
	elem.vertAlign = "middle";
	elem.x = 0;
	elem.y = 60;
	elem.font = "objective";
	elem.fontScale = 1.4;
	elem.color = ( 1, 1, 1 );
	elem.foreground = true;
	elem.hidewheninmenu = true;
	elem setText( &"ZOMBIE_RESCUE_HINT" );
	return elem;
}

// self = player.  Aboard: out of the fight for the horde (the native
// acquisition honours setDontTarget, as the last stand relies on), immune
// (_zombiemode::Callback_PlayerDamage), and carried by the helicopter.
board( heli, why )
{
	if ( is_true( self.zm_extracted ) )
		return;

	self mark_extracted();

	if ( isDefined( heli ) )
	{
		if ( !isDefined( heli.riders ) )
			heli.riders = [];
		seat = heli.riders.size;
		heli.riders[ seat ] = self;

		// The cabin: riders sit side by side behind the cockpit.
		offset = ( -40 - 24 * ( seat % 2 ), 30 - 60 * ( seat % 2 ), -40 );
		self setOrigin( heli.origin + offset );
		self linkTo( heli, "tag_origin", offset, ( 0, 0, 0 ) );
	}

	codolprintln( "CoDOL zm extraction: " + self.name + " is aboard (" + why + ")" );
}

mark_extracted()
{
	self.zm_extracted = true;
	self.ignoreme = true;
	self setDontTarget( true );
}

extraction_hud_clear()
{
	if ( isDefined( level.zm_lz_waypoint ) )
		level.zm_lz_waypoint destroy();
	if ( isDefined( level.zm_lz_objective ) )
		level.zm_lz_objective destroy();
	if ( isDefined( level.zm_lz_timer ) )
		level.zm_lz_timer destroy();
	if ( isDefined( level.zm_lz_flare ) )
		level.zm_lz_flare delete();
	if ( isDefined( level.zm_lz_sound ) )
	{
		level.zm_lz_sound stopLoopSound();
		level.zm_lz_sound delete();
	}
}

// --------------------------------------------------------------------------
// Lift-off and the outcome.
// --------------------------------------------------------------------------

extraction_leave( route, heli )
{
	level endon( "end_game" );

	codolprintln( "CoDOL zm extraction: lift-off with " + count_extracted() + " aboard" );
	radio_all( "zmby_liftoff1_chpr", 0 );

	// The ride out; the match ends a few seconds into it.
	heli thread heli_fly_outbound( route );
	wait( 6 );
}

count_extracted()
{
	n = 0;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( is_true( players[ i ].zm_extracted ) )
			n++;
	}
	return n;
}

// The end: C1's outcome splash per player, the team result for the match
// report (a rescue is a win: allies 1, axis 0; codol_match_report.c derives
// each row's outcome from the team scores), then _zombiemode's end of game.
extraction_finish()
{
	extracted = count_extracted();
	players = getPlayers();

	if ( extracted > 0 )
	{
		left = players.size - extracted;
		level.zm_extraction_result = "win";
		setTeamScore( "allies", 1 );
		setTeamScore( "axis", 0 );

		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( is_true( p.zm_extracted ) )
			{
				if ( left > 0 )
					p ShowHudSplash( "zrescue_exfil", 0 );
				else
					p ShowHudSplash( "zrescue_all_exfil", 0 );
			}
			else
				p ShowHudSplash( "zrescue_left", 0 );
		}

		level thread maps\mp\gametypes\_zombiemode_audio::change_zombie_music( "victory" );
		codolprintln( "CoDOL zm extraction: WIN -- " + extracted + " extracted, " + left + " left behind, on round " + level.round_number );
	}
	else
	{
		level.zm_extraction_result = "lost";
		codolprintln( "CoDOL zm extraction: nobody made it aboard; game over" );
	}

	wait( 1 );
	level notify( "end_game" );
}
