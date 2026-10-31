// zm extraction: survive the tier's waves, then reach the rescue helicopter.
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
//   rounds 1 .. cap  the survival waves, with a final-round warning on
//                      the last wave.
//   after round cap    extraction begins, with C1's MPUI_FINAL_ROUND HUD;
//                      zombies keep coming, a character radios for evac,
//                      HQ answers, and three minutes later the
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
// Dvars: scr_zm_roundcap overrides the survival round count (default 9/13/17
// for Easy/Medium/Hard; 0 = endless rounds, no extraction),
// scr_zm_extract_arrive (seconds from the call to the landing, default 180),
// scr_zm_extract_wait (seconds the helicopter waits, default 60).

#include maps\_zombiemode_utility;
#using_animtree( "player_3rd_person" );

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
	precacheModel( "tag_origin" );

	level._effect[ "zm_lz_flare" ] = loadfx( "smoke/flare_smoke" );

	level.zm_extract_arrive = dvar_seconds( "scr_zm_extract_arrive", 180 );
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
	{}

	level thread rescue_round_notices();
	level thread rescue_arrival_end_game();
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
	// A map whose own rescue structs are unusable flies a route written here.
	routes = map_routes();
	if ( isDefined( routes ) )
		return routes;

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
	chain = [];
	node = start;
	for ( guard = 0; isDefined( node ) && guard < 32; guard++ )
	{
		chain[ chain.size ] = node;
		if ( !isDefined( node.target ) )
			break;
		node = get_struct( node.target, "targetname" );
	}
	return route_from_chain( dest, chain );
}

