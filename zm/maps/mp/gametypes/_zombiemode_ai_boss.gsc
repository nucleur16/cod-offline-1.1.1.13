// zm boss: the Juggernaut, CoDOL's minigun-armed heavy mech zombie.
//
// ================================ EVIDENCE ==================================
//
// Everything this file names ships in C1 1.1.1.13 (docs/zone_assets indexes,
// strings of the inflated zones in %TEMP%\codol_zones, measured 2026-10-07):
//
//   models   c_zom_juggernaut_body1 + c_zom_juggernaut_head, all three zm MAP
//            zones.  The body is the standard 73-bone c_zom skeleton with no
//            gun bone; the head adds tag_boss_eye (a bone no other zombie
//            head has).  No gib set (docs/CODOL_ACTOR_SPECIES_20260920.md
//            4.1: that is how the art marks a boss).
//   gun      t5_weapon_carry_minigun_world (common_gametype_zm), minigun_zm's
//            world model: tag_weapon j_barrel tag_brass tag_clip tag_flash.
//            minigun_zm is a bullet weapon (type 0), 50 damage.
//   spawner  every zm map authors one actor_zombie_juggernaut spawner,
//            targetname "main_juggernaut_spawner", model
//            c_zom_juggernaut_body1 (zm_zombie_training (1236 512 68),
//            zm_prototype_sh the same, zm_asylum_sh (-26 1307 85) beside the
//            dog spawner).  C1 never spawned it natively (alpha0022
//            actors_zombies.md 1.6): the round script did.
//   anims    common_zombie.animlist, "=generic_human" group, and the shipped
//            generic_human.atr nodes beside IW4's Juggernaut_* set:
//            juggernaut_walkf_zm, juggernaut_runf_zm, juggernaut_sprint_zm,
//            juggernaut_stand_idle_zm, juggernaut_stand_fire_burst_zm,
//            juggernaut_stand_reload_zm, juggernaut_aim5_zm, juggernaut_scream,
//            juggernaut_flash_react, juggernaut_trap_through (plus
//            exposed_aim_2/4/6/8_zm).  There is NO juggernaut pain or death
//            clip; T5's ai_zombie_boss_death* set is not shipped either.
//   sounds   zmb_juggernaut_spawn / _hunt / _taunt / _step / _death /
//            _gatling_spinup / _gatling_fire (zmb_juggernaut_fire_loop.wav) /
//            _gatling_spindown, mus_zombie_bossbattle (common_gametype_zm).
//   fx       zombie/zm_eye_lensflare_blue_boss (the boss's eye, sibling of the
//            round zombies' zm_eye_lensflare_blue), muzzleflashes/minigun_flash,
//            weapon/shellejects/fx_minigun.
//   text     ZOMBIE_WARNING_JUGGERNAUT_ARRIVED "warning: the heavy mech zombie
//            is about to arrive" (localized_code_post_gfx_mp, and in the
//            zm_prototype_sh / zm_zombie_training zones).
//            Splash row extra_boss_damage (code_post_gfx_mp splash table, icon
//            award_damagedone): SPLASHES_ZBOSSDMG "priority target: heavy mech
//            zombie" / _DESC "dealt extra damage to the heavy mech zombie".
//            CHALLENGE_ZM_MINIGUN_DIE "downed by a mech zombie while raising
//            the minigun".
//   tuning   mp/zombiemode.csv (common_gametype_zm + patch) still carries T5's
//            boss rows ape_min_health_solo / ape_min_health_coop /
//            ape_health_multiplier: the only boss health data C1 ships.
//
// NOT SHIPPED, so not invented: no boss health bar menu or dvar, no aitype /
// character asset (CoDOL ships no such xasset type), no AI-only minigun
// WeaponDef, and no record of WHEN C1 sent the juggernaut in.
//
// ================================ DESIGN ====================================
//
// Behaviour is T5's boss AI (maps\_zombiemode_ai_ape.gsc, the only T5 zombies
// boss whose tuning rows CoDOL kept) with the minigun of the shipped clips:
//
//   * appears DURING a round (it does not replace the round, so the round flow
//     and dog rounds other modules own are untouched): from round
//     scr_zm_boss_first_round, every scr_zm_boss_interval rounds, a few seconds
//     after the round starts; never on a dog round; at most one alive.
//   * spawns inside the playable area at a window's landing point away from
//     the players, screams (juggernaut_scream) and walks at them
//     (juggernaut_walkf_zm, runf_zm when far).
//   * in range and in sight it stops, spins the barrels up
//     (zmb_juggernaut_gatling_spinup), fires a burst
//     (juggernaut_stand_fire_burst_zm over its full-body idle) and spins down.
//     Each round is checked against cover before magicBullet( minigun_zm )
//     fires from the gun's tag_flash, i.e. a real Bullet_Fire round with
//     impacts and the EV_FIRE_WEAPON event every client
//     sees, plus the shipped muzzle flash on tag_flash and the boss's own
//     fire loop.  _zombiemode::Callback_PlayerDamage turns each round that
//     hits a player into this boss's hit (scr_zm_boss_bullet_damage).
//   * armour: everything but the head (the c_zom_juggernaut_head model, or a
//     head bone) does scr_zm_boss_armor_scale of its damage; the head takes
//     full damage.  Health is T5's ape rule over C1's table rows, scaled by
//     round and player count.  Nuke and tesla only chip it (T5 ape
//     nuke_damage_func / tesla_damage_func).
//   * melee: within reach the native combat animscript swings, as any zombie
//     does, for scr_zm_boss_melee_damage.
//   * death: zmb_juggernaut_death, zombie death clip and ragdoll, the round's
//     kill accounting (_zombiemode_spawner::zombie_death_event), a guaranteed
//     power-up (scr_zm_boss_reward, default full_ammo) and the
//     extra_boss_damage splash for every player who did a real share.
//
// Dvars (all optional; "" keeps the default):
//   scr_zm_boss_enable          1      0 turns the boss off
//   scr_zm_boss_first_round     5      first round it can appear
//   scr_zm_boss_interval        5      rounds between appearances
//   scr_zm_boss_last_round      0      0 = no limit
//   scr_zm_boss_spawn_delay     12     seconds into the round
//   scr_zm_boss_health          0      fixed health; 0 = computed
//   scr_zm_boss_armor_scale     0.35   non-head damage factor
//   scr_zm_boss_bullet_damage   5      one minigun round on a player
//   scr_zm_boss_melee_damage    90
//   scr_zm_boss_fire_range      1400
//   scr_zm_boss_reward          full_ammo
// Developer verbs (developer >= 1), dvar scr_zm_boss_dev:
//   spawn [client]   spawn the boss now (near the client when one is given)
//   status           health, state, position
//   fire <client>    make the boss fire one burst at the client
//   kill             kill the boss as world (no reward)
//   hurt <dmg> [head]  damage the boss as the first player (body or head)
//   shoot <client> <head|body> [weapon]  magicBullet from the client into the boss

