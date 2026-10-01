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

#using_animtree ("generic_human");

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
	init_anims();
	init_hints();
}

// --------------------------------------------------------------------------
// T5 _zombiemode.gsc:530 precache_zombie_hints, retargeted at CoDOL's keys.
//
// MEASURED, docs/zone_assets/ALL_localize.txt: CoDOL ships the T5 ZOMBIE_*
// localize set verbatim -- every key below is present in
// localized_code_post_gfx_mp (always mounted) AND again in the zm_prototype_sh
// and zm_zombie_training map zones.  The one T5 key that is NOT shipped is
// &"ZOMBIE_UNDEFINED", T5's fallback, so get_zombie_hint() returns undefined
// instead and each caller names a shipped generic as its fallback ref.
//
// The cost-suffixed keys already contain their own cost, which is why T5
// builds the ref at runtime as "default_buy_door_" + cost rather than passing
// the number as a format argument.  CoDOL additionally ships the _2500 door
// and area steps that T5 never registered; they are registered here.
//
// This must run before any wait: precacheString shares the precache window
// with precacheItem ("precacheItem must be called before any wait statements
// in the gametype or level script"), and init() is called from the level
// script's first frame.
// --------------------------------------------------------------------------

init_hints()
{
	level.zombie_hints = [];

	// Generic forms, used as the fallback when a map's cost has no shipped
	// cost-suffixed key of its own.
	add_zombie_hint( "generic_open_door",    &"ZOMBIE_OPEN_DOOR" );
	add_zombie_hint( "generic_clear_debris", &"ZOMBIE_CLEAR_DEBRIS" );
	add_zombie_hint( "generic_buy_weapon",   &"ZOMBIE_BUY_WEAPON" );

	// Barrier rebuild.  T5 :552-557.
	add_zombie_hint( "default_reward_barrier_piece",    &"ZOMBIE_BUTTON_REWARD_BARRIER" );
	add_zombie_hint( "default_reward_barrier_piece_10", &"ZOMBIE_BUTTON_REWARD_BARRIER_10" );
	add_zombie_hint( "default_reward_barrier_piece_20", &"ZOMBIE_BUTTON_REWARD_BARRIER_20" );
	add_zombie_hint( "default_reward_barrier_piece_30", &"ZOMBIE_BUTTON_REWARD_BARRIER_30" );
	add_zombie_hint( "default_reward_barrier_piece_40", &"ZOMBIE_BUTTON_REWARD_BARRIER_40" );
	add_zombie_hint( "default_reward_barrier_piece_50", &"ZOMBIE_BUTTON_REWARD_BARRIER_50" );

	// Barrier buy-back.  T5 :546-549.
	add_zombie_hint( "default_buy_barrier_piece_10", &"ZOMBIE_BUTTON_BUY_BACK_BARRIER_10" );
	add_zombie_hint( "default_buy_barrier_piece_20", &"ZOMBIE_BUTTON_BUY_BACK_BARRIER_20" );
	add_zombie_hint( "default_buy_barrier_piece_50", &"ZOMBIE_BUTTON_BUY_BACK_BARRIER_50" );
	add_zombie_hint( "default_buy_barrier_piece_100", &"ZOMBIE_BUTTON_BUY_BACK_BARRIER_100" );

	// Debris.  T5 :560-569.
	add_zombie_hint( "default_buy_debris_100", &"ZOMBIE_BUTTON_BUY_CLEAR_DEBRIS_100" );
	add_zombie_hint( "default_buy_debris_200", &"ZOMBIE_BUTTON_BUY_CLEAR_DEBRIS_200" );
	add_zombie_hint( "default_buy_debris_250", &"ZOMBIE_BUTTON_BUY_CLEAR_DEBRIS_250" );
	add_zombie_hint( "default_buy_debris_500", &"ZOMBIE_BUTTON_BUY_CLEAR_DEBRIS_500" );
	add_zombie_hint( "default_buy_debris_750", &"ZOMBIE_BUTTON_BUY_CLEAR_DEBRIS_750" );
	add_zombie_hint( "default_buy_debris_1000", &"ZOMBIE_BUTTON_BUY_CLEAR_DEBRIS_1000" );
	add_zombie_hint( "default_buy_debris_1250", &"ZOMBIE_BUTTON_BUY_CLEAR_DEBRIS_1250" );
	add_zombie_hint( "default_buy_debris_1500", &"ZOMBIE_BUTTON_BUY_CLEAR_DEBRIS_1500" );
	add_zombie_hint( "default_buy_debris_1750", &"ZOMBIE_BUTTON_BUY_CLEAR_DEBRIS_1750" );
	add_zombie_hint( "default_buy_debris_2000", &"ZOMBIE_BUTTON_BUY_CLEAR_DEBRIS_2000" );

	// Doors.  T5 :572-581, plus CoDOL's _2500.
	add_zombie_hint( "default_buy_door_100", &"ZOMBIE_BUTTON_BUY_OPEN_DOOR_100" );
	add_zombie_hint( "default_buy_door_200", &"ZOMBIE_BUTTON_BUY_OPEN_DOOR_200" );
	add_zombie_hint( "default_buy_door_250", &"ZOMBIE_BUTTON_BUY_OPEN_DOOR_250" );
	add_zombie_hint( "default_buy_door_500", &"ZOMBIE_BUTTON_BUY_OPEN_DOOR_500" );
	add_zombie_hint( "default_buy_door_750", &"ZOMBIE_BUTTON_BUY_OPEN_DOOR_750" );
	add_zombie_hint( "default_buy_door_1000", &"ZOMBIE_BUTTON_BUY_OPEN_DOOR_1000" );
	add_zombie_hint( "default_buy_door_1250", &"ZOMBIE_BUTTON_BUY_OPEN_DOOR_1250" );
	add_zombie_hint( "default_buy_door_1500", &"ZOMBIE_BUTTON_BUY_OPEN_DOOR_1500" );
	add_zombie_hint( "default_buy_door_1750", &"ZOMBIE_BUTTON_BUY_OPEN_DOOR_1750" );
	add_zombie_hint( "default_buy_door_2000", &"ZOMBIE_BUTTON_BUY_OPEN_DOOR_2000" );
	add_zombie_hint( "default_buy_door_2500", &"ZOMBIE_BUTTON_BUY_OPEN_DOOR_2500" );

	// Areas.  T5 :584-593, plus CoDOL's _2500.
	add_zombie_hint( "default_buy_area_100", &"ZOMBIE_BUTTON_BUY_OPEN_AREA_100" );
	add_zombie_hint( "default_buy_area_200", &"ZOMBIE_BUTTON_BUY_OPEN_AREA_200" );
	add_zombie_hint( "default_buy_area_250", &"ZOMBIE_BUTTON_BUY_OPEN_AREA_250" );
	add_zombie_hint( "default_buy_area_500", &"ZOMBIE_BUTTON_BUY_OPEN_AREA_500" );
	add_zombie_hint( "default_buy_area_750", &"ZOMBIE_BUTTON_BUY_OPEN_AREA_750" );
	add_zombie_hint( "default_buy_area_1000", &"ZOMBIE_BUTTON_BUY_OPEN_AREA_1000" );
	add_zombie_hint( "default_buy_area_1250", &"ZOMBIE_BUTTON_BUY_OPEN_AREA_1250" );
	add_zombie_hint( "default_buy_area_1500", &"ZOMBIE_BUTTON_BUY_OPEN_AREA_1500" );
	add_zombie_hint( "default_buy_area_1750", &"ZOMBIE_BUTTON_BUY_OPEN_AREA_1750" );
	add_zombie_hint( "default_buy_area_2000", &"ZOMBIE_BUTTON_BUY_OPEN_AREA_2000" );
	add_zombie_hint( "default_buy_area_2500", &"ZOMBIE_BUTTON_BUY_OPEN_AREA_2500" );

	// The mystery box.  T5 :543 registers only the 950 step; CoDOL ships four.
	add_zombie_hint( "default_treasure_chest_650", &"ZOMBIE_RANDOM_WEAPON_650" );
	add_zombie_hint( "default_treasure_chest_750", &"ZOMBIE_RANDOM_WEAPON_750" );
	add_zombie_hint( "default_treasure_chest_850", &"ZOMBIE_RANDOM_WEAPON_850" );
	add_zombie_hint( "default_treasure_chest_950", &"ZOMBIE_RANDOM_WEAPON_950" );
	// treasure_chest_cost() returns 10 while a fire sale is up.
	add_zombie_hint( "default_treasure_chest_10",  &"ZOMBIE_FIRE_SALE_COST" );

	// Strings the zm tree hands straight to setHintString rather than through
	// the ref table.  MEASURED (src/asmsource/_g_scr_main_mp.asm, the
	// GScr_SetHintString body): the builtin calls Scr_GetNumParam and passes
	// numParam-1 to Scr_ConstructMessageString, so &&1 format arguments ARE
	// supported and the cost can be an argument for these.
	precacheString( &"ZOMBIE_NEED_POWER" );
	precacheString( &"ZOMBIE_ELECTRIC_SWITCH" );
	precacheString( &"ZOMBIE_PERK_PACKAPUNCH" );
	precacheString( &"ZOMBIE_GET_UPGRADED" );
	precacheString( &"ZOMBIE_WEAPONCOSTAMMO" );
	precacheString( &"ZOMBIE_WEAPONCOSTAMMO_UPGRADE" );
	precacheString( &"ZOMBIE_PERK_JUGGERNAUT" );
	precacheString( &"ZOMBIE_PERK_QUICKREVIVE" );
	precacheString( &"ZOMBIE_PERK_QUICKREVIVE_SOLO" );
	precacheString( &"ZOMBIE_PERK_FASTRELOAD" );
	precacheString( &"ZOMBIE_PERK_DOUBLETAP" );
	precacheString( &"ZOMBIE_PERK_MARATHON" );
	precacheString( &"ZOMBIE_PERK_DIVETONUKE" );
	precacheString( &"ZOMBIE_PERK_DEADSHOT" );
	precacheString( &"ZOMBIE_PERK_ADDITIONALPRIMARYWEAPON" );
	precacheString( &"ZOMBIE_PERK_GENERIC" );

	// The downed-teammate prompt is CoDOL's own, not a T5 key: the shipped mp
	// tree uses it for exactly this (MEASURED,
	// bin/home/codol_rawfiles/maps/mp/gametypes/_damage.gsc:1875 and :1905,
	// the dieHardMode revive entity).  Per the C1-is-source-of-truth mandate
	// the in-repo working idiom wins over T5's zombies-specific key.
	precacheString( &"PLATFORM_REVIVE" );

	maps\mp\gametypes\_zombiemode_perks::precache_perk_bottles();
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
	set_zombie_var( "player_health_regen_delay",          5000 );

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
			codolprintln( "CoDOL zm: starting weapon " + name );
			return name;
		}
	}

	codolprintln( "CoDOL zm: no starting weapon is present in the mounted zm zones" );
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
	// Stage reporting for the zm gameplay chain.  codolprintln, not println:
	// println is a DEVELOPER builtin and production runs developer 0, so every
	// println in this tree is compiled out and the chain was invisible in the
	// log for weeks.  See docs/CODOL_ZOMBIE_SPAWN_20260921.md.
	codolprintln( "CoDOL zm: Callback_StartGameType entered on " + getDvar( "mapname" ) );

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

	// Equipment test hooks (idle unless scr_codol_equiptest is set).
	maps\mp\gametypes\_codol_equiptest::init();

	// Player-damage rule test verbs (idle unless developer and
	// scr_codol_fftest is set).
	maps\mp\gametypes\_zombiemode_fftest::init();

	// Each Cyborg's pistol/lethal/special/trap (mp/cyborgclasstable.csv) and
	// the grenade_fire registry the item modules below hook into.  Before
	// them, so their register_equipment calls find the registry.
	maps\mp\gametypes\_zombiemode_equipment::init();
	// The four placeable traps (equipment slot): register with the registry
	// above and precache their models and FX in this window.
	maps\mp\gametypes\_zombiemode_traps_placeable::init();

	// C1-original zombies items (docs/CODOL_ZM_EQUIPMENT_EVIDENCE_20260924.md):
	// the money grenade's kill bonus, the rescue throwing knife and the random
	// teleporter.  After the registry, which their register_equipment calls use.
	maps\mp\gametypes\_zombiemode_weap_money_ball::init();
	maps\mp\gametypes\_zombiemode_weap_revive_knife::init();
	maps\mp\gametypes\_zombiemode_weap_teleporter::init();

	// Each character's two perk pops: C1's survival-streak rewards.  After
	// the equipment module, which reads the roster that names them.
	maps\mp\gametypes\_zombiemode_perkpops::init();

	// Points of interest, and the specials that lure with them (zm2's cymbal
	// monkey, zm3's black hole bomb).  After the registry they hook into.
	maps\mp\gametypes\_zombiemode_poi::init();
	maps\mp\gametypes\_zombiemode_weap_cymbal_monkey::init();
	maps\mp\gametypes\_zombiemode_weap_black_hole_bomb::init();

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
	// Power-ups LAST: get_valid_powerup asks the blockers module whether any
	// barrier is damaged and the perks module which machines this map has,
	// so both have to have run.
	maps\mp\gametypes\_zombiemode_powerups::init();

	codolprintln( "CoDOL zm: subsystems initialised; starting the round loop" );

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
	// The difficulty table counts the life currently being played.  The
	// last-stand code stores only spare lives; otherwise a solo player gets
	// a free self-revive without buying Quick Revive.
	self.pers[ "lives" ] = level.zombie_vars[ "starting_lives" ] - 1;
	if ( self.pers[ "lives" ] < 0 )
		self.pers[ "lives" ] = 0;

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

	// Players never hurt each other.  T5 _callbackglobal.gsc:640
	// Callback_PlayerDamage (with no friendly-fire mutator, which CoDOL lacks)
	// drops all damage from another player and, from the player himself,
	// everything but explosive, burn and suicide damage.  Before the perk-pop
	// rules so a teammate's hit cannot trigger them either.
	if ( self player_damage_blocked( eAttacker, sMeansOfDeath ) )
		return;

	// T5 _zombiemode.gsc:4636: placeable mines harm no player, their owner
	// included.
	if ( is_placeable_mine( sWeapon ) )
		return;

	// Perk-pop damage rules: flak jacket immunity and dive explosion, the
	// electro counter-shock.  Before the test-client guard so the rules also
	// run for invulnerable test clients.
	iDamage = self maps\mp\gametypes\_zombiemode_perkpops::player_damage_override( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon );
	if ( iDamage <= 0 )
		return;

	// T5 _zombiemode.gsc:4664: explosive damage to a player is capped at 75
	// while he has more than 75 health (after the flak jacket, above, has
	// had its say).  Only a player's own explosives reach this point.
	if ( is_capped_explosive_mod( sMeansOfDeath ) && self.health > 75 && iDamage > 75 )
		iDamage = 75;

	// Test clients added by _codol_equiptest (scr_codol_equiptest "addbots")
	// stand still next to the equipment under test; nothing else sets this.
	if ( isDefined( self.codolTestInvulnerable ) )
		return;

	// Already down: T5 lets a downed player be finished only by the bleedout,
	// never by another hit.  The native path expresses the same rule as a
	// 500 ms grace window (src/codol/script/codol_script_laststand.c:41-48).
	if ( self maps\mp\gametypes\_zombiemode_laststand::player_is_in_laststand() )
		return;

	self.zmLastDamageTime = getTime();

	// THE LETHAL-HIT VETO USED TO BE HERE, and is deliberately gone.
	//
	// 2026-09-20, docs/CODOL_ZOMBIES_LOOP_20260920.md section 8a: the engine
	// now publishes C1 ability bit 23 for a zm player at spawn, so
	// CODOL_LastStand_CanEnter (src/codol/script/codol_script_laststand.c:100)
	// passes and the ENGINE owns the down -- it gives the correct ps.pm_type 7
	// movement mode, the correct 500 ms damage grace window and the correct
	// wire state, none of which the script approximation could reproduce.  It
	// enters last stand by calling Callback_PlayerLastStand, which already
	// routes to the same player_laststand(), so re-vetoing the hit here would
	// down the player twice: once from this callback and once from the
	// engine.  The already-down guard above stays, because it is the one
	// clause the engine's own grace window does not cover (a second attacker
	// inside the same frame).
	//
	// T5 keeps its player_damage_override (:4544) only because T5's engine
	// has no equivalent gate.  C1 does.  Per the standing mandate, C1 wins.

	self finishPlayerDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, timeOffset );
}

