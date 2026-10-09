// coop soldier animscript: soldier_turn.
//
// The native frame only selects a species' turn module for the dog
// (ActorIsDog in CODOL_ActorRunFrame); the loader still requires all nine
// modules, so a human soldier's turn is the move cycle's own facing: hold
// orientation to motion until the frame selects something else.

main()
{
	self endon( "killanimscript" );
	self orientmode( "face motion" );
	for ( ;; )
		wait( 0.05 );
}
