// zm spawner: the round's spawn budget, and the actors that consume it.
//
// Port of the BUDGET half of T5's maps\_zombiemode_spawner.gsc (5,030 lines)
// plus the spawn loop from maps\_zombiemode.gsc:3103 round_spawning().
//
// ============================== SCOPE ======================================
//
// 2026-09-20 UPDATE.  The first pass of this file left every animation path
// out, because this server registered no animation builtin and naming one in
// GSC is a COMPILE error that kills the match at map load.  Commit d4548385
// registered twenty of them and MEASURED that the %anim reference system and
// the notetrack producer were already complete
// (docs/CODOL_SCRIPTED_ANIMATION_20260920.md).  The deferred list below is
// now RESTORED:
//
//   zombie_setup_attack_properties  T5 :3899   -- live
//   the animscript handoff in zombie_think  T5 :415 -- live
//   set_zombie_run_cycle / set_run_speed / set_run_anim  T5 :3690 -- live
//   zombie_eye_glow / zombie_eye_glow_stop / delayed_zombie_eye_glow
//                                   T5 :3657   -- live, FX form, see below
//   zombie_death_animscript         T5 :3840   -- live
//   do_zombies_playvocals / play_ambient_zombie_vocals
//                                   T5 _zombiemode_audio -- live
//   make_crawler                    T5 :4416   -- live
//
//   zombie_gib_on_damage / zombie_head_gib (T5 :2819, :2605) -- live since
//     2026-10-05 in _zombiemode_gib.gsc.  The 17 gib models per family were
//     bone-dumped from a zm probe (codol_checkmodel): the torso models keep
//     the full 73-bone skeleton, so T5's J_Elbow_* / J_Knee_* spawn tags
//     resolve on them; character_zm_zombie::set_gib_models maps each CoDOL
//     suffix to its T5 field.
//
// STILL OUT, with the reason, not as a silent gap:
//   every maps\_zombiemode_ai_* variant (dogs, apes, monkeys, thieves,
//     napalm, quads, astro, sonic, faller) -- each is a species of its own.
//
// MELEE IS NO LONGER A HOLE.  maps\mpnimscripts\zombie_combat.gsc plays
// T5's attack clips and lands the damage on their "fire" notetrack through
// the registered melee() method, which is T5's own mechanism.  The only
// thing this file owes it is self.aiweapon, resolved in init() below.
//
// TAKEN, by T5 function:
//   round_spawning              _zombiemode.gsc:3103  -- the whole budget:
//                               ai_calculate_health, the per-player multiplier,
//                               the >=10 exponential, max_zombie_func, the
//                               zombie_ai_limit gate, the spawn_delay pacing
//                               and the level.zombie_total decrement.
//   default_max_zombie_func     _zombiemode.gsc:3075  -- the round 1-6 ramp.
//   zombie_speed_up             _zombiemode.gsc:3294
//   round_spawn_failsafe        _zombiemode.gsc:4059  -- the stuck/fell-out-of
//                               -world killer.
//   zombie_spawn_init           _zombiemode_spawner.gsc:142
//   zombie_death_event          _zombiemode_spawner.gsc:3795
//   find_flesh                  _zombiemode_spawner.gsc:3922
//

#include maps\_zombiemode_utility;

#using_animtree ("generic_human");

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

	level.zombie_melee_weapon = resolve_melee_weapon();
	level._effect[ "zombie_eye_glow" ] = loadfx( "maps/zombie/zm_eye_lensflare_blue" );

	// Dismemberment and the blue blood (_zombiemode_gib.gsc).
	// C1 common_gametype_zm: one-shot gfx_double_points impact.
	level._effect[ "double_points_impact" ] = loadfx( "impacts/double_points" );

	maps\_zombiemode_gib::init();

	maps\_zombiemode_kino_risers::init();

	init_spawn_points();
	init_spawner();

	// Developer-only entrance-walk soak (scr_codol_pathtest); returns at once
	// on a production server.
	level thread maps\_zombiemode_pathtest::init();

	// Developer-only actor telemetry for the T5 comparison (t5cap_scn).
	level thread maps\_zombiemode_t5cap::init();
}

// The aiweapon every zombie needs before melee() will do anything.
//
// MEASURED, docs/zone_assets/ALL_weapon.txt: "zombie_melee" ships in
// common_gametype_zm -- T5's own zombie claw WeaponDef name, carried into
// CoDOL unchanged.  "dog_bite_zm" (the three zm MAP zones) is the fallback
// because it is the only other melee-shaped WeaponDef a zm match mounts.
//
// Validated the same way _zombiemode_weapons::add_zombie_weapon and
// _zombiemode::resolveStartWeapon validate theirs: precache, then ask the
// engine whether the name actually resolved.
resolve_melee_weapon()
{
	candidates = [];
	candidates[ 0 ] = "zombie_melee";
	candidates[ 1 ] = "dog_bite_zm";

	for ( i = 0; i < candidates.size; i++ )
	{
		precacheItem( candidates[ i ] );
		if ( getWeaponIndexFromName( candidates[ i ] ) > 0 )
		{
			{}
			return candidates[ i ];
		}
	}

	{}
	return undefined;
}

// T5 keys the spawn list off the active zone (maps\_zombiemode_zone_manager).
init_spawn_points()
{
	maps\_zombiemode_zone_manager::init();

	{}
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
			{}
			level.no_zombies = true;
			return;
		}

		level.zombie_spawner.targetname = "zombie_spawner";
	}

	{}
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
		{
			// Saturate before converting or adding: late Endless rounds must
			// never overflow a signed native health field into a dead actor.
			next_health = level.zombie_health * ( 1.0 + level.zombie_vars[ "zombie_health_increase_multiplier" ] );
			if ( next_health >= 2000000000 )
			{
				level.zombie_health = 2000000000;
				return;
			}
			level.zombie_health += int( level.zombie_health * level.zombie_vars[ "zombie_health_increase_multiplier" ] );
		}
		else
			level.zombie_health = int( level.zombie_health + level.zombie_vars[ "zombie_health_increase" ] );
	}
}

// Preserve T5's budget curve and map override. Only numeric saturation is
// new: the horde still uses the existing simultaneous actor limit, and rounds
// do not stop at this representational bound.
round_budget( player_num )
{
	max = level.zombie_vars[ "zombie_max_ai" ];
	multiplier = level.round_number / 5;
	if ( multiplier < 1 ) multiplier = 1;
	if ( level.round_number >= 10 )
		multiplier *= level.round_number * 0.15;
	if ( player_num == 1 )
		extra = ( 0.5 * level.zombie_vars[ "zombie_ai_per_player" ] ) * multiplier;
	else
		extra = ( ( player_num - 1 ) * level.zombie_vars[ "zombie_ai_per_player" ] ) * multiplier;
	if ( extra >= 2000000000 - max )
		max = 2000000000;
	else
		max += int( extra );
	if ( !isDefined( level.max_zombie_func ) )
		level.max_zombie_func = ::default_max_zombie_func;
	return [[ level.max_zombie_func ]]( max );
}

// T5 _zombiemode.gsc:3103, with the dog-mixing block and the zone occupancy
// walk left out (no dogs species and no zone manager in this port).
round_spawning()
{
	level endon( "intermission" );
	level endon( "end_game" );
	level endon( "zm_stop_spawning" );
	level endon( "end_of_round" );
	level endon( "restart_round" );

	if ( spawning_closed() )
		return;

	ai_calculate_health( level.round_number );

	// The split-start Asylum map has no single hardcoded initial room.  The
	// player_volume may not have been polled yet when round one begins.
	for ( zone_wait = 0; zone_wait < 100 && level.enemy_spawns.size < 1; zone_wait++ )
	{
		maps\_zombiemode_zone_manager::refresh_spawners();
		if ( level.enemy_spawns.size > 0 )
			break;
		wait( 0.1 );
	}

	if ( is_true( level.no_zombies ) || level.enemy_spawns.size < 1 )
	{
		// T5 ASSERTMSGs here ("No active spawners in the map").  An assert
		// would end the match; the round budget is still published so the HUD
		// and the round number are honest about what round it is, and
		// round_wait's own end_round_wait escape closes the round out.
		level.zombie_total = 0;
		{}
		return;
	}

	level.zombie_total = round_budget( getPlayers().size );
	level.zm_round_budget = level.zombie_total;
	level.zm_round_spawned = 0;

	for ( ;; )
	{
		while ( get_enemy_count() >= level.zombie_ai_limit || level.zombie_total <= 0 )
			wait( 0.1 );

		if ( !flag( "spawn_zombies" ) )
			flag_wait( "spawn_zombies" );
		if ( spawning_closed() )
			return;

		// Final hounds on every difficulty share this admission, cap and budget with
		// the humanoid horde. Failed placement falls through to a normal zombie.
		if ( maps\_zombiemode_ai_dogs::final_round_dog_due() )
		{
			dog = maps\_zombiemode_ai_dogs::spawn_final_round_dog();
			if ( isDefined( dog ) )
			{
				level.zombie_total--;
				level.zm_round_spawned++;
				wait( level.zombie_vars[ "zombie_spawn_delay" ] );
				wait_network_frame();
				continue;
			}
		}

		spawn_point = select_spawn_point();
		if ( !isDefined( spawn_point ) )
		{
			// All active entrances have a queue or are cooling down. Keep the
			// unspawned budget; a slot opens as soon as an actor enters/dies.
			wait( 0.1 );
			continue;
		}

		ai = spawn_zombie( spawn_point );
		if ( isDefined( ai ) )
		{
			entrance = spawn_entrance_key( spawn_point );
			entrance.zm_last_spawn_time = getTime();
			entrance.zm_spawn_round = level.round_number;
			entrance.zm_round_spawn_count++;
			level.zombie_total--;
			level.zm_round_spawned++;
			ai thread round_spawn_failsafe();
			// One line per round, not per zombie: enough to prove the chain
			// reached a live actor and where it was put.
			if ( !isDefined( level.zm_spawn_reported ) || level.zm_spawn_reported != level.round_number )
			{
				level.zm_spawn_reported = level.round_number;
				{}
			}
		}
		else if ( !isDefined( level.zm_spawn_fail_reported ) || level.zm_spawn_fail_reported != level.round_number )
		{
			level.zm_spawn_fail_reported = level.round_number;
			{}
		}

		wait( level.zombie_vars[ "zombie_spawn_delay" ] );
		wait_network_frame();
	}
}

// Close admission before terminal cleanup frees actor slots. Extraction's
// boarding window has already closed at lift-off; its ride/music continues.
stop_spawning()
{
	level.zm_spawning_closed = true;
	level.zombie_total = 0;
	level notify( "zm_stop_spawning" );
}

spawning_closed()
{
	return is_true( level.intermission ) || is_true( level.gameEnded ) ||
		is_true( level.zm_spawning_closed );
}

// Select entrances, rather than weighting a window by how many authored
// spawner structs target it. Only the current zone manager's pool is eligible.
// Counts belong to the persistent entrance structs so zone refreshes cannot
// reset the queue/cooldown. Untargeted interior spawners use their own key.
spawn_entrance_key( spawn_point )
{
	window = spawner_window( spawn_point );
	if ( isDefined( window ) )
		return window;
	return spawn_point;
}

select_spawn_point()
{
	groups = [];
	for ( i = 0; i < level.enemy_spawns.size; i++ )
	{
		sp = level.enemy_spawns[ i ];
		key = spawn_entrance_key( sp );
		found = undefined;
		for ( j = 0; j < groups.size; j++ )
		{
			if ( groups[ j ].key == key )
			{
				found = groups[ j ];
				break;
			}
		}
		if ( !isDefined( found ) )
		{
			found = spawnStruct();
			found.key = key;
			found.spawners = [];
			groups[ groups.size ] = found;
		}
		found.spawners[ found.spawners.size ] = sp;
	}

	cooldown = 5000 - level.zm_rule_tier * 1000;
	if ( cooldown < 3000 )
		cooldown = 3000;
	choices = [];
	best_count = 999999;
	for ( i = 0; i < groups.size; i++ )
	{
		key = groups[ i ].key;
		if ( !isDefined( key.zm_spawn_round ) || key.zm_spawn_round != level.round_number )
		{
			key.zm_spawn_round = level.round_number;
			key.zm_round_spawn_count = 0;
		}
		if ( isDefined( key.zm_last_spawn_time ) && getTime() - key.zm_last_spawn_time < cooldown )
			continue;
		pending = 0;
		for ( j = 0; j < level.zombies.size; j++ )
		{
			ai = level.zombies[ j ];
			if ( !isDefined( ai ) || !isAlive( ai ) || is_true( ai.zm_entered ) ||
				( isDefined( ai.zm_path_phase ) && ai.zm_path_phase == "gave_up" ) )
				continue;
			assigned = ai.zm_spawn_entrance;
			if ( isDefined( ai.first_node ) )
				assigned = ai.first_node;
			if ( isDefined( assigned ) && assigned == key )
				pending++;
		}
		if ( pending >= 3 )
			continue;
		if ( key.zm_round_spawn_count < best_count )
		{
			best_count = key.zm_round_spawn_count;
			choices = [];
		}
		if ( key.zm_round_spawn_count == best_count )
			choices[ choices.size ] = groups[ i ];
	}
	if ( choices.size < 1 )
		return undefined;
	group = choices[ randomInt( choices.size ) ];
	return group.spawners[ randomInt( group.spawners.size ) ];
}

