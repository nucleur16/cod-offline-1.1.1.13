// PROVENANCE: vendor/iw4/maps/mp/perks/_perkfunctions.gsc (IW4 per-perk set/unset
// functions, tactical insertion handling and the helpers other vendor scripts
// link against, e.g. _playerlogic::deleteTI). The native server applies each perk's server-side effect
// through these functions exactly once; the client reads its own copy from the
// ability-modifier field, so nothing here is applied twice on one machine.

/******************************************************************* 
//						_perkfunctions.gsc  
//	
//	Holds all the perk set/unset and listening functions 
//	
//	Jordan Hirsh	Sept. 11th 	2008
********************************************************************/

#include common_scripts\utility;
#include maps\mp\_utility;
#include maps\mp\gametypes\_hud_util;
#include maps\mp\perks\_perks;


blastshieldUseTracker( perkName, useFunc )
{
	self endon ( "death" );
	self endon ( "disconnect" );
	self endon ( "end_perkUseTracker" );
	level endon ( "game_ended" );

	for ( ;; )
	{
		self waittill ( "empty_offhand" );

		if ( !isOffhandWeaponEnabled() )
			continue;
			
		self [[useFunc]]( self _hasPerk( "_specialty_blastshield" ) );
	}
}

perkUseDeathTracker()
{
	self endon ( "disconnect" );
	
	self waittill("death");
	self._usePerkEnabled = undefined;
}

setRearView()
{
	//self thread perkUseTracker( "specialty_rearview", ::toggleRearView );
}

unsetRearView()
{
	self notify ( "end_perkUseTracker" );
}

toggleRearView( isEnabled )
{
	if ( isEnabled )
	{
		self _setPerk( "_specialty_rearview" );
		self SetRearViewRenderEnabled(true);
	}
	else
	{
		self _unsetPerk( "_specialty_rearview" );
		self SetRearViewRenderEnabled(false);
	}
}


setEndGame()
{
	if ( isdefined( self.endGame ) )
		return;
		
	self.maxhealth = ( maps\mp\gametypes\_tweakables::getTweakableValue( "player", "maxhealth" ) * 4 );
	self.health = self.maxhealth;
	self.endGame = true;
	self.attackerTable[0] = "";
	self visionSetNakedForPlayer("end_game", 5 );
	self thread endGameDeath( 7 );
	self.hasDoneCombat = true;
}


unsetEndGame()
{
	self notify( "stopEndGame" );
	self.endGame = undefined;
	revertVisionSet();
	
	if (! isDefined( self.endGameTimer ) )
		return;
	
	self.endGameTimer destroyElem();
	self.endGameIcon destroyElem();		
}


revertVisionSet()
{
	self VisionSetNakedForPlayer( getDvar( "mapname" ), 1 );	
}

endGameDeath( duration )
{
	self endon( "death" );
	self endon( "disconnect" );
	self endon( "joined_team" );
	level endon( "game_ended" );
	self endon( "stopEndGame" );
		
	wait( duration + 1 );
	//self visionSetNakedForPlayer("end_game2", 1 );
	//wait(1);
	self _suicide();			
}