#include maps\mp\gametypes\_zombiemode_utility;

#using_animtree ( "generic_human" );

BOSS_BODY = "c_zom_juggernaut_body1";
BOSS_HEAD = "c_zom_juggernaut_head";
BOSS_GUN_MODEL = "t5_weapon_carry_minigun_world";
BOSS_GUN_TAG = "tag_weapon_right";
BOSS_MUZZLE_TAG = "tag_flash";
BOSS_WEAPON = "minigun_zm";

init()
{
	level.zm_boss = undefined;
	level.zm_boss_kills = 0;
	level.zm_boss_ready = false;

	precacheModel( BOSS_BODY );
	precacheModel( BOSS_HEAD );
	precacheModel( BOSS_GUN_MODEL );
	precacheItem( BOSS_WEAPON );
	precacheString( &"ZOMBIE_WARNING_JUGGERNAUT_ARRIVED" );

	if ( getWeaponIndexFromName( BOSS_WEAPON ) <= 0 )
	{
		codolprintln( "CoDOL zm boss: " + BOSS_WEAPON + " is not mounted; the juggernaut is disabled" );
		return;
	}

	level._effect[ "zm_boss_eye" ] = loadfx( "zombie/zm_eye_lensflare_blue_boss" );
	level._effect[ "zm_boss_muzzle" ] = loadfx( "muzzleflashes/minigun_flash" );
	level._effect[ "zm_boss_shells" ] = loadfx( "weapon/shellejects/fx_minigun" );

	// C1's boss rows in mp/zombiemode.csv (T5's ape tuning, kept by CoDOL).
	set_zombie_var( "ape_min_health_solo", 8000 );
	set_zombie_var( "ape_min_health_coop", 14000 );
	set_zombie_var( "ape_health_multiplier", 7, true );

	level.zm_boss_player_damage_func = ::boss_player_damage;
	level.zm_boss_ready = true;

	level thread boss_round_watch();
	level thread boss_dev_watch();

	codolprintln( "CoDOL zm boss: juggernaut ready (first round " + boss_dvar_int( "scr_zm_boss_first_round", 5 ) +
		", every " + boss_dvar_int( "scr_zm_boss_interval", 5 ) + ")" );
}

boss_dvar_int( name, def )
{
	v = getDvar( name );
	if ( v == "" )
		return def;
	return int( v );
}

boss_dvar_float( name, def )
{
	v = getDvar( name );
	if ( v == "" )
		return def;
	return float( v );
}

boss_dvar_string( name, def )
{
	v = getDvar( name );
	if ( v == "" )
		return def;
	return v;
}

// --------------------------------------------------------------------------
// When.  T5 ape_round_tracker picks a round 7-9 and then every 5-8; here the
// schedule is a dvar so the round-cap / extraction work can place it.
// --------------------------------------------------------------------------

boss_round_watch()
{
	level endon( "end_game" );

	for ( ;; )
	{
		level waittill( "start_of_round" );

		if ( !boss_round_due( level.round_number ) )
			continue;

		level thread boss_round_spawn( level.round_number );
	}
}

boss_round_due( round )
{
	if ( boss_dvar_int( "scr_zm_boss_enable", 1 ) <= 0 )
		return false;

	first = boss_dvar_int( "scr_zm_boss_first_round", 5 );
	interval = boss_dvar_int( "scr_zm_boss_interval", 5 );
	last = boss_dvar_int( "scr_zm_boss_last_round", 0 );

	if ( round < first )
		return false;
	if ( last > 0 && round > last )
		return false;
	if ( interval < 1 )
		return round == first;
	return ( ( round - first ) % interval ) == 0;
}

// A dog round belongs to the hounds (_zombiemode dog rounds, T5's
// "dog_round" flag); is_dog_round covers a level that keeps a field instead.
boss_is_dog_round()
{
	if ( flag_exist( "dog_round" ) && flag( "dog_round" ) )
		return true;
	if ( is_true( level.dog_round ) || is_true( level.zm_dog_round ) )
		return true;
	return false;
}

boss_round_spawn( round )
{
	level endon( "end_game" );
	level endon( "end_of_round" );

	wait( boss_dvar_float( "scr_zm_boss_spawn_delay", 12 ) );

	if ( boss_is_dog_round() )
	{
		codolprintln( "CoDOL zm boss: round " + round + " is a dog round; the juggernaut waits" );
		return;
	}

	if ( is_true( level.intermission ) || is_true( level.gameEnded ) )
		return;

	boss_spawn();
}