// --------------------------------------------------------------------------
// The actor.  T5 _zombiemode_spawner.gsc:142 zombie_spawn_init, data half.
// --------------------------------------------------------------------------

spawn_zombie( spawn_point )
{
	level.zm_spawn_fail_reason = "unknown";
	if ( spawning_closed() )
	{
		level.zm_spawn_fail_reason = "match spawn admission closed";
		return undefined;
	}

	if ( !isDefined( level.zombie_spawner ) )
	{
		level.zm_spawn_fail_reason = "no zombie spawner (init_spawner produced none)";
		return undefined;
	}

	// T5 rise selection happens before actor admission and round accounting.
	kino_riser = maps\_zombiemode_kino_risers::is_riser( spawn_point );
	rise_context = undefined;
	if ( kino_riser )
	{
		rise_context = maps\_zombiemode_kino_risers::prepare( spawn_point );
		if ( !isDefined( rise_context ) )
		{
			level.zm_spawn_fail_reason = "Kino riser has no active owned floor-valid rise location";
			return undefined;
		}
	}

	ai = level.zombie_spawner spawnActor();
	if ( !isDefined( ai ) )
	{
		// The native actor layer prints its own "CoDOL actor:" stage warning.
		level.zm_spawn_fail_reason = "spawnactor returned nothing (actor budget, aitype or animscripts)";
		return undefined;
	}

	origin = spawn_point.origin;
	angles = spawn_point.angles;
	if ( !isDefined( angles ) )
		angles = ( 0, randomInt( 360 ), 0 );

	// Near-window spawn (see near_window_spawn below): the zombie keeps its
	// authored spawner, and therefore its window, but starts a few seconds'
	// walk outside that window instead of 700-2400 units away.
	ai.zm_spawn_time = getTime();
	ai.zm_spawn_near = false;
	near = undefined;
	if ( !kino_riser )
		near = near_window_spawn( spawn_point );
	if ( isDefined( near ) )
	{
		origin = near.origin;
		angles = near.angles;
		ai.zm_spawn_near = true;
	}

	if ( kino_riser )
		ai hide();
	else
	{
		ai forceTeleport( origin, angles );
		ai show();
	}

	ai.targetname = "zombie";
	ai.animname = "zombie";
	// T5 spawns the zombie from a spawner entity and the AI inherits its
	// .target; here the spawner is a struct, so the AI keeps the struct and
	// pick_entrance_node reads the target from it (T5 zombie_think :461).
	ai.zm_spawn_point = spawn_point;
	if ( kino_riser )
		ai.script_string = spawn_point.script_string;
	ai.zm_spawn_entrance = spawn_entrance_key( spawn_point );
	ai.zm_entered = false;
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

	// T5 :244.  What one hit is worth to a player; applied by
	// _zombiemode::Callback_PlayerDamage, which overrides the native melee
	// damage the same way T5's player_damage_override does.
	ai.meleeDamage = 60;

	// Melee.  Without a real aiweapon the melee() method raises a script
	// error instead of damaging anyone -- see zombie_combat.gsc.
	if ( isDefined( level.zombie_melee_weapon ) )
		ai.aiweapon = level.zombie_melee_weapon;

	// T5 :159-:160.  A freshly spawned zombie ignores every sentient until it
	// has walked to its entrance and torn the boards; zombie_think hands it
	// zombie_setup_attack_properties (ignoreall = false) only then, exactly
	// where T5 zombie_goto_entrance :620 does.  Calling that at spawn, as
	// this file did until 2026-09-21, let the native chase override the
	// entrance goal with the player's position on the very next frame
	// (docs/CODOL_ZOMBIE_ROUTE_20260921.md).
	ai.ignoreall = true;
	ai.ignoreme = true;

	// T5 :3690 set_zombie_run_cycle picks BOTH halves: the animation and, via
	// set_run_speed, the walk/run/sprint bucket.  The scalar half -- how fast
	// the actor actually travels -- is apply_move_speed_scale(), which derives
	// it from the bucket the cycle chose rather than from the raw round speed,
	// so the clip and the translation agree.
	ai set_zombie_run_cycle();
	ai apply_move_speed_scale();
	ai.needs_run_update = false;

	// T5 :3840.  zombie_death.gsc runs this after the death clip.
	ai.deathanimscript = ::zombie_death_animscript;

	// The bootstrap pose.  See the note in zombie_init.gsc: the animscript's
	// own main() runs too early in CODOL_ActorScriptsInitialize to animate,
	// so the first pose is set here, where the actor is fully initialised.
	// Use the mounted complete zombie pose; ai_zombie_idle_v1 is additive and
	// cannot pose the model alone before the first movement cycle.
	ai setAnimKnobAll( %zombie_stationary_idle_v7, %body, 1, 0.1, 1 );

	if ( kino_riser && !ai maps\_zombiemode_kino_risers::begin( rise_context ) )
	{
		level.zm_spawn_fail_reason = "Kino riser capsule/alignment/clip validation failed";
		ai delete();
		return undefined;
	}

	level.zombies[ level.zombies.size ] = ai;

	ai play_sound_on_ent( "zombie_spawn" );
	if ( kino_riser )
		ai thread delayed_zombie_eye_glow( 0.5 );
	else
		ai thread zombie_eye_glow();
	ai thread play_ambient_zombie_vocals();

	level thread zombie_death_event( ai );
	ai thread zombie_think();
	ai thread zombie_damage_watch();
	ai thread maps\_zombiemode_gib::zombie_gib_on_damage();

	return ai;
}

// T5 _zombiemode_spawner.gsc:3899 zombie_setup_attack_properties, restored.
// T5's own comment: "this is where zombies go into attack mode, and need
// different attributes set up".
//
// NOT TAKEN from it: PushPlayer( true ) -- T5 itself has the call commented
// out -- and self.maxsightdistsqrd, which is not a field this server's actor
// class declares (Scr_AddClassField, codol_actor_lifecycle.cpp:190-199).
zombie_setup_attack_properties()
{
	self.ignoreall = false;

	self.pathEnemyFightDist = 64;
	self.meleeAttackDist = 64;

	self.disableArrivals = true;
	self.disableExits = true;
}

// T5 :3690 set_zombie_run_cycle.
set_zombie_run_cycle( new_move_speed )
{
	if ( isDefined( new_move_speed ) )
		self.zombie_move_speed = new_move_speed;
	else
	{
		self set_run_speed();
		self.zombie_move_speed_original = self.zombie_move_speed;
	}

	self.needs_run_update = true;

	// T5 :3706.  self.deathanim is what zombie_death.gsc plays.
	if ( is_true( self.has_legs ) )
		deaths = level._zombie_deaths[ self.animname ];
	else
		deaths = level._zombie_crawl_deaths[ self.animname ];

	if ( isDefined( deaths ) && deaths.size > 0 )
		self.deathanim = deaths[ randomInt( deaths.size ) ];

	if ( !is_true( self.has_legs ) )
	{
		self set_run_anim( "crawl" + randomIntRange( 1, 7 ) );
		self.crawl_anim = self.run_noncombatanim;
		return;
	}

	switch ( self.zombie_move_speed )
	{
	case "walk":
		self set_run_anim( "walk" + randomIntRange( 1, 9 ) );
		break;
	case "run":
		// run1..3 are fast-walk clips (65..81 units/s); run4..6 are
		// actual runs (100..120). Use those once the early grace ends.
		first_run = 1;
		if ( isDefined( level.round_number ) && level.round_number >= 5 )
			first_run = 4;
		self set_run_anim( "run" + randomIntRange( first_run, 7 ) );
		break;
	case "sprint":
		self set_run_anim( "sprint" + randomIntRange( 1, 5 ) );
		break;
	}
}

// T5 maps\_utility.gsc:7551 set_run_anim, minus the assertEx trio and the
// weapon-relative alwaysRunForward flag (no zm actor carries a weapon pose).
set_run_anim( anime )
{
	if ( !isDefined( self.animname ) )
		return;

	animRef = level.scr_anim[ self.animname ][ anime ];
	if ( !isDefined( animRef ) )
		return;

	self.a.combatrunanim = animRef;
	self.run_combatanim = animRef;
	self.run_noncombatanim = animRef;
	self.walk_combatanim = animRef;
	self.walk_noncombatanim = animRef;
}

// T5 :3671 set_run_speed.  level.zombie_move_speed is the round's 0..100 dial
// that round_think raises; this buckets it into the three cycles.
set_run_speed()
{
	// A later hunt update must not re-roll the final survivor back to walking.
	if ( is_true( self.zm_final_sprinter ) && is_true( self.has_legs ) )
	{
		self.zombie_move_speed = "sprint";
		return;
	}

	// Shared early grace: rounds 1..5 choose runs at 0/25/50/75/100%.
	// This changes clip selection, not playback speed or difficulty health.
	if ( isDefined( level.round_number ) && level.round_number <= 5 )
	{
		run_chance = ( level.round_number - 1 ) * 25;
		if ( randomInt( 100 ) < run_chance )
			self.zombie_move_speed = "run";
		else
			self.zombie_move_speed = "walk";
		return;
	}

	// Keep the later capped walk/run curve and final-survivor-only sprint.
	speed = level.zombie_move_speed;
	if ( speed > 35 )
		speed = 35;
	rand = randomIntRange( speed, speed + 35 );

	if ( rand <= 35 )
		self.zombie_move_speed = "walk";
	else if ( rand <= 70 )
		self.zombie_move_speed = "run";
	else
		self.zombie_move_speed = "sprint";
}

// The translation that matches the cycle the actor is playing.  T5 gets this
// for free because its AI moves at the animation's own root-motion speed;
// this server's mover is scalar, so the bucket picks the scalar.
// MEASURED (qconsole.log, 6 per spawn wave): `ai setMoveSpeedScale(...)` threw
// "only valid on players; called on entity N ... classname
// actor_enemy_zombie_zm".  setmovespeedscale IS registered
// (src/scr_vm_main.c:867) but its handler calls GetPlayerEntity first and
// writes client->sess.moveSpeedScaleMultiplier, so it rejects any entity
// without a client.  There is no actor speed-scale builtin on this server.
//
// The lever that DOES move an actor here is the animation playback rate:
// actor translation is pure root motion (CODOL_ActorAdvanceGoalAnimation ->
// XAnimCalcDelta, no speed multiplier anywhere in the actor sources), and
// `animplaybackrate` is a registered actor class field
// (src/codol/player/codol_actor_lifecycle.cpp:194).  The zm tree already
// drives the move cycle off self.moveplaybackrate
// (animscripts/zombie_move.gsc:105,:122), so scaling that is the
// faithful equivalent of T5's call.
//
// 0.9 is the "run" value of move_speed_scale() and the centre of the
// per-zombie jitter zombie_init.gsc:137 assigns, so it is the normalisation
// point: a "run" zombie keeps exactly the rate it was given.
apply_move_speed_scale( scale )
{
	if ( !isDefined( scale ) )
		scale = move_speed_scale();

	base = 1;
	if ( isDefined( self.animplaybackrate ) )
		base = self.animplaybackrate;

	self.moveplaybackrate = base * ( scale / 0.9 );
}

move_speed_scale()
{
	if ( !isDefined( self.zombie_move_speed ) )
		return walk_speed_scale();

	switch ( self.zombie_move_speed )
	{
	case "sprint":
		return 1.4;
	case "run":
		return 0.9;
	}

	return walk_speed_scale();
}

