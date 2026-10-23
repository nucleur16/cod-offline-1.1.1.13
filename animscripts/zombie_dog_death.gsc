// zm hellhound animscript: animscripts/zombie_dog_death.gsc
//
// Ported from this tree's multiplayer dog animscript
// maps/mp/animscripts/dog_death.gsc (itself adapted from the shipped T5
// maps\mp\animscripts\dog_death.gsc, header kept below).  The zm hellhound
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
	maps/mp/animscripts/dog_death.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\dog_death.gsc

	Changed for CoDOL:
	  - #include maps\mp\animscripts\utility and the namespaced
	    maps\mp\animscripts\shared::DoNoteTracks call now point at the
	    dog-local animscripts\zombie_dog_shared module.
	  - nothing else changed.
*/

//#using_animtree ("dog");
#include animscripts\zombie_dog_shared;

main()
{
	{}
	self SetAimAnimWeights( 0, 0 );

	self endon("killanimscript");
	if ( isdefined( self.a.nodeath ) )
	{
		assertex( self.a.nodeath, "Nodeath needs to be set to true or undefined." );

		// allow death script to run for a bit so it doesn't turn to corpse and get deleted too soon during melee sequence
		wait 3;
		return;
	}

	self unlink();

	if ( isdefined( self.enemy ) && isdefined( self.enemy.syncedMeleeTarget ) && self.enemy.syncedMeleeTarget == self )
	{
		self.enemy.syncedMeleeTarget = undefined;
	}

	death_anim = "death_" + getAnimDirection( self t5DamageYaw() );

	// T5 animscripts/zombie_dog_death.gsc:383-395: a hound the wunderwaffe
	// (or the microwave gun) killed plays one of its five tesla deaths
	// (native states death_tesla_a..e, codol_dog_states.cpp).
	if ( ( isdefined( self.tesla_death ) && self.tesla_death ) ||
		( isdefined( self.microwavegun_death ) && self.microwavegun_death ) ||
		( isdefined( self.microwavegun_dw_death ) && self.microwavegun_dw_death ) )
	{
		tesla_deaths = [];
		tesla_deaths[ tesla_deaths.size ] = "death_tesla_a";
		tesla_deaths[ tesla_deaths.size ] = "death_tesla_b";
		tesla_deaths[ tesla_deaths.size ] = "death_tesla_c";
		tesla_deaths[ tesla_deaths.size ] = "death_tesla_d";
		tesla_deaths[ tesla_deaths.size ] = "death_tesla_e";
		death_anim = tesla_deaths[ randomint( tesla_deaths.size ) ];
	}
/#
	{}
#/

	self animMode( "gravity" );
	{}
	self setanimstate( death_anim );
	self animscripts\zombie_dog_shared::DoNoteTracks( "done" );
}