// --------------------------------------------------------------------------
// Where.  Inside the playable area: the landing point of an open zone's
// window (the node_negotiation_end a zombie climbs onto), the one whose
// nearest player is closest to 900 units, so it arrives in view but not on
// top of anyone.  T5 ape_zombie_default_spawn_heuristic scores its spawners
// by player distance the same way.
// --------------------------------------------------------------------------

boss_spawn_candidates( include_spawners )
{
	points = [];

	if ( isDefined( level.exterior_goals ) )
	{
		for ( i = 0; i < level.exterior_goals.size; i++ )
		{
			goal = level.exterior_goals[ i ];
			if ( !maps\mp\gametypes\_zombiemode_zone_manager::entrance_available( goal ) )
				continue;
			inside = maps\mp\gametypes\_zombiemode_zone_manager::entrance_inside_point( goal );
			if ( isDefined( inside ) )
				points[ points.size ] = inside;
		}
	}

	// The round zombies' spawn structs: outside the windows on Prototype and
	// Asylum (a boss there would need a board-free route in), on the playable
	// side on zm_zombie_training's start room.  Used when nothing else is
	// there, and by the stuck recovery, which has to reach the players.
	if ( ( points.size < 1 || is_true( include_spawners ) ) && isDefined( level.enemy_spawns ) )
	{
		for ( i = 0; i < level.enemy_spawns.size; i++ )
			points[ points.size ] = level.enemy_spawns[ i ].origin;
	}

	if ( points.size < 1 )
	{
		authored = getEnt( "main_juggernaut_spawner", "targetname" );
		if ( isDefined( authored ) )
			points[ points.size ] = authored.origin;
	}

	return points;
}

boss_pick_spawn_point( near_player, include_spawners )
{
	points = boss_spawn_candidates( include_spawners );
	if ( points.size < 1 )
		return undefined;

	players = getPlayers();
	best = undefined;
	best_score = 0;
	for ( i = 0; i < points.size; i++ )
	{
		nearest = 999999;
		for ( p = 0; p < players.size; p++ )
		{
			if ( !is_player_valid( players[ p ] ) )
				continue;
			if ( isDefined( near_player ) && players[ p ] != near_player )
				continue;
			d = distance( points[ i ], players[ p ].origin );
			if ( d < nearest )
				nearest = d;
		}
		if ( nearest == 999999 )
			nearest = 900;

		want = 900;
		if ( isDefined( near_player ) )
			want = 500;
		score = abs( nearest - want );
		if ( !isDefined( best ) || score < best_score )
		{
			best = points[ i ];
			best_score = score;
		}
	}
	return best;
}

// --------------------------------------------------------------------------
// The actor.  Built from the round zombie's own spawner (aitype
// enemy_zombie_zm, the generic_human species), then re-dressed: there is no
// juggernaut aitype or character asset to spawn from (CoDOL ships none), and
// the species, tree and animscripts are the zombie's.
// --------------------------------------------------------------------------

boss_spawn( near_player )
{
	if ( !is_true( level.zm_boss_ready ) )
		return undefined;

	if ( isDefined( level.zm_boss ) && isAlive( level.zm_boss ) )
	{
		codolprintln( "CoDOL zm boss: a juggernaut is already alive" );
		return undefined;
	}

	if ( !isDefined( level.zombie_spawner ) )
	{
		codolprintln( "CoDOL zm boss: no zombie spawner; cannot spawn the juggernaut" );
		return undefined;
	}

	origin = boss_pick_spawn_point( near_player );
	if ( !isDefined( origin ) )
	{
		codolprintln( "CoDOL zm boss: no spawn point" );
		return undefined;
	}

	ai = level.zombie_spawner spawnActor();
	if ( !isDefined( ai ) )
	{
		codolprintln( "CoDOL zm boss: spawnactor returned nothing" );
		return undefined;
	}

	yaw = randomInt( 360 );
	target = closest_valid_player( origin );
	if ( isDefined( target ) )
	{
		facing = vectorToAngles( target.origin - origin );
		yaw = facing[ 1 ];
	}
	ai forceTeleport( origin, ( 0, yaw, 0 ) );
	ai show();

	ai boss_setup();

	level.zm_boss = ai;
	level.zombies[ level.zombies.size ] = ai;

	level thread maps\mp\gametypes\_zombiemode_spawner::zombie_death_event( ai );
	level thread boss_death_watch( ai );
	ai thread boss_damage_watch();
	ai thread boss_think();

	codolprintln( "CoDOL zm boss: juggernaut spawned at " + ai.origin + " (ent " + ai getEntityNumber() +
		", " + ai.boss_health_max + " hp, round " + level.round_number + ", " + getPlayers().size + " player(s))" );
	return ai;
}

closest_valid_player( origin )
{
	return maps\mp\gametypes\_zombiemode_spawner::closest_valid_player( origin );
}

