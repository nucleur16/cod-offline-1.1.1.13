// zm hellhound animscript: animscripts/zombie_dog_jump.gsc
//
// Ported from this tree's multiplayer dog animscript
// maps/mp/animscripts/dog_jump.gsc (itself adapted from the shipped T5
// maps\mp\animscripts\dog_jump.gsc, header kept below).  The zm hellhound
// (species "zombie_dog", codol_actor_scripts.cpp) runs the same native T5 dog
// state machine bound against C1's animtrees/zombie_dog.atr, and every state
// is selected by name through setanimstate(), so the bodies are the MP
// dog's.  Changed for zm, the same way T5 forks animscripts\dog_* into
// animscripts\zombie_dog_*:
//   - module paths maps\mp\animscripts\dog_* -> ...\zombie_dog_*, so the zm
//     category compiles no multiplayer module (see _zombiemode.gsc);
//   - no maps\mp\_utility and no maps\mp\_dogs reference;
//   - nothing else changed.

/*
	maps/mp/animscripts/dog_jump.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\dog_jump.gsc

	Changed for CoDOL:
	  - #include maps\mp\animscripts\utility + ...\shared and the namespaced
	    maps\mp\animscripts\shared::DoNoteTracks call now point at the
	    dog-local animscripts\zombie_dog_shared module.
	  - main() takes the native traversal's arguments (begin, begin_angles,
	    end, animscript), as the zm species jump module does.  The native dog
	    wall hop (codol_actor_lifecycle.cpp ActorStartWallHop) passes the T5
	    traversal state the geometry calls for in <animscript>:
	    "traverse_wallhop" (T5's own and only state here), or T5's
	    traverse\shared dog_jump_up/dog_jump_down states
	    "traverse_jump_up_40" / "traverse_jump_down_40", or its
	    dog_wall_and_window_hop state "traverse_window" for a low opening.
	    Anything else plays "traverse_wallhop".
*/

#include animscripts\zombie_dog_shared;

main( begin, begin_angles, end, animscript )
{
	self endon("killanimscript");

	debug_anim_print("dog_jump::main()" );
	self SetAimAnimWeights( 0, 0 );

	state = "traverse_wallhop";
	if ( isdefined( animscript ) && ( animscript == "traverse_jump_up_40" || animscript == "traverse_jump_down_40" || animscript == "traverse_window" ) )
		state = animscript;

	self.safeToChangeScript = false;
	self setanimstate( state );
	animscripts\zombie_dog_shared::DoNoteTracks( "done" );
	self.safeToChangeScript = true;
}
