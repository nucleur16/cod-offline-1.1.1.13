// coop dog animscript: animscripts/coop_dog_stop.gsc
//
// The Spec Ops dog (survival's dog_reg / dog_splode, IW5 maps\_so_survival_ai
// spawn_dogs; species "coop_dog", codol_actor_scripts.cpp).  Generated from
// this tree's zm hellhound set animscripts/zombie_dog_stop.gsc (category-clean: it names
// no multiplayer module), bound against C1's animtrees/dog.atr (common_mp) and
// common_dog.animlist (common_gametype_coop) instead of zombie_dog:
//   - module paths animscripts\zombie_dog_* -> animscripts\coop_dog_*;
//   - #using_animtree("dog"), setTalkToSpecies("dog");
//   - the hellhound vocals -> the German shepherd's anml_dog_* aliases
//     (common_gametype_coop / common_mp).
// The original file's notes follow.

// zm hellhound animscript: animscripts/zombie_dog_stop.gsc
//
// Ported from this tree's multiplayer dog animscript
// maps/mp/animscripts/dog_stop.gsc (itself adapted from the shipped T5
// maps\mp\animscripts\dog_stop.gsc, header kept below).  The zm hellhound
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
	maps/mp/animscripts/dog_stop.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\dog_stop.gsc

	Changed for CoDOL:
	  - #include maps\mp\animscripts\shared + ...\utility and every namespaced
	    maps\mp\animscripts\shared::... call now point at the dog-local
	    animscripts\coop_dog_shared module.
	  - nothing else changed.  cansee() is kept verbatim; it is an engine
	    method this server does not register yet (reported as a required
	    builtin).
*/

//#using_animtree ("dog");
#include animscripts\coop_dog_shared;

main()
{
	debug_anim_print("dog_stop::main()" );

	self endon("killanimscript");
	self SetAimAnimWeights( 0, 0 );

	self thread lookAtTarget( "attackIdle" );

	while (1)
	{
		if ( shouldAttackIdle() )
		{
			// a bite (T5 :84-93) has run its own clips to their end
			if ( !self randomAttackIdle() )
				animscripts\coop_dog_shared::DoNoteTracks("done");
		}
		else
		{
			self set_orient_mode( "face current" );
			debug_anim_print("dog_stop::main() - Setting stop_idle"  );

			self notify( "stop tracking" );
			self SetAimAnimWeights( 0, 0 );
			self setanimstate( "stop_idle" );
			animscripts\coop_dog_shared::DoNoteTracks("done");
			self thread lookAtTarget( "attackIdle" );
		}

			debug_anim_print("dog_stop::main() - stop idle loop notify done." );
	}
}

isFacingEnemy( toleranceCosAngle )
{
	assert( isdefined( self.enemy ) );

	vecToEnemy = self.enemy.origin - self.origin;
	distToEnemy = length( vecToEnemy );

	if ( distToEnemy < 1 )
		return true;

	forward = anglesToForward( self.angles );

	val1 = ( forward[0] * vecToEnemy[0] ) + ( forward[1] * vecToEnemy[1] );
	val2 =(( forward[0] * vecToEnemy[0] ) + ( forward[1] * vecToEnemy[1] ) ) / distToEnemy;

	return ( ( forward[0] * vecToEnemy[0] ) + ( forward[1] * vecToEnemy[1] ) ) / distToEnemy > toleranceCosAngle;
}

randomAttackIdle()
{
	if ( isFacingEnemy( -0.5 ) )	// cos120
		self set_orient_mode( "face current" );
	else
		self set_orient_mode("face enemy");

	// T5 animscripts/zombie_dog_stop.gsc:84-93: a hound standing within
	// ai_meleeRange of its player (100 in a hound round, dog_round_start)
	// bites from here instead of idling.  MEASURED (t5cap dog clip timeline,
	// zombie_cod5_sumpf, 2026-10-22): T5 hounds queued 64-71 units from a
	// frozen player bite (69 % run_attack, 30 % attack idles); CoDOL's idled.
	if ( isdefined( self.enemy ) && isplayer( self.enemy ) && isalive( self.enemy ) )
	{
		range = getDvarFloat( "ai_meleeRange" );
		if ( distanceSquared( self.origin, self.enemy.origin ) < range * range )
		{
			self notify( "dog_combat" );
			self animscripts\coop_dog_combat::meleeBiteAttackPlayer( self.enemy );
			return true;
		}
	}

	if ( should_growl() )
	{
		// just growl
		debug_anim_print("dog_stop::main() - Setting stop_attackidle_growl" );
		self setanimstate( "stop_attackidle_growl" );
		return false;
	}

	idleChance = 33;
	barkChance = 66;

	if ( isdefined( self.mode ) )
	{
		if ( self.mode == "growl" )
		{
			idleChance = 15;
			barkChance = 30;
		}
		else if ( self.mode == "bark" )
		{
			idleChance = 15;
			barkChance = 85;
		}
	}

	// T5 zombie_dog_stop.gsc:110-113: the "idle" share plays the growl too
	// (the multiplayer dog's plain attackidle is never picked here).
	rand = randomInt( 100 );
	if ( rand < idleChance )
	{
			debug_anim_print("dog_stop::main() - Setting stop_attackidle_growl" );
			self setanimstate( "stop_attackidle_growl" );
	}
	else if ( rand < barkChance )
	{
			debug_anim_print("dog_stop::main() - Setting stop_attackidle_bark " );
			self setanimstate( "stop_attackidle_bark" );
	}
	else
	{
			debug_anim_print("dog_stop::main() - Setting stop_attackidle_growl " );
			self setanimstate( "stop_attackidle_growl" );
	}
	return false;
}

shouldAttackIdle()
{
	return ( isdefined( self.enemy ) && isalive( self.enemy ) && distanceSquared( self.origin, self.enemy.origin ) < 1000000 );
}

should_growl()
{
	if ( isdefined( self.script_growl ) )
		return true;
	if ( !isalive( self.enemy ) )
		return true;
	return !( self cansee( self.enemy ) );
}

lookAtTarget( lookPoseSet )
{
	self endon( "killanimscript" );
	self endon( "stop tracking" );
	debug_anim_print("dog_stop::lookAtTarget() - Starting look at " + lookPoseSet  );

	self.rightAimLimit = 90;
	self.leftAimLimit = -90;
	self.upAimLimit = 45;
	self.downAimLimit = -45;

	self animscripts\coop_dog_shared::setAnimAimWeight( 1, 0.2 );
	self animscripts\coop_dog_shared::trackLoop( );
}