boss_setup()
{
	// The body.  character_zm_zombie attached a round zombie's head and set
	// its gib models; a juggernaut has neither.
	if ( isDefined( self.headModel ) )
		self detach( self.headModel, "" );
	self setModel( BOSS_BODY );
	self attach( BOSS_HEAD, "", true );
	self.headModel = BOSS_HEAD;
	self attach( BOSS_GUN_MODEL, BOSS_GUN_TAG );
	self.boss_gun_model = BOSS_GUN_MODEL;

	self.torsoDmg1 = undefined;
	self.torsoDmg2 = undefined;
	self.torsoDmg3 = undefined;
	self.torsoDmg4 = undefined;
	self.torsoDmg5 = undefined;
	self.legDmg1 = undefined;
	self.legDmg2 = undefined;
	self.legDmg3 = undefined;
	self.legDmg4 = undefined;
	self.no_gib = true;
	self.gibbed = false;
	self.head_gibbed = false;
	self.no_eye_glow = true;

	self.targetname = "zombie_boss";
	self.animname = "zombie";
	self.is_zombie = true;
	self.is_boss = true;
	self.zm_path_phase = "hunt";
	if ( !isDefined( self.a ) )
		self.a = spawnStruct();
	self.has_legs = true;
	self.a.pose = "stand";

	// T5 ape_prespawn: no pain flinches, no dodging, no suppression.
	self.allowpain = false;
	self.a.disablePain = true;
	self.ignoreSuppression = true;
	self.suppressionThreshold = 1;
	self.noDodgeMove = true;
	self.disableArrivals = true;
	self.disableExits = true;
	self.grenadeawareness = 0;
	self.badplaceawareness = 0;
	self.dropweapon = false;
	self.allowdeath = true;

	self.ignoreall = false;
	self.ignoreme = false;
	self.pathEnemyFightDist = 64;
	self.meleeAttackDist = 64;
	self.goalradius = 96;

	self.meleeDamage = boss_dvar_int( "scr_zm_boss_melee_damage", 90 );
	if ( isDefined( level.zombie_melee_weapon ) )
		self.aiweapon = level.zombie_melee_weapon;

	// Health.  T5 ape_zombie_think :811-:847: the round zombie's health times
	// ape_zombie_health_mult, floored at the min health (C1's two rows), +50%
	// per extra player, doubled for each juggernaut already killed.
	players = getPlayers().size;
	if ( players < 1 )
		players = 1;
	health = boss_dvar_int( "scr_zm_boss_health", 0 );
	if ( health <= 0 )
	{
		mult = level.zombie_vars[ "ape_health_multiplier" ];
		if ( !isDefined( mult ) || mult <= 0 )
			mult = 7;
		health = int( level.zombie_health * mult );
		floor_health = level.zombie_vars[ "ape_min_health_solo" ];
		if ( players > 1 )
			floor_health = level.zombie_vars[ "ape_min_health_coop" ];
		if ( health < floor_health )
			health = floor_health;
		if ( players > 2 )
			health = int( health * ( 1 + 0.25 * ( players - 2 ) ) );
		for ( i = 0; i < level.zm_boss_kills && i < 3; i++ )
			health = health * 2;
	}
	self.boss_hp = health;
	self.boss_health_max = health;
	self boss_sync_health();
	self.maxhealth = self.health;
	self.boss_damage_by = [];
	self.boss_damage_since_stagger = 0;

	// The move cycle zombie_move.gsc knobs (current_run_anim).
	self boss_set_move_anim( %juggernaut_walkf_zm );
	self.animplaybackrate = 1;
	self.moveplaybackrate = 1;
	self.needs_run_update = false;
	self.zombie_move_speed = "walk";

	// The death.  zombie_death.gsc runs deathFunction, then this clip.
	self.deathanim = %ai_zombie_death_v1;
	self.deathFunction = ::boss_death_function;
	self.allow_ragdoll = true;

	// The idle zombie_stop.gsc holds when the boss is not moving.
	self.idle_anim_override = %juggernaut_stand_idle_zm;

	// T5 ape: nuke and tesla only hurt the boss.
	self.nuke_damage_func = ::boss_nuke_damage;
	self.tesla_damage_func = ::boss_tesla_damage;
	self.instakill_func = ::boss_instakill;

	self setAnimKnobAll( %juggernaut_stand_idle_zm, %body, 1, 0.1, 1 );

	self boss_eye_on();
	self thread boss_ambient_sounds();
}

boss_set_move_anim( animRef )
{
	self.run_combatanim = animRef;
	self.run_noncombatanim = animRef;
	self.walk_combatanim = animRef;
	self.walk_noncombatanim = animRef;
	self.a.combatrunanim = animRef;
}

boss_eye_on()
{
	if ( isDefined( level._effect[ "zm_boss_eye" ] ) )
		playFxOnTag( level._effect[ "zm_boss_eye" ], self, "tag_boss_eye" );
	self.boss_eye_on = true;
}

boss_eye_off()
{
	if ( !is_true( self.boss_eye_on ) )
		return;
	self.boss_eye_on = false;
	if ( isDefined( level._effect[ "zm_boss_eye" ] ) )
		stopFxOnTag( level._effect[ "zm_boss_eye" ], self, "tag_boss_eye" );
}

boss_ambient_sounds()
{
	self endon( "death" );

	for ( ;; )
	{
		wait( randomFloatRange( 6, 11 ) );
		if ( is_true( self.boss_firing ) )
			continue;
		if ( randomInt( 3 ) == 0 )
			self playSound( "zmb_juggernaut_taunt" );
		else
			self playSound( "zmb_juggernaut_hunt" );
	}
}

// --------------------------------------------------------------------------
// The arrival.  ZOMBIE_WARNING_JUGGERNAUT_ARRIVED to everyone, the spawn
// roar, the boss-battle music and a scream before it moves.
// --------------------------------------------------------------------------

boss_announce()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		players[ i ] iPrintLnBold( &"ZOMBIE_WARNING_JUGGERNAUT_ARRIVED" );
		if ( soundExists( "mus_zombie_bossbattle" ) )
			players[ i ] playLocalSound( "mus_zombie_bossbattle" );
	}
	level.zm_boss_music = soundExists( "mus_zombie_bossbattle" );
}

boss_music_stop()
{
	if ( !is_true( level.zm_boss_music ) )
		return;
	level.zm_boss_music = false;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] stopLocalSound( "mus_zombie_bossbattle" );
}

