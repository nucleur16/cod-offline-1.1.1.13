// coop soldier animscript: soldier_move.  The run / patrol-walk cycle, the
// starts and stops around it, and turns on the move.
//
// The native frame (CODOL_ActorRunFrame) selects <animscript>_move whenever
// the actor has a route and is not in combat or pain; the native mover owns
// translation (the clip's root-motion pace along the route), this module owns
// the pose.  While a start, stop or turn clip plays the module puts the
// actor in "zonly_physics" + "face current", so the clip's own root motion
// and rotation move and turn him (IW4 AI_ANIM_USE_BOTH_DELTAS).
//
// IW4 animscripts\run.gsc, move.gsc and cover_arrival.gsc, as far as the
// native mover allows:
//  * EXITS (cover_arrival startMoveTransition / doCoverExitAnimation): a
//    soldier leaving a cover node plays the node type's exit toward the
//    route's direction (corner_stand[LR]_trans_OUT_*, CornerCr[LR]_trans_OUT_*,
//    coverstand_trans_OUT_*, covercrouch_run_out_*, their CQB variants);
//    one setting off from an exposed stand or kneel plays IW4's exposed
//    starts (stand_2_run_*, crouch_2run_*, CQB_start_*, surprise_start).
//    The direction is IW4's: the clip whose yaw change best matches the
//    path, checked for room to play (maymovefrompointtopoint) and for a
//    path longer than the clip.
//  * ARRIVALS (cover_arrival startCoverApproach): heading for a claimed cover
//    node he plays the node type's arrival (corner_stand[LR]_trans_IN_*,
//    CornerCr[LR]_trans_IN_*, coverstand_trans_IN_*, covercrouch_run_in_*)
//    from the distance the clip covers, turned so the clip ends on the node,
//    and arrives facing the node's hide direction; a soldier stopping short
//    of a goal with no enemy plays IW4's exposed stop (run_2_stand_*).
//  * MOVING TURNS (move.gsc pathChange / animset init_moving_turn_animations):
//    a path that bends 45 degrees or more is taken with run_turn_L/R45..135
//    or run_turn_180.
//  * MoveStandCombatNormal: the combat run is run_lowready_F, with IW4's
//    short sprints for variation (sprint1_loop) while hunting;
//  * RunNGun: with a visible enemy within 120 degrees of his path he runs
//    with the weapon up at him (run_n_gun_F/L/R) and fires bursts on the
//    additive %add_fire layer (move.gsc shootWhileMoving);
//  * a soldier with no enemy who is not hunting walks IW4 _patrol's bored
//    patrol walk with its twitch;
//  * self.coop_cqb (a mission's choice) moves him at IW4's CQB walk/run;
//  * a player grenade at his feet is dived from or thrown back, and an enemy
//    in reach is struck (melee.gsc run_2_melee_charge).
// Every clip here is in C1's common_human animlist.

