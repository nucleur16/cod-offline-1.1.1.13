/*
	scripts/codol/maps/mp/animscripts/dog_init.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\dog_init.gsc

	Changed for CoDOL:
	  - #include maps\mp\animscripts\shared + ...\utility replaced by the single
	    dog-local maps\mp\animscripts\dog_shared (neither T5 path exists here,
	    and the IW4-shaped animscripts\shared / animscripts\utility already
	    mounted in this tree are a different, human-AI module).
	  - nothing else changed.
*/

//#using_animtree ("dog");
#include maps\mp\animscripts\dog_shared;

main()
{
	level.dog_debug_orient = 0;
	level.dog_debug_anims = 0;
	level.dog_debug_anims_ent = 0;
	level.dog_debug_turns = 0;

	{}

	firstInit();

	maps\mp\animscripts\dog_move::setup_sound_variables();

	anim_get_dvar_int("debug_dog_sound","0");
	anim_get_dvar_int("debug_dog_notetracks","0");

	self.ignoreSuppression = true;

	self.chatInitialized = false;
	self.noDodgeMove = true;

	level.dogAttackPlayerDist = 102; // hard code for now, above is not accurate.
	level.dogAttackPlayerCloseRangeDist = 102;

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

	self setTalkToSpecies( "dog" );

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
