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

	Teams: axis are the infected, allies the survivors. Native team spawn and
	death handling are reused; this mode owns selection and vacancy outcomes.

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
	level.partyMode = true;
	level.infectStarted = false;
	level.givePartyLoadout = ::givePartyLoadout;
	level.applyPartyAppearance = ::applyPartyAppearance;
	level.onPlayerDamageFx = ::onPlayerDamageFx;
	if ( getDvar( "scr_infect_cyborg" ) == "" )
		setDvar( "scr_infect_cyborg", 1 );
	level.onStartGameType = ::onStartGameType;
	level.getSpawnPoint = ::getSpawnPoint;
	level.onSpawnPlayer = ::onSpawnPlayer;
	level.onPlayerKilled = ::onPlayerKilled;
	level.onTimeLimit = ::onTimeLimit;
	level.onRespawnDelay = ::onRespawnDelay;
	level.autoassign = ::menuAutoAssign;
	level.allies = ::menuAutoAssign;
	level.axis = ::menuAutoAssign;
	level.class = ::menuAutoAssign;

	// C1 ships these bases AND their composition tables in common_gametype_mp.
	// Its knife is the held weapon's melee action, not a separate knife_mp asset.
	precacheItem( "m1014_mp" );
	precacheItem( "usp_mp" );
	precacheItem( "throwingknife_mp" );
	precacheItem( "flare_mp" );
	precacheItem( "c4_mp" );
	level.infectSurvivorWeapon = codol_getPartyWeapon( "m1014_mp" );
	level.infectMeleeWeapon = codol_getPartyWeapon( "usp_mp", "cqc_tactical" );
	precacheItem( level.infectSurvivorWeapon );
	precacheItem( level.infectMeleeWeapon );
	level.infectEyeFx = loadFx( "maps/zombie/zm_eye_lensflare_blue" );
	level.infectBloodFx = loadFx( "misc/fx_zombie_bloodspurt_codo" );

	game["dialog"]["gametype"] = "tm_death";
}


onStartGameType()
{
	setClientNameMode( "auto_change" );
	level.killstreaksenabled = 0;
	level.hardpointsenabled = 0;

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

	level thread monitorInfection();
}


getSpawnPoint()
{
	spawnteam = self.pers["team"];
	spawnPoint = undefined;

	if ( level.inGracePeriod )
	{
		spawnPoints = maps\mp\gametypes\_spawnlogic::getSpawnpointArray( "mp_tdm_spawn_" + spawnteam + "_start" );
		if ( spawnPoints.size )
			spawnPoint = maps\mp\gametypes\_spawnlogic::getSpawnpoint_Random( spawnPoints );
	}
	else
	{
		spawnPoints = maps\mp\gametypes\_spawnlogic::getTeamSpawnPoints( spawnteam );
		spawnPoint = maps\mp\gametypes\_spawnlogic::getSpawnpoint_NearTeam( spawnPoints );
	}
	if ( !isDefined( spawnPoint ) )
	{
		spawnPoints = maps\mp\gametypes\_spawnlogic::getTeamSpawnPoints( spawnteam );
		spawnPoint = maps\mp\gametypes\_spawnlogic::getSpawnpoint_NearTeam( spawnPoints );
	}

	return spawnPoint;
}


// Team/class menus cannot change sides or refill a living player's weapons.
// _playerlogic also calls this after its initial spectator spawn; assigning a
// valid fixed class here keeps every entrant on the normal native spawn path.
menuAutoAssign( response )
{
	self closeMenus();
	if ( game["state"] == "postgame" || isAlive( self ) )
		return;

	assignment = "allies";
	if ( level.infectStarted )
	{
		assignment = "axis";
		if ( isDefined( self.infectAssigned ) && isDefined( self.pers["team"] ) && self.pers["team"] == "allies" )
			assignment = "allies";
	}
	if ( !isDefined( self.team ) || self.team != assignment )
		self maps\mp\gametypes\_menus::addToTeam( assignment );
	self.sessionteam = assignment;
	self.infectAssigned = true;
	self.pers["class"] = "class0";
	self.class = "class0";
	self.pers["primary"] = 0;
	self.selectedClass = true;
	// The death callback owns ordinary respawns; a menu response during its
	// killcam must not start a second respawn thread.
	if ( self.sessionstate != "dead" )
		self thread maps\mp\gametypes\_playerlogic::spawnClient();
}


