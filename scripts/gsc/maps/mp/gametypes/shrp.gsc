// Local CoDOL adaptation of Black Ops 1 raw/maps/mp/gametypes/shrp.gsc.
// BO1 chooses random T5 weapons and attachments from its wager inventory.
// This mode rotates mounted C1 MP guns instead; each player gets the same gun.
#include maps\mp\_utility;
#include maps\mp\gametypes\_hud_util;

main()
{
	level.partyMode = true;
	level.givePartyLoadout = ::givePartyLoadout;
	maps\mp\gametypes\_globallogic::init();
	maps\mp\gametypes\_callbacksetup::SetupCallbacks();
	maps\mp\gametypes\_globallogic::SetupCallbacks();

	registerTimeLimitDvar( level.gameType, 5.25, 0, 1440 );
	registerScoreLimitDvar( level.gameType, 0, 0, 5000 );
	registerRoundLimitDvar( level.gameType, 1, 0, 10 );
	registerNumLivesDvar( level.gameType, 0, 0, 10 );
	registerWinLimitDvar( level.gameType, 1, 0, 5000 );
	registerHalfTimeDvar( level.gameType, 0, 0, 1 );

	level.onStartGameType = ::onStartGameType;
	level.onPlayerKilled = ::onPlayerKilled;
	level.getSpawnPoint = ::getSpawnPoint;
	game["dialog"]["gametype"] = "freeforall";

	level.partyBaseWeapons = [];
	level.partyLabels = [];
	level.partyWeapons = [];
	level.partyBaseWeapons[0] = "m4_mp";
	level.partyBaseWeapons[1] = "ak47_mp";
	level.partyBaseWeapons[2] = "mp5k_mp";
	level.partyBaseWeapons[3] = "ump45_mp";
	level.partyBaseWeapons[4] = "p90_mp";
	level.partyBaseWeapons[5] = "fal_mp";
	level.partyBaseWeapons[6] = "famas_mp";
	level.partyBaseWeapons[7] = "m21ebr_mp";
	level.partyBaseWeapons[8] = "m1014_mp";
	level.partyBaseWeapons[9] = "as50_mp";
	level.partyBaseWeapons[10] = "deserteagle_mp";
	level.partyLabels[0] = "M4";
	level.partyLabels[1] = "AK-47";
	level.partyLabels[2] = "MP5K";
	level.partyLabels[3] = "UMP45";
	level.partyLabels[4] = "P90";
	level.partyLabels[5] = "FAL";
	level.partyLabels[6] = "FAMAS";
	level.partyLabels[7] = "M21 EBR";
	level.partyLabels[8] = "M1014";
	level.partyLabels[9] = "AS50";
	level.partyLabels[10] = "Desert Eagle";
	for ( i = 0; i < level.partyBaseWeapons.size; i++ )
	{
		precacheItem( level.partyBaseWeapons[i] );
		level.partyWeapons[i] = codol_getPartyWeapon( level.partyBaseWeapons[i] );
		precacheItem( level.partyWeapons[i] );
	}
	level.partyWeaponIndex = randomInt( level.partyWeapons.size );
	level.partyWeapon = level.partyWeapons[level.partyWeaponIndex];
	level.partyFinalMultiplier = 1;
}

onStartGameType()
{
	level.xpScale = 0;
	level.killstreaksenabled = 0;
	level.hardpointsenabled = 0;
	level.useStartSpawns = false;
	setClientNameMode( "auto_change" );
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
	allowed[0] = "shrp";
	maps\mp\gametypes\_gameobjects::main( allowed );

	// The shared damage path already processes each kill. Its base score is
	// zero here, then this mode grants BO1's 100 firearm / 25 melee points.
	maps\mp\gametypes\_rank::registerScoreInfo( "kill", 0 );
	maps\mp\gametypes\_rank::registerScoreInfo( "headshot", 0 );
	maps\mp\gametypes\_rank::registerScoreInfo( "assist", 0 );
	maps\mp\gametypes\_rank::registerScoreInfo( "shrp_gun_kill", 100 );
	maps\mp\gametypes\_rank::registerScoreInfo( "shrp_melee_kill", 25 );
	maps\mp\gametypes\_rank::registerScoreInfo( "suicide", 0 );
	level.QuickMessageToAll = true;
	level thread rotateWeapons();
}

getSpawnPoint()
{
	spawnPoints = maps\mp\gametypes\_spawnlogic::getTeamSpawnPoints( self.pers["team"] );
	return maps\mp\gametypes\_spawnlogic::getSpawnpoint_DM( spawnPoints );
}

givePartyLoadout()
{
	self _clearPerks();
	self.partyKillCount = 0;
	self.partyScoreMultiplier = 1;
	self equipCurrentWeapon();
	self iPrintLnBold( "SHARPSHOOTER - " + level.partyLabels[level.partyWeaponIndex] + ". Make every kill count!" );
	self notify( "shrp_loadout" );
	self thread refillCurrentAmmo();
	return level.partyWeapon;
}