setCombatHigh()
{
	self endon( "death" );
	self endon( "disconnect" );
	self endon( "unset_combathigh" );
	level endon( "end_game" );
	// C1 selected perk data owns the lifetime; do not retain IW4's ten seconds.
	duration = self getSelectedPerkModifierValue( "modCombatHighTime" );
	if ( !isDefined( duration ) || duration <= 0 )
	{
		self _unsetPerk( "specialty_combathigh" );
		return;
	}
	fadeTime = min( 2.0, duration );
	
	self.damageBlockedTotal = 0;
	//self visionSetNakedForPlayer( "end_game", 1 );

	if ( level.splitscreen )
	{
		yOffset = 56;
		iconSize = 21; // 32/1.5
	}
	else
	{
		yOffset = 112;
		iconSize = 32;
	}

	self.combatHighOverlay = newClientHudElem( self );
	self.combatHighOverlay.x = 0;
	self.combatHighOverlay.y = 0;
	self.combatHighOverlay.alignX = "left";
	self.combatHighOverlay.alignY = "top";
	self.combatHighOverlay.horzAlign = "fullscreen";
	self.combatHighOverlay.vertAlign = "fullscreen";
	self.combatHighOverlay setshader ( "combathigh_overlay", 640, 480 );
	self.combatHighOverlay.sort = -10;
	self.combatHighOverlay.archived = true;
	
	self.combatHighTimer = createTimer( "hudsmall", 1.0 );
	self.combatHighTimer setPoint( "CENTER", "CENTER", 0, yOffset );
	self.combatHighTimer setTimer( duration );
	self.combatHighTimer.color = (.8,.8,0);
	self.combatHighTimer.archived = false;
	self.combatHighTimer.foreground = true;

	self.combatHighIcon = self createIcon( "specialty_painkiller", iconSize, iconSize );
	self.combatHighIcon.alpha = 0;
	self.combatHighIcon setParent( self.combatHighTimer );
	self.combatHighIcon setPoint( "BOTTOM", "TOP" );
	self.combatHighIcon.archived = true;
	self.combatHighIcon.sort = 1;
	self.combatHighIcon.foreground = true;

	self.combatHighOverlay.alpha = 0.0;	
	self.combatHighOverlay fadeOverTime( 1.0 );
	self.combatHighIcon fadeOverTime( 1.0 );
	self.combatHighOverlay.alpha = 1.0;
	self.combatHighIcon.alpha = 0.85;
	
	self thread unsetCombatHighOnDeath();
	
	wait( duration - fadeTime );

	self.combatHighIcon	fadeOverTime( fadeTime );
	self.combatHighIcon.alpha = 0.0;
	
	self.combatHighOverlay fadeOverTime( fadeTime );
	self.combatHighOverlay.alpha = 0.0;
	
	self.combatHighTimer fadeOverTime( fadeTime );
	self.combatHighTimer.alpha = 0.0;

	wait( fadeTime );
	self.damageBlockedTotal = undefined;

	self _unsetPerk( "specialty_combathigh" );
}

unsetCombatHighOnDeath()
{
	self endon ( "disconnect" );
	self endon ( "unset_combathigh" );
	
	self waittill ( "death" );
	
	self thread _unsetPerk( "specialty_combathigh" );
}

unsetCombatHigh()
{
	self notify ( "unset_combathigh" );
	self.damageBlockedTotal = undefined;
	if ( isDefined( self.combatHighOverlay ) )
		self.combatHighOverlay destroy();
	if ( isDefined( self.combatHighIcon ) )
		self.combatHighIcon destroy();
	if ( isDefined( self.combatHighTimer ) )
		self.combatHighTimer destroy();
}

setSiege()
{
	self thread trackSiegeEnable();
	self thread trackSiegeDissable();
}

trackSiegeEnable()
{
	self endon ( "death" );
	self endon ( "disconnect" );
	self endon ( "stop_trackSiege" );
	
	for ( ;; )
	{
		self waittill ( "gambit_on" );
		
		//self setStance( "crouch" );
		//self thread stanceStateListener();
		//self thread jumpStateListener();  
		self.moveSpeedScaler = 0;
		self maps\mp\gametypes\_weapons::updateMoveSpeedScale( self getCurrentWeapon() );
		class = weaponClass( self getCurrentWeapon() );
		
		if ( class == "pistol" || class == "smg" ) 
			self setSpreadOverride( 1 );
		else
			self setSpreadOverride( 2 );
		
		self player_recoilScaleOn( 0 );
		self allowJump(false);	
	}	
}

trackSiegeDissable()
{
	self endon ( "death" );
	self endon ( "disconnect" );
	self endon ( "stop_trackSiege" );
	
	for ( ;; )
	{
		self waittill ( "gambit_off" );
		
		unsetSiege();
	}	
}

stanceStateListener()
{
	self endon ( "death" );
	self endon ( "disconnect" );	
	
	self notifyOnPlayerCommand( "adjustedStance", "+stance" );

	for ( ;; )
	{
		self waittill( "adjustedStance" );
		if ( self.moveSPeedScaler != 0 )
			continue;
			
		unsetSiege();
	}
}

