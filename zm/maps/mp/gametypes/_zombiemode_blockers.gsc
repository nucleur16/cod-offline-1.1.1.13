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
//   remove_chunk                :1729     -- state + notSolid, then the T5
//                                            board flight (remove_chunk_fly).
//   replace_chunk               :2174     -- the "board" arm, animated: the
//                                            board flies back from where it
//                                            landed (see replace_chunk).
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
//   zombie_bartear_* (:2155 and the bar_bend family) -- C1-vs-T5, C1 WINS:
//   MEASURED, %ai_zombie_bartear_* and %ai_zombie_bar_bend_* ARE declared in
//   CoDOL's generic_human.atr but ship NO xanim in any zone a zm match mounts
//   (docs/zone_assets/ALL_xanim.txt), so the metal-bar barricade variant does
//   not exist in CoDOL.  The wood-board variant, which does ship all 36
//   clips, is ported in full.
//   The "anim" door style (door_classify :195, door_activate case "anim",
//   door_solid_thread_anim :890).  The reason changed on 2026-09-20 and is
//   now sharper than "no animation builtins": there ARE twenty of them, but
//   MEASURED, src/codol/script/codol_script_anim.c:143 anim_receiver refuses
//   any entity for which CODOL_ActorOwnedTree returns NULL, and its own error
//   text says so -- "only actors own one on this server".  A zombie_door is a
//   script_brushmodel, not an actor, so NONE of the twenty can drive it, and
//   T5's level.blocker_anim_func additionally wants animscripted.  A door
//   authored script_string "anim" is still skipped with one println.  Making
//   this work is a native change (give a script model a server animtree), not
//   a script one.
//
// TWO BUILTINS T5 USES THAT THIS SERVER DOES NOT REGISTER (MEASURED
// 2026-09-20): ConnectPaths and DisconnectPaths.  T5 calls them on every door
// brush and every barrier clip so the AI pathfinder learns the map changed.
// They are simply not called here.  The consequence is that opening a door
// does not re-path the AI; the actors reach new space by walking, and
// setGoalPos is recomputed every second (_zombiemode_spawner::zombie_think),
// so a newly opened room becomes reachable as soon as a player stands in it.
//
// THE INVENTED TEAR TIMER IS GONE.  2026-09-20.  The previous pass shipped
// blocker_zombie_tear_watch(), a barrier-side poll that removed one chunk
// every zombie_board_tear_delay seconds while a zombie stood in an attack
// spot, because with no notetracks nothing would ever remove a board and the
// repair economy could not pay out.  Its own header said to delete it the
// moment notetracks landed.  They landed (commit d4548385,
// docs/CODOL_SCRIPTED_ANIMATION_20260920.md section 3), so it is deleted.
//
// What replaces it is T5's own mechanism, in T5's own two halves:
//   * _zombiemode_spawner::tear_into_building (T5 :800) claims an attack
//     spot, plays the tear CLIP for that barrier slot, and runs
//     _zombiemode_spawner::zombie_tear_notetracks (T5 :1150), which waits on
//     the clip's "board" notetrack;
//   * on that note it calls remove_chunk() below -- unchanged, still T5's --
//     together with the tear audio and FX offset functions, which are now
//     ported here (T5 :2105 zombie_boardtear_audio_offset, :1428/:1493
//     zombie_boardtear_offset_fx_horizontle / _verticle, which T5 keeps in
//     its spawner and which this module keeps next to the chunks they act
//     on).
// Nothing polls, and no board is removed except by an animation.

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	set_zombie_var( "rebuild_barrier_cap_per_round", 500 );
	set_zombie_var( "zombie_board_tear_delay", 3 );

	// T5 _zombiemode.gsc:618-627.  All three ship in common_gametype_zm
	// (docs/zone_assets/ALL_sound.txt).  rebuild_barrier_piece used to be
	// zmb_cha_ching here, so a repair never made the hammer sound; the
	// cha-ching is "purchase", played when the board pays out.
	add_sound( "rebuild_barrier_piece", "zmb_repair_boards" );
	add_sound( "rebuild_barrier_hover", "zmb_boards_float" );
	add_sound( "barrier_rebuild_slam", "zmb_board_slam" );
	add_sound( "purchase", "zmb_cha_ching" );
	// CoDOL alias, not T5's.  MEASURED: T5's name is absent from
	// docs/zone_assets/ALL_sound.txt and the engine logged
	// "unknown sound alias"; the name below is the shipped CoDOL
	// equivalent, in common_gametype_zm, which a zm match mounts.
	add_sound( "no_purchase", "zmb_no_cha_ching" );
	add_sound( "door_slide_open", "zmb_door_slide_open" );
	add_sound( "debris_move", "zmb_debris_clear" );

	// T5 maps\_zombiemode_fx.gsc loads this as level._effect["wood_chunk_destory"]
	// (T5's own typo).  MEASURED, docs/zone_assets/ALL_fx.txt:
	// C1-vs-T5 CONFLICT, C1 WINS, and this one is a SUBSTITUTION rather than
	// a port -- called out so anyone who identifies a better match can
	// correct it.  MEASURED, docs/zone_assets/ALL_fx.txt: CoDOL ships no
	// wood-chunk destruction effect at all (no fx_wood_chunk*, no
	// destructibles/*board*, no splinter effect).  The nearest shipped wood
	// debris burst that a zm match mounts is "impacts/fx_large_woodhit"
	// (common_gametype_zm).  Every use is guarded, so deleting this line
	// degrades the tear to silent geometry rather than breaking it.
	level._effect[ "wood_chunk_destroy" ] = loadfx( "impacts/fx_large_woodhit" );

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

	// Developer-only test verbs (scr_codol_blockertest); returns at once on
	// the live server, which runs developer 0.
	level thread maps\mp\gametypes\_zombiemode_blockertest::init();
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

	// T5: the clip is a script_brushmodel with spawnflag 1, DISCONNECT_PATHS,
	// so the window's node_negotiation link is cut from spawn until
	// zombie_goto_entrance's `clip connectpaths()` after the tear.  Cutting
	// it here (native Path_DisconnectPaths over the map graph) is what keeps
	// a zombie chasing the player from planning THROUGH untorn boards --
	// it must route to the exterior_goal and tear first.
	if ( isDefined( self.clip ) )
		self.clip disconnectpaths();

	self blocker_attack_spots();

	// T5 asserts self.clip here.  A barrier with no clip brush still repairs;
	// it just has nothing to re-solidify, so the assert is dropped.
	self.trigger_location = get_struct( self.target, "targetname" );

	self thread blocker_think();
}