// chain: the helicopter's nodes in flight order, the hover point marked
// script_noteworthy "stop".
route_from_chain( dest, chain )
{
	route = spawnStruct();
	route.dest = dest;
	route.inbound = [];
	route.outbound = [];
	route.hover = undefined;

	for ( i = 0; i < chain.size; i++ )
	{
		node = chain[ i ];
		if ( !isDefined( route.hover ) )
		{
			route.inbound[ route.inbound.size ] = node;
			if ( isDefined( node.script_noteworthy ) && node.script_noteworthy == "stop" )
				route.hover = node;
		}
		else
			route.outbound[ route.outbound.size ] = node;
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

// Routes written here, for maps whose rescue structs cannot be flown.
// undefined: use the map's own.
map_routes()
{
	switch ( getDvar( "mapname" ) )
	{
	case "zm_sumpf_sh":
		return sumpf_routes();
	}
	return undefined;
}

// Shi No Numa.  The map's rescue_dest and helicopter structs are Prototype's
// prefab with Prototype's coordinates (rescue_dest (-224, -32, 216) and
// (1024, 704, 216), the helicopter from (955, -4536, 216)): 4,000 units west
// of the swamp, over the void at the bottom of the world.  The rescue here
// lands at the center building's north-east balcony: the upstairs deck
// (center_building_upstairs_buy, z -528, railing at x 10790).  The
// helicopter starts high over the open water south-east of the building,
// inside the world (its ceiling is z 3128), comes down over that water
// (no canopy above (11200..11700, -300..650)), stops, slides north under
// the canopy and hovers beside the deck, nose north, its left door at the
// railing.  It leaves the same way.  The hover point is clear of the roof,
// the railing and the trees for the rotor's 320-unit radius (bulletTrace
// rays at the rotor's height); the leaning trunk over the deck's east side
// is at z +130..+240, far above it.
sumpf_routes()
{
	dest = route_point( ( 10740, 1120, -500 ), 0 );

	chain = [];
	chain[ chain.size ] = route_point( ( 11900, -150, 1600 ), 180 );	// above the south-east water
	descend = route_point( ( 11200, -150, 300 ), 105 );				// down over the open water
	descend.zm_stop = true;
	chain[ chain.size ] = descend;
	approach = route_point( ( 11000, 550, -330 ), 100 );				// under the canopy, south of the deck
	approach.zm_speed = 25;
	approach.zm_stop = true;
	chain[ chain.size ] = approach;
	hover = route_point( ( 10900, 1160, -400 ), 90 );					// beside the balcony
	hover.script_noteworthy = "stop";
	hover.zm_speed = 12;
	chain[ chain.size ] = hover;
	leave = route_point( ( 11000, 550, -300 ), 280 );					// back out under the canopy
	leave.zm_speed = 20;
	chain[ chain.size ] = leave;
	chain[ chain.size ] = route_point( ( 11200, -150, 400 ), 285 );	// over the water
	chain[ chain.size ] = route_point( ( 11900, -150, 1600 ), 0 );		// and up

	routes = [];
	route = route_from_chain( dest, chain );
	if ( isDefined( route ) )
	{
		// The deck is behind the upstairs door: used whenever its zone is open.
		route.zm_priority = 1;
		routes[ routes.size ] = route;
	}

	// The start room's landing zone, for a rescue before that door is bought:
	// inside the room at the east window of the south wall (window
	// (10326, 221, -502)), the helicopter hovering just outside it over the
	// south walkway, nose west, its right door to the window.  It comes in
	// from the east along the building's south side and leaves the same way.
	dest = route_point( ( 10326, 320, -500 ), 0 );
	chain = [];
	chain[ chain.size ] = route_point( ( 11900, -150, 1600 ), 180 );	// above the south-east water
	descend = route_point( ( 11200, -150, 300 ), 165 );				// down over the open water
	descend.zm_stop = true;
	chain[ chain.size ] = descend;
	approach = route_point( ( 10700, 90, -380 ), 180 );				// south-east corner of the building
	approach.zm_speed = 25;
	approach.zm_stop = true;
	chain[ chain.size ] = approach;
	hover = route_point( ( 10325, 100, -400 ), 180 );					// outside the window
	hover.script_noteworthy = "stop";
	hover.zm_speed = 12;
	chain[ chain.size ] = hover;
	leave = route_point( ( 10700, 90, -340 ), 0 );
	leave.zm_speed = 20;
	chain[ chain.size ] = leave;
	chain[ chain.size ] = route_point( ( 11200, -150, 400 ), 330 );	// over the water
	chain[ chain.size ] = route_point( ( 11900, -150, 1600 ), 0 );		// and up
	route = route_from_chain( dest, chain );
	if ( isDefined( route ) )
		routes[ routes.size ] = route;
	return routes;
}

route_point( origin, yaw )
{
	s = spawnStruct();
	s.origin = origin;
	s.angles = ( 0, yaw, 0 );
	return s;
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

		// First wave and a documented midpoint adaptation (ceil(cap/2)).
		// The current survival wave is excluded from the remaining count.
		middle = int( ( cap + 1 ) / 2 );
		if ( level.round_number != 1 && level.round_number != middle )
			continue;
		remaining = cap - level.round_number;
		if ( remaining > 1 )
			notice_all( &"ZOMBIE_ROUNDS_REMAINING", remaining );
		else if ( remaining == 1 )
			notice_all( &"ZOMBIE_ROUNDS_FINAL" );
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
	level endon( "end_game" );

	// One notice at a time: a newer one replaces the one showing.
	self notify( "zm_rescue_notice" );
	self endon( "zm_rescue_notice" );

	elem = newClientHudElem( self );
	elem.alignX = "center";
	elem.alignY = "middle";
	elem.horzAlign = "center";
	elem.vertAlign = "top";
	elem.x = 0;
	elem.y = 25;
	elem.font = "objective";
	elem.fontScale = 1.6;
	elem.color = ( 1, 1, 1 );
	elem.glowColor = ( 0.35, 0.65, 1 );
	elem.glowAlpha = 0.5;
	elem.foreground = true;
	elem.hidewheninmenu = true;
	elem.alpha = 1;
	if ( isDefined( value ) )
	{
		elem.label = text;
		elem setValue( value );
	}
	else
		elem setText( text );

	self thread notice_destroy_on_replace( elem );

	// Native typewriter fields, not an alpha-only approximation.
	elem setPulseFX( 40, 4500, 1000 );
	wait( 5.5 );
	self notify( "zm_rescue_notice_done" );
}

notice_destroy_on_replace( elem )
{
	self waittill_either( "zm_rescue_notice", "zm_rescue_notice_done" );
	if ( isDefined( elem ) )
		elem destroy();
}

// --------------------------------------------------------------------------
// The extraction.  _zombiemode::round_think hands over at the start of the
// extraction round; it does not return: it ends in "end_game".
// --------------------------------------------------------------------------

extraction_start()
{
	level endon( "end_game" );

	if ( is_true( level.zm_extraction_active ) )
		return;
	level.zm_extraction_active = true;
	// Explicit final phase: ordinary start_of_round intentionally is not sent.
	notice_all( &"ZOMBIE_RESCUE_BEGIN" );
	level notify( "zm_extraction_start" );

	{}

	if ( level.zm_extraction_routes.size < 1 )
	{
		// A map with no rescue route still finishes as a win: there is nowhere
		// to be picked up, so everyone standing is extracted on the spot.
		{}
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( is_player_valid( players[ i ] ) )
				players[ i ] mark_extracted();
		}
		record_extracted_results();
		extraction_finish();
		return;
	}

	route = pick_route();
	level.zm_extraction_route = route;
	{}

	// The horde does not stop for the rescue.
	level thread extraction_horde();

	// Reserve the final phase without playing its rescue track early.
	maps\_zombiemode_audio::change_zombie_music( "rescue_wait" );
	level thread evac_call_vo();

	level.zm_rescue_arrival_deadline = getTime() + int( level.zm_extract_arrive * 1000 );
	rescue_arrival_hud();
	level thread rescue_arrival_radio();
	level thread rescue_countdown_beeps( level.zm_rescue_arrival_deadline, "zm_rescue_arrived" );
	// The helicopter's own flight is part of the wait: it is launched so it
	// reaches the hover point at the announced time.
	flight = route_flight_time( route );
	pre = level.zm_extract_arrive - flight;
	if ( pre > 0 )
		wait( pre );

	heli = spawn_rescue_heli( route );
	level.zm_rescue_heli = heli;
	rescue_landing_waypoint( route );
	radio_all( "zmby_lz_01_chpr", 0 );

	heli heli_fly_inbound( route );

	rescue_arrival_clear();
	{}
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
		zone = maps\_zombiemode_zone_manager::zone_at_point( level.zm_extraction_routes[ i ].lz );
		if ( isDefined( zone ) && zone.enabled )
			open[ open.size ] = level.zm_extraction_routes[ i ];
	}
	if ( open.size > 0 )
	{
		// A route may outrank the others (zm_priority) while its zone is open.
		best = [];
		top = 0;
		for ( i = 0; i < open.size; i++ )
		{
			prio = route_priority( open[ i ] );
			if ( best.size == 0 || prio > top )
			{
				best = [];
				top = prio;
			}
			if ( prio == top )
				best[ best.size ] = open[ i ];
		}
		return best[ randomInt( best.size ) ];
	}
	return level.zm_extraction_routes[ randomInt( level.zm_extraction_routes.size ) ];
}

