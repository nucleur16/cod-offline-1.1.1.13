#include maps\mp\_utility;
#include maps\mp\gametypes\_hud_util;
#include common_scripts\utility;

SENTRY_MODE_ON			= "sentry";
SENTRY_MODE_OFF			= "sentry_offline";
SENTRY_TIME_OUT			= 90.0;
SENTRY_SPINUP_TIME		= .05;
SENTRY_OVERHEAT_TIME	= 8.0;
SENTRY_FX_TIME			= .3;

init()
{
	level.sentryType = [];
	level.sentryType[ "sentry_minigun" ] 	= "sentry";

	level.killStreakFuncs[ level.sentryType[ "sentry_minigun" ] ] 	= ::tryUseAutoSentry;

	// SAM turret (catalogue ID 21 "sam_turret", activation weapon auto_tow_mp).
	// Adapted from IW5 maps/mp/killstreaks/_autosentry.gsc (plutoniummod
	// iw5-scripts 42f3db22: tryusesam, sam_attacktargets, sam_acquiretarget,
	// sam_fireontarget, sam_watch*). C1 ships the T5 turret, not IW5's:
	// weapon tow_turret_mp (projectile turret, 1000 damage), models
	// t5_weapon_sam_turret{,_yellow,_red}. That skeleton has tag_aim and
	// tag_flash but none of IW5's tag_laser / tag_le_missile* tags
	// (codol_checkmodel, 2026-09-24), so sight and launch use those two.
	level.sentryType[ "sam_turret" ] = "sam_turret";
	level.killStreakFuncs[ level.sentryType[ "sam_turret" ] ] = ::tryUseSAM;

	level.sentrySettings = [];

	level.sentrySettings[ "sentry_minigun" ] = spawnStruct();
	level.sentrySettings[ "sentry_minigun" ].burstMin = 20;
	level.sentrySettings[ "sentry_minigun" ].burstMax = 120;
	level.sentrySettings[ "sentry_minigun" ].pauseMin = 0.15;
	level.sentrySettings[ "sentry_minigun" ].pauseMax = 0.35;
	level.sentrySettings[ "sentry_minigun" ].weaponInfo = "sentry_minigun_mp";
	level.sentrySettings[ "sentry_minigun" ].modelBase = "sentry_minigun";
	level.sentrySettings[ "sentry_minigun" ].modelPlacement = "sentry_minigun_obj";
	level.sentrySettings[ "sentry_minigun" ].modelPlacementFailed = "sentry_minigun_obj_red";
	level.sentrySettings[ "sentry_minigun" ].modelDestroyed = "sentry_minigun_destroyed";

	level.sentrySettings[ "sam_turret" ] = spawnStruct();
	level.sentrySettings[ "sam_turret" ].weaponInfo = "tow_turret_mp";
	level.sentrySettings[ "sam_turret" ].modelBase = "t5_weapon_sam_turret";
	level.sentrySettings[ "sam_turret" ].modelPlacement = "t5_weapon_sam_turret_yellow";
	level.sentrySettings[ "sam_turret" ].modelPlacementFailed = "t5_weapon_sam_turret_red";
	level.sentrySettings[ "sam_turret" ].modelDestroyed = "t5_weapon_sam_turret";

	// Sitrep's modDetectTurrets (PERKS_DESC_SITREP: "detects enemy explosives,
	// turrets and tactical equipment"). C1's common_gametype_mp ships a bomb
	// squad model for both turrets, sentry_minigun_bombsquad and
	// t5_weapon_sam_turret_bombsquad, next to the explosive ones IW4 already
	// uses; they are shown to enemy detectors like the IW4 explosive models.
	level.sentrySettings[ "sentry_minigun" ].modelBombSquad = "sentry_minigun_bombsquad";
	level.sentrySettings[ "sam_turret" ].modelBombSquad = "t5_weapon_sam_turret_bombsquad";

	foreach ( sentryInfo in level.sentrySettings )
	{
		precacheItem( sentryInfo.weaponInfo );
		precacheModel( sentryInfo.modelBase );
		precacheModel( sentryInfo.modelPlacement );
		precacheModel( sentryInfo.modelPlacementFailed );
		precacheModel( sentryInfo.modelDestroyed );
		precacheModel( sentryInfo.modelBombSquad );
	}

	level._effect[ "sentry_overheat_mp" ]	= loadfx( "smoke/sentry_turret_overheat_smoke" );
	level._effect[ "sentry_explode_mp" ]	= loadfx( "explosions/sentry_gun_explosion" );
	level._effect[ "sentry_smoke_mp" ]		= loadfx( "smoke/car_damage_blacksmoke" );
}

