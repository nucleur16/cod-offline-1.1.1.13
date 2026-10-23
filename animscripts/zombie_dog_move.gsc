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
	  - setup_sound_variables(): T5's "aml_dog_bark_close" / "aml_dog_bark"
	    are not MP aliases in C1 (the aml_* dog sounds ship only in
	    common_gametype_zm); C1's MP bark is "zmb_hellhound_vocals_amb"
	    (common_gametype_mp), so both sets name it.  As in T5 this table is
	    not read by any MP script.
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

// T5 clientscripts\mp\_dogs.csc init(): the movement bark states.  T5's
// aliases are aml_dog_bark (normal), aml_dog_bark_mid (both attack_mid
// states) and aml_dog_bark_close; C1 MP ships one bark, anml_dog_bark, so
// every state names it.  soundLength stands in for the CSC's soundplaying()
// wait and keeps T5's cadence: it is the length of the T5 alias the state
// played, as C1's own sound/soundlength.csv lists it (aml_dog_bark 170 ms,
// aml_dog_bark_mid 170 ms, aml_dog_bark_close 640 ms).
setup_movement_sound_variables()
{
	level.movementStateSound = [];

	level.maxStateSoundDistance = 99999999;

	level.movementStateSound["normal"] = spawnStruct();
	level.movementStateSound["normal"].sound = "zmb_hellhound_vocals_amb";
	level.movementStateSound["normal"].waitMax = 4;
	level.movementStateSound["normal"].waitMin = 1;
	level.movementStateSound["normal"].enemyrange = level.maxStateSoundDistance;  // this has no range
	level.movementStateSound["normal"].soundLength = 0.170;
	level.movementStateSound["normal"].localenemyonly = false;

	// this is for everyone other then the enemy
	level.movementStateSound["attack_mid_everyone"] = spawnStruct();
	level.movementStateSound["attack_mid_everyone"].sound = "zmb_hellhound_vocals_amb";
	level.movementStateSound["attack_mid_everyone"].waitMax = 2;
	level.movementStateSound["attack_mid_everyone"].waitMin = 0.5;
	level.movementStateSound["attack_mid_everyone"].enemyrange = 1500;
	level.movementStateSound["attack_mid_everyone"].soundLength = 0.170;
	level.movementStateSound["attack_mid_everyone"].localenemyonly = false;

	// this is only for the enemy to hear
	level.movementStateSound["attack_mid_enemy"] = spawnStruct();
	level.movementStateSound["attack_mid_enemy"].sound = "zmb_hellhound_vocals_amb";
	level.movementStateSound["attack_mid_enemy"].waitMax = 0.5;
	level.movementStateSound["attack_mid_enemy"].waitMin = 0.01;
	level.movementStateSound["attack_mid_enemy"].enemyrange = 1500;
	level.movementStateSound["attack_mid_enemy"].soundLength = 0.170;
	level.movementStateSound["attack_mid_enemy"].localenemyonly = true;

	// this is only for the enemy to hear
	level.movementStateSound["attack_close_enemy"] = spawnStruct();
	level.movementStateSound["attack_close_enemy"].sound = "zmb_hellhound_vocals_amb";
	level.movementStateSound["attack_close_enemy"].waitMax = 0.1;
	level.movementStateSound["attack_close_enemy"].waitMin = 0.01;
	level.movementStateSound["attack_close_enemy"].enemyrange = 1000;
	level.movementStateSound["attack_close_enemy"].soundLength = 0.640;
	level.movementStateSound["attack_close_enemy"].localenemyonly = true;
}

// T5 clientscripts\mp\_dogs.csc playMovementSounds / getMovementSoundState,
// run on the server.  In T5 every client runs this loop for each dog while
// the dog's anim category is "move" and picks the closest state it qualifies
// for: the player the dog is hunting gets the *_enemy states (rapid barks
// within 1500/1000 units), everyone else the everyone states.  The server
// keeps one loop per listener class: the hunted player is sent his stream
// with playSoundToPlayer, every other player the everyone stream.  The loop
// lives as long as dog_move (killanimscript), as the CSC loop lives as long
// as the "move" category.
playMovementSounds()
{
	self endon( "killanimscript" );
	self endon( "death" );

	everyone = spawnStruct();
	everyone.state = "normal";
	everyone.next = 0;
	everyone.wait_time = 0;

	hunted = spawnStruct();
	hunted.state = "normal";
	hunted.next = 0;
	hunted.wait_time = 0;
	hunted.player = undefined;

	while ( 1 )
	{
		enemy = undefined;
		if ( isDefined( self.enemy ) && isPlayer( self.enemy ) )
			enemy = self.enemy;

		// a new quarry starts its own stream, as its client's loop would
		if ( !isDefined( enemy ) || !isDefined( hunted.player ) || hunted.player != enemy )
		{
			hunted.player = enemy;
			hunted.state = "normal";
			hunted.next = 0;
			hunted.wait_time = 0;
		}

		self movementSoundStream( everyone, getMovementSoundState( false ), enemy, false );

		if ( isDefined( enemy ) )
			self movementSoundStream( hunted, getMovementSoundState( true ), enemy, true );

		wait( 0.05 );
	}
}