jumpStateListener()
{
	self endon ( "death" );
	self endon ( "disconnect" );	
	
	self notifyOnPlayerCommand( "jumped", "+goStand" );

	for ( ;; )
	{
		self waittill( "jumped" );
		if ( self.moveSPeedScaler != 0 )
			continue;
				
		unsetSiege();
	}
}

unsetSiege()
{
	self.moveSpeedScaler = 1;
	//if siege is not cut add check to see if
	//using lightweight and siege for movespeed scaler
	self resetSpreadOverride();
	self maps\mp\gametypes\_weapons::updateMoveSpeedScale( self getCurrentWeapon() );
	self player_recoilScaleOff();
	self allowJump(true);
}


setFinalStand()
{
	// Keep the selected perk3 ID; this deathstreak owns an independent ability.
	self setAbilityModifier( "modPistolDeath" );
}

unsetFinalStand()
{
	self unsetAbilityModifier( "modPistolDeath" );
}


setChallenger()
{
	if ( !level.hardcoreMode )
	{
		self.maxhealth = maps\mp\gametypes\_tweakables::getTweakableValue( "player", "maxhealth" );
		
		if ( isDefined( self.xpScaler ) && self.xpScaler == 1 && self.maxhealth > 30 )
		{		
			self.xpScaler = 2;
		}	
	}
}

unsetChallenger()
{
	self.xpScaler = 1;
}


setSaboteur()
{
	self.objectiveScaler = 1.2;
}

unsetSaboteur()
{
	self.objectiveScaler = 1;
}


setLightWeight()
{
	// Native coPerkStatsTable modifier owns Lightweight's movement scale.
	self.moveSpeedScaler = 1;
	self maps\mp\gametypes\_weapons::updateMoveSpeedScale( self getCurrentWeapon() );
}

unsetLightWeight()
{
	self.moveSpeedScaler = 1;
	self maps\mp\gametypes\_weapons::updateMoveSpeedScale( self getCurrentWeapon() );
}


setBlackBox()
{
	self.killStreakScaler = 1.5;
}

unsetBlackBox()
{
	self.killStreakScaler = 1;
}

setSteelNerves()
{
	self _setperk( "specialty_bulletaccuracy" );
	self _setperk( "specialty_holdbreath" );
}

unsetSteelNerves()
{
	self _unsetperk( "specialty_bulletaccuracy" );
	self _unsetperk( "specialty_holdbreath" );
}

setDelayMine()
{
}

unsetDelayMine()
{
}


setBackShield()
{
	self AttachShieldModel( "weapon_riot_shield_mp", "tag_shield_back" );	
}


unsetBackShield()
{
	self DetachShieldModel( "weapon_riot_shield_mp", "tag_shield_back" );
}


setLocalJammer()
{
	if ( !self isEMPed() )
		self RadarJamOn();
}


unsetLocalJammer()
{
	self RadarJamOff();
}


setAC130()
{
	self thread killstreakThink( "ac130", 7, "end_ac130Think" );
}

unsetAC130()
{
	self notify ( "end_ac130Think" );
}


setSentryMinigun()
{
	self thread killstreakThink( "airdrop_sentry_minigun", 2, "end_sentry_minigunThink" );
}

unsetSentryMinigun()
{
	self notify ( "end_sentry_minigunThink" );
}

setCarePackage()
{
	self thread killstreakThink( "airdrop", 2, "endCarePackageThink" );
}

unsetCarePackage()
{
	self notify ( "endCarePackageThink" );
}

setTank()
{
	self thread killstreakThink( "tank", 6, "end_tankThink" );
}

unsetTank()
{
	self notify ( "end_tankThink" );
}

setPrecision_airstrike()
{
	println( "!precision airstrike!" );
	self thread killstreakThink( "precision_airstrike", 6, "end_precision_airstrike" );
}

unsetPrecision_airstrike()
{
	self notify ( "end_precision_airstrike" );
}

setPredatorMissile()
{
	self thread killstreakThink( "predator_missile", 4, "end_predator_missileThink" );
}

unsetPredatorMissile()
{
	self notify ( "end_predator_missileThink" );
}


