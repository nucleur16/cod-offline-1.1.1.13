// coop soldier animscript: soldier_move.  The run / patrol-walk cycle.
//
// The native frame (CODOL_ActorRunFrame) selects <animscript>_move whenever
// the actor has a route and is not in combat or pain; the native mover owns
// translation (the clip's root-motion pace along the route), this module owns
// the pose.
//
// IW4 animscripts\run.gsc and move.gsc, as far as the native mover allows:
//  * MoveStandCombatNormal: the combat run is run_lowready_F (the tree's
//    "primary run animation", generic_human.atr combatrun_forward), with
//    IW4's short sprints for variation (sprint1_loop) while hunting;
//  * RunNGun: with a visible enemy within 120 degrees of his path he runs
//    with the weapon up at him (run_n_gun_F/L/R) and fires bursts on the
//    additive %add_fire layer (move.gsc shootWhileMoving);
//  * a crouched soldier rises into the run with IW4's exposed_crouch exit
//    (crouch_2run_F);
//  * a soldier with no enemy who is not hunting walks IW4 _patrol's bored
//    patrol walk with its twitch;
//  * self.coop_cqb (a mission's choice) moves him at IW4's CQB walk/run.
// Every clip here is in C1's common_human animlist.

#include animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self endon( "killanimscript" );

	self orientmode( "face motion" );
	self animmode( "normal" );
	stop_aiming();

	if ( isDefined( self.a.pose ) && self.a.pose == "crouch" )
		rise_into_run();
	self.a.pose = "stand";
	set_anim_array( "stand" );

	// A killed module leaves its run-and-gun state behind.
	self.coop_rng_dir = undefined;
	self.coop_rng_firing = undefined;
	self.coop_moving_fire = undefined;

	self.coop_sprint_until = 0;
	if ( !isDefined( self.coop_next_sprint ) )
		self.coop_next_sprint = getTime() + 4000 + randomInt( 6000 );

	for ( ;; )
	{
		enemy = self.enemy;
		if ( isDefined( enemy ) || isDefined( self.coop_hunting ) )
		{
			self.a.movement = "run";
			if ( isDefined( enemy ) && run_n_gun_dir( enemy ) != "" )
			{
				run_n_gun( enemy );
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

		self setFlaggedAnimKnobAll( "runanim", moveAnim, %body, 1, 0.2, self.moveplaybackrate );
		DoNoteTracksForTime( 0.4, "runanim" );
	}
}

// IW4 coverExit["exposed_crouch"][8].
rise_into_run()
{
	self endon( "killanimscript" );
	self setFlaggedAnimKnobAllRestart( "exitanim", %crouch_2run_f, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( %crouch_2run_f ) * 0.8, "exitanim" );
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
			return %sprint1_loop;
		if ( now >= self.coop_next_sprint )
		{
			self.coop_next_sprint = now + 8000 + randomInt( 8000 );
			if ( randomInt( 3 ) == 0 )
			{
				self.coop_sprint_until = now + 1500 + randomInt( 1500 );
				return %sprint1_loop;
			}
		}
	}
	return %run_lowready_f;
}

walk_anim()
{
	if ( isDefined( self.coop_cqb ) )
		return %walk_cqb_f;
	// IW4 _patrol: the bored walk, its twitch once in a while.
	if ( randomInt( 12 ) == 0 )
		return %patrol_bored_patrolwalk_twitch;
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
	yaw = angleClamp180( ang[ 1 ] - self.angles[ 1 ] );
	if ( abs( yaw ) > 120 )
		return "";

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
	if ( yaw > 0 )
		return "L";
	return "R";
}

run_n_gun_anim( dir )
{
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
