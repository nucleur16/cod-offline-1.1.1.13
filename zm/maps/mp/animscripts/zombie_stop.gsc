// zm zombie animscript: zombie_stop.  The idle pose.
//
// Ported from T5 animscripts\zombie_stop.gsc:10 main (163 lines).
//
// The native path selects <animscript>_stop when the actor has no route and
// is not attacking (src/codol/player/codol_actor_lifecycle.cpp:866), which is
// the same condition T5's Stop covers.
//
// TAKEN: the loop shape, the stand/prone split, and the idle knob-all.
//
// NOT TAKEN, and why:
//   getDesiredIdlePose / GetClaimedNode / choosePose (:66) -- T5 picks the
//     idle pose from the cover node the AI has claimed.  This server has no
//     claim system and zm zombies never take cover; the pose is the actor's
//     own self.a.pose, which the notetracks maintain.
//   self ClearAnim( %root, 0.3 ) on a pose change -- C1-vs-T5, C1 WINS:
//     CoDOL's generic_human.atr has no node named root.  %body is used.
//   randomizeIdleSet / playIdle / self.idle_override (:47) -- T5's weighted
//     idle table lives in anim.idleAnimArray, which is populated by
//     _zombiemode.gsc::init_animscripts from %ai_zombie_idle_v1_delta.
//     C1-vs-T5, C1 WINS: MEASURED, that name is declared in CoDOL's tree but
//     ships NO xanim in any zone a zm match mounts, so the delta idle does
//     not exist here.  %ai_zombie_idle_v1 (which does ship) is played
//     directly, which is what a one-entry idle table would have done.
//   self.exception["stop_immediate"] / delayedException -- zm has no
//     exception table.
//   setLastStoppedTime / trackScriptState -- telemetry.

#using_animtree ("generic_human");

#include maps\mp\gametypes\_zombiemode_utility;
#include maps\mp\animscripts\zombie_shared;

main()
{
	self notify( "stopScript" );
	self endon( "killanimscript" );

	self orientmode( "face default" );
	self animmode( "normal" );

	for ( ;; )
	{
		if ( is_true( self.has_legs ) )
			idle = %ai_zombie_idle_v1;
		else
			idle = %ai_zombie_idle_crawl;

		self setFlaggedAnimKnobAll( "idleanim", idle, %body, 1, 0.3, self.animplaybackrate );

		DoNoteTracksForTime( 1, "idleanim" );
	}
}
