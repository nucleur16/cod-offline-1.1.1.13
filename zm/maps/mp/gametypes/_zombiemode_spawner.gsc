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
// STILL OUT, with the reason, not as a silent gap:
//   zombie_gib_on_damage / zombie_head_gib (T5 :2800, :3050).  T5 detaches a
//     tagged limb and attaches a stump model.  MEASURED
//     (docs/CODOL_ACTOR_SPECIES_20260920.md): CoDOL ships 17 gib models per
//     body family under ITS OWN suffixes (_behead, _larmoff_1, not T5's
//     _g_behead / _g_rarmspawn), and no shipped data names the TAGS they
//     attach to.  Guessing the tag names is the fabrication the mandate
//     forbids.  The hook self.gib_func is wired in zombie_death.gsc so the
//     module that learns the tags changes nothing else.
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

#include maps\mp\gametypes\_zombiemode_utility;

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
	level._effect[ "zombie_eye_glow" ] = loadfx( "misc/fx_zombie_eye_single" );

	init_spawn_points();
	init_spawner();
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
			println( "CoDOL zm: zombie melee weapon " + candidates[ i ] );
			return candidates[ i ];
		}
	}

	println( "CoDOL zm: no zombie melee WeaponDef is present; zombies cannot damage anyone" );
	return undefined;
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

	// Melee.  Without a real aiweapon the melee() method raises a script
	// error instead of damaging anyone -- see zombie_combat.gsc.
	if ( isDefined( level.zombie_melee_weapon ) )
		ai.aiweapon = level.zombie_melee_weapon;

	// T5 :3899, restored.
	ai zombie_setup_attack_properties();

	// T5 :3690 set_zombie_run_cycle picks BOTH halves: the animation and, via
	// set_run_speed, the walk/run/sprint bucket.  The scalar half -- how fast
	// the actor actually travels -- is setMoveSpeedScale, which the cycle now
	// derives from the bucket it chose rather than from the raw round speed,
	// so the clip and the translation agree.
	ai set_zombie_run_cycle();
	ai setMoveSpeedScale( ai move_speed_scale() );
	ai.needs_run_update = false;

	// T5 :3840.  zombie_death.gsc runs this after the death clip.
	ai.deathanimscript = ::zombie_death_animscript;

	// The bootstrap pose.  See the note in zombie_init.gsc: the animscript's
	// own main() runs too early in CODOL_ActorScriptsInitialize to animate,
	// so the first pose is set here, where the actor is fully initialised.
	// Without it the client draws the bind pose until the first cycle blends
	// in one frame later.
	ai setAnimKnobAll( %ai_zombie_idle_v1, %body, 1, 0.1, 1 );

	level.zombies[ level.zombies.size ] = ai;

	ai play_sound_on_ent( "zombie_spawn" );
	ai thread zombie_eye_glow();
	ai thread play_ambient_zombie_vocals();

	level thread zombie_death_event( ai );
	ai thread zombie_think();
	ai thread zombie_damage_watch();

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
		self set_run_anim( "run" + randomIntRange( 1, 7 ) );
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

	anim = level.scr_anim[ self.animname ][ anime ];
	if ( !isDefined( anim ) )
		return;

	self.a.combatrunanim = anim;
	self.run_combatanim = anim;
	self.run_noncombatanim = anim;
	self.walk_combatanim = anim;
	self.walk_noncombatanim = anim;
}