movementSoundStream( stream, state, enemy, toEnemy )
{
	now = getTime();
	next_sound = false;

	// "New State forcing next sound".  T5 compares waitMax (seconds) with
	// wait_time (milliseconds), so in practice any state change barks at once;
	// kept as shipped.
	if ( state != stream.state && level.movementStateSound[state].waitMax < stream.wait_time )
		next_sound = true;

	if ( !next_sound && now < stream.next )
		return;

	sound = level.movementStateSound[state].sound;
	if ( toEnemy )
	{
		self playSoundToPlayer( sound, enemy );
	}
	else
	{
		foreach ( player in level.players )
		{
			if ( isDefined( enemy ) && player == enemy )
				continue;
			self playSoundToPlayer( sound, player );
		}
	}

	stream.state = state;
	stream.wait_time = 1000 * randomfloatrange( level.movementStateSound[state].waitMin, level.movementStateSound[state].waitMax );
	stream.next = now + int( level.movementStateSound[state].soundLength * 1000 ) + stream.wait_time;
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

	debug_anim_print("dog_move::main()" );
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
			debug_anim_print("dog_move::main() - Setting move_run" );
			self setanimstate( "move_run" );
			animscripts\zombie_dog_shared::DoNoteTracksForTime(0.1, "done");
			debug_anim_print("dog_move::main() - move_run wait 0.1 done " );
		}
		else
		{
			debug_anim_print("dog_move::main() - Setting move_start ");
			self setanimstate( "move_walk" );
		}
	}

	// T5 animscripts/zombie_dog_move.gsc:51 leaves the look-at commented out:
	// a hound runs and trots with its head on its path.  MEASURED (t5cap dog
	// clip timeline, 2026-10-22): no T5 hound frame moved a look additive;
	// CoDOL's look_up/left/right moved in 208 frames of a chase.
	// T5 animscripts/zombie_dog_move.gsc:14 leaves the running bark loop
	// commented out: a hellhound's voice is _zombiemode_ai_dogs'
	// dog_stalk_audio / dog_behind_audio, not the multiplayer dog's
	// movement barks (playMovementSounds, kept below for reference).

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
			debug_anim_print("dog_move::moveLoop() - Setting move_run" );
			self setanimstate( "move_run" );
			animscripts\zombie_dog_shared::DoNoteTracksForTime(0.2, "done");
			debug_anim_print("dog_move::moveLoop() - move_run wait 0.2 done " );
		}
		else
		{
			assert( self.a.movement == "walk" );

			debug_anim_print("dog_move::moveLoop() - Setting move_walk " );
			self setanimstate( "move_walk" );
			animscripts\zombie_dog_shared::DoNoteTracksForTime(0.2, "done");
			debug_anim_print("dog_move::moveLoop() - move_walk wait 0.2 done " );

			// T5 zombie_dog_move.gsc:104-110, the "stalking" behaviour.
			if ( self need_to_run() )
			{
				self.a.movement = "run";
				self notify( "dog_running" );
			}
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
	debug_anim_print("dog_move::startMove() - Setting move_start " );
	self setanimstate( "move_start" );
	animscripts\zombie_dog_shared::DoNoteTracks("done");
	debug_anim_print("dog_move::startMove() - move_start notify done." );

	self animMode( "none" );
	self set_orient_mode( "face motion" );
}


stopMove()
{
	self endon( "killanimscript" );
	self endon( "run" );

	debug_anim_print("dog_move::stopMove() - Setting move_stop" );
	self setanimstate( "move_stop" );
	animscripts\zombie_dog_shared::DoNoteTracks("done");
	debug_anim_print("dog_move::stopMove() - move_stop notify done." );
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

// T5 animscripts/zombie_dog_move.gsc:250 need_to_run: a stalking hound breaks
// into the sprint once it has been hurt, or once it sees its enemy inside
// scr_dog_run_distance (500), within 64 units of its height, 20 degrees of
// its facing and 30 degrees of pitch.
need_to_run()
{
	run_dist_squared = 384 * 384;
	if ( getDvar( "scr_dog_run_distance" ) != "" )
	{
		dist = getDvarInt( "scr_dog_run_distance" );
		run_dist_squared = dist * dist;
	}

	run_yaw = 20;
	run_pitch = 30;
	run_height = 64;

	if ( self.a.movement != "walk" )
		return false;

	if ( self.health < self.maxhealth )
		return true;	// dog took damage

	if ( !isdefined( self.enemy ) || !isalive( self.enemy ) )
		return false;

	if ( !self cansee( self.enemy ) )
		return false;

	dist = distancesquared( self.origin, self.enemy.origin );
	if ( dist > run_dist_squared )
		return false;

	height = self.origin[2] - self.enemy.origin[2];
	if ( abs( height ) > run_height )
		return false;

	// T5 AbsYawToEnemy: the yaw from the dog's facing to its enemy.
	yaw = abs( angleclamp180( vectortoangles( self.enemy.origin - self.origin )[1] - self.angles[1] ) );
	if ( yaw > run_yaw )
		return false;

	pitch = angleclamp180( vectortoangles( self.origin - self.enemy.origin )[0] );
	if ( abs( pitch ) > run_pitch )
		return false;

	return true;
}
