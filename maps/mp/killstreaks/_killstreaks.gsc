#include maps\mp\_utility;
#include maps\mp\gametypes\_hud_util;
#include common_scripts\utility;

KILLSTREAK_STRING_TABLE = "mp/coKillstreakStatsTable.csv";

init()
{
	// &&1 Kill Streak!
	precacheString( &"MP_KILLSTREAK_N" );
	precacheString( &"MP_NUKE_ALREADY_INBOUND" );
	precacheString( &"MP_UNAVILABLE_IN_LASTSTAND" );
	precacheString( &"MP_UNAVAILABLE_WHEN_EMP" );
	precacheString( &"MP_UNAVAILABLE_USING_TURRET" );
	precacheString( &"MP_UNAVAILABLE_WHEN_INCAP" );
	precacheString( &"MP_HELI_IN_QUEUE" );

	level.killstreakFuncs = [];
	level.killstreakSetupFuncs = [];
	level.killstreakWeapons = [];
	
	level.killStreakMod = 0;

	thread maps\mp\killstreaks\_ac130::init();
	thread maps\mp\killstreaks\_remotemissile::init();
	thread maps\mp\killstreaks\_uav::init();
	thread maps\mp\killstreaks\_airstrike::init();
	thread maps\mp\killstreaks\_airdrop::init();
	thread maps\mp\killstreaks\_helicopter::init();
	thread maps\mp\killstreaks\_autosentry::init();
	thread maps\mp\killstreaks\_tank::init();
	thread maps\mp\killstreaks\_emp::init();
	thread maps\mp\killstreaks\_nuke::init();
	thread maps\mp\killstreaks\_mortar::init();
	// Attack dogs (catalogue row20 dogs_mp at 11 kills, pro row41 dogs_pro_mp;
	// activation weapon column13 "dogs_mp" is a real C1 WeaponDef). _dogs lives
	// at maps\mp\_dogs rather than under killstreaks\ because it is also the
	// level-wide dog subsystem (preexisting BSP "actor_enemy_dog_mp" spawners,
	// flash_dogs) and not only the reward handler. _dogs::init() precaches the
	// model/bite weapon/shellshock and builds the pathnode arrays; it never
	// waits before returning, so it completes synchronously here, before
	// initKillstreakData() below, and the dogs_pro_mp row inherits this handler
	// from its base the same way every other pro row does.
	thread maps\mp\_dogs::init();
	level.killStreakFuncs[ "dogs_mp" ] = maps\mp\_dogs::useKillstreakDogs;
	// RC-XD (catalogue row18 rcbomb_mp). This reaches spawnVehicle, which the
	// VM registers from build 2035702B onward (src/scr_vm_main.c,
	// CODOL_Scr_SpawnVehicle). GSC resolves builtin names at COMPILE time, so
	// on a binary without that registration this line is a fatal map-load
	// "unknown function" rather than a runtime failure - it took the server
	// down once. Only reachable scripts are lowered, so commenting this single
	// line is also the way to take the four _rcbomb files back out of the
	// compile set if the binary is ever rolled back.
	// DISABLED 2026-09-15 21:30: RC-XD native-crashed the live server on use; re-enable only after an isolated test match passes.
	thread maps\mp\killstreaks\_rcbomb::init();

	// Registrations happen before the handlers enter their event waiters.
	// Reward metadata is also used when earning a streak, before activation.
	initKillstreakData();

	level.killstreakRoundDelay = getIntProperty( "scr_game_killstreakdelay", 8 );

	level thread aerialPathCensus();
	level thread onPlayerConnect();
}


// One line per map naming the authored aerial route data each streak will
// find. Every installed CoDOL MP map ships the stock IW node set
// (heli_start / heli_loop_start / heli_leave / heli_crash_start / heli_dest,
// two minimap_corner), so a zero here is real missing map data and not a
// renamed convention - the one actual rename is mp_nuked's
// air_support_height, which _airstrike and _harrier now both accept.
// level.mapCenter is set by the gametype from the spawnpoint bounding box
// (for example war.gsc:108) and published with setMapCenter, so this waits a
// frame rather than reading it before its producer has run.
aerialPathCensus()
{
	waittillframeend;

	heightEnt = getEnt( "airstrikeheight", "targetname" );
	heightSource = "airstrikeheight";
	if ( !isDefined( heightEnt ) )
	{
		heightEnt = getEnt( "air_support_height", "targetname" );
		heightSource = "air_support_height";
	}
	if ( !isDefined( heightEnt ) )
		heightSource = "none (scripts use their scaled default height)";

	center = "undefined";
	if ( isDefined( level.mapCenter ) )
		center = "" + level.mapCenter;

	println( "CoDOL killstreak paths: heli_start " + getEntArray( "heli_start", "targetname" ).size +
		", heli_loop_start " + getEntArray( "heli_loop_start", "targetname" ).size +
		", heli_leave " + getEntArray( "heli_leave", "targetname" ).size +
		", heli_crash_start " + getEntArray( "heli_crash_start", "targetname" ).size +
		", heli_dest " + getEntArray( "heli_dest", "targetname" ).size +
		", heli_attack_area " + getEntArray( "heli_attack_area", "targetname" ).size +
		", minimap_corner " + getEntArray( "minimap_corner", "targetname" ).size +
		", airstrike height " + heightSource +
		", mapCenter " + center );
}