// T5 :1313.  Three positions in front of the barrier that a zombie stands in
// to attack it.  These are T5's claim system, used as T5 uses them:
// _zombiemode_spawner::get_attack_spot reserves one and walks the zombie to
// it before the tear animation plays.
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

	// T5 :1327.  One reservation per spot, so two zombies do not tear the
	// same board from the same place.  _zombiemode_spawner::get_attack_spot
	// claims one and reset_attack_spot releases it.
	self.attack_spots_taken = [];
	for ( i = 0; i < spots.size; i++ )
		self.attack_spots_taken[ i ] = false;
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

	trigger set_hint_string( self, "default_reward_barrier_piece" );
	trigger setCursorHint( "HINT_NOICON" );
	// C1's cursor-hint scan ignores trigger_radius (its contents are a touch
	// volume, not a usable trigger), although its touch event still lets the
	// player repair boards.  Publish the same hint to the touching player
	// while they can repair; clear it before leaving or deleting the trigger.
	repair_hint = get_zombie_hint( "default_reward_barrier_piece" );

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

		// The revive prompt wins while the player also stands in a revive trigger.
		if ( isDefined( repair_hint ) && is_player_valid( player ) && !player in_revive_trigger() )
			player forceUseHintOn( repair_hint );

		for ( ;; )
		{
			if ( !isDefined( trigger ) )
			{
				if ( is_player_valid( player ) )
					player forceUseHintOff();
				return;
			}

			if ( !is_player_valid( player ) )
				break;
			if ( !player isTouching( trigger ) )
				break;

			if ( player in_revive_trigger() )
				break;

			if ( !player use_button_held() )
			{
				wait( 0.05 );
				continue;
			}

			chunk = get_random_destroyed_chunk( self.barrier_chunks );
			if ( !isDefined( chunk ) )
				break;

			// T5 :1545: the hammer sound on the board as the player starts it,
			// then replace_chunk THREADED (T5 :1576) -- the board takes ~1.2 s
			// to fly back, and only counts as repaired once it is in its slot.
			fast = false;
			if ( isDefined( player.perkpop_active ) )
				fast = is_true( player.perkpop_active[ "carpenter" ] );
			chunk play_sound_on_ent( "rebuild_barrier_piece" );
			self thread replace_chunk( chunk, fast );
			level notify( "zm_cortex_event", "repair" );   // _zombiemode_cortex reaction

			if ( isDefined( self.clip ) )
			{
				self.clip enable_trigger();
				self.clip disconnectpaths();
				self.clip.paths_open = false;
			}

			// T5 waits one second per board unless the barrier carries a
			// script_delay.  The carpenter perk pop shortens it.
			delay = 1;
			if ( isDefined( self.script_delay ) )
				delay = self.script_delay;
			wait( player maps\mp\gametypes\_zombiemode_perkpops::board_repair_delay( delay ) );

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

			// The board is still flying when the 1 s (or carpenter 0.25 s)
			// delay ends.  When that was the last board, let it land before
			// asking whether the window is whole, or the last board would end
			// the loop through "no valid boards" and the window-rebuilt
			// challenge and the carpenter C4 would never fire.
			if ( !isDefined( get_random_destroyed_chunk( self.barrier_chunks ) ) )
				wait_for_mid_repair( self.barrier_chunks, 3 );

			if ( all_chunks_intact( self.barrier_chunks ) )
			{
				// CHALLENGE_ZM_WINDOWS "rebuild &&1 windows": the player who
				// puts the last board back has rebuilt the window.
				player maps\mp\gametypes\_zombiemode_challenges::zm_challenge_inc( "zm_windows" );

				// The carpenter perk pop plants its C4 behind a finished window.
				player thread maps\mp\gametypes\_zombiemode_perkpops::barrier_repaired( self );
				player forceUseHintOff();
				trigger notify( "all_boards_repaired" );
				return;
			}

			if ( no_valid_repairable_boards( self.barrier_chunks ) )
			{
				player forceUseHintOff();
				trigger notify( "no valid boards" );
				return;
			}
		}

		if ( is_player_valid( player ) )
			player forceUseHintOff();
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