#include animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self endon( "killanimscript" );
	// A helper endon terminates its own call frame; the owning behavior must
	// also end on death so it cannot evaluate a cancelled helper result.
	self endon( "death" );

	init_transitions();
	watch_player_grenades();
	update_cover_use();

	fromCover = isDefined( self.coop_exit_time ) && getTime() - self.coop_exit_time < 500 && isDefined( self.coop_exit_type );
	exitType = self.coop_exit_type;
	self.coop_exit_time = undefined;
	self.coop_in_cover = undefined;
	wasMovement = self.a.movement;

	self orientmode( "face motion" );
	self animmode( "normal" );
	stop_aiming();

	// A killed module leaves its run-and-gun state behind.
	self.coop_rng_dir = undefined;
	self.coop_rng_firing = undefined;
	self.coop_moving_fire = undefined;

	exited = false;
	if ( fromCover )
		exited = start_move_transition( exitType );
	else if ( isDefined( self.coop_idle_state ) && self.coop_idle_state == "bored" && !isDefined( self.enemy ) && !isDefined( self.coop_hunting ) )
		exited = patrol_start();
	else if ( isDefined( wasMovement ) && wasMovement == "stop" )
		exited = start_move_transition( undefined );
	self.coop_idle_state = undefined;
	if ( !exited && isDefined( self.a.pose ) && self.a.pose == "crouch" )
		rise_into_run();
	self.a.pose = "stand";
	set_anim_array( "stand" );
	self.coop_arrival_node = undefined;

	self.coop_sprint_until = 0;
	if ( !isDefined( self.coop_next_sprint ) )
		self.coop_next_sprint = getTime() + 4000 + randomInt( 6000 );

	lastAnim = undefined;
	lastSet = 0;
	for ( ;; )
	{
		if ( react_to_grenade() )
		{
			lastAnim = undefined;
			self orientmode( "face motion" );
			continue;
		}
		enemy = self.enemy;
		if ( isDefined( enemy ) && can_melee( enemy ) )
		{
			stop_run_n_gun();
			melee_strike( enemy );
			lastAnim = undefined;
			self orientmode( "face motion" );
			continue;
		}
		if ( try_cover_arrival() || try_exposed_arrival() )
		{
			lastAnim = undefined;
			continue;
		}
		if ( try_run_turn() )
		{
			lastAnim = undefined;
			continue;
		}
		if ( try_run_reaction() || try_run_reload() )
		{
			lastAnim = undefined;
			continue;
		}

		if ( isDefined( enemy ) || isDefined( self.coop_hunting ) )
		{
			self.a.movement = "run";
			if ( isDefined( enemy ) && run_n_gun_dir( enemy ) != "" )
			{
				run_n_gun( enemy );
				lastAnim = undefined;
				continue;
			}
			stop_run_n_gun();
			moveAnim = run_anim( enemy );
		}
		else
		{
			stop_run_n_gun();
			self.a.movement = "walk";
			moveAnim = walk_anim();
		}

		if ( !isDefined( lastAnim ) || lastAnim != moveAnim || getTime() - lastSet >= 400 )
		{
			self setFlaggedAnimKnobAll( "runanim", moveAnim, %body, 1, 0.2, self.moveplaybackrate );
			lastAnim = moveAnim;
			lastSet = getTime();
		}
		wait( 0.05 );
	}
}

// IW4 _patrol: a bored patrol sets off from its idle with
// patrol_bored_2_walk, or the 180-degree turn version when his way lies
// behind him.
patrol_start()
{
	self endon( "killanimscript" );
	dir = self codol_actorLookahead();
	if ( !isDefined( dir ) )
		return false;
	yaw = angleClamp180( vectorToYaw( dir ) - self.angles[ 1 ] );
	clip = %patrol_bored_2_walk;
	if ( abs( yaw ) > 135 )
		clip = %patrol_bored_2_walk_180turn;
	else if ( abs( yaw ) > 45 )
		return false;
	trace_anim( "patrol start" );
	stat_bump( "patrol_start" );
	self orientmode( "face current" );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "patrolstart", clip, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( clip ) * 0.9, "patrolstart" );
	self orientmode( "face motion" );
	self animmode( "normal", false );
	self.a.movement = "walk";
	return true;
}

// IW4 reactions.gsc runReactions: rounds cracking past a running soldier
// make him duck, flinch or stumble on.
try_run_reaction()
{
	self endon( "killanimscript" );
	if ( self.a.movement != "run" || isDefined( self.coop_rng_dir ) || !is_suppressed() )
		return false;
	if ( isDefined( self.coop_next_react ) && getTime() < self.coop_next_react )
		return false;
	self.coop_next_react = getTime() + 5000 + randomInt( 5000 );
	clip = pick( list3( %run_react_duck, %run_react_flinch, %run_react_stumble ) );
	endPos = delta_end( self.origin, self.angles[ 1 ], getMoveDelta( clip, 0, 1 ) );
	if ( !may_move( self.origin, endPos ) )
		return false;
	trace_anim( "run reaction" );
	stat_bump( "run_reaction" );
	self orientmode( "face current" );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "reactanim", clip, %body, 1, 0.1, 1 );
	DoNoteTracksForTime( getAnimLength( clip ) * 0.9, "reactanim" );
	self orientmode( "face motion" );
	self animmode( "normal", false );
	return true;
}

