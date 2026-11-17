// zm hellhound animscript: animscripts/zombie_dog_turn.gsc
//
// Ported from this tree's multiplayer dog animscript
// maps/mp/animscripts/dog_turn.gsc (itself adapted from the shipped T5
// maps\mp\animscripts\dog_turn.gsc, header kept below).  The zm hellhound
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
	maps/mp/animscripts/dog_turn.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\dog_turn.gsc

	Changed for CoDOL:
	  - #include maps\mp\animscripts\utility + ...\shared and the namespaced
	    maps\mp\animscripts\shared::DoNoteTracksForTime calls now point at the
	    dog-local animscripts\zombie_dog_shared module.
	  - GetDvarFloat( #"dog_turn90_angle" ) / #"dog_turn180_angle" rewritten to
	    the plain string form: the T5 #"..." canonical-hash token does not
	    exist in this VM.  maps\mp\_dogs::dog_dvar_update registers both dvars
	    with their T5 code defaults so GetDvarFloat does not read 0 here.
	  - nothing else changed.
*/

#include animscripts\zombie_dog_shared;

main()
{
	self endon("killanimscript");

	{}
	self SetAimAnimWeights( 0, 0 );

	self.safeToChangeScript = false;

	deltaYaw = self GetDeltaTurnYaw();

	if ( need_to_turn_around( deltaYaw ) )
	{
		turn_180( deltaYaw );
	}
	else
	{
		turn_90( deltaYaw );
	}

	move_out_of_turn();

	self.skipStartMove = true;
	self.safeToChangeScript = true;
}

need_to_turn_around( deltaYaw )
{
	angle = GetDvarFloat( "dog_turn180_angle" );

	if ( ( deltaYaw > angle ) || ( deltaYaw < ( -1 * angle ) ) )
	{
		{}
		return true;
	}

	{}
	return false;
}

do_turn_anim( stopped_anim, run_anim, wait_time, run_wait_time )
{
	speed = length( self getvelocity() );

	do_anim = stopped_anim;

	if ( level.dogRunTurnSpeed < speed )
	{
		do_anim = run_anim;
		wait_time = run_wait_time;
	}

	{}
	self setanimstate( do_anim );
	// T5 animscripts/zombie_dog_turn.gsc:89 plays the turn clip to its end
	// (DoNoteTracks "turn"); the multiplayer dog cut it at wait_time.
	// MEASURED (t5cap dog clip timeline, 2026-10-22): every T5 hound turn
	// clip ran to 1.00; CoDOL's stopped at 0.86 of turn_90_left.
	animscripts\zombie_dog_shared::DoNoteTracks( "done" );
	debug_anim_print("dog_move::turn_around_right() - done with " + do_anim + " wait time " + run_wait_time );
}

turn_left()
{
	self do_turn_anim( "move_turn_left", "move_run_turn_left", 0.5, 0.5 );
}

turn_right()
{
	self do_turn_anim( "move_turn_right", "move_run_turn_right", 0.5, 0.5 );
}

turn_180_left()
{
	self do_turn_anim( "move_turn_around_left", "move_run_turn_around_left", 0.5, 0.7 );
}

turn_180_right()
{
	self do_turn_anim( "move_turn_around_right", "move_run_turn_around_right", 0.5, 0.7 );
}

move_out_of_turn()
{
	if ( self.a.movement == "run" )
	{
		{}
		self setanimstate( "move_run" );
		animscripts\zombie_dog_shared::DoNoteTracksForTime(0.1, "done");
		{}
	}
	else
	{
		{}
		self setanimstate( "move_walk" );
	}
}

turn_90( deltaYaw )
{
	self animMode( "zonly_physics" );
	{}

	if ( deltaYaw > GetDvarFloat( "dog_turn90_angle" ) )
	{
		{}
		self turn_left();
	}
	else
	{
		{}
		self turn_right();
	}
}

turn_180( deltaYaw )
{
	self animMode( "zonly_physics" );
	{}

	// pick either
	if ( deltaYaw > 177 || deltaYaw < -177 )
	{
		if ( randomint(2) == 0 )
		{
			{}
			self turn_180_right();
		}
		else
		{
			{}
			self turn_180_left();
		}
	}
	else if ( deltaYaw > GetDvarFloat( "dog_turn180_angle" ) )
	{
		{}
		self turn_180_left();
	}
	else
	{
		{}
		self turn_180_right();
	}
}