// T5 :1729 remove_chunk, the "board" arm (:1900-:1990), which is the only
// arm this map needs: all 72 chunks of zm_prototype_sh are script_brushmodel
// with script_parameters "board" and script_move_dist 125 (MEASURED,
// obj/zm-mapents/zm_prototype_sh-mapents.txt).
//
// THE BOARD NOW FLIES.  2026-09-22.  User report: "they tear down the windows
// but the animation for the boards flying off isn't working."  The previous
// pass stopped at "state + Hide + notSolid" (its own header said so), so on
// the "board" note the chunk simply vanished.  T5 does not hide the chunk on
// the note; it launches it.  Verbatim from T5 :1900-:1990:
//
//   ent = Spawn( "script_origin", chunk.origin )      -- a launcher
//   ent.angles = node.angles + ( 0, 180, 0 )          -- away from the room,
//                                                        toward the zombie
//   dist = 100 + RandomInt( script_move_dist - 100 )  -- 100..124 here
//   dest = forward * dist, BulletTrace'd 200 down     -- land on the floor
//   chunk LinkTo( ent )
//   time = ent fake_physicslaunch( dest, 200 + RandomInt( 100 ) )
//                                                     -- MoveGravity arc
//   ent RotatePitch( 180, time*0.5 ) 60% / RotatePitch( 90, time, time*0.5 )
//   wait( time ); chunk Hide(); wait( 0.1 ); ent Delete()
//   chunk update_states( "destroyed" ); chunk notify( "destroyed" )
//
// Every builtin on that path is native and non-stub here (audited,
// docs/CODOL_ZOMBIE_BOARDS_20260922.md): spawn, linkto, movegravity
// (ScriptEntCmd_GravityMove writes TR_GRAVITY = C1 wire mode 5 and accepts
// script_origin/script_model/script_brushmodel), rotatepitch, bullettrace,
// hide, delete; G_RunFrame runs both the launcher and the linked brushmodel
// through G_RunMover, which follows the tagInfo link every frame.
//
// Two deliberate differences from T5, both about the state, not the visual:
//   * T5 sets "mid_tear" here and "destroyed" only after the flight, and is
//     threaded off level by the note handler.  This port's tear loop and
//     open_barrier_paths gate on all_chunks_destroyed(), verified live on the
//     synchronous state, so the state and the .destroyed flag are still set
//     synchronously and only the flight is the threaded part
//     (remove_chunk_fly).  The zombie mantles as soon as the last note fires,
//     as before; the last board is still in the air when it does, as in T5.
//   * chunk.in_flight guards a repair that lands inside the ~1 s flight:
//     replace_chunk unlinks the board from its launcher first, and the flight
//     thread does not hide a board whose repair has started under it (its
//     state is no longer "destroyed").
//
// STILL NOT TAKEN: the bar / grate arms (:1770-:1900, :1995-:2060) and the
// per-piece anim_grate_index (remove_chunk_rotate_grate :2083) -- no chunk of
// this map is a bar or a grate.
remove_chunk( chunk, node, destroy_immediately, zomb )
{
	if ( !isDefined( chunk ) || is_true( chunk.destroyed ) )
		return;

	chunk update_states( "destroyed" );
	chunk.destroyed = true;
	chunk notSolid();
	chunk notify( "destroyed" );

	chunk thread zombie_boardtear_audio_offset();
	chunk thread zombie_boardtear_offset_fx();

	// T5 needs the barrier's facing for the launch direction; the "end
	// without notetrack" fallback in the spawner passes it too.  With no node
	// there is nothing to aim along, so the board just goes (the old
	// behaviour), and the log says so.
	if ( !isDefined( node ) || !isDefined( node.angles ) )
	{
		chunk hide();
		codolprintln( "CoDOL zm board: chunk " + chunk.script_noteworthy + " at " + chunk.origin + " hidden without a launch (no barrier node)" );
		return;
	}

	level thread remove_chunk_fly( chunk, node );
}