// --------------------------------------------------------------------------
// The brain.  T5 ape_zombie_think / ape_find_flesh: hunt the closest valid
// player; here, when in range and in sight, stop and fire.
// --------------------------------------------------------------------------

boss_think()
{
	self endon( "death" );

	boss_announce();
	self playSound( "zmb_juggernaut_spawn" );
	earthquake( 0.4, 1.5, self.origin, 900 );
	self boss_play_scripted( "bossscream", %juggernaut_scream, undefined );

	self.boss_next_fire = getTime() + 2000;
	self thread boss_stuck_watch();

	for ( ;; )
	{
		player = closest_valid_player( self.origin );
		if ( !isDefined( player ) )
		{
			wait( 0.5 );
			continue;
		}

		dist = distance( self.origin, player.origin );

		if ( self boss_should_stagger() )
		{
			self boss_play_scripted( "bossstagger", %juggernaut_flash_react, player );
			continue;
		}

		if ( getTime() >= self.boss_next_fire && dist > 96 &&
			dist <= boss_dvar_float( "scr_zm_boss_fire_range", 1400 ) && self boss_can_see( player ) )
		{
			self boss_fire_burst( player );
			self.boss_next_fire = getTime() + randomIntRange( 2500, 4500 );
			continue;
		}

		// Walk when close, lumber faster when far (juggernaut_runf_zm).
		if ( dist > 1000 && self.zombie_move_speed != "run" )
		{
			self.zombie_move_speed = "run";
			self boss_set_move_anim( %juggernaut_runf_zm );
		}
		else if ( dist <= 1000 && self.zombie_move_speed != "walk" )
		{
			self.zombie_move_speed = "walk";
			self boss_set_move_anim( %juggernaut_walkf_zm );
		}

		self.goalradius = 96;
		self setGoalPos( player.origin );

		if ( self.origin[ 2 ] < level.zombie_vars[ "below_world_check" ] )
		{
			codolprintln( "CoDOL zm boss: fell out of the world at " + self.origin );
			self.no_powerup_drop = true;
			self.boss_no_reward = true;
			self.boss_dying = true;
			self doDamage( self.health + 100, self.origin );
			return;
		}

		wait( 0.25 );
	}
}

// T5 round_spawn_failsafe for the boss, without the kill: a juggernaut that
// has made no progress for 20 s while not firing (the native route ended at a
// graph component the players are not in, as zm_zombie_training's final room
// is, or it is wedged) is moved to the window landing point nearest the
// closest player and hunts from there.
boss_stuck_watch()
{
	self endon( "death" );

	last = self.origin;
	for ( ;; )
	{
		wait( 20 );
		if ( is_true( self.boss_firing ) || is_true( self.boss_dying ) )
		{
			last = self.origin;
			continue;
		}
		player = closest_valid_player( self.origin );
		if ( !isDefined( player ) )
			continue;
		if ( distanceSquared( self.origin, last ) < 96 * 96 && distance( self.origin, player.origin ) > 300 )
		{
			spot = boss_pick_spawn_point( player, true );
			if ( isDefined( spot ) && distanceSquared( spot, self.origin ) > 128 * 128 )
			{
				codolprintln( "CoDOL zm boss: no progress at " + self.origin + " toward " + player.name +
					"; moving to " + spot );
				facing = vectorToAngles( player.origin - spot );
				self forceTeleport( spot, ( 0, facing[ 1 ], 0 ) );
			}
		}
		last = self.origin;
	}
}

boss_can_see( player )
{
	start = self boss_muzzle_origin();
	end = player getEye();
	return sightTracePassed( start, end, false, self );
}

boss_should_stagger()
{
	if ( !isDefined( self.boss_health_max ) || self.boss_health_max <= 0 )
		return false;
	if ( self.boss_damage_since_stagger < self.boss_health_max * 0.25 )
		return false;
	self.boss_damage_since_stagger = 0;
	return true;
}

// One clip in the native scripted state (the same one the Cortex taunt and
// the board tear use), facing the player when one is given.
boss_play_scripted( flagName, animRef, face )
{
	self endon( "death" );

	yaw = self.angles[ 1 ];
	if ( isDefined( face ) )
	{
		facing = vectorToAngles( face.origin - self.origin );
		yaw = facing[ 1 ];
	}
	self animscripted( flagName, self.origin, ( 0, yaw, 0 ), animRef, "normal", %body, 1, 0.2 );
	wait( getAnimLength( animRef ) );
	self stopanimscripted();
}

// --------------------------------------------------------------------------
// The minigun.
// --------------------------------------------------------------------------

boss_muzzle_origin()
{
	if ( isDefined( self.boss_gun_model ) )
		return self getTagOrigin( BOSS_MUZZLE_TAG );
	return self getTagOrigin( "tag_weapon_right" );
}

