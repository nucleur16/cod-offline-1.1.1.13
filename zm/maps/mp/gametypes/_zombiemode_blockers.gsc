// zm barricades, purchasable doors and purchasable debris.
//
// Port of T5's maps\_zombiemode_blockers.gsc (3,431 lines).
//
// TAKEN, by T5 function:
//   init / init_blockers        :9,:45    -- the four entity sweeps that exist
//                                            on CoDOL's zm maps (exterior_goal
//                                            structs, zombie_door, zombie_debris,
//                                            flag_blocker).
//   blocker_init                :1163     -- barrier chunk discovery and state.
//   blocker_attack_spots        :1313     -- the three AI attack positions.
//   blocker_think               :1350     -- the 0.5s barrier poll.
//   blocker_trigger_think       :1401     -- THE REPAIR-FOR-POINTS LOOP, whole:
//                                            trigger spawn, use-button hold,
//                                            one chunk per second,
//                                            rebuild_barrier_reward cap.
//   rebuild_barrier_reward_reset:1724
//   remove_chunk                :1729     -- state + Hide + notSolid only.
//   replace_chunk               :2174     -- state + Show + Solid only.
//   trigger_delete_on_repair    :1689
//   door_init / door_classify   :92,:171
//   door_buy                    :240      -- solo purse branch (see the note in
//                                            _zombiemode_score.gsc on why the
//                                            team pot is not ported).
//   door_activate               :584      -- rotate / move / slide_apart.
//   door_think                  :699      -- including electric_door and
//                                            electric_buyable_door.
//   door_opened                 :791      -- script_flag fan-out.
//   debris_init / debris_think  :934,:960
//   debris_move                 :1104
//   flag_blocker                :2877
//
// NOT TAKEN, and why:
//   The "anim" door style (door_classify :195, door_activate case "anim",
//   door_solid_thread_anim :890) -- it calls level.blocker_anim_func, which is
//   animscripted().  A door authored script_string "anim" is skipped with one
//   println rather than compiled.
//   kill_countdown / door_kill_counter (:347,:484) -- the counter doors need
//   p_zom_counter_* models and a kill-counter HUD; no CoDOL zm map authors
//   script_string counter_1s/_10s/_100s.
//   shutter_* (:2956-3285) -- window_shutter entities exist on no CoDOL zm map.
//   The grate and repair_board sub-types' model swapping (:1440-1590,
//   remove_chunk_rotate_grate :2083) -- board geometry variants that need the
//   per-piece anim_grate_index bookkeeping and the tear FX.
//   quantum_bomb_* (:3356,:3391) -- that power-up is not ported.
//   Every zombie_boardtear_* / zombie_bartear_* audio-plus-FX offset function
//   (:2105,:2155,:2605,:2642,:2680,:2737,:2792) -- FX-only, and they are the
//   functions T5 drives from animation notetracks.
//
// TWO BUILTINS T5 USES THAT THIS SERVER DOES NOT REGISTER (MEASURED
// 2026-09-20): ConnectPaths and DisconnectPaths.  T5 calls them on every door
// brush and every barrier clip so the AI pathfinder learns the map changed.
// They are simply not called here.  The consequence is that opening a door
// does not re-path the AI; the actors reach new space by walking, and
// setGoalPos is recomputed every second (_zombiemode_spawner::zombie_think),
// so a newly opened room becomes reachable as soon as a player stands in it.
//
// ONE ADAPTATION, flagged loudly.  In T5 a zombie tears a board off a window
// from an ANIMATION NOTETRACK (the animscript calls remove_chunk).  This
// server has no notetracks, so nothing would ever remove a board and the
// repair economy -- the classic early-round income, and the reason this module
// is priority 3 -- could never pay out.  blocker_zombie_tear_watch() below
// removes one chunk per zombie_board_tear_delay seconds while a zombie stands
// in the barrier's attack spots.  It is the TRIGGER that is invented, not the
// effect: remove_chunk is T5's.  When the animation workstream lands its
// notetracks, delete blocker_zombie_tear_watch and call remove_chunk from the
// notetrack instead; nothing else in this file changes.

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	set_zombie_var( "rebuild_barrier_cap_per_round", 500 );
	set_zombie_var( "zombie_board_tear_delay", 3 );

	add_sound( "rebuild_barrier_piece", "zmb_cha_ching" );
	add_sound( "purchase", "zmb_cha_ching" );
	add_sound( "no_purchase", "zmb_no_purchase" );
	add_sound( "door_slide_open", "zmb_door_slide_open" );
	add_sound( "debris_move", "zmb_debris_move" );

	init_blockers();
}