initKillstreakData()
{
	// Traverse physical rows: catalogue IDs need not be contiguous.
	for ( row = 0; true; row++ )
	{
		streakRef = tableLookupByRow( KILLSTREAK_STRING_TABLE, row, 1 );
		if ( !isDefined( streakRef ) || streakRef == "" )
			break;
		if ( streakRef == "none" )
			continue;

		// C1 has an extra column6 before the IW4 activation metadata.
		streakUseHint = getKillstreakHint( streakRef );
		if ( streakUseHint != &"" )
			precacheString( streakUseHint );

		// The existing leader-dialog consumer requires a defined entry and
		// treats "null" as its explicit silent sentinel.
		game["dialog"][streakRef] = "null";
		game["dialog"]["allies_friendly_" + streakRef + "_inbound"] = "null";
		game["dialog"]["allies_enemy_" + streakRef + "_inbound"] = "null";
		game["dialog"]["axis_friendly_" + streakRef + "_inbound"] = "null";
		game["dialog"]["axis_enemy_" + streakRef + "_inbound"] = "null";
		streakEarnDialog = getKillstreakDialog( streakRef );
		if ( streakEarnDialog != "" && streakEarnDialog != "null" )
			game["dialog"][streakRef] = streakEarnDialog;

		streakAlliesUseDialog = tableLookupByRow( KILLSTREAK_STRING_TABLE, row, 10 );
		if ( streakAlliesUseDialog != "" && streakAlliesUseDialog != "null" )
		{
			game["dialog"]["allies_friendly_" + streakRef + "_inbound"] = "use_" + streakAlliesUseDialog;
			game["dialog"]["allies_enemy_" + streakRef + "_inbound"] = "enemy_" + streakAlliesUseDialog;
		}
		streakAxisUseDialog = tableLookupByRow( KILLSTREAK_STRING_TABLE, row, 11 );
		if ( streakAxisUseDialog != "" && streakAxisUseDialog != "null" )
		{
			game["dialog"]["axis_friendly_" + streakRef + "_inbound"] = "use_" + streakAxisUseDialog;
			game["dialog"]["axis_enemy_" + streakRef + "_inbound"] = "enemy_" + streakAxisUseDialog;
		}

        // Catalogue identity also covers activation weapons without the old
        // IW4 "killstreak" substring, such as C1 rcbomb_mp. Read column13 raw
        // here so the cleanup set covers the pro spellings too, even though
        // getKillstreakWeapon resolves a pro reward to its base weapon.
        catalogueWeapon = tableLookupByRow( KILLSTREAK_STRING_TABLE, row, 13 );
        if ( catalogueWeapon != "" && catalogueWeapon != "none" )
            level.killstreakWeapons[catalogueWeapon] = true;

		// Pro rewards: the catalogue's own base-reward column22 supplies the
		// behaviour, so install the base handler under the pro identity. The
		// row walk is in catalogue order and every pro row (23..43) follows
		// its base row (1..22), so the base handler is already registered by
		// the init threads above. The base must have a real registered handler
		// (an unimplemented base such as dogs_mp leaves its pro row
		// unimplemented too).
		//
		// This registration MUST NOT precache a pro-specific weapon. Every
		// precacheItem consumes one of the engine's finite stock weapon slots
		// and composed weapons spend the remainder; resolving 17
		// killstreak_pro_* definitions here moved map-start stock from about
		// 95 to 112 and starved the bots' composed loadouts. Precaching cannot
		// be deferred either: Scr_PrecacheItem tests level+0x1c and raises
		// "precacheItem must be called before..." outside level load.
		//
		// A pro reward therefore inherits its base's ACTIVATION WEAPON as well
		// as its callback - see getKillstreakWeapon - which costs zero slots
		// and loses nothing: the pro row's cost, sound, all three dialog
		// columns, inform-enemy, XP, both icons and dpad shader are identical
		// to the base row, and it keeps its own MP_EARNED_PRO_* hint.
		if ( !isDefined( level.killstreakFuncs[streakRef] ) )
		{
			baseRef = getKillstreakBaseRef( streakRef );
			if ( baseRef == streakRef || !isDefined( level.killstreakFuncs[baseRef] ) )
				continue;
			level.killstreakFuncs[streakRef] = level.killstreakFuncs[baseRef];
			if ( isDefined( level.killstreakSetupFuncs[baseRef] ) )
				level.killstreakSetupFuncs[streakRef] = level.killstreakSetupFuncs[baseRef];
		}
		// Only a BASE row precaches. A pro row's getKillstreakWeapon resolves
		// to the base weapon, which the base row precached earlier in this
		// same walk (catalogue order puts rows 1..22 before 23..43).
		if ( getKillstreakBaseRef( streakRef ) == streakRef )
		{
			streakWeapon = getKillstreakWeapon( streakRef );
			if ( streakWeapon != "" && streakWeapon != "none" )
				precacheItem( streakWeapon );
		}

		streakPoints = int( tableLookupByRow( KILLSTREAK_STRING_TABLE, row, 14 ) );
		if ( streakPoints > 0 )
			maps\mp\gametypes\_rank::registerScoreInfo( "killstreak_" + streakRef, streakPoints );

		streakShader = getKillstreakIcon( streakRef );
		if ( streakShader != "" )
			precacheShader( streakShader );
		streakShader = getKillstreakCrateIcon( streakRef );
		if ( streakShader != "" )
			precacheShader( streakShader );
	}
}


onPlayerConnect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		
		if( !isDefined ( player.pers[ "killstreaks" ] ) )
			player.pers[ "killstreaks" ] = [];

		if( !isDefined ( player.pers[ "kID" ] ) )
			player.pers[ "kID" ] = 10;

		if( !isDefined ( player.pers[ "kIDs_valid" ] ) )
			player.pers[ "kIDs_valid" ] = [];
		
		player.lifeId = 0;
			
		if ( isDefined( player.pers["deaths"] ) )
			player.lifeId = player.pers["deaths"];

		player VisionSetMissilecamForPlayer( game["thermal_vision"] );
	
		player thread onPlayerSpawned();
		player thread onPlayerChangeKit();
	}
}