setHelicopterMinigun()
{
	self thread killstreakThink( "helicopter_minigun", 5, "end_helicopter_minigunThink" );
}

unsetHelicopterMinigun()
{
	self notify ( "end_helicopter_minigunThink" );
}



killstreakThink( streakName, streakVal, endonString )
{
	self endon ( "death" );
	self endon ( "disconnect" );
	self endon ( endonString );
	
	for ( ;; )
	{
		self waittill ( "killed_enemy" );
		
		if ( self.pers["cur_kill_streak"] != streakVal )
			continue;

		self thread maps\mp\killstreaks\_killstreaks::giveKillstreak( streakName );
		self thread maps\mp\gametypes\_hud_message::killstreakSplashNotify( streakName, streakVal );
		return;
	}
}


setThermal()
{
	self ThermalVisionOn();
}


unsetThermal()
{
	self ThermalVisionOff();
}


setOneManArmy()
{
	self thread oneManArmyWeaponChangeTracker();
}


unsetOneManArmy()
{
	self notify ( "stop_oneManArmyTracker" );
}


oneManArmyWeaponChangeTracker()
{
	self endon ( "death" );
	self endon ( "disconnect" );
	level endon ( "game_ended" );
	self endon ( "stop_oneManArmyTracker" );
	
	for ( ;; )
	{
		self waittill( "weapon_change", newWeapon );

		if ( newWeapon != "onemanarmy_mp" )	
			continue;
	
		//if ( self isUsingRemote() )
		//	continue;
		
		self thread selectOneManArmyClass();	
	}
}


isOneManArmyMenu( menu )
{
	if ( menu == game["menu_onemanarmy"] )
		return true;

	if ( isDefined( game["menu_onemanarmy_defaults_splitscreen"] ) && menu == game["menu_onemanarmy_defaults_splitscreen"] )
		return true;

	if ( isDefined( game["menu_onemanarmy_custom_splitscreen"] ) && menu == game["menu_onemanarmy_custom_splitscreen"] )
		return true;

	return false;
}


selectOneManArmyClass()
{
	self endon ( "death" );
	self endon ( "disconnect" );
	level endon ( "game_ended" );
	
	self _disableWeaponSwitch();
	self _disableOffhandWeapons();
	self _disableUsability();
	
	self openPopupMenu( game["menu_onemanarmy"] );
	
	self thread closeOMAMenuOnDeath();
	
	self waittill ( "menuresponse", menu, className );

	self _enableWeaponSwitch();
	self _enableOffhandWeapons();
	self _enableUsability();
	
	if ( className == "back" || !isOneManArmyMenu( menu ) || self isUsingRemote() )
	{
		if ( self getCurrentWeapon() == "onemanarmy_mp" )
		{
			self _disableWeaponSwitch();
			self _disableOffhandWeapons();
			self _disableUsability();
			self switchToWeapon( self getLastWeapon() );
			self waittill ( "weapon_change" );
			self _enableWeaponSwitch();
			self _enableOffhandWeapons();
			self _enableUsability();
		}
		return;
	}	
	
	self thread giveOneManArmyClass( className );	
}

closeOMAMenuOnDeath()
{
	self endon ( "menuresponse" );
	self endon ( "disconnect" );
	level endon ( "game_ended" );
	
	self waittill ( "death" );

	self _enableWeaponSwitch();
	self _enableOffhandWeapons();
	self _enableUsability();

	self closePopupMenu();
}

