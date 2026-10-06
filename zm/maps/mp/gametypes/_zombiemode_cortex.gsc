// zm Cortex taunts: now and then the voice that drives the cyborg horde
// speaks through ALL of it at once -- every hunting zombie stops and plays a
// taunt, and every zombie's eyes burn red and mouth blue for the line.
//
// CoDOL-ORIGINAL BEHAVIOUR.  T5 has no counterpart; every name below is read
// from the C1 1.1.1.13 zones, not invented.  MEASURED 2026-10-05:
//
//   VOICE.  localized_common_gametype_zm carries seven reaction alias lists
//   whose entries play voiceovers/_cyborgs/ch/crtx/CORTEX_PERSONALITY_NN.mp3
//   and each entry names its own subtitle (the alias's subtitle field), so C1
//   prints the line itself when the alias plays -- the script sends nothing
//   but the sound:
//
//     alias                    files (secs)                 subtitle keys
//     cortex_reaction_death    32 33 35-40 (1.3-1.9)       ..._REACTION_DEATH_01..08
//     cortex_reaction_door     24 27 30    (1.3-1.9)       ..._REACTION_DOOR_01..03
//     cortex_reaction_power    22          (2.1)           ..._REACTION_POWER
//     cortex_reaction_repair   25 26 28    (1.5-1.8)       ..._REACTION_REPAIR_01..03
//     cortex_reaction_round    31          (1.6)           ..._REACTION_ROUND
//     cortex_reaction_weapon   23 29 34    (1.4-2.2)       ..._REACTION_WEAPON_01..03
//
//   (subtitle prefix SUBTITLE_ZOMBIE_CORTEX_CRTX_; every value starts with
//   "^1<mysterious voice>: ^7" -- e.g. REACTION_REPAIR_02 is "What's the
//   point of doing this?", the line players report seeing).  The
//   aliases are 120/600 distance-falloff sounds, i.e. positional.
//   cortex_backstory and cortex_zm_<map>_0N (2.2-6.4 s, lab/zone intrusion
//   announcements) are deliberately NOT in the taunt pool: they outlast the
//   stop clip and are announcer lines, not reactions.
//
//   ANIMATION.  animlists/common_zombie.animlist (common_gametype_zm, and its
//   patch_common_gametype_zm replacement) lists ai_zombie_taunts_5 and
//   ai_zombie_taunts_2 apart from the eight board taunts.  Both are nodes of
//   the patch_common_mp generic_human.atr the server compiles against.
//   xanim headers: taunts_5 is 96 frames @30 (3.2 s) with NO vocal
//   notetrack; taunts_2 is 80 frames (2.67 s) and carries two
//   sndnt#zmb_zombie_nt_vocals_taunt growls.  Every reaction line is <= 2.2 s.
//
//   FX.  common_gametype_zm ships zombie/eye_glow_red (a recolour of
//   misc/fx_zombie_eye_single / zombie/eye_glow_blue: identical 4-element
//   looping layout) and zombie/mouth_glow_blue.  The c_zom_*_head models
//   carry j_eyeball_le, j_eyeball_ri and tag_mouth.
//
// THE EVENT.  A level thread (cortex_horde_think) paces the events.  When
// one fires it publishes level.cortex_event (id, alias, end time) and:
//   - lays the overlay on every living zombie that still has its head;
//   - wakes every zombie's hunt loop (notify "cortex_wake"), so the stops
//     start together instead of within the next one-second poll;
//   - plays the line ONCE per listener: for each player the nearest zombie
//     within earshot (550) speaks it, and a speaker already in earshot of a
//     player covers that player too, so nobody hears overlapping copies.  A
//     player with no zombie in earshot gets it on their own entity, so every
//     player hears the line exactly once.
// Each zombie decides for itself, on its own zombie_think thread -- the
// thread that owns the goal and the board tear -- whether it may stop
// (cortex_should_join): hunting, legs, not trap-frozen, not ignoreall (a
// window climb), and the native actor reports safeToChangeScript (false
// during a traversal and a melee swing).  A zombie tearing boards or
// following a point of interest never reaches the hunt-loop hook.  One that
// may not stop just glows and keeps doing what it was doing.
//
// THE NORMAL EYES ARE NOT TOUCHED.  The red eyes and blue mouth are an
// overlay: played on top of the spawner's eye glow and stopped again
// (stopFxOnTag on the same effect and tags) when the event ends, when the
// zombie dies, or -- before its head is detached -- by
// _zombiemode_gib::zombie_head_gib.  Owners of the base eye glow can hide it
// for the duration: self.cortex_taunting is true and "cortex_overlay_on" /
// "cortex_overlay_off" are notified on the zombie around the overlay.
//
// THE PAUSE.  The stop is animscripted, i.e. the native T5 AIS_SCRIPTEDANIM
// hold (codol_actor_lifecycle.cpp CODOL_ActorScriptedAnimBegin): the route
// is dropped, velocity is zeroed and mode "normal" applies no route
// translation, so the zombie stands where it is and turns to face the
// player; stopanimscripted pops the state, clears the route and the stuck
// detector, and the next frame's chase replans.  A player who walks up to a
// stopped zombie (90 units) ends that zombie's stop early.
//
// PACING.  scr_zm_cortex_per_round events per round (default 3), 20-40 s
// apart, the first 12-25 s into the round, and only while some hunting
// zombie is within 900 units of a player (someone is there to see it).  A
// player's action (board repair, door/debris, weapon, power, a player going
// down, a new round) queues the matching reaction for the next event and
// pulls it forward.
//
// Dvars: scr_zm_cortex "0" turns it off.  scr_zm_cortex_test "1" (developer
// servers only) fires an event every 8 s with no round cap.

