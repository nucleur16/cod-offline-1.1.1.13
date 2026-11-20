#include maps\mp\_utility;
#include common_scripts\utility;
#include maps\mp\gametypes\_hud_util;
/*
	Infected (engine gametype name "infect")

	Objective:  One player starts infected. An infected player who kills a
	            survivor converts them. Survivors win if any of them are still
	            alive when the time runs out.
	Map ends:   When every survivor has been converted, or the time limit is
	            reached.
	Respawning: No wait, near teammates.

	Why this file exists
	--------------------
	The client offers "infect" and the playlists carry 14 rows of it, but the
	IW4 gametype library this server runs has no infect.gsc, so the loader fell
	back to "dm". This is a native adaptation built on the same helpers war.gsc
	uses, so the spawn logic, the score and round dvars and the callback wiring
	are inherited rather than reimplemented.

	Teams: axis are the infected, allies the survivors. Using the team-based
	path rather than free-for-all is what gives the mode its team scores, its
	spawn selection and its end conditions for free.

	Conversion moves the player between teams the way _menus.gsc addToTeam
	does, which is the only place team, pers["team"] and sessionteam are all
	kept in step. Doing it by hand in one place here would drift from that.
*/

main()
{
	if ( getdvar( "mapname" ) == "mp_background" )
		return;

	maps\mp\gametypes\_globallogic::init();
	maps\mp\gametypes\_callbacksetup::SetupCallbacks();
	maps\mp\gametypes\_globallogic::SetupCallbacks();

	registerTimeLimitDvar( level.gameType, 5, 0, 1440 );
	registerScoreLimitDvar( level.gameType, 0, 0, 5000 );
	registerRoundLimitDvar( level.gameType, 1, 0, 10 );
	registerWinLimitDvar( level.gameType, 1, 0, 10 );
	registerNumLivesDvar( level.gameType, 0, 0, 10 );
	registerHalfTimeDvar( level.gameType, 0, 0, 0 );

	level.teamBased = true;
	level.onStartGameType = ::onStartGameType;
	level.getSpawnPoint = ::getSpawnPoint;
	level.onNormalDeath = ::onNormalDeath;
	level.onTimeLimit = ::onTimeLimit;

	game["dialog"]["gametype"] = "tm_death";
}


onStartGameType()
{
	setClientNameMode( "auto_change" );

	if ( !isdefined( game["switchedsides"] ) )
		game["switchedsides"] = false;

	setObjectiveText( "allies", &"OBJECTIVES_WAR" );
	setObjectiveText( "axis", &"OBJECTIVES_WAR" );
	setObjectiveScoreText( "allies", &"OBJECTIVES_WAR_SCORE" );
	setObjectiveScoreText( "axis", &"OBJECTIVES_WAR_SCORE" );
	setObjectiveHintText( "allies", &"OBJECTIVES_WAR_HINT" );
	setObjectiveHintText( "axis", &"OBJECTIVES_WAR_HINT" );

	level.spawnMins = ( 0, 0, 0 );
	level.spawnMaxs = ( 0, 0, 0 );
	maps\mp\gametypes\_spawnlogic::placeSpawnPoints( "mp_tdm_spawn_allies_start" );
	maps\mp\gametypes\_spawnlogic::placeSpawnPoints( "mp_tdm_spawn_axis_start" );
	maps\mp\gametypes\_spawnlogic::addSpawnPoints( "allies", "mp_tdm_spawn" );
	maps\mp\gametypes\_spawnlogic::addSpawnPoints( "axis", "mp_tdm_spawn" );

	level.mapCenter = maps\mp\gametypes\_spawnlogic::findBoxCenter( level.spawnMins, level.spawnMaxs );
	setMapCenter( level.mapCenter );

	allowed[0] = level.gameType;
	maps\mp\gametypes\_gameobjects::main( allowed );

	level thread chooseFirstInfected();
}


getSpawnPoint()
{
	spawnteam = self.pers["team"];

	if ( level.inGracePeriod )
	{
		spawnPoints = maps\mp\gametypes\_spawnlogic::getSpawnpointArray( "mp_tdm_spawn_" + spawnteam + "_start" );
		spawnPoint = maps\mp\gametypes\_spawnlogic::getSpawnpoint_Random( spawnPoints );
	}
	else
	{
		spawnPoints = maps\mp\gametypes\_spawnlogic::getTeamSpawnPoints( spawnteam );
		spawnPoint = maps\mp\gametypes\_spawnlogic::getSpawnpoint_NearTeam( spawnPoints );
	}

	return spawnPoint;
}


/*
	Everyone starts a survivor; once the grace period is over one of them is
	chosen. Waiting until the players are actually in the world avoids picking
	someone who is still connecting.
*/
chooseFirstInfected()
{
	level endon( "game_ended" );

	while ( level.inGracePeriod )
		wait 0.5;

	survivors = [];
	foreach ( player in level.players )
	{
		if ( isdefined( player.pers["team"] ) && player.pers["team"] == "allies" )
			survivors[ survivors.size ] = player;
	}

	if ( survivors.size <= 0 )
		return;

	survivors[ randomInt( survivors.size ) ] infectPlayer();
}


onNormalDeath( victim, attacker, lifeId )
{
	if ( !isdefined( victim ) || !isdefined( attacker ) || !isPlayer( attacker ) )
		return;
	if ( attacker == victim )
		return;
	if ( !isdefined( attacker.pers["team"] ) || attacker.pers["team"] != "axis" )
		return;
	if ( !isdefined( victim.pers["team"] ) || victim.pers["team"] != "allies" )
		return;

	score = maps\mp\gametypes\_rank::getScoreInfoValue( "kill" );
	if ( !isdefined( score ) )
		score = 100;

	maps\mp\gametypes\_gamescore::giveTeamScoreForObjective( "axis", 1 );
	victim infectPlayer();

	level thread checkSurvivors();
}


// The conversion itself. addToTeam is the one place that keeps team,
// pers["team"] and sessionteam in step, so it is used rather than assigning
// the three by hand here.
infectPlayer()
{
	if ( !isdefined( self ) || !isdefined( self.pers["team"] ) )
		return;
	if ( self.pers["team"] == "axis" )
		return;

	self maps\mp\gametypes\_menus::addToTeam( "axis" );
	self notify( "end_respawn" );
}


// Infected win the moment the last survivor is converted.
checkSurvivors()
{
	level endon( "game_ended" );
	wait 0.05;

	foreach ( player in level.players )
	{
		if ( isdefined( player.pers["team"] ) && player.pers["team"] == "allies" )
			return;
	}

	thread maps\mp\gametypes\_gamelogic::endGame( "axis", game["strings"]["axis_eliminated"] );
}


// Anyone left uninfected when the clock runs out has survived.
onTimeLimit()
{
	foreach ( player in level.players )
	{
		if ( isdefined( player.pers["team"] ) && player.pers["team"] == "allies" )
		{
			thread maps\mp\gametypes\_gamelogic::endGame( "allies", game["strings"]["time_limit_reached"] );
			return;
		}
	}

	thread maps\mp\gametypes\_gamelogic::endGame( "axis", game["strings"]["time_limit_reached"] );
}