// T5 :45.
init_blockers()
{
	level.exterior_goals = get_struct_array( "exterior_goal", "targetname" );

	for ( i = 0; i < level.exterior_goals.size; i++ )
		level.exterior_goals[ i ] thread blocker_init();

	zombie_doors = getEntArray( "zombie_door", "targetname" );
	for ( i = 0; i < zombie_doors.size; i++ )
		zombie_doors[ i ] thread door_init();

	zombie_debris = getEntArray( "zombie_debris", "targetname" );
	for ( i = 0; i < zombie_debris.size; i++ )
		zombie_debris[ i ] thread debris_init();

	flag_blockers = getEntArray( "flag_blocker", "targetname" );
	for ( i = 0; i < flag_blockers.size; i++ )
		flag_blockers[ i ] thread flag_blocker();

	println( "CoDOL zm: " + level.exterior_goals.size + " barriers, " + zombie_doors.size +
		" doors, " + zombie_debris.size + " debris\n" );
}

// ==========================================================================
// BARRIERS
// ==========================================================================

// T5 :1163.  self is an exterior_goal struct; its target names the chunks.
blocker_init()
{
	if ( !isDefined( self.target ) )
		return;

	targets = getEntArray( self.target, "targetname" );

	self.barrier_chunks = [];

	for ( j = 0; j < targets.size; j++ )
	{
		if ( isDefined( targets[ j ].script_noteworthy ) && targets[ j ].script_noteworthy == "clip" )
		{
			self.clip = targets[ j ];
			continue;
		}

		targets[ j ] update_states( "repaired" );
		targets[ j ].destroyed = false;
		targets[ j ].claimed = false;
		targets[ j ].og_origin = targets[ j ].origin;
		targets[ j ].og_angles = targets[ j ].angles;

		self.barrier_chunks[ self.barrier_chunks.size ] = targets[ j ];
	}

	if ( self.barrier_chunks.size < 1 )
		return;

	self blocker_attack_spots();

	// T5 asserts self.clip here.  A barrier with no clip brush still repairs;
	// it just has nothing to re-solidify, so the assert is dropped.
	self.trigger_location = get_struct( self.target, "targetname" );

	self thread blocker_think();
	self thread blocker_zombie_tear_watch();
}

// T5 :1313.  Three positions in front of the barrier that a zombie stands in
// to attack it.  T5 hands these to the AI's claim system; here they are the
// proximity test blocker_zombie_tear_watch uses.
blocker_attack_spots()
{
	chunk = get_closest( self.origin, self.barrier_chunks );
	if ( !isDefined( chunk ) )
		return;

	// A script_struct need not carry an "angles" key; T5's exterior_goals
	// always do, but a missing one would make anglesToForward a runtime error
	// rather than a missing attack spot.
	angles = self.angles;
	if ( !isDefined( angles ) )
		angles = vectorToAngles( chunk.origin - self.origin );

	dist = distance( self.origin, chunk.origin ) - 36;

	spots = [];
	spots[ 0 ] = groundpos( self.origin + ( anglesToForward( angles ) * dist ) + ( 0, 0, 60 ) );
	spots[ spots.size ] = groundpos( spots[ 0 ] + ( anglesToRight( angles ) * 28 ) + ( 0, 0, 60 ) );
	spots[ spots.size ] = groundpos( spots[ 0 ] + ( anglesToRight( angles ) * -28 ) + ( 0, 0, 60 ) );

	self.attack_spots = spots;
}

// T5 :1350.  Poll the barrier; when a chunk is missing, run the repair loop.
blocker_think()
{
	for ( ;; )
	{
		wait( 0.5 );

		if ( all_chunks_intact( self.barrier_chunks ) )
			continue;

		if ( no_valid_repairable_boards( self.barrier_chunks ) )
			continue;

		self blocker_trigger_think();
	}
}