// T5 :3671 set_run_speed.  level.zombie_move_speed is the round's 0..100 dial
// that round_think raises; this buckets it into the three cycles.
set_run_speed()
{
	rand = randomIntRange( level.zombie_move_speed, level.zombie_move_speed + 35 );

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
move_speed_scale()
{
	if ( !isDefined( self.zombie_move_speed ) )
		return 0.5;

	switch ( self.zombie_move_speed )
	{
	case "sprint":
		return 1.4;
	case "run":
		return 0.9;
	}

	return 0.5;
}

// T5 :3657 zombie_eye_glow / zombie_eye_glow_stop / delayed_zombie_eye_glow.
//
// C1-vs-T5 CONFLICT, resolved by playing what CoDOL ships.  T5 calls
// self haseyes(1), a native toggle that makes the engine attach
// level._effect["eye_glow"] to the model.  MEASURED: haseyes is NOT a
// registered builtin here, and stubbing it is forbidden.  The FX it toggles
// IS shipped -- "misc/fx_zombie_eye_single" in common_gametype_zm, the exact
// name T5 loads at maps\_zombiemode.gsc:1027 -- so the effect is played
// directly on the actor's eye tag instead of via the toggle.
//
// tag_eye is the tag this server already resolves on an actor
// (codol_actor_lifecycle.cpp:1024 ActorEye uses it and falls back to
// origin+64 when the model has none), so it is the one eye tag known to work
// on these bodies.  T5's two-eye variant needs per-body eyeball tag names
// that no shipped CoDOL data states.
zombie_eye_glow()
{
	self endon( "death" );

	if ( is_true( self.no_eye_glow ) )
		return;

	if ( !isDefined( level._effect[ "zombie_eye_glow" ] ) )
		return;

	playFxOnTag( level._effect[ "zombie_eye_glow" ], self, "tag_eye" );
	self.eye_glow_on = true;
}

zombie_eye_glow_stop()
{
	if ( !is_true( self.eye_glow_on ) )
		return;

	stopFxOnTag( level._effect[ "zombie_eye_glow" ], self, "tag_eye" );
	self.eye_glow_on = false;
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

// T5 :3620 play_ambient_zombie_vocals.
play_ambient_zombie_vocals()
{
	self endon( "death" );

	for ( ;; )
	{
		wait( randomFloatRange( 3, 8 ) );

		if ( is_true( self.has_legs ) )
			self play_sound_on_ent( "zombie_ambient" );
		else
			self play_sound_on_ent( "crawler_ambient" );
	}
}

// T5 :4416 make_crawler.  A zombie whose legs are gone keeps fighting on the
// ground: the cycle, the melee pool and the death clip all switch sets, and
// the native mover is told to travel at a crawl.
//
// T5 reaches this from zombie_gib_on_damage, which is still out (see the
// header).  It is exposed so the module that lands the gibs -- or a level
// script, or the powerups module -- can call it, and so the crawl half of
// every table above is reachable rather than decorative.
make_crawler()
{
	if ( !is_true( self.has_legs ) )
		return;

	self.has_legs = false;
	self.a.gib_ref = "no_legs";
	self.a.pose = "prone";

	self set_zombie_run_cycle( "walk" );
	self setMoveSpeedScale( 0.35 );

	// Re-pick immediately rather than at the next cycle boundary.
	self notify( "killanimscript" );
}

// T5 :3840 zombie_death_animscript.  The hook zombie_death.gsc runs after the
// death clip has played out, i.e. the last thing that happens before the
// actor becomes a corpse.
zombie_death_animscript()
{
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

	for ( ;; )
	{
		// T5 find_flesh :3922 hands off to tear_into_building when a barrier
		// stands between the zombie and the flesh.  Restored 2026-09-20; this
		// is what removes a board now that the invented timer is gone.
		barrier = self nearest_blocking_barrier();
		if ( isDefined( barrier ) )
		{
			self tear_into_building( barrier );
			self.goalradius = 64;
			continue;
		}

		player = closest_valid_player( self.origin );

		if ( isDefined( player ) )
			self setGoalPos( player.origin );

		// T5 :455.  The round raised level.zombie_move_speed since this actor
		// picked its cycle: re-roll the bucket, re-pick the clip and re-scale
		// the translation so all three agree again.  zombie_speed_up() below
		// sets needs_run_update on every live zombie when it fires.
		if ( is_true( self.needs_run_update ) && isDefined( self.a ) )
		{
			self set_zombie_run_cycle();
			self setMoveSpeedScale( self move_speed_scale() );
			// set_zombie_run_cycle raises the flag again (T5 :3699 does the
			// same), so it is cleared after the call, not before.
			self.needs_run_update = false;
		}

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

	// T5 :3818.  The kill is what rolls for a power-up drop.
	//
	// Two kinds of kill must NOT roll: a nuke (T5 suppresses it so one
	// power-up cannot carpet the map with more) and a kill this server makes
	// for bookkeeping -- round_spawn_failsafe's stuck-actor cull and
	// clear_zombies' between-round sweep, neither of which T5 has at all.
	// Both set .no_powerup_drop.
	if ( isDefined( zombie ) && !is_true( zombie.nuked ) && !is_true( zombie.no_powerup_drop ) )
		level thread maps\mp\gametypes\_zombiemode_powerups::powerup_drop( zombie.origin );

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
			// T5 :3294 swaps the run CYCLE as well as the scalar.  Passing
			// the bucket explicitly is T5's own set_zombie_run_cycle( speed )
			// form and skips the re-roll, so every straggler ends up
			// sprinting rather than re-rolling into a walk.
			level.zombies[ i ] set_zombie_run_cycle( "sprint" );
			level.zombies[ i ].needs_run_update = false;
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
// ONE THING T5 HAS THAT THIS DOES NOT, named rather than faked.  T5 plays the
// tear clip with AnimScripted( "tear_anim", attacking_spot, node.angles,
// tear_anim, ... ), which LERPS the actor onto the attack spot and aligns the
// clip's root motion to it.  MEASURED: animscripted is deliberately not a
// registered builtin (docs/CODOL_SCRIPTED_ANIMATION_20260920.md section 5)
// and stubbing it is forbidden.  Ported around: the zombie is walked to the
// attack spot by the native mover first (setGoalPos with T5's own goalradius
// 2 and the node's angles), and the clip is then played in place with
// setFlaggedAnimKnobAllRestart.  The notetracks -- the half that removes the
// board -- are identical.  What is lost is sub-inch alignment of the hands to
// the board, not the mechanic.
//
// ALSO NOT TAKEN: zombie_history (T5's per-actor debug ring),
// priority_board_bar_selection and get_linear_destroyed_chunk (T5 has both
// commented out in favour of get_closest_non_destroyed_chunk),
// self.zombie_board_tear_down_callback (no zm setter), and the bar/grate
// branches of get_tear_anim -- see _zombiemode_blockers.gsc's header for why
// the metal-bar variant does not exist in CoDOL.
// ==========================================================================

// T5 :1114.  Reserve one of the barrier's three attack positions.
get_attack_spot( node )
{
	if ( !isDefined( node ) || !isDefined( node.attack_spots ) )
		return false;

	indexes = [];
	for ( i = 0; i < node.attack_spots.size; i++ )
	{
		if ( !is_true( node.attack_spots_taken[ i ] ) )
			indexes[ indexes.size ] = i;
	}

	if ( indexes.size < 1 )
		return false;

	index = indexes[ randomInt( indexes.size ) ];

	self.attacking_node = node;
	self.attacking_spot_index = index;
	node.attack_spots_taken[ index ] = true;
	self.attacking_spot = node.attack_spots[ index ];

	return true;
}

// T5 :1100 reset_attack_spot.
reset_attack_spot()
{
	if ( !isDefined( self.attacking_node ) || !isDefined( self.attacking_spot_index ) )
		return;

	self.attacking_node.attack_spots_taken[ self.attacking_spot_index ] = false;
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
	side = "m";
	if ( isDefined( node ) && isDefined( node.angles ) )
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
// maps\mp\gametypes\_zombiemode_blockers::remove_chunk.  The FX/audio are
// threaded off remove_chunk itself here rather than off this note, because
// this port keeps them next to the chunk they act on; the ordering within the
// frame is the same.
//
// The "bar_bend" arm is dropped: MEASURED, CoDOL ships no ai_zombie_bar_bend
// xanim, so no clip this server can play ever emits that note.
zombie_tear_notetracks( msg, chunk, node )
{
	self endon( "death" );
	self endon( "killanimscript" );

	for ( ;; )
	{
		self waittill( msg, notetrack );

		if ( !isDefined( notetrack ) )
			return;

		if ( notetrack == "end" || notetrack == "finish" )
			return;

		if ( notetrack == "board" )
		{
			if ( isDefined( chunk ) && !is_true( chunk.destroyed ) )
			{
				self.lastchunk_destroy_time = getTime();
				maps\mp\gametypes\_zombiemode_blockers::remove_chunk( chunk );

				if ( isDefined( node ) && isDefined( node.clip ) )
					node.clip disable_trigger();
			}
			continue;
		}

		self maps\mp\animscripts\zombie_shared::HandleNoteTrack( notetrack, msg );
	}
}

// T5 :760 tear_into_building.
tear_into_building( node )
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

	// T5 waits on "goal" and then on "orientdone" with a timeout.  This
	// server's native mover publishes neither, so the walk is bounded by
	// distance-or-timeout, which is the same contract T5's timeout expresses.
	if ( !wait_until_at_spot( self.attacking_spot, 6 ) )
	{
		self reset_attack_spot();
		return;
	}

	if ( all_chunks_destroyed( node.barrier_chunks ) )
	{
		self reset_attack_spot();
		return;
	}

	// T5 :820, the tear loop.
	for ( ;; )
	{
		chunk = get_closest_non_destroyed_chunk( self.origin, node.barrier_chunks );

		if ( !isDefined( chunk ) )
		{
			self reset_attack_spot();
			return;
		}

		tear_anim = self get_tear_anim( chunk, node );
		if ( !isDefined( tear_anim ) )
		{
			self reset_attack_spot();
			return;
		}

		chunk update_states( "target_by_zombie" );

		self thread do_zombies_playvocals( "teardown" );

		// T5 uses AnimScripted here -- see the header for why this does not.
		self.safeToChangeScript = false;
		self orientmode( "face angle", node.angles[ 1 ] );
		self setFlaggedAnimKnobAllRestart( "tear_anim", tear_anim, %body, 1, 0.3, self.animplaybackrate );

		self zombie_tear_notetracks( "tear_anim", chunk, node );

		self.safeToChangeScript = true;

		if ( all_chunks_destroyed( node.barrier_chunks ) )
		{
			self reset_attack_spot();
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

	for ( ;; )
	{
		if ( distanceSquared( self.origin, spot ) < 1024 )
			return true;

		if ( getTime() > deadline )
			return false;

		wait( 0.1 );
	}
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

// T5 :917 should_attack_player_thru_boards, reduced to its question: is a
// player close enough on the other side of this barrier to swipe at.  T5's
// version additionally traces; the native melee() traces for itself, so this
// only decides whether to play the reach-through clip instead of a taunt.
should_attack_player_thru_boards()
{
	players = getPlayers();

	for ( i = 0; i < players.size; i++ )
	{
		if ( !is_player_valid( players[ i ] ) )
			continue;

		if ( distanceSquared( self.origin, players[ i ].origin ) < 16384 )
		{
			if ( isDefined( level._zombie_window_attack ) && level._zombie_window_attack.size > 0 )
			{
				reach = level._zombie_window_attack[ randomInt( level._zombie_window_attack.size ) ];
				self setFlaggedAnimKnobAllRestart( "windowattack", reach, %body, 1, 0.2, self.animplaybackrate );
				self maps\mp\animscripts\zombie_shared::DoNoteTracksForTime( getAnimLength( reach ), "windowattack" );
			}
			return true;
		}
	}

	return false;
}

// T5 :960 do_a_taunt.
do_a_taunt()
{
	self endon( "death" );
	self endon( "killanimscript" );

	if ( !is_true( self.has_legs ) )
		return;

	pool = level._zombie_board_taunt[ self.animname ];
	if ( !isDefined( pool ) || pool.size < 1 )
		return;

	taunt = pool[ randomInt( pool.size ) ];

	self play_sound_on_ent( "zombie_taunt" );
	self setFlaggedAnimKnobAllRestart( "tauntanim", taunt, %body, 1, 0.2, self.animplaybackrate );
	self maps\mp\animscripts\zombie_shared::DoNoteTracksForTime( getAnimLength( taunt ), "tauntanim" );
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

	best = undefined;
	best_dist = 0;

	for ( i = 0; i < level.exterior_goals.size; i++ )
	{
		node = level.exterior_goals[ i ];

		if ( !isDefined( node.barrier_chunks ) || node.barrier_chunks.size < 1 )
			continue;

		if ( !isDefined( node.attack_spots ) || node.attack_spots.size < 1 )
			continue;

		if ( all_chunks_destroyed( node.barrier_chunks ) )
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