// T5 _callbackglobal.gsc:640.  T5 tests isPlayer( eAttacker ) only; the owner
// of a player's sentry, RC car or trap stands in for it here, so their stray
// hits are held to the same rule.
player_damage_blocked( eAttacker, sMeansOfDeath )
{
	if ( !isDefined( eAttacker ) )
		return false;

	attacker = eAttacker;
	if ( !isPlayer( attacker ) && isDefined( attacker.owner ) && isPlayer( attacker.owner ) )
		attacker = attacker.owner;
	if ( !isPlayer( attacker ) )
		return false;

	if ( attacker != self )
		return true;

	switch ( sMeansOfDeath )
	{
	case "MOD_GRENADE_SPLASH":
	case "MOD_GRENADE":
	case "MOD_EXPLOSIVE":
	case "MOD_PROJECTILE":
	case "MOD_PROJECTILE_SPLASH":
	case "MOD_BURNED":
	case "MOD_SUICIDE":
		return false;
	}
	return true;
}

// T5 _zombiemode_utility.gsc:3595 is_placeable_mine over
// level.zombie_placeable_mine_list (claymore_zm, mine_bouncing_betty); CoDOL's
// betty is bouncingbetty_zm (mp/cyborgclasstable.csv, the zm3 lethal).
is_placeable_mine( sWeapon )
{
	if ( !isDefined( sWeapon ) )
		return false;
	return sWeapon == "bouncingbetty_zm" || sWeapon == "claymore_zm";
}

