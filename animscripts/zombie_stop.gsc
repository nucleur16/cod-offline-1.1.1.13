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
//     idle table uses %ai_zombie_idle_v1_delta, which has no mounted xanim.
//     CoDOL ships %zombie_stationary_idle_v7 as a complete standing pose.
//     %ai_zombie_idle_v1 is only an additive layer and alone gives a bind
//     pose, so the stop state uses the complete clip.
//   self.exception["stop_immediate"] / delayedException -- zm has no
//     exception table.
//   setLastStoppedTime / trackScriptState -- telemetry.


#include maps\_zombiemode_utility;
#include animscripts\zombie_shared;

#using_animtree ("generic_human");

main()
{
	self notify( "stopScript" );
	self endon( "killanimscript" );

	self orientmode( "face default" );
	self animmode( "normal" );

	for ( ;; )
	{
		if ( is_true( self.has_legs ) )
			idle = %zombie_stationary_idle_v7;
		else
			idle = %ai_zombie_idle_crawl;
		// A species variant's own idle (the juggernaut, _zombiemode_ai_boss).
		if ( isDefined( self.idle_anim_override ) )
			idle = self.idle_anim_override;

		self setFlaggedAnimKnobAll( "idleanim", idle, %body, 1, 0.3, self.animplaybackrate );
		if ( isDefined( self.idle_additive_override ) )
		{
			self setAnim( %add_idle, 1, 0.3, 1 );
			self setAnimKnob( self.idle_additive_override, 1, 0.3, self.animplaybackrate );
		}

		DoNoteTracksForTime( 1, "idleanim" );
	}
}
