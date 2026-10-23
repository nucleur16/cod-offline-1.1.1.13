/*
	scripts/codol/maps/mp/animscripts/dog_pain.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\dog_pain.gsc

	Changed for CoDOL:
	  - #include maps\mp\animscripts\utility and the namespaced
	    maps\mp\animscripts\shared::DoNoteTracksForTime call now point at the
	    dog-local maps\mp\animscripts\dog_shared module.
	  - nothing else changed.
*/

//#using_animtree ("dog");
#include maps\mp\animscripts\dog_shared;

main()
{
	debug_anim_print("dog_pain::main() " );

	self endon("killanimscript");
	self SetAimAnimWeights( 0, 0 );

	if ( isdefined( self.enemy ) && isdefined( self.enemy.syncedMeleeTarget) && self.enemy.syncedMeleeTarget == self )
	{
		self unlink();
		self.enemy.syncedMeleeTarget = undefined;
	}

	speed = length( self getvelocity() );

	pain_anim = getAnimDirection( self t5DamageYaw() );

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

	self maps\mp\animscripts\dog_shared::DoNoteTracksForTime(0.2, "done");
}