// The means of death T5 _zombiemode.gsc:4664 caps (MOD_EXPLOSIVE is not one).
is_capped_explosive_mod( sMeansOfDeath )
{
	switch ( sMeansOfDeath )
	{
	case "MOD_PROJECTILE":
	case "MOD_PROJECTILE_SPLASH":
	case "MOD_GRENADE":
	case "MOD_GRENADE_SPLASH":
		return true;
	}
	return false;
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
		codolprintln( "CoDOL zm: no usable spawn point on " + getDvar( "mapname" ) );
		return;
	}

	// IW4 _playerlogic::spawnPlayer (closepopupMenu + closeInGameMenu) and T5
	// _globallogic_player::spawnPlayer (closeMenu + closeInGameMenu) both tear
	// the client's menu stack down on every spawn.  On C1 that is the two-pass
	// legacy teardown ('r','r' -> 0x005F4F20: close the focused menus, then
	// release KEYCATCH_UI) plus 'H' (0x005F5160: cpp_closePauseMenu).  This is
	// the one place the multiplayer tree guaranteed a stuck key-catcher -- the
	// sf_pausemenu_minimap panel C1 leaves focused after the first ESC out of
	// the pause menu -- got released; zm never sent it.  See
	// docs/CODOL_CL_PAUSED_20260920.md, 2026-09-21 section.
	self closeMenu();
	self closeInGameMenu();

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

	// ClientSpawn clears the playerState extension's abilities_known flag.
	// Commit the freshly empty script ability set so the native Zombies
	// category floor republishes modPistolDeath before the first zombie hit.
	// Without this, CODOL_LastStand_CanEnter rejects a lethal claw hit and
	// the player dies outright instead of entering the engine's down state.
	self clearAbilityModifiers();

	self allowSprint( true );
	self allowJump( true );
	self allowAds( true );
	self setMoveSpeedScale( 1 );
	self enableOffhandWeapons();

	self giveStartingLoadout();
	self.zmLastDamageTime = getTime();
	self notify( "zm_regen_restart" );
	self thread zm_player_health_regen();

	// C1 already draws the player's purse in its Zombies HUD.  Reset the
	// floating award delta at spawn without creating a second score readout.
	self.old_score = self.score;

	self updateRoundHud();
}