// The T5 :1900-:1990 flight, threaded off level so the zombie's notetrack
// loop is not held for the ~1 s of air time (T5 :1183 threads remove_chunk
// itself off level for the same reason).
remove_chunk_fly( chunk, node )
{
	chunk.in_flight = true;

	ent = spawn( "script_origin", chunk.origin );
	ent.angles = node.angles + ( 0, 180, 0 );

	// T5 "DCS 030711": script_move_dist caps the flight for boards in
	// shallow closets.  The map carries 125 on every chunk.
	dist = 100;
	if ( isDefined( chunk.script_move_dist ) && int( chunk.script_move_dist ) > 100 )
		dist = 100 + randomInt( int( chunk.script_move_dist ) - 100 );
	else
		dist = 100 + randomInt( 100 );

	dest = ent.origin + ( anglesToForward( ent.angles ) * dist );
	trace = bulletTrace( dest + ( 0, 0, 16 ), dest + ( 0, 0, -200 ), false, undefined );
	if ( trace[ "fraction" ] == 1 )
		dest = dest + ( 0, 0, -200 );
	else
		dest = trace[ "position" ];

	chunk linkTo( ent );

	time = ent fake_physicslaunch( dest, 200 + randomInt( 100 ) );

	spin = "pitch 180 over " + ( time * 0.5 );
	if ( randomInt( 100 ) > 40 )
	{
		ent rotatePitch( 180, time * 0.5 );
	}
	else
	{
		spin = "pitch 90 over " + time + " accel " + ( time * 0.5 );
		ent rotatePitch( 90, time, time * 0.5 );
	}

	codolprintln( "CoDOL zm board: chunk " + chunk.script_noteworthy + " at " + chunk.origin + " linked to launcher, movegravity vel " + ent.launch_velocity + " for " + time + " s to " + dest + " (dist " + dist + " along yaw " + ent.angles[ 1 ] + "), spin " + spin + ", fx impacts/fx_large_woodhit, snd zmb_break_board + zmb_board_slam" );

	wait( time );

	// A repair inside the flight has already unlinked and shown the board
	// and moved it to "mid_repair"; .destroyed stays true until it lands.
	if ( chunk get_chunk_state() == "destroyed" )
		chunk hide();

	wait( 0.1 );

	if ( chunk islinked() )
		chunk unlink();
	chunk.in_flight = false;
	ent delete();
}

