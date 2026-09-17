// zm core: tuning, callbacks, spawn.
//
// The zm counterpart of the multiplayer _gamelogic/_playerlogic/_damage trio,
// reduced to the boot chain.  Behaviour reference is T5's maps/_zombiemode*.gsc
// (D:\CoD\COD-GSC-Source\BO1-GSC, 55 files / 33,189 lines); this file mirrors
// its data access exactly -- set_zombie_var()/get_table_var() read
// mp/zombiemode.csv by row name -- and reimplements only the part T5 gets from
// its own engine.
//
// T5 has no zm.gsc-style gametype file at all: _callbackglobal::Callback_Start-
// GameType is empty and the mode is entered from the level script calling
// maps\_zombiemode::main().  CoDOL's engine demands a gametype script
// (GScr_LoadGameTypeScript, src/scr_vm_main.c), so the entry point here is the
// gametype's, and the level script only carries per-map data.
//
// NOTHING IN THIS FILE MAY NAME A MULTIPLAYER MODULE.  A `file::function`
// reference is a compile-time dependency: naming maps\mp\_load or
// maps\mp\gametypes\_gamelogic would compile the multiplayer chain and precache
// multiplayer assets against zm zones, which is the whole problem this script
// set exists to remove.  There are therefore no #includes -- every helper below
// is either local or an engine builtin.

// --------------------------------------------------------------------------
// Tuning.  mp/zombiemode.csv ships in common_gametype_zm.ff (overridden by
// patch_common_gametype_zm.ff), 5 columns x 106 rows, NAME | EASY | MEDIUM |
// HARD | VETERAN.  T5 reads it as TableLookUp(table, 0, name, difficulty + 1)
// (_zombiemode_utility.gsc:1331-1347, _zombiemode.gsc:900-902).  Column 1 is
// T5's own default for set_zombie_var, and the mode's difficulty selector is
// not part of the boot chain, so column 1 it is.
// --------------------------------------------------------------------------

zombie_table()
{
	return "mp/zombiemode.csv";
}

// T5 _zombiemode_utility.gsc:1331 set_zombie_var(), same contract: the table
// value wins when the row exists and is non-empty, otherwise the supplied
// default stands.  tableLookup returns "" for a missing table or row and never
// errors (src/codol/script/codol_script_table.c:77-98), so an absent
// mp/zombiemode.csv degrades to the defaults instead of failing the boot.
set_zombie_var( var, value, is_float, column )
{
	if ( !isDefined( is_float ) )
		is_float = false;
	if ( !isDefined( column ) )
		column = 1;

	table_value = tableLookup( zombie_table(), 0, var, column );
	if ( isDefined( table_value ) && table_value != "" )
	{
		if ( is_float )
			value = float( table_value );
		else
			value = int( table_value );
	}

	level.zombie_vars[ var ] = value;
	return value;
}

get_zombie_var( var )
{
	return level.zombie_vars[ var ];
}

// --------------------------------------------------------------------------
// Gametype init.  Runs from zm.gsc::main(), i.e. at gametype-script load,
// before the level script and before CodeCallback_StartGameType.  Data only:
// no entity may be touched here.
// --------------------------------------------------------------------------

