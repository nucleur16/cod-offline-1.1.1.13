// zm hellhound animscript: animscripts/zombie_dog_combat.gsc
//
// Ported from this tree's multiplayer dog animscript
// maps/mp/animscripts/dog_combat.gsc (itself adapted from the shipped T5
// maps\mp\animscripts\dog_combat.gsc, header kept below).  The zm hellhound
// (species "zombie_dog", codol_actor_scripts.cpp) runs the same native T5 dog
// state machine bound against C1's animtrees/zombie_dog.atr, and every state
// is selected by name through setanimstate(), so the bodies are the MP
// dog's.  Changed for zm, the same way T5 forks animscripts\dog_* into
// animscripts\zombie_dog_*:
//   - module paths maps\mp\animscripts\dog_* -> ...\zombie_dog_*, so the zm
//     category compiles no multiplayer module (see _zombiemode.gsc);
//   - no maps\mp\_utility and no maps\mp\_dogs reference;
//   - T5 animscripts\zombie_dog_combat.gsc:229 dog_cant_kill_in_one_hit
//     "right now we want the dogs not to do the singleplayer instant-kill":
//     always true, so a hellhound only ever bites (its damage is its
//     meleeDamage, through _zombiemode::Callback_PlayerDamage), and the
//     bite shellshock is T5 zombie's 0.35 s;
//   - the debug dvar read goes through the local anim_get_dvar_int.
//   - C1's raised attack is selected for standing/crouched targets with a
//     clear attack trace; prone/obstructed targets retain the low bite.
//     Native navigation/root motion owns the approach, without MP gap teleport.

/*
	maps/mp/animscripts/dog_combat.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\dog_combat.gsc

	Changed for CoDOL:
	  - #include maps\mp\animscripts\shared + ...\utility and every namespaced
	    maps\mp\animscripts\shared::... call now point at the dog-local
	    animscripts\zombie_dog_shared module.
	  - vector_scale() -> vector_multiply(): the mounted IW4
	    common_scripts\utility has no vector_scale; vector_multiply
	    (common_scripts/utility.gsc:1457) is the same scalar-by-vector
	    operation and is already in scope.  Same substitution the CoDOL
	    _rcbomb port made.
	  - GetDvarInt( #"debug_dog_sound" ) rewritten to the plain string form:
	    the T5 #"..." canonical-hash token does not exist in this VM.
	  - playsound( "aml_dog_neckbreak" ) -> "anml_dog_bite": the
	    T5 alias has no counterpart in the C1 zones; the neck-break alias the
	    client does ship is anml_dog_neckbreak_collar
	    (obj/dog-asset-string-inventory.json).
	  - shellshock( "dog_bite" ) kept as-is: C1 ships shock/dog_bite.shock.
	  - melee() and teleport() kept verbatim; they are engine methods this
	    server does not register yet (reported as required builtins).
*/

#include common_scripts\utility;
#include animscripts\zombie_dog_shared;

main()
{
	{}

	self endon("killanimscript");
	self endon("death");
	self SetAimAnimWeights( 0, 0 );

/#
	if( !debug_allow_combat() )
	{
		combatIdle();
		return;
	}
#/

	if ( isDefined( level.hostMigrationTimer ) )
	{
		combatIdle();
		return;
	}


	assert( isdefined( self.enemy ) );
	if ( !isalive( self.enemy ) )
	{
		combatIdle();
		return;
	}

	if ( IsPlayer(self.enemy) )
		self meleeBiteAttackPlayer(self.enemy);
}

combatIdle()
{
	self set_orient_mode("face enemy");
	self animMode( "zonly_physics" );

	idleAnims = [];
	idleAnims[ 0 ] = "combat_attackidle";
	idleAnims[ 1 ] = "combat_attackidle_bark";
	idleAnims[ 2 ] = "combat_attackidle_growl";
	idleAnim = random( idleAnims );

	self thread combatIdlePreventOverlappingPlayer();

	{}
	self setanimstate( idleAnim );
	self animscripts\zombie_dog_shared::DoNoteTracks( "done" );
	{}

	self notify( "combatIdleEnd" );
}