// T5 _zombiemode_spawner.gsc:2105 zombie_boardtear_audio_offset.  T5's own
// comment: "I added an audio offset to give more length and depth to the
// tearing off feeling".
//
// T5 branches on chunk.material over glass / metal / rock / metal_vent /
// default.  MEASURED, docs/zone_assets/ALL_sound.txt: of T5's five aliases
// CoDOL ships only the wood-board pair, as zmb_break_board and zmb_board_slam
// (there is a zmb_break_glass_barrier too, and it is used).  The metal,
// rock and vent arms are dropped rather than pointed at a substitute alias,
// because a rock barricade would then sound like wood.
zombie_boardtear_audio_offset()
{
	// T5 gates only glass (and metal) on already_broken: a pane breaks once,
	// a board sounds every time it is torn.  Gating every material made a
	// board that had been repaired once tear off silently from then on.
	if ( isDefined( self.material ) && self.material == "glass" )
	{
		if ( is_true( self.already_broken ) )
			return;
		self.already_broken = true;
		self playSound( "zmb_break_glass_barrier" );
		wait( randomFloatRange( 0.3, 0.6 ) );
		self playSound( "zmb_break_glass_barrier" );
		return;
	}

	self play_sound_on_ent( "break_barrier_piece" );
	wait( randomFloatRange( 0.3, 0.6 ) );
	self play_sound_on_ent( "board_slam" );
}

// T5 _zombiemode_spawner.gsc:1428 zombie_boardtear_offset_fx_horizontle and
// :1493 _verticle, merged.
//
// T5 picks between them on the chunk's script_noteworthy slot ("4","6","5","1"
// horizontal, the rest vertical) and the only difference between the two
// bodies is the sign of the two wood-chunk FX offsets.  That is reproduced.
//
// T5's clientside variants (setclientflag with
// level._ZOMBIE_SCRIPTMOVER_FLAG_BOARD_HORIZONTAL_FX) are NOT used: those
// flag bits are T5 client-script numbers and this server publishes no
// equivalent, so the FX is played server-side, which is the branch T5 itself
// takes when level.use_clientside_board_fx is unset.
zombie_boardtear_offset_fx()
{
	fx = level._effect[ "wood_chunk_destroy" ];
	if ( !isDefined( fx ) )
		return;

	horizontal = false;
	if ( isDefined( self.script_noteworthy ) )
	{
		n = self.script_noteworthy;
		if ( n == "1" || n == "4" || n == "5" || n == "6" )
			horizontal = true;
	}

	if ( horizontal )
	{
		playFx( fx, self.origin + ( 0, 0, 30 ) );
		wait( randomFloatRange( 0.2, 0.4 ) );
		playFx( fx, self.origin + ( 0, 0, -30 ) );
		return;
	}

	playFx( fx, self.origin + ( 0, 30, 0 ) );
	wait( randomFloatRange( 0.2, 0.4 ) );
	playFx( fx, self.origin + ( 0, -30, 0 ) );
}