#include maps\mp\gametypes\_zombiemode_utility;

#using_animtree ( "generic_human" );

init()
{
	level._effect[ "cortex_eye_red" ] = loadfx( "zombie/eye_glow_red" );
	level._effect[ "cortex_mouth_blue" ] = loadfx( "zombie/mouth_glow_blue" );

	level.cortex_taunt_anims = [];
	level.cortex_taunt_anims[ 0 ] = %ai_zombie_taunts_5;
	level.cortex_taunt_anims[ 1 ] = %ai_zombie_taunts_2;

	level.cortex_lines = [];
	level.cortex_lines[ "death" ] = "cortex_reaction_death";
	level.cortex_lines[ "door" ] = "cortex_reaction_door";
	level.cortex_lines[ "power" ] = "cortex_reaction_power";
	level.cortex_lines[ "repair" ] = "cortex_reaction_repair";
	level.cortex_lines[ "round" ] = "cortex_reaction_round";
	level.cortex_lines[ "weapon" ] = "cortex_reaction_weapon";

	// The unprompted pool, weighted by how many lines each list holds so no
	// single line repeats much more often than another.  "door" and "power"
	// only make sense right after the event and stay out of it.
	level.cortex_idle_pool = [];
	cortex_pool_add( "death", 4 );
	cortex_pool_add( "weapon", 2 );
	cortex_pool_add( "repair", 2 );
	cortex_pool_add( "round", 1 );

	level.cortex_overlay_tags = [];
	level.cortex_overlay_tags[ 0 ] = "j_eyeball_le";
	level.cortex_overlay_tags[ 1 ] = "j_eyeball_ri";

	// The overlay and the stops last as long as the longer taunt clip
	// (taunts_5, 3.2 s); every reaction line is <= 2.2 s after a 0.3 s lead.
	level.cortex_event_length = 3.2;
	level.cortex_voice_range = 550;

	level.cortex_round_count = 0;
	level.cortex_next_time = 0;
	level.cortex_last_time = 0;
	level.cortex_total = 0;
	level.cortex_event_id = 0;

	level thread cortex_horde_think();
	level thread cortex_round_watch();
	level thread cortex_event_watch();
	level thread cortex_level_notify_watch( "master_switch_activated", "power" );
	level thread cortex_level_notify_watch( "junk purchased", "door" );
	level thread cortex_level_notify_watch( "player_bled_out", "death" );
}

cortex_pool_add( category, weight )
{
	for ( i = 0; i < weight; i++ )
		level.cortex_idle_pool[ level.cortex_idle_pool.size ] = category;
}

cortex_enabled()
{
	return getDvar( "scr_zm_cortex" ) != "0";
}