// when player is in melee sequence, other dogs need to move away
combatIdlePreventOverlappingPlayer()
{
	self endon( "killanimscript" );
	self endon( "combatIdleEnd" );

	while( 1 )
	{
		wait 0.1;

		if ( !isdefined( self.enemy))
			continue;

		if ( !isdefined( self.enemy.syncedMeleeTarget ) || self.enemy.syncedMeleeTarget == self )
			continue;

		offsetVec = self.enemy.origin - self.origin;

		if ( offsetVec[2] * offsetVec[2] > 6400 )
			continue;

		offsetVec = ( offsetVec[0], offsetVec[1], 0 );

		offset = length( offsetVec );

		if ( offset < 1 )
			offsetVec = anglestoforward( self.angles );

		if ( offset < 30 )
		{
			offsetVec = vector_multiply( offsetVec, 3 / offset );
			self teleport( self.origin - offsetVec );
		}
	}
}

// prevent multiple dogs attacking at the same time and overlapping
shouldWaitInCombatIdle()
{
	if ( isDefined( level.hostMigrationTimer ) )
	{
		return true;
	}

	assert( isdefined( self.enemy ) && isalive( self.enemy ) );

	return isdefined( self.enemy.dogAttackAllowTime ) && ( gettime() < self.enemy.dogAttackAllowTime );
}

// call on target
setNextDogAttackAllowTime( time )
{
	self.dogAttackAllowTime = gettime() + time;
}

meleeBiteAttackPlayer(player)
{
	attackRangeBuffer = 30;

	for ( ;; )
	{
		if ( !isalive( self.enemy ) )
			break;

		// CoDOL: the loop bites the player it was started for, but
		// checkEndCombat and prepareAttackPlayer measure self.enemy.  When
		// the native sight pass hands the dog a nearer enemy mid-loop, the
		// two differ: MEASURED 2026-10-02 (mp_rust) a dog lunged in place
		// every 1.3 s at a player 1617 units away while a new enemy stood
		// beside it.  End the script; the native exposed think restarts
		// dog_combat for the new enemy when it is in attack range.
		if ( self.enemy != player )
			break;

		meleeRange = self.meleeAttackDist + attackRangeBuffer;

		if ( ( isdefined( player.syncedMeleeTarget ) && player.syncedMeleeTarget != self ) )
		{
			if ( checkEndCombat( meleeRange ) )
			{
				break;
			}
			else
			{
				combatIdle();
				continue;
			}
		}

		if ( self shouldWaitInCombatIdle() )
		{
			combatIdle();
			continue;
		}

		self set_orient_mode("face enemy");
		self animMode( "gravity" );

		self.safeToChangeScript = false;

		prepareAttackPlayer(player);

		//self clearpitchorient();

/#
		if ( GetDvarInt( "debug_dog_sound" ) )
			{}

#/
		player setNextDogAttackAllowTime( 200 );

		if ( dog_cant_kill_in_one_hit(player) )
		{
			level.lastDogMeleePlayerTime = getTime();
			level.dogMeleePlayerCounter++;

			if ( use_low_attack() )
			{
				// this is a hack using an existing anim to try and do a short range
				// ground based attack
				self animMode("angle deltas");
				self setanimstate( "combat_attack_player_close_range" );
				self animscripts\zombie_dog_attack_log::anim_chosen( "close_range" );

				doMeleeAfterWait( 0.1 );
				self animscripts\zombie_dog_shared::DoNoteTracksForTime( 1.4, "done" );
				self animscripts\zombie_dog_attack_log::end();

				self animMode( "gravity" );
			}
			else
			{
				{}
				self setanimstate( "combat_attack_run" );
				self animscripts\zombie_dog_attack_log::anim_chosen( "run" );
				// Mounted C1 zombie_dog_run_attack is 45 frames at 30 fps:
				// dog_melee is at .2 (300 ms at rate 1), end at 1. Use its
				// actual notetracks, including playback rate, for hit/recovery.
				self animscripts\zombie_dog_shared::DoNoteTracks( "done", ::handleMeleeBiteAttackNoteTracks, player );
				self animscripts\zombie_dog_attack_log::end();
				{}
			}
		}
		else
		{
			self thread dog_melee_death(player);
			player.attacked_by_dog = true;
			self thread clear_player_attacked_by_dog_on_death(player);

			{}
			self setanimstate( "combat_attack_player" );
			self animscripts\zombie_dog_shared::DoNoteTracks( "done", ::Handlemeleefinishattacknotetracks, player );
			{}
			self notify( "dog_no_longer_melee_able" );
			self setcandamage( true );
			self unlink();
		}

		self.safeToChangeScript = true;

		if ( checkEndCombat( meleeRange ) )
			break;
	}

	self.safeToChangeScript = true;
	self animMode("none");
}