// The barrels spin up, the additive burst plays over the idle pose, and each
// round is traced before firing. The barrels spin down afterward.
boss_fire_burst( target )
{
	self endon( "death" );

	self.boss_firing = true;
	self.boss_fire_target = target;
	self.boss_fire_time = getTime();
	self setGoalPos( self.origin );

	// Spin-up: the shipped one-shot, on the aim pose.
	yaw = vectorToAngles( target.origin - self.origin );
	self animscripted( "bossspin", self.origin, ( 0, yaw[ 1 ], 0 ), %juggernaut_stand_idle_zm, "normal", %body, 1, 0.2 );
	self playSound( "zmb_juggernaut_gatling_spinup" );
	wait( 0.8 );

	burst_end = getTime() + randomIntRange( 2200, 3400 );
	clip_len = getAnimLength( %juggernaut_stand_fire_burst_zm );
	if ( clip_len < 0.3 )
		clip_len = 0.3;
	clip_end = 0;
	shots = 0;

	self playLoopSound( "zmb_juggernaut_gatling_fire" );
	// The mounted generic_human tree puts this clip under add_fire : additive.
	// Keep the scripted full-body idle alive beneath it for the whole burst.
	self setAnim( %add_fire, 1, 0.1, 1 );

	while ( getTime() < burst_end )
	{
		if ( !is_player_valid( target ) )
			break;
		if ( !self boss_can_see( target ) )
			break;

		now = getTime();
		if ( now >= clip_end )
		{
			self setAnimKnobRestart( %juggernaut_stand_fire_burst_zm, 1, 0.05, 1 );
			clip_end = now + int( clip_len * 1000 );
		}
		self orientmode( "face point", target.origin );

		if ( self boss_fire_round( target, shots ) )
			shots++;
		wait( 0.1 );
	}

	self stopLoopSound();
	self playSound( "zmb_juggernaut_gatling_spindown" );
	self clearAnim( %add_fire, 0.1 );
	self stopanimscripted();

	self.boss_firing = false;
	self.boss_fire_target = undefined;
	self.boss_fire_time = getTime();
	self.boss_shots_total = self boss_shots_total_safe() + shots;

	if ( getDvarInt( "developer" ) > 0 )
		codolprintln( "CoDOL zm boss: burst at " + target.name + " " + shots + " rounds, " +
			int( distance( self.origin, target.origin ) ) + " units" );
}

boss_shots_total_safe()
{
	if ( !isDefined( self.boss_shots_total ) )
		return 0;
	return self.boss_shots_total;
}

boss_fire_round( target, shot )
{
	start = self boss_muzzle_origin();
	aim = target getEye() - ( 0, 0, 12 );

	// A minigun sprays: spread grows with range, about 4.5 degrees.
	dist = distance( start, aim );
	spread = dist * 0.08;
	aim = aim + ( randomFloatRange( 0 - spread, spread ), randomFloatRange( 0 - spread, spread ),
		randomFloatRange( 0 - spread * 0.5, spread * 0.5 ) );

	// Start a little down the barrel line so the round clears the boss's own
	// hit boxes (Bullet_Fire with no weapon entity ignores nothing).
	dir = vectorNormalize( aim - start );
	start = start + vector_scale( dir, 20 );
	// Test the exact bullet path before Bullet_Fire's penetration pass can
	// carry a minigun round through cover to the player.
	trace = bulletTrace( start, aim, true, self );
	if ( trace["fraction"] < 1 &&
		( !isDefined( trace["entity"] ) || trace["entity"] != target ) )
		return false;

	magicBullet( BOSS_WEAPON, start, aim );

	if ( shot % 2 == 0 && isDefined( level._effect[ "zm_boss_muzzle" ] ) && isDefined( self.boss_gun_model ) )
		playFxOnTag( level._effect[ "zm_boss_muzzle" ], self, BOSS_MUZZLE_TAG );
	if ( shot % 4 == 0 && isDefined( level._effect[ "zm_boss_shells" ] ) && isDefined( self.boss_gun_model ) )
		playFxOnTag( level._effect[ "zm_boss_shells" ], self, "tag_brass" );

	if ( !isDefined( level.zm_boss_rounds_fired ) )
		level.zm_boss_rounds_fired = 0;
	level.zm_boss_rounds_fired++;
	return true;
}

// _zombiemode::Callback_PlayerDamage asks this about every hit a player takes.
// magicBullet has no owner to give (an explicit attacker must be a player),
// so a juggernaut round arrives with the world as attacker, a bullet means of
// death and no weapon name.  Answers the boss that fired it, or undefined.
boss_player_damage( eAttacker, sWeapon, sMeansOfDeath )
{
	// MEASURED 2026-10-07 (probe): Bullet_Fire with the world as owner and
	// no weapon entity reports the weapon as "none", MOD_RIFLE_BULLET.
	if ( !isDefined( sWeapon ) || ( sWeapon != BOSS_WEAPON && sWeapon != "none" ) )
		return undefined;
	if ( !isDefined( sMeansOfDeath ) || ( sMeansOfDeath != "MOD_RIFLE_BULLET" && sMeansOfDeath != "MOD_PISTOL_BULLET" ) )
		return undefined;
	if ( isDefined( eAttacker ) && ( isPlayer( eAttacker ) || is_true( eAttacker.is_zombie ) ) )
		return undefined;
	boss = level.zm_boss;
	if ( !isDefined( boss ) || !isAlive( boss ) )
		return undefined;
	if ( !is_true( boss.boss_firing ) && ( !isDefined( boss.boss_fire_time ) || getTime() - boss.boss_fire_time > 500 ) )
		return undefined;
	boss.boss_minigun_damage = boss_dvar_int( "scr_zm_boss_bullet_damage", 5 );
	if ( !isDefined( level.zm_boss_player_hits ) )
		level.zm_boss_player_hits = 0;
	level.zm_boss_player_hits++;
	if ( getDvarInt( "developer" ) > 0 && level.zm_boss_player_hits <= 10 )
		codolprintln( "CoDOL zm boss: minigun round hit a player (" + diag_val( sWeapon ) + ", " + sMeansOfDeath +
			") -> " + boss.boss_minigun_damage + " damage from ent " + boss getEntityNumber() );
	return boss;
}

// --------------------------------------------------------------------------
// Damage.  The zm tree defines no CodeCallback_ActorDamage, so the hit has
// already landed when the "damage" notify arrives; the armour is applied to
// the script-side health pool below.  Only players hurt the boss (T5 ape
// non_attacker_func): its own rounds, other zombies and the world are given
// back in full.
// --------------------------------------------------------------------------