// IW4 run.gsc ReloadWhileRunning: a soldier on the move with half a clip
// gone and nobody in his sights reloads at a run.
try_run_reload()
{
	self endon( "killanimscript" );
	if ( projectile_weapon() )
		return false;
	if ( self.a.movement != "run" || isDefined( self.coop_rng_dir ) || isDefined( self.coop_cqb ) )
		return false;
	if ( animscripts\soldier_combat::clip_remaining() > animscripts\soldier_combat::role_clip() * 0.5 )
		return false;
	if ( isDefined( self.enemy ) && isDefined( self.coop_rng_seen ) && self.coop_rng_seen )
		return false;
	trace_anim( "run reload" );
	stat_bump( "run_reload" );
	self setFlaggedAnimKnobAll( "reloadanim", %run_lowready_reload, %body, 1, 0.2, self.moveplaybackrate );
	DoNoteTracksForTime( getAnimLength( %run_lowready_reload ), "reloadanim" );
	self codol_actorReloadComplete();
	self.a.burstShots = 0;
	return true;
}

// IW4 coverExit["exposed_crouch"][8].
rise_into_run()
{
	self endon( "killanimscript" );
	self setFlaggedAnimKnobAllRestart( "exitanim", %crouch_2run_f, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( %crouch_2run_f ) * 0.8, "exitanim" );
}

// ---------------------------------------------------------------------------
// Starts: IW4 cover_arrival startMoveTransition / doCoverExitAnimation.
// ---------------------------------------------------------------------------

start_move_transition( exitType )
{
	self endon( "killanimscript" );
	if ( getDvar( "scr_coop_ai_transitions" ) == "0" )
		return false;

	dir = self codol_actorLookahead();
	if ( !isDefined( dir ) )
	{
		wait( 0.05 );
		dir = self codol_actorLookahead();
		if ( !isDefined( dir ) )
			return false;
	}
	isExposed = !isDefined( exitType );
	if ( isExposed )
	{
		pose = "stand";
		if ( isDefined( self.a.pose ) && self.a.pose == "crouch" )
			pose = "crouch";
		exitType = cover_trans_type( "Exposed", pose );
	}
	t = level.coop_trans;
	if ( !isDefined( t.exit[ exitType ] ) )
		return false;

	exitYaw = self.angles[ 1 ];
	leaveDir = ( 0 - dir[ 0 ], 0 - dir[ 1 ], 0 );
	i = best_transition( exitType, false, exitYaw, leaveDir );
	if ( i < 0 )
		return false;
	exitAnim = t.exit[ exitType ][ i ];
	delta = t.exitDist[ exitType ][ i ];
	exitPos = delta_end( self.origin, exitYaw, delta );
	if ( !may_move( self.origin, exitPos ) )
	{
		trace_anim( "exit " + exitType + " dir " + i + " blocked" );
		return false;
	}
	goal = self codol_actorGoal();
	if ( isDefined( goal ) && distanceSquared( self.origin, goal ) < distanceSquared( self.origin, exitPos ) * 1.5625 )
		return false;

	trace_anim( "exit " + exitType + " dir " + i );
	stat_bump( "exit_" + exitType + "_" + i );
	self orientmode( "face current" );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "coverexit", exitAnim, %body, 1, 0.2, 1 );
	len = getAnimLength( exitAnim );
	frac = t.exitMoveFrac[ exitType ][ i ];
	DoNoteTracksForTime( len * frac, "coverexit" );
	// IW4: from code_move the path moves him while the clip finishes.
	self.a.movement = "run";
	self orientmode( "face motion" );
	self animmode( "normal", false );
	remaining = len * ( 1 - frac ) - 0.2;
	if ( remaining > 0 )
		DoNoteTracksForTime( remaining, "coverexit" );
	self.a.pose = "stand";
	return true;
}