// The walk bucket.  T5 plays every zombie move cycle at moveplaybackrate 1
// (animscripts\zombie_init.gsc:320) and never scales a zombie's movement
// (no SetMoveSpeedScale on a zombie anywhere in T5's zombiemode), so a
// walker covers its walk clip's own root motion.  This port played walks at
// 0.5 / 0.9 = 0.56 of that: MEASURED on the VPS, every walker moved 19-20
// units/s against 34-36 at rate 1, so the round-1..3 walkers took 60-120 s
// from Prototype's yard spawners to their windows.  0.9 here is rate 1, the
// same normalisation point as "run".  scr_zm_t5_walk_rate 0 restores 0.5.
walk_speed_scale()
{
	if ( zm_dvar_int( "scr_zm_t5_walk_rate", 1 ) <= 0 )
		return 0.5;
	return 0.9;
}

zm_dvar_int( name, default_value )
{
	value = getDvar( name );
	if ( value == "" )
		return default_value;
	return int( value );
}

// --------------------------------------------------------------------------
// Near-window spawning.
//
// MEASURED (VPS codol-match-17733, 2026-10-07, 152 entrance walks on
// zm_prototype_sh): every authored zombie_spawner targets the node inside
// its own window, as in T5, but CoDOL placed the structs out at the edge
// of the expanded map, 666-2397 units (median 1165) in a straight line from
// that window -- (1885 -1502) feeds the window at (273 247), (135 2388) the
// one at (238 1176).  zm_asylum_sh's are mostly 160-300 units out (median
// 241), with a few at 800-1460.  T5 spawns from these same structs and
// keeps them within a few seconds of the boards.
//
// So the spawner still picks the window (the zone manager's active-zone
// choice, T5's random spawner, the spawner's target -> closest
// exterior_goal, all unchanged), but unless the authored struct already
// stands within scr_zm_spawn_near_min (160) of that window the zombie
// starts at one of the window's own approach points instead: a map path
// node outside the window (on the far side of the window from its inside
// traversal point, in no player volume), between scr_zm_spawn_near_min and
// scr_zm_spawn_near_max (320; 480 when that band holds fewer than three) of
// the exterior_goal, with clear line of sight to it at knee and chest height,
// a floor under it and a findpath() route to it.  A point some player can
// see is skipped (no zombie appears in view), as is one another zombie is
// standing on; when none is left the authored spawner is used, which is
// the old behaviour.  scr_zm_spawn_near_window 0 restores it everywhere.
//
// OFF BY DEFAULT since 2026-10-21 (docs/CODOL_ZOMBIE_T5_MEASURED.md): the
// premise above is wrong.  MEASURED in real T5 (Plutonium t5cap,
// zombie_cod5_prototype round 1): T5 spawns at the same far structs --
// (1808 -1398), (-1324 -1056), (1971 -1155), (-1393 -62) -- and its walkers
// take 29-49 s (median 37.6) from spawn to the first board, 53 s to get
// inside.  Near-window spawning cut that to 8 s / 25 s.  With it off CoDOL
// measures 44 s / 52 s and every zombie reaches its window.
// scr_zm_spawn_near_window 1 turns it back on.
// --------------------------------------------------------------------------

near_window_spawn( spawn_point )
{
	if ( zm_dvar_int( "scr_zm_spawn_near_window", 0 ) <= 0 )
		return undefined;
	if ( !isDefined( spawn_point ) || !isDefined( spawn_point.script_noteworthy ) ||
		spawn_point.script_noteworthy != "zombie_spawner" )
		return undefined;

	window = spawner_window( spawn_point );
	if ( !isDefined( window ) )
		return undefined;

	// An authored spawner already at the boards (Asylum's (-178 413) is 57
	// units from its window) is T5's own near-window spawner; keep it.  One
	// farther out is replaced even when its straight-line distance is short:
	// MEASURED, Asylum's (902 566) is 272 units from the window (705 754)
	// but its route takes 23-27 s; an approach point sees its window.
	if ( distance( spawn_point.origin, window.origin ) <= zm_dvar_int( "scr_zm_spawn_near_min", 160 ) )
		return undefined;

	spots = window_spawn_spots( window );
	if ( spots.size < 1 )
		return undefined;

	// Random start, then the first spot nobody can see and nobody stands on.
	first = randomInt( spots.size );
	for ( i = 0; i < spots.size; i++ )
	{
		spot = spots[ ( first + i ) % spots.size ];
		if ( spot_occupied( spot.origin ) )
			continue;
		if ( spot_seen_by_player( spot.origin ) )
		{
			if ( zm_dvar_int( "scr_zm_spawn_log", 0 ) > 0 )
				{}
			continue;
		}
		return spot;
	}

	if ( !isDefined( level.zm_near_all_seen_reported ) || level.zm_near_all_seen_reported != level.round_number )
	{
		level.zm_near_all_seen_reported = level.round_number;
		{}
	}
	return undefined;
}

// The window a zombie from this spawner attacks: pick_entrance_node's rule
// for a targeted spawner (T5 zombie_think :461, the exterior_goal closest to
// the spawner's target).  An untargeted spawner keeps its own position: T5
// sends those to the closest of three windows from where they stand.
spawner_window( spawn_point )
{
	if ( isDefined( spawn_point.zm_window ) )
		return spawn_point.zm_window;
	if ( !isDefined( level.exterior_goals ) || level.exterior_goals.size < 1 )
		return undefined;
	if ( !isDefined( spawn_point.target ) || spawn_point.target == "" )
		return undefined;

	desired = undefined;
	ent = getEnt( spawn_point.target, "targetname" );
	if ( isDefined( ent ) )
		desired = ent.origin;
	if ( !isDefined( desired ) )
	{
		s = get_struct( spawn_point.target, "targetname" );
		if ( isDefined( s ) )
			desired = s.origin;
	}
	if ( !isDefined( desired ) )
	{
		node = getNode( spawn_point.target, "targetname" );
		if ( isDefined( node ) )
			desired = node.origin;
	}
	if ( !isDefined( desired ) )
		return undefined;

	spawn_point.zm_window = get_closest( desired, level.exterior_goals );
	return spawn_point.zm_window;
}

// The approach points of one window, built once per window.  Nodes between
// min and max (within 96 units of the window's height) come first; when
// that band yields fewer than three (Asylum's upper-floor windows open onto
// narrow ledges), the band is widened by half, and then ground up to 320
// units below the window is allowed.
window_spawn_spots( window )
{
	if ( isDefined( window.zm_near_spots ) )
		return window.zm_near_spots;

	min_dist = zm_dvar_int( "scr_zm_spawn_near_min", 160 );
	max_dist = zm_dvar_int( "scr_zm_spawn_near_max", 320 );
	wide_dist = int( max_dist * 1.5 );

	// The outward direction: from the window's inside traversal point out
	// through the exterior_goal.
	inside = maps\_zombiemode_zone_manager::entrance_inside_point( window );
	out = undefined;
	if ( isDefined( inside ) )
	{
		out = window.origin - inside;
		out = vectorNormalize( ( out[ 0 ], out[ 1 ], 0 ) );
	}

	goal_low = window.origin + ( 0, 0, 24 );
	goal_high = window.origin + ( 0, 0, 52 );
	near = [];
	wide = [];
	below = [];
	rej_side = 0;
	rej_zone = 0;
	rej_floor = 0;
	rej_sight = 0;
	rej_path = 0;
	nodes = getAllNodes();
	for ( i = 0; isDefined( nodes ) && i < nodes.size && near.size < 10; i++ )
	{
		node = nodes[ i ];
		if ( !isDefined( node.classname ) || node.classname != "node_pathnode" )
			continue;
		d = distance2D( node.origin, window.origin );
		if ( d < min_dist || d > wide_dist )
			continue;
		dz = node.origin[ 2 ] - window.origin[ 2 ];
		low = dz < -96;
		if ( dz > 96 || dz < -320 )
			continue;
		if ( low && below.size >= 6 )
			continue;
		if ( !low && d > max_dist && wide.size >= 6 )
			continue;
		if ( isDefined( out ) )
		{
			v = node.origin - window.origin;
			if ( v[ 0 ] * out[ 0 ] + v[ 1 ] * out[ 1 ] < 0.25 * d )
			{
				rej_side++;
				continue;
			}
		}
		if ( isDefined( maps\_zombiemode_zone_manager::zone_at_point( node.origin ) ) )
		{
			rej_zone++;
			continue;
		}

		trace = bulletTrace( node.origin + ( 0, 0, 16 ), node.origin - ( 0, 0, 128 ), false, undefined );
		if ( trace[ "fraction" ] >= 1 )
		{
			rej_floor++;
			continue;
		}
		pos = trace[ "position" ];

		if ( !bulletTracePassed( pos + ( 0, 0, 24 ), goal_low, false, undefined ) ||
			!bulletTracePassed( pos + ( 0, 0, 52 ), goal_high, false, undefined ) )
		{
			rej_sight++;
			continue;
		}
		if ( !findpath( pos, window.origin ) )
		{
			rej_path++;
			continue;
		}

		spot = spawnStruct();
		// 2 units up: a teleport exactly onto the traced floor reads as
		// solid to the actor and gets lifted (and logged) on every spawn.
		spot.origin = pos + ( 0, 0, 2 );
		spot.angles = vectorToAngles( window.origin - pos );
		spot.angles = ( 0, spot.angles[ 1 ], 0 );
		spot.window = window;
		if ( low )
			below[ below.size ] = spot;
		else if ( d <= max_dist )
			near[ near.size ] = spot;
		else
			wide[ wide.size ] = spot;
	}

	spots = near;
	band = min_dist + "-" + max_dist;
	if ( spots.size < 3 )
	{
		for ( i = 0; i < wide.size; i++ )
			spots[ spots.size ] = wide[ i ];
		band = min_dist + "-" + wide_dist;
	}
	// Still short: an upper-floor window whose zombies climb to it from the
	// ground (Asylum's (-122 424 253): the T5-placed spawner (-178 413 13)
	// stands 240 below it).  Ground nodes below the window with a sight line
	// up to it and a route to it join the list.
	if ( spots.size < 3 )
	{
		for ( i = 0; i < below.size; i++ )
			spots[ spots.size ] = below[ i ];
		if ( below.size > 0 )
			band = band + " incl. " + below.size + " below";
	}

	window.zm_near_spots = spots;
	{}
	return spots;
}

abs_float( value )
{
	if ( value < 0 )
		return 0 - value;
	return value;
}

spot_occupied( origin )
{
	if ( !isDefined( level.zombies ) )
		return false;
	for ( i = 0; i < level.zombies.size; i++ )
	{
		z = level.zombies[ i ];
		if ( isDefined( z ) && isAlive( z ) && distanceSquared( z.origin, origin ) < 40 * 40 )
			return true;
	}
	return false;
}

// A live player looking towards the point (within about 70 degrees of his
// view direction) with a clear sight line to the zombie's chest, or one
// standing within 200 units of it.
spot_seen_by_player( origin )
{
	chest = origin + ( 0, 0, 48 );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		player = players[ i ];
		if ( !isDefined( player ) || !isAlive( player ) )
			continue;
		if ( isDefined( player.sessionstate ) && player.sessionstate != "playing" )
			continue;
		if ( distanceSquared( player.origin, origin ) < 200 * 200 )
			return true;
		eye = player getEye();
		dir = vectorNormalize( chest - eye );
		forward = anglesToForward( player getPlayerAngles() );
		if ( vectorDot( forward, dir ) < 0.34 )
			continue;
		if ( sightTracePassed( eye, chest, false, undefined ) )
			return true;
	}
	return false;
}

// Spawn-to-window and spawn-to-inside times.  The first of each per round is
// always printed (one line each); scr_zm_spawn_log 1 prints every zombie.
zm_entry_timing( what, node )
{
	if ( !isDefined( self.zm_spawn_time ) )
		return;
	ms = getTime() - self.zm_spawn_time;
	round_ms = -1;
	if ( isDefined( level.round_start_time ) )
		round_ms = getTime() - level.round_start_time;
	where = "";
	if ( isDefined( node ) )
		where = " window " + node.origin;
	key = "zm_first_" + what + "_round";
	first = !isDefined( level.zm_entry_first ) || !isDefined( level.zm_entry_first[ key ] ) ||
		level.zm_entry_first[ key ] != level.round_number;
	if ( first )
	{
		if ( !isDefined( level.zm_entry_first ) )
			level.zm_entry_first = [];
		level.zm_entry_first[ key ] = level.round_number;
	}
	if ( first || zm_dvar_int( "scr_zm_spawn_log", 0 ) > 0 )
	{
		prefix = "";
		if ( first )
			prefix = "round " + level.round_number + " first ";
		{}
	}
}

