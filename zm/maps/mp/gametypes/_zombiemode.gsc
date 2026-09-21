// zm core: tuning, callbacks, spawn, and THE ROUND LOOP.
//
// The zm counterpart of the multiplayer _gamelogic/_playerlogic/_damage trio.
// Behaviour reference is T5's maps\_zombiemode*.gsc
// (D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps, 68 files); this file mirrors its
// data access exactly -- set_zombie_var()/get_zombie_var() read
// mp/zombiemode.csv by row name -- and reimplements only the part T5 gets from
// its own engine.
//
// T5 has no zm.gsc-style gametype file at all: _callbackglobal::Callback_Start-
// GameType is empty and the mode is entered from the level script calling
// maps\_zombiemode::main().  CoDOL's engine demands a gametype script
// (GScr_LoadGameTypeScript, src/scr_vm_main.c), so the entry point here is the
// gametype's, and the level script only carries per-map data.
//
// NOTHING IN THIS SCRIPT SET MAY NAME A MULTIPLAYER MODULE.  A `file::function`
// reference is a compile-time dependency: naming maps\mp\_load or
// maps\mp\gametypes\_gamelogic would compile the multiplayer chain and precache
// multiplayer assets against zm zones, which is the whole problem this script
// set exists to remove.  The only #include here is the zm-local utility
// module, whose compile closure is the zm tree and nothing else.
//
// ROUND LOOP PROVENANCE, by T5 function in maps\_zombiemode.gsc:
//   init_levelvars   :649   -- every level.zombie_vars row below.
//   round_start      :3457  -- the grace delay, the round-1 grenade grant,
//                              begin_spawning, and the three overridable
//                              function pointers.
//   round_pause      :3414  -- the countdown before round 1.
//   round_think      :3899  -- THE LOOP: rebuild cap, round start notify,
//                              spawn, wait, end-of-round notify, the
//                              spawn-delay decay, the move-speed ramp and
//                              round_number++.
//   round_wait       :4130
//   award_grenades_for_survivors :3986
//   end_game         :5064
//   onPlayerSpawned  :1598
//   player_damage_override :4544  -- the lethal-hit-becomes-a-down rule.
//
// NOT TAKEN from round_think, and why: change_zombie_music (no zm music
// aliases are registered), powerup_round_start (the power-up drop system is a
// module of its own; the kis_*_zm pickups exist as weapons but nothing drops
// them yet), UploadStats / bbPrint / update_leaderboards (no stats backend),
// spectators_respawn (T5's spectator path needs spawnSpectator, and
// allowSpectateTeam plus a spectator sessionstate is a separate port),
// crawler_round_tracker and the dog rounds (both animation-first).
//
// THE ROUND COUNTER is C1's own: dvar ui_hud_round, drawn at 0x00461A71, and
// MEASURED rendering live under the previous server
// (alpha0022/docs/zombies_gameplay_plan.md:312-321).  T5's chalk-mark HUD
// (create_chalk_hud :3550, chalk_one_up :3641) is therefore NOT ported: it
// SetShaders "hud_chalk_1".."hud_chalk_5", which are WaW/T5 materials that no
// CoDOL zone carries, and C1 already has a round widget that works.

#include maps\mp\gametypes\_zombiemode_utility;

// --------------------------------------------------------------------------
// Gametype init.  Runs from zm.gsc::main(), i.e. at gametype-script load,
// before the level script and before CodeCallback_StartGameType.  Data only:
// no entity may be touched here.
// --------------------------------------------------------------------------

init()
{
	// common_scripts\utility::isSP() infers singleplayer from the map name
	// (!string_starts_with(mapname, "mp_")) because IW4 shipped no multiplayer
	// map that was not mp_*.  zm_* maps are not mp_*.  A dedicated match
	// server is never singleplayer whatever the map is called.
	level.isSP = false;

	// mp_playlists.ff's playlists_allmaps.info has no `teambased` line for zm
	// and mp/gametypescptable.csv places zm in its non-team half.
	level.teamBased = false;
	level.gametype = "zm";
	level.players = [];

	level.zombie_vars = [];
	level.flag = [];

	init_levelvars();
}