/* ============================
	Killstreak Functions
   ============================ */

tryUseAutoSentry( lifeId )
{
	result = self giveSentry( "sentry_minigun" );
	if ( result )
		self maps\mp\_matchdata::logKillstreakEvent( "sentry", self.origin );

	return ( result );
}


tryUseSAM( lifeId )
{
	result = self giveSentry( "sam_turret" );
	if ( result )
		self maps\mp\_matchdata::logKillstreakEvent( "sam_turret", self.origin );

	return ( result );
}


tryUseAutoGlSentry( lifeId )
{
	result = self giveSentry( "sentry_gun" );
	if ( result )
		self maps\mp\_matchdata::logKillstreakEvent( "sentry_gl", self.origin );

	return ( result );
}


giveSentry( sentryType )
{
	self.last_sentry = sentryType;

	sentryGun = createSentryForPlayer( sentryType, self );

	self setCarryingSentry( sentryGun, true );

	// if we failed to place the sentry, it will have been deleted at this point
	if ( isDefined( sentryGun ) )
		return true;
	else
		return false;
}


/* ============================
	Player Functions
   ============================ */


setCarryingSentry( sentryGun, allowCancel )
{
	self endon ( "death" );
	self endon ( "disconnect" );

	assert( isReallyAlive( self ) );

	// CoDOL: a placed turret picked up again (allowCancel false) is put back
	// where it stood on cancel, instead of IW4's no-cancel carry.
	restoreOrigin = undefined;
	restoreAngles = undefined;
	if ( !allowCancel )
	{
		restoreOrigin = sentryGun.origin;
		restoreAngles = sentryGun.angles;
	}

	sentryGun sentry_setCarried( self );

	self _disableWeapon();

	self notifyOnPlayerCommand( "place_sentry", "+attack" );
	self notifyOnPlayerCommand( "cancel_sentry", "+actionslot 4" );
	// C1 binds +actionslot 4 to key 7, a killstreak key, so IW4's cancel is
	// undiscoverable there. The weapon-switch commands (keys 1/2/3 and the
	// mouse wheel: weapnext, weapprev, weapalt) also cancel; with the weapon
	// disabled they have nothing else to do.
	self notifyOnPlayerCommand( "cancel_sentry", "weapnext" );
	self notifyOnPlayerCommand( "cancel_sentry", "weapprev" );
	self notifyOnPlayerCommand( "cancel_sentry", "weapalt" );
	// So does every other reward's action slot, as IW4's killstreak key
	// (+actionslot 4) does: C1 cannot raise that reward's weapon while this
	// carry holds the weapons down, so the key would otherwise do nothing.
	// The pressed reward is raised once the carry is cancelled
	// (_killstreaks::codolRunRewardUse).
	for ( slot = 1; slot <= 5; slot++ )
		self notifyOnPlayerCommand( "cancel_sentry", "+actionslot " + slot );
	self thread sentry_carryHint( allowCancel );

	for ( ;; )
	{
		result = waittill_any_return( "place_sentry", "cancel_sentry" );

		if ( result == "cancel_sentry" )
		{
			if ( !allowCancel )
			{
				// Put it back only where nobody now stands in it.
				if ( !sentry_canRestore( restoreOrigin ) )
					continue;
				sentryGun.origin = restoreOrigin;
				sentryGun.angles = restoreAngles;
				sentryGun sentry_setPlaced();
				self _enableWeapon();
				self notify( "codol_sentry_carry_done" );
				return true;
			}

			sentryGun sentry_setCancelled();
			self _enableWeapon();
			self notify( "codol_sentry_carry_done" );
			return false;
		}

		if ( !sentryGun.canBePlaced )
			continue;

		sentryGun sentry_setPlaced();
		self _enableWeapon();
		self notify( "codol_sentry_carry_done" );

		return true;
	}
}