// ---------------------------------------------------------------------------
// Stops: IW4 cover_arrival startCoverApproach / doLastMinuteExposedApproach.
// ---------------------------------------------------------------------------

try_cover_arrival()
{
	self endon( "killanimscript" );
	if ( getDvar( "scr_coop_ai_transitions" ) == "0" )
		return false;
	point = self codol_actorCoverPoint();
	if ( !isDefined( point ) || self codol_actorCoverArrived() )
		return false;
	node = self codol_actorCoverNode();
	if ( !isDefined( node ) )
		return false;
	if ( isDefined( self.coop_arrival_node ) && self.coop_arrival_node == node )
		return false;
	if ( abs( point[ 2 ] - self.origin[ 2 ] ) > 24 )
		return false;

	if ( !isDefined( self.coop_cover_pose_node ) || self.coop_cover_pose_node != node )
	{
		self.coop_cover_pose_node = node;
		self.coop_cover_pose = cover_pose_for( node );
	}
	type = cover_trans_type( node.type, self.coop_cover_pose );
	t = level.coop_trans;
	dist = distance2d( self.origin, point );
	if ( dist > t.longest[ type ] + 16 )
		return false;

	flat = ( point[ 0 ] - self.origin[ 0 ], point[ 1 ] - self.origin[ 1 ], 0 );
	if ( lengthSquared( flat ) < 1 )
		return false;
	dir = vectorNormalize( flat );
	arrivalYaw = node.angles[ 1 ] + node_stance_yaw_offset( type );
	// Only the arrivals from the node's open side: the ones from behind a
	// corner bend round the wall (IW4 plays them in two halves).
	i = best_transition( type, true, arrivalYaw, dir, true );
	if ( i < 0 )
		return false;
	delta = t.transDist[ type ][ i ];
	clipLen = length( ( delta[ 0 ], delta[ 1 ], 0 ) );
	if ( dist > clipLen + 8 )
		return false;
	// Committed to this node one way or the other from here.
	self.coop_arrival_node = node;
	if ( dist < clipLen - 40 || self.coop_trans_dot < 0.6 )
	{
		trace_anim( "arrival " + type + " skipped (dist " + int( dist ) + " clip " + int( clipLen ) + " dot " + self.coop_trans_dot + ")" );
		return false;
	}
	if ( !may_move( self.origin, point ) )
		return false;

	arrivalAnim = t.trans[ type ][ i ];
	requiredYaw = vectorToYaw( flat ) - vectorToYaw( ( delta[ 0 ], delta[ 1 ], 0 ) );
	trace_anim( "arrival " + type + " dir " + i + " from " + int( dist ) + " (clip " + int( clipLen ) + ")" );
	stat_bump( "arrival_" + type + "_" + i );
	stop_run_n_gun();
	self.safeToChangeScript = false;
	self orientmode( "face angle", requiredYaw, 1 );
	self orientmode( "face current" );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "coverArrival", arrivalAnim, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( arrivalAnim ), "coverArrival" );
	self.a.pose = t.endStance[ type ];
	self.a.movement = "stop";
	self.safeToChangeScript = true;
	self orientmode( "face angle", arrivalYaw );
	self animmode( "normal", false );
	// Hold the arrival's last pose until the frame hands him to combat.
	wait( 0.1 );
	return true;
}

