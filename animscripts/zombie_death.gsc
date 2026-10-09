// zm zombie animscript: zombie_death.
//
// Ported from T5 animscripts\zombie_death.gsc:25 main (1,774 lines), the
// self.deathanim arm (:72-:110) -- the arm every zm zombie takes, because
// _zombiemode_spawner::set_zombie_run_cycle assigns self.deathanim on every
// spawn exactly as T5 does.
//
// CONTRACT WITH THE NATIVE PATH.  MEASURED,
// src/codol/player/codol_actor_lifecycle.cpp:668-678: once health reaches 0
// CODOL_ActorRunFrame switches to <animscript>_death with safeToChangeScript
// forced, and turns the actor into a corpse only when THIS THREAD HAS ENDED.
// So the death animation's length is the corpse delay, and returning early
// would snap the body away mid-fall.  T5 has the same contract and the same
// comment (:44).
//
// TAKEN, by T5 line:
//   :27  stopsounds
//   :34  self.deathFunction hook
//   :43  the self.a.nodeath early-out, with T5's own 0.1 s wait and its
//        reason (the actor must think once before it can be deleted)
//   :76  SetFlaggedAnimKnobAll of self.deathanim
//   :79  the "no start_ragdoll notetrack" ragdoll fallback timer
//   :88  DoNoteTracks with self.handle_death_notetracks
//   :104 the self.deathanimscript hook -- this is the hook the previous
//        pass listed as deferred; it is live now.
//
// NOT TAKEN, and why:
//   :62 ClearAnim( %scripted_look_straight ) -- facial animation; the zombie
//     heads are separate xmodels with no face tree.
//   (do_gib is TAKEN since 2026-10-05: T5 :76 runs it when the death starts
//     with self.a.gib_ref set.  The gib itself -- models, pieces, blood --
//     lives in maps\_zombiemode_gib.gsc, whose damage watcher
//     also gibs the killing hit; do_gib is idempotent, so this call is the
//     backstop for a gib_ref set by any other script.)
//   the non-deathanim arms (:112-:1774): explosive deaths, helmetPop, the
//     long-death set, the bullet-shield set.  Every one selects from a T5
//     anim table whose entries have no shipped xanim in a zm zone.
//   death_anim_short_circuit (:86) -- it calls stopanimscripted.
//
// C1-vs-T5 CONFLICT, C1 WINS: T5 passes %root as the knob-all root anim.
// MEASURED, CoDOL's generic_human.atr declares no root node; its single
// top-level node is body, which is also the only legal knob-all root on this
// server (docs/CODOL_SCRIPTED_ANIMATION_20260920.md section 4.2 refuses a
// root anim that has a parent).  %body is used.


#include maps\_zombiemode_utility;
#include animscripts\zombie_shared;

#using_animtree ("generic_human");

main()
{
	self endon( "killanimscript" );

	self stopsounds();

	// T5 :34.
	if ( isDefined( self.deathFunction ) )
	{
		successful_death = self [[ self.deathFunction ]]();

		if ( !isDefined( successful_death ) || successful_death )
			return;
	}

	// T5 :43, comment and timing preserved.
	if ( is_true( self.a.nodeath ) )
	{
		wait( 0.1 );
		return;
	}

	if ( !isDefined( self.deathanim ) )
	{
		// set_zombie_run_cycle always assigns one; this is the one-frame-old
		// actor case.  Fall back to the stand death rather than vanishing.
		if ( is_true( self.has_legs ) )
			self.deathanim = %ai_zombie_death_v1;
		else
			self.deathanim = %ai_zombie_crawl_death_v1;
	}

	// T5 :76.
	if ( isDefined( self.a ) && isDefined( self.a.gib_ref ) )
		self thread maps\_zombiemode_gib::do_gib();

	if ( isDefined( self.gib_func ) )
		self thread [[ self.gib_func ]]();

	self setFlaggedAnimKnobAll( "deathanim", self.deathanim, %body, 1, 0.05, 1 );

	// T5 :79.  A clip with no ragdoll notetrack gets one on a timer at 35% of
	// its length, so the body always ends up on the floor.
	if ( !animHasNotetrack( self.deathanim, "start_ragdoll" ) )
		self thread waitForRagdoll( getAnimLength( self.deathanim ) * 0.35 );

	self DoNoteTracks( "deathanim", self.handle_death_notetracks );

	// T5 :104.  The hook the previous pass deferred: a level script or
	// _zombiemode_spawner may run its own tail here (the round accounting
	// already runs off the "death" notify, so this is presentation only).
	if ( isDefined( self.deathanimscript ) )
		self [[ self.deathanimscript ]]();
}

// T5 zombie_death.gsc waitForRagdoll.
waitForRagdoll( delay )
{
	self endon( "killanimscript" );

	wait( delay );

	if ( is_true( self.allow_ragdoll ) )
		self startRagdoll();
}