doMeleeAfterWait( time )
{
	self endon("death");

	wait(time);
	if ( !isAlive( self ) )
		return;

	// not useing angles on the melee hit so it will always hit
	self animscripts\zombie_dog_attack_log::melee_before( self.enemy );
	hitEnt = self melee( );
	self animscripts\zombie_dog_attack_log::melee_after( self.enemy, hitEnt );
	if ( isdefined( hitEnt ) )
	{
		if ( isplayer(hitEnt) )
			hitEnt shellshock("dog_bite", 0.35);
	}
}

// CoDOL: the bite trace of C1's dog.  C1's dog animtree and clips are the
// IW4 dog's (codol_dog_states.cpp), and IW4 animscripts\dog\dog_combat
// dogMelee() bites a target the lunge has carried the dog into (inside 32
// units, bodies touching) straight at its eye instead of along the dog's
// facing: from there the eye-height trace along the facing starts beside
// or past the target and misses it (MEASURED 2026-10-02, mp_rust: bites
// within 35 units landed 29 of 86).  Farther out the T5 trace is kept.
// T5's reach for the hound's bite.  At the dog_melee notetrack (run_attack
// 0.20) T5's hound has reared up: its eye -- where the melee trace starts --
// is 0-6 units ahead of its origin and 65-66 up, and the trace reaches
// ai_meleeRange from there.  C1's run_attack lunges instead: at the same
// frame the eye is 36 ahead and 62 up.  The server's pose matches C1's own
// (AssetExplorer anim-pose of zombie_dog_run_attack on zombie_wolf, frame 9:
// eye 36.4 / 61.9), so C1's clip really differs, and its bites connected
// from up to ~100 units.  MEASURED (t5cap, Shi No Numa, frozen player): T5
// bites from 55-65 units landed 8 of 17, 65-75 19 of 67, 75 and over none;
// these landed 174 of 186 from 55-65.  The bite only connects when T5's eye
// would have reached the player, unblocked; the C1 trace then decides as
// before.
t5BiteReaches( player )
{
	if ( !isDefined( player ) )
		return true;
	forward = anglesToForward( ( 0, self.angles[1], 0 ) );
	eye = self.origin + vector_multiply( forward, 3 ) + ( 0, 0, 66 );
	target = player getEye();
	if ( distanceSquared( eye, target ) > t5Sq( getDvarFloat( "ai_meleeRange" ) ) )
		return false;
	// From T5's eye a hound queued behind another bites into that hound's
	// body (T5 Melee_Trace hits actors); C1's eye starts past it.
	trace = bulletTrace( eye, target, true, self );
	// World hits have no entity, just like a clear ray. Preserve the
	// fraction so a nose animated past cover cannot bite back through it.
	if ( !isDefined( trace[ "entity" ] ) && trace[ "fraction" ] < 1 )
		return false;
	if ( isDefined( trace[ "entity" ] ) && trace[ "entity" ] != player )
		return false;
	return true;
}

t5Sq( v )
{
	return v * v;
}