onPlayerSpawned()
{
	self endon( "disconnect" );

	for ( ;; )
	{
		self waittill( "spawned_player" );
		// Publish script progression after the native spawn clears playerState.
		if ( isDefined( self.pers["cur_kill_streak"] ) )
			self setKillstreak( self.pers["cur_kill_streak"] );
		else
			self setKillstreak( 0 );
		self thread killstreakUseWaiter();
		self thread killstreakUseWaiterMarkerRestart();
		self thread waitForChangeTeam();

		self giveOwnedKillstreakItem( true );
	}
}


// OVERLAY PATCH (2026-09-01, second of two changes in this file).
// beginAirdropViaMarker() arms `self endon("grenade_fire")` on the
// killstreakUseWaiter THREAD, so the waiter dies the moment the airdrop
// marker fires -- by design the marker watcher consumes the streak, but the
// dead waiter means NO further killstreak can be used until the next spawn.
// Restart it after a marker throw; the self-notify dedupe at the top of
// killstreakUseWaiter() makes the restart idempotent.
killstreakUseWaiterMarkerRestart()
{
	self notify( "killstreakUseWaiterMarkerRestart" );
	self endon( "killstreakUseWaiterMarkerRestart" );
	self endon( "disconnect" );
	self endon( "finish_death" );
	level endon( "game_ended" );

	for ( ;; )
	{
		self waittill( "grenade_fire", weap, weapname );

		if ( !maps\mp\killstreaks\_airdrop::isAirdropMarker( weapname ) )
			continue;

		wait ( 0.10 );
		self thread killstreakUseWaiter();
	}
}

onPlayerChangeKit()
{
	self endon( "disconnect" );
	
	for ( ;; )
	{
		self waittill( "changed_kit" );
		self giveOwnedKillstreakItem();
	}
}


waitForChangeTeam()
{
	self endon ( "disconnect" );
	
	self notify ( "waitForChangeTeam" );
	self endon ( "waitForChangeTeam" );
	
	for ( ;; )
	{
		self waittill ( "joined_team" );
		clearKillstreaks();
	}
}


// These three classifiers gate activation (airborne, turret, last stand, round
// grace). A pro reward runs its base reward's callback, so it must be gated by
// the base reward's rules; resolving through the catalogue base column keeps
// one rule set instead of duplicating 21 case labels that could drift apart.
isRideKillstreak( streakName )
{
	switch( getKillstreakBaseRef( streakName ) )
	{
		case "helicopter_minigun":
		case "helicopter_mk19":
		case "ac130":
		case "predator_missile":
		case "rcbomb_mp":
			return true;

		default:
			return false;
	}
}

// Use functions whose second parameter is the reward's kID: every airdrop
// variant validates it against pers["kIDs_valid"] (tryUseAirdrop) and raises
// a runtime error - which shuts the server down - when it is missing. Keyed
// by the BASE reward so the pro rows, which alias the base handler, get it
// too; the literal-name list this replaced missed airdrop_sam_turret and every
// *_pro airdrop.
killstreakTakesKID( streakName )
{
	switch( getKillstreakBaseRef( streakName ) )
	{
		case "airdrop":
		case "airdrop_mega":
		case "airdrop_sentry_minigun":
		case "airdrop_sam_turret":
		case "airdrop_predator_missile":
		case "mortar_mp":
			return true;

		default:
			return false;
	}
}


isCarryKillstreak( streakName )
{
	switch( getKillstreakBaseRef( streakName ) )
	{
		case "sentry":
		case "sentry_gl":
		case "sam_turret":	// IW5 isCarryKillstreak
			return true;

		default:
			return false;
	}
}


deadlyKillstreak( streakName )
{
	switch ( getKillstreakBaseRef( streakName ) )
	{
		case "predator_missile":
		case "precision_airstrike":
		case "mortar_mp":
		case "harrier_airstrike":
		//case "helicopter":
		//case "helicopter_flares":
		case "stealth_airstrike":
		//case "helicopter_minigun":
		case "ac130":
			return true;
	}

	return false;
}


// Every refusal below used to be a bare "return false": the reward stayed
// owned, nothing happened on screen, and the server log said nothing at all,
// so "I pressed it and nothing happened" was unattributable. This states the
// reason on the developer console and returns the same false the callers
// already expect, so no control flow changes.
killstreakRefused( streakName, reason )
{
	codolPrintLn( "CoDOL killstreak: " + streakName + " refused: " + reason );
	return false;
}