cortex_test_mode()
{
	return getDvarInt( "developer" ) > 0 && getDvar( "scr_zm_cortex_test" ) == "1";
}

cortex_round_cap()
{
	if ( cortex_test_mode() )
		return 1000;

	cap = getDvarInt( "scr_zm_cortex_per_round" );
	if ( getDvar( "scr_zm_cortex_per_round" ) == "" )
		cap = 3;
	return cap;
}

// --------------------------------------------------------------------------
// Pacing.
// --------------------------------------------------------------------------

cortex_round_watch()
{
	level endon( "end_game" );

	for ( ;; )
	{
		level waittill( "start_of_round" );

		level.cortex_round_count = 0;
		level.cortex_next_time = getTime() + randomIntRange( 12000, 25000 );

		// REACTION_ROUND ("why resist?") is the new-round line.
		if ( isDefined( level.round_number ) && level.round_number > 1 && randomInt( 100 ) < 50 )
			cortex_queue( "round", 30000, false );
	}
}

// Single-line hooks elsewhere in zm send level notify( "zm_cortex_event",
// <category> ) at the moment the player does the thing the line reacts to.
cortex_event_watch()
{
	level endon( "end_game" );

	for ( ;; )
	{
		level waittill( "zm_cortex_event", category );
		cortex_queue( category, 12000, true );
	}
}

cortex_level_notify_watch( msg, category )
{
	level endon( "end_game" );

	for ( ;; )
	{
		level waittill( msg );
		cortex_queue( category, 12000, true );
	}
}

// Remember the reaction for the next event.  pull: also bring the next event
// forward (never closer than 6 s after the previous one).
cortex_queue( category, life, pull )
{
	if ( !isDefined( category ) || !isDefined( level.cortex_lines ) || !isDefined( level.cortex_lines[ category ] ) )
		return;

	now = getTime();
	level.cortex_pending = category;
	level.cortex_pending_expire = now + life;

	if ( !is_true( pull ) )
		return;

	soon = level.cortex_last_time + 6000;
	if ( soon < now + 500 )
		soon = now + 500;
	if ( soon < level.cortex_next_time )
		level.cortex_next_time = soon;
}

cortex_pick_category()
{
	if ( isDefined( level.cortex_pending ) )
	{
		category = level.cortex_pending;
		level.cortex_pending = undefined;
		if ( getTime() < level.cortex_pending_expire )
			return category;
	}

	pool = level.cortex_idle_pool;
	category = pool[ randomInt( pool.size ) ];
	if ( isDefined( level.cortex_last_category ) && category == level.cortex_last_category )
		category = pool[ randomInt( pool.size ) ];
	return category;
}

cortex_living_zombie( z )
{
	if ( !isDefined( z ) || !isAlive( z ) )
		return false;
	return true;
}

// Is any hunting zombie close enough to a player for the stop to be seen?
cortex_audience()
{
	if ( !isDefined( level.zombies ) )
		return false;

	players = getPlayers();
	for ( i = 0; i < level.zombies.size; i++ )
	{
		z = level.zombies[ i ];
		if ( !cortex_living_zombie( z ) )
			continue;
		if ( !isDefined( z.zm_path_phase ) || z.zm_path_phase != "hunt" )
			continue;

		for ( j = 0; j < players.size; j++ )
		{
			if ( is_player_valid( players[ j ] ) && distanceSquared( z.origin, players[ j ].origin ) < 900 * 900 )
				return true;
		}
	}

	return false;
}

cortex_horde_think()
{
	level endon( "end_game" );

	for ( ;; )
	{
		wait( 0.5 );

		if ( !cortex_enabled() )
			continue;

		now = getTime();
		if ( cortex_test_mode() )
		{
			if ( now - level.cortex_last_time < 8000 )
				continue;
		}
		else if ( now < level.cortex_next_time )
			continue;

		if ( level.cortex_round_count >= cortex_round_cap() )
			continue;
		if ( !cortex_audience() )
			continue;

		cortex_horde_event();
	}
}

// --------------------------------------------------------------------------
// One event: the overlay on the whole horde, every hunt loop woken so the
// stops start together, and the line once per listener.
// --------------------------------------------------------------------------