route_priority( route )
{
	if ( isDefined( route.zm_priority ) )
		return route.zm_priority;
	return 0;
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
	level.round_spawn_func = maps\_zombiemode_spawner::round_spawning;
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
		callers[ randomInt( callers.size ) ] maps\_zombiemode_audio::create_and_play_dialog( "evac_call", true );
	}

}

// One authoritative arrival deadline drives the persistent native M:SS HUD
// and once-only shipped HQ cues. Explicit short test overrides are preserved:
// thresholds above the configured duration are never announced.
rescue_arrival_hud()
{
	timer = newHudElem();
	timer.horzAlign = "center";timer.vertAlign = "top";
	timer.alignX = "center";timer.alignY = "top";
	timer.x = 0;timer.y = 0;
	timer.font = "objective";timer.fontScale = 1.4;
	timer.foreground = true;timer.hidewheninmenu = true;
	timer.label = &"ZOMBIE_RESCUE_DISTANCE";
	timer setTimer( level.zm_extract_arrive );
	level.zm_rescue_arrival_hud = timer;
}

// Deadline-driven milestones: no 3:00 rescue music, no stale catch-up burst.
// Explicit shortened local durations skip milestones they never traversed.
rescue_arrival_radio()
{
 level endon( "end_game" );
 level endon( "zm_rescue_arrived" );
 thresholds = [];
 thresholds[0] = 180;
 thresholds[1] = 120;
 thresholds[2] = 110;
 thresholds[3] = 90;
 thresholds[4] = 60;
 thresholds[5] = 30;
 for ( i = 0; i < thresholds.size; i++ )
 {
  second = thresholds[i];
  if ( level.zm_extract_arrive < second ) continue;
  at = level.zm_rescue_arrival_deadline - second * 1000;
  if ( second == 180 ) at += 4000;
  while ( getTime() < at ) wait( 0.05 );
  // A long frame must not replay several already elapsed radio/music cues.
  if ( getTime() - at >= 1000 ) continue;
  rescue_arrival_event( second );
 }
}