// The multiplayer tree runs _healthoverlay::playerHealthRegen on each spawn.
// Zombies has its own gametype root and never starts that thread, leaving
// every nonlethal claw hit permanently subtracted.  Keep this health-only
// variant local to zm; the client already owns its damage overlay.
zm_player_health_regen()
{
	self endon( "death" );
	self endon( "disconnect" );
	self endon( "player_downed" );
	self endon( "zm_regen_restart" );

	for ( ;; )
	{
		wait( 0.1 );
		if ( self.health <= 0 || self.laststand || self.health >= self.maxhealth )
			continue;
		if ( getTime() - self.zmLastDamageTime < level.zombie_vars[ "player_health_regen_delay" ] )
			continue;

		// Match the multiplayer thread's rapid recovery after its five-second
		// quiet window; avoid writing through the native damage callback.
		self.health += 10;
		if ( self.health > self.maxhealth )
			self.health = self.maxhealth;
	}
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

	// C1's roster gives each character its own pistol (column 5: m1911_zm, or
	// m1911dw_zm for Danby); the level default covers a character whose pistol
	// did not resolve.
	start_weapon = self maps\mp\gametypes\_zombiemode_equipment::character_pistol();
	if ( !isDefined( start_weapon ) )
		start_weapon = level.zombie_start_weapon;
	if ( !isDefined( start_weapon ) )
		return;
	self.zombie_start_weapon = start_weapon;

	self giveWeapon( start_weapon );
	self giveMaxAmmo( start_weapon );
	self switchToWeapon( start_weapon );

	// Lethal / special / trap in C1's three offhand slots.
	self maps\mp\gametypes\_zombiemode_equipment::give_character_loadout();

	// Perk-pop charges, HUD previews and action slots after the native spawn
	// cleared them.
	self maps\mp\gametypes\_zombiemode_perkpops::on_spawned();

	// C1 keeps a separate zmCharacterIndex and mp/cyborgclasstable.csv is the
	// whole zm roster; the native selector already forces the zm faction when a
	// zm zone is mounted (CODOL_SelectClientCharacter -> zone_faction,
	// src/codol/player/codol_player_appearance.c:184-188).
	// A test client has no uploaded profile to compose, and the builtin raises
	// for one - a runtime error that would end the match.
	if ( !self.isbot )
		self codol_applyProfileCharacter( "zm", start_weapon );
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
		// The character's own lethal (give_character_loadout armed it in slot
		// 0); the level default only for a character without one.
		lethal = players[ i ] maps\mp\gametypes\_zombiemode_equipment::lethal_grenade();
		if ( isDefined( lethal ) && !players[ i ] hasWeapon( lethal ) )
		{
			players[ i ] giveWeapon( lethal );
			players[ i ] setWeaponAmmoClip( lethal, 0 );
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

		codolprintln( "CoDOL zm: round " + level.round_number + " begins with " + players.size + " player(s)" );

		level.round_start_time = getTime();
		level thread [[ level.round_spawn_func ]]();

		level notify( "start_of_round" );

		[[ level.round_wait_func ]]();
		if ( isDefined( level.zm_round_budget ) && isDefined( level.zm_round_spawned ) )
			codolprintln( "CoDOL zm: round " + level.round_number + " accounting: budget " +
				level.zm_round_budget + " spawned " + level.zm_round_spawned +
				" remaining " + level.zombie_total + " alive " + get_enemy_count() );

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
	players = getPlayers();

	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];

		// Each character tops up its own lethal (semtex, money ball, betty).
		grenade = p maps\mp\gametypes\_zombiemode_equipment::lethal_grenade();
		if ( !isDefined( grenade ) )
			continue;

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

	// T5 _zombiemode::player_intermission opens with closeMenu +
	// closeInGameMenu before parking the player for the intermission.
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		players[ i ] closeMenu();
		players[ i ] closeInGameMenu();
		players[ i ] freezeControls( true );
	}

	wait( level.zombie_vars[ "zombie_intermission_time" ] );

	exitLevel( false );
}