// A soldier walking or running to a plain goal (a hunt, a mission point)
// stops with IW4's exposed stop when he has no enemy to fight.
try_exposed_arrival()
{
	self endon( "killanimscript" );
	if ( getDvar( "scr_coop_ai_transitions" ) == "0" )
		return false;
	if ( isDefined( self.enemy ) || isDefined( self codol_actorCoverPoint() ) || self.a.movement != "run" )
		return false;
	goal = self codol_actorGoal();
	if ( !isDefined( goal ) || abs( goal[ 2 ] - self.origin[ 2 ] ) > 24 )
		return false;
	radius = 4;
	if ( isDefined( self.goalradius ) )
		radius = self.goalradius;
	dist = distance2d( self.origin, goal ) - radius;
	t = level.coop_trans;
	type = cover_trans_type( "Exposed", "stand" );
	stopAnim = t.trans[ type ][ 2 ];
	delta = t.transDist[ type ][ 2 ];
	clipLen = length( ( delta[ 0 ], delta[ 1 ], 0 ) );
	if ( dist > clipLen + 6 || dist < clipLen - 24 )
		return false;
	if ( isDefined( self.coop_exposed_stop_goal ) && distanceSquared( self.coop_exposed_stop_goal, goal ) < 16 )
		return false;
	self.coop_exposed_stop_goal = goal;
	dir = self codol_actorLookahead();
	if ( !isDefined( dir ) )
		return false;
	stopPos = self.origin + dir * clipLen;
	if ( !may_move( self.origin, stopPos ) )
		return false;
	trace_anim( "exposed stop from " + int( dist ) );
	stat_bump( "arrival_exposed_stop" );
	self orientmode( "face angle", vectorToYaw( dir ), 1 );
	self orientmode( "face current" );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "exposedArrival", stopAnim, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( stopAnim ), "exposedArrival" );
	self.a.movement = "stop";
	self orientmode( "face motion" );
	self animmode( "normal", false );
	return true;
}

// ---------------------------------------------------------------------------
// Turns on the move: IW4 animset init_moving_turn_animations
// (anim.runTurnAnims) and move.gsc's path-change turns.
// ---------------------------------------------------------------------------

try_run_turn()
{
	self endon( "killanimscript" );
	if ( getDvar( "scr_coop_ai_transitions" ) == "0" )
		return false;
	if ( self.a.movement != "run" && !( isDefined( self.coop_cqb ) && self.a.movement == "walk" ) )
		return false;
	if ( isDefined( self.coop_next_run_turn ) && getTime() < self.coop_next_run_turn )
		return false;
	dir = self codol_actorLookahead();
	if ( !isDefined( dir ) )
		return false;
	yaw = angleClamp180( vectorToYaw( dir ) - self.angles[ 1 ] );
	if ( abs( yaw ) < 45 )
		return false;
	goal = self codol_actorGoal();
	if ( isDefined( goal ) && distanceSquared( goal, self.origin ) < 160 * 160 )
		return false;
	turnAnim = run_turn_anim( yaw );
	endPos = delta_end( self.origin, self.angles[ 1 ], getMoveDelta( turnAnim, 0, 1 ) );
	if ( !may_move( self.origin, endPos ) )
		return false;
	trace_anim( "run turn " + int( yaw ) );
	stat_bump( "run_turn" );
	self.coop_next_run_turn = getTime() + 1200;
	stop_run_n_gun();
	self orientmode( "face current" );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "turnanim", turnAnim, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( turnAnim ) * 0.8, "turnanim" );
	self orientmode( "face motion" );
	self animmode( "normal", false );
	return true;
}

run_turn_anim( yaw )
{
	// IW4 anim.cqbTurnAnims for a CQB walker.
	if ( isDefined( self.coop_cqb ) )
	{
		if ( yaw > 157.5 || yaw < -157.5 )
			return %cqb_walk_turn_2;
		if ( yaw > 112.5 )
			return %cqb_walk_turn_1;
		if ( yaw > 67.5 )
			return %cqb_walk_turn_4;
		if ( yaw > 0 )
			return %cqb_walk_turn_7;
		if ( yaw < -112.5 )
			return %cqb_walk_turn_3;
		if ( yaw < -67.5 )
			return %cqb_walk_turn_6;
		return %cqb_walk_turn_9;
	}
	if ( yaw > 157.5 || yaw < -157.5 )
		return %run_turn_180;
	if ( yaw > 112.5 )
		return %run_turn_l135;
	if ( yaw > 67.5 )
		return %run_turn_l90;
	if ( yaw > 0 )
		return %run_turn_l45;
	if ( yaw < -112.5 )
		return %run_turn_r135;
	if ( yaw < -67.5 )
		return %run_turn_r90;
	return %run_turn_r45;
}