rescue_arrival_event( second )
{
 switch ( second )
 {
  case 180: radio_all( "zmby_evac3mins_hq_r", 0 );break;
  case 120: radio_all( "zmby_evac2mins_hq_r", 0 );break;
  case 110: maps\_zombiemode_audio::change_zombie_music( "rescue" );break;
  case 90: radio_all( "zmby_longer_hq_r", 0 );break;
  case 60: radio_all( "zmby_evac1min_hq_r", 0 );break;
  case 30: radio_all( "zmby_30seconds_hq_r", 0 );break;
 }
}

// Same real local-sound transport and positive-second policy as the downed
// Game Over timer. One tick per observed decrement, never a catch-up burst,
// initial-value beep, zero beep, or sound after its owning phase has ended.
rescue_countdown_beeps( deadline, stop_event )
{
 level endon( "end_game" );
 level endon( stop_event );
 last_second = int( ( deadline - getTime() + 999 ) / 1000 );
 if ( !maps\_zombiemode_audio::alias_exists( "ui_mp_timer_countdown" ) ) return;
 while ( getTime() < deadline )
 {
  second = int( ( deadline - getTime() + 999 ) / 1000 );
  if ( second < last_second && second > 0 )
  {
   last_second = second;
   foreach ( player in getPlayers() )
    player playLocalSound( "ui_mp_timer_countdown" );
  }
  wait( 0.05 );
 }
}

rescue_arrival_clear()
{
	level notify( "zm_rescue_arrived" );
	if ( isDefined( level.zm_rescue_arrival_hud ) )
		level.zm_rescue_arrival_hud destroy();
	level.zm_rescue_arrival_hud = undefined;
}

rescue_arrival_end_game()
{
	level waittill( "end_game" );
	rescue_arrival_clear();
	extraction_hud_clear();
	foreach ( player in getPlayers() )
	{
		player notify( "zm_rescue_notice" );
	}
}

// The radio: a 2D line on every player.  Aliases that a zone does not carry
// are skipped (_zombiemode_audio's own check).
radio_all( alias, delay )
{
	if ( isDefined( delay ) && delay > 0 )
		wait( delay );
	if ( !maps\_zombiemode_audio::alias_exists( alias ) )
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
HELI_DECEL = 30;	// mph/s; the native default (half the acceleration) drifts past a hover point
HELI_UPS = 800;		// units/s, the mover's cruise; ~ HELI_SPEED * 17.6

route_flight_time( route )
{
	total = 0;
	for ( i = 1; i < route.inbound.size; i++ )
		total += distance( route.inbound[ i - 1 ].origin, route.inbound[ i ].origin ) / node_ups( route.inbound[ i ] );
	return total + 2;
}

// A node may set the speed of the leg flown to it (zm_speed, mph) and stop
// there before going on (zm_stop): a slow, straight final approach keeps the
// vehicle's momentum from carrying it past a hover point in tight spots.
node_speed( node )
{
	if ( isDefined( node.zm_speed ) )
		return node.zm_speed;
	return HELI_SPEED;
}

node_ups( node )
{
	return HELI_UPS * node_speed( node ) / HELI_SPEED;
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
			heli Vehicle_SetSpeed( HELI_SPEED, HELI_ACCEL, HELI_DECEL );
			heli setYawSpeed( 60, 30 );
			heli setMaxPitchRoll( 15, 15 );
			heli setHoverParams( 20, 10, 5 );
			heli setCanDamage( false );
			{}
		}
		else
			{}
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
	heli.zm_boarding_models = [];
	heli thread rescue_rider_lifecycle();
	level thread rescue_riders_end_game( heli );
	if ( heli.zm_vehicle )
		heli thread rescue_gun_fire();
	return heli;
}

// Fly a chain of structs.  The last node of the inbound chain is the hover
// point, where the helicopter stops.
heli_fly_inbound( route )
{
	for ( i = 1; i < route.inbound.size; i++ )
		self heli_fly_to( route.inbound[ i ], i == route.inbound.size - 1 );
	if ( getDvar( "mapname" ) == "zm_factory_sh" )
		self factory_wait_landing_frame( route );
}