onSpawnPlayer()
{
	// Bot and carried-team spawns can bypass the human menu callback. Both
	// routes obey the same survivor/late-join policy before loadout selection.
	assignment = self.team;
	if ( !level.infectStarted )
		assignment = "allies";
	else if ( !isDefined( self.infectAssigned ) )
		assignment = "axis";
	if ( self.team != assignment )
	{
		self maps\mp\gametypes\_playerlogic::removeFromAliveCount();
		self maps\mp\gametypes\_menus::addToTeam( assignment );
		self maps\mp\gametypes\_playerlogic::addToAliveCount();
	}
	self.sessionteam = assignment;
	self.infectAssigned = true;
}


givePartyLoadout()
{
	self _clearPerks();
	self takeAllWeapons();
	weapon = level.infectSurvivorWeapon;
	if ( self.pers["team"] == "axis" )
		weapon = level.infectMeleeWeapon;
	self giveWeapon( weapon );
	if ( self.pers["team"] == "axis" )
	{
		// Keep a native melee-capable held weapon, with no live ammunition.
		self setWeaponAmmoClip( weapon, 0 );
		self setWeaponAmmoStock( weapon, 0 );
		self giveWeapon( "throwingknife_mp" );
		self setWeaponAmmoClip( "throwingknife_mp", 1 );
		// The perk callback starts the real use/plant/respawn-beacon monitor.
		self maps\mp\perks\_perks::givePerk( "specialty_tacticalinsertion" );
		self setWeaponAmmoClip( "flare_mp", 1 );
		self setOffhandEquipment( "throwingknife_mp", "none", "flare_mp" );
	}
	else
	{
		self giveMaxAmmo( weapon );
		self giveWeapon( "c4_mp" );
		self setWeaponAmmoClip( "c4_mp", 1 );
		self setOffhandEquipment( "none", "none", "c4_mp" );
	}
	self switchToWeapon( weapon );
	self setSpawnWeapon( weapon );
	return weapon;
}


applyPartyAppearance()
{
	// Compose the normal outfit first so survivors and later modes can restore
	// the player's own parts. The native owner preserves this override through
	// snapshot refresh, respawn, native posing and corpse creation.
	if ( self.pers["team"] == "axis" && getDvarInt( "scr_infect_cyborg" ) )
	{
		self.infectCyborgApplied = self codol_setAppearanceModels( "c_zom_ger_body1_infect_mp", "c_zom_ger_head" );
		if ( self.infectCyborgApplied )
		{
			{}
			self thread cyborgEyeGlow();
		}
	}
	else
	{
		self codol_clearAppearanceModels();
		self.infectCyborgApplied = false;
	}
}


// The original effect draws both blue eyes from the left eyeball tag. Wait
// one frame for the player DObj to adopt its new composition. Replacing the
// loop periodically also supplies the effect to clients that join later.
cyborgEyeGlow()
{
	self notify( "infect_eye_glow" );
	self endon( "infect_eye_glow" );
	self endon( "disconnect" );
	self endon( "death" );
	level endon( "game_ended" );
	wait 0.05;
	while ( isAlive( self ) && self.pers["team"] == "axis" && self.infectCyborgApplied )
	{
		playFxOnTag( level.infectEyeFx, self, "j_eyeball_le" );
		self.infectEyesOn = true;
		wait 2;
		self stopCyborgEyes();
		wait 0.05;
	}
}


stopCyborgEyes()
{
	if ( !isDefined( self.infectEyesOn ) || !self.infectEyesOn )
		return;
	stopFxOnTag( level.infectEyeFx, self, "j_eyeball_le" );
	self.infectEyesOn = false;
}


