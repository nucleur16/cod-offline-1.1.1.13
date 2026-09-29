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
			codolprintln( "CoDOL zm: zombie melee weapon " + candidates[ i ] );
			return candidates[ i ];
		}
	}

	codolprintln( "CoDOL zm: no zombie melee WeaponDef is present; zombies cannot damage anyone" );
	return undefined;
}

// T5 keys the spawn list off the active zone (maps\_zombiemode_zone_manager).
init_spawn_points()
{
	maps\mp\gametypes\_zombiemode_zone_manager::init();

	codolprintln( "CoDOL zm: spawn points found: " + level.enemy_spawns.size + " on " + getDvar( "mapname" ) );
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
			codolprintln( "CoDOL zm: spawnactorspawner produced no zombie spawner; no enemies this match" );
			level.no_zombies = true;
			return;
		}

		level.zombie_spawner.targetname = "zombie_spawner";
	}

	codolprintln( "CoDOL zm: zombie spawner ready" );
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

	// The split-start Asylum map has no single hardcoded initial room.  The
	// player_volume may not have been polled yet when round one begins.
	for ( zone_wait = 0; zone_wait < 100 && level.enemy_spawns.size < 1; zone_wait++ )
	{
		maps\mp\gametypes\_zombiemode_zone_manager::refresh_spawners();
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
		codolprintln( "CoDOL zm: round " + level.round_number + " has no spawners; nothing to spawn" );
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
	level.zm_round_budget = level.zombie_total;
	level.zm_round_spawned = 0;

	codolprintln( "CoDOL zm: round " + level.round_number + " budget " + level.zombie_total +
		" zombies at " + level.zombie_health + " health" );

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
			level.zm_round_spawned++;
			ai thread round_spawn_failsafe();
			// One line per round, not per zombie: enough to prove the chain
			// reached a live actor and where it was put.
			if ( !isDefined( level.zm_spawn_reported ) || level.zm_spawn_reported != level.round_number )
			{
				level.zm_spawn_reported = level.round_number;
				codolprintln( "CoDOL zm: round " + level.round_number + " first zombie spawned at " +
					ai.origin + " (ent " + ai getEntityNumber() + ", " + ai.health + " hp)" );
			}
		}
		else if ( !isDefined( level.zm_spawn_fail_reported ) || level.zm_spawn_fail_reported != level.round_number )
		{
			level.zm_spawn_fail_reported = level.round_number;
			codolprintln( "CoDOL zm: round " + level.round_number + " spawn attempt FAILED: " +
				level.zm_spawn_fail_reason );
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
	level.zm_spawn_fail_reason = "unknown";

	if ( !isDefined( level.zombie_spawner ) )
	{
		level.zm_spawn_fail_reason = "no zombie spawner (init_spawner produced none)";
		return undefined;
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
// (zm/maps/mp/animscripts/zombie_move.gsc:105,:122), so scaling that is the
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
			println( "CoDOL zm DIAG damage notify #" + level.zm_diag_damage +
				": amount=" + diag_val( amount ) +
				" attacker=" + diag_ent( attacker ) +
				" direction=" + diag_val( direction ) +
				" point=" + diag_val( point ) +
				" mod=" + diag_val( mod ) +
				" modelName=" + diag_val( modelName ) +
				" tagName=" + diag_val( tagName ) +
				" partName=" + diag_val( partName ) );
		}

		if ( !isDefined( attacker ) || !isPlayer( attacker ) )
			continue;

		self.attacker = attacker;
		self.damagemod = mod;
		self.damagelocation = partName;
		// CoDOL equipment kill hook: the killing hit's weapon (10th notify
		// argument, codol_equipment_lifecycle.c CODOL_AddDamageWeaponArgument),
		// read by zombie_death_event for level.zombie_kill_weapon_callbacks.
		self.damageweapon = weapon;

		// BEGIN tesla hook -- T5 _zombiemode_spawner.gsc:3548 zombie_damage:
		// a tesla projectile hit starts the chain lightning (the weapon's
		// real damage) and pays no damage points.  Threaded on level because
		// this watcher ends with this zombie, the chain's first kill.
		if ( maps\mp\gametypes\_zombiemode_weap_tesla::is_tesla_damage( weapon, mod ) )
		{
			level thread maps\mp\gametypes\_zombiemode_weap_tesla::tesla_damage_init( self, partName, point, attacker );
			continue;
		}
		// END tesla hook

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
	node = self pick_entrance_node();
	if ( isDefined( node ) )
	{
		self.first_node = node;
		self.goalradius = 128;
		self setGoalPos( node.origin );
		// T5 waits on "goal" alone (:585), with zombie_assure_node moving
		// the goal to the next closest entrance on "bad_path".  The 20 s
		// give-up this port had here fired on every zombie -- a walk cycle
		// covers the 700-1100 units from a yard spawner to its window in
		// 45-70 s -- and sent it into the chase with the boards still up
		// (docs/CODOL_ZOMBIE_WINDOW_20260921.md).
		self zombie_wait_entrance();
		node = self.first_node;
		self.goalradius = 64;
		self tear_into_building( node );
		self open_barrier_paths( node );
	}

	// T5 zombie_goto_entrance :620 and find_flesh :3934: attack mode and
	// visibility to other AI begin once the entrance has been dealt with.
	self zombie_setup_attack_properties();
	self.ignoreme = false;

	for ( ;; )
	{
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
			self.goalradius = 64;
			continue;
		}

		// T5 find_flesh :3989 / zombie_follow_enemy :4305: a point of interest
		// (cymbal monkey, black hole bomb) outranks flesh and owns the goal
		// for this pass; _zombiemode_poi hands the zombie back when it ends.
		if ( !self maps\mp\gametypes\_zombiemode_poi::zombie_follow_poi() )
		{
			player = closest_valid_player( self.origin );

			if ( isDefined( player ) )
				self setGoalPos( player.origin );
		}

		// T5 :455.  The round raised level.zombie_move_speed since this actor
		// picked its cycle: re-roll the bucket, re-pick the clip and re-scale
		// the translation so all three agree again.  zombie_speed_up() below
		// sets needs_run_update on every live zombie when it fires.
		if ( is_true( self.needs_run_update ) && isDefined( self.a ) )
		{
			self set_zombie_run_cycle();
			self apply_move_speed_scale();
			// set_zombie_run_cycle raises the flag again (T5 :3699 does the
			// same), so it is cleared after the call, not before.
			self.needs_run_update = false;
		}

		wait( 1 );
	}
}