// T5 :1401.  THE repair-for-points loop.
//
// T5 comment, preserved because it is the design: "They don't cost, they now
// award the player the cost..."  A board is worth self.zombie_cost (default
// 10) per piece, capped per round at rebuild_barrier_cap_per_round, which
// round_think raises to min(50 * round, 500).
blocker_trigger_think()
{
	cost = 10;
	if ( isDefined( self.zombie_cost ) )
		cost = self.zombie_cost;

	radius = 96;
	height = 96;

	if ( isDefined( self.trigger_location ) )
		trigger_location = self.trigger_location;
	else
		trigger_location = self;

	if ( isDefined( trigger_location.radius ) )
		radius = trigger_location.radius;
	if ( isDefined( trigger_location.height ) )
		height = trigger_location.height;

	trigger_pos = groundpos( trigger_location.origin ) + ( 0, 0, 4 );

	trigger = spawn( "trigger_radius", trigger_pos, 0, radius, height );
	trigger thread trigger_delete_on_repair();

	trigger set_hint_string( self, "Hold ^3[{+activate}]^7 to Rebuild Barrier" );
	trigger setCursorHint( "HINT_NOICON" );

	for ( ;; )
	{
		trigger waittill( "trigger", player );

		if ( all_chunks_intact( self.barrier_chunks ) )
		{
			trigger notify( "all_boards_repaired" );
			return;
		}

		if ( no_valid_repairable_boards( self.barrier_chunks ) )
		{
			trigger notify( "no valid boards" );
			return;
		}

		for ( ;; )
		{
			if ( !isDefined( trigger ) )
				return;

			if ( !player isTouching( trigger ) )
				break;

			if ( !is_player_valid( player ) )
				break;

			if ( player in_revive_trigger() )
				break;

			if ( !player use_button_held() )
				break;

			chunk = get_random_destroyed_chunk( self.barrier_chunks );
			if ( !isDefined( chunk ) )
				break;

			self replace_chunk( chunk );

			if ( isDefined( self.clip ) )
				self.clip enable_trigger();

			// T5 waits one second per board unless the barrier carries a
			// script_delay.
			if ( isDefined( self.script_delay ) )
				wait( self.script_delay );
			else
				wait( 1 );

			if ( !is_player_valid( player ) )
				break;

			if ( !isDefined( player.rebuild_barrier_reward ) )
				player.rebuild_barrier_reward = 0;

			player.rebuild_barrier_reward += cost;
			if ( player.rebuild_barrier_reward < level.zombie_vars[ "rebuild_barrier_cap_per_round" ] )
			{
				player maps\mp\gametypes\_zombiemode_score::player_add_points( "rebuild_board", cost );
				player play_sound_on_ent( "purchase" );
			}

			if ( all_chunks_intact( self.barrier_chunks ) )
			{
				trigger notify( "all_boards_repaired" );
				return;
			}

			if ( no_valid_repairable_boards( self.barrier_chunks ) )
			{
				trigger notify( "no valid boards" );
				return;
			}
		}
	}
}

// T5 :1689.
trigger_delete_on_repair()
{
	self waittill_either( "all_boards_repaired", "no valid boards" );
	self delete();
}

// T5 :1724.  round_think resets every player's per-round reward cap.
rebuild_barrier_reward_reset()
{
	self.rebuild_barrier_reward = 0;
}

// T5 :1729 remove_chunk, state half.  Everything T5 does around these three
// lines is tear audio, tear FX, the grate rotation and the per-piece
// anim_grate_index -- see the header.
remove_chunk( chunk )
{
	chunk update_states( "destroyed" );
	chunk.destroyed = true;
	chunk hide();
	chunk notSolid();
	chunk notify( "destroyed" );
}

// T5 :2174 replace_chunk, state half.
replace_chunk( chunk )
{
	chunk update_states( "repaired" );
	chunk.destroyed = false;
	if ( isDefined( chunk.og_origin ) )
		chunk.origin = chunk.og_origin;
	if ( isDefined( chunk.og_angles ) )
		chunk.angles = chunk.og_angles;
	chunk show();
	chunk solid();
	chunk play_sound_on_ent( "rebuild_barrier_piece" );
}

// ADAPTATION -- see the header.  This replaces an animation notetrack, and
// only the notetrack: remove_chunk is T5's.
blocker_zombie_tear_watch()
{
	if ( !isDefined( self.attack_spots ) || self.attack_spots.size < 1 )
		return;

	for ( ;; )
	{
		wait( 1 );

		if ( all_chunks_destroyed( self.barrier_chunks ) )
			continue;

		if ( !isDefined( level.zombies ) )
			continue;

		attacker = undefined;
		for ( i = 0; i < level.zombies.size; i++ )
		{
			z = level.zombies[ i ];
			if ( !isDefined( z ) )
				continue;

			for ( s = 0; s < self.attack_spots.size; s++ )
			{
				if ( distanceSquared( z.origin, self.attack_spots[ s ] ) < 4096 )
				{
					attacker = z;
					break;
				}
			}

			if ( isDefined( attacker ) )
				break;
		}

		if ( !isDefined( attacker ) )
			continue;

		intact = [];
		for ( i = 0; i < self.barrier_chunks.size; i++ )
		{
			if ( self.barrier_chunks[ i ] get_chunk_state() == "repaired" )
				intact[ intact.size ] = self.barrier_chunks[ i ];
		}

		if ( intact.size < 1 )
			continue;

		remove_chunk( intact[ randomInt( intact.size ) ] );

		if ( isDefined( self.clip ) )
			self.clip disable_trigger();

		wait( level.zombie_vars[ "zombie_board_tear_delay" ] );
	}
}

