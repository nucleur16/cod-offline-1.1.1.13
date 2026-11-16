// zm hellhound animscript: animscripts/zombie_dog_move.gsc
//
// Ported from this tree's multiplayer dog animscript
// maps/mp/animscripts/dog_move.gsc (itself adapted from the shipped T5
// maps\mp\animscripts\dog_move.gsc, header kept below).  The zm hellhound
// (species "zombie_dog", codol_actor_scripts.cpp) runs the same native T5 dog
// state machine bound against C1's animtrees/zombie_dog.atr, and every state
// is selected by name through setanimstate(), so the bodies are the MP
// dog's.  Changed for zm, the same way T5 forks animscripts\dog_* into
// animscripts\zombie_dog_*:
//   - module paths maps\mp\animscripts\dog_* -> ...\zombie_dog_*, so the zm
//     category compiles no multiplayer module (see _zombiemode.gsc);
//   - no maps\mp\_utility and no maps\mp\_dogs reference;
//   - the movement barks are C1's zm hellhound vocals
//     (zmb_hellhound_vocals_close/_amb, common_gametype_zm); anml_dog_bark
//     ships only in the mp/coop gametype zones.

/*
	maps/mp/animscripts/dog_move.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\dog_move.gsc

	Changed for CoDOL:
	  - #include maps\mp\animscripts\utility + ...\shared and every namespaced
	    maps\mp\animscripts\shared::... call now point at the dog-local
	    animscripts\zombie_dog_shared module.
	  - setup_sound_variables(): the close and ambient families use C1's
	    zmb_hellhound_vocals_close / _amb aliases in common_gametype_zm.
	  - playMovementSounds(): T5 barks while a dog moves from its client
	    script (clientscripts\mp\_dogs.csc playMovementSounds).  C1 runs no
	    client script and its dog notetracks (mp/constconfigstrings_mp.csv
	    6134-6149) only carry footsteps, run start, pain, death and attack
	    sounds, so the running dog was silent.  The CSC state machine is
	    ported to the server here; see the function for the adaptation.
*/

#include animscripts\zombie_dog_shared;

//#using_animtree ("dog");

setup_sound_variables()
{
	// these should be ordered by distance far to close
	level.dog_sounds["far"] = spawnstruct();
	level.dog_sounds["close"] = spawnstruct();

	level.dog_sounds["close"].minRange = 0;
	level.dog_sounds["close"].maxRange = 500;
	level.dog_sounds["close"].sound = "zmb_hellhound_vocals_close";
	level.dog_sounds["close"].soundLengthPlaceholder = 0.2;
	level.dog_sounds["close"].afterSoundWaitMin = 0.1;
	level.dog_sounds["close"].afterSoundWaitMax = 0.3;
	level.dog_sounds["close"].minRangeSqr = level.dog_sounds["close"].minRange * level.dog_sounds["close"].minRange;
	level.dog_sounds["close"].maxRangeSqr = level.dog_sounds["close"].maxRange * level.dog_sounds["close"].maxRange;

	level.dog_sounds["far"].minRange = 500;
	level.dog_sounds["far"].maxRange = 0;   // zero meaning infinity
	level.dog_sounds["far"].sound = "zmb_hellhound_vocals_amb";
	level.dog_sounds["far"].soundLengthPlaceholder = 0.2;
	level.dog_sounds["far"].afterSoundWaitMin = 0.1;
	level.dog_sounds["far"].afterSoundWaitMax = 0.3;
	level.dog_sounds["far"].minRangeSqr = level.dog_sounds["far"].minRange * level.dog_sounds["far"].minRange;
	level.dog_sounds["far"].maxRangeSqr = level.dog_sounds["far"].maxRange * level.dog_sounds["far"].maxRange;

	setup_movement_sound_variables();
}

