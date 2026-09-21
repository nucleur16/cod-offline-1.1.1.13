// zm spawner: the round's spawn budget, and the actors that consume it.
//
// Port of the BUDGET half of T5's maps\_zombiemode_spawner.gsc (5,030 lines)
// plus the spawn loop from maps\_zombiemode.gsc:3103 round_spawning().
//
// ============================== SCOPE ======================================
//
// A second workstream is porting T5's scripted-animation builtins (%anim
// tokens, setanim, setanimknob, animscripted, notetracks).  MEASURED
// 2026-09-20: this server registers none of them, and naming one in GSC is a
// COMPILE error that kills the match at map load, not a runtime failure.  So
// everything in T5's spawner that drives the actor's BODY is left out, and
// what is here is only what drives the round.
//
// TAKEN, by T5 function:
//   round_spawning              _zombiemode.gsc:3103  -- the whole budget:
//                               ai_calculate_health, the per-player multiplier,
//                               the >=10 exponential, max_zombie_func, the
//                               zombie_ai_limit gate, the spawn_delay pacing
//                               and the level.zombie_total decrement.
//   default_max_zombie_func     _zombiemode.gsc:3075  -- the round 1-6 ramp.
//   zombie_speed_up             _zombiemode.gsc:3294  -- ported as the
//                               move-speed scale only (see below).
//   round_spawn_failsafe        _zombiemode.gsc:4059  -- the stuck/fell-out-of
//                               -world killer.  This one is not optional here:
//                               without an animation layer a zombie that
//                               cannot reach a player would hold the round
//                               open forever.
//   zombie_spawn_init           _zombiemode_spawner.gsc:142 -- the data half
//                               (health from level.zombie_health, team, the
//                               suppression/dodge/arrival disables).
//   zombie_death_event          _zombiemode_spawner.gsc:3795 -- the kill
//                               accounting and the attacker's point award.
//   find_flesh                  _zombiemode_spawner.gsc:3922 -- reduced to
//                               "walk at the nearest valid player", which is
//                               the only part of it that does not go through
//                               the animscript state machine.
//
// NOT TAKEN, left for the animation workstream, by T5 function:
//   zombie_setup_attack_properties (:3899), the whole animscript handoff in
//   zombie_think (:415), set_zombie_run_cycle, zombie_gib_on_damage,
//   zombie_head_gib, zombie_eye_glow / delayed_zombie_eye_glow,
//   zombie_death_animscript, make_crawler, do_zombies_playvocals /
//   play_ambient_zombie_vocals, and every maps\_zombiemode_ai_* variant
//   (dogs, apes, monkeys, thieves, napalm, quads, astro, sonic, faller).
//   T5 reaches all of these through %anim / animscripted / DoNoteTracks.
//   Also not taken: allowedStances(), setTeamForEntity(), PushPlayer(),
//   disable_react() and setGoalRadius() -- MEASURED, none of the five is a
//   registered builtin on this server.
//
// MELEE.  T5 zombies damage players from an animscript notetrack.  With no
// notetracks there is no melee, so a zombie that reaches a player does
// nothing.  That is a deliberate hole and the animation workstream's, not a
// stub standing in for logic: no fake proximity-damage timer is shipped here,
// because it would be invented gameplay rather than a port.

#include maps\mp\gametypes\_zombiemode_utility;

// --------------------------------------------------------------------------
// Init.  T5 _zombiemode_spawner.gsc:9 init() reads the map's spawner
// entities; CoDOL's zm maps author zombie spawn points as script_structs with
// script_noteworthy "zombie_spawner" (51 of them on zm_zombie_training,
// docs/CODOL_ZOMBIES_STARTUP_20260915.md section 3.6) rather than as
// actor_zombie_* entities the engine would build spawners from.
// --------------------------------------------------------------------------

init()
{
	level.zombies = [];
	level.zombie_total = 0;
	level.total_zombies_killed = 0;
	level.zombie_ai_limit = 24;
	level.zombie_health = level.zombie_vars[ "zombie_health_start" ];
	level.zombie_move_speed = 1;

	flag_init( "spawn_zombies" );
	flag_set( "spawn_zombies" );
	flag_init( "begin_spawning" );
	flag_init( "end_round_wait" );

	init_spawn_points();
	init_spawner();
}

