// coop soldier animscript: soldier_stop.  Standing still, no route.
//
// IW4 animscripts\stop.gsc for the exposed stand: with an enemy the soldier
// holds his weapon up in the exposed aim pose of his weapon family, aims at
// the enemy with the additive aim groups and plays the alert idles over it
// (combat_utility aimIdleThread), with the occasional exposed twitch
// (reactions.gsc).  With no enemy he stands at ease with IW4's casual idle
// set (utility.gsc setEnv) or, after a patrol walk, the bored patrol idles.

#include animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self notify( "stopScript" );
	self endon( "killanimscript" );

	self orientmode( "face default" );
	self animmode( "normal", false );
	wasMoving = self.a.movement;
	self.a.movement = "stop";
	self.coop_in_cover = undefined;
	init_transitions();
	watch_player_grenades();
	update_cover_use();

	// A cast member (_cooptraining.gsc) holds one looping idle, and plays a
	// gesture when the training script sets coop_pose_anim.  Each change is
	// sent once: re-sending a restart of a non-looping clip every cycle left
	// a fading clone at phase 0 that the actor-animation pose bootstrap
	// refuses, and a refused bootstrap drops that client's whole animation
	// delivery (MEASURED, VPS 2026-10-07: "pose bootstrap: ent 109 ref 1039:
	// non-looping clone at phase 0").
	if ( isDefined( self.coop_cast ) )
	{
		for ( ;; )
		{
			cast_pose( 0.3 );
			self waittill( "coop_cast_pose" );
		}
	}

	if ( !isDefined( self.a.pose ) )
		self.a.pose = "stand";

	self.coop_idle_state = undefined;
	for ( ;; )
	{
		if ( isDefined( self.enemy ) )
			alert_idle();
		else
			casual_idle( wasMoving );
		wasMoving = undefined;
		wait( 0.05 );
	}
}

// IW4 _patrol / utility setEnv: the way out of an at-ease idle when an
// enemy turns up (the bored patrol's look and advance, the casual idle's
// transition out).
leave_casual_idle()
{
	self endon( "killanimscript" );
	state = self.coop_idle_state;
	self.coop_idle_state = undefined;
	if ( !isDefined( state ) || self.a.pose != "stand" )
		return;
	clip = undefined;
	if ( state == "bored" )
		clip = pick( list2( %patrol_bored_react_look_advance, %patrol_bored_react_look_retreat ) );
	else if ( state == "casual" )
		clip = %casual_stand_idle_trans_out;
	if ( !isDefined( clip ) )
		return;
	trace_anim( "idle -> alert (" + state + ")" );
	stat_bump( "idle_to_alert_" + state );
	self.safeToChangeScript = false;
	self orientmode( "face current" );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "reactanim", clip, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( clip ), "reactanim" );
	self.safeToChangeScript = true;
}

// Weapon up, aiming at the enemy, until he has no enemy.
alert_idle()
{
	self endon( "killanimscript" );

	leave_casual_idle();
	self.a.pose = set_anim_array( self.a.pose );
	start_aiming( 0.3 );
	// IW4 exposed turning: the stance's turn clip carries the body round to
	// an enemy well off his facing; the aim tracks the rest.
	self orientmode( "face current" );
	self animmode( "zonly_physics", false );
	nextTwitch = getTime() + 4000 + randomInt( 6000 );
	while ( isDefined( self.enemy ) )
	{
		if ( react_to_grenade() )
		{
			self.a.pose = set_anim_array( self.a.pose );
			start_aiming( 0.2 );
			continue;
		}
		if ( turn_toward( self.enemy.origin, 45 ) )
			continue;
		// IW4 reactions.gsc: rounds cracking past a standing soldier.
		if ( self.a.pose == "stand" && is_suppressed() &&
		     ( !isDefined( self.coop_next_react ) || getTime() >= self.coop_next_react ) )
		{
			self.coop_next_react = getTime() + 4000 + randomInt( 4000 );
			stop_aiming();
			react = pick( list2( %exposed_idle_reacta, %exposed_idle_reactb ) );
			trace_anim( "exposed reaction" );
			stat_bump( "exposed_reaction" );
			self setFlaggedAnimKnobAllRestart( "reactanim", react, %body, 1, 0.1, 1 );
			DoNoteTracksForTime( getAnimLength( react ), "reactanim" );
			start_aiming( 0.2 );
			continue;
		}
		if ( getTime() >= nextTwitch && self.a.array[ "twitch" ].size )
		{
			// A whole-body clip: the aim pose comes back after it.
			stop_aiming();
			twitch = animArrayPickRandom( "twitch" );
			self setFlaggedAnimKnobAllRestart( "twitchanim", twitch, %body, 1, 0.2, 1 );
			DoNoteTracksForTime( getAnimLength( twitch ), "twitchanim" );
			start_aiming( 0.2 );
			nextTwitch = getTime() + 6000 + randomInt( 8000 );
		}
		wait( 0.25 );
	}
	stop_aiming();
}

// At ease: the looping idle and now and then one of its twitches.
casual_idle( wasMoving )
{
	self endon( "killanimscript" );

	crouched = ( self.a.pose == "crouch" );
	fromPatrol = isDefined( wasMoving ) && wasMoving == "walk";
	set = casual_idle_set( fromPatrol );

	// IW4 _patrol / utility setEnv: the way into the idle -- a patrol walk
	// stops with patrol_bored_walk_2_bored, an alert soldier lowers his
	// weapon with the casual idle's transition in.
	inClip = undefined;
	if ( crouched )
		inClip = %casual_crouch_idle_in;
	else if ( fromPatrol )
		inClip = %patrol_bored_walk_2_bored;
	else if ( !isDefined( wasMoving ) || wasMoving == "stop" )
		inClip = %casual_stand_idle_trans_in;
	if ( isDefined( inClip ) )
	{
		self setFlaggedAnimKnobAllRestart( "idlein", inClip, %body, 1, 0.3, 1 );
		DoNoteTracksForTime( getAnimLength( inClip ), "idlein" );
		stat_bump( "idle_in" );
	}
	self.coop_idle_state = "casual";
	if ( fromPatrol )
		self.coop_idle_state = "bored";
	if ( crouched )
		self.coop_idle_state = "crouch";

	if ( crouched )
		self setFlaggedAnimKnobAll( "idleanim", %casual_crouch_idle, %body, 1, 0.3, 1 );
	else
		self setFlaggedAnimKnobAll( "idleanim", set[ "idle" ], %body, 1, 0.3, 1 );

	nextTwitch = getTime() + 5000 + randomInt( 7000 );
	while ( !isDefined( self.enemy ) )
	{
		if ( react_to_grenade() )
			return;
		if ( !crouched && getTime() >= nextTwitch && set[ "twitch" ].size )
		{
			twitch = pick( set[ "twitch" ] );
			self setFlaggedAnimKnobAllRestart( "twitchanim", twitch, %body, 1, 0.3, 1 );
			DoNoteTracksForTime( getAnimLength( twitch ), "twitchanim" );
			if ( isDefined( self.enemy ) )
				return;
			self setFlaggedAnimKnobAll( "idleanim", set[ "idle" ], %body, 1, 0.3, 1 );
			nextTwitch = getTime() + 6000 + randomInt( 9000 );
		}
		wait( 0.25 );
	}
}
