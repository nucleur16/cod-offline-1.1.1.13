// coop soldier animscript: soldier_flashed.
//
// IW4 animscripts\flashed.gsc: a flashbanged soldier staggers through one of
// the exposed_flashbang reactions, then the native frame resumes.

#include maps\mp\animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self endon( "killanimscript" );
	self endon( "death" );

	if ( randomInt( 2 ) )
		react = %exposed_flashbang_v1;
	else
		react = %exposed_flashbang_v2;

	self animmode( "gravity" );
	self setFlaggedAnimKnobAllRestart( "flashanim", react, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( react ), "flashanim" );
	self setFlashBanged( false );
	self animmode( "normal" );
}
