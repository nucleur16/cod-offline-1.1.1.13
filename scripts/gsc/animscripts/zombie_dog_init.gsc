// zm hellhound animscript: animscripts/zombie_dog_init.gsc
//
// Ported from this tree's multiplayer dog animscript
// maps/mp/animscripts/dog_init.gsc (itself adapted from the shipped T5
// maps\mp\animscripts\dog_init.gsc, header kept below).  The zm hellhound
// (species "zombie_dog", codol_actor_scripts.cpp) runs the same native T5 dog
// state machine bound against C1's animtrees/zombie_dog.atr, and every state
// is selected by name through setanimstate(), so the bodies are the MP
// dog's.  Changed for zm, the same way T5 forks animscripts\dog_* into
// animscripts\zombie_dog_*:
//   - module paths maps\mp\animscripts\dog_* -> ...\zombie_dog_*, so the zm
//     category compiles no multiplayer module (see _zombiemode.gsc);
//   - no maps\mp\_utility and no maps\mp\_dogs reference;
//   - #using_animtree("zombie_dog") is declared HERE: it is what registers
//     the server tree the native actor binds (CODOL_ActorPrepareAnimation);
//   - setTalkToSpecies( "zombie_dog" ); hellhound vocals for the barks.
//   - ground bites start close enough for the native 64-unit melee trace.

/*
	maps/mp/animscripts/dog_init.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\dog_init.gsc

	Changed for CoDOL:
	  - #include maps\mp\animscripts\shared + ...\utility replaced by the single
	    dog-local animscripts\zombie_dog_shared (neither T5 path exists here,
	    and the IW4-shaped animscripts\shared / animscripts\utility already
	    mounted in this tree are a different, human-AI module).
	  - nothing else changed.
*/

#include animscripts\zombie_dog_shared;

#using_animtree ("zombie_dog");

main()
{
	level.dog_debug_orient = 0;
	level.dog_debug_anims = 0;
	level.dog_debug_anims_ent = 0;
	level.dog_debug_turns = 0;

	{}

	firstInit();

	animscripts\zombie_dog_move::setup_sound_variables();

	anim_get_dvar_int("debug_dog_sound","0");
	anim_get_dvar_int("debug_dog_notetracks","0");

	self.ignoreSuppression = true;

	self.chatInitialized = false;
	self.noDodgeMove = true;

	// The old 102-unit range relied on the running attack's forward motion.
	// Native admission adds 15 (30 for moving targets); script combat adds
	// 30. A 32-unit approach keeps both within the native 64-unit bite range.
	level.dogAttackPlayerDist = 32;
	level.dogAttackPlayerCloseRangeDist = 32;

	level.dogRunTurnSpeed = 20; // if the speed is greater then play the run turns
	level.dogRunPainSpeed = 20; // if the speed is greater then play the run pains

	self.meleeAttackDist = 0;
	self thread setMeleeAttackDist();

	self.a = spawnStruct();
	self.a.pose = "stand";					// to use DoNoteTracks()
	self.a.nextStandingHitDying = false;	// to allow dogs to use bullet shield
	self.a.movement = "run";

	set_anim_playback_rate();

	self.suppressionThreshold = 1;
	self.disableArrivals = false;

	// need to do the getmovedelta.  took this value from sp
	level.dogStoppingDistSq = 3416.82;
	self.stopAnimDistSq = level.dogStoppingDistSq;

	self.pathEnemyFightDist = 512;

	self setTalkToSpecies( "zombie_dog" );

	level.lastDogMeleePlayerTime = 0;
	level.dogMeleePlayerCounter = 0;

	if ( !isdefined( level.dog_hits_before_kill ) )
		level.dog_hits_before_kill = 1;
}

firstInit()
{
	level.lastPlayerSighted = 100;
}

set_anim_playback_rate()
{
	self.animplaybackrate = 0.9 + randomfloat( 0.2 );
	self.moveplaybackrate = 1;
}

setMeleeAttackDist()
{
	self endon( "death" );

	while ( 1 )
	{
		if ( isdefined( self.enemy ) )
		{

			if ( isplayer(self.enemy) )
			{
				stance = self.enemy getStance();

				if ( stance == "prone" )
				{
					self.meleeAttackDist = level.dogAttackPlayerCloseRangeDist;
				}
				else
				{
					self.meleeAttackDist = level.dogAttackPlayerDist;
				}
			}
			else
			{
				self.meleeAttackDist = level.dogAttackPlayerDist;
			}
		}

		wait(1);
	}
}