run_anim( enemy )
{
	if ( isDefined( self.coop_cqb ) )
		return %run_cqb_f_search_v1;

	// IW4 ShouldSprintForVariation: a hunter with no one in sight now and
	// then breaks into a short sprint.
	now = getTime();
	if ( !isDefined( enemy ) && isDefined( self.coop_hunting ) )
	{
		if ( now < self.coop_sprint_until )
			return sprint_anim();
		if ( now >= self.coop_next_sprint )
		{
			self.coop_next_sprint = now + 8000 + randomInt( 8000 );
			if ( randomInt( 3 ) == 0 )
			{
				self.coop_sprint_until = now + 1500 + randomInt( 1500 );
				self.coop_sprint_anim = undefined;
				return sprint_anim();
			}
		}
	}
	return %run_lowready_f;
}

// IW4 animset run move set: the sprint and its distant variant.
sprint_anim()
{
	if ( !isDefined( self.coop_sprint_anim ) )
	{
		self.coop_sprint_anim = %sprint1_loop;
		if ( randomInt( 2 ) )
			self.coop_sprint_anim = %sprint_loop_distant;
	}
	return self.coop_sprint_anim;
}

walk_anim()
{
	if ( isDefined( self.coop_cqb ) )
		return %walk_cqb_f;
	// IW4 _patrol: the bored walk, its twitch once in a while (played to
	// its end, then back to the walk).
	now = getTime();
	if ( isDefined( self.coop_walk_twitch_until ) && now < self.coop_walk_twitch_until )
		return %patrol_bored_patrolwalk_twitch;
	if ( !isDefined( self.coop_next_walk_twitch ) )
		self.coop_next_walk_twitch = now + 4000 + randomInt( 8000 );
	if ( now >= self.coop_next_walk_twitch )
	{
		self.coop_next_walk_twitch = now + 8000 + randomInt( 12000 );
		self.coop_walk_twitch_until = now + int( getAnimLength( %patrol_bored_patrolwalk_twitch ) * 1000 );
		return %patrol_bored_patrolwalk_twitch;
	}
	return %patrol_bored_patrolwalk;
}

// ---------------------------------------------------------------------------
// Run and gun (IW4 run.gsc RunNGun / move.gsc shootWhileMoving).
// ---------------------------------------------------------------------------

// "F", "L", "R" for the run_n_gun clip that faces him, "" when he cannot be
// shot at from the run (no weapon that fires on the run, out of sight, too
// far round, or a friendly cast member the mission drives).
run_n_gun_dir( enemy )
{
	if ( getDvar( "scr_coop_ai_run_n_gun" ) == "0" )
		return "";
	if ( !isDefined( self.coop_weapon ) || self.coop_weapon == "" )
		return "";
	kind = weapon_kind();
	if ( kind != "rifle" && kind != "shotgun" )
		return "";
	if ( !isAlive( enemy ) )
		return "";
	if ( isDefined( self.ignoreall ) && self.ignoreall )
		return "";

	ang = vectorToAngles( enemy.origin - self.origin );
	// The path's direction, not the body's: a soldier backing away faces
	// the enemy while the route leads away from him.
	dir = self codol_actorLookahead();
	pathYaw = self.angles[ 1 ];
	if ( isDefined( dir ) )
		pathYaw = vectorToYaw( dir );
	yaw = angleClamp180( ang[ 1 ] - pathYaw );

	// Only every 0.4 s: the trace is the expensive part.
	if ( !isDefined( self.coop_rng_seen_time ) || getTime() - self.coop_rng_seen_time > 400 )
	{
		self.coop_rng_seen_time = getTime();
		from = self.origin + ( 0, 0, 58 );
		self.coop_rng_seen = bulletTracePassed( from, aim_point( enemy ), false, self );
	}
	if ( !self.coop_rng_seen )
		return "";

	if ( abs( yaw ) <= 50 )
		return "F";
	// IW4 MoveStandCombatNormal / shootWhileMoving: an enemy behind the
	// path is fired at running backwards (combatrun_backward), facing him.
	if ( abs( yaw ) > 120 )
		return "B";
	// IW4 set_animset_run_n_gun: past 90 degrees the _120 clips.
	if ( abs( yaw ) > 90 )
	{
		if ( yaw > 0 )
			return "LB";
		return "RB";
	}
	if ( yaw > 0 )
		return "L";
	return "R";
}