// The cancel hint, repeated while the carry lasts (placed, cancelled, put
// back, death, game end). C1 ships no localized string naming these keys and
// a HUD text element takes only localize keys, so this is a plain bold print;
// C1 replaces [{command}] with the player's own binding when it draws it.
sentry_carryHint( allowCancel )
{
	self endon( "disconnect" );
	self endon( "death" );
	self endon( "codol_sentry_carry_done" );
	level endon( "game_ended" );

	for ( ;; )
	{
		if ( allowCancel )
			self iPrintLnBold( "Press ^3[{+actionslot 4}]^7 or ^3[{weapnext}]^7 to cancel" );
		else
			self iPrintLnBold( "Press ^3[{+actionslot 4}]^7 or ^3[{weapnext}]^7 to put the turret back" );
		wait ( 4.0 );
	}
}


sentry_canRestore( origin )
{
	if ( !isDefined( origin ) )
		return false;
	foreach ( player in level.players )
	{
		if ( player == self || !isReallyAlive( player ) )
			continue;
		if ( distance2D( player.origin, origin ) < 40 && abs( player.origin[2] - origin[2] ) < 72 )
			return false;
	}
	return true;
}


/* ============================
	Sentry Functions
   ============================ */

createSentryForPlayer( sentryType, owner )
{
	assertEx( isDefined( owner ), "createSentryForPlayer() called without owner specified" );

	sentryGun = spawnTurret( "misc_turret", owner.origin, level.sentrySettings[ sentryType ].weaponInfo );
	sentryGun.angles = owner.angles;

	sentryGun sentry_initSentry( sentryType, owner );

	return ( sentryGun );
}


sentry_initSentry( sentryType, owner )
{
	self.sentryType = sentryType;

	self setModel( level.sentrySettings[ self.sentryType ].modelBase );
	self.health = 1000;

	self setCanDamage( true );
	self makeTurretInoperable();

	self setTurretModeChangeWait( true );
//	self setConvergenceTime( .25, "pitch" );
//	self setConvergenceTime( .25, "yaw" );
	self sentry_setInactive();
	self setDefaultDropPitch( -89.0 );	// setting this mainly prevents Turret_RestoreDefaultDropPitch() from running

	if ( self.sentryType == "sam_turret" )
	{
		// IW5 sentry_initsentry "sam_turret": full yaw, steep top arc.
		self setLeftArc( 180 );
		self setRightArc( 180 );
		self setTopArc( 80 );
		// T5 _turret_killstreak.gsc (tow turret, the model and weapon C1
		// ships): "Set the scan angle to look at the sky." Without it the
		// idle sweep (C1 0x57b610) steers the launcher to scanning pitch 0
		// and scans level, like the sentry gun. Negative pitch is up.
		self setScanningPitch( -35.0 );
	}

	self sentry_setOwner( owner );

	bombSquadTeam = undefined;
	if ( level.teamBased )
		bombSquadTeam = level.otherTeam[ self.team ];
	self thread maps\mp\gametypes\_weapons::createBombSquadModel( level.sentrySettings[ self.sentryType ].modelBombSquad, undefined, bombSquadTeam, owner, "modDetectTurrets" );

	self thread sentry_handleOwner();
	self thread sentry_handleDamage();
	self thread sentry_handleDeath();
	self thread sentry_handleUse();
	self thread sentry_timeOut();
	// The SAM never auto-engages ground targets; sam_attackTargets drives it
	// from sentry_setActive once it is placed.
	if ( self.sentryType != "sam_turret" )
		self thread sentry_attackTargets();
	self thread sentry_beepSounds();
}


/* ============================
	Sentry Handlers
   ============================ */