// T5 keys the spawn list off the active zone (maps\_zombiemode_zone_manager).
// Zones are not ported -- they gate which spawners a round may use, which only
// matters once doors divide the map, and the door flags that would drive it
// are the blockers module's.  Every authored spawn point is live instead, in
// the order the map declares them.
init_spawn_points()
{
	level.enemy_spawns = [];

	structs = get_struct_array( "zombie_spawner", "script_noteworthy" );
	for ( i = 0; i < structs.size; i++ )
		level.enemy_spawns[ level.enemy_spawns.size ] = structs[ i ];

	// zm_zombie_training also names its spawner groups by targetname
	// (start_zone_spawners, upstairs_zone_spawners, box_zone_spawners,
	// window_spawners).  Those are the same structs under a second key on that
	// map, but zm_asylum_sh and zm_prototype_sh need not agree, so the
	// noteworthy scan above is the primary and this is a fallback only.
	if ( level.enemy_spawns.size < 1 )
	{
		names = [];
		names[ 0 ] = "start_zone_spawners";
		names[ 1 ] = "upstairs_zone_spawners";
		names[ 2 ] = "box_zone_spawners";
		names[ 3 ] = "window_spawners";
		names[ 4 ] = "main_zombie_spawner";

		for ( i = 0; i < names.size; i++ )
		{
			structs = get_struct_array( names[ i ], "targetname" );
			for ( j = 0; j < structs.size; j++ )
				level.enemy_spawns[ level.enemy_spawns.size ] = structs[ j ];
		}
	}

	println( "CoDOL zm: " + level.enemy_spawns.size + " zombie spawn points on " + getDvar( "mapname" ) );
}

// The template every zombie is duplicated from.  Same two-step the dogs
// killstreak uses (maps/mp/_dogs.gsc:650-706): prefer an authored actor
// spawner, otherwise build one from the aitype through the engine's own actor
// spawner path so that aitype/enemy_zombie_zm::spawner() still runs on it.
init_spawner()
{
	level.zombie_spawner = undefined;

	candidates = getEntArray( "zombie_spawner", "targetname" );
	for ( i = 0; i < candidates.size; i++ )
	{
		usable = candidates[ i ] isActorSpawner();
		if ( isDefined( usable ) && usable )
		{
			level.zombie_spawner = candidates[ i ];
			break;
		}
	}

	if ( !isDefined( level.zombie_spawner ) )
	{
		level.zombie_spawner = spawnActorSpawner( "actor_enemy_zombie_zm", ( 0, 0, 0 ) );

		if ( !isDefined( level.zombie_spawner ) )
		{
			// The native side has already printed a "CoDOL actor:" line naming
			// the cause.  The round loop reads this and stops asking.
			println( "CoDOL zm: spawnactorspawner produced no zombie spawner; no enemies this match\n" );
			level.no_zombies = true;
			return;
		}

		level.zombie_spawner.targetname = "zombie_spawner";
	}

	println( "CoDOL zm: zombie spawner ready\n" );
}

// --------------------------------------------------------------------------
// The budget.  T5 _zombiemode.gsc:3075 and :3103, unchanged arithmetic.
// --------------------------------------------------------------------------

default_max_zombie_func( max_num )
{
	max = max_num;

	if ( is_true( level.first_round ) )
		max = int( max_num * 0.25 );
	else if ( level.round_number < 3 )
		max = int( max_num * 0.3 );
	else if ( level.round_number < 4 )
		max = int( max_num * 0.5 );
	else if ( level.round_number < 5 )
		max = int( max_num * 0.7 );
	else if ( level.round_number < 6 )
		max = int( max_num * 0.9 );

	return max;
}

// T5 _zombiemode.gsc:4017.  Round 1 health, +zombie_health_increase per round
// to round 9, then compounding by zombie_health_increase_multiplier.
ai_calculate_health( round_number )
{
	level.zombie_health = level.zombie_vars[ "zombie_health_start" ];

	for ( i = 2; i <= round_number; i++ )
	{
		if ( i >= 10 )
			level.zombie_health += int( level.zombie_health * level.zombie_vars[ "zombie_health_increase_multiplier" ] );
		else
			level.zombie_health = int( level.zombie_health + level.zombie_vars[ "zombie_health_increase" ] );
	}
}