// ==========================================================================
// DOORS
// ==========================================================================

// T5 :92.
door_init()
{
	self._door_open = false;

	if ( !isDefined( self.target ) )
		return;

	targets = getEntArray( self.target, "targetname" );

	if ( isDefined( self.script_flag ) )
	{
		tokens = strTok( self.script_flag, "," );
		for ( i = 0; i < tokens.size; i++ )
			flag_init( tokens[ i ] );
	}

	if ( !isDefined( self.script_noteworthy ) )
		self.script_noteworthy = "default";

	self.doors = [];
	for ( i = 0; i < targets.size; i++ )
		targets[ i ] door_classify( self );

	cost = 1000;
	if ( isDefined( self.zombie_cost ) )
		cost = self.zombie_cost;

	self setCursorHint( "HINT_NOICON" );
	self thread door_think();

	if ( self.script_noteworthy == "electric_door" || self.script_noteworthy == "electric_buyable_door" )
	{
		self setHintString( "Requires Power" );
		return;
	}

	self set_hint_string( self, "Hold ^3[{+activate}]^7 to Open Door [Cost: " + cost + "]" );
}

// T5 :171.  self is a door element, parent_trig the zombie_door trigger.
door_classify( parent_trig )
{
	if ( isDefined( self.script_noteworthy ) && self.script_noteworthy == "clip" )
	{
		parent_trig.clip = self;
		self.script_string = "clip";
	}
	else if ( !isDefined( self.script_string ) )
	{
		if ( isDefined( self.script_angles ) )
			self.script_string = "rotate";
		else if ( isDefined( self.script_vector ) )
			self.script_string = "move";
		else
			self.script_string = "none";
	}

	parent_trig.doors[ parent_trig.doors.size ] = self;
}

// T5 :240, solo purse branch.  Returns true when the door was paid for.
door_buy()
{
	self waittill( "trigger", who );

	if ( !isDefined( who ) || !isPlayer( who ) )
		return false;

	if ( !who useButtonPressed() )
		return false;

	if ( who in_revive_trigger() )
		return false;

	if ( !is_player_valid( who ) )
		return false;

	cost = 1000;
	if ( isDefined( self.zombie_cost ) )
		cost = self.zombie_cost;

	if ( who.score < cost )
	{
		play_sound_at_pos( "no_purchase", self.origin );
		return false;
	}

	who maps\mp\gametypes\_zombiemode_score::minus_to_player_score( cost );
	return true;
}

// T5 :699.
door_think()
{
	self endon( "kill_door_think" );

	cost = 1000;
	if ( isDefined( self.zombie_cost ) )
		cost = self.zombie_cost;

	for ( ;; )
	{
		if ( self.script_noteworthy == "electric_door" )
		{
			flag_wait( "power_on" );
		}
		else if ( self.script_noteworthy == "electric_buyable_door" )
		{
			flag_wait( "power_on" );

			self set_hint_string( self, "Hold ^3[{+activate}]^7 to Open Door [Cost: " + cost + "]" );

			if ( !self door_buy() )
				continue;
		}
		else
		{
			if ( !self door_buy() )
				continue;
		}

		self door_opened();
		break;
	}

	self._door_open = true;
	self notify( "door_opened" );
}

// T5 :791.
door_opened()
{
	if ( isDefined( self.script_flag ) )
	{
		tokens = strTok( self.script_flag, "," );
		for ( i = 0; i < tokens.size; i++ )
			flag_set( tokens[ i ] );
	}

	play_sound_at_pos( "purchase", self.origin );

	for ( i = 0; i < self.doors.size; i++ )
		self.doors[ i ] thread door_activate();

	if ( isDefined( self.clip ) )
		self.clip delete();

	self delete();
}

