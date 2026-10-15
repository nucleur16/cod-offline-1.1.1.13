// zm hellhound animscript: animscripts/zombie_dog_pain.gsc
//
// Ported from this tree's multiplayer dog animscript
// maps/mp/animscripts/dog_pain.gsc (itself adapted from the shipped T5
// maps\mp\animscripts\dog_pain.gsc, header kept below).  The zm hellhound
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
	maps/mp/animscripts/dog_pain.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\dog_pain.gsc

	Changed for CoDOL:
	  - #include maps\mp\animscripts\utility and the namespaced
	    maps\mp\animscripts\shared::DoNoteTracksForTime call now point at the
	    dog-local animscripts\zombie_dog_shared module.
	  - nothing else changed.
*/

//#using_animtree ("dog");
#include animscripts\zombie_dog_shared;

main()
{
	debug_anim_print("dog_pain::main() " );

	self endon("killanimscript");
	self SetAimAnimWeights( 0, 0 );

	// T5 animscripts/zombie_dog_pain.gsc:7: a hellhound never flinches
	// (_zombiemode_ai_dogs dog_init sets allowpain false and a.disablePain).
	if ( ( isdefined( self.a ) && isdefined( self.a.disablePain ) && self.a.disablePain ) || ( isdefined( self.allowpain ) && !self.allowpain ) )
		return;

	if ( isdefined( self.enemy ) && isdefined( self.enemy.syncedMeleeTarget) && self.enemy.syncedMeleeTarget == self )
	{
		self unlink();
		self.enemy.syncedMeleeTarget = undefined;
	}

	speed = length( self getvelocity() );

	pain_anim = getAnimDirection( self.damageyaw );

	// should only do this while running because the anim is supposed to while running
	if ( speed > level.dogRunPainSpeed )
	{
			pain_anim = "pain_run_" + pain_anim;
	}
	else
	{
			pain_anim = "pain_" + pain_anim;
	}

	self setanimstate( pain_anim );

	self animscripts\zombie_dog_shared::DoNoteTracksForTime(0.2, "done");
}