sentry_handleDamage()
{
	// use a health buffer to prevent the turret from dying to friendly fire
	healthBuffer = 20000;
	self.health += healthbuffer;

	while ( self.health > 0 )
	{
		self waittill( "damage", amount, attacker, dir, point, type, model, tag, part, flags, weapon );

		if ( isDefined( attacker ) && isPlayer( attacker ) && attacker != self.owner && attacker isFriendlyToSentry( self ) && !isDefined( level.nukeDetonated ) )
		{
			self.health += amount;
			continue;
		}

		// IW5 EMP grenade destroys the sentry through its existing death path.
		// Native damage notify supplies the exact weapon as argument ten.
		if ( isDefined( weapon ) && weapon == "emp_grenade_mp" )
			self.health = 0;

		// 7x damage for explosives - GRENADES
		if ( isExplosiveDamage( type ) )
			self.health -= (amount * 1);

		if ( type == "MOD_MELEE" )
			self.health = 0;

		if ( isPlayer( attacker ) )
		{
			attacker maps\mp\gametypes\_damagefeedback::updateDamageFeedback( "sentry" );

			if ( attacker _hasPerk( "specialty_armorpiercing" ) )
			{
				damageAdd = amount*level.armorPiercingMod;
				self.health -= int(damageAdd);
			}
		}

		if ( self.health - healthbuffer < 0 )
		{
			thread maps\mp\gametypes\_missions::vehicleKilled( self.owner, self, undefined, attacker, amount, type );

			if ( isPlayer( attacker ) && (!isDefined(self.owner) || attacker != self.owner) )
			{
				attacker thread maps\mp\gametypes\_rank::giveRankXP( "kill", 100 );
				attacker notify( "destroyed_killstreak" );
			}

			if ( isDefined( self.owner ) )
				self.owner thread leaderDialogOnPlayer( "sentry_destroyed" );

			self notify ( "death" );
			return;
		}
	}
}

sentry_handleDeath()
{
	entNum = self GetEntityNumber();

	self addToTurretList( entNum );

	self waittill ( "death" );

	self removeFromTurretList( entNum );
	// this handles cases of deletion
	if ( !isDefined( self ) )
		return;

	self setModel( level.sentrySettings[ self.sentryType ].modelDestroyed );

	self sentry_setInactive();
	self setDefaultDropPitch( 40 );
	self SetSentryOwner( undefined );
	self SetTurretMinimapVisible( false );

	self playSound( "sentry_explode" );
	playFxOnTag( getFx( "sentry_explode_mp" ), self, "tag_aim" );

	// A wreck takes no more damage. Every hit on it would otherwise notify
	// "death" again natively once its health is at or below zero.
	self setCanDamage( false );

	// IW4 ended this thread on any further "death" to notice a deletion by
	// other means. But "death" also comes again on its own: a melee or EMP
	// kill sets health to 0 inside the damage notify, and the native damage
	// path reads that health right after the notify and sends "death" a
	// second time (G_Damage). That ended this thread before the delete, so
	// a knifed sentry or SAM never went away. Check for the deletion
	// instead, after every wait (IW5 dropped the endon too).
	wait ( 1.5 );
	if ( !isDefined( self ) )
		return;

	self playSound( "sentry_explode_smoke" );
	for ( smokeTime = 8; smokeTime > 0; smokeTime -= 0.4 )
	{
		playFxOnTag( getFx( "sentry_smoke_mp" ), self, "tag_aim" );
		wait ( 0.4 );
		if ( !isDefined( self ) )
			return;
	}

	self delete();
}


sentry_handleUse()
{
	self endon ( "death" );
	level endon ( "game_ended" );

	for ( ;; )
	{
		self waittill ( "trigger", player );

		assert( player == self.owner );
		assert( !isDefined( self.carriedBy ) );

		// IW4 only asserts this: disablePlayerUse keeps everybody else off.
		// A player who joined after the turret was placed was never
		// disabled, so refuse anyone but the owner here as well.
		if ( !isDefined( self.owner ) || player != self.owner || isDefined( self.carriedBy ) )
			continue;

		if ( !isReallyAlive( player ) )
			continue;

		player setCarryingSentry( self, false );
	}
}


sentry_handleOwner()
{
	self endon ( "death" );
	level endon ( "game_ended" );

	self notify ( "sentry_handleOwner" );
	self endon ( "sentry_handleOwner" );

	self.owner waittill_any( "disconnect", "joined_team", "joined_spectators" );

	self notify( "death" );
}


/* ============================
	Sentry Utility Functions
   ============================ */

