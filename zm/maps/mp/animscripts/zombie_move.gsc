// zm zombie animscript: zombie_move.  The walk/run/sprint cycle.
//
// Ported from T5 animscripts\zombie_move.gsc (575 lines) MoveMainLoop :60 and
// animscripts\zombie_run.gsc (508 lines) MoveRun :8 /
// MoveStandNoncombatOverride :228 / MoveCrouchRunOverride :283.
//
// The native path drives this module: CODOL_ActorRunFrame
// (src/codol/player/codol_actor_lifecycle.cpp:866) selects <animscript>_move
// whenever the actor has a route and is not attacking or in pain, and the
// native mover owns TRANSLATION.  This module owns only the pose on top of
// that translation, which is precisely the split T5 has.
//
// TAKEN, by T5 function:
//   MoveMainLoop            zombie_move.gsc:60  -- the re-pick loop, its
//                           getAnimTime(%walk_and_run_loops) loop counter and
//                           the "closer than 64 units, walk instead" rule.
//   MoveRun                 zombie_run.gsc:8    -- the pose switch.
//   MoveStandNoncombatOverride zombie_run.gsc:228 -- the knob-all itself,
//                           unchanged: SetFlaggedAnimKnobAll onto %body at
//                           weight 1 / 0.2 s / self.moveplaybackrate, then
//                           DoNoteTracksForTime.
//
// NOT TAKEN, and why:
//   sideStepInit / trySideStep (zombie_move.gsc:117) -- T5's Spetsnaz-zombie
//     sidestep.  It calls AnimScripted to translate the actor sideways along
//     the clip's root motion; animscripted is not a registered builtin, and
//     faking the translation with a teleport would be invented movement.
//   animscripts\face::SetIdleFaceDelayed, battleChatter_ai::evaluateMoveEvent,
//     animscripts\debug -- no counterpart module in this tree.
//   self.exception["move"] -- T5's per-level exception table; zm has none.
//
// CoDOL-vs-T5: %walk_and_run_loops.  MEASURED, the extracted CoDOL
// generic_human.atr carries the node, so getAnimTime on it is legal.  It is
// only used as a loop-completion counter and nothing here reads the count, so
// the call is kept for parity but its result is not acted on.


#include maps\mp\gametypes\_zombiemode_utility;
#include maps\mp\animscripts\zombie_shared;

#using_animtree ("generic_human");

main()
{
	self endon( "killanimscript" );

	self orientmode( "face motion" );
	self animmode( "normal" );

	MoveMainLoop();
}

MoveMainLoop()
{
	self endon( "killanimscript" );

	self.a.runLoopCount = randomInt( 10000 );
	prevLoopTime = 0;

	// T5 :66.  A destination inside 64 units is walked to, whatever the
	// round's speed says.
	if ( isDefined( self.zombie_move_speed ) && self.zombie_move_speed != "walk" )
	{
		if ( isDefined( self.pathGoalPos ) && distanceSquared( self.origin, self.pathGoalPos ) < 4096 )
			self.a.movement = "walk";
	}

	for ( ;; )
	{
		loopTime = self getAnimTime( %walk_and_run_loops );
		if ( loopTime < prevLoopTime )
			self.a.runLoopCount++;
		prevLoopTime = loopTime;

		MoveRun();
	}
}

// T5 zombie_run.gsc:8.  Two poses reach this server: a zombie with legs runs
// its stand cycle, a crawler runs its prone cycle.  T5's "crouch" arm has no
// zombie caller (choosePose only ever answers "stand" for animname "zombie").
MoveRun()
{
	self endon( "killanimscript" );

	if ( is_true( self.has_legs ) )
		MoveStandRun();
	else
		MoveCrawl();
}

// T5 zombie_run.gsc:228 MoveStandNoncombatOverride, verbatim apart from the
// debug push/pop.
MoveStandRun()
{
	runAnim = current_run_anim();
	if ( !isDefined( runAnim ) )
	{
		// set_zombie_run_cycle has not run yet (the actor is one frame old).
		// Hold the init pose rather than blending to nothing.
		wait( 0.05 );
		return;
	}

	self setFlaggedAnimKnobAll( "runanim", runAnim, %body, 1, 0.2, self.moveplaybackrate );
	DoNoteTracksForTime( level.zombie_run_anim_update_time, "runanim" );
}

// T5 zombie_run.gsc:283 MoveCrouchRunOverride, retargeted at the crawl set --
// T5 reaches the same clips through maps\_zombiemode_spawner::make_crawler,
// which swaps self.run_combatanim for a level._zombie_crawl entry and then
// runs the same knob-all.
MoveCrawl()
{
	crawlAnim = self.crawl_anim;
	if ( !isDefined( crawlAnim ) )
	{
		wait( 0.05 );
		return;
	}

	self setFlaggedAnimKnobAll( "runanim", crawlAnim, %body, 1, 0.2, self.moveplaybackrate );
	DoNoteTracksForTime( level.zombie_run_anim_update_time, "runanim" );
}

// The cycle _zombiemode_spawner::set_zombie_run_cycle chose for this actor.
// T5 keeps two (run_combatanim for a zombie that can see its enemy and
// run_noncombatanim otherwise); the zombie set assigns the same clip to both,
// so one accessor covers it and the second field is honoured if a level
// script sets it.
current_run_anim()
{
	if ( isDefined( self.enemy ) && isDefined( self.run_combatanim ) )
		return self.run_combatanim;

	if ( isDefined( self.run_noncombatanim ) )
		return self.run_noncombatanim;

	return self.run_combatanim;
}