// T5 _zombiemode.gsc:3103, with the dog-mixing block and the zone occupancy
// walk left out (no dogs species and no zone manager in this port).
round_spawning()
{
	level endon( "intermission" );
	level endon( "end_of_round" );
	level endon( "restart_round" );

	if ( is_true( level.intermission ) )
		return;

	ai_calculate_health( level.round_number );

	if ( is_true( level.no_zombies ) || level.enemy_spawns.size < 1 )
	{
		// T5 ASSERTMSGs here ("No active spawners in the map").  An assert
		// would end the match; the round budget is still published so the HUD
		// and the round number are honest about what round it is, and
		// round_wait's own end_round_wait escape closes the round out.
		level.zombie_total = 0;
		println( "CoDOL zm: round " + level.round_number + " has no spawners; nothing to spawn\n" );
		return;
	}

	max = level.zombie_vars[ "zombie_max_ai" ];

	multiplier = level.round_number / 5;
	if ( multiplier < 1 )
		multiplier = 1;

	if ( level.round_number >= 10 )
		multiplier *= level.round_number * 0.15;

	player_num = getPlayers().size;

	if ( player_num == 1 )
		max += int( ( 0.5 * level.zombie_vars[ "zombie_ai_per_player" ] ) * multiplier );
	else
		max += int( ( ( player_num - 1 ) * level.zombie_vars[ "zombie_ai_per_player" ] ) * multiplier );

	if ( !isDefined( level.max_zombie_func ) )
		level.max_zombie_func = ::default_max_zombie_func;

	level.zombie_total = [[ level.max_zombie_func ]]( max );

	println( "CoDOL zm: round " + level.round_number + " budget " + level.zombie_total +
		" zombies at " + level.zombie_health + " health\n" );

	old_spawn = undefined;

	for ( ;; )
	{
		while ( get_enemy_count() >= level.zombie_ai_limit || level.zombie_total <= 0 )
			wait( 0.1 );

		if ( !flag( "spawn_zombies" ) )
			flag_wait( "spawn_zombies" );

		spawn_point = level.enemy_spawns[ randomInt( level.enemy_spawns.size ) ];

		// T5's "don't use the same window twice in a row" reroll.
		if ( isDefined( old_spawn ) && spawn_point == old_spawn )
			spawn_point = level.enemy_spawns[ randomInt( level.enemy_spawns.size ) ];
		old_spawn = spawn_point;

		ai = spawn_zombie( spawn_point );
		if ( isDefined( ai ) )
		{
			level.zombie_total--;
			ai thread round_spawn_failsafe();
		}

		wait( level.zombie_vars[ "zombie_spawn_delay" ] );
		wait_network_frame();
	}
}

// --------------------------------------------------------------------------
// The actor.  T5 _zombiemode_spawner.gsc:142 zombie_spawn_init, data half.
// --------------------------------------------------------------------------

spawn_zombie( spawn_point )
{
	if ( !isDefined( level.zombie_spawner ) )
		return undefined;

	ai = level.zombie_spawner spawnActor();
	if ( !isDefined( ai ) )
		return undefined;

	origin = spawn_point.origin;
	angles = spawn_point.angles;
	if ( !isDefined( angles ) )
		angles = ( 0, randomInt( 360 ), 0 );

	ai forceTeleport( origin, angles );
	ai show();

	ai.targetname = "zombie";
	ai.animname = "zombie";
	ai.is_zombie = true;

	ai.maxhealth = level.zombie_health;
	ai.health = level.zombie_health;

	// T5 :142.  The fields that exist as native class fields here; the rest of
	// T5's list addresses the animscript state machine and is left out.
	ai.ignoreSuppression = true;
	ai.suppressionThreshold = 1;
	ai.noDodgeMove = true;
	ai.disableArrivals = true;
	ai.disableExits = true;
	ai.grenadeawareness = 0;
	ai.badplaceawareness = 0;
	ai.dropweapon = false;
	ai.allowdeath = true;

	// T5 sets the walk/run/sprint cycle from level.zombie_move_speed through
	// set_zombie_run_cycle(), which picks an ANIMATION.  The scalar half of
	// that -- how fast the actor actually travels -- is setMoveSpeedScale,
	// which is registered and is not an animation call.
	scale = 0.4 + ( level.zombie_move_speed * 0.01 );
	if ( scale > 1.4 )
		scale = 1.4;
	ai setMoveSpeedScale( scale );

	level.zombies[ level.zombies.size ] = ai;

	level thread zombie_death_event( ai );
	ai thread zombie_think();
	ai thread zombie_damage_watch();

	return ai;
}

// T5 _zombiemode_spawner.gsc:3518 zombie_damage(), scoring half.
//
// T5 reaches this from level.callbackActorDamage.  This server has no actor
// damage callback in GSC -- CODOL_ActorPain/CODOL_ActorDie are native
// (src/codol/player/codol_actor_lifecycle.cpp:1551-1569) and the comment
// there records that "Generic G_Damage already sent death", i.e. the ordinary
// "damage" and "death" notifies do reach script on an actor.  So the award is
// driven off the notify instead, with the same two events T5 pays for:
// "damage" (or "damage_ads" while the attacker is aiming down sights) per hit,
// and "death" for the kill, with T5's hit-location bonus.
zombie_damage_watch()
{
	self endon( "death" );

	for ( ;; )
	{
		self waittill( "damage", amount, attacker, direction, point, mod, modelName, tagName, partName, flags, weapon );

		if ( !isDefined( attacker ) || !isPlayer( attacker ) )
			continue;

		self.attacker = attacker;
		self.damagemod = mod;
		self.damagelocation = partName;

		if ( self.health <= 0 )
			continue;

		// T5 pays damage_ads at 1.25x for a hit landed while aiming.
		if ( attacker playerADS() > 0.9 )
			attacker maps\mp\gametypes\_zombiemode_score::player_add_points( "damage_ads", mod, partName );
		else
			attacker maps\mp\gametypes\_zombiemode_score::player_add_points( "damage", mod, partName );
	}
}