sentry_setOwner( owner )
{
	assertEx( isDefined( owner ), "sentry_setOwner() called without owner specified" );
	assertEx( isPlayer( owner ), "sentry_setOwner() called on non-player entity type: " + owner.classname );

	self.owner = owner;

	self SetSentryOwner( self.owner );
	self SetTurretMinimapVisible( true );

	// The SAM turret acquires nothing natively: in "sentry" mode a turret
	// with no turret team never auto-targets (C1 0x57cd16), so it sweeps
	// and follows only the aircraft sam_attackTargets hands it through
	// setTargetEntity, and never swings its launcher at ground players.
	if ( self.sentryType == "sam_turret" )
	{
		if ( level.teamBased )
			self.team = self.owner.team;
	}
	else if ( level.teamBased )
	{
		self.team = self.owner.team;
		self setTurretTeam( self.team );
	}
	else
	{
		// CoDOL: C1 acquires nothing for a turret without a team (0x57cd16),
		// where IW4's free-for-all sentry shot at anyone. C1's FFA turret is
		// "team3": it takes every free-for-all player and skips its owner
		// (setSentryOwner above, 0x57cde9).
		self setTurretTeam( "team3" );
	}

	self thread sentry_handleOwner();
}


sentry_setPlaced()
{
	self setModel( level.sentrySettings[ self.sentryType ].modelBase );

	self setSentryCarried( false );
	self setCanDamage( true );
	self sentry_makeSolid();

	self.carriedBy forceUseHintOff();
	self.carriedBy = undefined;

	self sentry_setActive();

	self playSound( "sentry_gun_plant" );

	self notify ( "placed" );
}


sentry_setCancelled()
{
	self.carriedBy forceUseHintOff();

	self sentry_delete();
}


sentry_setCarried( carrier )
{
	assert( isPlayer( carrier ) );
	assertEx( carrier == self.owner, "sentry_setCarried() specified carrier does not own this sentry" );

	self setModel( level.sentrySettings[ self.sentryType ].modelPlacement );

	self setSentryCarried( true );
	self setCanDamage( false );
	self sentry_makeNotSolid();

	self.carriedBy = carrier;

	carrier thread updateSentryPlacement( self );

	self thread sentry_onCarrierDeath( carrier );
	self thread sentry_onCarrierDisconnect( carrier );
	self thread sentry_onGameEnded();

	self sentry_setInactive();

	self notify ( "carried" );
}

updateSentryPlacement( sentryGun )
{
	self endon ( "death" );
	self endon ( "disconnect" );
	level endon ( "game_ended" );

	sentryGun endon ( "placed" );
	sentryGun endon ( "death" );

	sentryGun.canBePlaced = true;
	lastCanPlaceSentry = -1; // force initial update

	for( ;; )
	{
		placement = self canPlayerPlaceSentry();

		sentryGun.origin = placement[ "origin" ];
		sentryGun.angles = placement[ "angles" ];
		sentryGun.canBePlaced = self isOnGround() && placement[ "result" ];

		if ( sentryGun.canBePlaced != lastCanPlaceSentry )
		{
			if ( sentryGun.canBePlaced )
			{
				sentryGun setModel( level.sentrySettings[ sentryGun.sentryType ].modelPlacement );
				self ForceUseHintOn( &"SENTRY_PLACE" );
			}
			else
			{
				sentryGun setModel( level.sentrySettings[ sentryGun.sentryType ].modelPlacementFailed );
				self ForceUseHintOn( &"SENTRY_CANNOT_PLACE" );
			}
		}

		lastCanPlaceSentry = sentryGun.canBePlaced;
		wait ( 0.05 );
	}
}

sentry_onCarrierDeath( carrier )
{
	self endon ( "placed" );
	self endon ( "death" );

	carrier waittill ( "death" );

	if ( self.canBePlaced )
		self sentry_setPlaced();
	else
		self sentry_delete();
}


sentry_onCarrierDisconnect( carrier )
{
	self endon ( "placed" );
	self endon ( "death" );

	carrier waittill ( "disconnect" );

	self sentry_delete();
}

sentry_onGameEnded( carrier )
{
	self endon ( "placed" );
	self endon ( "death" );

	level waittill ( "game_ended" );

	self sentry_delete();
}


sentry_setActive()
{
	// IW5 runs the placed SAM in "sentry" mode too (sentrymodeon). Only
	// that mode sweeps the launcher round while it has no target (C1
	// 0x57d023: sentry and not offline); in sentry_offline the SAM sat
	// motionless. It has no turret team (sentry_setOwner), so it still
	// acquires no ground target of its own.
	self setMode( SENTRY_MODE_ON );

	self setCursorHint( "HINT_NOICON" );
	self setHintString( &"SENTRY_PICKUP" );

	self makeUsable();

	foreach ( player in level.players )
	{
		if ( player == self.owner )
			self enablePlayerUse( player );
		else
			self disablePlayerUse( player );
	}

	if ( level.teamBased )
		self maps\mp\_entityheadicons::setTeamHeadIcon( self.team, (0,0,65) );
	else
		self maps\mp\_entityheadicons::setPlayerHeadIcon( self.owner, (0,0,65) );

	if ( self.sentryType == "sam_turret" )
		self thread sam_attackTargets();
}


