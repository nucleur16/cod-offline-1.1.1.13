#include maps\mp\_utility;
#include maps\mp\gametypes\_hud_util;

// Gun Game rules adapted from Black Ops 1 raw/maps/mp/gametypes/gun.gsc.
// C1 has different weapons and an IW4-derived gametype callback lifecycle.
main()
{
	maps\mp\gametypes\_globallogic::init();
	maps\mp\gametypes\_callbacksetup::SetupCallbacks();
	maps\mp\gametypes\_globallogic::SetupCallbacks();

	level.partyMode = true;
	level.givePartyLoadout = ::givePartyLoadout;
	level.onStartGameType = ::onStartGameType;
	level.getSpawnPoint = ::getSpawnPoint;
	level.onPlayerKilled = ::onPlayerKilled;

	// BO1 has twenty tiers.  These C1 bases have complete model recipes in
	// mp/coweapmodels/<base>.csv; C1 builds the viewmodel from their published
	// composed names, including the weapon's declared default parts.
	level.gunBaseProgression = [];
	level.gunTierLabels = [];
	level.gunProgression = [];
	level.gunBaseProgression[0] = "coltanaconda_mp";
	level.gunBaseProgression[1] = "pp2000_mp";
	level.gunBaseProgression[2] = "g18_mp";
	level.gunBaseProgression[3] = "m1014_mp";
	level.gunBaseProgression[4] = "spas12_mp";
	level.gunBaseProgression[5] = "aa12_mp";
	level.gunBaseProgression[6] = "mp5k_mp";
	level.gunBaseProgression[7] = "ump45_mp";
	level.gunBaseProgression[8] = "vector_mp";
	level.gunBaseProgression[9] = "p90_mp";
	level.gunBaseProgression[10] = "fal_mp";
	level.gunBaseProgression[11] = "m16_mp";
	level.gunBaseProgression[12] = "famas_mp";
	level.gunBaseProgression[13] = "acr_mp";
	level.gunBaseProgression[14] = "aug_mp";
	level.gunBaseProgression[15] = "rpd_mp";
	level.gunBaseProgression[16] = "m240_mp";
	level.gunBaseProgression[17] = "cheytac_mp";
	level.gunBaseProgression[18] = "wa2000_mp";
	level.gunBaseProgression[19] = "deserteagle_mp";
	level.gunTierLabels[0] = "Colt Anaconda";
	level.gunTierLabels[1] = "PP2000";
	level.gunTierLabels[2] = "G18";
	level.gunTierLabels[3] = "M1014";
	level.gunTierLabels[4] = "SPAS-12";
	level.gunTierLabels[5] = "AA-12";
	level.gunTierLabels[6] = "MP5K";
	level.gunTierLabels[7] = "UMP45";
	level.gunTierLabels[8] = "Vector";
	level.gunTierLabels[9] = "P90";
	level.gunTierLabels[10] = "FAL";
	level.gunTierLabels[11] = "M16";
	level.gunTierLabels[12] = "FAMAS";
	level.gunTierLabels[13] = "ACR";
	level.gunTierLabels[14] = "AUG";
	level.gunTierLabels[15] = "RPD";
	level.gunTierLabels[16] = "M240";
	level.gunTierLabels[17] = "CheyTac";
	level.gunTierLabels[18] = "WA2000";
	level.gunTierLabels[19] = "Desert Eagle";

	for ( i = 0; i < level.gunBaseProgression.size; i++ )
	{
		// Publish the base first: the native composer needs its C1 weapon slot.
		precacheItem( level.gunBaseProgression[i] );
		level.gunProgression[i] = codol_getPartyWeapon( level.gunBaseProgression[i] );
		precacheItem( level.gunProgression[i] );
	}

	registerTimeLimitDvar( level.gameType, 10, 0, 1440 );
	registerScoreLimitDvar( level.gameType, level.gunProgression.size, 0, 5000 );
	registerWinLimitDvar( level.gameType, 1, 0, 5000 );
	registerRoundLimitDvar( level.gameType, 1, 0, 10 );
	registerNumLivesDvar( level.gameType, 0, 0, 10 );
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

	allowed[0] = "gun";
	maps\mp\gametypes\_gameobjects::main( allowed );
	maps\mp\gametypes\_rank::registerScoreInfo( "kill", 0 );
	maps\mp\gametypes\_rank::registerScoreInfo( "headshot", 0 );
	maps\mp\gametypes\_rank::registerScoreInfo( "assist", 0 );
	maps\mp\gametypes\_rank::registerScoreInfo( "suicide", 0 );
	maps\mp\gametypes\_rank::registerScoreInfo( "teamkill", 0 );
	level.QuickMessageToAll = true;
}