// T5 _zombiemode_spawner.gsc:3922 find_flesh, reduced to the goal it sets.
// The animscript half of T5's zombie_think (:415) is the other workstream's.
zombie_think()
{
	self endon( "death" );
	self endon( "zombie_delete" );

	for ( ;; )
	{
		player = closest_valid_player( self.origin );

		if ( isDefined( player ) )
			self setGoalPos( player.origin );

		wait( 1 );
	}
}

closest_valid_player( origin )
{
	players = getPlayers();

	best = undefined;
	best_dist = 0;

	for ( i = 0; i < players.size; i++ )
	{
		if ( !is_player_valid( players[ i ] ) )
			continue;

		d = distanceSquared( origin, players[ i ].origin );
		if ( !isDefined( best ) || d < best_dist )
		{
			best = players[ i ];
			best_dist = d;
		}
	}

	return best;
}

// T5 _zombiemode_spawner.gsc:3795, kill accounting and the kill award.
zombie_death_event( zombie )
{
	zombie waittill( "death", attacker );

	level.total_zombies_killed++;

	if ( !isDefined( attacker ) && isDefined( zombie ) )
		attacker = zombie.attacker;

	mod = undefined;
	hit_location = undefined;
	if ( isDefined( zombie ) )
	{
		mod = zombie.damagemod;
		hit_location = zombie.damagelocation;
	}

	if ( isDefined( attacker ) && isPlayer( attacker ) )
	{
		attacker maps\mp\gametypes\_zombiemode_score::player_add_points( "death", mod, hit_location );

		if ( !isDefined( attacker.kills ) )
			attacker.kills = 0;
		attacker.kills++;

		attacker notify( "zom_kill", zombie );
	}

	zombie_list_compact();

	level notify( "zom_kill" );
}

// T5 _zombiemode.gsc:4059.  Fell out of the world, or has not moved 24 inches
// in 30 seconds -> kill it and hand its slot back to the round budget.
//
// T5 treats this as a hack for a stuck pathfinder.  Here it is load-bearing:
// the actors move but do not animate and cannot melee, so a zombie that ends
// up somewhere a player never goes is the normal case, not the rare one, and
// this is what keeps a round finite.
round_spawn_failsafe()
{
	self endon( "death" );

	prevorigin = self.origin;

	for ( ;; )
	{
		if ( !is_true( level.zombie_vars[ "zombie_use_failsafe" ] ) )
			return;

		wait( 30 );

		if ( self.origin[ 2 ] < level.zombie_vars[ "below_world_check" ] )
		{
			self doDamage( self.health + 100, ( 0, 0, 0 ) );
			return;
		}

		if ( distanceSquared( self.origin, prevorigin ) < 576 )
		{
			self doDamage( self.health + 100, ( 0, 0, 0 ) );
			return;
		}

		prevorigin = self.origin;
	}
}

// T5 _zombiemode.gsc:3294 zombie_speed_up, scalar half.  T5 also swaps the
// run cycle animation, which is left out.
zombie_speed_up()
{
	level endon( "end_of_round" );

	if ( level.round_number <= 3 )
		return;

	for ( ;; )
	{
		wait( 5 );

		if ( get_enemy_count() > 3 || level.zombie_total > 0 )
			continue;

		for ( i = 0; i < level.zombies.size; i++ )
		{
			if ( !isDefined( level.zombies[ i ] ) )
				continue;
			level.zombies[ i ] setMoveSpeedScale( 1.4 );
		}
		return;
	}
}

// Called by the round loop between rounds so a leftover actor cannot survive
// into the next round's accounting.  T5 has no equivalent because its rounds
// cannot end with a zombie alive.
clear_zombies()
{
	for ( i = 0; i < level.zombies.size; i++ )
	{
		if ( !isDefined( level.zombies[ i ] ) )
			continue;
		level.zombies[ i ] doDamage( level.zombies[ i ].health + 100, ( 0, 0, 0 ) );
	}

	zombie_list_compact();
}