// T5 :3657 zombie_eye_glow / zombie_eye_glow_stop / delayed_zombie_eye_glow.
//
// C1-vs-T5 CONFLICT, resolved by playing what CoDOL ships.  T5 calls
// self haseyes(1), a native toggle that makes the engine attach
// level._effect["eye_glow"] to the model.  MEASURED: haseyes is NOT a
// registered builtin here, and stubbing it is forbidden, so the effect is
// played directly on the eye bones instead of via the toggle.
//
// THE EFFECT is CoDOL's own blue one, not T5's misc/fx_zombie_eye_single
// (which ships too, but is T5's generic fxt_light_glow tinted per element).
// MEASURED 2026-10-05, strings of the inflated common_gametype_zm zone:
// maps/zombie/zm_eye_lensflare_blue draws gfx_codo_zm_eye_lensflare_blue
// (a CoDOL-authored "codo" texture) with the eyeOffset lens-flare technique,
// and its sibling zm_eye_lensflare_blue_boss is the juggernaut's.
//
// THE TAGS.  MEASURED with codol_checkmodel: every c_zom_*_head carries
// j_eyeball_le, j_eyeball_ri and tag_eye (the bodies carry none of them,
// which is why character_zm_zombie attaches the head).
//
// ONE TAG, NOT ONE PER EYE: the effect already draws BOTH eyes.  MEASURED
// 2026-10-13 from its FxEffectDef in the inflated common_gametype_zm zone:
// 10 looping elements, five with spawnOrigin x = -0.15 and five with
// x = -2.85, i.e. a flare on the tag and a second one 2.7 units along the
// tag's -X axis.  That is the layout of T5's misc/fx_zombie_eye_single
// (elements at x -0.15 and -2.85), which T5/BO1 play once, on J_Eyeball_LE
// (BO1 clientscripts/_zombiemode.csc:273, _waw_zombie_mode.csc:12).  Played
// on both eyeballs it drew four flares: two coincided on one eye and the
// fourth floated 2.7 units off the side of the head (the user's "third
// glowing dot drawing to the left off their head").  MEASURED on a
// zm_prototype_sh probe (getTagOrigin/getTagAngles, all four heads): the
// eyeballs are 2.66-2.73 apart; from j_eyeball_le the second flare lands
// 0.57 from j_eyeball_ri, from j_eyeball_ri it lands 5.4 from j_eyeball_le,
// i.e. 2.7 outside the zombie's right eye (the viewer's left).
zombie_eye_glow_tags()
{
	tags = [];
	tags[ 0 ] = "j_eyeball_le";
	return tags;
}

zombie_eye_glow()
{
	self endon( "death" );

	if ( is_true( self.no_eye_glow ) || is_true( self.head_gibbed ) )
		return;

	if ( !isDefined( level._effect[ "zombie_eye_glow" ] ) )
		return;

	self.eye_glow_tags = zombie_eye_glow_tags();
	for ( i = 0; i < self.eye_glow_tags.size; i++ )
		playFxOnTag( level._effect[ "zombie_eye_glow" ], self, self.eye_glow_tags[ i ] );
	self.eye_glow_on = true;
}

// Must run while the head is still attached: stopFxOnTag on a bone the DObj
// no longer has is a script error (_zombiemode_gib::zombie_head_gib stops the
// glow before it detaches the head).
zombie_eye_glow_stop()
{
	if ( !is_true( self.eye_glow_on ) )
		return;

	self.eye_glow_on = false;
	if ( !isDefined( self.eye_glow_tags ) )
		return;

	for ( i = 0; i < self.eye_glow_tags.size; i++ )
		stopFxOnTag( level._effect[ "zombie_eye_glow" ], self, self.eye_glow_tags[ i ] );
}

// The glow is an entity event (playFxOnTag -> G_AddEvent), so a client that
// did not have the zombie in its snapshot when it spawned -- the spawners
// stand outside the playable area -- never saw it start.  It is re-issued
// once the zombie is through its window, where every player inside can see
// it; a client that already shows it gets the stop first, so the glow is
// replaced, not doubled.  The two halves are a server frame apart: an entity
// carries four events per snapshot, and two stops plus two starts in one
// frame would let any other event push a stop out.
zombie_eye_glow_refresh()
{
	self endon( "death" );

	if ( !is_true( self.eye_glow_on ) )
		return;

	self zombie_eye_glow_stop();
	wait( 0.05 );
	self zombie_eye_glow();
}

// T5 _zombiemode_ai_ape.gsc:236 uses this for a zombie that rises out of the
// ground: the eyes would otherwise float above the floor before the rise
// animation starts.
delayed_zombie_eye_glow( delay )
{
	self endon( "death" );

	if ( !isDefined( delay ) )
		delay = 1;

	wait( delay );

	self zombie_eye_glow();
}

// T5 maps\_zombiemode_audio.gsc do_zombies_playvocals, reduced to the alias
// lookup _zombiemode_utility already provides.  T5 keys its vocals off
// self.animname and a per-map alias prefix; CoDOL ships one zombie voice set
// (zmb_zombie_*, docs/zone_assets/ALL_sound.txt), so the ref is the state.
do_zombies_playvocals( state )
{
	if ( !isDefined( state ) )
		return;

	self play_sound_on_ent( "zombie_" + state );
}

// T5 :3620 play_ambient_zombie_vocals. Local pacing: idle vocals share an
// 8-15 second listener budget. Combat/death/notetrack sounds remain separate.
play_ambient_zombie_vocals()
{
	self endon( "death" );
	level endon( "end_game" );

	for ( ;; )
	{
		wait( randomFloatRange( 0.75, 1.25 ) );
		ambient_zombie_vocals_tick();
	}
}

ambient_zombie_vocals_tick()
{
	if ( !isDefined( level.zombies ) || !isDefined( level.zombie_sounds ) )
		return 0;

	listeners = 0;
	now = getTime();
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		player = players[ i ];
		if ( !isDefined( player ) || !isAlive( player ) )
			continue;
		if ( isDefined( player.sessionstate ) && player.sessionstate != "playing" )
			continue;
		if ( isDefined( player.zmb_ambient_vocal_until ) && now < player.zmb_ambient_vocal_until )
			continue;

		best = undefined;
		// Exact C1 alias distance maximum for both idle vocal families.
		best_sq = 1000 * 1000;
		for ( j = 0; j < level.zombies.size; j++ )
		{
			z = level.zombies[ j ];
			if ( !isDefined( z ) || !isAlive( z ) || is_true( z.is_boss ) || !isDefined( z.has_legs ) )
				continue;
			d = distanceSquared( z.origin, player.origin );
			if ( d <= best_sq )
			{
				best = z;
				best_sq = d;
			}
		}
		if ( !isDefined( best ) )
			continue;

		ref = "crawler_ambient";
		if ( is_true( best.has_legs ) )
			ref = "zombie_ambient";
		if ( !isDefined( level.zombie_sounds[ ref ] ) )
			continue;

		// No wait between reservation and emission: competing actor threads
		// cannot emit a second idle vocal for this listener in the same frame.
		player.zmb_ambient_vocal_until = now + int( randomFloatRange( 8, 15 ) * 1000 );
		best playSoundToPlayer( level.zombie_sounds[ ref ], player );
		listeners++;
	}
	return listeners;
}

// T5 :4416 make_crawler.  A zombie whose legs are gone keeps fighting on the
// ground: the cycle, the melee pool and the death clip all switch sets, and
// the native mover is told to travel at a crawl.
//
// T5 reaches this from zombie_gib_on_damage (_zombiemode_gib.gsc), which
// has already swapped the legs for the gib models and set self.a.gib_ref to
// the leg that went.  A crawler made by any other script has no gib yet, so
// it loses both legs here the same way (T5's "no_legs").
make_crawler()
{
	if ( !is_true( self.has_legs ) )
		return;

	self.has_legs = false;
	self.zm_final_sprinter = undefined;
	if ( !isDefined( self.a.gib_ref ) )
		self.a.gib_ref = "no_legs";
	if ( !is_true( self.gibbed ) )
		self maps\_zombiemode_gib::do_gib();
	self.a.pose = "prone";

	self set_zombie_run_cycle( "walk" );

	// T5 :2925: with no legs at all the zombie walks on its hands.
	if ( self.a.gib_ref == "no_legs" )
	{
		hands = "crawl_hand_" + randomIntRange( 1, 3 );
		if ( isDefined( level.scr_anim[ self.animname ][ hands ] ) )
		{
			self set_run_anim( hands );
			self.crawl_anim = self.run_noncombatanim;
		}
	}
	// setMoveSpeedScale is player-only on this engine; see
	// apply_move_speed_scale() below.  0.35 is T5's crawler scalar, which is
	// lower than any bucket move_speed_scale() returns, so it is passed
	// explicitly rather than derived.
	self apply_move_speed_scale( 0.35 );

	// Re-pick immediately rather than at the next cycle boundary.
	self notify( "killanimscript" );
}

// T5 :3840 zombie_death_animscript.  The hook zombie_death.gsc runs after the
// death clip has played out, i.e. the last thing that happens before the
// actor becomes a corpse.
zombie_death_animscript()
{
	// T5 :3353: a zombie killed at a window gives its attack spot back.
	self reset_attack_spot();
	self zombie_eye_glow_stop();
	self do_zombies_playvocals( "death" );
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
	// NO endon( "death" ).  MEASURED 2026-09-24 (probe w3, temporary log of
	// both notifies): on an actor the killing hit's "damage" notify reaches
	// this thread only AFTER "death" has been delivered, in the same frame.
	// With endon( "death" ) the killing hit was therefore never seen, and
	// zombie_death_event read the mod, hit location and weapon of the
	// PREVIOUS hit (a revive-knife kill after a money-ball hit was paid as a
	// money-ball kill).  Without it this thread records the killing hit, and
	// zombie_death_event waits for the end of the frame before reading it.
	// The health <= 0 test below keeps the killing hit from also paying
	// "damage" points.  The thread ends when the corpse entity is freed.

	for ( ;; )
	{
		self waittill( "damage", amount, attacker, direction, point, mod, modelName, tagName, partName, flags, weapon );

		// DIAGNOSTIC 2026-09-20.  The parameter list of the actor "damage"
		// notify was INFERRED by the porting pass, never measured, and the
		// live log says the economy only ever SPENDS: every score change in
		// bin/home/main/qconsole.log reaches set_player_score_hud through
		// minus_to_player_score (:231, 5 changes), and add_to_player_score
		// (:218) appears in ZERO stacks.  If `attacker` is not in fact the
		// 2nd argument of this notify, that is the reason.  Report the first
		// few so the next live test answers it outright instead of guessing.
		if ( !isDefined( level.zm_diag_damage ) )
			level.zm_diag_damage = 0;

		if ( level.zm_diag_damage < 5 )
		{
			level.zm_diag_damage++;
			{}
		}

		// Native vehicle FireWeapon attributes the real bullet to the vehicle.
		// Its live player gunner owns that damage, as with owned equipment in
		// _zombiemode::player_damage_blocked. Preserve the raw notify above.
		if ( isDefined( attacker ) && !isPlayer( attacker ) &&
			isDefined( attacker.owner ) && isPlayer( attacker.owner ) )
			attacker = attacker.owner;
		if ( !isDefined( attacker ) || !isPlayer( attacker ) )
			continue;

		self.attacker = attacker;
		self.damagemod = mod;
		self.damagelocation = partName;
		// CoDOL equipment kill hook: the killing hit's weapon (10th notify
		// argument, codol_equipment_lifecycle.c CODOL_AddDamageWeaponArgument),
		// read by zombie_death_event for level.zombie_kill_weapon_callbacks.
		self.damageweapon = weapon;

		self maps\_zombiemode_score::double_points_impact( point, amount, mod );

		// BEGIN tesla hook -- T5 _zombiemode_spawner.gsc:3548 zombie_damage:
		// a tesla projectile hit starts the chain lightning (the weapon's
		// real damage) and pays no damage points.  Threaded on level because
		// this watcher ends with this zombie, the chain's first kill.
		if ( maps\_zombiemode_weap_tesla::is_tesla_damage( weapon, mod ) )
		{
			level thread maps\_zombiemode_weap_tesla::tesla_damage_init( self, partName, point, attacker );
			continue;
		}
		// END tesla hook

		if ( self.health <= 0 )
			continue;

		// A beheaded zombie's bleed-out tick (_zombiemode_gib::damage_over_time)
		// is not a hit the player landed.
		if ( isDefined( self.zm_gib_dot_time ) && self.zm_gib_dot_time == getTime() )
			continue;

		attacker maps\_zombiemode_challenges::show_hitmarker( false );

		// T5 pays damage_ads at 1.25x for a hit landed while aiming.
		if ( attacker playerADS() > 0.9 )
			attacker maps\_zombiemode_score::player_add_points( "damage_ads", mod, partName );
		else
			attacker maps\_zombiemode_score::player_add_points( "damage", mod, partName );

		// T5 zombie_damage :3654 / zombie_damage_ads :3712 end every hit with
		// check_for_instakill: while Insta-Kill is up the hit is lethal.
		self maps\_zombiemode_powerups::check_for_instakill( attacker, mod, partName );
	}
}