// Called after friendly-fire/armor filtering and before native final damage,
// so a survivor's killing hit cannot turn into infected-colored blood when
// the death callback converts them. Bound shotgun pellets to one gush/frame.
onPlayerDamageFx( damage, mod, weapon, point, direction )
{
	if ( damage <= 0 || self.pers["team"] != "axis" || !self.infectCyborgApplied || !isDefined( point ) )
		return;
	if ( mod != "MOD_PISTOL_BULLET" && mod != "MOD_RIFLE_BULLET" && mod != "MOD_HEAD_SHOT" && mod != "MOD_MELEE" && mod != "MOD_EXPLOSIVE_BULLET" )
		return;
	now = getTime();
	if ( isDefined( self.infectBloodTime ) && self.infectBloodTime == now )
		return;
	if ( !isDefined( level.infectBloodTime ) || level.infectBloodTime != now )
	{
		level.infectBloodTime = now;
		level.infectBloodCount = 0;
	}
	if ( level.infectBloodCount >= 6 )
		return;
	level.infectBloodCount++;
	self.infectBloodTime = now;
	forward = ( 0, 0, 1 );
	if ( isDefined( direction ) && lengthSquared( direction ) > 0.0001 )
		forward = vectorNormalize( direction * -1 );
	playFx( level.infectBloodFx, point, forward );
}


onRespawnDelay()
{
	return 0;
}


// _damage invokes this after removing the old team's alive count and cloning
// its corpse. Changing teams in onNormalDeath used to change the corpse and
// other death consumers midway through their old-team processing.
onPlayerKilled( eInflictor, attacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, psOffsetTime, deathAnimDuration, lifeId )
{
	self stopCyborgEyes();
	if ( !level.infectStarted || !isDefined( self.pers["team"] ) || self.pers["team"] != "allies" )
		return;
	if ( isDefined( self.switching_teams ) )
		return;
	// Falling and suicide also convert a survivor once the round has begun;
	// they cannot be used to reset an almost-lost survivor life.
	self infectPlayer();
}


monitorInfection()
{
	level endon( "game_ended" );
	for ( ;; )
	{
		wait 0.25;
		if ( level.inGracePeriod || game["state"] != "playing" )
			continue;
		survivors = [];
		livingSurvivors = [];
		infected = 0;
		foreach ( player in level.players )
		{
			if ( !isDefined( player.pers["team"] ) )
				continue;
			if ( player.pers["team"] == "axis" )
				infected++;
			else if ( player.pers["team"] == "allies" )
			{
				survivors[ survivors.size ] = player;
				if ( isAlive( player ) )
					livingSurvivors[ livingSurvivors.size ] = player;
			}
		}

		if ( !level.infectStarted )
		{
			// Grace can end before either human finishes loading. Keep waiting
			// for two spawned players instead of abandoning first selection.
			if ( livingSurvivors.size < 2 )
				continue;
			level.infectStarted = true;
			livingSurvivors[ randomInt( livingSurvivors.size ) ] infectPlayer();
		}
		else if ( !survivors.size )
		{
			winner = "axis";
			if ( !infected )
				winner = "tie";
			thread maps\mp\gametypes\_gamelogic::endGame( winner, game["strings"]["allies_eliminated"] );
			return;
		}
		else if ( !infected )
		{
			// Replace a departed first/last infected when another 1-vs-all
			// round is possible; the sole remaining survivor has survived.
			if ( survivors.size == 1 )
			{
				thread maps\mp\gametypes\_gamelogic::endGame( "allies", game["strings"]["axis_eliminated"] );
				return;
			}
			if ( livingSurvivors.size )
				livingSurvivors[ randomInt( livingSurvivors.size ) ] infectPlayer();
		}
	}
}


// addToTeam keeps pers, script team and native sessionteam aligned. A live
// first/replacement infected also transfers its alive count and changes kit
// immediately; a dead convert leaves its single respawn to _damage.
infectPlayer()
{
	if ( !isDefined( self.pers["team"] ) || self.pers["team"] == "axis" )
		return;

	wasAlive = isAlive( self );
	if ( wasAlive )
		self maps\mp\gametypes\_playerlogic::removeFromAliveCount();
	self maps\mp\gametypes\_menus::addToTeam( "axis" );
	self.sessionteam = "axis";
	self.infectAssigned = true;
	if ( wasAlive )
	{
		self maps\mp\gametypes\_playerlogic::addToAliveCount();
		self maps\mp\gametypes\_class::giveLoadout( "axis", self.class );
	}
	self iPrintLnBold( "INFECTED: melee the survivors" );
}


// Anyone left uninfected when the clock runs out has survived.
onTimeLimit()
{
	if ( !level.infectStarted )
	{
		thread maps\mp\gametypes\_gamelogic::endGame( "tie", game["strings"]["time_limit_reached"] );
		return;
	}
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