// T5 :584, minus ConnectPaths (not registered) and the "anim" style.
door_activate()
{
	time = 1;
	if ( isDefined( self.script_transition_time ) )
		time = self.script_transition_time;

	self notSolid();

	if ( isDefined( self.door_moving ) )
		return;
	self.door_moving = 1;

	if ( isDefined( self.script_string ) && self.script_string == "clip" )
		return;

	if ( isDefined( self.script_sound ) )
		play_sound_at_pos( self.script_sound, self.origin );
	else
		play_sound_at_pos( "door_slide_open", self.origin );

	switch( self.script_string )
	{
	case "rotate":
		if ( isDefined( self.script_angles ) )
			self rotateTo( self.script_angles, time, 0, 0 );
		break;

	case "move":
	case "slide_apart":
		if ( isDefined( self.script_vector ) )
		{
			if ( time >= 0.5 )
				self moveTo( self.origin + self.script_vector, time, time * 0.25, time * 0.25 );
			else
				self moveTo( self.origin + self.script_vector, time );
		}
		break;

	case "anim":
		// T5 case "anim" calls level.blocker_anim_func, which is
		// animscripted().  Not registered here; the door is simply removed.
		println( "CoDOL zm: door element " + self.targetname + " is script_string anim; opened without animation\n" );
		self delete();
		return;

	default:
		self delete();
		return;
	}

	// RotateTo notifies "rotatedone" and MoveTo notifies "movedone"; waiting
	// on the wrong one would hang this thread forever, and one of the two
	// branches above is always the one that ran, so the transition time is
	// waited out directly instead.
	wait( time );
	self notSolid();
}

// ==========================================================================
// DEBRIS
// ==========================================================================

// T5 :934.
debris_init()
{
	cost = 1000;
	if ( isDefined( self.zombie_cost ) )
		cost = self.zombie_cost;

	self set_hint_string( self, "Hold ^3[{+activate}]^7 to Remove Debris [Cost: " + cost + "]" );
	self setCursorHint( "HINT_NOICON" );

	if ( isDefined( self.script_flag ) )
		flag_init( self.script_flag );

	self thread debris_think();
}

// T5 :960, solo purse branch, minus ConnectPaths.
debris_think()
{
	cost = 1000;
	if ( isDefined( self.zombie_cost ) )
		cost = self.zombie_cost;

	for ( ;; )
	{
		self waittill( "trigger", who );

		if ( !isDefined( who ) || !isPlayer( who ) )
			continue;

		if ( !who useButtonPressed() )
			continue;

		if ( who in_revive_trigger() )
			continue;

		if ( !is_player_valid( who ) )
			continue;

		if ( who.score < cost )
		{
			play_sound_at_pos( "no_purchase", self.origin );
			continue;
		}

		who maps\mp\gametypes\_zombiemode_score::minus_to_player_score( cost );

		junk = getEntArray( self.target, "targetname" );

		if ( isDefined( self.script_flag ) )
		{
			tokens = strTok( self.script_flag, "," );
			for ( i = 0; i < tokens.size; i++ )
				flag_set( tokens[ i ] );
		}

		play_sound_at_pos( "purchase", self.origin );
		level notify( "junk purchased" );

		clip = undefined;
		for ( i = 0; i < junk.size; i++ )
		{
			if ( isDefined( junk[ i ].script_noteworthy ) && junk[ i ].script_noteworthy == "clip" )
			{
				clip = junk[ i ];
				continue;
			}

			s = undefined;
			if ( isDefined( junk[ i ].script_linkTo ) )
				s = get_struct( junk[ i ].script_linkTo, "script_linkname" );

			if ( isDefined( s ) )
				junk[ i ] thread debris_move( s );
			else
				junk[ i ] delete();
		}

		if ( isDefined( clip ) )
			clip delete();

		self delete();
		return;
	}
}

// T5 :1104.
debris_move( s )
{
	if ( isDefined( self.script_delay ) )
		wait( self.script_delay );

	self notSolid();
	self play_sound_on_ent( "debris_move" );

	time = 1;
	if ( isDefined( self.script_transition_time ) )
		time = self.script_transition_time;

	self moveTo( s.origin, time );
	if ( isDefined( s.angles ) )
		self rotateTo( s.angles, time );

	wait( time );
	self notify( "debris_moved" );
}

// ==========================================================================
// FLAG BLOCKERS -- T5 :2877.  A brush that vanishes when its flag is set.
// ==========================================================================

flag_blocker()
{
	if ( !isDefined( self.script_flag ) )
		return;

	flag_init( self.script_flag );
	flag_wait( self.script_flag );

	self delete();
}