killstreakUsePressed( selectedID )
{
    if ( !isDefined( selectedID ) && isDefined( self.pers["killstreaks"][0] ) )
        selectedID = self.pers["killstreaks"][0].kID;
    reward = self codolRewardByID( selectedID );
    if ( !isDefined( reward ) )
        return false;
    streakName = reward.streakName;
    lifeId = reward.lifeId;
    isEarned = reward.earned;
    awardXp = reward.awardXp;
    kID = reward.kID;

	assert( isDefined( streakName ) );
	// A catalogue reward may be earned before its native controller exists.
	// Keep it owned; never call an undefined function or consume the reward.
	if ( !isDefined( level.killstreakFuncs[ streakName ] ) )
	{
		self iPrintLnBold( "Killstreak not implemented: " + streakName );
		return false;
	}

	if ( !self isOnGround() && ( isRideKillstreak( streakName ) || isCarryKillstreak( streakName ) ) )
		return killstreakRefused( streakName, "player is not on the ground" );

	if ( self isUsingRemote() )
		return killstreakRefused( streakName, "player is already using a remote killstreak" );

	if ( isDefined( self.selectingLocation ) )
		return killstreakRefused( streakName, "a location selection is already open" );
		
	if ( deadlyKillstreak( streakName ) && level.killstreakRoundDelay && getGametypeNumLives() )
	{
		if ( level.gracePeriod - level.inGracePeriod < level.killstreakRoundDelay )
		{
			self iPrintLnBold( &"MP_UNAVAILABLE_FOR_N", (level.killstreakRoundDelay - (level.gracePeriod - level.inGracePeriod)) );
			return ( false );
		}
	}

	if ( (level.teamBased && level.teamEMPed[self.team]) || (!level.teamBased && isDefined( level.empPlayer ) && level.empPlayer != self) )
	{
		self iPrintLnBold( &"MP_UNAVAILABLE_WHEN_EMP" );
		return ( false );
	}

	if ( self IsUsingTurret() && ( isRideKillstreak( streakName ) || isCarryKillstreak( streakName ) ) )
	{
		self iPrintLnBold( &"MP_UNAVAILABLE_USING_TURRET" );
		return ( false );
	}
	
	if ( isDefined( self.lastStand )  && isRideKillstreak( streakName ) )
	{
		self iPrintLnBold( &"MP_UNAVILABLE_IN_LASTSTAND" );
		return ( false );
	}
	
	if ( !self isWeaponEnabled() )
		return killstreakRefused( streakName, "weapon is disabled" );

	// T5 _hardpoints::useKillstreak awards XP/stat/dialog on activation. A
	// ride's use function (T5 useRCBomb) only returns when the ride is over,
	// so it awards itself through awardRideKillstreakActivation() as soon as
	// the vehicle is taken; the post-return award below then skips.
	activation = undefined;
	if ( isRideKillstreak( streakName ) )
	{
		activation = spawnStruct();
		activation.streakName = streakName;
		activation.awardXp = awardXp;
		activation.awarded = false;
	}
	self.codolRideActivation = activation;

	if ( killstreakTakesKID( streakName ) )
		used = self [[ level.killstreakFuncs[ streakName ] ]]( lifeId, kID );
	else
		used = self [[ level.killstreakFuncs[ streakName ] ]]( lifeId );

	self.codolRideActivation = undefined;
	if ( !isDefined( used ) || !used )
		return killstreakRefused( streakName, "the reward's own use function returned false" );
	
	// Placement/remote callbacks may yield. A team change can revoke this
	// reward meanwhile; newly earned rewards may only move it within the queue.
	stillOwned = false;
	foreach ( reward in self.pers["killstreaks"] )
	{
		if ( reward.kID == kID && reward.streakName == streakName )
			stillOwned = true;
	}
	if ( !stillOwned )
		return killstreakRefused( streakName, "ownership was revoked while the use function ran" );

	if ( !isDefined( activation ) || !activation.awarded )
		self usedKillstreak( streakName, awardXp );
	self shuffleKillStreaksFILO( streakName, kID );
	// A use can finish after its owner died (codolActivateReward); the spawn
	// re-grants the remaining rewards, so do not hand weapons to a corpse.
	if ( isAlive( self ) )
		self giveOwnedKillstreakItem();
	// mortar_pro_mp runs the base handler (catalogue column22), so it must be
	// committed too; the literal name left a pro mortar's three confirmed
	// targets consumed without a single shell.
	if ( getKillstreakBaseRef( streakName ) == "mortar_mp" )
		self maps\mp\killstreaks\_mortar::commitMortar( lifeId, kID );

	return ( true );
}


//this overwrites killstreak at index 0 and decrements all other killstreaks (FCLS style)
shuffleKillStreaksFILO( streakName, kID )
{
	arraySize = self.pers["killstreaks"].size;

	streakIndex = -1;
	for ( i = 0; i < arraySize; i++ )
	{
		if ( self.pers["killstreaks"][i].streakName != streakName )
			continue;
			
		if ( isDefined( kID ) && self.pers["killstreaks"][i].kID != kID )
			continue;
			
		streakIndex = i;
		break;
	}
	assert( streakIndex >= 0 );

	self.pers["killstreaks"][streakIndex] = undefined;

	for( i = streakIndex + 1; i < arraySize; i++ )	
	{
		if ( i == arraySize - 1 ) 
		{	
			self.pers["killstreaks"][i-1] = self.pers["killstreaks"][i];
			self.pers["killstreaks"][i] = undefined;
		}	
		else
		{
			self.pers["killstreaks"][i-1] = self.pers["killstreaks"][i];
		}	
	}
}


// Called by a ride killstreak's use function at the moment the ride starts
// (RC-XD: _rcbomb::givePlayerControlOfRCBomb after usevehicle succeeds).
// Awards exactly once; killstreakUsePressed then skips its own award.
awardRideKillstreakActivation()
{
	activation = self.codolRideActivation;
	if ( !isDefined( activation ) || activation.awarded )
		return;
	activation.awarded = true;
	self usedKillstreak( activation.streakName, activation.awardXp );
}

usedKillstreak( streakName, awardXp )
{
	self playLocalSound( "weap_c4detpack_trigger_plr" );

	if ( awardXp )
		self thread [[ level.onXPEvent ]]( "killstreak_" + streakName );

	self thread maps\mp\gametypes\_missions::useHardpoint( streakName );
	
	awardref = maps\mp\_awards::getKillstreakAwardRef( streakName );
	if ( isDefined( awardref ) )
		self thread incPlayerStat( awardref, 1 );

	team = self.team;

	if ( level.teamBased )
	{
		thread leaderDialog( team + "_friendly_" + streakName + "_inbound", team );
		
		if ( getKillstreakInformEnemy( streakName ) )
			thread leaderDialog( team + "_enemy_" + streakName + "_inbound", level.otherTeam[ team ] );
	}
	else
	{
		self thread leaderDialogOnPlayer( team + "_friendly_" + streakName + "_inbound" );
		
		if ( getKillstreakInformEnemy( streakName ) )
		{
			excludeList[0] = self;
			thread leaderDialog( team + "_enemy_" + streakName + "_inbound", undefined, undefined, excludeList );
		}
	}
}