// T5 _zombiemode.gsc:649 init_levelvars.  The comment T5 puts above its table
// applies unchanged: "These values are in mp/zombiemode.csv and will override
// whatever you put in as a value below.  However, if they don't exist in the
// file, then the values below will be used."
//
// mp/zombiemode.csv ships in common_gametype_zm.ff, 5 columns x 106 rows,
// NAME | EASY | MEDIUM | HARD | VETERAN; T5 reads difficulty+1, difficulty 1,
// which is the same cell tableLookup(table, 0, name, 1) returns here.
init_levelvars()
{
	level.is_zombie_level = true;
	level.first_round = true;
	level.round_number = 1;
	level.round_start_time = 0;
	level.intermission = false;
	level.zombie_total = 0;
	level.total_zombies_killed = 0;
	level.hudelem_count = 0;
	level.zombie_move_speed = 1;
	level.enemy_spawns = [];
	level.zombies = [];
	level.gameEnded = false;

	column = 1;

	// AI
	set_zombie_var( "zombie_health_increase",            100,  false, column );
	set_zombie_var( "zombie_health_increase_multiplier", 0.1,  true,  column );
	set_zombie_var( "zombie_health_start",               150,  false, column );
	set_zombie_var( "zombie_spawn_delay",                2.0,  true,  column );
	set_zombie_var( "zombie_new_runner_interval",        10,   false, column );
	set_zombie_var( "zombie_move_speed_multiplier",      8,    false, column );
	set_zombie_var( "zombie_max_ai",                     24,   false, column );
	set_zombie_var( "zombie_ai_per_player",              6,    false, column );
	set_zombie_var( "below_world_check",                 -1000 );

	// Round
	set_zombie_var( "zombie_use_failsafe",               1 );
	set_zombie_var( "zombie_between_round_time",         10 );
	set_zombie_var( "zombie_intermission_time",          15 );
	set_zombie_var( "game_start_delay",                  0,    false, column );

	// Life and death
	set_zombie_var( "penalty_no_revive",                 0.10, true,  column );
	set_zombie_var( "penalty_died",                      0.0,  true,  column );
	set_zombie_var( "penalty_downed",                    0.05, true,  column );
	set_zombie_var( "starting_lives",                    1,    false, column );

	// Scoring
	set_zombie_var( "zombie_point_scalar",               1 );
	set_zombie_var( "zombie_score_start_1p",             500,  false, column );
	set_zombie_var( "zombie_score_kill_4player",         50 );
	set_zombie_var( "zombie_score_kill_3player",         50 );
	set_zombie_var( "zombie_score_kill_2player",         50 );
	set_zombie_var( "zombie_score_kill_1player",         50 );
	set_zombie_var( "zombie_score_damage_normal",        10 );
	set_zombie_var( "zombie_score_damage_light",         10 );
	set_zombie_var( "zombie_score_bonus_melee",          80 );
	set_zombie_var( "zombie_score_bonus_head",           50 );
	set_zombie_var( "zombie_score_bonus_neck",           20 );
	set_zombie_var( "zombie_score_bonus_torso",          10 );
	set_zombie_var( "zombie_score_bonus_burn",           10 );
	set_zombie_var( "zombie_powerup_insta_kill_on",      0 );

	// Player
	set_zombie_var( "player_starting_health",            100 );

	flag_init( "all_players_connected" );
	flag_init( "begin_spawning" );
	flag_init( "end_round_wait" );
	flag_init( "power_on" );
}

// --------------------------------------------------------------------------
// Level-script entry point.
//
// GScr_LoadScripts asks for exactly one module, maps/mp/<mapname>, for every
// asset category (src/scr_vm_main.c).  All three zm maps therefore need a file
// under zm/maps/mp/, and each of them calls this.
//
// The VISION NAME is passed in rather than taken from getDvar( "mapname" )
// because the three zm zones disagree.  MEASURED from the shipped fastfiles:
//
//   zm_prototype_sh.ff    sun/zm_prototype_sh.sun    vision/zm_prototype_sh.vision
//   zm_asylum_sh.ff       sun/zm_asylum_sh.sun       vision/zm_asylum_sh.vision
//   zm_zombie_training.ff (neither)
// --------------------------------------------------------------------------