dogMelee()
{
	// A pending notetrack must not let a dead hound start another trace.
	if ( !isAlive( self ) )
		return undefined;

	if ( isDefined( self.enemy ) && distance2D( self.origin, self.enemy.origin ) < 32 )
		return self melee();

	// C1's raised clip moves tag_eye ahead of the touching bodies. Once
	// the eye passes a target still in front of the dog, retaining the
	// facing argument sends the native trace farther away from that target.
	// Use the existing target-directed trace for this measured geometry.
	if ( isDefined( self.enemy ) )
	{
		eye = self getEye();
		forward = anglesToForward( ( 0, self.angles[1], 0 ) );
		to_enemy = self.enemy.origin - self.origin;
		from_eye = self.enemy.origin - eye;
		to_enemy = ( to_enemy[0], to_enemy[1], 0 );
		from_eye = ( from_eye[0], from_eye[1], 0 );
		if ( distance2D( self.origin, self.enemy.origin ) <= distance2D( self.origin, eye ) &&
			vectorDot( to_enemy, forward ) >= 0 && vectorDot( from_eye, forward ) <= 0 )
			return self melee();
	}

	return self melee( anglesToForward( self.angles ) );
}

handleMeleeBiteAttackNoteTracks( note, player )
{
	self animscripts\zombie_dog_attack_log::note( note );
	switch ( note )
	{
		case "dog_melee":
		{
			if ( !isdefined(level.dogMeleeBiteAttackTime) )
			{
				level.dogMeleeBiteAttackTime = GetTime() - level.dogMeleeBiteAttackTimeStart;
				level.dogMeleeBiteAttackTime += 50;
			}

			self animscripts\zombie_dog_attack_log::melee_before( player );
			hitEnt = undefined;
			if ( self t5BiteReaches( player ) )
				hitEnt = self dogMelee();
			self animscripts\zombie_dog_attack_log::melee_after( player, hitEnt );

			if ( isdefined( hitEnt ) )
			{
				if ( isplayer(hitEnt) )
					hitEnt shellshock("dog_bite", 0.35);
			}
			else
			{
				attackMiss();
				return true;
			}
		}
		break;

		case "stop_tracking":
		{
			// best guess
			melee_time = 200;

			// how much longer until the bite
			// bit of a hack solution
			if ( !isdefined(level.dogMeleeBiteAttackTime) )
			{
				level.dogMeleeBiteAttackTimeStart = GetTime();
			}
			else
			{
				melee_time = level.dogMeleeBiteAttackTime;
			}

			self thread orientToPlayerDeadReckoning(player, melee_time);
		}
			break;
	}
}

handleMeleeFinishAttackNoteTracks( note, player )
{
	switch( note )
	{
		case "dog_melee":

			if ( !isdefined(level.dogMeleeFinishAttackTime) )
			{
				level.dogMeleeFinishAttackTime = GetTime() - level.dogMeleeFinishAttackTimeStart;
				level.dogMeleeFinishAttackTime += 50;
			}

			hitEnt = self melee( anglesToForward( self.angles ) );
			if ( isdefined( hitEnt ) && isalive( player ) )
			{
				if ( hitEnt == player )
				{
					break;
				}
			}
			else
			{
				attackMiss();
				return true;
			}
			break;

		case "dog_early":
			self notify( "dog_early_notetrack" );

			{}
			self setanimstate( "combat_attack_player_early" );
			break;

		case "dog_lunge":
			thread set_melee_timer(player);
			{}
			self setanimstate( "combat_attack_player_lunge" );
			break;

		case "dogbite_damage":

			self thread killplayer(player);
			break;

		case "stop_tracking":
		{
			// best guess
			melee_time = 200;

			// how much longer until the bite
			// bit of a hack solution
			if ( !isdefined(level.dogMeleeFinishAttackTime) )
			{
				level.dogMeleeFinishAttackTimeStart = GetTime();
			}
			else
			{
				melee_time = level.dogMeleeFinishAttackTime;
			}

			self thread orientToPlayerDeadReckoning(player, melee_time );
		}
			break;
	}
}

orientToPlayerDeadReckoning(player, time_till_bite )
{
	enemy_attack_current_origin = player.origin;
	enemy_attack_current_time = GetTime();

	enemy_motion_time_delta = enemy_attack_current_time - self.enemy_attack_start_time;
	enemy_motion_direction = enemy_attack_current_origin - self.enemy_attack_start_origin;

	if ( enemy_motion_time_delta == 0 )
	{
		enemy_predicted_position = player.origin;
	}
	else
	{
		enemy_velocity =  enemy_motion_direction / enemy_motion_time_delta;
		enemy_predicted_position = player.origin + (enemy_velocity * time_till_bite);
	}

	self set_orient_mode("face point", enemy_predicted_position );
}