giveOneManArmyClass( className )
{
	self endon ( "death" );
	self endon ( "disconnect" );
	level endon ( "game_ended" );

	if ( self _hasPerk( "specialty_omaquickchange" ) )
	{
		changeDuration = 3.0;
		self playLocalSound( "foly_onemanarmy_bag3_plr" );
		self playSoundToTeam( "foly_onemanarmy_bag3_npc", "allies", self );
		self playSoundToTeam( "foly_onemanarmy_bag3_npc", "axis", self );
	}
	else
	{
		changeDuration = 6.0;
		self playLocalSound( "foly_onemanarmy_bag6_plr" );
		self playSoundToTeam( "foly_onemanarmy_bag6_npc", "allies", self );
		self playSoundToTeam( "foly_onemanarmy_bag6_npc", "axis", self );
	}
		
	self thread omaUseBar( changeDuration );
		
	self _disableWeapon();
	self _disableOffhandWeapons();
	self _disableUsability();
	
	wait ( changeDuration );

	self _enableWeapon();
	self _enableOffhandWeapons();
	self _enableUsability();
	
	self.OMAClassChanged = true;

	self maps\mp\gametypes\_class::giveLoadout( self.pers["team"], className, false );
	
	// handle the fact that detachAll in giveLoadout removed the CTF flag from our back
	// it would probably be better to handle this in _detachAll itself, but this is a safety fix
	if ( isDefined( self.carryFlag ) )
		self attach( self.carryFlag, "J_spine4", true );
	
	self notify ( "changed_kit" );
	level notify ( "changed_kit" );
}


omaUseBar( duration )
{
	self endon( "disconnect" );
	
	useBar = createPrimaryProgressBar( 25 );
	useBarText = createPrimaryProgressBarText( 25 );
	useBarText setText( &"MPUI_CHANGING_KIT" );

	useBar updateBar( 0, 1 / duration );
	for ( waitedTime = 0; waitedTime < duration && isAlive( self ) && !level.gameEnded; waitedTime += 0.05 )
		wait ( 0.05 );
	
	useBar destroyElem();
	useBarText destroyElem();
}


// sh1x adaptation: C1's blast shield is worn, not toggled. IW4 switched it on
// and off with the "empty_offhand" notify of a frag press on an empty primary
// offhand. C1's PM_Weapon_CheckForOffHand (0x41cba8..0x41cbc7) returns
// silently for a selected slot with no ammo, with no event, and its only
// EV_EMPTY_OFFHAND_PRIMARY producer (0x41c7dd..0x41c809) follows the use of
// an offhand that had no ammo left. C1 has no blast-shield offhand weapon, so
// a C1 player can never raise that notify. PERKS_DESC_BLASTSHIELD
// ("increased defence against explosive damage") describes a standing
// effect. Holding the equipment therefore keeps the IW4 blast-shield state
// (_specialty_blastshield) on, which feeds the existing damage, feedback and
// ch_masterblaster consumers.
setBlastShield()
{
	self _setPerk( "_specialty_blastshield" );
	self SetWeaponHudIconOverride( "primaryoffhand", "specialty_blastshield" );
}


unsetBlastShield()
{
	self notify ( "end_perkUseTracker" );
	if ( self _hasPerk( "_specialty_blastshield" ) )
		self _unsetPerk( "_specialty_blastshield" );
	self SetWeaponHudIconOverride( "primaryoffhand", "none" );
}

toggleBlastShield( isEnabled )
{
	if ( !isEnabled )
	{
		self VisionSetNakedForPlayer( "black_bw", 0.15 );
		wait ( 0.15 );
		self _setPerk( "_specialty_blastshield" );
		self VisionSetNakedForPlayer( getDvar( "mapname" ), 0 );
		self playSoundToPlayer( "item_blast_shield_on", self );
	}
	else
	{
		self VisionSetNakedForPlayer( "black_bw", 0.15 );
		wait ( 0.15 );	
		self _unsetPerk( "_specialty_blastshield" );
		self VisionSetNakedForPlayer( getDvar( "mapname" ), 0 );
		self playSoundToPlayer( "item_blast_shield_off", self );
	}
}


setFreefall()
{
	//eventually set a listener to do a roll when falling damage is taken
}

unsetFreefall()
{
}


setTacticalInsertion()
{
	self notify( "end_monitorTIUse" );
	self.TISpawnPosition = undefined;
	self _giveWeapon( "flare_mp", 0 );
	self giveStartAmmo( "flare_mp" );
	
	self thread monitorTIUse();
}

unsetTacticalInsertion()
{
	// IW4 only stops the monitor. A planted beacon outlives a mid-life class
	// change; spawnPlayer consumes it and clearPreviousTISpawnpoint removes it
	// on disconnect or team change.
	self notify( "end_monitorTIUse" );
}