sentry_setInactive()
{
	self setMode( SENTRY_MODE_OFF );
	self makeUnusable();

	if ( level.teamBased )
		self maps\mp\_entityheadicons::setTeamHeadIcon( "none", ( 0, 0, 0 ) );
	else if ( isDefined( self.owner ) )
		self maps\mp\_entityheadicons::setPlayerHeadIcon( undefined, ( 0, 0, 0 ) );
}


sentry_makeSolid()
{
	self makeTurretSolid();
}


sentry_makeNotSolid()
{
	self setContents( 0 );
}


isFriendlyToSentry( sentryGun )
{
	if ( level.teamBased && self.team == sentryGun.team )
		return true;

	return false;
}


addToTurretList( entNum )
{
	level.turrets[entNum] = self;
}


removeFromTurretList( entNum )
{
	level.turrets[entNum] = undefined;
}


// CoDOL: a turret deleted while carried (cancelled, dropped where it cannot
// stand, owner gone, game over) never reaches sentry_handleDeath, whose
// "death" wait dies with the entity, so its level.turrets slot kept a freed
// entity that EMP and spawn-logic iterators then touched. Leave the list
// first, without the destroyed-turret effects a "death" notify would play.
sentry_delete()
{
	self removeFromTurretList( self getEntityNumber() );
	self delete();
}

/* ============================
	Sentry Logic Functions
   ============================ */

sentry_attackTargets()
{
	self endon( "death" );
	level endon( "game_ended" );

	self.momentum = 0;
	self.heatLevel = 0;
	self.overheated = false;

	self thread sentry_heatMonitor();

	for ( ;; )
	{
		self waittill_either( "turretstatechange", "cooled" );

		if ( self isFiringTurret() )
		{
			self thread sentry_burstFireStart();
		}
		else
		{
			self sentry_spinDown();
			self thread sentry_burstFireStop();
		}
	}
}


sentry_timeOut()
{
	self endon( "death" );
	level endon ( "game_ended" );

	lifeSpan = SENTRY_TIME_OUT;

	while ( lifeSpan )
	{
		wait ( 1.0 );
		maps\mp\gametypes\_hostmigration::waitTillHostMigrationDone();

		if ( !isDefined( self.carriedBy ) )
			lifeSpan = max( 0, lifeSpan - 1.0 );
	}

	if ( isDefined( self.owner ) )
		self.owner thread leaderDialogOnPlayer( "sentry_gone" );

	self notify ( "death" );
}

sentry_targetLockSound()
{
	self endon ( "death" );

	self playSound( "sentry_gun_beep" );
	wait ( 0.1 );
	self playSound( "sentry_gun_beep" );
	wait ( 0.1 );
	self playSound( "sentry_gun_beep" );
}

sentry_spinUp()
{
	self thread sentry_targetLockSound();

	while ( self.momentum < SENTRY_SPINUP_TIME )
	{
		self.momentum += 0.1;

		wait ( 0.1 );
	}
}

sentry_spinDown()
{
	self.momentum = 0;
}


sentry_burstFireStart()
{
	self endon( "death" );
	self endon( "stop_shooting" );

	level endon( "game_ended" );

	self sentry_spinUp();

	fireTime = weaponFireTime( level.sentrySettings[ self.sentryType ].weaponInfo );
	minShots = level.sentrySettings[ self.sentryType ].burstMin;
	maxShots = level.sentrySettings[ self.sentryType ].burstMax;
	minPause = level.sentrySettings[ self.sentryType ].pauseMin;
	maxPause = level.sentrySettings[ self.sentryType ].pauseMax;

	for ( ;; )
	{
		numShots = randomIntRange( minShots, maxShots + 1 );

		for ( i = 0; i < numShots && !self.overheated; i++ )
		{
			self shootTurret();
			self.heatLevel += fireTime;
			wait ( fireTime );
		}

		wait ( randomFloatRange( minPause, maxPause ) );
	}
}