run_n_gun_anim( dir )
{
	if ( dir == "B" )
		return %combatrun_backward;
	if ( dir == "LB" )
		return %run_n_gun_l_120;
	if ( dir == "RB" )
		return %run_n_gun_r_120;
	if ( dir == "L" )
		return %run_n_gun_l;
	if ( dir == "R" )
		return %run_n_gun_r;
	return %run_n_gun_f;
}

run_n_gun( enemy )
{
	dir = run_n_gun_dir( enemy );
	if ( !isDefined( self.coop_rng_dir ) || self.coop_rng_dir != dir )
	{
		self.coop_rng_dir = dir;
		if ( dir == "B" )
		{
			self orientmode( "face enemy" );
			stat_bump( "run_backward_fire" );
		}
		else
			self orientmode( "face motion" );
		self setAnimKnobAll( run_n_gun_anim( dir ), %body, 1, 0.3, 1 );
	}
	if ( !isDefined( self.coop_rng_firing ) )
	{
		self.coop_rng_firing = true;
		self thread run_n_gun_fire();
	}
	DoNoteTracksForTime( 0.2, "runanim" );
}

stop_run_n_gun()
{
	if ( !isDefined( self.coop_rng_dir ) && !isDefined( self.coop_rng_firing ) )
		return;
	self.coop_rng_dir = undefined;
	self notify( "stop_run_n_gun" );
	self.coop_rng_firing = undefined;
	self clearAnim( %add_fire, 0.2 );
	self orientmode( "face motion" );
}

// Bursts while he runs: IW4's exposed fire clips on %add_fire over the run,
// the rounds through soldier_combat's shot (the same accuracy model, at the
// moving shooter's penalty).
run_n_gun_fire()
{
	self endon( "killanimscript" );
	self endon( "death" );
	self endon( "stop_run_n_gun" );

	wait( 0.3 + randomFloat( 0.4 ) );
	for ( ;; )
	{
		enemy = self.enemy;
		if ( !isDefined( enemy ) || !isAlive( enemy ) || !isDefined( self.coop_rng_dir ) )
		{
			wait( 0.2 );
			continue;
		}
		dist = distance( self.origin, enemy.origin );
		shots = int( animscripts\soldier_combat::role_burst( dist ) * 0.6 + 0.5 );
		if ( shots < 1 )
			shots = 1;

		animscripts\soldier_combat::begin_engagement( enemy );
		self setAnim( %add_fire, 1, 0.1 );
		self setFlaggedAnimKnobRestart( "fireanim", animscripts\soldier_combat::fire_anim_for( shots ), 1, 0, 1 );
		self.coop_moving_fire = true;
		delay = animscripts\soldier_combat::role_fire_delay();
		for ( i = 0; i < shots; i++ )
		{
			if ( !isDefined( enemy ) || !isAlive( enemy ) || !isDefined( self.coop_rng_dir ) )
				break;
			animscripts\soldier_combat::fire_shot( enemy );
			wait( delay );
		}
		self.coop_moving_fire = undefined;
		self clearAnim( %add_fire, 0.2 );
		wait( animscripts\soldier_combat::role_pause() * 1.5 );
	}
}
