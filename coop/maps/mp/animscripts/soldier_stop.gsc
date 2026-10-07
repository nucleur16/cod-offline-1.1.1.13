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

	for ( ;; )
	{
		idle_pose( 0.3 );
		DoNoteTracksForTime( 1, "idleanim" );
	}
}