levelMain( visionName )
{
	level.isSP = false;

	if ( isDefined( visionName ) && visionName != "" )
	{
		level.zm_vision = visionName;
		VisionSetNaked( visionName, 0 );
		VisionSetPain( visionName );
	}
}

// The mode's weapon set is named in mp/zombiemode.csv, and CoDOL publishes zm
// weapon configstrings natively before any script runs
// (CODOL_InitializeServerWeaponConfigstrings, src/sv_main.c).
//
// precacheItem() is the probe rather than a guess: it is a silent no-op for a
// weapon that is in no mounted zone (codol_script_precache.c:24-27), and it is
// what registers the definition, so getWeaponIndexFromName() only answers
// non-zero afterwards for a name the zm zones really carry.
resolveStartWeapon()
{
	candidates = [];
	candidates[ 0 ] = tableLookup( zombie_table(), 0, "zombie_starting_weapon", 1 );
	candidates[ 1 ] = "coltanaconda_zm";
	candidates[ 2 ] = "m1911_zm";

	for ( i = 0; i < candidates.size; i++ )
	{
		name = candidates[ i ];
		if ( !isDefined( name ) || name == "" )
			continue;

		precacheItem( name );
		if ( getWeaponIndexFromName( name ) > 0 )
		{
			println( "CoDOL zm: starting weapon " + name );
			return name;
		}
	}

	println( "CoDOL zm: no starting weapon is present in the mounted zm zones" );
	return undefined;
}

// T5 round_start :3486 gives every player a lethal grenade at round 1 and
// award_grenades_for_survivors :3986 tops it up every round.  MEASURED
// (docs/zone_assets/ALL_weapon.txt): frag_grenade_zm and semtex_grenade_zm
// both ship in common_gametype_zm.
resolveLethalGrenade()
{
	candidates = [];
	candidates[ 0 ] = "frag_grenade_zm";
	candidates[ 1 ] = "semtex_grenade_zm";

	for ( i = 0; i < candidates.size; i++ )
	{
		precacheItem( candidates[ i ] );
		if ( getWeaponIndexFromName( candidates[ i ] ) > 0 )
			return candidates[ i ];
	}

	return undefined;
}

// --------------------------------------------------------------------------
// Callbacks
// --------------------------------------------------------------------------

Callback_StartGameType()
{
	level.isSP = false;

	if ( !isDefined( level.players ) )
		level.players = [];

	level.gameEnded = false;
	level.inGracePeriod = false;

	// Resolved here rather than in init() because Callback_StartGameType is the
	// precache window this server is demonstrably in: it is where the
	// multiplayer chain's maps\mp\gametypes\_class::init() precaches every
	// loadout weapon today.
	level.zombie_start_weapon = resolveStartWeapon();
	level.zombie_lethal_grenade = resolveLethalGrenade();

	// Subsystem order matters: last stand first because it reads
	// level.zombie_start_weapon, then the weapon table, then anything that
	// spawns triggers, then the spawner, then the loop.
	maps\mp\gametypes\_zombiemode_laststand::init();
	maps\mp\gametypes\_zombiemode_weapons::init();
	maps\mp\gametypes\_zombiemode_blockers::init();
	maps\mp\gametypes\_zombiemode_perks::init();
	// Power LAST of the trigger modules: it fans "<perk>_power_on" out by
	// notify, and on a map with no use_elec_switch it sets the flag
	// immediately, so every machine has to be waiting before it runs.
	maps\mp\gametypes\_zombiemode_power::init();
	maps\mp\gametypes\_zombiemode_spawner::init();

	level thread end_game_watcher();
	level thread start_round_loop();

	level notify( "prematch_over" );
}

Callback_CodeEndGame()
{
	level.gameEnded = true;
}