// T5 zombie_think gives an authored target priority over zombie_chaser.
// Kino's untargeted authored chasers hunt directly, as in BO1, without
// selecting a window first. Riser emergence is a separate sequence.
kino_untargeted_chaser()
{
	if ( getDvar( "mapname" ) != "zm_theater_sh" )
		return false;

	spawn_point = self.zm_spawn_point;
	if ( !isDefined( spawn_point ) || !isDefined( spawn_point.script_string ) ||
		spawn_point.script_string != "zombie_chaser" )
		return false;

	return !isDefined( spawn_point.target ) || spawn_point.target == "";
}

// T5 _zombiemode_spawner.gsc:3922 find_flesh plus the animscript handoff in
// T5's zombie_think (:415), which the previous pass deferred.
//
// THE HANDOFF.  T5's zombie_think re-picks the run cycle whenever the round
// raises the move speed (self.needs_run_update), and hands the actor to the
// melee animscript when it closes on its target.  On this server the SECOND
// half is the native frame's job already -- CODOL_ActorRunFrame selects
// <animscript>_combat once the enemy is inside self.meleeAttackDist
// (codol_actor_lifecycle.cpp:702-712 and :866) -- so the handoff here is the
// first half only: keep the cycle in step with the round, and keep the goal
// pointed at flesh.
zombie_think()
{
	self endon( "death" );
	self endon( "zombie_delete" );
	level endon( "end_game" );

	// T5 zombie_think :415-:568 and zombie_goto_entrance :572, restored
	// 2026-09-21 (docs/CODOL_ZOMBIE_SPAWN_20260921.md part 2).  A zombie is
	// spawned OUTSIDE the playable area (the 42 zombie_spawner structs on
	// zm_prototype_sh all are) and T5 never sends it at the player from
	// there: it first walks to an entrance -- one of the closest exterior_goal
	// structs (3 candidates, dropping any more than max_dist farther than
	// the previous, random among the rest) at goalradius 128 -- tears the
	// boards there, and only then seeks flesh.  The old loop went straight for
	// the player, so with the entrance 100+ units to the side the zombie
	// walked into the outer wall next to the window and stood there.
	// T5 zombie_think: finish a genuine rise before deciding on teardown.
	// Only authored Kino risers enter this branch; chasers/windows retain818.
	rise_result = undefined;
	if ( maps\_zombiemode_kino_risers::is_riser( self.zm_spawn_point ) )
	{
		rise_result = self maps\_zombiemode_kino_risers::run();
		if ( !is_true( self.kino_rise_completed ) )
			return;
	}
	node = undefined;
	if ( !isDefined( rise_result ) || rise_result != "find_flesh" )
	{
		if ( !self kino_untargeted_chaser() )
			node = self pick_entrance_node();
	}
	if ( isDefined( node ) )
	{
		self.first_node = node;
		self.zm_path_phase = "entrance";
		self.goalradius = 128;
		self setGoalPos( node.origin );
		// T5 waits on "goal" alone (:585), with zombie_assure_node moving
		// the goal to the next closest entrance on "bad_path".  The 20 s
		// give-up this port had here fired on every zombie -- a walk cycle
		// covers the 700-1100 units from a yard spawner to its window in
		// 45-70 s -- and sent it into the chase with the boards still up
		// (docs/CODOL_ZOMBIE_WINDOW_20260921.md).
		if ( self zombie_wait_entrance() )
		{
			node = self.first_node;
			self zm_entry_timing( "window", node );
			self.goalradius = 64;
			self zombie_tear_until_open( node );
			self open_barrier_paths( node );
			self zombie_climb_in( node );
			self zm_entry_timing( "inside", node );
		}
		else if ( !self zombie_can_reach_flesh() )
		{
			// T5 zombie_assure_node :688-:697: no entrance is reachable from
			// here.  T5 leaves the zombie where it is for 20 s and then kills
			// it ("Please fix pathing or Entity setup").  Sending it on to the
			// boards of an unreachable window and then at the player, as this
			// port did, walked it from the yard into the outer wall and left
			// it there until the 30 s failsafe.
			self.zm_path_phase = "gave_up";
			wait( 20 );
			self.no_powerup_drop = true;
			self doDamage( self.health + 10, self.origin );
			return;
		}
		// else: no entrance, but the route to a player exists -- the spawner
		// is on the playable side already (zm_zombie_training's start-room
		// spawner (125 -1246 272) has no target and no reachable window).
		// T5 sends such zombies straight to find_flesh (should_skip_teardown,
		// :478).
	}

	// T5 zombie_goto_entrance :620 and find_flesh :3934: attack mode and
	// visibility to other AI begin once the entrance has been dealt with.
	self zombie_setup_attack_properties();
	self thread zombie_eye_glow_refresh();
	self.ignoreme = false;
	self.zm_entered = true;
	self.zm_path_phase = "hunt";

	for ( ;; )
	{
		if ( is_true( self.zm_solo_retreat ) )
		{
			wait( 0.1 );
			continue;
		}
		// ---- zm placeable traps hook (_zombiemode_traps_placeable.gsc) ----
		// A zombie held by a freeze trap stays in the native scripted state;
		// neither the chase nor a board tear (whose animscripted would
		// replace the freeze) may run until it thaws.
		if ( is_true( self.trap_frozen ) )
		{
			wait( 0.1 );
			continue;
		}
		// ---- end traps hook ----

		// T5 find_flesh :3922 hands off to tear_into_building when a barrier
		// stands between the zombie and the flesh.  Restored 2026-09-20; this
		// is what removes a board now that the invented timer is gone.
		barrier = self nearest_blocking_barrier();
		if ( isDefined( barrier ) )
		{
			self tear_into_building( barrier );
			self open_barrier_paths( barrier );
			// Torn open from the hunt (a zombie that lost its spot, or one
			// a repair sent back out): it climbs in through this window
			// rather than chasing a player it may have no route to from
			// outside, which left it against the outer wall.  The climb owns
			// the goal (ignoreall), as the entrance climb does.
			if ( all_chunks_destroyed( barrier.barrier_chunks ) )
			{
				self.ignoreall = true;
				self zombie_climb_in( barrier );
				self.ignoreall = false;
				self.zm_path_phase = "hunt";
			}
			self.goalradius = 64;
			// The tear can return at once (a crawler has no taunt to wait
			// out); never spin this loop without yielding.
			wait( 0.05 );
			continue;
		}

		// T5 find_flesh :3989 / zombie_follow_enemy :4305: a point of interest
		// (cymbal monkey, black hole bomb) outranks flesh and owns the goal
		// for this pass; _zombiemode_poi hands the zombie back when it ends.
		if ( !self maps\_zombiemode_poi::zombie_follow_poi() )
		{
			player = closest_valid_player( self.origin, self );

			if ( isDefined( player ) )
				self setGoalPos( player.origin );
			else
				// No eligible flesh: drop the former chase, including the
				// airborne helicopter goal, without replacing entrance work.
				self setGoalPos( self.origin );

			// CoDOL Cortex taunt: when the voice that drives the horde
			// speaks, every hunting zombie that can stops together (red
			// eyes, blue mouth) and then resumes the chase.  Run here, on
			// the thread that owns the goal and the board tear, so the
			// pause overlaps neither.
			if ( self maps\_zombiemode_cortex::cortex_should_join( player ) )
				self maps\_zombiemode_cortex::cortex_join( player );
		}

		// T5 :455.  The round raised level.zombie_move_speed since this actor
		// picked its cycle: re-roll the bucket, re-pick the clip and re-scale
		// the translation so all three agree again. A final survivor's sprint
		// remains latched through set_run_speed instead of being re-rolled.
		if ( is_true( self.needs_run_update ) && isDefined( self.a ) )
		{
			self set_zombie_run_cycle();
			self apply_move_speed_scale();
			// set_zombie_run_cycle raises the flag again (T5 :3699 does the
			// same), so it is cleared after the call, not before.
			self.needs_run_update = false;
		}

		// One-second poll, woken early when a Cortex event starts so the
		// whole horde stops together.
		self maps\_zombiemode_cortex::cortex_hunt_wait( 1 );
	}
}

// T5 tear_into_building :746 is one `while( 1 )` around the entrance window:
// a zombie with no free attack spot taunts, waits 0.5 s and asks again; one
// whose spot work ended with boards still up goes round again; it returns
// only once every chunk is gone, and zombie_goto_entrance then opens the
// window and the zombie climbs through.  This port's tear_into_building is
// one pass (the hunt loop reuses it for the nearest barrier), so the
// entrance walk loops it here.  Without the loop a zombie that found the
// three spots taken (a crowd at one window) or did not reach its spot taunted
// once and fell straight into the hunt from outside the building: it skipped
// zombie_climb_in, and when the player was not reachable from the yard
// (downed, upstairs behind closed debris, another graph component) its chase
// ran it into the outer wall beside the window, where the crowd piled up.
// Bounded where T5 is not: a zombie that fails to reach a spot four times
// (geometry, not a crowd) is handed to the hunt as before.
zombie_tear_until_open( node )
{
	self endon( "death" );
	self endon( "zombie_delete" );

	self.zm_spot_failures = 0;
	for ( ;; )
	{
		if ( !isDefined( node ) || !isDefined( node.barrier_chunks ) || all_chunks_destroyed( node.barrier_chunks ) )
			return;
		if ( !isDefined( node.target ) || ( isDefined( node.script_noteworthy ) && node.script_noteworthy == "no_blocker" ) )
			return;
		if ( self.zm_spot_failures >= 4 )
		{
			{}
			return;
		}

		self tear_into_building( node );

		// T5 :793 wait( 0.5 ) between attack-spot requests.
		if ( !all_chunks_destroyed( node.barrier_chunks ) )
			wait( 0.5 );
	}
}

// T5 zombie_goto_entrance :601-:607: once the barrier's chunks are all gone
// the clip is dropped out of the way (disable_trigger) and the window's
// negotiation link is reconnected (native Path_ConnectPaths), so the
// interior route can cross it; the native mover then plays the traversal at
// the begin node (animscripts\zombie_jump).  Idempotent.
open_barrier_paths( node )
{
	if ( !isDefined( node ) || !isDefined( node.clip ) )
		return;

	if ( isDefined( node.barrier_chunks ) && !all_chunks_destroyed( node.barrier_chunks ) )
		return;

	if ( is_true( node.clip.paths_open ) )
		return;

	node.clip.paths_open = true;
	node.clip disable_trigger();
	node.clip connectpaths();
	{}
}

// T5 zombie_think :448-:556: the entrance choice.
//
// A spawner that targets something names its window.  T5 :461-:475: the
// desired origin is the spawner's target (an ent, a struct or a node) and the
// entrance is the exterior_goal closest to it, with no zone filter.  Every
// zombie_spawner on zm_prototype_sh and 38 of 40 on zm_asylum_sh target the
// path node just inside their own window.  This port picked the three
// exterior_goals closest to the SPAWN point instead, which on Prototype are
// often in another yard: the directed graph cannot reach them (negotiation
// links run one way, yard -> interior), so each spawn burned bad_path on
// every candidate, gave up the entrance walk and chased the player from the
// yard into the outer wall (VPS 2026-10-01: ent 583 from the box-zone yard
// (700 1360) tried 633 566, 277 -257, -265 -185, -265 -752, 177 -943, never
// its own north window 457 1066, then wedged on the wall at 401 1035).
// A spawner without a target keeps T5's closest-3 choice (:497-:536).
pick_entrance_node()
{
	if ( !isDefined( level.exterior_goals ) || level.exterior_goals.size < 1 )
		return undefined;

	desired_origin = self zombie_desired_origin();
	if ( isDefined( desired_origin ) )
	{
		node = get_closest( desired_origin, level.exterior_goals );
		if ( isDefined( node ) )
		{
			self.entrance_nodes = [];
			self.entrance_nodes[ 0 ] = node;
			return node;
		}
	}

	max_dist = 500;
	if ( isDefined( level.max_barrier_search_dist_override ) )
		max_dist = level.max_barrier_search_dist_override;

	nodes = closest_exterior_goals( self.origin, 3 );
	if ( nodes.size < 1 )
		return undefined;

	desired = [];
	desired[ 0 ] = nodes[ 0 ];
	prev_dist = distance( self.origin, nodes[ 0 ].origin );
	for ( i = 1; i < nodes.size; i++ )
	{
		dist = distance( self.origin, nodes[ i ].origin );
		if ( ( dist - prev_dist ) > max_dist )
			break;
		prev_dist = dist;
		desired[ i ] = nodes[ i ];
	}

	self.entrance_nodes = desired;
	return desired[ randomInt( desired.size ) ];
}