// T5 clientscripts\mp\_dogs.csc init(): the movement bark states. Adapt its
// soundplaying() wait to the actual C1 hellhound alias duration; the T5
// aml_dog_* clips are shorter. Keep its randomized rest after each clip.
setup_movement_sound_variables()
{
	level.movementStateSound = [];

	level.maxStateSoundDistance = 99999999;

	level.movementStateSound["normal"] = spawnStruct();
	level.movementStateSound["normal"].sound = "zmb_hellhound_vocals_amb";
	level.movementStateSound["normal"].waitMax = 4;
	level.movementStateSound["normal"].waitMin = 1;
	level.movementStateSound["normal"].enemyrange = level.maxStateSoundDistance;  // this has no range
	level.movementStateSound["normal"].soundLengthMs = movement_sound_length_ms( "zmb_hellhound_vocals_amb", 998 );
	level.movementStateSound["normal"].localenemyonly = false;

	// this is for everyone other then the enemy
	level.movementStateSound["attack_mid_everyone"] = spawnStruct();
	level.movementStateSound["attack_mid_everyone"].sound = "zmb_hellhound_vocals_amb";
	level.movementStateSound["attack_mid_everyone"].waitMax = 2;
	level.movementStateSound["attack_mid_everyone"].waitMin = 0.5;
	level.movementStateSound["attack_mid_everyone"].enemyrange = 1500;
	level.movementStateSound["attack_mid_everyone"].soundLengthMs = movement_sound_length_ms( "zmb_hellhound_vocals_amb", 998 );
	level.movementStateSound["attack_mid_everyone"].localenemyonly = false;

	// this is only for the enemy to hear
	level.movementStateSound["attack_mid_enemy"] = spawnStruct();
	level.movementStateSound["attack_mid_enemy"].sound = "zmb_hellhound_vocals_amb";
	level.movementStateSound["attack_mid_enemy"].waitMax = 0.5;
	level.movementStateSound["attack_mid_enemy"].waitMin = 0.01;
	level.movementStateSound["attack_mid_enemy"].enemyrange = 1500;
	level.movementStateSound["attack_mid_enemy"].soundLengthMs = movement_sound_length_ms( "zmb_hellhound_vocals_amb", 998 );
	level.movementStateSound["attack_mid_enemy"].localenemyonly = true;

	// this is only for the enemy to hear
	level.movementStateSound["attack_close_enemy"] = spawnStruct();
	level.movementStateSound["attack_close_enemy"].sound = "zmb_hellhound_vocals_close";
	level.movementStateSound["attack_close_enemy"].waitMax = 0.1;
	level.movementStateSound["attack_close_enemy"].waitMin = 0.01;
	level.movementStateSound["attack_close_enemy"].enemyrange = 1000;
	level.movementStateSound["attack_close_enemy"].soundLengthMs = movement_sound_length_ms( "zmb_hellhound_vocals_close", 719 );
	level.movementStateSound["attack_close_enemy"].localenemyonly = true;
}

// Use the mounted C1 alias duration, not the shorter T5 bark being replaced.
// Both shipped base and patch tables give amb 998 ms and close 719 ms.
movement_sound_length_ms( alias, measured_fallback )
{
	length_ms = int( tableLookup( "sound/soundlength.csv", 0, alias, 1 ) );
	if ( length_ms > 0 )
		return length_ms;
	return measured_fallback;
}

// T5 clientscripts\mp\_dogs.csc playMovementSounds / getMovementSoundState,
// run on the server.  In T5 every client runs this loop for each dog while
// the dog's anim category is "move" and picks the closest state it qualifies
// for: the player the dog is hunting gets the *_enemy states (rapid barks
// within 1500/1000 units), everyone else the everyone states.  The server
// routes the hunted player's state and the everyone state separately. The
// loop lives as long as dog_move (killanimscript); pending playback deadlines
// stay on the actor per listener so re-entering move cannot cut off a bark.
playMovementSounds()
{
	self endon( "killanimscript" );
	self endon( "death" );

	while ( 1 )
	{
		enemy = undefined;
		if ( isDefined( self.enemy ) && isPlayer( self.enemy ) )
			enemy = self.enemy;

		self movementSoundStream( getMovementSoundState( false ), enemy, false );

		if ( isDefined( enemy ) )
			self movementSoundStream( getMovementSoundState( true ), enemy, true );

		wait( 0.05 );
	}
}

movementSoundStream( state, enemy, toEnemy )
{
	if ( toEnemy )
	{
		self movementSoundForPlayer( state, enemy );
	}
	else
	{
		foreach ( player in level.players )
		{
			if ( isDefined( enemy ) && player == enemy )
				continue;
			self movementSoundForPlayer( state, player );
		}
	}
}

movementSoundForPlayer( state, player )
{
	if ( !isDefined( player ) )
		return;
	if ( !isDefined( self.zm_dog_movement_audio ) )
		self.zm_dog_movement_audio = [];
	listener = player getEntityNumber();
	stream = self.zm_dog_movement_audio[ listener ];
	if ( !isDefined( stream ) || !isDefined( stream.player ) || stream.player != player )
	{
		stream = spawnStruct();
		stream.player = player;
		stream.state = undefined;
		stream.audible_until = 0;
		stream.next = 0;
		self.zm_dog_movement_audio[ listener ] = stream;
	}

	// This actor-owned listener record survives animscript restarts, changes
	// of quarry and movement categories. A state change can shorten the rest
	// after a completed bark; it cannot restart audio that is still playing.
	now = getTime();
	if ( now < stream.audible_until )
		return;
	state_changed = !isDefined( stream.state ) || state != stream.state;
	if ( !state_changed && now < stream.next )
		return;

	sound = level.movementStateSound[ state ].sound;
	self playSoundToPlayer( sound, player );

	stream.state = state;
	stream.wait_time = 1000 * randomfloatrange( level.movementStateSound[state].waitMin, level.movementStateSound[state].waitMax );
	stream.audible_until = now + level.movementStateSound[state].soundLengthMs;
	stream.next = stream.audible_until + stream.wait_time;
}