clearPreviousTISpawnpoint()
{
    if ( !isDefined( self.codolTICleanupGeneration ) ) self.codolTICleanupGeneration = 0;
    self.codolTICleanupGeneration++;
    generation = self.codolTICleanupGeneration;
	self notify( "codol_ti_cleanup_monitor" );
	self endon( "codol_ti_cleanup_monitor" );
	self waittill_any ( "disconnect", "joined_team", "joined_spectators" );
    // A cancelled nested wait can currently resume its caller in this VM.
    if ( !isDefined( self ) || self.codolTICleanupGeneration != generation ) return;
	if ( isDefined ( self.setSpawnpoint ) )
		self deleteTI( self.setSpawnpoint );
}

updateTISpawnPosition()
{
	self endon ( "death" );
	self endon ( "disconnect" );
	level endon ( "game_ended" );
	self endon ( "end_monitorTIUse" );
	
	while ( isReallyAlive( self ) )
	{
		if ( self isValidTISpawnPosition() )
			self.TISpawnPosition = self.origin;

		wait ( 0.05 );
	}
}

isValidTISpawnPosition()
{
	if ( CanSpawn( self.origin ) && self IsOnGround() )
		return true;
	else
		return false;
}

monitorTIUse()
{
	self endon ( "death" );
	self endon ( "disconnect" );
	level endon ( "game_ended" );
	self endon ( "end_monitorTIUse" );

	self thread updateTISpawnPosition();
	self thread clearPreviousTISpawnpoint();
	
	for ( ;; )
	{
		self waittill( "grenade_fire", lightstick, weapName );
				
		if ( weapName != "flare_mp" )
			continue;
		
		// The beacon replaces the native placement missile; never leave both.
		if ( isDefined( lightstick ) ) lightstick delete();
		if ( !isDefined( self.TISpawnPosition ) || self touchingBadTrigger() )
		{
			self setWeaponAmmoStock( "flare_mp", self getWeaponAmmoStock( "flare_mp" ) + 1 );
			continue;
		}
		if ( isDefined( self.setSpawnPoint ) )
			self deleteTI( self.setSpawnPoint );

		TIGroundPosition = playerPhysicsTrace( self.TISpawnPosition + (0,0,16), self.TISpawnPosition - (0,0,2048) ) + (0,0,1);
		
		glowStick = spawn( "script_model", TIGroundPosition );
		glowStick.angles = self.angles;
		glowStick.team = self.team;
		glowStick.owner = self;
		glowStick.enemyTrigger =  spawn( "script_origin", TIGroundPosition );
		glowStick thread GlowStickSetupAndWaitForDeath( self );
		glowStick.playerSpawnPos = self.TISpawnPosition;
		
		glowStick thread maps\mp\gametypes\_weapons::createBombSquadModel( "weapon_light_stick_tactical_bombsquad", "tag_fire_fx", level.otherTeam[self.team], self );
		
		self.setSpawnPoint = glowStick;
		level thread codolTIEnd( glowStick );		
		// Keep listening after pickup; do not grant start ammo to restart placement.
	}
}

codolTIEnd( beacon )
{
    beacon endon( "death" );
    level waittill( "game_ended" );
    deleteTI( beacon );
}

GlowStickSetupAndWaitForDeath( owner )
{
	self setModel( level.spawnGlowModel["enemy"] );
	if ( level.teamBased )
		self maps\mp\_entityheadIcons::setTeamHeadIcon( self.team , (0,0,20) );
	else
		self maps\mp\_entityheadicons::setPlayerHeadIcon( owner, (0,0,20) );

	self thread GlowStickDamageListener( owner );
	self thread GlowStickEnemyUseListener( owner );
	self thread GlowStickUseListener( owner );
	self thread GlowStickTeamUpdater( level.otherTeam[self.team], level.spawnGlow["enemy"], owner );	

	dummyGlowStick = spawn( "script_model", self.origin+ (0,0,0) );
	dummyGlowStick.angles = self.angles;
	dummyGlowStick setModel( level.spawnGlowModel["friendly"] );
	dummyGlowStick setContents( 0 );
	dummyGlowStick thread GlowStickTeamUpdater( self.team, level.spawnGlow["friendly"], owner );
	
	dummyGlowStick playLoopSound( "emt_road_flare_burn" );

	self waittill ( "death" );
	
	dummyGlowStick stopLoopSound();
	dummyGlowStick delete();
}