clearKillstreaks()
{
	// Revoke deferred marker permissions before weapon removal can notify.
	foreach ( index, streakStruct in self.pers["killstreaks"] )
	{
		self.pers["kIDs_valid"][streakStruct.kID] = undefined;
		self.pers["killstreaks"][index] = undefined;
	}
	// Team changes discard all reward activation slots.
	for ( slot = 1; slot <= 5; slot++ )
		self _setActionSlot( slot, "" );
	self.codolPendingRewardID = undefined;
	self setKillstreakID( 0, 0 );
	self setKillstreakTarget( 0, 0 );
    foreach ( item in self getWeaponsListAll() )
        if ( isDefined( level.killstreakWeapons[item] ) )
            self takeWeapon( item );
}

getFirstPrimaryWeapon()
{
	weaponsList = self getWeaponsListPrimaries();
	
	assert ( isDefined( weaponsList[0] ) );
	assert ( !isKillstreakWeapon( weaponsList[0] ) );

	if ( weaponsList[0] == "onemanarmy_mp" )
	{
		assert ( isDefined( weaponsList[1] ) );
		assert ( !isKillstreakWeapon( weaponsList[1] ) );
		
		return weaponsList[1];
	}

	return weaponsList[0];
}


killstreakUseWaiter()
{
	// OVERLAY PATCH (2026-09-01, killstreak integration; the ONLY change in
	// this file -- rest is byte-identical to vendor/iw4).  The engine starts
	// exactly one of these per life because it raises "spawned_player" once;
	// this server's lifecycle can raise it twice for the first spawn (the
	// runtime phase edge AND _playerlogic::spawnPlayer), and TWO waiters
	// double-consume one use: both call killstreakUsePressed for the same
	// weapon_change (asserts at :214-215), and for the airdrop both die on
	// endon("grenade_fire"), leaving NO waiter for every later streak.
	// MEASURED in killstreak_support.py --selftest.  The self-notify dedupe
	// below restores the engine's single-instance invariant -- the same
	// idiom stock code uses in waitForChangeTeam() a few lines up.
	self notify( "killstreakUseWaiter" );
	self endon( "killstreakUseWaiter" );

	self endon( "disconnect" );
	self endon( "finish_death" );
	level endon( "game_ended" );

	self.lastKillStreak = 0;
	if ( !isDefined( self.pers["lastEarnedStreak"] ) )
		self.pers["lastEarnedStreak"] = undefined;
		
    self.codolPendingRewardID = undefined;
    // Identity of this waiter's life. A reward use that outlives it (see
    // codolActivateReward) must not release a later life's lock or weapon.
    self.codolRewardLife = spawnStruct();
    self.codolRewardActivationBusy = false;
    self.codolRewardSuppressWeapon = undefined;
    for ( action = 1; action <= 5; action++ )
        self thread codolRewardActionWaiter( action );
	self thread finishDeathWaiter();

	for ( ;; )
	{
		self waittill ( "weapon_change", newWeapon );
        self codolCleanupRewardWeapons();
		
		if ( !isAlive( self ) )
			continue;

        // A direct same-weapon use can still receive a delayed change edge.
        // Do not let that edge consume the next copy of the same reward.
        if ( isDefined( self.codolRewardSuppressWeapon ) )
        {
            if ( newWeapon == self.codolRewardSuppressWeapon &&
                 !isDefined( self.codolPendingRewardID ) )
                continue;
            if ( newWeapon != self.codolRewardSuppressWeapon )
                self.codolRewardSuppressWeapon = undefined;
        }
        reward = self codolRewardByID( self.codolPendingRewardID );
        if ( !isDefined( reward ) || newWeapon != getKillstreakWeapon( reward.streakName ) )
        {
            reward = undefined;
            foreach ( candidate in self.pers["killstreaks"] )
            {
                // Native foreach keeps sibling insertion order, so the first
                // matching record is the oldest earned copy.
                if ( newWeapon == getKillstreakWeapon( candidate.streakName ) &&
                     !isDefined( reward ) )
                    reward = candidate;
            }
        }
        self.codolPendingRewardID = undefined;
        if ( !isDefined( reward ) )
            continue;
        self codolActivateReward( reward.kID );
	}
}


// Both routes share one lock acquired before yielding: native weapon_change
// and pressing another slot whose activation weapon is already equipped.
//
// The reward's use function runs on its own thread, not on this waiter's.
// Both callers end on "finish_death", and a use can outlive its owner's life:
// the RC-XD ride continues after its driver is killed (T5 rule, see
// _rcbomb::givePlayerControlOfRCBomb) and returns only at "rcbomb_done", and
// a ride's laptop wait (initRideKillstreak) or a location selection yields
// across the death. Run on this stack, "finish_death" killed the use midway:
// the RC-XD never released its _rcbomb_rules claim (updateRulesOnEnd), so the
// reward stayed owned but was refused for the rest of the match, and the
// global/team slot leaked for everyone; an AC-130 or ride could leave its
// level lock or disabled usability behind the same way. T5 has the same
// shape: _hardpoints::useKillstreak threads triggerKillstreak with no death
// endon and settles ownership in removeKillstreakWhenDone (disconnect only).
// This waiter still waits for the use, so the lock keeps its meaning.
codolActivateReward( selectedID )
{
    if ( isDefined( self.codolRewardActivationBusy ) && self.codolRewardActivationBusy )
        return;
    self.codolRewardActivationBusy = true;
    use = spawnStruct();
    use.done = false;
    self thread codolRunRewardUse( selectedID, self.codolRewardLife, use );
    // A use killed by its own endon (the airdrop marker's "grenade_fire" or
    // "death") never finishes; killstreakUseWaiterMarkerRestart and
    // "finish_death" end this waiter and the next one resets the lock.
    while ( !use.done )
        self waittill( "codol_reward_use_done" );
}