Callback_PlayerConnect()
{
	// self.statusicon is deliberately only ever cleared, never named.  The
	// setter resolves the string against the precached status-icon
	// configstrings and Scr_Errors -- fatally -- on a name that was not
	// precached (GScr_GetStatusIconIndex, src/g_scr_main_mp.cpp:165-183), and
	// the precaching is done by the multiplayer chain this script set exists
	// not to load.  The empty string is the documented zero index.
	self.statusicon = "";
	self waittill( "begin" );

	self.connected = true;
	level.players[ level.players.size ] = self;

	self.score = level.zombie_vars[ "zombie_score_start_1p" ];
	self.score_total = self.score;
	self.old_score = self.score;
	self.deaths = 0;
	self.kills = 0;
	self.downs = 0;
	self.revives = 0;
	self.num_perks = 0;
	self.laststand = false;
	self.is_drinking = false;
	self.rebuild_barrier_reward = 0;

	if ( !isDefined( self.pers ) )
		self.pers = [];
	self.pers[ "lives" ] = level.zombie_vars[ "starting_lives" ];

	level notify( "connected", self );

	// T5 waits on flag "all_players_connected" before the team pot, the perk
	// machines' solo test and the round loop.  This server has no lobby-driven
	// expected-player count, so the first player to reach "begin" opens it.
	flag_set( "all_players_connected" );

	self thread spawnPlayer();
}

Callback_PlayerDisconnect()
{
	players = [];
	for ( i = 0; i < level.players.size; i++ )
	{
		if ( level.players[ i ] == self )
			continue;
		players[ players.size ] = level.players[ i ];
	}
	level.players = players;

	if ( isDefined( self.revivetrigger ) )
	{
		self.revivetrigger delete();
		self.revivetrigger = undefined;
	}
}

// T5 _zombiemode.gsc:4544 player_damage_override.  The zombies rule is that a
// lethal hit puts the player DOWN, not dead, for as long as anyone else is up.
Callback_PlayerDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, timeOffset )
{
	if ( level.gameEnded )
		return;

	// Already down: T5 lets a downed player be finished only by the bleedout,
	// never by another hit.  The native path expresses the same rule as a
	// 500 ms grace window (src/codol/script/codol_script_laststand.c:41-48).
	if ( self maps\mp\gametypes\_zombiemode_laststand::player_is_in_laststand() )
		return;

	if ( iDamage >= self.health && sMeansOfDeath != "MOD_SUICIDE" )
	{
		self maps\mp\gametypes\_zombiemode_laststand::player_laststand();
		return;
	}

	self finishPlayerDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, timeOffset );
}

Callback_PlayerKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration )
{
	self.sessionstate = "dead";
	self.deaths++;
	self.laststand = false;

	if ( isDefined( self.revivetrigger ) )
	{
		self.revivetrigger delete();
		self.revivetrigger = undefined;
	}

	// T5 _zombiemode.gsc:1567 checkForAllDead.
	level thread all_dead_check();

	self thread respawnPlayer();
}

all_dead_check()
{
	wait( 0.05 );
	maps\mp\gametypes\_zombiemode_laststand::checkForAllDead();
}

// The engine fires this when it decides a player is eligible for last stand.
// Entry is currently script-side (see the header of
// _zombiemode_laststand.gsc), so this is unreachable today -- but it is wired
// to the SAME function, so publishing C1 ability bit 23 natively would move
// the down into the engine with no script change at all.
Callback_PlayerLastStand( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration )
{
	self maps\mp\gametypes\_zombiemode_laststand::player_laststand();
}

// --------------------------------------------------------------------------
// Spawning
// --------------------------------------------------------------------------

respawnPlayer()
{
	self endon( "disconnect" );

	// T5 does not respawn inside a round: a dead player waits for
	// spectators_respawn() between rounds (_zombiemode.gsc:2778).  With no
	// spectator path ported, the wait is on the round boundary itself.
	level waittill( "between_round_over" );

	if ( level.gameEnded )
		return;

	self spawnPlayer();
}

spawnPlayer()
{
	self endon( "disconnect" );

	spawnpoint = getSpawnPoint();
	if ( !isDefined( spawnpoint ) )
	{
		println( "CoDOL zm: no usable spawn point on " + getDvar( "mapname" ) );
		return;
	}

	self.sessionteam = "allies";
	self.sessionstate = "playing";
	self.statusicon = "";
	self.laststand = false;
	self.is_drinking = false;
	self.ignoreme = false;
	self.maxhealth = level.zombie_vars[ "player_starting_health" ];
	self.health = self.maxhealth;

	// ClientSpawn memsets gclient, so the native perk state is already gone
	// (src/g_client_mp.cpp:108); the script-side bookkeeping has to follow it
	// or the four-perk cap would never open again.
	self.num_perks = 0;
	self.zm_perks = [];

	self spawn( spawnpoint.origin, spawnpoint.angles );

	self allowSprint( true );
	self allowJump( true );
	self allowAds( true );
	self setMoveSpeedScale( 1 );
	self enableOffhandWeapons();

	self giveStartingLoadout();

	if ( !isDefined( self.score_hud ) )
		self maps\mp\gametypes\_zombiemode_score::score_hud_init();
	else
		self maps\mp\gametypes\_zombiemode_score::set_player_score_hud( true );

	self updateRoundHud();
}