// T5 :2174 replace_chunk, the "board" arm, with its animation.
//
// THE BOARD FLIES BACK.  2026-10-05.  User report: rebuilding a window just
// popped the board into its slot.  The previous pass set .origin/.angles to
// og_origin/og_angles and showed the chunk in one frame.  T5 shows the board
// where it LANDED after the tear (remove_chunk_fly leaves it hidden there,
// outside the window on the floor) and flies it back.  T5 runs two blocks in
// sequence for script_parameters "board" (:2208-:2240 then :2335-:2356):
//
//   Show(); hover sound "rebuild_barrier_hover" at the chunk
//   RotateTo( angles + (0,-9,0), 0.1 ); RotateTo( angles + (0,18,0), 0.1 )
//                                                  -- a wobble where it lies
//   MoveTo( (x, y, og_z), 0.15 ); RotateTo( og_angles, 0.3 ); wait 0.2
//                                                  -- rises to slot height,
//                                                     turns square
//   RotateTo( -9 ), RotateTo( +18 ), RotateTo( og_angles, 0.1 )
//   MoveTo( og_origin, 0.1 ); waittill movedone    -- slams into the slot
//   repair FX/quake thread, Solid(), update_states( "repaired" ),
//   slam sound "barrier_rebuild_slam" at the chunk.
//
// The carpenter perk pop (T5's specialty_fastreload arm) skips the wobble:
// RotateTo( og_angles, 0.15 ), wait 0.1, MoveTo( og_origin ).  T5 moves in
// 0.05 s; that is shorter than one 20 Hz snapshot interval, so C1 could
// receive the trajectory after it had already ended and only see the board
// appear in its slot -- this uses 0.1 s.
//
// The waits are explicit rather than on "rotatedone"/"movedone", so a mover
// notify that never comes cannot strand the board mid-air; after the last
// move the origin and angles are written to the exact og values, which ends
// the TR_LINEAR_STOP trajectory (C1 wire mode 3, interpolated by C1's
// BG_EvaluateTrajectory) with a stationary one at the same spot.
//
// State while it flies (T5's "mid_repair"): not "destroyed", so the repair
// loop does not pick it again and all_chunks_destroyed() is false (a zombie
// does not mantle through a window whose board is coming back); not
// "repaired", so all_chunks_intact() is false.  .destroyed stays TRUE until
// the board lands, so get_closest_non_destroyed_chunk() does not hand a
// zombie a board that is still in the air and remove_chunk() refuses it; a
// second replace_chunk (the carpenter power-up) is refused by the state
// check below.  The board is not solid until it is in its slot.
replace_chunk( chunk, fast, via_powerup )
{
	if ( !isDefined( chunk ) )
		return;
	// Only a board that is down: not one already flying back, not one in
	// its slot (or one a zombie is pulling on).
	if ( chunk get_chunk_state() != "destroyed" )
		return;

	chunk update_states( "mid_repair" );
	chunk notify( "mid_repair" );

	// Repaired while still in the air: take it off the launcher first, or the
	// moves below would be overwritten by the link every frame.
	if ( chunk islinked() )
		chunk unlink();

	og_origin = chunk.og_origin;
	og_angles = chunk.og_angles;
	if ( !isDefined( og_origin ) )
		og_origin = chunk.origin;
	if ( !isDefined( og_angles ) )
		og_angles = chunk.angles;

	how = "";
	if ( is_true( fast ) )
		how = " (carpenter)";
	codolprintln( "CoDOL zm board: chunk " + chunk.script_noteworthy + " repair starts at " + chunk.origin + " angles " + chunk.angles +
		" -> slot " + og_origin + " angles " + og_angles + how );

	chunk show();
	chunk notSolid();

	if ( !is_true( via_powerup ) )
		play_sound_at_pos( "rebuild_barrier_hover", chunk.origin );

	if ( is_true( fast ) )
	{
		chunk rotateTo( og_angles, 0.15 );
		wait( 0.15 );
		wait( 0.1 );
		chunk moveTo( og_origin, 0.1 );
		wait( 0.1 );
	}
	else
	{
		only_z = ( chunk.origin[ 0 ], chunk.origin[ 1 ], og_origin[ 2 ] );

		chunk rotateTo( chunk.angles + ( 0, -9, 0 ), 0.1 );
		wait( 0.1 );
		chunk rotateTo( chunk.angles + ( 0, 18, 0 ), 0.1 );
		wait( 0.1 );
		chunk moveTo( only_z, 0.15 );
		chunk rotateTo( og_angles, 0.3 );
		wait( 0.3 );
		wait( 0.2 );

		chunk rotateTo( chunk.angles + ( 0, -9, 0 ), 0.1 );
		wait( 0.1 );
		chunk rotateTo( chunk.angles + ( 0, 18, 0 ), 0.1 );
		wait( 0.1 );
		chunk rotateTo( og_angles, 0.1 );
		wait( 0.1 );
		chunk moveTo( og_origin, 0.1 );
		wait( 0.1 );
	}

	// Let the last move's end time pass, then pin the board exactly (T5
	// ensure_chunk_is_back_to_origin).
	wait( 0.05 );
	chunk.origin = og_origin;
	chunk.angles = og_angles;

	chunk thread zombie_boardtear_repair_fx();

	chunk solid();
	chunk.destroyed = false;
	chunk update_states( "repaired" );
	chunk notify( "repaired" );

	play_sound_at_pos( "barrier_rebuild_slam", chunk.origin );

	codolprintln( "CoDOL zm board: chunk " + chunk.script_noteworthy + " repaired in its slot at " + chunk.origin + " angles " + chunk.angles );
}