// Factory's positional goal can arrive with the cabin still turning (the
// measured B route was 130 degrees short of its authored hover yaw). Start
// the boarding window only once that door faces the actual landing marker.
// Native flight and authored coordinates stay unchanged; a failed settle
// has a bounded wait and a single operator diagnostic, never a hung rescue.
factory_wait_landing_frame( route )
{
	self endon( "death" );
	level endon( "end_game" );
	marker = spawnStruct();
	marker.origin = route.lz;
	yaw = struct_angles( route.hover )[ 1 ];
	deadline = getTime() + 5000;
	for ( ;; )
	{
		delta = self.angles[ 1 ] - yaw;
		while ( delta > 180 )
			delta -= 360;
		while ( delta < -180 )
			delta += 360;
		if ( abs_f( delta ) <= 3 && in_landing_zone( marker, route, self ) )
			return;
		if ( getTime() >= deadline )
		{
			codolprintln( "CoDOL zm Factory rescue: landing alignment did not settle within 5 seconds" );
			return;
		}
		wait( 0.05 );
	}
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

	if ( is_true( node.zm_stop ) )
		stop = true;

	if ( is_true( self.zm_vehicle ) )
	{
		self Vehicle_SetSpeed( node_speed( node ), HELI_ACCEL, HELI_DECEL );
		self setGoalYaw( struct_angles( node )[ 1 ] );
		self setVehGoalPos( node.origin, stop );
		self heli_wait_goal( node, stop );
		return;
	}

	dist = distance( self.origin, node.origin );
	time = dist / node_ups( node );
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
	limit = dist / ( node_speed( node ) * 17.6 ) * 2.5 + 4;
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
			{}
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

rescue_landing_waypoint( route )
{
 if ( isDefined( level.zm_lz_waypoint ) ) return;
 lz = route.lz;
	wp = newHudElem();
	wp.x = lz[ 0 ];
	wp.y = lz[ 1 ];
	wp.z = lz[ 2 ] + 72;
	wp.alpha = 1;
	wp.hidewheninmenu = true;
	wp setShader( "waypoint_extraction", 8, 8 );
	wp setWaypoint( true, true );
	level.zm_lz_waypoint = wp;
}

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

	rescue_landing_waypoint( route );

	// "Go to the extraction point" and "Rescue team leaving: m:ss".
	objective = newHudElem();
	objective.alignX = "center";
	objective.alignY = "middle";
	objective.horzAlign = "center";
	objective.vertAlign = "top";
	objective.x = 0;
	objective.y = 0;
	objective.font = "objective";
	objective.fontScale = 1.6;
	objective.color = ( 1, 1, 1 );
	objective.glowColor = ( 0.35, 0.65, 1 );
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
	timer.y = 22;
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
	level thread rescue_countdown_beeps( deadline, "zm_extraction_liftoff" );
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
			p thread board( heli, "in the landing zone at lift-off" );
	}
	// A boarding clip already started at the deadline gets its authored time;
	// lift-off must not cut the longest 5.333-second get-in animation in half.
	while ( rescue_boardings_pending( heli ) )
		wait( 0.05 );

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
		if ( is_true( p.zm_boarding ) )
			return false;
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

rescue_entry_frame( heli )
{
 if ( !isDefined( heli ) ) return undefined;
 rear = heli getTagOrigin( "side_door_r_jnt" );
 front = heli getTagOrigin( "tag_gun_r" );
 left = heli getTagOrigin( "side_door_l_jnt" );
 up = anglesToUp( heli.angles );
 outward = rear - left;
 outward -= up * vectorDot( outward, up );
 if ( distance( outward, ( 0, 0, 0 ) ) < 1 ) return undefined;
 outward = vectorNormalize( outward );
 along = front - rear;
 along -= up * vectorDot( along, up );
 along -= outward * vectorDot( along, outward );
 length = distance( along, ( 0, 0, 0 ) );
 if ( length < 1 ) return undefined;
 entry = spawnStruct();entry.rear = rear;entry.outward = outward;
 entry.along = vectorNormalize( along );entry.length = length;
 return entry;
}