// T5 :549 get_desired_origin, for the spawner struct this zombie came from:
// its target as an entity, a struct or a path node.
zombie_desired_origin()
{
	spawn_point = self.zm_spawn_point;
	if ( !isDefined( spawn_point ) || !isDefined( spawn_point.target ) || spawn_point.target == "" )
		return undefined;

	ent = getEnt( spawn_point.target, "targetname" );
	if ( isDefined( ent ) )
		return ent.origin;

	s = get_struct( spawn_point.target, "targetname" );
	if ( isDefined( s ) )
		return s.origin;

	node = getNode( spawn_point.target, "targetname" );
	if ( isDefined( node ) )
		return node.origin;

	return undefined;
}

// T5 maps\_utility::get_array_of_closest, for the exterior_goal structs.
closest_exterior_goals( origin, n )
{
	pool = [];
	for ( i = 0; i < level.exterior_goals.size; i++ )
	{
		if ( maps\_zombiemode_zone_manager::entrance_available( level.exterior_goals[ i ] ) )
			pool[ pool.size ] = level.exterior_goals[ i ];
	}
	// T5 :497 and zombie_assure_node :660 search every exterior_goal.  The
	// zone filter is CoDOL's; when it leaves nothing (its board-point test
	// finds 4 of Prototype's 12 windows inside a volume), an empty list sent
	// the zombie straight into the chase from the yard.  Fall back to T5.
	if ( pool.size < 1 )
		pool = level.exterior_goals;

	result = [];
	while ( pool.size > 0 && result.size < n )
	{
		best = 0;
		best_dist = distanceSquared( origin, pool[ 0 ].origin );
		for ( i = 1; i < pool.size; i++ )
		{
			d = distanceSquared( origin, pool[ i ].origin );
			if ( d < best_dist )
			{
				best = i;
				best_dist = d;
			}
		}
		result[ result.size ] = pool[ best ];
		rest = [];
		for ( i = 0; i < pool.size; i++ )
		{
			if ( i != best )
				rest[ rest.size ] = pool[ i ];
		}
		pool = rest;
	}
	return result;
}

// T5 zombie_goto_entrance :585 `self waittill( "goal" )` plus
// zombie_assure_node :636: on "bad_path" (the native mover found no route,
// or was wedged twice) the goal moves to the next closest entrance in
// self.entrance_nodes, then to the 20 closest, and only when every one of
// those is unreachable does the zombie give up (returns false; zombie_think
// then does what T5 does: waits 20 s and kills it).
zombie_wait_entrance()
{
	self endon( "death" );

	for ( ;; )
	{
		result = self zombie_wait_goal_or_bad_path();
		if ( result == "goal" )
			return true;

		next = self next_entrance_node();
		if ( !isDefined( next ) )
		{
			{}
			return false;
		}

		{}
		self.first_node = next;
		self setGoalPos( next.origin );
	}
}

zombie_wait_goal_or_bad_path()
{
	self endon( "death" );

	self thread notify_on( "goal", "entrance_result" );
	self thread notify_on( "bad_path", "entrance_result" );
	self waittill( "entrance_result", which );
	self notify( "entrance_result_done" );
	return which;
}

notify_on( event, forward )
{
	self endon( "death" );
	self endon( "entrance_result_done" );
	self waittill( event );
	self notify( forward, event );
}

// The next entrance to try: the rest of self.entrance_nodes, then (T5 :672)
// the 20 closest exterior_goals, each once.
next_entrance_node()
{
	if ( !isDefined( self.entrance_tried ) )
		self.entrance_tried = [];
	self.entrance_tried[ self.entrance_tried.size ] = self.first_node;

	pool = self.entrance_nodes;
	if ( !isDefined( pool ) || self.entrance_tried.size >= pool.size )
		pool = closest_exterior_goals( self.origin, 20 );

	for ( i = 0; i < pool.size; i++ )
	{
		tried = false;
		for ( j = 0; j < self.entrance_tried.size; j++ )
		{
			if ( self.entrance_tried[ j ] == pool[ i ] )
				tried = true;
		}
		if ( !tried )
			return pool[ i ];
	}
	return undefined;
}

// T5 zombie_bad_path :706: set the goal and give the planner 2 s to say
// "bad_path".  Here the goal is the closest valid player.
zombie_can_reach_flesh()
{
	self endon( "death" );

	player = closest_valid_player( self.origin, self );
	if ( !isDefined( player ) )
		return false;

	self.goalradius = 64;
	self setGoalPos( player.origin );
	self thread notify_on( "bad_path", "entrance_result" );
	self thread notify_after( 2, "entrance_result", "timeout" );
	self waittill( "entrance_result", which );
	self notify( "entrance_result_done" );
	return which != "bad_path";
}

// T5 zombie_goto_entrance :624-:626: after the tear the zombie starts
// find_flesh and waits for "zombie_start_traverse" / "zombie_end_traverse" --
// every zombie climbs through its window, because the path to the player runs
// through it.  When no path to a player exists from here (the player is
// downed, or stands where this side of the map cannot reach --
// zm_zombie_training's final room is its own graph component), the chase goal
// fails and the zombie would stand outside an open window.  So the goal is
// first the window's inside point (its node_negotiation_end); the native
// mover takes the window's traversal on the way, and the chase starts inside.
zombie_climb_in( node )
{
	self endon( "death" );

	if ( !isDefined( node ) || !isDefined( node.barrier_chunks ) || !all_chunks_destroyed( node.barrier_chunks ) )
		return;
	inside = maps\_zombiemode_zone_manager::entrance_inside_point( node );
	if ( !isDefined( inside ) )
		return;

	self.zm_path_phase = "climb";
	self.goalradius = 32;
	self setGoalPos( inside );
	self thread notify_on( "zombie_end_traverse", "entrance_result" );
	self thread notify_on( "goal", "entrance_result" );
	self thread notify_on( "bad_path", "entrance_result" );
	self thread notify_after( 15, "entrance_result", "timeout" );
	self waittill( "entrance_result", which );
	self notify( "entrance_result_done" );
}

notify_after( seconds, event, value )
{
	self endon( "death" );
	self endon( "entrance_result_done" );
	wait( seconds );
	self notify( event, value );
}

closest_valid_player( origin, seeker )
{
	players = getPlayers();

	best = undefined;
	best_dist = 0;

	for ( i = 0; i < players.size; i++ )
	{
		if ( !is_player_valid( players[ i ] ) || is_true( players[ i ].zm_extracted ) ||
			is_true( players[ i ].ignoreme ) || players[ i ] getDontTarget() )
			continue;
		// Native perfect-info acquisition uses this same actor-aware route
		// predicate, including live door/barrier cuts and both hull end legs.
		if ( isDefined( seeker ) && !seeker canReachPlayer( players[ i ] ) )
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

	// DIAGNOSTIC 2026-09-20, same reason as the "damage" probe above: whether
	// the actor "death" notify carries an attacker at all is INFERRED.
	if ( !isDefined( level.zm_diag_death ) )
		level.zm_diag_death = 0;

	if ( level.zm_diag_death < 5 )
	{
		level.zm_diag_death++;
		cached = undefined;
		if ( isDefined( zombie ) )
			cached = zombie.attacker;

		{}
	}

	// MEASURED (src/asmsource/_g_combat_mp.asm, G_Damage): a NULL attacker is
	// replaced by g_entities[2046], the world entity, BEFORE the notify is
	// pushed -- so `attacker` is never undefined here and the old
	// !isDefined() fallback was dead code.  A world kill (the stuck-actor
	// cull and the between-round sweep both doDamage with no attacker)
	// arrives as a defined non-player entity, which is what this must catch.
	// MEASURED 2026-09-24 (probe w3): on an actor the killing hit's "damage"
	// notify reaches zombie_damage_watch AFTER this "death" waiter resumes, in
	// the same frame.  Wait for the frame's other threads so .attacker,
	// .damagemod, .damagelocation and .damageweapon describe the killing hit,
	// not the one before it.
	waittillframeend;

	if ( isDefined( attacker ) && !isPlayer( attacker ) &&
		isDefined( attacker.owner ) && isPlayer( attacker.owner ) )
		attacker = attacker.owner;
	if ( !isPlayer( attacker ) && isDefined( zombie ) && isDefined( zombie.attacker ) )
		attacker = zombie.attacker;

	mod = undefined;
	hit_location = undefined;
	kill_weapon = undefined;
	if ( isDefined( zombie ) )
	{
		mod = zombie.damagemod;
		hit_location = zombie.damagelocation;
		kill_weapon = zombie.damageweapon;
	}

	if ( isDefined( attacker ) && isPlayer( attacker ) )
	{
		// T5 zombie_death_points :3283.
		event = "death";
		if ( isDefined( kill_weapon ) && isSubStr( kill_weapon, "knife_ballistic_" ) && isDefined( mod ) &&
			( mod == "MOD_MELEE" || mod == "MOD_IMPACT" ) )
			event = "ballistic_knife_death";
		// T5 zombie_death_animscript :3360 pays nothing for a thundergun
		// fling kill; the blast pays "thundergun_fling" itself.
		if ( !( isDefined( zombie ) && is_true( zombie.thundergun_death ) ) )
			attacker maps\_zombiemode_score::player_add_points( event, mod, hit_location );

		if ( !isDefined( attacker.kills ) )
			attacker.kills = 0;
		// MEASURED 2026-09-21: the player's kills field arrives as a float
		// from the stats layer and ++ is int-only ("++ must be applied to an
		// int", seven runtime errors in one session, each killing this
		// thread before the drop roll and zom_kill notify below).
		attacker.kills = attacker.kills + 1;

		attacker notify( "zom_kill", zombie );

		// CoDOL equipment kill hook: a per-weapon kill callback (money_ball_zm's
		// +100, _zombiemode_weap_money_ball.gsc), run on the attacker.
		if ( isDefined( kill_weapon ) && isDefined( level.zombie_kill_weapon_callbacks ) &&
			isDefined( level.zombie_kill_weapon_callbacks[ kill_weapon ] ) )
			attacker thread [[ level.zombie_kill_weapon_callbacks[ kill_weapon ] ]]( zombie, kill_weapon );
	}

	// T5 :3818.  The kill is what rolls for a power-up drop.
	//
	// Two kinds of kill must NOT roll: a nuke (T5 suppresses it so one
	// power-up cannot carpet the map with more) and a kill this server makes
	// for bookkeeping -- round_spawn_failsafe's stuck-actor cull and
	// clear_zombies' between-round sweep, neither of which T5 has at all.
	// Both set .no_powerup_drop.
	if ( isDefined( zombie ) && !is_true( zombie.nuked ) && !is_true( zombie.no_powerup_drop ) )
		level thread maps\_zombiemode_powerups::powerup_drop( zombie.origin, is_true( zombie.ignoreme ) );

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

		// T5 :4081: a zombie that tore a board in the last 8 s is working its
		// window, not stuck; check again in 30.
		if ( isDefined( self.lastchunk_destroy_time ) && getTime() - self.lastchunk_destroy_time < 8000 )
			continue;

		if ( self.origin[ 2 ] < level.zombie_vars[ "below_world_check" ] )
		{
			self.no_powerup_drop = true;
			self doDamage( self.health + 100, ( 0, 0, 0 ) );
			return;
		}

		if ( distanceSquared( self.origin, prevorigin ) < 576 )
		{
			self.no_powerup_drop = true;
			self doDamage( self.health + 100, ( 0, 0, 0 ) );
			return;
		}

		prevorigin = self.origin;
	}
}