sentry_burstFireStop()
{
	self notify( "stop_shooting" );
}


sentry_heatMonitor()
{
	self endon ( "death" );

	fireTime = weaponFireTime( level.sentrySettings[ self.sentryType ].weaponInfo );

	lastHeatLevel = 0;
	lastFxTime = 0;

	for ( ;; )
	{
		if ( self.heatLevel != lastHeatLevel )
			wait ( fireTime );
		else
			self.heatLevel = max( 0, self.heatLevel - 0.05 );

		if ( self.heatLevel > SENTRY_OVERHEAT_TIME )
		{
			self.overheated = true;
			self thread PlayHeatFX();

			while ( self.heatLevel )
			{
				self.heatLevel = max( 0, self.heatLevel - 0.1 );
				wait ( 0.1 );
			}

			self.overheated = false;
			self notify( "not_overheated" );
		}

		lastHeatLevel = self.heatLevel;
		wait ( 0.05 );
	}
}

playHeatFX()
{
	self endon( "death" );
	self endon( "not_overheated" );
	level endon ( "game_ended" );

	for( ;; )
	{
		playFxOnTag( getFx( "sentry_overheat_mp" ), self, "tag_flash" );

		wait( SENTRY_FX_TIME );
	}
}

sentry_beepSounds()
{
	self endon( "death" );
	level endon ( "game_ended" );

	for ( ;; )
	{
		wait ( 3.0 );

		if ( !isDefined( self.carriedBy ) )
			self playSound( "sentry_gun_beep" );
	}
}


/* ============================
	SAM Turret Logic
	Adapted from IW5 _autosentry.gsc sam_* (see init). Targets are this
	tree's aircraft: UAV / counter-UAV models, level.helis (attack and
	support helicopters, pave low, harriers) and the manned AC-130.
   ============================ */

sam_attackTargets()
{
	self endon( "carried" );
	self endon( "death" );
	level endon( "game_ended" );

	self.samTargetEnt = undefined;
	{}

	for ( ;; )
	{
		self.samTargetEnt = self sam_acquireTarget();
		self sam_fireOnTarget();
		wait ( 0.05 );
	}
}


sam_isEnemyAircraft( aircraft )
{
	if ( !isDefined( aircraft ) )
		return false;

	if ( level.teamBased )
	{
		if ( isDefined( aircraft.team ) && isDefined( self.team ) && aircraft.team == self.team )
			return false;
	}
	else if ( isDefined( aircraft.owner ) && isDefined( self.owner ) && aircraft.owner == self.owner )
	{
		return false;
	}

	return true;
}


// IW5 calls sightTracePassed( start, end, false, self, aircraft ) so the
// aircraft's own collision is not an obstruction. This VM's sightTracePassed
// is the retained IW3 four-argument builtin, which cannot ignore a second
// entity; a vehicle target then blocks its own sight line and the lock never
// survives the lock-on time (measured: 14 locks, 0 salvos against a helicopter).
// bulletTrace reports what it hit, so "reached the end or hit the aircraft"
// is the same test.
sam_canSee( aircraft )
{
	trace = bulletTrace( self getTagOrigin( "tag_aim" ), aircraft.origin, false, self );
	if ( trace["fraction"] >= 1.0 )
		return true;

	return ( isDefined( trace["entity"] ) && trace["entity"] == aircraft );
}


sam_acquireTarget()
{
	if ( isDefined( self.samTargetEnt ) )
	{
		if ( self sam_canSee( self.samTargetEnt ) )
			return self.samTargetEnt;

		self clearTargetEntity();
		return undefined;
	}

	if ( level.teamBased )
		uavs = level.uavModels[ level.otherTeam[ self.team ] ];
	else
		uavs = level.uavModels;

	if ( isDefined( uavs ) )
	{
		foreach ( uav in uavs )
		{
			if ( !self sam_isEnemyAircraft( uav ) )
				continue;
			if ( self sam_canSee( uav ) )
				return uav;
		}
	}

	if ( isDefined( level.helis ) )
	{
		foreach ( heli in level.helis )
		{
			if ( !self sam_isEnemyAircraft( heli ) )
				continue;
			if ( self sam_canSee( heli ) )
				return heli;
		}
	}

	if ( self sam_ac130IsTarget() && self sam_canSee( level.ac130.planeModel ) )
		return level.ac130.planeModel;

	self clearTargetEntity();
	return undefined;
}