boss_is_head_hit( modelName, partName )
{
	if ( isDefined( modelName ) && modelName == BOSS_HEAD )
		return true;
	if ( !isDefined( partName ) )
		return false;
	switch ( partName )
	{
	case "j_head":
	case "j_neck":
	case "j_helmet":
	case "j_head_end":
	case "tag_eye":
	case "tag_boss_eye":
	case "j_eyeball_le":
	case "j_eyeball_ri":
	case "j_jaw":
		return true;
	}
	return false;
}

// The boss's real health is self.boss_hp, kept in script.  The engine's
// self.health is held above boss_hp / armour scale, so no armoured hit can
// kill natively before its reduced share is known; boss_hp reaching zero is
// what kills it, through doDamage with the player who landed the hit.
boss_sync_health()
{
	scale = boss_dvar_float( "scr_zm_boss_armor_scale", 0.35 );
	if ( scale < 0.05 )
		scale = 0.05;
	if ( scale > 1 )
		scale = 1;
	self.health = int( self.boss_hp / scale ) + 100;
}

boss_damage_watch()
{
	for ( ;; )
	{
		self waittill( "damage", amount, attacker, direction, point, mod, modelName, tagName, partName, flags, weapon );

		if ( !isDefined( self ) )
			return;
		if ( !isDefined( amount ) || amount <= 0 || is_true( self.boss_dying ) )
			continue;
		if ( !isAlive( self ) || self.health <= 0 )
			continue;

		// Only players hurt the boss: its own rounds, other zombies, traps'
		// touch damage and the world are given back in full.
		if ( !isDefined( attacker ) || !isPlayer( attacker ) )
		{
			self boss_sync_health();
			continue;
		}

		head = boss_is_head_hit( modelName, partName );
		effective = amount;
		if ( !head && mod != "MOD_MELEE" )
			effective = int( amount * boss_dvar_float( "scr_zm_boss_armor_scale", 0.35 ) );
		if ( effective < 1 )
			effective = 1;

		self.attacker = attacker;
		self.damagemod = mod;
		self.damagelocation = partName;
		self.damageweapon = weapon;
		self.boss_damage_since_stagger += effective;

		num = attacker getEntityNumber();
		if ( !isDefined( self.boss_damage_by[ num ] ) )
			self.boss_damage_by[ num ] = 0;
		self.boss_damage_by[ num ] += effective;

		if ( attacker playerADS() > 0.9 )
			attacker maps\mp\gametypes\_zombiemode_score::player_add_points( "damage_ads", mod, partName );
		else
			attacker maps\mp\gametypes\_zombiemode_score::player_add_points( "damage", mod, partName );

		self.boss_hp -= effective;

		if ( getDvarInt( "developer" ) > 0 )
			codolprintln( "CoDOL zm boss: hit by " + attacker.name + " " + amount + " -> " + effective +
				" (" + diag_val( partName ) + ", head " + head + ") health " + self.boss_hp + "/" + self.boss_health_max );

		if ( self.boss_hp <= 0 )
		{
			self boss_kill( attacker, mod, weapon );
			return;
		}
		self boss_sync_health();
	}
}

boss_kill( attacker, mod, weapon )
{
	self.boss_dying = true;
	self.boss_hp = 0;
	if ( !isDefined( mod ) )
		mod = "MOD_RIFLE_BULLET";
	if ( isDefined( weapon ) && weapon != "" && weapon != "none" )
		self doDamage( self.health + 100, self.origin, attacker, attacker, 0, mod, 0, weapon );
	else
		self doDamage( self.health + 100, self.origin, attacker, attacker, 0, mod );
}

// Script-side damage that is not a player's hit (nuke, tesla).
boss_take( amount, attacker, mod )
{
	if ( !isAlive( self ) || is_true( self.boss_dying ) )
		return;
	self.boss_hp -= amount;
	if ( self.boss_hp <= 0 && isDefined( attacker ) && isPlayer( attacker ) )
	{
		self.attacker = attacker;
		self.damagemod = mod;
		self boss_kill( attacker, mod );
		return;
	}
	if ( self.boss_hp < 1 )
		self.boss_hp = 1;
	self boss_sync_health();
}

// T5 ape_nuke_damage: half its health, never the kill.
boss_nuke_damage()
{
	dmg = int( self.boss_hp * 0.5 );
	self boss_take( dmg, undefined, "MOD_UNKNOWN" );
	codolprintln( "CoDOL zm boss: nuke took " + dmg + " (half its health)" );
}

// T5 ape_tesla_damage: a fixed chunk, credited to the arc's player.
boss_tesla_damage( origin, player )
{
	self boss_take( 1000, player, "MOD_UNKNOWN" );
}

// Insta-Kill does not one-shot a boss (T5 ape_actor_damage doubles the hit).
boss_instakill()
{
}

// --------------------------------------------------------------------------
// Death.
// --------------------------------------------------------------------------

// zombie_death.gsc: the boss's death roar and lights out; returning false
// lets the stock death clip and ragdoll play.
boss_death_function()
{
	self stopLoopSound();
	self playSound( "zmb_juggernaut_death" );
	self boss_eye_off();
	return false;
}