// T5 zombie_goto_entrance :601-:607: once the barrier's chunks are all gone
// the clip is dropped out of the way (disable_trigger) and the window's
// negotiation link is reconnected (native Path_ConnectPaths), so the
// interior route can cross it; the native mover then plays the traversal at
// the begin node (maps\mp\animscripts\zombie_jump).  Idempotent.
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
	codolprintln( "CoDOL zm: ent " + self getEntityNumber() + " opened the window at " + node.origin + " (clip dropped, paths connected)" );
}

// T5 zombie_think :448-:556: the entrance choice.
pick_entrance_node()
{
	if ( !isDefined( level.exterior_goals ) || level.exterior_goals.size < 1 )
		return undefined;

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

// T5 maps\_utility::get_array_of_closest, for the exterior_goal structs.
closest_exterior_goals( origin, n )
{
	pool = [];
	for ( i = 0; i < level.exterior_goals.size; i++ )
		pool[ pool.size ] = level.exterior_goals[ i ];

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
// those is unreachable does the zombie give up (T5 kills it; here it falls
// through to the chase, which the cut window links keep honest).
zombie_wait_entrance()
{
	self endon( "death" );

	for ( ;; )
	{
		result = self zombie_wait_goal_or_bad_path();
		if ( result == "goal" )
			return;

		next = self next_entrance_node();
		if ( !isDefined( next ) )
		{
			codolprintln( "CoDOL zm: ent " + self getEntityNumber() + " found no reachable entrance from " + self.origin + "; giving up on the entrance walk" );
			return;
		}

		codolprintln( "CoDOL zm: ent " + self getEntityNumber() + " bad_path to entrance " + self.first_node.origin + "; trying " + next.origin );
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

		println( "CoDOL zm DIAG death notify #" + level.zm_diag_death +
			": arg attacker=" + diag_ent( attacker ) +
			" cached .attacker=" + diag_ent( cached ) +
			" total killed=" + level.total_zombies_killed );
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
		attacker maps\mp\gametypes\_zombiemode_score::player_add_points( "death", mod, hit_location );

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
		level thread maps\mp\gametypes\_zombiemode_powerups::powerup_drop( zombie.origin, is_true( zombie.ignoreme ) );

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
			level.zombies[ i ] apply_move_speed_scale();
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
				codolprintln( "CoDOL zm: ent " + self getEntityNumber() + " tear clip ended without its board notetrack; removing board " + chunk.script_noteworthy + " at the end instead" );
				self.lastchunk_destroy_time = getTime();
				maps\mp\gametypes\_zombiemode_blockers::remove_chunk( chunk, node, true, self );
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
				maps\mp\gametypes\_zombiemode_blockers::remove_chunk( chunk, node, true, self );
			}
			continue;
		}

		self maps\mp\animscripts\zombie_shared::HandleNoteTrack( notetrack, msg );
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
	self tear_into_building_inner( node );
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
		codolprintln( "CoDOL zm: ent " + self getEntityNumber() + " did not reach attack spot " + self.attacking_spot + " from " + self.origin + " in 15 s" );
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

		codolprintln( "CoDOL zm: ent " + self getEntityNumber() + " tearing board " + chunk.script_noteworthy + " at " + chunk.origin + " (barrier " + node.origin + ")" );

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
			codolprintln( "CoDOL zm: ent " + self getEntityNumber() + " barrier " + node.origin + " is open" );
			self stopanimscripted();
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

	self.at_spot_goal = false;
	self thread flag_on_goal();

	for ( ;; )
	{
		if ( self.at_spot_goal || distanceSquared( self.origin, spot ) < 1024 )
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
				// T5 :471 animScripted( "window_melee", self.origin, self.angles, reach, "normal", undefined, 1, 0.3 ).
				self animscripted( "windowattack", self.origin, self.angles, reach, "normal", %body, self.animplaybackrate, 0.3 );
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
	self maps\mp\animscripts\zombie_shared::DoNoteTracksForTime( getAnimLength( taunt ), "tauntanim" );
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