// ==========================================================================
// THE ANIMATION TABLES
//
// Port of T5 maps\_zombiemode.gsc:1057 init_standard_zombie_anims, :1366
// init_anims and :1372 init_animscripts -- the three functions the previous
// pass listed as deferred because no animation builtin was registered.
//
// EVERY NAME BELOW IS VALIDATED AGAINST CoDOL, NOT TAKEN ON TRUST FROM T5.
// MEASURED 2026-09-20, two sources intersected:
//   1. the rawfile animtrees/generic_human.atr extracted from
//      patch_common_mp (5,345 lines; 386 distinct ai_zombie_* nodes).  This
//      is the tree the SERVER parses: src/cscr_parser.cpp:567 makes
//      Scr_ReadFile return the mounted archive's copy for any "animtrees/"
//      path, so the vendor IW4 file in bin/home/codol_rawfiles/animtrees/
//      (which has no ai_zombie node at all) is never consulted in zm.  A
//      %name that is not in this tree is a COMPILE error.
//   2. docs/zone_assets/ALL_xanim.txt restricted to the zones a zm match
//      mounts (common_mp, patch_common_mp, common_gametype_zm,
//      patch_common_gametype_zm, zm_asylum_sh, zm_prototype_sh,
//      zm_zombie_training).  225 of the 386 tree nodes have a shipped xanim
//      behind them; a %name in the tree with no xanim compiles and then
//      animates nothing.
//
// The intersection is the palette.  T5 names that lose, C1 winning both:
//   %ai_zombie_run_v4        -> %ai_zombie_run_v1     (v4 ships no xanim;
//                               v1 does, and T5's own source has v1 in this
//                               slot commented out immediately above it)
//   %ai_zombie_idle_v1_delta -> %ai_zombie_idle_v1    (see zombie_stop.gsc)
//   %ai_zombie_taunts_1/_3/_6/_8 -- dropped from the board-taunt pool; T5
//                               already had 1 commented out and the other
//                               three ship no xanim.  The eight that remain
//                               are T5's own board-taunt list.
//
// NOT TAKEN from init_standard_zombie_anims, and why: the tesla, freezegun,
// blackhole, microwave, humangun and thundergun tables (:1165-:1250) and the
// ground-rise / traverse tables (:1290-:1330).  Every one of them belongs to
// a weapon script or a traversal system that is not ported, and the traverse
// set additionally needs animscripted.  They are not silently absent: the
// clips exist and the tables can be pasted back the day their consumer does.
// ==========================================================================