// T5 _zombiemode.gsc:3294 zombie_speed_up, with the requested final survivor
// sprint: round 4+, all spawning finished, one standing zombie alone for 10 s.
zombie_speed_up()
{
	level endon( "end_of_round" );
	level endon( "end_game" );

	if ( level.round_number <= 3 )
		return;

	started = getTime();
	last = undefined;
	alone_since = 0;
	for ( ;; )
	{
		wait( 1 );
		if ( is_true( level.intermission ) )
			return;

		living = [];
		for ( i = 0; i < level.zombies.size; i++ )
		{
			zombie = level.zombies[ i ];
			if ( isDefined( zombie ) && isAlive( zombie ) && !is_true( zombie.ignore_enemy_count ) )
				living[ living.size ] = zombie;
		}
		count = living.size;
		candidate = undefined;

		for ( i = 0; i < living.size; i++ )
		{
			zombie = living[ i ];
			if ( is_true( zombie.is_boss ) || !isDefined( zombie.animname ) || zombie.animname != "zombie" ||
				!is_true( zombie.has_legs ) )
				continue;

			// Retain the five-second catch-up for the final three, without
			// downgrading an actor that is already sprinting.
			if ( count <= 3 && level.zombie_total <= 0 && getTime() - started >= 5000 &&
				zombie.zombie_move_speed == "walk" )
			{
				zombie set_zombie_run_cycle( "run" );
				zombie.needs_run_update = false;
				zombie apply_move_speed_scale();
			}
			if ( count == 1 && level.zombie_total <= 0 )
				candidate = zombie;
		}

		if ( !isDefined( candidate ) )
		{
			last = undefined;
			continue;
		}
		if ( !isDefined( last ) || last != candidate )
		{
			last = candidate;
			alone_since = getTime();
		}
		if ( getTime() - alone_since < 10000 )
			continue;

		candidate.zm_final_sprinter = true;
		candidate.zombie_move_speed_original = "sprint";
		candidate set_zombie_run_cycle( "sprint" );
		candidate.needs_run_update = false;
		candidate apply_move_speed_scale();
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
		level.zombies[ i ].no_powerup_drop = true;
		level.zombies[ i ] doDamage( level.zombies[ i ].health + 100, ( 0, 0, 0 ) );
	}

	zombie_list_compact();
}

// ==========================================================================
// THE BARRICADE TEAR.  T5 _zombiemode_spawner.gsc:760 tear_into_building,
// :1114 get_attack_spot, :1150 zombie_tear_notetracks, :1790 get_tear_anim.
//
// THIS REPLACES AN INVENTED TIMER.  The previous pass could not tear a board
// with an animation, so _zombiemode_blockers.gsc carried
// blocker_zombie_tear_watch(), a barrier-side poll that deleted a chunk every
// few seconds while a zombie stood nearby.  That function is DELETED as of
// 2026-09-20.  A board now comes off only when the tear animation's "board"
// notetrack fires, which is T5's own mechanism, and remove_chunk on the other
// side of it is still T5's.
//
// T5 plays the tear clip with AnimScripted( "tear_anim", attacking_spot,
// node.angles, tear_anim, ... ).  As of 2026-09-21 that is what this does
// too: animscripted() is native (codol_script_anim.c publishes the clip,
// codol_actor_lifecycle.cpp kills the animscript and holds the actor in T5's
// AIS_SCRIPTEDANIM state, aligned to the spot and facing the window, until
// stopanimscripted() or the clip's length).  MEASURED in the 6f2b8e29 log:
// the previous in-place setFlaggedAnimKnobAllRestart lost to the move
// animscript's own knob-all loop, which re-applied the walk cycle every
// 0.2 s over the tear -- "tearing board 6" printed, then the actor stood in
// ai_zombie_walk_fast_v3 at weight 1 until it died, no "board" note, no
// board removed, all session.  The zombie still walks to the attack spot
// first (setGoalPos, goalradius 2); animscripted closes the remainder.
//
// ALSO NOT TAKEN: zombie_history (T5's per-actor debug ring),
// priority_board_bar_selection and get_linear_destroyed_chunk (T5 has both
// commented out in favour of get_closest_non_destroyed_chunk),
// self.zombie_board_tear_down_callback (no zm setter), and the bar/grate
// branches of get_tear_anim -- see _zombiemode_blockers.gsc's header for why
// the metal-bar variant does not exist in CoDOL.
// ==========================================================================

// T5 :1114.  Reserve one of the barrier's three attack positions.
//
// Each reservation also records its holder (node.attack_spots_holder), and a
// spot whose holder is gone, dead, or no longer reserving this window counts
// as free.  MEASURED 2026-10-05 (VPS 17733, zm_asylum_sh, one match): 25
// zombies were shot while holding a spot, and this port -- unlike T5, whose
// zombie_death_animscript :3353 calls reset_attack_spot -- never released it.
// Five windows, among them (125 -971 253) and (1119 -482.5 239.5), lost all
// three spots that way; every later zombie sent there found none free and
// taunted outside it for the rest of the match: the pile-ups the user saw
// (up to 79 s motionless per zombie, 10+ zombies at one window).
get_attack_spot( node )
{
	if ( !isDefined( node ) || !isDefined( node.attack_spots ) )
		return false;

	if ( !isDefined( node.attack_spots_holder ) )
		node.attack_spots_holder = [];

	indexes = [];
	for ( i = 0; i < node.attack_spots.size; i++ )
	{
		if ( is_true( node.attack_spots_taken[ i ] ) && !attack_spot_holder_valid( node, i ) )
		{
			node.attack_spots_taken[ i ] = false;
			node.attack_spots_holder[ i ] = undefined;
		}
		if ( !is_true( node.attack_spots_taken[ i ] ) )
			indexes[ indexes.size ] = i;
	}

	if ( indexes.size < 1 )
		return false;

	index = indexes[ randomInt( indexes.size ) ];

	self.attacking_node = node;
	self.attacking_spot_index = index;
	node.attack_spots_taken[ index ] = true;
	node.attack_spots_holder[ index ] = self;
	self.attacking_spot = node.attack_spots[ index ];

	return true;
}

// A reservation stands only while its zombie is alive and still holds this
// window's spot.
attack_spot_holder_valid( node, index )
{
	if ( !isDefined( node.attack_spots_holder ) )
		return true;
	holder = node.attack_spots_holder[ index ];
	if ( !isDefined( holder ) )
		return false;
	if ( !isAlive( holder ) )
		return false;
	if ( !isDefined( holder.attacking_node ) || holder.attacking_node != node )
		return false;
	if ( !isDefined( holder.attacking_spot_index ) || holder.attacking_spot_index != index )
		return false;
	return true;
}

// T5 :815 / :858 / :920: once every board is down the window's reservations
// are all dropped, so a zombie that arrives later is never turned away by a
// stale claim.
reset_all_attack_spots( node )
{
	if ( !isDefined( node ) || !isDefined( node.attack_spots_taken ) )
		return;
	for ( i = 0; i < node.attack_spots_taken.size; i++ )
	{
		node.attack_spots_taken[ i ] = false;
		if ( isDefined( node.attack_spots_holder ) )
			node.attack_spots_holder[ i ] = undefined;
	}
}

// T5 :1100 reset_attack_spot.
reset_attack_spot()
{
	if ( !isDefined( self.attacking_node ) || !isDefined( self.attacking_spot_index ) )
		return;

	node = self.attacking_node;
	index = self.attacking_spot_index;
	// Only this zombie's own claim: the spot may already have been freed as
	// stale and handed to another zombie.
	if ( !isDefined( node.attack_spots_holder ) || !isDefined( node.attack_spots_holder[ index ] ) || node.attack_spots_holder[ index ] == self )
	{
		node.attack_spots_taken[ index ] = false;
		if ( isDefined( node.attack_spots_holder ) )
			node.attack_spots_holder[ index ] = undefined;
	}
	self.attacking_node = undefined;
	self.attacking_spot_index = undefined;
	self.attacking_spot = undefined;
}

// T5 :1790 get_tear_anim, collapsed.
//
// T5's version is a 200-line switch on chunk.script_noteworthy (the barrier's
// slot number, "1".."6") crossed with the chunk's side and the actor's pose,
// picking one of the 36 boardtear clips.  The slot number is the index into
// the six clips of that side, which is what the switch computes the long way;
// the side comes from where the chunk sits relative to the barrier.
get_tear_anim( chunk, node )
{
	// T5 :1785 picks the clip set by the zombie's attack spot, not by where
	// the board is: spot 0 the middle set, 1 the right, 2 the left, for every
	// board it pulls.  The geometric choice below is kept for a zombie with no
	// reserved spot.
	side = "m";
	if ( isDefined( self.attacking_spot_index ) )
	{
		if ( self.attacking_spot_index == 1 )
			side = "r";
		else if ( self.attacking_spot_index == 2 )
			side = "l";
	}
	else if ( isDefined( node ) && isDefined( node.angles ) )
	{
		right = anglesToRight( node.angles );
		offset = chunk.origin - node.origin;
		dot = ( offset[ 0 ] * right[ 0 ] ) + ( offset[ 1 ] * right[ 1 ] );
		if ( dot > 12 )
			side = "r";
		else if ( dot < -12 )
			side = "l";
	}

	if ( is_true( self.has_legs ) )
		pool = level._zombie_boardtear[ side ];
	else
		pool = level._zombie_boardtear_crawl[ side ];

	if ( !isDefined( pool ) || pool.size < 1 )
	{
		// T5 falls back to the bash set for a slot with no tear clip.
		if ( !isDefined( level._zombie_boardbash ) || level._zombie_boardbash.size < 1 )
			return undefined;
		return level._zombie_boardbash[ randomInt( level._zombie_boardbash.size ) ];
	}

	// T5 indexes the pool by the authored slot number when there is one.
	if ( isDefined( chunk.script_noteworthy ) )
	{
		slot = int( chunk.script_noteworthy );
		if ( slot >= 1 && slot <= pool.size )
			return pool[ slot - 1 ];
	}

	return pool[ randomInt( pool.size ) ];
}

// T5 :1150 zombie_tear_notetracks.  Self is the zombie.
//
// The "board" arm is T5's, verbatim in behaviour: on the note, if the chunk
// is still there, stamp the time, play the tear FX and audio and call
// maps\_zombiemode_blockers::remove_chunk.  The FX/audio are
// threaded off remove_chunk itself here rather than off this note, because
// this port keeps them next to the chunk they act on; the ordering within the
// frame is the same.
//
// The "bar_bend" arm is dropped: MEASURED, CoDOL ships no ai_zombie_bar_bend
// xanim, so no clip this server can play ever emits that note.
zombie_tear_notetracks( msg, chunk, node )
{
	// T5 :1150 ends on "death" only.  The "killanimscript" endon this port
	// carried here attached to the CALLING thread (zombie_think) for the rest
	// of its life, and animscripted() now fires that notify to kill the move
	// animscript -- it would have ended the zombie's think loop instead.
	self endon( "death" );

	for ( ;; )
	{
		self waittill( msg, notetrack );

		if ( !isDefined( notetrack ) )
			return;

		if ( notetrack == "end" || notetrack == "finish" )
		{
			// MEASURED (obj/claude-workers/xanim_probe.py): every CoDOL
			// boardtear clip carries a "board" note at t 0.55-0.60, so this
			// arm only fires if notetrack delivery dropped it.  The tear
			// still happened on screen; remove the board and say so.
			if ( isDefined( chunk ) && !is_true( chunk.destroyed ) )
			{
				{}
				self.lastchunk_destroy_time = getTime();
				maps\_zombiemode_blockers::remove_chunk( chunk, node, true, self );
			}
			return;
		}

		if ( notetrack == "board" )
		{
			if ( isDefined( chunk ) && !is_true( chunk.destroyed ) )
			{
				self.lastchunk_destroy_time = getTime();
				// T5 :1183 remove_chunk( chunk, node, true, zomb ): node is
				// the barrier, whose facing aims the board's flight
				// (2026-09-22, the boards fly now).  remove_chunk marks the
				// chunk synchronously and threads the flight itself.
				maps\_zombiemode_blockers::remove_chunk( chunk, node, true, self );
			}
			continue;
		}

		self animscripts\zombie_shared::HandleNoteTrack( notetrack, msg );
	}
}