equipCurrentWeapon( rotating )
{
	self notify( "shrp_weapon_changed" );
	if ( !isDefined( rotating ) || !rotating )
		self takeAllWeapons();
	self giveWeapon( level.partyWeapon );
	self giveMaxAmmo( level.partyWeapon );
	if ( isDefined( rotating ) && rotating )
		self thread retirePreviousWeapon( level.partyWeapon );
	else
		self setSpawnWeapon( level.partyWeapon );
	self switchToWeapon( level.partyWeapon );
}

// BO1 keeps the old gun until the client changes weapons. Taking all weapons
// on every timed cycle interrupts the lowering/raising animation for each client.
retirePreviousWeapon( currentWeapon )
{
	self endon( "disconnect" );
	self endon( "death" );
	self endon( "shrp_weapon_changed" );
	level endon( "game_ended" );
	for ( ;; )
	{
		result = self common_scripts\utility::waittill_any_timeout( 2.0, "weapon_change" );
		if ( result == "timeout" || self getCurrentWeapon() == currentWeapon )
			break;
	}
	if ( !self hasWeapon( currentWeapon ) )
		return;
	weapons = self getWeaponsList();
	foreach ( weapon in weapons )
	{
		if ( weapon != currentWeapon )
			self takeWeapon( weapon );
	}
}

refillCurrentAmmo()
{
	self endon( "disconnect" );
	self endon( "death" );
	self endon( "shrp_loadout" );
	level endon( "game_ended" );
	for ( ;; )
	{
		wait 0.25;
		weapon = self getCurrentWeapon();
		if ( weapon != "none" )
			self giveMaxAmmo( weapon );
	}
}

rotateWeapons()
{
	level endon( "game_ended" );
	gameFlagWait( "prematch_done" );
	nextWeaponTime = getTime() + 45000;
	for ( ;; )
	{
		wait 0.25;
		hasTimeLimit = getTimeLimit() > 0;
		timeRemaining = maps\mp\gametypes\_gamelogic::getTimeRemaining();
		if ( hasTimeLimit && timeRemaining <= 0 )
			return;
		// Custom time limits need the full final 45-second bonus period too.
		// A zero time limit means unlimited play, rather than stopping cycling.
		startFinalRound = hasTimeLimit && timeRemaining <= 45000 && level.partyFinalMultiplier == 1;
		if ( startFinalRound )
			level.partyFinalMultiplier = 2;
		if ( getTime() < nextWeaponTime && !startFinalRound )
			continue;
		if ( level.partyFinalMultiplier == 2 && !startFinalRound )
			continue;
		oldIndex = level.partyWeaponIndex;
		level.partyWeaponIndex = randomInt( level.partyWeapons.size );
		if ( level.partyWeaponIndex == oldIndex )
			level.partyWeaponIndex = ( oldIndex + 1 ) % level.partyWeapons.size;
		level.partyWeapon = level.partyWeapons[level.partyWeaponIndex];
		nextWeaponTime = getTime() + 45000;
		for ( i = 0; i < level.players.size; i++ )
		{
			player = level.players[i];
			if ( isDefined( player ) && isAlive( player ) && ( player.pers["team"] == "allies" || player.pers["team"] == "axis" ) )
			{
				player equipCurrentWeapon( true );
				if ( level.partyFinalMultiplier == 2 )
					player iPrintLnBold( "FINAL ROUND - " + level.partyLabels[level.partyWeaponIndex] + "! Double points!" );
				else
					player iPrintLnBold( "NEW WEAPON - " + level.partyLabels[level.partyWeaponIndex] + "!" );
			}
		}
	}
}

onPlayerKilled( eInflictor, attacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration, lifeId )
{
	// BO1 clears the victim's powerups at death, including world/suicide deaths.
	// Waiting for the next spawn leaves dead players' multipliers active.
	self.partyKillCount = 0;
	self.partyScoreMultiplier = 1;
	self _clearPerks();
	if ( isDefined( self.switching_teams ) )
		return;
	if ( !isDefined( attacker ) || !isPlayer( attacker ) || attacker == self )
		return;

	if ( !isDefined( attacker.partyKillCount ) )
		attacker.partyKillCount = 0;
	if ( !isDefined( attacker.partyScoreMultiplier ) )
		attacker.partyScoreMultiplier = 1;
	if ( isAlive( attacker ) )
	{
		attacker.partyKillCount++;
		if ( attacker.partyKillCount == 1 )
		{
			attacker maps\mp\perks\_perks::givePerk( "specialty_fastreload" );
			attacker maps\mp\perks\_perks::givePerk( "specialty_fastads" );
		}
		else if ( attacker.partyKillCount == 2 )
			attacker maps\mp\perks\_perks::givePerk( "specialty_lightweight" );
		else if ( attacker.partyKillCount == 3 )
			attacker maps\mp\perks\_perks::givePerk( "specialty_bulletaccuracy" );
		else if ( attacker.partyKillCount == 4 )
			attacker.partyScoreMultiplier = 2;
	}

	if ( sMeansOfDeath == "MOD_MELEE" )
		scoreEvent = "shrp_melee_kill";
	else
		scoreEvent = "shrp_gun_kill";
	awardCount = attacker.partyScoreMultiplier * level.partyFinalMultiplier;
	for ( i = 0; i < awardCount; i++ )
		maps\mp\gametypes\_gamescore::givePlayerScore( scoreEvent, attacker, self );
}