cortex_horde_event()
{
	now = getTime();
	level.cortex_round_count++;
	level.cortex_total++;
	level.cortex_last_time = now;
	level.cortex_next_time = now + randomIntRange( 20000, 40000 );

	category = cortex_pick_category();
	level.cortex_last_category = category;

	level.cortex_event_id++;
	event = spawnStruct();
	event.id = level.cortex_event_id;
	event.category = category;
	event.alias = level.cortex_lines[ category ];
	event.end_time = now + int( level.cortex_event_length * 1000 );
	level.cortex_event = event;

	glowing = 0;
	for ( i = 0; i < level.zombies.size; i++ )
	{
		z = level.zombies[ i ];
		if ( !cortex_living_zombie( z ) )
			continue;

		if ( z cortex_overlay_on() )
			glowing++;
		z notify( "cortex_wake" );
	}

	// The voice starts once the stops have visibly begun.
	wait( 0.3 );
	speakers = cortex_speak( event.alias );

	codolprintln( "CoDOL zm cortex: event " + event.id + " " + event.alias + " (" + category + ") round " +
		diag_round() + " " + level.cortex_round_count + "/" + cortex_round_cap() + ": " + glowing +
		" glowing, " + speakers + " speaker(s)" );

	wait( level.cortex_event_length - 0.3 );

	if ( isDefined( level.cortex_event ) && level.cortex_event.id == event.id )
		level.cortex_event = undefined;
	level notify( "cortex_event_over" );
}

// Play the alias once per listener; returns the number of speakers.
cortex_speak( alias )
{
	speakers = [];
	players = getPlayers();
	range_sq = level.cortex_voice_range * level.cortex_voice_range;

	for ( i = 0; i < players.size; i++ )
	{
		player = players[ i ];
		if ( !isDefined( player ) )
			continue;
		if ( isDefined( player.sessionstate ) && player.sessionstate != "playing" )
			continue;

		// Already in earshot of a chosen speaker?
		covered = false;
		for ( j = 0; j < speakers.size; j++ )
		{
			if ( isDefined( speakers[ j ] ) && distanceSquared( speakers[ j ].origin, player.origin ) < range_sq )
			{
				covered = true;
				break;
			}
		}
		if ( covered )
			continue;

		best = undefined;
		best_sq = range_sq;
		for ( j = 0; j < level.zombies.size; j++ )
		{
			z = level.zombies[ j ];
			if ( !cortex_living_zombie( z ) )
				continue;
			d = distanceSquared( z.origin, player.origin );
			if ( d < best_sq )
			{
				best = z;
				best_sq = d;
			}
		}

		// No zombie in earshot: the player's own entity carries the line.
		if ( !isDefined( best ) )
			best = player;

		speakers[ speakers.size ] = best;
		best playSound( alias );
	}

	return speakers.size;
}

diag_round()
{
	if ( isDefined( level.round_number ) )
		return level.round_number;
	return 0;
}

// --------------------------------------------------------------------------
// The stop.  Called by zombie_think's hunt loop with the player it chases;
// runs on that thread and returns when the zombie is free to chase again.
// --------------------------------------------------------------------------

// The hunt loop's poll, cut short when an event starts.
cortex_hunt_wait( seconds )
{
	if ( !isDefined( level.cortex_lines ) )
	{
		wait( seconds );
		return;
	}

	self thread cortex_wake_timer( seconds );
	self waittill( "cortex_wake" );
}

cortex_wake_timer( seconds )
{
	self endon( "death" );
	self endon( "cortex_wake" );

	wait( seconds );
	self notify( "cortex_wake" );
}

cortex_should_join( player )
{
	if ( !isDefined( level.cortex_event ) )
		return false;
	event = level.cortex_event;
	// Too little of the line left for a stop to read as part of it.
	if ( event.end_time - getTime() < 1500 )
		return false;
	if ( isDefined( self.cortex_joined ) && self.cortex_joined == event.id )
		return false;

	if ( !is_true( self.has_legs ) || is_true( self.trap_frozen ) )
		return false;
	if ( !isDefined( self.zm_path_phase ) || self.zm_path_phase != "hunt" )
		return false;
	// A window climb holds ignoreall for its duration.
	if ( is_true( self.ignoreall ) )
		return false;
	// The native actor clears this during a traversal and during a melee
	// swing; animscripted would otherwise cut either one off.
	if ( !is_true( self.safeToChangeScript ) )
		return false;
	// Already in reach: keep swinging rather than stand there.
	if ( isDefined( player ) && distanceSquared( self.origin, player.origin ) < 90 * 90 )
		return false;

	return true;
}