getSpawnPoint()
{
	spawnPoints = maps\mp\gametypes\_spawnlogic::getTeamSpawnPoints( self.pers["team"] );
	return maps\mp\gametypes\_spawnlogic::getSpawnpoint_DM( spawnPoints );
}

givePartyLoadout( reason )
{
	if ( !isDefined( self.gunProgress ) )
		self.gunProgress = 0;

	self notify( "gun_loadout_changed" );
	self clearPerks();
	tier = self.gunProgress;
	if ( tier >= level.gunProgression.size )
		tier = level.gunProgression.size - 1;
	weapon = level.gunProgression[tier];
	oldWeapon = self getCurrentWeapon();
	promoted = isDefined( reason ) && reason == "promoted" && oldWeapon != "none" && oldWeapon != weapon;
	if ( !promoted )
		self takeAllWeapons();
	self giveWeapon( weapon );
	self giveMaxAmmo( weapon );
	if ( promoted )
	{
		// Let the client lower the old gun and raise the new one before removing it.
		self thread retirePreviousGun( weapon );
		wait 0.05;
	}
	self switchToWeapon( weapon );
	if ( !promoted )
		self setSpawnWeapon( weapon );
	self updateGunProgressHud();
}

retirePreviousGun( currentWeapon )
{
	self endon( "gun_loadout_changed" );
	self endon( "death" );
	self endon( "disconnect" );
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
	foreach ( oldWeapon in weapons )
	{
		if ( oldWeapon != currentWeapon )
			self takeWeapon( oldWeapon );
	}
}


updateGunProgressHud()
{
	if ( isDefined( self.isbot ) && self.isbot )
		return;

	if ( !isDefined( self.gunProgressHud ) )
	{
		self.gunProgressHud = self createFontString( "hudsmall", 1.3 );
		self.gunProgressHud setPoint( "BOTTOMLEFT", "BOTTOMLEFT", 115, -75 );
		self.gunProgressHud.color = ( 1, 1, 1 );
		self.gunProgressHud.alpha = 0.9;
	}

	tier = self.gunProgress + 1;
	if ( tier > level.gunProgression.size )
		tier = level.gunProgression.size;
	self.gunProgressHud setText( "WEAPON TIER: " + tier + " OF " + level.gunProgression.size );
}

refreshPromotedLoadout()
{
	self endon( "death" );
	self endon( "disconnect" );
	level endon( "game_ended" );
	wait 0.05;
	if ( isAlive( self ) )
		self givePartyLoadout( "promoted" );
}

onPlayerKilled( eInflictor, attacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, psOffsetTime, deathAnimDuration, lifeId )
{
	if ( sMeansOfDeath == "MOD_SUICIDE" )
	{
		self demotePlayer( "self" );
		return;
	}
	if ( !isDefined( attacker ) || !isPlayer( attacker ) )
	{
		// Generic death-point penalties must not desync score from the tier.
		maps\mp\gametypes\_gamescore::_setPlayerScore( self, self.gunProgress );
		return;
	}
	if ( attacker == self )
	{
		self demotePlayer( "self" );
		return;
	}

	if ( sMeansOfDeath == "MOD_MELEE" )
	{
		self demotePlayer( "humiliated", attacker );
		return;
	}

	if ( !isDefined( attacker.gunProgress ) )
		attacker.gunProgress = 0;
	if ( attacker.gunProgress >= level.gunProgression.size )
		return;
	if ( sWeapon != level.gunProgression[attacker.gunProgress] )
		return;

	attacker.gunProgress++;
	maps\mp\gametypes\_gamescore::_setPlayerScore( attacker, attacker.gunProgress );
	attacker updateGunProgressHud();
	if ( attacker.gunProgress < level.gunProgression.size )
		attacker thread refreshPromotedLoadout();
}

demotePlayer( reason, attacker )
{
	if ( !isDefined( self.gunProgress ) )
		self.gunProgress = 0;
	if ( self.gunProgress > 0 )
		self.gunProgress--;
	maps\mp\gametypes\_gamescore::_setPlayerScore( self, self.gunProgress );
	self updateGunProgressHud();
	tier = self.gunProgress + 1;
	return tier;
}