// T5 :2605 zombie_boardtear_audio_plus_fx_offset_repair_horizontal and :2642
// _verticle, wood arm, merged as the tear pair is above.  The repair pair
// takes slots "1","5","6" as horizontal (the tear pair also takes "4"; that
// is T5's own inconsistency, kept).  The offsets are along z for a
// horizontal board and along x for a vertical one, as in T5.
zombie_boardtear_repair_fx()
{
	earthquake( randomFloatRange( 0.3, 0.4 ), randomFloatRange( 0.2, 0.4 ), self.origin, 150 );

	fx = level._effect[ "wood_chunk_destroy" ];

	horizontal = false;
	if ( isDefined( self.script_noteworthy ) )
	{
		n = self.script_noteworthy;
		if ( n == "1" || n == "5" || n == "6" )
			horizontal = true;
	}

	offset = ( 30, 0, 0 );
	if ( horizontal )
		offset = ( 0, 0, 30 );

	if ( isDefined( fx ) )
		playFx( fx, self.origin + offset );
	wait( randomFloatRange( 0.3, 0.6 ) );
	self play_sound_on_ent( "break_barrier_piece" );
	if ( isDefined( fx ) )
		playFx( fx, self.origin - offset );
}

// The repair loop's wait for boards still flying back (state "mid_repair").
wait_for_mid_repair( barrier_chunks, timeout )
{
	end = getTime() + int( timeout * 1000 );
	for ( ;; )
	{
		flying = false;
		for ( i = 0; i < barrier_chunks.size; i++ )
		{
			if ( barrier_chunks[ i ] get_chunk_state() == "mid_repair" )
				flying = true;
		}
		if ( !flying || getTime() >= end )
			return;
		wait( 0.05 );
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
	{
		targets[ i ] door_classify( self );
		if ( targets[ i ].classname == "script_brushmodel" )
			targets[ i ] disconnectpaths();
	}

	cost = 1000;
	if ( isDefined( self.zombie_cost ) )
		cost = self.zombie_cost;

	self setCursorHint( "HINT_NOICON" );
	self thread door_think();

	if ( self.script_noteworthy == "electric_door" || self.script_noteworthy == "electric_buyable_door" )
	{
		self setHintString( &"ZOMBIE_NEED_POWER" );
		return;
	}

	self set_hint_string( self, "default_buy_door_" + cost, "generic_open_door" );
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
		who maps\mp\gametypes\_zombiemode_audio::no_cash_vox();
		return false;
	}

	who maps\mp\gametypes\_zombiemode_score::minus_to_player_score( cost );
	// CHALLENGE_ZM_DOORS "open &&1 doors".
	who maps\mp\gametypes\_zombiemode_challenges::zm_challenge_inc( "zm_doors" );
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

			self set_hint_string( self, "default_buy_door_" + cost, "generic_open_door" );

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

	// MEASURED (qconsole.log, "cannot set field of removed entity"): the
	// door trigger can be deleted by door_opened() while this thread is
	// still between its waittill and this assignment.
	if ( !isDefined( self ) )
		return;

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
	level notify( "zm_cortex_event", "door" );   // _zombiemode_cortex reaction

	for ( i = 0; i < self.doors.size; i++ )
		self.doors[ i ] thread door_activate();

	if ( isDefined( self.clip ) )
		self.clip delete();

	// T5 :816 -- "get all trigs for the door, we might want a trigger on both
	// sides".  Every trigger that targets these door elements goes, not just
	// the one that was bought (zm_asylum_sh authors two zombie_door triggers
	// on target auto172).  T5 trigger_off()s them; this port deletes, as it
	// always did for self.
	self delete_blocker_triggers();
}

