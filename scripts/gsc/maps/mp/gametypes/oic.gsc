#include maps\mp\_utility;
#include maps\mp\gametypes\_hud_util;

// One in the Chamber rules adapted from Black Ops 1 raw/maps/mp/gametypes/oic.gsc.
// The BO1 M1911 is replaced by C1's composed Desert Eagle; score and lives follow BO1.
main()
{
	maps\mp\gametypes\_globallogic::init();
	maps\mp\gametypes\_callbacksetup::SetupCallbacks();
	maps\mp\gametypes\_globallogic::SetupCallbacks();

	level.partyMode = true;
	level.givePartyLoadout = ::givePartyLoadout;
	level.partyDamageCallback = ::onPartyDamage;
	level.onStartGameType = ::onStartGameType;
	level.getSpawnPoint = ::getSpawnPoint;
	level.onPlayerKilled = ::onPlayerKilled;
	precacheItem( "deserteagle_mp" );
	level.oicWeapon = codol_getPartyWeapon( "deserteagle_mp" );
	precacheItem( level.oicWeapon );

	registerTimeLimitDvar( level.gameType, 10, 0, 1440 );
	registerScoreLimitDvar( level.gameType, 0, 0, 5000 );
	registerWinLimitDvar( level.gameType, 1, 0, 5000 );
	registerRoundLimitDvar( level.gameType, 1, 0, 10 );
	registerNumLivesDvar( level.gameType, 3, 0, 10 );
	registerHalfTimeDvar( level.gameType, 0, 0, 1 );
}

onStartGameType()
{
	setClientNameMode( "auto_change" );
	level.xpScale = 0;
	level.killstreaksenabled = 0;
	level.hardpointsenabled = 0;
	level.useStartSpawns = false;

	setObjectiveText( "allies", &"OBJECTIVES_DM" );
	setObjectiveText( "axis", &"OBJECTIVES_DM" );
	setObjectiveScoreText( "allies", &"OBJECTIVES_DM_SCORE" );
	setObjectiveScoreText( "axis", &"OBJECTIVES_DM_SCORE" );
	setObjectiveHintText( "allies", &"OBJECTIVES_DM_HINT" );
	setObjectiveHintText( "axis", &"OBJECTIVES_DM_HINT" );

	level.spawnMins = ( 0, 0, 0 );
	level.spawnMaxs = ( 0, 0, 0 );
	wagerSpawns = getEntArray( "mp_wager_spawn", "classname" );
	if ( wagerSpawns.size )
		spawnClass = "mp_wager_spawn";
	else
		spawnClass = "mp_dm_spawn";
	maps\mp\gametypes\_spawnlogic::addSpawnPoints( "allies", spawnClass );
	maps\mp\gametypes\_spawnlogic::addSpawnPoints( "axis", spawnClass );
	level.mapCenter = maps\mp\gametypes\_spawnlogic::findBoxCenter( level.spawnMins, level.spawnMaxs );
	setMapCenter( level.mapCenter );

	allowed[0] = "oic";
	maps\mp\gametypes\_gameobjects::main( allowed );
	maps\mp\gametypes\_rank::registerScoreInfo( "kill", 100 );
	maps\mp\gametypes\_rank::registerScoreInfo( "headshot", 0 );
	maps\mp\gametypes\_rank::registerScoreInfo( "assist", 0 );
	maps\mp\gametypes\_rank::registerScoreInfo( "suicide", 0 );
	maps\mp\gametypes\_rank::registerScoreInfo( "teamkill", 0 );
	maps\mp\gametypes\_rank::registerScoreInfo( "oic_survival", 10 );
	level.QuickMessageToAll = true;
}

getSpawnPoint()
{
	spawnPoints = maps\mp\gametypes\_spawnlogic::getTeamSpawnPoints( self.pers["team"] );
	return maps\mp\gametypes\_spawnlogic::getSpawnpoint_DM( spawnPoints );
}

givePartyLoadout()
{
	self _clearPerks();
	self.oicEliminated = false;
	self takeAllWeapons();
	self giveWeapon( level.oicWeapon );
	self switchToWeapon( level.oicWeapon );
	self setSpawnWeapon( level.oicWeapon );
	self setWeaponAmmoClip( level.oicWeapon, 1 );
	self setWeaponAmmoStock( level.oicWeapon, 0 );
	self iPrintLnBold( "ONE IN THE CHAMBER - One shot. " + ( self.pers["lives"] + 1 ) + " lives. Make it count!" );
	return level.oicWeapon;
}

onPartyDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime )
{
	if ( sMeansOfDeath == "MOD_PISTOL_BULLET" || sMeansOfDeath == "MOD_RIFLE_BULLET" || sMeansOfDeath == "MOD_HEAD_SHOT" )
		return 999999;
	return iDamage;
}

onPlayerKilled( eInflictor, attacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, psOffsetTime, deathAnimDuration, lifeId )
{
	if ( isDefined( self.switching_teams ) )
		return;
	if ( isDefined( attacker ) && isPlayer( attacker ) && attacker != self && isAlive( attacker ) && attacker hasWeapon( level.oicWeapon ) )
	{
		ammo = attacker getWeaponAmmoClip( level.oicWeapon );
		attacker setWeaponAmmoClip( level.oicWeapon, ammo + 1 );
		attacker iPrintLnBold( "BULLET EARNED! One more chance." );
	}

	// pers["lives"] is remaining respawns in this IW4-derived lifecycle.
	// It is decremented at spawn, so zero after the victim's third spawn.
	if ( !getGametypeNumLives() || self.pers["lives"] > 0 || ( isDefined( self.oicEliminated ) && self.oicEliminated ) )
		return;
	self.oicEliminated = true;
	for ( i = 0; i < level.players.size; i++ )
	{
		player = level.players[i];
		if ( !isDefined( player ) || player == self || ( player.pers["team"] != "allies" && player.pers["team"] != "axis" ) )
			continue;
		if ( !isReallyAlive( player ) && !player maps\mp\gametypes\_playerlogic::maySpawn() )
			continue;
		if ( !isDefined( player.pers["survived"] ) )
			player.pers["survived"] = 0;
		player.pers["survived"]++;
		player.survived = player.pers["survived"];
		maps\mp\gametypes\_gamescore::givePlayerScore( "oic_survival", player, self );
		player iPrintLnBold( "SURVIVAL BONUS! +10 points" );
	}
}