checkEndCombat( meleeRange )
{
	if ( !isdefined( self.enemy ) )
		return false;

	distToTargetSq = distanceSquared( self.origin, self.enemy.origin );

	return ( distToTargetSq > meleeRange * meleeRange );
}

use_low_attack(player)
{
	// T5's stance/clearance choice, against C1's separate high/low clips.
	// Original CoDOL xWsKn1NEBt8 6:25/6:27 shows raised hound attacks;
	// forcing the low clip for every player stance suppresses that pose.
	height_diff = self.enemy_attack_start_origin[2] - self.origin[2];
	if ( height_diff < 30 && self.enemy_attack_start_stance == "prone" )
		return true;

	melee_origin = self.origin + ( 0, 0, 65 );
	enemy_origin = self.enemy.origin + ( 0, 0, 32 );
	return !BulletTracePassed( melee_origin, enemy_origin, false, self );
}

prepareAttackPlayer(player)
{
	level.dog_death_quote = &"SCRIPT_PLATFORM_DOG_DEATH_DO_NOTHING";
	distanceToTarget = distance( self.origin, self.enemy.origin );
	targetHeight = Abs(self.enemy.origin[2] - self.origin[2]);

	self.enemy_attack_start_distance = distanceToTarget;
	self.enemy_attack_start_origin = player.origin;
	self.enemy_attack_start_time = GetTime();
	self.enemy_attack_start_stance = player getStance();

	// The 32-unit Zombies approach already admits a native melee contact.
	// Selecting the raised pose must not restore the MP gap-closing teleport.

	if ( animscripts\zombie_dog_attack_log::enabled() )
	{
		teleport = 0;
		traceClear = BulletTracePassed( ( self.origin[0], self.origin[1], self.origin[2] + 65 ), ( self.enemy.origin[0], self.enemy.origin[1], self.enemy.origin[2] + 32 ), false, self );
		self animscripts\zombie_dog_attack_log::begin( player, use_low_attack(), traceClear, teleport );
	}

/#
	if ( anim_get_dvar_int( "debug_dog_attack", "0" )  )
	{
		teleported = "";

		//println("Attack Target - Dist: " + distanceToTarget + " Stance: " + self.enemy_attack_start_stance + " " + teleported);
	}
#/

}

// make up for error in intial attack jump position
attackTeleportThread( offset, player )
{
	self endon ("death");
	self endon ("killanimscript");
	reps = 5;
	increment = ( offset[0] / reps, offset[1] / reps, offset[2] / reps );

	// CoDOL: T5 ActorCmd_Teleport (OpenBLOPS actor_script_cmd.cpp) refuses
	// a move longer than 10 units when the player can see the actor or its
	// destination, so in T5 this thread only closes gaps of up to 50 units
	// (five 10-unit steps) on a dog its victim is watching.  The native
	// teleport here is unconditional; without this check a dog that began
	// its lunge 160 units out slid up to 80 units in a quarter second, and
	// an approaching target then ended up under it (MEASURED 2026-10-02,
	// mp_rust: 84 lunges begun beyond 117 units, 26 landed; the misses
	// stood 3-22 units from the target).
	longStep = ( increment[0] * increment[0] + increment[1] * increment[1] + increment[2] * increment[2] ) > 100;

	for ( i = 0; i < reps; i++ )
	{
		applied = !longStep || !attackTeleportSeen( player, self.origin + increment );
		if ( applied )
			self teleport (self.origin + increment);
		self animscripts\zombie_dog_attack_log::teleport_step( applied );
		wait (0.05);
	}
}

attackTeleportSeen( player, destination )
{
	if ( !isDefined( player ) || !isAlive( player ) )
		return false;
	eye = player getEye();
	return sightTracePassed( eye, self.origin + ( 0, 0, 32 ), false, self ) || sightTracePassed( eye, destination + ( 0, 0, 32 ), false, self );
}

player_attacked()
{
	return isalive( self ) && ( self MeleeButtonPressed() );
}

set_melee_timer(player)
{
	wait( 0.15 );
	self.melee_able_timer = gettime();
}