init()
{
	// common_scripts\utility::isSP() infers singleplayer from the map name
	// (!string_starts_with(mapname, "mp_"), utility.gsc:2080-2086) because IW4
	// shipped no multiplayer map that was not mp_*.  zm_* maps are not mp_*.
	// Nothing in this script set calls isSP(), but the level script or a future
	// shared common_scripts helper can, and a dedicated match server is never
	// singleplayer whatever the map is called.  Answering it is the mechanism
	// the vendor function itself provides -- it caches into level.isSP and only
	// computes when that is undefined.
	level.isSP = false;

	// mp_playlists.ff's playlists_allmaps.info has no `teambased` line for zm
	// and mp/gametypescptable.csv places zm in its non-team half.
	level.teamBased = false;
	level.gametype = "zm";
	level.players = [];

	level.zombie_vars = [];

	// Round 1 and the players' opening points are the only tuning the boot
	// chain consumes; the rest of the 106 rows belong to the follow-on
	// workstream and are deliberately not pre-read, so a miss shows up in the
	// system that uses it rather than here.
	set_zombie_var( "game_start_delay", 0 );
	set_zombie_var( "starting_lives", 1 );
	set_zombie_var( "zombie_score_start_1p", 500 );
	// Not zombie_health_start: that row is the AI's health, and the AI is the
	// follow-on workstream. The table wins if it carries a player row; 100 is
	// the zombies player default otherwise.
	set_zombie_var( "player_starting_health", 100 );

	level.round_number = 1;
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
//
// zm_zombie_training is the one map with no vision or sun rawfile in any
// mounted zone -- audit row 18 of docs/CODOL_ZOMBIES_STARTUP_20260915.md -- so
// it passes undefined and nothing is asked for.  The other two can have the
// vision the multiplayer maps/mp/_load.gsc:93,97 would have given them, without
// compiling a single multiplayer script to get it.
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
// (CODOL_InitializeServerWeaponConfigstrings, src/sv_main.c).  Ask the table
// first, then fall back to the one zm weapon this client is recorded as having
// accepted live -- coltanaconda_zm, equipped with ammo 8|32 under the previous
// server (alpha0022/docs/zombies_gameplay_plan.md:312-321) -- then to T5's own
// starting pistol name.
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

	// The zone ships no maps/mp/gametypes GSC and no vision or sun rawfile for
	// zm_zombie_training specifically (unlike zm_prototype_sh and zm_asylum_sh,
	// and unlike every mp zone), so there is deliberately no VisionSetNaked or
	// VisionSetPain call anywhere in this script set -- asking for a vision
	// name that exists in no mounted zone is the open risk recorded as audit
	// row 18 of docs/CODOL_ZOMBIES_STARTUP_20260915.md.

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

	self.score = get_zombie_var( "zombie_score_start_1p" );
	self.deaths = 0;
	self.kills = 0;
	if ( !isDefined( self.pers ) )
		self.pers = [];
	self.pers[ "lives" ] = get_zombie_var( "starting_lives" );

	level notify( "connected", self );

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
}

Callback_PlayerDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, timeOffset )
{
	if ( level.gameEnded )
		return;

	self finishPlayerDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, timeOffset );
}

Callback_PlayerKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration )
{
	self.sessionstate = "dead";
	self.deaths++;

	self thread respawnPlayer();
}

// Reached only when the engine decides a player is eligible for last stand.
// Nothing in this script set grants that eligibility, so this is unreachable
// today; it is registered because GScr_LoadGameTypeScript binds the label with
// errorIfMissing=1 (src/scr_vm_main.c:1308) and the server would not start
// without it.  The real zombies behaviour -- downed, bleed-out, revive, the
// MPUI_REVIVES scoreboard column C1 already substitutes under zm -- is the
// follow-on workstream's (T5 maps/_laststand.gsc).
Callback_PlayerLastStand( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration )
{
	self Callback_PlayerKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration );
}

// --------------------------------------------------------------------------
// Spawning
// --------------------------------------------------------------------------

respawnPlayer()
{
	self endon( "disconnect" );

	wait( 5 );
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
	self.maxhealth = get_zombie_var( "player_starting_health" );
	self.health = self.maxhealth;

	self spawn( spawnpoint.origin, spawnpoint.angles );

	self giveStartingLoadout();
	self updateRoundHud();
}

// zm_zombie_training's MapEnts carry the ordinary multiplayer spawn classnames
// and exactly one of each: mp_tdm_spawn_allies_start, mp_tdm_spawn_axis_start,
// mp_tdm_spawn, mp_dm_spawn, mp_global_intermission, info_player_start
// (docs/CODOL_ZOMBIES_STARTUP_20260915.md section 3.6).  There is no
// script_gametype key on worldspawn.  One point per classname means no spawn
// spreading; picking randomly among whatever exists is honest about that and
// works unchanged on zm_prototype_sh and zm_asylum_sh.
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
	self switchToWeapon( level.zombie_start_weapon );

	// C1 keeps a separate zmCharacterIndex and mp/cyborgclasstable.csv is the
	// whole zm roster; the native selector already forces the zm faction when a
	// zm zone is mounted (CODOL_SelectClientCharacter -> zone_faction,
	// src/codol/player/codol_player_appearance.c:184-188).
	self codol_applyProfileCharacter( "zm", level.zombie_start_weapon );
}

// C1 draws the ZOMBIE_ROUND HUD from the client dvar ui_hud_round (renderer at
// 0x00461A71); it rendered live under the previous server
// (alpha0022/docs/zombies_gameplay_plan.md:312-321).  The round never advances
// yet -- the round manager is the follow-on workstream -- but publishing round
// 1 is what proves the client half of the loop is wired.
updateRoundHud()
{
	self setClientDvar( "ui_hud_round", "" + level.round_number );
}
