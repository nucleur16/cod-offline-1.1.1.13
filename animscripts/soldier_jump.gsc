// coop soldier animscript: soldier_jump.
//
// Run for a traversal (node_negotiation_begin -> _end pair) the route
// crosses.  None of the coop base levels authors negotiation nodes for this
// server's planner, so the module only has to end the traversal state
// cleanly: it holds the stand pose for one beat and returns, which is what
// releases the native negotiation hold.

#include animscripts\soldier_shared;

#using_animtree ( "generic_human" );

// The native traversal starts this with four arguments, as dog_jump; a
// script that declares none is a fatal "too many parameters" error.
main( begin, begin_angles, end, animscript )
{
	self endon( "killanimscript" );
	self animmode( "gravity" );
	stand_pose( 0.1 );
	wait( 0.1 );
	self animmode( "normal" );
}
