// coop soldier animscript: soldier_flashed.
//
// IW4 animscripts\flashed.gsc: a flashbanged soldier staggers through one of
// IW4's five exposed_flashbang reactions (anim.flashAnimArray) and ends on his
// feet, then the native frame resumes.

#include animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self endon( "killanimscript" );
	self endon( "death" );

	stop_aiming();
	reacts = [];
	reacts[ 0 ] = %exposed_flashbang_v1;
	reacts[ 1 ] = %exposed_flashbang_v2;
	reacts[ 2 ] = %exposed_flashbang_v3;
	reacts[ 3 ] = %exposed_flashbang_v4;
	reacts[ 4 ] = %exposed_flashbang_v5;
	react = pick( reacts );

	self animmode( "gravity" );
	self setFlaggedAnimKnobAllRestart( "flashanim", react, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( react ), "flashanim" );
	self.a.pose = "stand";
	self setFlashBanged( false );
	self animmode( "normal", false );
}