codolRunRewardUse( selectedID, life, use )
{
    self endon( "disconnect" );

    waittillframeend;
    reward = self codolRewardByID( selectedID );
    if ( isAlive( self ) && isDefined( reward ) )
    {
        streakName = reward.streakName;
        self.codolRewardSuppressWeapon = getKillstreakWeapon( streakName );
        result = self killstreakUsePressed( selectedID );

        // The weapon belongs to the life that raised the reward weapon. After
        // a death (or a respawn during a ride) the spawn loadout owns it.
        if ( isAlive( self ) && self codolIsRewardLife( life ) )
        {
            //no force switching weapon for ridable killstreaks
            // Put away only the reward's own weapon: a player who already
            // switched (which cancels a mortar selection) keeps that choice,
            // and a reward weapon is never the one switched back to - that
            // raised the tablet again and re-entered the selection.
            current = self getCurrentWeapon();
            if ( ( !isRideKillstreak( streakName ) || !result ) &&
                 ( current == "none" || current == getKillstreakWeapon( streakName ) ) )
            {
                lastWeapon = self getLastWeapon();
                if ( !self hasWeapon( lastWeapon ) || isDefined( level.killstreakWeapons[lastWeapon] ) )
                    self switchToWeapon( self getFirstPrimaryWeapon() );
                else
                    self switchToWeapon( lastWeapon );
            }

            // give time to switch to the near weapon; when the weapon is none (such as during a "disableWeapon()" period
            // re-enabling the weapon immediately does a "weapon_change" to the killstreak weapon we just used.  In the case that
            // we have two of that killstreak, it immediately uses the second one
            if ( self getCurrentWeapon() == "none" )
            {
                while ( isAlive( self ) && self getCurrentWeapon() == "none" )
                    wait ( 0.05 );

                waittillframeend;
            }
            self codolCleanupRewardWeapons();
        }
    }

    // Release only this life's lock; a new life's waiter has already reset it.
    if ( self codolIsRewardLife( life ) )
        self.codolRewardActivationBusy = false;
    use.done = true;
    self notify( "codol_reward_use_done" );
}

codolIsRewardLife( life )
{
    if ( !isDefined( life ) || !isDefined( self.codolRewardLife ) )
        return !isDefined( life ) && !isDefined( self.codolRewardLife );
    return self.codolRewardLife == life;
}


finishDeathWaiter()
{
	self endon ( "disconnect" );
	level endon ( "game_ended" );
	
	self waittill ( "death" );
	self setKillstreak( 0 );
	wait ( 0.05 );
	self notify ( "finish_death" );
	self.pers["lastEarnedStreak"] = undefined;
}


checkKillstreakReward( streakCount )
{
	self setKillstreak( streakCount );
	self notify( "got_killstreak", streakCount );

	maxVal = 0;
	killStreaks = [];
	foreach ( streakVal, streakName in self.killStreaks )
	{
		killStreaks[streakName] = streakVal;
		if ( streakVal > maxVal )
			maxVal = streakVal;
	}

	// Native getarraykeys walks sibling insertion order, not numeric cost.
	// Process costs explicitly so later rollover keys cannot hide earned rewards
	// and lastEarnedStreak advances only after lower eligible rewards were tried.
	for ( streakVal = 0; streakVal <= maxVal; streakVal++ )
	{
		if ( !isDefined( self.killStreaks[streakVal] ) )
			continue;
		streakName = self.killStreaks[streakVal];
		actualVal = streakVal + level.killStreakMod;
		
		if ( actualVal > streakCount )
			break;
		
		if ( isDefined( self.pers["lastEarnedStreak"] ) && killStreaks[streakName] <= killStreaks[self.pers["lastEarnedStreak"]] )
			continue;

		if ( isSubStr( streakName, "-rollover" ) )
			useStreakName = strTok( streakName, "-" )[0];
		else
		{
			useStreakName = streakName;
		}
		
		if ( self tryGiveKillstreak( useStreakName, int(max( actualVal, streakCount )) ) )
		{
			self thread killstreakEarned( useStreakName );
			self.pers["lastEarnedStreak"] = streakName;
		}
	}
}


killstreakEarned( streakName )
{
	// Retain IW4's first/third reward challenge semantics, sourced from
	// the selected CoDOL class. Bots have no persistent challenge account.
	if ( self.isbot || !isDefined( self.codolSelectedKillstreaks ) )
		return;
	if ( self.codolSelectedKillstreaks[0] == streakName )
	{
		self.firstKillstreakEarned = getTime();
	}	
	else if ( self.codolSelectedKillstreaks[2] == streakName && isDefined( self.firstKillstreakEarned ) )
	{
		if ( getTime() - self.firstKillstreakEarned < 20000 )
			self thread maps\mp\gametypes\_missions::genericChallenge( "wargasm" );
	}
}


rewardNotify( streakName, streakVal )
{
	self endon( "disconnect" );

	self maps\mp\gametypes\_hud_message::killstreakSplashNotify( streakName, streakVal );
}


tryGiveKillstreak( streakName, streakVal )
{
	level notify ( "gave_killstreak", streakName );

	if ( !level.gameEnded )
		self thread rewardNotify( streakName, streakVal );

	self giveKillstreak( streakName, streakVal, true );
	return true;
}


