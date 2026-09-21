// zm zombie animscript: zombie_flashed.
//
// Ported from T5 animscripts\flashed.gsc (T5 has no zombie_flashed; the
// zombie species falls through to the shared one) plus the zombie's own
// flash reaction clips.
//
// The native path reaches this module through setFlashBanged
// (src/codol/player/codol_actor_lifecycle.cpp:1323).  Like pain, the frame
// keeps it selected only while this thread lives, so it returns.
//
// C1-vs-T5: CoDOL ships %ai_zombie_flash_react_01 and _02 -- MEASURED,
// common_gametype_zm, both declared in generic_human.atr.  T5's shared
// flashed.gsc plays a human cower set instead; the zombie-specific clips are
// obviously the intended ones and CoDOL shipping them is the evidence.
//
// NOT TAKEN: T5's flashed.gsc blindfire, the flash_end transition and
// animscripts\shared::DoNoteTracksForever -- the first two need weapon
// firing the zombie does not have.


#include maps\mp\gametypes\_zombiemode_utility;
#include maps\mp\animscripts\zombie_shared;

#using_animtree ("generic_human");

main()
{
	self endon( "killanimscript" );
	self endon( "death" );

	if ( !is_true( self.has_legs ) )
		return;

	if ( randomInt( 2 ) == 0 )
		react = %ai_zombie_flash_react_01;
	else
		react = %ai_zombie_flash_react_02;

	self animmode( "gravity" );
	self setFlaggedAnimKnobAllRestart( "flashanim", react, %body, 1, 0.2, self.animplaybackrate );

	DoNoteTracksForTime( getAnimLength( react ), "flashanim" );

	self animmode( "normal" );
}