clear_player_attacked_by_dog_on_death(player)
{
	self waittill( "death" );
	player.attacked_by_dog = undefined;
}


dog_cant_kill_in_one_hit(player)
{
	// T5 animscripts\zombie_dog_combat.gsc:231.
	return true;

	// right now we want the dogs not to do the singleplayer "instant-kill"
	return true;

	if ( isdefined( player.dogs_dont_instant_kill ) )
	{
		assertex( player.dogs_dont_instant_kill, "Dont set player.dogs_dont_instant_kill to false, set to undefined" );
		return true;
	}

	if ( getTime() - level.lastDogMeleePlayerTime > 8000 )
		level.dogMeleePlayerCounter = 0;

	return level.dogMeleePlayerCounter < level.dog_hits_before_kill &&
		   player.health > 25;	// little more than the damage one melee dog bite hit will do
}

dog_melee_death(player)
{
	self endon( "killanimscript" );
	self endon( "dog_no_longer_melee_able" );
	pressed = false;

	// change this number for difficulty level:
	press_time = anim.dog_presstime;


	self waittill( "dog_early_notetrack" );

	while ( player player_attacked() )
	{
		// wait until the player lets go of the button, if he's holding it
		wait( 0.05 );
	}

	for ( ;; )
	{
		if ( !pressed )
		{

			if ( player player_attacked() )
			{
				pressed = true;
				if ( isdefined( self.melee_able_timer ) && isalive( player ) )
				{
					if ( gettime() - self.melee_able_timer <= press_time )
					{
						player.player_view.custom_dog_save = "neck_snap";
						self notify( "melee_stop" );
						{}
						self setanimstate( "combat_player_neck_snap" );

						self waittillmatch( "done", "dog_death" );
						{}
						self playsound( "anml_dog_bite", self gettagorigin( "tag_eye" ) );
						self setcandamage( true );
						self.a.nodeath = true;
						dif = player.origin - self.origin;
						dif = ( dif[ 0 ], dif [ 1 ], 0 );
						self dodamage( self.health + 503, self geteye() - dif, player );
						self notify( "killanimscript" );
					}
					else
					{
						{}
						self setanimstate( "combat_attack_player" );
						level.dog_death_quote = &"SCRIPT_PLATFORM_DOG_DEATH_TOO_LATE";
					}
					return;
				}

				level.dog_death_quote = &"SCRIPT_PLATFORM_DOG_DEATH_TOO_SOON";
				{}
				self setanimstate( "combat_player_neck_miss" );

				// once player clicks, if it is at the wrong time, he does not get another chance.
				return;
			}
		}
		else
		{
			if ( !player player_attacked() )
			{
				pressed = false;
			}
		}

		wait( 0.05 );
	}
}


attackMiss()
{
	if ( isdefined( self.enemy ) )
	{
		forward	= anglestoforward( self.angles );
		dirToEnemy = self.enemy.origin - ( self.origin + vector_multiply( forward, 50 ) );
		if ( vectordot( dirToEnemy, forward ) > 0 )
		{
			{}
			self setanimstate( "combat_attack_miss" );
			self thread animscripts\zombie_dog_stop::lookAtTarget( "normal" );
		}
		else
		{
			self.skipStartMove = true;
			self thread attackMissTrackTargetThread();

			if ( ( dirToEnemy[0] * forward[1] - dirToEnemy[1] * forward[0] ) > 0 )
			{
						{}
						self setanimstate( "combat_attack_miss_right" );
			}
			else
			{
						{}
						self setanimstate( "combat_attack_miss_left" );
			}
		}
	}
	else
	{
					{}
					self setanimstate( "combat_attack_miss" );
	}

	self animscripts\zombie_dog_shared::DoNoteTracks( "done" );
	{}
	self notify("stop tracking");
	{}
}


attackMissTrackTargetThread()
{
	self endon( "killanimscript" );

	wait 0.6;
	self set_orient_mode( "face enemy" );
}

killplayer(player)
{
	self endon( "pvd_melee_interrupted" );

	player.specialDeath = true;
	player setcandamage( true );

	wait 1;

	damage = player.health + 1;

	if ( !isalive( player ) )
		return;

}