giveKillstreak( streakName, isEarned, awardXp, owner )
{
	self endon ( "disconnect" );

	weapon = getKillstreakWeapon( streakName );

	self giveKillstreakWeapon( weapon );
	
	// shuffle existing killstreaks up a notch
	for( i = self.pers["killstreaks"].size; i >= 0; i-- )	
		self.pers["killstreaks"][i + 1] = self.pers["killstreaks"][i]; 	
	
	self.pers["killstreaks"][0] = spawnStruct();
	self.pers["killstreaks"][0].streakName = streakName;
	self.pers["killstreaks"][0].earned = isDefined( isEarned ) && isEarned;
	self.pers["killstreaks"][0].awardxp = isDefined( awardXp ) && awardXp;
	self.pers["killstreaks"][0].owner = owner;

	self.pers["killstreaks"][0].kID = self.pers["kID"];
	self.pers["kIDs_valid"][self.pers["kID"]] = true;

    actionSlot = 1; // Extra care-package rewards occupy C1 action slot1.
    if ( self.pers["killstreaks"][0].earned && isDefined( self.codolSelectedKillstreaks ) )
    {
        for ( selected = 0; selected < self.codolSelectedKillstreaks.size; selected++ )
        {
            if ( isDefined( self.codolSelectedKillstreaks[selected] ) &&
                 self.codolSelectedKillstreaks[selected] == streakName )
                actionSlot = selected + 2;
        }
    }
    self.pers["killstreaks"][0].actionSlot = actionSlot;
	self.pers["kID"]++;

	if ( !self.pers["killstreaks"][0].earned )
		self.pers["killstreaks"][0].lifeId = -1;
	else
		self.pers["killstreaks"][0].lifeId = self.pers["deaths"];
	
    self giveOwnedKillstreakItem( true );

	// probably obsolete unless we bring back the autoshotty	
	if ( isdefined( level.killstreakSetupFuncs[ streakName ] ) )
		self [[ level.killstreakSetupFuncs[ streakName ] ]]();
		
	if ( isDefined( isEarned ) && isEarned && isDefined( awardXp ) && awardXp )
		self notify( "received_earned_killstreak" );
}


giveKillstreakWeapon( weapon )
{
    if ( !self hasWeapon( weapon ) )
        self _giveWeapon( weapon, 0 );
}



getStreakCost( streakName )
{
	return int( tableLookup( KILLSTREAK_STRING_TABLE, 1, streakName, 4 ) );
}


getKillstreakHint( streakName )
{
	return tableLookupIString( KILLSTREAK_STRING_TABLE, 1, streakName, 7 );
}


getKillstreakInformEnemy( streakName )
{
	return int( tableLookup( KILLSTREAK_STRING_TABLE, 1, streakName, 12 ) );
}


getKillstreakSound( streakName )
{
	return tableLookup( KILLSTREAK_STRING_TABLE, 1, streakName, 8 );
}


getKillstreakDialog( streakName )
{
	return tableLookup( KILLSTREAK_STRING_TABLE, 1, streakName, 9 );
}


// A pro reward resolves to its BASE reward's activation weapon. Its own
// killstreak_pro_* weapon is never precached, because each precacheItem
// consumes one of the engine's limited stock weapon slots and the composed
// weapon system spends the remainder - 17 pro definitions alone moved the
// map-start stock count from about 95 to 112 and broke composed bot loadouts.
// Resolving here rather than at each call site keeps every consumer
// consistent: earn-time giveKillstreakWeapon, the weapon_change activation
// match, the suppression record and the owned-weapon cleanup all ask this
// function. Pro and base rows carry identical icons and costs, so sharing the
// activation weapon changes nothing the player can see.
// A pro reward resolves to its BASE reward's activation weapon. Its own
// killstreak_pro_* weapon is never precached, because each precacheItem
// consumes one of the engine's finite stock weapon slots and composed weapons
// spend the remainder - these 17 rows alone moved map-start stock from about
// 95 to 112 and broke composed bot loadouts. Resolving here rather than at
// each call site keeps every consumer consistent: earn-time
// giveKillstreakWeapon, the weapon_change activation match, the suppression
// record and the owned-weapon cleanup all ask this function. Pro and base rows
// carry identical icons and costs, so sharing the activation weapon changes
// nothing the player can see.
getKillstreakWeapon( streakName )
{
	baseRef = getKillstreakBaseRef( streakName );
	if ( isDefined( baseRef ) && baseRef != streakName )
		streakName = baseRef;
	return tableLookup( KILLSTREAK_STRING_TABLE, 1, streakName, 13 );
}

getKillstreakIcon( streakName )
{
	return tableLookup( KILLSTREAK_STRING_TABLE, 1, streakName, 15 );
}

getKillstreakCrateIcon( streakName )
{
	return tableLookup( KILLSTREAK_STRING_TABLE, 1, streakName, 16 );
}


// C1 column22 is the catalogue's own base-reward column. It is empty on every
// base row and names the base reward on all 21 "_pro" rows. Those pro rows are
// otherwise identical to their base row in every gameplay column (cost4, use
// hint7, sound8, dialogs9..11, inform-enemy12, XP14, icons15/16, unlock17..21,
// crate weight23); only the ID, the localized title/description, the
// MP_EARNED_PRO_* hint and the killstreak_pro_* activation weapon differ.
// The catalogue therefore states that a pro reward's behaviour is its base
// reward's behaviour, and this column is the only thing that expresses it.
// Resolving through it is data-derived, not an invented alias. It does not
// claim CoDOL applies no extra pro-only buff: no shipped column expresses one,
// so none is fabricated here.
getKillstreakBaseRef( streakName )
{
	if ( !isDefined( streakName ) )
		return undefined;
	baseRef = tableLookup( KILLSTREAK_STRING_TABLE, 1, streakName, 22 );
	if ( !isDefined( baseRef ) || baseRef == "" || baseRef == streakName )
		return streakName;
	return baseRef;
}