in_landing_zone( p, route, heli )
{
 entry = rescue_entry_frame( heli );
 if ( !isDefined( entry ) ) return false;
 delta = p.origin - entry.rear;
 along = vectorDot( delta, entry.along );
 outside = vectorDot( delta, entry.outward );
 // Factory's authored route A marker is 19 units behind the mounted
 // right door's rear plane (actual native tags/ground trace, 2026-10-31).
 // Allow one player-width at either end on this map so its own landing
 // marker can board. Keep the exterior half-space, range and height gates;
 // all other maps retain their exact door span and underbody rejection.
 margin = 0;
 if ( getDvar( "mapname" ) == "zm_factory_sh" )
  margin = 32;
 return along >= 0 - margin && along <= entry.length + margin && outside >= 0 && outside <= BOARD_RADIUS &&
  abs_f( p.origin[ 2 ] - route.lz[ 2 ] ) <= 160;
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
		// Positive wait rounds to exactly one VM frame at supported sv_fps1..250.
		wait( 0.001 );

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

// Four distinct cabin locations on vehicle_blackhawk_minigun_hero.
// The mounted LOD0 mesh has its inner cabin floor at z -115.18 for the
// front pair and -115.63 for the rear pair; tag_guy1/3/7/9 sit at -96.638.
// tag_player is a minigun attachment at -123.758, BELOW that floor, and
// cannot be used as the passenger's feet. These offsets are in the seat
// tag's frame; the native link carries the complete rotated transform.
rescue_seat_tag( seat )
{
	tags = strTok( "tag_guy1 tag_guy3 tag_guy7 tag_guy9", " " );
	if ( seat < 0 || seat >= tags.size )
		return undefined;
	return tags[ seat ];
}

rescue_seat_offset( heli, tag )
{
	if ( tag == "tag_guy1" || tag == "tag_guy3" )
		return ( 0, 0, -18.542 );
	return ( 0, 0, -18.992 );
}

// Keep the measured cabin stance while retaining mouse/attack input. Freezing
// controls here also froze the player's aim and the helicopter gunner's input.
rescue_seat_controls()
{
	self endon( "disconnect" );
	level endon( "end_game" );
	self allowJump( false );
	self allowSprint( false );
	self allowCrouch( true );
	self allowStand( false );
	self allowProne( false );
	self setStance( "crouch" );
	self freezeControls( false );
}

// self = player.  Aboard: out of the fight for the horde (the native
// acquisition honours setDontTarget, as the last stand relies on), immune
// (_zombiemode::Callback_PlayerDamage), and carried by the helicopter.
board( heli, why )
{
	if ( is_true( self.zm_extracted ) || is_true( self.zm_boarding ) )
		return;

	if ( isDefined( heli ) )
	{
		if ( !isDefined( heli.riders ) )
			heli.riders = [];
		seat = rescue_free_seat( heli );
		tag = rescue_seat_tag( seat );
		if ( !isDefined( tag ) )
		{
			{}
			return;
		}

		// The boarding/turret path holsters the current weapon. Retain only
		// the genuinely held, owned primary for the later character clone.
		self.zm_rescue_held_primary = undefined;
		held_primary = self getCurrentPrimaryWeapon();
		if ( isDefined( held_primary ) && held_primary != "" && held_primary != "none" && self hasWeapon( held_primary ) )
			self.zm_rescue_held_primary = held_primary;

		offset = rescue_seat_offset( heli, tag );
		angles = heli getTagAngles( tag );
		// Initial placement and attachment use the same tag-local transform,
		// including pitch/roll. Do not add a local offset to a world origin.
		self setOrigin( heli getTagOrigin( tag ) + anglesToUp( angles ) * offset[ 2 ] );
		self setPlayerAngles( angles );
		// Preserve the measured feet-to-tag delta and carry the camera with
		// helicopter turns. Generic linkTo only updates the linked origin.
		self playerLinkToDelta( heli, tag, 1, 180, 180, 180, 180, true );
		heli.riders[ seat ] = self;
		self.zm_rescue_heli = heli;
		self.zm_rescue_seat = seat;
		self.zm_boarding = true;
		// Protect the actual player through the real boarding animation. The
		// all-aboard check above still waits for zm_boarding to clear.
		self mark_extracted();
		self rescue_boarding_animation( heli, seat, tag, offset );
		self.zm_boarding = false;
		self thread rescue_seat_controls();
		self rescue_mount_gun( heli );
		{}
	}
	self mark_extracted();

	{}
}

rescue_free_seat( heli )
{
	for ( i = 0; i < 4; i++ )
		if ( i >= heli.riders.size || !isDefined( heli.riders[ i ] ) )
			return i;
	return -1;
}

rescue_boardings_pending( heli )
{
	if ( !isDefined( heli ) )
		return false;
	foreach ( rider in heli.riders )
		if ( isDefined( rider ) && is_true( rider.zm_boarding ) )
			return true;
	return false;
}

rescue_getin_anim( seat )
{
	switch ( seat )
	{
		case 0: return %zombie_exfil_heli_getin_01;
		case 1: return %zombie_exfil_heli_getin_02;
		case 2: return %zombie_exfil_heli_getin_03;
		default: return %zombie_exfil_heli_getin_04;
	}
}

// C1 carries these four complete character clips in common_gametype_zm.
// Their getMoveDelta is zero: the jump is authored inside the skeleton,
// including the eye bone's roughly 200-unit approach. Play the clip itself;
// do not replace that bone motion with a linear camera move or a teleport.
rescue_boarding_animation( heli, seat, tag, offset )
{
	self endon( "disconnect" );
	self endon( "death" );
	level endon( "end_game" );
	clip = rescue_getin_anim( seat );
	model = spawn( "script_model", self.origin );
	model setModel( "tag_origin" );
	model notSolid();
	model.weapons = model codol_setCharacterClone( self );
	// Clip01 approaches along +X; clips02..04 approach along -Y.
 // Rotate ONLY01 toward the real right-side gunner heading. Keep existing
 // root translation; this does not claim recovery of the original scene root.
 scene_angles = ( 0, 0, 0 );
 if ( seat == 0 )
 {
  gun_angles = heli getTagAngles( "tag_player" );
  seat_angles = heli getTagAngles( tag );
  scene_angles = ( 0, gun_angles[1] - seat_angles[1], 0 );
 }
 model linkTo( heli, tag, offset, scene_angles );
	heli.zm_boarding_models[ seat ] = model;
	self.zm_boarding_model = model;
	self disableWeapons();
	self freezeControls( true );
	self hide();
	model setAnimKnobRestart( clip, 1, 0, 1 );
	self cameraLinkTo( model, "tag_eye" );
	{}
	wait( getAnimLength( clip ) );
	self cameraUnlink();
	self show();
	self enableWeapons();
	model rescue_remove_boarding_model();
	heli.zm_boarding_models[ seat ] = undefined;
	self.zm_boarding_model = undefined;
}

rescue_remove_boarding_model()
{
	if ( isDefined( self.weapons ) )
		foreach ( weapon in self.weapons )
			if ( isDefined( weapon ) )
				weapon delete();
	self delete();
}

// The authored Black Hawk has one integrated gun, not four overlapping
// sentries. Its VehicleDef owns the real bullet weapon and muzzle tags.
// This is the same native vehicle-turret contract as IW4 _helicopter::heliRide.
rescue_mount_gun( heli )
{
	if ( !isDefined( heli ) || !is_true( heli.zm_vehicle ) || isDefined( heli.zm_rescue_gunner ) )
		return false;
	if ( !isAlive( self ) || is_true( self.zm_boarding ) || !is_true( self.zm_extracted ) )
		return false;
	heli.zm_rescue_gunner = self;
	heli.owner = self;
	self.zm_rescue_gun_active = true;
	self disableWeapons();
	self freezeControls( false );
	self setPlayerAngles( heli getTagAngles( "tag_player" ) );
	heli VehicleTurretControlOn( self );
	// tag_player is the gun attachment BELOW the cabin floor. Weapon-view
 // linking forces the C1 eye onto it; native turret authority does not need
 // that camera mode. Keep the real cabin feet/eye and normal physical aim.
 self cameraUnlink();
 tag = rescue_seat_tag( self.zm_rescue_seat );
 self playerLinkToDelta( heli, tag, 1, 180, 180, 180, 180, true );
	{}
	return true;
}

rescue_gun_fire()
{
	self endon( "death" );
	self endon( "zm_rescue_cleanup" );
	level endon( "end_game" );
	for ( ;; )
	{
		// Produced by Helicopter_Think from the real controller's attack input
		// after the weapon's native refire delay. FireWeapon does normal traces,
		// Bullet_Fire, refire/event/muzzle/audio publication and authoritative damage.
		self waittill( "turret_fire" );
		if ( isDefined( self.zm_rescue_gunner ) && isAlive( self.zm_rescue_gunner ) )
			self fireWeapon();
	}
}

rescue_release_rider( restore_controls )
{
	heli = self.zm_rescue_heli;
	self cameraUnlink();
	if ( isDefined( heli ) && is_true( self.zm_rescue_gun_active ) )
	{
		if ( isDefined( heli.zm_rescue_gunner ) && heli.zm_rescue_gunner == self )
		{
			heli VehicleTurretControlOff( self );
			heli.zm_rescue_gunner = undefined;
			heli.owner = undefined;
		}
	}
	self.zm_rescue_gun_active = false;
	self unlink();
	self show();
	self enableWeapons();
	self allowJump( true );
	self allowSprint( true );
	self allowCrouch( true );
	self allowStand( true );
	self allowProne( true );
	self.zm_boarding = false;
	self.zm_rescue_heli = undefined;
	if ( restore_controls )
	{
		self.zm_rescue_held_primary = undefined;
		self.zm_extracted = false;
		self.ignoreme = false;
		self setDontTarget( false );
		self freezeControls( false );
	}
}

rescue_rider_lifecycle()
{
	self endon( "zm_rescue_cleanup" );
	for ( ;; )
	{
		for ( i = 0; i < self.riders.size; i++ )
		{
			rider = self.riders[ i ];
			if ( isDefined( rider ) && isAlive( rider ) )
				continue;
			if ( isDefined( rider ) )
				rider rescue_release_rider( true );
			if ( i < self.zm_boarding_models.size && isDefined( self.zm_boarding_models[ i ] ) )
				self.zm_boarding_models[ i ] rescue_remove_boarding_model();
			self.zm_boarding_models[ i ] = undefined;
			self.riders[ i ] = undefined;
		}
		// Native ClientLeft also clears ownership on disconnect. Let another
		// living passenger use the one gun rather than retain a dead controller.
		if ( !isDefined( self.zm_rescue_gunner ) )
		{
			self.owner = undefined;
			foreach ( rider in self.riders )
				if ( isDefined( rider ) && rider rescue_mount_gun( self ) )
					break;
		}
		wait( 0.05 );
	}
}

rescue_riders_end_game( heli )
{
	level waittill( "end_game" );
	if ( !isDefined( heli ) )
		return;
	heli notify( "zm_rescue_cleanup" );
	foreach ( rider in heli.riders )
		if ( isDefined( rider ) )
			rider rescue_release_rider( false );
	foreach ( model in heli.zm_boarding_models )
		if ( isDefined( model ) )
			model rescue_remove_boarding_model();
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
	level.zm_lz_waypoint = undefined;
	level.zm_lz_objective = undefined;
	level.zm_lz_timer = undefined;
	level.zm_lz_flare = undefined;
	level.zm_lz_sound = undefined;
}

// --------------------------------------------------------------------------
// Lift-off and the outcome.
// --------------------------------------------------------------------------

extraction_leave( route, heli )
{
	level endon( "end_game" );

	{}
	maps\_zombiemode_spawner::stop_spawning();
	radio_all( "zmby_liftoff1_chpr", 0 );

	// The ride out; the match ends a few seconds into it.
	heli thread heli_fly_outbound( route );
	wait( 6 );
	// The flight's real mounted-gun kills/headshots must precede the native
	// recorder's first immutable extraction event.
	record_extracted_results();
}

record_extracted_results()
{
	// Called at successful lift-off (or the no-helicopter success fallback),
	// before the outbound flight. The native recorder freezes its first event.
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( is_true( players[ i ].zm_extracted ) )
		{
			recorded = players[ i ] codol_reportZombiesResult( true,
				level.zm_round_mode, maps\_zombiemode::report_round_number(), level.zm_round_cap,
				players[ i ].downs, players[ i ].revives, players[ i ].headshots, true );
			{}
		}
	}
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

		level thread maps\_zombiemode_audio::change_zombie_music( "victory" );
		{}
	}
	else
	{
		level.zm_extraction_result = "lost";
		{}
	}

	// Exact C1 win pilot11592,2026-10-19 11:20:19UTC: native
	// ReceivedSplash zrescue_all_exfil slot0 for3.000000sec.
	// Preserve its full stock lifetime before returning to the frontend.
	if ( extracted > 0 )
		wait( 3 );
	else
		wait( 1 );
	level notify( "end_game" );
}
