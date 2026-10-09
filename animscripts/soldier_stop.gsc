// coop soldier animscript: soldier_stop.  Standing still, no route.
//
// IW4 animscripts\stop.gsc reduced to the exposed stand: alert aim pose while
// the soldier has an enemy, the casual idle while he has none.

#include maps\mp\animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self notify( "stopScript" );
	self endon( "killanimscript" );

	self orientmode( "face default" );
	self animmode( "normal" );
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

	for ( ;; )
	{
		idle_pose( 0.3 );
		DoNoteTracksForTime( 1, "idleanim" );
	}
}