GlowStickTeamUpdater( showForTeam, showEffect, owner )
{
	self endon ( "death" );
	
	// PlayFXOnTag fails if run on the same frame the parent entity was created
	wait ( 0.05 );
	
	//PlayFXOnTag( showEffect, self, "TAG_FX" );
	angles = self getTagAngles( "tag_fire_fx" );
	fxEnt = SpawnFx( showEffect, self getTagOrigin( "tag_fire_fx" ), anglesToForward( angles ), anglesToUp( angles ) );
	TriggerFx( fxEnt );
	
	self thread deleteOnDeath( fxEnt );
	
	for ( ;; )
	{
		self hide();
		fxEnt hide();
		foreach ( player in level.players )
		{
			if ( player.team == showForTeam && level.teamBased )
			{
				self showToPlayer( player );
				fxEnt showToPlayer( player );
			}
			else if ( !level.teamBased && player == owner && showEffect == level.spawnGlow["friendly"] )
			{
				self showToPlayer( player );
				fxEnt showToPlayer( player );
			}
			else if ( !level.teamBased && player != owner && showEffect == level.spawnGlow["enemy"] )
			{
				self showToPlayer( player );
				fxEnt showToPlayer( player );
			}
		}
		
		level waittill_either ( "joined_team", "player_spawned" );
	}
}

deleteOnDeath( ent )
{
	self waittill( "death" );
	if ( isdefined( ent ) )
		ent delete();
}

GlowStickDamageListener( owner )
{
	self endon ( "death" );

	self setCanDamage( true );
	// use large health to work around teamkilling issue
	self.health = 5000;

	for ( ;; )
	{
		self waittill ( "damage", amount, attacker );

		if ( level.teambased && isDefined( owner ) && isDefined( attacker ) && attacker != owner && ( isDefined( attacker.team ) && attacker.team == self.team ) )
		{
			self.health += amount;
			continue;
		}
		
		if ( self.health < (5000-20) )
		{
			if ( isDefined( owner ) && isDefined( attacker ) && attacker != owner )
			{
				attacker notify ( "destroyed_insertion", owner );
				attacker notify( "destroyed_explosive" ); // count towards SitRep Pro challenge
				owner thread leaderDialogOnPlayer( "ti_destroyed" );
			}
			
			deleteTI( self );
			return;
		}
	}
}

GlowStickUseListener( owner )
{
    pickedUp = self maps\mp\equipment\_pickup::waitForPickup( self, owner, "flare_mp", &"MP_PICKUP_TI" );
    if ( isDefined( pickedUp ) && pickedUp && isDefined( self ) && isDefined( owner ) )
    {
        owner playSound( "chemlight_pu" );
        deleteTI( self );
    }
}

updateEnemyUse( owner )
{
	self endon ( "death" );
	
	for ( ;; )
	{
		self setSelfUsable( owner );
		level waittill_either ( "joined_team", "player_spawned" );
	}
}

deleteTI( TI )
{
    // Death listeners must not terminate their own deletion path.
    level thread codolDeleteTIDetached( TI );
}

codolDeleteTIDetached( TI )
{
    if ( !isDefined( TI ) ) return;
    if ( isDefined( TI.owner ) && isDefined( TI.owner.setSpawnPoint ) && TI.owner.setSpawnPoint == TI )
        TI.owner.setSpawnPoint = undefined;
    spot = TI.origin;
    spotAngles = TI.angles;
    TI maps\mp\equipment\_pickup::close();
    TI notify( "death" );
    if ( isDefined( TI.enemyTrigger ) ) TI.enemyTrigger delete();
    TI delete();
    // Retain the IW4 spent-stick disappearance cosmetic after device cleanup.
    dummyGlowStick = spawn( "script_model", spot );
    dummyGlowStick.angles = spotAngles;
    dummyGlowStick setModel( level.spawnGlowModel["friendly"] );
    dummyGlowStick setContents( 0 );
    level thread dummyGlowStickDelete( dummyGlowStick );
}

