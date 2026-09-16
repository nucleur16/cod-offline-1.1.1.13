#include maps\mp\_utility;
#include common_scripts\utility;
#include maps\mp\gametypes\_hud_util;
/*
	Kill Confirmed (engine gametype name "conf")

	Objective:  Eliminate the other team, then collect the tag the body drops
	            to confirm the kill. Collecting a friendly tag denies the
	            enemy their confirm.
	Map ends:   When one team reaches the confirm limit, or the time limit is
	            reached.
	Respawning: No wait, near teammates.

	Why this file exists
	--------------------
	The client offers "conf" and the playlists carry 33 rows of it, but no
	conf.gsc exists in the IW4 gametype library this server runs, so
	GScr_LoadGameTypeScript fell back to "dm" while the server info still
	advertised "conf" (scr_vm_main.c). This is a native adaptation written
	against the same helpers war.gsc uses, so it inherits the team spawn logic,
	the score and round dvars and the callback wiring rather than reimplementing
	them.

	Kill Confirmed reuses the team deathmatch spawn points, as it does on
	retail: a map that supports war supports this.

	Tag pickup is proximity polled rather than trigger based. Nothing in this
	corpus creates a trigger from script, and a tag is short lived and few in
	number, so a cheap per-tag watcher is both sufficient and free of an
	unverified engine dependency.

	The tag model is a placeholder. CoDOL ships its own dog tag asset and its
	name is not measured yet; prop_suitcase_bomb is used because the gametype
	library already precaches it, so this cannot warn or fault on a missing
	model. Replace level.confTagModel once the real asset name is known.
*/

main()
{
	if ( getdvar( "mapname" ) == "mp_background" )
		return;

	maps\mp\gametypes\_globallogic::init();
	maps\mp\gametypes\_callbacksetup::SetupCallbacks();
	maps\mp\gametypes\_globallogic::SetupCallbacks();

	registerTimeLimitDvar( level.gameType, 10, 0, 1440 );
	registerScoreLimitDvar( level.gameType, 65, 0, 5000 );
	registerRoundLimitDvar( level.gameType, 1, 0, 10 );
	registerWinLimitDvar( level.gameType, 1, 0, 10 );
	registerNumLivesDvar( level.gameType, 0, 0, 10 );
	registerHalfTimeDvar( level.gameType, 0, 0, 1 );

	level.teamBased = true;
	level.onStartGameType = ::onStartGameType;
	level.getSpawnPoint = ::getSpawnPoint;
	level.onNormalDeath = ::onNormalDeath;

	// How long a tag stays on the ground before it expires, and how close a
	// player must be to take it. Both match the feel of the retail mode and
	// are dvars so a server can tune them without editing script.
	level.confTagLifetime = 30;
	level.confTagRadius = 64;
	level.confTagModel = "prop_suitcase_bomb";

	game["dialog"]["gametype"] = "tm_death";

	if ( getDvarInt( "g_hardcore" ) )
		game["dialog"]["gametype"] = "hc_" + game["dialog"]["gametype"];
	else if ( getDvarInt( "camera_thirdPerson" ) )
		game["dialog"]["gametype"] = "thirdp_" + game["dialog"]["gametype"];

	precacheModel( level.confTagModel );
}


onStartGameType()
{
	setClientNameMode( "auto_change" );

	if ( !isdefined( game["switchedsides"] ) )
		game["switchedsides"] = false;

	if ( game["switchedsides"] )
	{
		oldAttackers = game["attackers"];
		oldDefenders = game["defenders"];
		game["attackers"] = oldDefenders;
		game["defenders"] = oldAttackers;
	}

	setObjectiveText( "allies", &"OBJECTIVES_WAR" );
	setObjectiveText( "axis", &"OBJECTIVES_WAR" );

	if ( level.splitscreen )
	{
		setObjectiveScoreText( "allies", &"OBJECTIVES_WAR" );
		setObjectiveScoreText( "axis", &"OBJECTIVES_WAR" );
	}
	else
	{
		setObjectiveScoreText( "allies", &"OBJECTIVES_WAR_SCORE" );
		setObjectiveScoreText( "axis", &"OBJECTIVES_WAR_SCORE" );
	}
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
	allowed[1] = "airdrop_pallet";

	maps\mp\gametypes\_gameobjects::main( allowed );
}


getSpawnPoint()
{
	spawnteam = self.pers["team"];
	if ( game["switchedsides"] )
		spawnteam = getOtherTeam( spawnteam );

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
	A kill on its own scores nothing here: the tag is what scores. The kill is
	still credited to the player for their own stats and streaks, which
	_globallogic does before this runs.
*/
onNormalDeath( victim, attacker, lifeId )
{
	if ( !isdefined( victim ) )
		return;

	dropTeam = victim.pers["team"];
	if ( !isdefined( dropTeam ) || (dropTeam != "allies" && dropTeam != "axis") )
		return;

	level thread dropTag( victim.origin, dropTeam, attacker );
}


/*
	One watcher per tag. The tag belongs to the team that died: an enemy
	collecting it confirms, a teammate collecting it denies.
*/
dropTag( origin, dropTeam, attacker )
{
	level endon( "game_ended" );

	tag = spawn( "script_model", origin + ( 0, 0, 8 ) );
	tag setModel( level.confTagModel );

	confirmTeam = getOtherTeam( dropTeam );
	expires = getTime() + ( level.confTagLifetime * 1000 );

	while ( isdefined( tag ) && getTime() < expires )
	{
		wait 0.1;

		if ( !isdefined( level.players ) )
			continue;

		foreach ( player in level.players )
		{
			if ( !isdefined( player ) || !isAlive( player ) )
				continue;
			if ( !isdefined( player.pers["team"] ) )
				continue;
			if ( player.pers["team"] != "allies" && player.pers["team"] != "axis" )
				continue;
			if ( distanceSquared( player.origin, tag.origin ) > ( level.confTagRadius * level.confTagRadius ) )
				continue;

			if ( player.pers["team"] == confirmTeam )
				level thread confirmTag( player, attacker );
			else
				level thread denyTag( player );

			tag delete();
			return;
		}
	}

	if ( isdefined( tag ) )
		tag delete();
}


// The kill counts for the team that made it, and the collector is paid for
// collecting. When the collector is the killer, they get both.
confirmTag( collector, attacker )
{
	score = maps\mp\gametypes\_rank::getScoreInfoValue( "kill" );
	if ( !isdefined( score ) )
		score = 100;

	maps\mp\gametypes\_gamescore::giveTeamScoreForObjective( collector.pers["team"], 1 );

	if ( isdefined( attacker ) && isPlayer( attacker ) )
		maps\mp\gametypes\_gamescore::givePlayerScore( "kill", attacker, undefined );

	if ( !isdefined( attacker ) || attacker != collector )
		maps\mp\gametypes\_gamescore::givePlayerScore( "pickup", collector, undefined );
}


// Denying pays the denier but scores nothing for either team: the enemy simply
// never gets the confirm.
denyTag( collector )
{
	maps\mp\gametypes\_gamescore::givePlayerScore( "return", collector, undefined );
}