getMovementSoundState( localPlayer )
{
	closest_dist = level.maxStateSoundDistance * level.maxStateSoundDistance;
	closest_key = "normal";

	if ( !isDefined( self.enemy ) )
		return "normal";

	enemy_distance = distancesquared( self.origin, self.enemy.origin );

	stateArray = getArrayKeys( level.movementStateSound );
	for ( i = 0; i < stateArray.size; i++ )
	{
		if ( level.movementStateSound[stateArray[i]].localenemyonly && !localPlayer )
			continue;

		state_dist = level.movementStateSound[stateArray[i]].enemyrange;
		state_dist *= state_dist;

		if ( state_dist < enemy_distance )
			continue;

		if ( state_dist < closest_dist )
		{
			closest_dist = state_dist;
			closest_key = stateArray[i];
		}
	}

	return closest_key;
}

main()
{
	self endon("killanimscript");

	{}
	self SetAimAnimWeights( 0, 0 );

	do_movement = true;

	/#
		if( !debug_allow_movement() )
		{
			do_movement = false;
		}
	#/

	if ( isDefined( level.hostMigrationTimer ) )
	{
		do_movement = false;
	}

	if ( !isdefined( self.traverseComplete ) && !isdefined( self.skipStartMove ) && self.a.movement == "run" && do_movement )
	{
		self startMove();
		blendTime = 0;
	}
	else
	{
		blendTime = 0.2;
	}

	self.traverseComplete = undefined;
	self.skipStartMove = undefined;

	if ( do_movement )
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

	// T5 animscripts/zombie_dog_move.gsc:51 leaves the look-at commented out:
	// a hound runs and trots with its head on its path.  MEASURED (t5cap dog
	// clip timeline, 2026-10-22): no T5 hound frame moved a look additive;
	// CoDOL's look_up/left/right moved in 208 frames of a chase.
	// C1 has no T5 clientscript sound owner, and this port has neither
	// dog_stalk_audio nor dog_behind_audio. Keep the existing C1 hellhound
	// close/ambient streams alive while moving. They retain per-listener
	// playback deadlines across animscript restarts and end with this move.
	self thread playMovementSounds();

	while ( 1 )
	{
		self moveLoop();

		if ( self.a.movement == "run" )
		{
			if ( self.disableArrivals == false )
			self thread stopMove();

			// if a "run" notify is received while stopping, clear stop anim and go back to moveLoop
			self waittill( "run" );
		}
	}
}

moveLoop()
{
	self endon( "killanimscript" );
	self endon( "stop_soon" );

	while (1)
	{
		do_movement = true;

		/#
			if( !debug_allow_movement() )
			{
				do_movement = false;
			}
		#/

		if ( isDefined( level.hostMigrationTimer ) )
		{
			do_movement = false;
		}

		if( !do_movement )
		{
			self SetAimAnimWeights( 0, 0 );
			self setanimstate( "stop_idle" );
			animscripts\zombie_dog_shared::DoNoteTracks( "done" );
			continue;
		}

		if ( self.disableArrivals )
			self.stopAnimDistSq = 0;
		else
			self.stopAnimDistSq = level.dogStoppingDistSq;

		if ( self.a.movement == "run" )
		{
			{}
			self setanimstate( "move_run" );
			animscripts\zombie_dog_shared::DoNoteTracksForTime(0.2, "done");
			{}
		}
		else
		{
			assert( self.a.movement == "walk" );

			{}
			self setanimstate( "move_walk" );
			animscripts\zombie_dog_shared::DoNoteTracksForTime(0.1, "done");
			{}
		}
	}
}

startMoveTrackLookAhead()
{
	self endon("killanimscript");
	for ( i = 0; i < 2; i++ )
	{
		lookaheadAngle = vectortoangles( self.lookaheaddir );
		self set_orient_mode( "face angle", lookaheadAngle );
	}
}

startMove()
{
	{}
	self setanimstate( "move_start" );
	animscripts\zombie_dog_shared::DoNoteTracks("done");
	{}

	self animMode( "none" );
	self set_orient_mode( "face motion" );
}


stopMove()
{
	self endon( "killanimscript" );
	self endon( "run" );

	{}
	self setanimstate( "move_stop" );
	animscripts\zombie_dog_shared::DoNoteTracks("done");
	{}
}

getEnemyDistanceSqr()
{
	if ( isdefined(self.enemy) )
	{
		return DistanceSquared( self.origin, self.enemy.origin );
	}

	return (10000 * 10000);
}

getSoundKey( distanceSqr )
{
	keys = getarraykeys(level.dog_sounds);
	for ( i = 0; i < keys.size; i++ )
	{
		sound_set = level.dog_sounds[keys[i]];

		if ( sound_set.minRangeSqr > distanceSqr )
			continue;

		if ( sound_set.maxRangeSqr && sound_set.maxRangeSqr < distanceSqr )
			continue;

		return keys[i];
	}
	return keys[keys.size - 1];
}
