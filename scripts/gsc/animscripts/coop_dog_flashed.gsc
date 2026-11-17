// coop dog animscript: animscripts/coop_dog_flashed.gsc
//
// The Spec Ops dog (survival's dog_reg / dog_splode, IW5 maps\_so_survival_ai
// spawn_dogs; species "coop_dog", codol_actor_scripts.cpp).  Generated from
// this tree's zm hellhound set animscripts/zombie_dog_flashed.gsc (category-clean: it names
// no multiplayer module), bound against C1's animtrees/dog.atr (common_mp) and
// common_dog.animlist (common_gametype_coop) instead of zombie_dog:
//   - module paths animscripts\zombie_dog_* -> animscripts\coop_dog_*;
//   - #using_animtree("dog"), setTalkToSpecies("dog");
//   - the hellhound vocals -> the German shepherd's anml_dog_* aliases
//     (common_gametype_coop / common_mp).
// The original file's notes follow.

// zm hellhound animscript: animscripts/zombie_dog_flashed.gsc
//
// Ported from this tree's multiplayer dog animscript
// maps/mp/animscripts/dog_flashed.gsc (itself adapted from the shipped T5
// maps\mp\animscripts\dog_flashed.gsc, header kept below).  The zm hellhound
// (species "zombie_dog", codol_actor_scripts.cpp) runs the same native T5 dog
// state machine bound against C1's animtrees/zombie_dog.atr, and every state
// is selected by name through setanimstate(), so the bodies are the MP
// dog's.  Changed for zm, the same way T5 forks animscripts\dog_* into
// animscripts\coop_dog_*:
//   - module paths maps\mp\animscripts\dog_* -> ...\zombie_dog_*, so the zm
//     category compiles no multiplayer module (see _zombiemode.gsc);
//   - no maps\mp\_utility and no maps\mp\_dogs reference;
//   - nothing else changed.

/*
	maps/mp/animscripts/dog_flashed.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\dog_flashed.gsc

	Changed for CoDOL:
	  - the namespaced maps\mp\animscripts\shared::DoNoteTracks call now points
	    at the dog-local animscripts\coop_dog_shared module.
	  - nothing else changed.  setFlashBanged / getFlashBangedStrength are kept
	    verbatim; they are engine methods this server does not register yet
	    (reported as required builtins).
*/

main()
{
	self endon("killanimscript");
	self endon( "stop_flashbang_effect" );

	wait randomfloatrange( 0, 0.4 );

	duration = self startFlashBanged() * 0.001;

	self setanimstate( "flashed" );
	self animscripts\coop_dog_shared::DoNoteTracks("done");

	self setFlashBanged( false );

	self.flashed = false;
	self notify( "stop_flashbang_effect" );
}

startFlashBanged()
{
	if ( isdefined( self.flashduration ) )
		duration = self.flashduration;
	else
		duration = self getFlashBangedStrength() * 1000;

	self.flashendtime = gettime() + duration;
	self notify("flashed");

	return duration;
}