init_anims()
{
	init_standard_zombie_anims();
}

init_standard_zombie_anims()
{
	level.scr_anim = [];
	level.scr_anim[ "zombie" ] = [];

	// Deaths.  T5 :1059.
	level.scr_anim[ "zombie" ][ "death1" ] = %ai_zombie_death_v1;
	level.scr_anim[ "zombie" ][ "death2" ] = %ai_zombie_death_v2;
	level.scr_anim[ "zombie" ][ "death3" ] = %ai_zombie_crawl_death_v1;
	level.scr_anim[ "zombie" ][ "death4" ] = %ai_zombie_crawl_death_v2;

	// Walk cycles.  T5 :1066.  T5's own comment on walk7: "was goose step
	// walk - overridden in theatre only (v8)".
	level.scr_anim[ "zombie" ][ "walk1" ] = %ai_zombie_walk_v1;
	level.scr_anim[ "zombie" ][ "walk2" ] = %ai_zombie_walk_v2;
	level.scr_anim[ "zombie" ][ "walk3" ] = %ai_zombie_walk_v3;
	level.scr_anim[ "zombie" ][ "walk4" ] = %ai_zombie_walk_v4;
	level.scr_anim[ "zombie" ][ "walk5" ] = %ai_zombie_walk_v6;
	level.scr_anim[ "zombie" ][ "walk6" ] = %ai_zombie_walk_v7;
	level.scr_anim[ "zombie" ][ "walk7" ] = %ai_zombie_walk_v9;
	level.scr_anim[ "zombie" ][ "walk8" ] = %ai_zombie_walk_v9;

	// Run cycles.  T5 :1075; run5 is T5's %ai_zombie_run_v4, see the header.
	level.scr_anim[ "zombie" ][ "run1" ] = %ai_zombie_walk_fast_v1;
	level.scr_anim[ "zombie" ][ "run2" ] = %ai_zombie_walk_fast_v2;
	level.scr_anim[ "zombie" ][ "run3" ] = %ai_zombie_walk_fast_v3;
	level.scr_anim[ "zombie" ][ "run4" ] = %ai_zombie_run_v2;
	level.scr_anim[ "zombie" ][ "run5" ] = %ai_zombie_run_v1;
	level.scr_anim[ "zombie" ][ "run6" ] = %ai_zombie_run_v3;

	// Sprint cycles.  T5 :1084, including its own two-clip repeat.
	level.scr_anim[ "zombie" ][ "sprint1" ] = %ai_zombie_sprint_v1;
	level.scr_anim[ "zombie" ][ "sprint2" ] = %ai_zombie_sprint_v2;
	level.scr_anim[ "zombie" ][ "sprint3" ] = %ai_zombie_sprint_v1;
	level.scr_anim[ "zombie" ][ "sprint4" ] = %ai_zombie_sprint_v2;

	// Crawl cycles.  T5 :1093.
	level.scr_anim[ "zombie" ][ "crawl1" ] = %ai_zombie_crawl;
	level.scr_anim[ "zombie" ][ "crawl2" ] = %ai_zombie_crawl_v1;
	level.scr_anim[ "zombie" ][ "crawl3" ] = %ai_zombie_crawl_v2;
	level.scr_anim[ "zombie" ][ "crawl4" ] = %ai_zombie_crawl_v3;
	level.scr_anim[ "zombie" ][ "crawl5" ] = %ai_zombie_crawl_v4;
	level.scr_anim[ "zombie" ][ "crawl6" ] = %ai_zombie_crawl_v5;
	level.scr_anim[ "zombie" ][ "crawl_hand_1" ] = %ai_zombie_walk_on_hands_a;
	level.scr_anim[ "zombie" ][ "crawl_hand_2" ] = %ai_zombie_walk_on_hands_b;

	level.scr_anim[ "zombie" ][ "crawl_sprint1" ] = %ai_zombie_crawl_sprint;
	level.scr_anim[ "zombie" ][ "crawl_sprint2" ] = %ai_zombie_crawl_sprint_1;
	level.scr_anim[ "zombie" ][ "crawl_sprint3" ] = %ai_zombie_crawl_sprint_2;

	// Idles.  T5 puts these in init_animscripts (:1372) as a weighted
	// anim.idleAnimArray; see zombie_stop.gsc for why the delta idle loses.
	level.scr_anim[ "zombie" ][ "idle" ] = %ai_zombie_idle_v1;
	level.scr_anim[ "zombie" ][ "idle_crawl" ] = %ai_zombie_idle_crawl;

	// Melee.  T5 :1117, comments preserved.
	level._zombie_melee = [];
	level._zombie_melee[ "zombie" ] = [];
	level._zombie_melee[ "zombie" ][ 0 ] = %ai_zombie_attack_v2;          // slow swipes
	level._zombie_melee[ "zombie" ][ 1 ] = %ai_zombie_attack_v4;          // single left swipe
	level._zombie_melee[ "zombie" ][ 2 ] = %ai_zombie_attack_v6;          // weird single
	level._zombie_melee[ "zombie" ][ 3 ] = %ai_zombie_attack_v1;          // double swipe
	level._zombie_melee[ "zombie" ][ 4 ] = %ai_zombie_attack_forward_v1;  // double swipe
	level._zombie_melee[ "zombie" ][ 5 ] = %ai_zombie_attack_forward_v2;  // slow double swipe

	level._zombie_run_melee = [];
	level._zombie_run_melee[ "zombie" ] = [];
	level._zombie_run_melee[ "zombie" ][ 0 ] = %ai_zombie_run_attack_v1;  // fast single right
	level._zombie_run_melee[ "zombie" ][ 1 ] = %ai_zombie_run_attack_v2;  // fast double swipe
	level._zombie_run_melee[ "zombie" ][ 2 ] = %ai_zombie_run_attack_v3;  // fast swipe

	level._zombie_walk_melee = [];
	level._zombie_walk_melee[ "zombie" ] = [];
	level._zombie_walk_melee[ "zombie" ][ 0 ] = %ai_zombie_walk_attack_v1;
	level._zombie_walk_melee[ "zombie" ][ 1 ] = %ai_zombie_walk_attack_v2;
	level._zombie_walk_melee[ "zombie" ][ 2 ] = %ai_zombie_walk_attack_v3;
	level._zombie_walk_melee[ "zombie" ][ 3 ] = %ai_zombie_walk_attack_v4;

	level._zombie_melee_crawl = [];
	level._zombie_melee_crawl[ "zombie" ] = [];
	level._zombie_melee_crawl[ "zombie" ][ 0 ] = %ai_zombie_attack_crawl;
	level._zombie_melee_crawl[ "zombie" ][ 1 ] = %ai_zombie_attack_crawl_lunge;

	level._zombie_stumpy_melee = [];
	level._zombie_stumpy_melee[ "zombie" ] = [];
	level._zombie_stumpy_melee[ "zombie" ][ 0 ] = %ai_zombie_walk_on_hands_shot_a;
	level._zombie_stumpy_melee[ "zombie" ][ 1 ] = %ai_zombie_walk_on_hands_shot_b;

	// The self.deathanim pool.  T5 :1270.
	level._zombie_deaths = [];
	level._zombie_deaths[ "zombie" ] = [];
	level._zombie_deaths[ "zombie" ][ 0 ] = %ch_dazed_a_death;
	level._zombie_deaths[ "zombie" ][ 1 ] = %ch_dazed_b_death;
	level._zombie_deaths[ "zombie" ][ 2 ] = %ch_dazed_c_death;
	level._zombie_deaths[ "zombie" ][ 3 ] = %ch_dazed_d_death;

	level._zombie_crawl_deaths = [];
	level._zombie_crawl_deaths[ "zombie" ] = [];
	level._zombie_crawl_deaths[ "zombie" ][ 0 ] = %ai_zombie_crawl_death_v1;
	level._zombie_crawl_deaths[ "zombie" ][ 1 ] = %ai_zombie_crawl_death_v2;

	// Barricade taunts.  T5 :1340.
	level._zombie_board_taunt = [];
	level._zombie_board_taunt[ "zombie" ] = [];
	level._zombie_board_taunt[ "zombie" ][ 0 ] = %ai_zombie_taunts_4;
	level._zombie_board_taunt[ "zombie" ][ 1 ] = %ai_zombie_taunts_7;
	level._zombie_board_taunt[ "zombie" ][ 2 ] = %ai_zombie_taunts_9;
	level._zombie_board_taunt[ "zombie" ][ 3 ] = %ai_zombie_taunts_5b;
	level._zombie_board_taunt[ "zombie" ][ 4 ] = %ai_zombie_taunts_5c;
	level._zombie_board_taunt[ "zombie" ][ 5 ] = %ai_zombie_taunts_5d;
	level._zombie_board_taunt[ "zombie" ][ 6 ] = %ai_zombie_taunts_5e;
	level._zombie_board_taunt[ "zombie" ][ 7 ] = %ai_zombie_taunts_5f;

	// Barricade tear.  T5 keeps these in _zombiemode_spawner::get_tear_anim
	// (:1790-:2000) as a long switch on the chunk's script_noteworthy slot
	// number; the pools are the same six clips per side.  _zombiemode_blockers
	// picks from them by slot.
	level._zombie_boardtear = [];
	level._zombie_boardtear[ "m" ] = [];
	level._zombie_boardtear[ "m" ][ 0 ] = %ai_zombie_boardtear_m_1;
	level._zombie_boardtear[ "m" ][ 1 ] = %ai_zombie_boardtear_m_2;
	level._zombie_boardtear[ "m" ][ 2 ] = %ai_zombie_boardtear_m_3;
	level._zombie_boardtear[ "m" ][ 3 ] = %ai_zombie_boardtear_m_4;
	level._zombie_boardtear[ "m" ][ 4 ] = %ai_zombie_boardtear_m_5;
	level._zombie_boardtear[ "m" ][ 5 ] = %ai_zombie_boardtear_m_6;

	level._zombie_boardtear[ "l" ] = [];
	level._zombie_boardtear[ "l" ][ 0 ] = %ai_zombie_boardtear_l_1;
	level._zombie_boardtear[ "l" ][ 1 ] = %ai_zombie_boardtear_l_2;
	level._zombie_boardtear[ "l" ][ 2 ] = %ai_zombie_boardtear_l_3;
	level._zombie_boardtear[ "l" ][ 3 ] = %ai_zombie_boardtear_l_4;
	level._zombie_boardtear[ "l" ][ 4 ] = %ai_zombie_boardtear_l_5;
	level._zombie_boardtear[ "l" ][ 5 ] = %ai_zombie_boardtear_l_6;

	level._zombie_boardtear[ "r" ] = [];
	level._zombie_boardtear[ "r" ][ 0 ] = %ai_zombie_boardtear_r_1;
	level._zombie_boardtear[ "r" ][ 1 ] = %ai_zombie_boardtear_r_2;
	level._zombie_boardtear[ "r" ][ 2 ] = %ai_zombie_boardtear_r_3;
	level._zombie_boardtear[ "r" ][ 3 ] = %ai_zombie_boardtear_r_4;
	level._zombie_boardtear[ "r" ][ 4 ] = %ai_zombie_boardtear_r_5;
	level._zombie_boardtear[ "r" ][ 5 ] = %ai_zombie_boardtear_r_6;

	level._zombie_boardtear_crawl = [];
	level._zombie_boardtear_crawl[ "m" ] = [];
	level._zombie_boardtear_crawl[ "m" ][ 0 ] = %ai_zombie_boardtear_crawl_m_1;
	level._zombie_boardtear_crawl[ "m" ][ 1 ] = %ai_zombie_boardtear_crawl_m_2;
	level._zombie_boardtear_crawl[ "m" ][ 2 ] = %ai_zombie_boardtear_crawl_m_3;
	level._zombie_boardtear_crawl[ "m" ][ 3 ] = %ai_zombie_boardtear_crawl_m_4;
	level._zombie_boardtear_crawl[ "m" ][ 4 ] = %ai_zombie_boardtear_crawl_m_5;
	level._zombie_boardtear_crawl[ "m" ][ 5 ] = %ai_zombie_boardtear_crawl_m_6;

	level._zombie_boardtear_crawl[ "l" ] = [];
	level._zombie_boardtear_crawl[ "l" ][ 0 ] = %ai_zombie_boardtear_crawl_l_1;
	level._zombie_boardtear_crawl[ "l" ][ 1 ] = %ai_zombie_boardtear_crawl_l_2;
	level._zombie_boardtear_crawl[ "l" ][ 2 ] = %ai_zombie_boardtear_crawl_l_3;
	level._zombie_boardtear_crawl[ "l" ][ 3 ] = %ai_zombie_boardtear_crawl_l_4;
	level._zombie_boardtear_crawl[ "l" ][ 4 ] = %ai_zombie_boardtear_crawl_l_5;
	level._zombie_boardtear_crawl[ "l" ][ 5 ] = %ai_zombie_boardtear_crawl_l_6;

	level._zombie_boardtear_crawl[ "r" ] = [];
	level._zombie_boardtear_crawl[ "r" ][ 0 ] = %ai_zombie_boardtear_crawl_r_1;
	level._zombie_boardtear_crawl[ "r" ][ 1 ] = %ai_zombie_boardtear_crawl_r_2;
	level._zombie_boardtear_crawl[ "r" ][ 2 ] = %ai_zombie_boardtear_crawl_r_3;
	level._zombie_boardtear_crawl[ "r" ][ 3 ] = %ai_zombie_boardtear_crawl_r_4;
	level._zombie_boardtear_crawl[ "r" ][ 4 ] = %ai_zombie_boardtear_crawl_r_5;
	level._zombie_boardtear_crawl[ "r" ][ 5 ] = %ai_zombie_boardtear_crawl_r_6;

	// Barricade bash, for a barrier slot with no tear clip.  T5
	// get_tear_anim falls back to these.
	level._zombie_boardbash = [];
	level._zombie_boardbash[ 0 ] = %ai_zombie_boardbash_l;
	level._zombie_boardbash[ 1 ] = %ai_zombie_boardbash_m;
	level._zombie_boardbash[ 2 ] = %ai_zombie_boardbash_r;

	// Reaching through a torn barricade at a player.  T5 spawner :917
	// should_attack_player_thru_boards.
	level._zombie_window_attack = [];
	level._zombie_window_attack[ 0 ] = %ai_zombie_window_attack_arm_L_out;
	level._zombie_window_attack[ 1 ] = %ai_zombie_window_attack_arm_R_out;

	if ( isDefined( level.zombie_anim_override ) )
		[[ level.zombie_anim_override ]]();
}
