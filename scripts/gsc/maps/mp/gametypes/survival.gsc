#include maps\mp\_utility;
#include common_scripts\utility;
#include maps\mp\gametypes\_hud_util;
/*
	Survival (Spec Ops Survival, engine gametype name "survival")
	Objective:  Every player is on allies against waves of AI enemies. Survive
	            as many waves as possible.
	Map ends:   When nobody is left standing.
	Respawning: A player who dies in a wave comes back at the next
	            intermission; a downed player can be revived by a teammate.

	Ported from Infinity Ward's Modern Warfare 3 Spec Ops Survival
	(maps\_so_survival*.gsc; D:\CoD\COD-GSC-Source\IW5-GSC\scripts\1571.gsc
	and its modules), as CoDOL's own survival builds ran it on their data
	(docs/CODOL_SURVIVAL_PLAN.md). User direction 2026-10-27: survival has
	the multiplayer weapons and the player's own create-a-class, so it is a
	multiplayer gametype on the shared MP chain (_globallogic, _class,
	_menus, the killstreaks) in its own asset category, which mounts
	common_gametype_survival beside the MP gametype zones. The waves,
	enemies and kiosks are maps\mp\gametypes\_survival*.gsc; each level's
	survival entities are maps\mp\survival\<level>.gsc.
*/

main()
{
	if ( getdvar( "mapname" ) == "mp_background" )
		return;

	maps\mp\gametypes\_globallogic::init();
	maps\mp\gametypes\_callbacksetup::SetupCallbacks();
	maps\mp\gametypes\_globallogic::SetupCallbacks();

	registerTimeLimitDvar( level.gameType, 0, 0, 1440 );
	registerScoreLimitDvar( level.gameType, 0, 0, 1000000 );
	registerRoundLimitDvar( level.gameType, 1, 0, 1 );
	registerWinLimitDvar( level.gameType, 1, 0, 1 );
	registerNumLivesDvar( level.gameType, 0, 0, 0 );
	registerHalfTimeDvar( level.gameType, 0, 0, 0 );

	level.teamBased = true;
	// Every player fights on allies; the enemies are AI on axis.
	level.forcedTeam = "allies";
	// (level.forcedTeam also stops the MP room-fill bots and the team
	// balance: the opposing team is the waves.)

	level.onStartGameType = ::onStartGameType;
	level.getSpawnPoint = ::getSpawnPoint;
	level.onSpawnPlayer = ::onSpawnPlayer;
	level.onPlayerKilled = ::onPlayerKilled;
	level.onDeadEvent = ::onDeadEvent;
	level.onOneLeftEvent = ::blankEvent;
	level.onTimeLimit = ::blankEvent;
	level.autoassign = ::menuSurvivors;
	level.allies = ::menuSurvivors;
	level.axis = ::menuSurvivors;
	level.survivalMaySpawn = maps\mp\gametypes\_survival::may_spawn;
	level.survivalClassChange = maps\mp\gametypes\_survival::class_change_now;

	maps\mp\gametypes\_survival::precache();

	game["dialog"]["gametype"] = "tm_death";
}

blankEvent( team )
{
}

onStartGameType()
{
	setClientNameMode( "auto_change" );
	if ( !isdefined( game["switchedsides"] ) )
		game["switchedsides"] = false;

	setObjectiveText( "allies", &"MPUI_TAR_MAP_SURVIVAL_GLOBAL_OBJ" );
	setObjectiveText( "axis", &"MPUI_TAR_MAP_SURVIVAL_GLOBAL_OBJ" );
	setObjectiveScoreText( "allies", &"MPUI_TAR_MAP_SURVIVAL_GLOBAL_OBJ" );
	setObjectiveScoreText( "axis", &"MPUI_TAR_MAP_SURVIVAL_GLOBAL_OBJ" );
	setObjectiveHintText( "allies", &"MPUI_TAR_MAP_SURVIVAL_GLOBAL_OBJ" );
	setObjectiveHintText( "axis", &"MPUI_TAR_MAP_SURVIVAL_GLOBAL_OBJ" );

	// The level's survival entities (player starts, squads, kiosks, paths).
	if ( !maps\mp\gametypes\_survival_levels::data( getDvar( "mapname" ) ) )
		println( "CoDOL survival: " + getDvar( "mapname" ) + " has no survival data (maps/mp/survival/)" );

	level.spawnMins = ( 0, 0, 0 );
	level.spawnMaxs = ( 0, 0, 0 );
	maps\mp\gametypes\_spawnlogic::placeSpawnPoints( "mp_tdm_spawn_allies_start" );
	maps\mp\gametypes\_spawnlogic::placeSpawnPoints( "mp_tdm_spawn_axis_start" );
	maps\mp\gametypes\_spawnlogic::addSpawnPoints( "allies", "mp_tdm_spawn" );
	maps\mp\gametypes\_spawnlogic::addSpawnPoints( "axis", "mp_tdm_spawn" );
	level.mapCenter = maps\mp\gametypes\_spawnlogic::findBoxCenter( level.spawnMins, level.spawnMaxs );
	setMapCenter( level.mapCenter );

	allowed[0] = level.gameType;
	allowed[1] = "airdrop_pallet";
	maps\mp\gametypes\_gameobjects::main( allowed );

	// IW5's slam zoom from overhead instead of the MP prematch countdown.
	maps\mp\gametypes\_survival_intro::init();
	maps\mp\gametypes\_survival::init();
	level thread maps\mp\gametypes\_survival::main();
}

// The survival starts (IW5 info_player_start_so), spread as IW4
// _specialops_code pick_starting_location_so does; the level's MP spawns
// when the map has no survival data.
getSpawnPoint()
{
	if ( isDefined( level.survival_player_starts ) && level.survival_player_starts.size )
	{
		best = undefined;
		bestScore = -1;
		players = level.players;
		for ( i = 0; i < level.survival_player_starts.size; i++ )
		{
			p = level.survival_player_starts[ i ];
			near = 999999;
			for ( j = 0; j < players.size; j++ )
			{
				if ( players[ j ] == self || !isAlive( players[ j ] ) )
					continue;
				d = distance( players[ j ].origin, p.origin );
				if ( d < near )
					near = d;
			}
			if ( near < 64 )
				score = 0;
			else if ( near == 999999 )
				score = 1000 - i;
			else
				score = 100000 - near;
			if ( score > bestScore )
			{
				bestScore = score;
				best = p;
			}
		}
		if ( isDefined( best ) )
			return best;
	}
	spawnPoints = maps\mp\gametypes\_spawnlogic::getTeamSpawnPoints( "allies" );
	return maps\mp\gametypes\_spawnlogic::getSpawnpoint_NearTeam( spawnPoints );
}

onSpawnPlayer()
{
	self maps\mp\gametypes\_survival::on_spawn();
}

onPlayerKilled( eInflictor, attacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, psOffsetTime, deathAnimDuration, killId )
{
	self maps\mp\gametypes\_survival::player_killed();
}

onDeadEvent( team )
{
	if ( team == "allies" || team == "all" )
		maps\mp\gametypes\_survival::game_over();
}

// Joining: always allies, then the player's create-a-class choice (the
// normal class menu), as an MP team pick leads to it.
menuSurvivors( response )
{
	self closeMenus();
	if ( game["state"] == "postgame" )
		return;
	if ( !isDefined( self.pers["team"] ) || self.pers["team"] != "allies" || self.sessionteam == "spectator" )
		self maps\mp\gametypes\_menus::addToTeam( "allies" );
	self maps\mp\gametypes\_menus::beginClassChoice();
}