dummyGlowStickDelete( stick )
{
	wait(2.5);
	stick Delete();
}

GlowStickEnemyUseListener( owner )
{
	self endon ( "death" );
	level endon ( "game_ended" );
	owner endon ( "disconnect" );
	
	self.enemyTrigger setCursorHint( "HINT_NOICON" );
	self.enemyTrigger setHintString( &"MP_DESTROY_TI" );
	self.enemyTrigger makeEnemyUsable( owner );
	
	for ( ;; )
	{
		self.enemyTrigger waittill ( "trigger", player );
		if ( !isDefined( player ) || !isAlive( player ) || player == owner ) continue;
		if ( level.teamBased && isDefined( owner ) && player.team == owner.team ) continue;
		player notify ( "destroyed_insertion", owner );
		player notify( "destroyed_explosive" ); // count towards SitRep Pro challenge

		//playFX( level.spawnGlowSplat, self.origin);		
		
		if ( isDefined( owner ) && player != owner )
			owner thread leaderDialogOnPlayer( "ti_destroyed" );

		player thread deleteTI( self );
	}	
}

setLittlebirdSupport()
{
	self thread killstreakThink( "littlebird_support", 2, "end_littlebird_support_think" );
}

unsetLittlebirdSupport()
{
	self notify ( "end_littlebird_support_think" );
}

setC4Death()
{
	if ( ! self _hasperk( "specialty_pistoldeath" ) )
		self _setperk( "specialty_pistoldeath");
}

unsetC4Death()
{

}


// Recon (specialty_paint / specialty_paint_pro): a victim hit by a painter is
// marked on the painter's minimap for a while.
// PROVENANCE: IW5 maps/mp/perks/_perkfunctions.gsc setPainted / unsetPainted /
// watchPaintedDeath / isPainted (alpha0022 vendor/iw5), adapted to C1 abilities.
//  * IW5 marks with setPerk("specialty_radararrow"). C1 carries the mark as
//    ability 24 modRadarArrow: its compass (C1 0x4348c2..0x43492b) draws an
//    enemy that has ability 24 for a viewer that has 37 modExplosivePaint or
//    38 modBulletPaint, and its value slot 13 selects the viewer - 0 is any
//    such viewer, N only client N-1. The table clamps the value to 0..20,
//    one above C1's 19 client slots, so the producer writes the painter's
//    client number plus one: the mark belongs to whoever painted last, as
//    IW5's "painted_again" restart already implies.
//  * Duration: IW5 uses 10 seconds, halved for a victim with
//    specialty_quieter. C1 moved that halving into the table: the quieter
//    row (Ninja Pro, PERKS_DESC_NINJA_PRO "reduces the time you are marked by
//    enemy Recon") selects modAntiPaint -0.5 on a default of 1, so the
//    victim's selected modAntiPaint value is the multiplier. The 10 s base is
//    IW5's; no C1 script or binary value for it has been recovered.
//  * Script-owned abilities persist across lives here (IW5's setPerk was
//    wiped by the next spawn's clearPerks), so death clears the mark as well.
setPainted( painter )
{
	if ( !isPlayer( self ) || !isPlayer( painter ) )
		return;

	duration = 10.0;
	antiPaint = self getSelectedPerkModifierValue( "modAntiPaint" );
	if ( isDefined( antiPaint ) )
		duration *= antiPaint;

	self.painted = true;
	self setAbilityModifier( "modRadarArrow" );
	self setAbilityModifierValue( "modRadarArrow", painter getEntityNumber() + 1 );
	codolPrintLn( "CoDOL paint: " + painter.name + " painted " + self.name + " for " + duration + "s" );
	self thread unsetPaintedAfter( duration );
}

unsetPaintedAfter( duration )
{
	self notify( "painted_again" );
	self endon( "painted_again" );
	self endon( "disconnect" );

	self waittill_any_timeout( duration, "death" );

	self.painted = false;
	self unsetAbilityModifier( "modRadarArrow" );
	codolPrintLn( "CoDOL paint: cleared " + self.name );
}

isPainted()
{
	return isDefined( self.painted ) && self.painted;
}