// Records retain kID/lifeId ownership; only activation presentation is per-slot.
codolRewardByID( id )
{
    if ( !isDefined( id ) )
        return undefined;
    foreach ( reward in self.pers["killstreaks"] )
        if ( reward.kID == id )
            return reward;
    return undefined;
}

codolRewardForSlot( slot )
{
    foreach ( reward in self.pers["killstreaks"] )
        if ( isDefined( reward.actionSlot ) && reward.actionSlot == slot )
            return reward;
    return undefined;
}

codolRewardActionWaiter( slot )
{
    self endon( "killstreakUseWaiter" );
    self endon( "disconnect" );
    self endon( "finish_death" );
    level endon( "game_ended" );
    event = "codol_reward_slot_" + slot;
    self notifyOnPlayerCommand( event, "+actionslot " + slot );
    for ( ;; )
    {
        self waittill( event );
        if ( isDefined( self.codolRewardActivationBusy ) && self.codolRewardActivationBusy )
            continue;
        reward = self codolRewardForSlot( slot );
        self.codolPendingRewardID = undefined;
        if ( isDefined( reward ) )
        {
            self.codolPendingRewardID = reward.kID;
            // Selecting the same weapon has no native weapon_change edge.
            if ( self getCurrentWeapon() == getKillstreakWeapon( reward.streakName ) )
            {
                self.codolPendingRewardID = undefined;
                self codolActivateReward( reward.kID );
            }
        }
    }
}

giveOwnedKillstreakItem( skipDialog )
{
    for ( slot = 1; slot <= 5; slot++ )
    {
        reward = self codolRewardForSlot( slot );
        if ( !isDefined( reward ) )
        {
            self _setActionSlot( slot, "" );
            if ( slot == 1 )
            {
                self setKillstreakID( 0, 0 );
                self setKillstreakTarget( 0, 0 );
            }
            continue;
        }
        weapon = getKillstreakWeapon( reward.streakName );
        // The native weapon registry is finite and shared with composed
        // weapons, so a reward weapon can be named correctly and precached and
        // STILL have no publishable index. That is how rcbomb_mp reached
        // "engine 128 wire -1 owned 0": the give silently failed and the
        // setActionSlot that followed raised and ended the match.
        //
        // Verify the grant actually took before publishing an action for it.
        // hasWeapon is used rather than the newer codol_isWeaponPublished
        // because this script must keep compiling on the deployed binary -
        // GSC resolves builtin names at COMPILE time, so naming a builtin the
        // running server lacks is a fatal map-load error, not a runtime one.
        // Switch to the pre-give check once a binary carrying it is deployed.
        //
        // The reward stays owned and keeps its queue entry, so it becomes
        // usable again on a map where the weapon fits; it is only ungranted.
        self giveKillstreakWeapon( weapon );
        if ( !self hasWeapon( weapon ) )
        {
            println( "CoDOL killstreak: " + reward.streakName + " weapon " + weapon +
                " was not granted (no publishable index); action slot left empty," +
                " reward kept pending" );
            self _setActionSlot( slot, "" );
            continue;
        }
        self _setActionSlot( slot, "weapon", weapon );
        if ( slot == 1 )
        {
            id = int( tableLookup( KILLSTREAK_STRING_TABLE, 1, reward.streakName, 0 ) );
            self setKillstreakID( id, 0 );
            self setKillstreakTarget( 0, 0 );
        }
    }
    self codolCleanupRewardWeapons();
    if ( !isDefined( skipDialog ) && !level.inGracePeriod &&
         isDefined( self.pers["killstreaks"][0] ) )
        self leaderDialogOnPlayer( self.pers["killstreaks"][0].streakName, "killstreak_earned" );
}


codolCleanupRewardWeapons()
{
    // Remove a consumed reward weapon only when no remaining record owns it.
    foreach ( item in self getWeaponsListAll() )
    {
        if ( !isDefined( level.killstreakWeapons[item] ) )
            continue;
        owned = false;
        foreach ( reward in self.pers["killstreaks"] )
            if ( getKillstreakWeapon( reward.streakName ) == item )
                owned = true;
        if ( !owned && self getCurrentWeapon() != item )
            self takeWeapon( item );
    }
}


initRideKillstreak()
{
	self _disableUsability();
	result = self initRideKillstreak_internal();

	if ( isDefined( self ) )
		self _enableUsability();
		
	return result;
}

initRideKillstreak_internal()
{
	laptopWait = self waittill_any_timeout( 1.0, "disconnect", "death", "weapon_switch_started" );
	
	if ( laptopWait == "weapon_switch_started" )
		return ( "fail" );

	if ( !isAlive( self ) )
		return "fail";

	if ( laptopWait == "disconnect" || laptopWait == "death" )
	{
		if ( laptopWait == "disconnect" )
			return ( "disconnect" );

		if ( self.team == "spectator" )
			return "fail";

		return ( "success" );		
	}
	
	if ( self isEMPed() || self isNuked() )
	{
		return ( "fail" );
	}
	
	self VisionSetNakedForPlayer( "black_bw", 0.75 );
	blackOutWait = self waittill_any_timeout( 0.80, "disconnect", "death" );

	if ( blackOutWait != "disconnect" ) 
	{
		self thread clearRideIntro( 1.0 );
		
		if ( self.team == "spectator" )
			return "fail";
	}

	if ( !isAlive( self ) )
		return "fail";

	if ( self isEMPed() || self isNuked() )
		return "fail";
	
	if ( blackOutWait == "disconnect" )
		return ( "disconnect" );
	else
		return ( "success" );		
}


clearRideIntro( delay )
{
	self endon( "disconnect" );

	if ( isDefined( delay ) )
		wait( delay );

	//self freezeControlsWrapper( false );
	
	if ( !isDefined( level.nukeVisionInProgress ) )
		self VisionSetNakedForPlayer( getDvar( "mapname" ), 0 );
}