// IW5 only engages the gunship while someone is flying it.
sam_ac130IsTarget()
{
	if ( !isDefined( level.ac130InUse ) || !level.ac130InUse )
		return false;
	if ( !isDefined( level.ac130 ) || !isDefined( level.ac130.planeModel ) || !isDefined( level.ac130player ) )
		return false;
	if ( isDefined( self.owner ) && level.ac130player == self.owner )
		return false;
	if ( level.teamBased && isDefined( level.ac130player.team ) && level.ac130player.team == self.team )
		return false;

	return true;
}


sam_targetStillValid()
{
	if ( !isDefined( self.samTargetEnt ) )
		return false;

	if ( isDefined( level.ac130 ) && isDefined( level.ac130.planeModel ) &&
		self.samTargetEnt == level.ac130.planeModel && !self sam_ac130IsTarget() )
	{
		self.samTargetEnt = undefined;
		self clearTargetEntity();
		return false;
	}

	return true;
}


sam_fireOnTarget()
{
	if ( !self sam_targetStillValid() )
		return;

	self setTargetEntity( self.samTargetEnt );

	// IW5 blocks on "turret_on_target". Bound it: a target outside the
	// launcher's reach never produces the notify.
	self waittill_notify_or_timeout( "turret_on_target", 3.0 );

	if ( !self sam_targetStillValid() )
		return;

	// Work from a local copy from here on. The watchers below run
	// synchronously until their first wait and may clear samTargetEnt at any
	// point; an undefined field read is a script runtime error, and a runtime
	// error shuts the whole server down.
	target = self.samTargetEnt;

	self thread sam_watchLineOfSight();
	self thread sam_watchCrashing();
	self thread sam_watchLeaving();

	// Lock-on time before the salvo (IW5).
	wait ( 2.0 );

	for ( i = 0; i < 4; i++ )
	{
		if ( !self sam_targetStillValid() || !isDefined( target ) || self.samTargetEnt != target )
			return;

		if ( isDefined( self.carriedBy ) )
			return;

		missile = magicBullet( level.sentrySettings[ self.sentryType ].weaponInfo,
			self getTagOrigin( "tag_flash" ), target.origin, self.owner );

		if ( isDefined( missile ) )
		{
			missile Missile_SetTargetEnt( target );
			missile Missile_SetFlightmodeDirect();
			missile.samTurret = self;

			// IW4's aircraft react to this: UAVs proximity-detonate the
			// missile (_uav::handleIncomingStinger), helicopters with flares
			// decoy it (_helicopter::heli_flares_monitor).
			level notify ( "stinger_fired", self.owner, missile, target );
		}
		else
		{
			{}
		}

		if ( i == 3 )
			break;

		wait ( 0.25 );
	}

	// The salvo may already have destroyed the target.
	if ( isDefined( target ) )
		{}
	else
		{}

	wait ( 3.0 );
}


sam_watchLineOfSight()
{
	level endon( "game_ended" );
	self endon( "death" );
	self endon( "carried" );

	while ( isDefined( self.samTargetEnt ) )
	{
		if ( !self sam_canSee( self.samTargetEnt ) )
		{
			self clearTargetEntity();
			self.samTargetEnt = undefined;
			break;
		}

		wait ( 0.05 );
	}
}


sam_watchCrashing()
{
	self endon( "death" );
	self endon( "carried" );

	target = self.samTargetEnt;
	if ( !isDefined( target ) )
		return;
	target endon( "death" );

	target waittill( "crashing" );

	if ( isDefined( self.samTargetEnt ) && self.samTargetEnt == target )
	{
		self clearTargetEntity();
		self.samTargetEnt = undefined;
	}
}


sam_watchLeaving()
{
	self endon( "death" );
	self endon( "carried" );

	target = self.samTargetEnt;
	if ( !isDefined( target ) )
		return;
	target endon( "death" );

	target waittill( "leaving" );

	if ( isDefined( self.samTargetEnt ) && self.samTargetEnt == target )
	{
		self clearTargetEntity();
		self.samTargetEnt = undefined;
	}
}