// T5 :584, minus ConnectPaths (not registered) and the "anim" style.
door_activate()
{
	time = 1;
	if ( isDefined( self.script_transition_time ) )
		time = self.script_transition_time;

	self notSolid();
	if ( self.classname == "script_brushmodel" )
		self connectpaths();

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

	self set_hint_string( self, "default_buy_debris_" + cost, "generic_clear_debris" );
	self setCursorHint( "HINT_NOICON" );

	if ( isDefined( self.script_flag ) )
		flag_init( self.script_flag );
	if ( isDefined( self.target ) )
	{
		blockers = getEntArray( self.target, "targetname" );
		for ( i = 0; i < blockers.size; i++ )
		{
			if ( blockers[ i ].classname == "script_brushmodel" )
				blockers[ i ] disconnectpaths();
		}
	}

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
			who maps\mp\gametypes\_zombiemode_audio::no_cash_vox();
			continue;
		}

		who maps\mp\gametypes\_zombiemode_score::minus_to_player_score( cost );
		// CHALLENGE_ZM_DOORS counts debris too: it is the same bought
		// blocker, and zm_prototype_sh has no other kind.
		who maps\mp\gametypes\_zombiemode_challenges::zm_challenge_inc( "zm_doors" );

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
		{
			clip connectpaths();
			clip delete();
		}

		// T5 :1068 -- "get all trigs, we might want a trigger on both sides
		// of some junk sometimes".  MEASURED (zm_prototype_sh.ff entity
		// string): the stairs debris has two zombie_debris triggers, at
		// 167 42 144 (first floor) and 172 147 194 (second floor), both
		// target "upstairs_blocker"; deleting only self left the other one
		// buyable.  zm_asylum_sh's north/south_upstairs_blocker are pairs too.
		self delete_blocker_triggers();
		return;
	}
}

// T5 :816 / :1068.  self is the bought door or debris trigger.  Deletes it
// and every other entity whose target is the same blocker, i.e. the buy
// triggers on the other side(s).  self goes last so the lookup still has it.
delete_blocker_triggers()
{
	if ( isDefined( self.target ) )
	{
		all_trigs = getEntArray( self.target, "target" );
		for ( i = 0; i < all_trigs.size; i++ )
		{
			if ( isDefined( all_trigs[ i ] ) && all_trigs[ i ] != self )
				all_trigs[ i ] delete();
		}
	}

	self delete();
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