boss_death_watch( boss )
{
	boss waittill( "death", attacker );

	level.zm_boss = undefined;
	boss_music_stop();

	// zombie_death_event pays the kill; the boss's reward replaces the
	// ordinary drop roll.
	if ( isDefined( boss ) )
		boss.no_powerup_drop = true;

	waittillframeend;

	if ( !isPlayer( attacker ) && isDefined( boss ) && isDefined( boss.attacker ) && isPlayer( boss.attacker ) )
		attacker = boss.attacker;

	origin = undefined;
	if ( isDefined( boss ) )
		origin = boss.origin;

	no_reward = !isDefined( boss ) || is_true( boss.boss_no_reward ) || !isDefined( attacker ) || !isPlayer( attacker );
	if ( no_reward )
	{
		codolprintln( "CoDOL zm boss: juggernaut removed without a player kill" );
		return;
	}

	level.zm_boss_kills++;
	codolprintln( "CoDOL zm boss: juggernaut killed by " + attacker.name + " (" + diag_val( boss.damagemod ) +
		", " + diag_val( boss.damageweapon ) + ") at " + origin );

	// The reward: a named power-up (T5 ape_zombie_powerup_drop) and the
	// extra_boss_damage splash (C1's SPLASHES_ZBOSSDMG) with a share of
	// points for everyone who did at least 10% of its health.
	reward = boss_dvar_string( "scr_zm_boss_reward", "full_ammo" );
	if ( reward != "none" && isDefined( origin ) )
	{
		if ( !isDefined( level.zombie_powerups ) || !isDefined( level.zombie_powerups[ reward ] ) )
			reward = "full_ammo";
		level thread maps\mp\gametypes\_zombiemode_powerups::specific_powerup_drop( reward, origin );
	}

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		num = players[ i ] getEntityNumber();
		if ( !isDefined( boss.boss_damage_by[ num ] ) )
			continue;
		if ( boss.boss_damage_by[ num ] < boss.boss_health_max * 0.1 && players[ i ] != attacker )
			continue;
		players[ i ] maps\mp\gametypes\_zombiemode_score::add_to_player_score( 500 );
		players[ i ] ShowHudSplash( "extra_boss_damage", 0 );
		if ( getDvarInt( "developer" ) > 0 )
			codolprintln( "CoDOL zm boss: " + players[ i ].name + " did " + boss.boss_damage_by[ num ] + "; extra_boss_damage splash, +500" );
	}
}

// --------------------------------------------------------------------------
// Developer verbs.
// --------------------------------------------------------------------------

boss_dev_watch()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;

	setDvar( "scr_zm_boss_dev", "" );

	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_zm_boss_dev" );
		if ( request == "" )
			continue;
		setDvar( "scr_zm_boss_dev", "" );

		tokens = strTok( request, " " );
		switch ( tokens[ 0 ] )
		{
		case "spawn":
			p = undefined;
			if ( tokens.size > 1 )
				p = boss_dev_player( tokens[ 1 ] );
			level thread boss_spawn( p );
			break;

		case "status":
			boss_dev_status();
			break;

		case "fire":
			b = level.zm_boss;
			p = undefined;
			if ( tokens.size > 1 )
				p = boss_dev_player( tokens[ 1 ] );
			if ( isDefined( b ) && isAlive( b ) && isDefined( p ) )
				b thread boss_fire_burst( p );
			break;

		case "kill":
			b = level.zm_boss;
			if ( isDefined( b ) && isAlive( b ) )
			{
				b.boss_no_reward = true;
				b.no_powerup_drop = true;
				b.boss_dying = true;
				b doDamage( b.health + 100, b.origin );
			}
			break;

		case "shoot":
			// shoot <client> <head|body> [weapon]: a real bullet from the
			// client's eye into the boss (Bullet_Fire, locational trace).
			b = level.zm_boss;
			p = undefined;
			if ( tokens.size > 1 )
				p = boss_dev_player( tokens[ 1 ] );
			if ( isDefined( b ) && isAlive( b ) && isDefined( p ) )
			{
				bone = "j_spineupper";
				if ( tokens.size > 2 && tokens[ 2 ] == "head" )
					bone = "j_head";
				w = p getCurrentWeapon();
				if ( tokens.size > 3 )
					w = tokens[ 3 ];
				magicBullet( w, p getEye(), b getTagOrigin( bone ), p );
			}
			break;

		case "hurt":
			b = level.zm_boss;
			players = getPlayers();
			if ( isDefined( b ) && isAlive( b ) && players.size > 0 && tokens.size > 1 )
			{
				dmg = int( tokens[ 1 ] );
				bone = "j_spineupper";
				if ( tokens.size > 2 && tokens[ 2 ] == "head" )
					bone = "j_head";
				b doDamage( dmg, b getTagOrigin( bone ), players[ 0 ], players[ 0 ], 0, "MOD_RIFLE_BULLET" );
			}
			break;

		default:
			codolprintln( "CoDOL zm boss: unknown verb " + tokens[ 0 ] );
			break;
		}
	}
}

boss_dev_player( token )
{
	num = int( token );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] getEntityNumber() == num )
			return players[ i ];
	}
	return undefined;
}

boss_dev_status()
{
	b = level.zm_boss;
	if ( !isDefined( b ) || !isAlive( b ) )
	{
		codolprintln( "CoDOL zm boss: status: no juggernaut alive (killed " + level.zm_boss_kills +
			", rounds fired " + diag_val( level.zm_boss_rounds_fired ) + ", player hits " + diag_val( level.zm_boss_player_hits ) + ")" );
		return;
	}
	parts = "";
	size = b getAttachSize();
	for ( i = 0; i < size; i++ )
		parts = parts + " " + b getAttachModelName( i );
	codolprintln( "CoDOL zm boss: status: ent " + b getEntityNumber() + " health " + b.boss_hp + "/" + b.boss_health_max + " (engine " + b.health + ")" +
		" model " + b.model + " attached[" + parts + " ] origin " + b.origin + " firing " + is_true( b.boss_firing ) +
		" muzzle " + b boss_muzzle_origin() + " rounds fired " + diag_val( level.zm_boss_rounds_fired ) +
		" player hits " + diag_val( level.zm_boss_player_hits ) + " enemies " + get_enemy_count() );
}