// zm_zombie_training's MapEnts carry the ordinary multiplayer spawn classnames
// and exactly one of each: mp_tdm_spawn_allies_start, mp_tdm_spawn_axis_start,
// mp_tdm_spawn, mp_dm_spawn, mp_global_intermission, info_player_start
// (docs/CODOL_ZOMBIES_STARTUP_20260915.md section 3.6).  One point per
// classname means no spawn spreading; picking randomly among whatever exists
// is honest about that and works unchanged on the other two zm maps.
getSpawnPoint()
{
	names = [];
	names[ 0 ] = "mp_tdm_spawn_allies_start";
	names[ 1 ] = "mp_dm_spawn";
	names[ 2 ] = "mp_tdm_spawn";
	names[ 3 ] = "info_player_start";

	for ( i = 0; i < names.size; i++ )
	{
		points = getEntArray( names[ i ], "classname" );
		if ( points.size > 0 )
			return points[ randomInt( points.size ) ];
	}

	return undefined;
}

giveStartingLoadout()
{
	self takeAllWeapons();

	if ( !isDefined( level.zombie_start_weapon ) )
		return;

	self giveWeapon( level.zombie_start_weapon );
	self giveMaxAmmo( level.zombie_start_weapon );
	self switchToWeapon( level.zombie_start_weapon );

	// C1 keeps a separate zmCharacterIndex and mp/cyborgclasstable.csv is the
	// whole zm roster; the native selector already forces the zm faction when a
	// zm zone is mounted (CODOL_SelectClientCharacter -> zone_faction,
	// src/codol/player/codol_player_appearance.c:184-188).
	self codol_applyProfileCharacter( "zm", level.zombie_start_weapon );
}

// C1 draws the ZOMBIE_ROUND HUD from the client dvar ui_hud_round (renderer at
// 0x00461A71); it rendered live under the previous server
// (alpha0022/docs/zombies_gameplay_plan.md:312-321).
updateRoundHud()
{
	self setClientDvar( "ui_hud_round", "" + level.round_number );
}

publishRoundHud()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] updateRoundHud();
}

// --------------------------------------------------------------------------
// THE ROUND LOOP.  T5 _zombiemode.gsc:3457 round_start / :3899 round_think /
// :4130 round_wait / :3414 round_pause.
// --------------------------------------------------------------------------

start_round_loop()
{
	flag_wait( "all_players_connected" );
	round_start();
}

// T5 :3457.
round_start()
{
	wait( 2 );

	level.zombie_health = level.zombie_vars[ "zombie_health_start" ];

	// T5 :3486 -- players get grenades, and the ammo counter and mini
	// scoreboard are un-hidden.
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isDefined( level.zombie_lethal_grenade ) )
		{
			players[ i ] giveWeapon( level.zombie_lethal_grenade );
			players[ i ] setWeaponAmmoClip( level.zombie_lethal_grenade, 0 );
		}

		players[ i ] setClientDvars( "ammoCounterHide", "0", "miniscoreboardhide", "0" );
	}

	if ( level.zombie_vars[ "game_start_delay" ] > 0 )
		round_pause( level.zombie_vars[ "game_start_delay" ] );

	flag_set( "begin_spawning" );

	// T5's three overridable hooks, kept so a per-map script can replace any
	// one of them without touching this file.
	if ( !isDefined( level.round_spawn_func ) )
		level.round_spawn_func = maps\mp\gametypes\_zombiemode_spawner::round_spawning;

	if ( !isDefined( level.round_wait_func ) )
		level.round_wait_func = ::round_wait;

	if ( !isDefined( level.round_think_func ) )
		level.round_think_func = ::round_think;

	level thread [[ level.round_think_func ]]();
}

