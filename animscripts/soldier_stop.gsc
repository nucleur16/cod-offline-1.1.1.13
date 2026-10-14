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

	for ( ;; )
	{
		if ( isDefined( self.enemy ) )
			alert_idle();
		else
			casual_idle( wasMoving );
		wait( 0.05 );
	}
}

// Weapon up, aiming at the enemy, until he has no enemy.
alert_idle()
{
	self endon( "killanimscript" );

	self.a.pose = set_anim_array( self.a.pose );
	start_aiming( 0.3 );
	nextTwitch = getTime() + 4000 + randomInt( 6000 );
	while ( isDefined( self.enemy ) )
	{
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
	set = casual_idle_set( isDefined( wasMoving ) && wasMoving == "walk" );

	if ( crouched )
		self setFlaggedAnimKnobAll( "idleanim", %casual_crouch_idle, %body, 1, 0.3, 1 );
	else
		self setFlaggedAnimKnobAll( "idleanim", set[ "idle" ], %body, 1, 0.3, 1 );

	nextTwitch = getTime() + 5000 + randomInt( 7000 );
	while ( !isDefined( self.enemy ) )
	{
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