cortex_join( player )
{
	self endon( "death" );
	self endon( "zombie_delete" );

	event = level.cortex_event;
	self.cortex_joined = event.id;

	anime = level.cortex_taunt_anims[ randomInt( level.cortex_taunt_anims.size ) ];

	yaw = self.angles[ 1 ];
	if ( isDefined( player ) )
	{
		facing = vectorToAngles( player.origin - self.origin );
		yaw = facing[ 1 ];
	}

	self animscripted( "cortextaunt", self.origin, ( 0, yaw, 0 ), anime, "normal", %body, 1, 0.3 );

	hold = getAnimLength( anime );
	left = ( event.end_time - getTime() ) / 1000;
	if ( left < hold )
		hold = left;
	if ( hold < 0.5 )
		hold = 0.5;

	self thread cortex_hold_timer( hold );
	self thread cortex_abort_watch();
	self waittill( "cortex_hold_over" );

	self stopanimscripted();
}

cortex_hold_timer( hold )
{
	self endon( "death" );
	self endon( "cortex_hold_over" );

	wait( hold );
	self notify( "cortex_hold_over" );
}

// A player who walks up to a stopped zombie gets attacked, not ignored.
cortex_abort_watch()
{
	self endon( "death" );
	self endon( "cortex_hold_over" );

	for ( ;; )
	{
		wait( 0.1 );

		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( !is_player_valid( players[ i ] ) )
				continue;
			if ( distanceSquared( self.origin, players[ i ].origin ) < 90 * 90 )
			{
				self notify( "cortex_hold_over" );
				return;
			}
		}
	}
}

// --------------------------------------------------------------------------
// The red eyes / blue mouth overlay.  Returns true when it was laid.
// --------------------------------------------------------------------------

cortex_overlay_on()
{
	if ( is_true( self.cortex_overlay ) || is_true( self.head_gibbed ) || !isDefined( self.headModel ) )
		return false;

	self.cortex_taunting = true;
	self.cortex_overlay = true;
	self notify( "cortex_overlay_on" );

	for ( i = 0; i < level.cortex_overlay_tags.size; i++ )
		playFxOnTag( level._effect[ "cortex_eye_red" ], self, level.cortex_overlay_tags[ i ] );
	playFxOnTag( level._effect[ "cortex_mouth_blue" ], self, "tag_mouth" );

	self thread cortex_overlay_cleanup();
	return true;
}

// Must run while the head is still attached: stopFxOnTag on a bone the DObj
// no longer has is a script error (zombie_head_gib calls this before it
// detaches the head, as it does for the base eye glow).
cortex_overlay_off()
{
	if ( !is_true( self.cortex_overlay ) )
		return;
	self.cortex_overlay = false;

	for ( i = 0; i < level.cortex_overlay_tags.size; i++ )
		stopFxOnTag( level._effect[ "cortex_eye_red" ], self, level.cortex_overlay_tags[ i ] );
	stopFxOnTag( level._effect[ "cortex_mouth_blue" ], self, "tag_mouth" );

	self.cortex_taunting = false;
	self notify( "cortex_overlay_off" );
}

// Ends the overlay however the event ends for this zombie: the event's own
// end, or death mid-line (the entity is still the dying actor on "death";
// zombie_death_animscript stops the base eye glow on it the same way).
// waittill_either is not used: its endon is thread-scoped on this VM, so a
// "death" would end this whole thread before the overlay came off.
cortex_overlay_cleanup()
{
	self endon( "cortex_overlay_off" );

	self thread cortex_end_relay( self, "death" );
	self thread cortex_end_relay( level, "cortex_event_over" );

	self waittill( "cortex_cleanup" );

	if ( isDefined( self ) )
		self cortex_overlay_off();
}

cortex_end_relay( ent, msg )
{
	self endon( "cortex_cleanup" );
	self endon( "cortex_overlay_off" );

	ent waittill( msg );
	if ( isDefined( self ) )
		self notify( "cortex_cleanup" );
}