// T5 :3414.  The pre-round countdown.  T5's version SetShaders a chalk
// counter; this one is the same counter hudelem without the shader.
round_pause( delay )
{
	if ( !isDefined( delay ) )
		delay = 30;

	level.countdown_hud = create_counter_hud();
	level.countdown_hud setValue( delay );
	level.countdown_hud.color = ( 1, 1, 1 );
	level.countdown_hud.alpha = 1;

	while ( delay >= 1 )
	{
		wait( 1 );
		delay--;
		level.countdown_hud setValue( delay );
	}

	level.countdown_hud fadeOverTime( 1.0 );
	level.countdown_hud.alpha = 0;
	wait( 1.0 );

	level.countdown_hud destroy_hud();
	level.countdown_hud = undefined;
}

// T5 :3899.  THE LOOP.
round_think()
{
	level endon( "end_game" );

	for ( ;; )
	{
		// T5 DT#36173: the per-round barricade reward cap is
		// min( 50 * round, 500 ).
		maxreward = 50 * level.round_number;
		if ( maxreward > 500 )
			maxreward = 500;
		level.zombie_vars[ "rebuild_barrier_cap_per_round" ] = maxreward;

		publishRoundHud();

		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
			players[ i ] maps\mp\gametypes\_zombiemode_blockers::rebuild_barrier_reward_reset();

		level thread award_grenades_for_survivors();

		println( "CoDOL zm: round " + level.round_number + " begins with " + players.size + " player(s)\n" );

		level.round_start_time = getTime();
		level thread [[ level.round_spawn_func ]]();

		level notify( "start_of_round" );

		[[ level.round_wait_func ]]();

		level.first_round = false;
		level notify( "end_of_round" );

		// T5 :3948 -- the spawn delay decays 5% per round, floored at 0.08s.
		timer = level.zombie_vars[ "zombie_spawn_delay" ];
		if ( timer > 0.08 )
			level.zombie_vars[ "zombie_spawn_delay" ] = timer * 0.95;
		else
			level.zombie_vars[ "zombie_spawn_delay" ] = 0.08;

		// T5 :3960.
		level.zombie_move_speed = level.round_number * level.zombie_vars[ "zombie_move_speed_multiplier" ];

		level.round_number++;

		publishRoundHud();

		level notify( "between_round_over" );

		wait( level.zombie_vars[ "zombie_between_round_time" ] );
	}
}

// T5 :4130.  Wait for the budget to drain and every spawned zombie to die.
// flag "end_round_wait" is T5's own escape hatch and is honoured.
round_wait()
{
	level endon( "end_game" );

	wait( 1 );

	for ( ;; )
	{
		if ( flag( "end_round_wait" ) )
		{
			flag_clear( "end_round_wait" );
			return;
		}

		if ( get_enemy_count() <= 0 && level.zombie_total <= 0 && !is_true( level.intermission ) )
			return;

		wait( 1.0 );
	}
}

// T5 :3986.
award_grenades_for_survivors()
{
	if ( !isDefined( level.zombie_lethal_grenade ) )
		return;

	grenade = level.zombie_lethal_grenade;
	players = getPlayers();

	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];

		if ( !p hasWeapon( grenade ) )
		{
			p giveWeapon( grenade );
			p setWeaponAmmoClip( grenade, 0 );
		}

		frac = p getFractionMaxAmmo( grenade );
		if ( frac < 0.25 )
			p setWeaponAmmoClip( grenade, 2 );
		else if ( frac < 0.5 )
			p setWeaponAmmoClip( grenade, 3 );
		else
			p setWeaponAmmoClip( grenade, 4 );
	}
}

// T5 :5064 end_game, reduced to what this server can do: stop the loop, stop
// the spawner, and let the match end.  T5's intermission stats screen needs
// the leaderboard and stats backends, which do not exist here.
end_game_watcher()
{
	level waittill( "end_game" );

	if ( level.intermission )
		return;

	level.gameEnded = true;
	level.intermission = true;

	println( "CoDOL zm: game over on round " + level.round_number + " after " +
		level.total_zombies_killed + " kills\n" );

	maps\mp\gametypes\_zombiemode_spawner::clear_zombies();

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] freezeControls( true );

	wait( level.zombie_vars[ "zombie_intermission_time" ] );

	exitLevel( false );
}