// T5 :760 tear_into_building.
//
// Wrapped: while the zombie walks to its attack spot and tears, the native
// perception pass must not own the goal.  MEASURED 2026-09-21: with
// ignoreall false the native chase re-targets the goal at the enemy every
// frame (CODOL_ActorRunFrame), so the script's setGoalPos( attacking_spot )
// held for one frame and wait_until_at_spot timed out on every zombie; the
// tear animation was never played in a whole session (no boardtear clip in
// the log).  T5 has no native chase -- find_flesh sets the goal -- so
// ignoreall = true here IS the T5 contract: the script owns the goal.
tear_into_building( node )
{
	self endon( "death" );
	self endon( "zombie_delete" );

	restore = !is_true( self.ignoreall );
	self.ignoreall = true;
	phase = self.zm_path_phase;
	self.zm_path_phase = "tear";
	self tear_into_building_inner( node );
	self.zm_path_phase = phase;
	if ( restore )
		self.ignoreall = false;
}

tear_into_building_inner( node )
{
	self endon( "death" );
	self endon( "zombie_delete" );

	if ( !isDefined( node ) || !isDefined( node.target ) )
		return;

	if ( isDefined( node.script_noteworthy ) && node.script_noteworthy == "no_blocker" )
		return;

	if ( all_chunks_destroyed( node.barrier_chunks ) )
		return;

	// T5 :785.  No free attack spot: taunt at the window and come back.
	if ( !self get_attack_spot( node ) )
	{
		self do_a_taunt();
		return;
	}

	self.goalradius = 2;
	self setGoalPos( self.attacking_spot );

	// T5 waits on "goal" and then on "orientdone" with a timeout.  The
	// native mover notifies "goal" inside goalradius; arrival is also
	// accepted within 32 units, and the wait is bounded (the spot is at most
	// ~130 units from the entrance goal: 15 s covers it at walk speed).
	if ( !wait_until_at_spot( self.attacking_spot, 15 ) )
	{
		{}
		if ( isDefined( self.zm_spot_failures ) )
			self.zm_spot_failures++;
		self reset_attack_spot();
		return;
	}

	if ( all_chunks_destroyed( node.barrier_chunks ) )
	{
		self reset_attack_spot();
		reset_all_attack_spots( node );
		return;
	}

	// T5 :820, the tear loop.
	for ( ;; )
	{
		chunk = get_closest_non_destroyed_chunk( self.origin, node.barrier_chunks );

		if ( !isDefined( chunk ) )
		{
			self stopanimscripted();
			self reset_attack_spot();
			return;
		}

		tear_anim = self get_tear_anim( chunk, node );
		if ( !isDefined( tear_anim ) )
		{
			self stopanimscripted();
			self reset_attack_spot();
			return;
		}

		chunk update_states( "target_by_zombie" );

		{}

		self thread do_zombies_playvocals( "teardown" );

		// T5 :405 AnimScripted( "tear_anim", attacking_spot, node.angles,
		// tear_anim, "normal", undefined, 1, 0.3 ): the animscript dies, the
		// clip owns the tree, the actor is aligned to the spot facing the
		// window.  MEASURED 2026-09-21 part 2: played with the knob-all from
		// this thread instead, the move animscript's own knob loop drove the
		// walk cycle back over the tear every 0.2 s and no "board" note ever
		// fired -- no board came down all session.  animscripted is native
		// now (codol_script_anim.c / codol_actor_lifecycle.cpp).
		self animscripted( "tear_anim", self.attacking_spot, node.angles, tear_anim, "normal", %body, self.animplaybackrate, 0.3 );

		self zombie_tear_notetracks( "tear_anim", chunk, node );

		if ( all_chunks_destroyed( node.barrier_chunks ) )
		{
			{}
			self stopanimscripted();
			self reset_attack_spot();
			reset_all_attack_spots( node );
			return;
		}

		// T5 :912.  Between boards, reach through at anyone standing there.
		if ( !self should_attack_player_thru_boards() && is_true( self.has_legs ) )
			self do_a_taunt();

		wait_network_frame();
	}
}

// T5 waits on the engine's "goal" notify.  This server's mover does not send
// one, so arrival is polled against the spot with a hard timeout -- the same
// safety T5 puts on its own "orientdone" wait, and for the same reason
// (T5's comment: "Otherwise, zombies could get stuck waiting").
wait_until_at_spot( spot, timeout )
{
	deadline = getTime() + int( timeout * 1000 );

	self.at_spot_goal = false;
	self thread flag_on_goal();

	// Arrival: the native "goal" (goalradius 2, floored at 4), or within 8
	// units.  The old 32-unit acceptance stopped the walk up to 32 short and
	// left the rest to the animscripted alignment slide, which has no step
	// and stalled on the sill lip or the crowd in front of the window
	// (MEASURED 2026-10-05: 5-8% of tears ran 10-32 units from their spot,
	// the zombie clawing at the boards from behind the others).  32 units is
	// still accepted once the walk has stopped getting closer for 1.5 s (a
	// walk cycle's authored root-motion pause runs up to about a second).
	best = distanceSquared( self.origin, spot );
	best_time = getTime();

	for ( ;; )
	{
		d = distanceSquared( self.origin, spot );
		if ( d < best - 4 )
		{
			best = d;
			best_time = getTime();
		}
		if ( self.at_spot_goal || d < 64 || ( d < 1024 && getTime() - best_time >= 1500 ) )
		{
			self notify( "at_spot_done" );
			return true;
		}

		if ( getTime() > deadline )
		{
			self notify( "at_spot_done" );
			return false;
		}

		wait( 0.1 );
	}
}

flag_on_goal()
{
	self endon( "death" );
	self endon( "at_spot_done" );
	self waittill( "goal" );
	self.at_spot_goal = true;
}

// T5 _zombiemode_utility.gsc get_closest_non_destroyed_chunk.
get_closest_non_destroyed_chunk( org, chunks )
{
	if ( !isDefined( chunks ) )
		return undefined;

	best = undefined;
	best_dist = 0;

	for ( i = 0; i < chunks.size; i++ )
	{
		if ( is_true( chunks[ i ].destroyed ) )
			continue;

		d = distanceSquared( org, chunks[ i ].origin );
		if ( !isDefined( best ) || d < best_dist )
		{
			best = chunks[ i ];
			best_dist = d;
		}
	}

	return best;
}

// T5 spawner should_attack_player_thru_boards / window_notetracks.
// A board is expected to stop an ordinary melee trace. The authored window
// fire note therefore owns the T5 reach/inside-window/floor checks and enters
// the existing native damage callback, not a timer or a generic note pump.
should_attack_player_thru_boards()
{
    if ( !is_true( self.has_legs ) || !isDefined( self.attacking_node ) )
        return false;
    node = self.attacking_node;
    if ( isDefined( node.barrier_chunks ) )
        for ( i = 0; i < node.barrier_chunks.size; i++ )
            if ( isDefined( node.barrier_chunks[ i ].unbroken ) && node.barrier_chunks[ i ].unbroken )
                return false;
    players = getPlayers();
    self.window_melee_targets = [];
    for ( i = 0; i < players.size; i++ )
        if ( is_player_valid( players[ i ] ) && distance2d( self.origin, players[ i ].origin ) <= 90 &&
             abs( self.origin[ 2 ] - players[ i ].origin[ 2 ] ) < 90 )
            self.window_melee_targets[ self.window_melee_targets.size ] = players[ i ];
    if ( self.window_melee_targets.size < 1 || !isDefined( level._zombie_window_attack ) || level._zombie_window_attack.size < 1 )
        return false;
    self.window_melee_node = node;
    self.window_melee_contact = false;
    reach = level._zombie_window_attack[ randomInt( level._zombie_window_attack.size ) ];
    self animscripted( "windowattack", self.origin, self.angles, reach, "normal", %body, self.animplaybackrate, 0.3 );
    self animscripts\zombie_shared::DoNoteTracksForTime( getAnimLength( reach ), "windowattack", ::window_attack_notetrack );
    self.window_melee_targets = undefined;
    self.window_melee_node = undefined;
    return true;
}

window_attack_notetrack( note )
{
    if ( note != "fire" || !isAlive( self ) || is_true( self.window_melee_contact ) ||
         !isDefined( self.window_melee_node ) || !isDefined( self.attacking_node ) ||
         self.window_melee_node != self.attacking_node || !isDefined( self.window_melee_targets ) )
        return;
    node = self.window_melee_node;
    // The blocker repair path uses the authored node when no separate target
    // marker exists; use that same real window anchor here.
    anchor = node;
    if ( isDefined( node.trigger_location ) )
        anchor = node.trigger_location;
    if ( !isDefined( self.meleeDamage ) || self.meleeDamage <= 0 )
        return;
    for ( i = 0; i < self.window_melee_targets.size; i++ )
    {
        player = self.window_melee_targets[ i ];
        // Recheck at the actual strike, not only when choosing the clip.
        if ( !is_player_valid( player ) || distance2d( self.origin, player.origin ) >= 90 ||
             abs( self.origin[ 2 ] - player.origin[ 2 ] ) >= 90 ||
             distance2d( anchor.origin, player.origin ) >= 51 ||
             abs( anchor.origin[ 2 ] - player.origin[ 2 ] ) >= 51 )
            continue;
        // Retain this server's one-contact-per-animation rule. Native player
        // damage still owns laststand, invulnerability and attacker cooldown.
        self.window_melee_contact = true;
        // Native MP positional ABI: attacker, inflictor, headshot, MOD. T5's
        // SP-shaped five-argument call cannot be copied into this VM.
        player doDamage( self.meleeDamage, self.origin, self, self, 0, "MOD_MELEE" );
        return;
    }
}

// T5 :960 do_a_taunt.
do_a_taunt()
{
	// T5 :424 ends on "death" only; see zombie_tear_notetracks.
	self endon( "death" );

	if ( !is_true( self.has_legs ) )
		return;

	pool = level._zombie_board_taunt[ self.animname ];
	if ( !isDefined( pool ) || pool.size < 1 )
		return;

	taunt = pool[ randomInt( pool.size ) ];

	self play_sound_on_ent( "zombie_taunt" );
	// T5 :437 animScripted( "zombie_taunt", self.origin, self.angles, anime, "normal", undefined, 1, 0.4 ).
	self animscripted( "tauntanim", self.origin, self.angles, taunt, "normal", %body, self.animplaybackrate, 0.4 );
	self animscripts\zombie_shared::DoNoteTracksForTime( getAnimLength( taunt ), "tauntanim" );
	self stopanimscripted();
}

// The seeker's barrier test.  T5 reaches tear_into_building from find_flesh
// when the pathfinder reports the zombie's route is blocked by an
// exterior_goal node; this server's mover has no such report, so the barrier
// is found the way blocker_attack_spots already indexes it -- by being the
// nearest one whose chunks are not all gone and whose attack spots are within
// reach of where the zombie is standing.
nearest_blocking_barrier()
{
	if ( !isDefined( level.exterior_goals ) )
		return undefined;

	// A window stands between the zombie and the flesh only while the zombie
	// is outside.  One already inside a player volume (it climbed in, or
	// spawned inside) that passes within 256 of a window still boarded
	// tried to tear it from the inside: its attack spots are outside the
	// wall, so it stood against the wall until wait_until_at_spot gave up
	// 15 s later, again and again (MEASURED 2026-10-02, zm_prototype_sh:
	// "did not reach attack spot (-250 -157) from (-191 -156)").  T5 only
	// tears from the exterior_goal side (zombie_goto_entrance).
	if ( isDefined( maps\_zombiemode_zone_manager::zone_at_point( self.origin ) ) )
		return undefined;

	best = undefined;
	best_dist = 0;

	for ( i = 0; i < level.exterior_goals.size; i++ )
	{
		node = level.exterior_goals[ i ];
		if ( !maps\_zombiemode_zone_manager::entrance_available( node ) )
			continue;

		if ( !isDefined( node.barrier_chunks ) || node.barrier_chunks.size < 1 )
			continue;

		if ( !isDefined( node.attack_spots ) || node.attack_spots.size < 1 )
			continue;

		if ( all_chunks_destroyed( node.barrier_chunks ) )
			continue;
		// An upstairs window can be within 256 units in 3D of a zombie
		// downstairs, yet none of its attack spots is reachable from this
		// floor.  Prototype traces showed that selection leaves the actor
		// stopped below the window with no path to its attack spot.
		delta_z = node.origin[ 2 ] - self.origin[ 2 ];
		if ( delta_z > 64 || delta_z < -64 )
			continue;

		d = distanceSquared( self.origin, node.origin );
		if ( d > 65536 )
			continue;

		if ( !isDefined( best ) || d < best_dist )
		{
			best = node;
			best_dist = d;
		}
	}

	return best;
}
