// Adapted original T5 MP _rcbomb.gsc (T5-STEAM-GSC maps/mp/_rcbomb.gsc, pinned in
// obj/rc-physics-stage/t5-original). Placement is copied from the original;
// native C1 interfaces replace incompatible T5 providers. The original
// control/damage/20+6+4 timer lifecycle is retained, including the T5
// "rcbomb_done" completion contract: the use function blocks until the ride
// is over, exactly as T5's useRCBomb/updateRulesOnEnd do.
//
// T5 framework calls re-pointed to this server's adapters:
//   _killstreakrules   -> maps\mp\killstreaks\_rcbomb_rules
//   _weaponobjects     -> maps\mp\killstreaks\_rcbomb_watchers
//   _flashgrenades::monitorRCBombFlash -> _rcbomb_watchers::weaponObjectDamage stun
//   _spawning::create_rcbomb_influencers -> codol_setRCSpawnInfluencer
//   _hostmigration::waitLongDurationWithHostMigrationPause -> mounted IW4 copy
//   _hardpoints stat/XP/dialog/popups -> the C1 reward dispatcher (_killstreaks)
// Not available on this server and therefore not emulated: _hud::fadeToBlack
// ForXSec, _treadfx (the FX are precached like T5, the tag consumer is not
// ported), _medals/_properks/_rank destroyer credit (the "destroyed_killstreak"
// notify carries it), per-viewer setEnemyModel (see spawnRCBomb).
#include maps\mp\_utility;
#include common_scripts\utility;

init()
{
    precacheModel( "t5_veh_rcbomb_allies" );
    precacheModel( "t5_veh_rcbomb_axis" );
    precacheVehicle( "rc_car_medium_mp" );
    precacheString( &"KILLSTREAK_RCBOMB_NOT_PLACABLE" );

    loadTreadFx( "dust" );
    loadTreadFx( "concrete" );
    loadTreadFx( "snow" );

    level._effect["rcbomb_stunned"] = loadfx( "weapon/grenade/fx_spark_disabled_rc_car" );
    level._effect["rcbomb_blinky"] = loadfx( "vehicle/light/fx_rcbomb_blinky_light" );
    level._effect["rcbomb_solid"] = loadfx( "vehicle/light/fx_rcbomb_solid_light" );
    level._effect["rcbomb_green"] = loadfx( "vehicle/light/fx_rcbomb_light_green_os" );
    level._effect["rcbomb_red"] = loadfx( "vehicle/light/fx_rcbomb_light_red_os" );
    level._effect["rcbombexplosion"] = loadfx( "maps/mp_maps/fx_mp_exp_rc_bomb" );

    // T5 _clientflags.gsc. Server-owned flags have actual consumers below and
    // in the native stun gate; no T5 CSC or guessed C1 eFlags bits are installed.
    level.const_flag_countdown = 1;
    level.const_flag_timeout = 2;
    level.const_flag_stunned = 9;

    if ( getDvar( "scr_rcbomb_car_size" ) == "" )
        setDvar( "scr_rcbomb_car_size", "1" );
    if ( getDvar( "scr_rcbomb_notimeout" ) == "" )
        setDvar( "scr_rcbomb_notimeout", "0" );

    maps\mp\killstreaks\_rcbomb_rules::init();
    level.killstreakFuncs["rcbomb_mp"] = ::useKillstreakRCBomb;

    // Sets up RC bomb to die from trigger_hurt's. Threaded because there is a
    // 'spread_array_thread()' within it.
    level thread trigger_monitor_init();
}

loadTreadFx( type )
{
    // T5 loads seven per surface. C1 common_gametype_mp ships the six suffixed
    // variants (obj/rc-physics-stage/rc-c1-asset-inventory.json); the bare
    // "fx_treadfx_rcbomb_<type>" asset is not in the C1 inventory and is not
    // requested, so no missing-asset reference reaches the client.
    level._effect["rcbomb_tread_" + type + "_drift"] = loadfx( "vehicle/treadfx/fx_treadfx_rcbomb_" + type + "_drift" );
    level._effect["rcbomb_tread_" + type + "_peel"] = loadfx( "vehicle/treadfx/fx_treadfx_rcbomb_" + type + "_peel" );
    level._effect["rcbomb_tread_" + type + "_first_person"] = loadfx( "vehicle/treadfx/fx_treadfx_rcbomb_" + type + "_first_person" );
    level._effect["rcbomb_tread_" + type + "_reverse"] = loadfx( "vehicle/treadfx/fx_treadfx_rcbomb_" + type + "_reverse" );
    level._effect["rcbomb_tread_" + type + "_trail"] = loadfx( "vehicle/treadfx/fx_treadfx_rcbomb_" + type + "_trail" );
    level._effect["rcbomb_tread_" + type + "_slow"] = loadfx( "vehicle/treadfx/fx_treadfx_rcbomb_" + type + "_slow" );
}

useKillstreakRCBomb( lifeId )
{
    if ( self maps\mp\killstreaks\_rcbomb_rules::isKillstreakAllowed( "rcbomb_mp", self.team ) == false )
        return false;

    if ( !self isOnGround() )
    {
        self iPrintLnBold( &"KILLSTREAK_RCBOMB_NOT_PLACABLE" );
        return false;
    }

    placement = self.rcbombPlacement;
    if ( !isDefined( placement ) )
        placement = getRCBombPlacement();
    if ( !isDefined( placement ) )
    {
        self iPrintLnBold( &"KILLSTREAK_RCBOMB_NOT_PLACABLE" );
        return false;
    }

    // T5 _rcbomb.gsc:107 _hud::fadeToBlackForXSec, ported in _rcbomb_presentation.
    self thread maps\mp\killstreaks\_rcbomb_presentation::fadeIn();

    ret = self useRCBomb( placement );

    if ( !isDefined( ret ) && level.gameEnded )
        ret = true;
    else if ( !isDefined( ret ) )
        ret = false;

    return ret;
}

spawnRCBomb( placement, team )
{
    // T5 shows each viewer a different car through setEnemyModel plus a
    // per-viewer model audience. C1's entity state publishes ONE model per
    // entity, and no measured C1 producer selects a second model per
    // recipient, so implementing setEnemyModel would mean inventing a
    // snapshot facility. Both shipped models exist (t5_veh_rcbomb_allies,
    // t5_veh_rcbomb_axis, verified in common_gametype_mp), so the car is
    // coloured by its OWNER's team for every viewer instead. Friend/enemy
    // identification still comes from the per-recipient green/red blink in
    // _rcbomb_presentation::blink. Stated presentation limitation, not parity.
    model = "t5_veh_rcbomb_allies";
    if ( team == "axis" )
        model = "t5_veh_rcbomb_axis";
    death_model = model;
    car = "rc_car_medium_mp";

    vehicle = spawnVehicle( model, "rcbomb", car, placement.origin, placement.angles );
    if ( !isDefined( vehicle ) )
        return undefined;

    vehicle makeVehicleUnusable();
    vehicle.death_model = death_model;
    vehicle.allowFriendlyFireDamageOverride = ::RCCarAllowFriendlyFireDamage;

    vehicle setOwner( self );
    vehicle.owner = self;
    vehicle setVehicleTeam( team );
    vehicle.team = team;
    vehicle thread maps\mp\killstreaks\_rcbomb_presentation::start();

    // T5 _treadfx::loadtreadfx( vehicle ) attaches surface tread FX watchers;
    // not ported (see header).

    // create the influencers: T5 _spawning:25 player-heights (70), score -200,
    // steep 2, enemies only.
    protectedTeam = team;
    if ( !level.teamBased )
        protectedTeam = "none";
    vehicle codol_setRCSpawnInfluencer( 1750, -200, 2, protectedTeam );

    return vehicle;
}

getRCBombPlacement()
{
    return calculateSpawnOrigin( self.origin, self.angles );
}

givePlayerControlOfRCBomb()
{
    self endon( "disconnect" );
    level endon( "game_ended" );

    // T5: _hardpoints::playKillstreakStartDialog, statAdd RCBOMB_USED,
    // globalKillstreaksCalled, incItemStatByReference, giveRankXP. The C1
    // dispatcher (_killstreaks::codolActivateReward) consumes the exact kID
    // and awards once on acceptance; those providers are not duplicated here.

    self freeze_player_controls( false );

    self.rcbomb useVehicle( self, 0 );
    if ( !self isRemoteControlling() )
    {
        // Native ownership was refused (conflicting link). Nothing to ride;
        // let the deletion waiter release the rule slot and the car.
        self notify( "weapon_object_destroyed" );
        self restoreWeaponAfterRide();
        return;
    }

    self thread playerDisconnectWaiter( self.rcbomb );
    self thread carDetonateWaiter( self.rcbomb );
    //self thread carDeathWaiter( self.rcbomb );
    self thread exitCarWaiter( self.rcbomb );
    self thread GameEndWatcher( self.rcbomb );
    self thread changeTeamWaiter( self.rcbomb );
    // T5 _flashgrenades::monitorRCBombFlash: the flash/concussion stun is
    // applied by _rcbomb_watchers::weaponObjectDamage -> stun, already
    // threaded on the car by addWeaponObjectToWatcher.
    self thread carTimer( self.rcbomb );
    self.rcbomb thread maps\mp\killstreaks\_rcbomb_presentation::monitor();

    self waittill( "rcbomb_done" );
    self thread maps\mp\killstreaks\_rcbomb_presentation::fadeOut();
    self restoreWeaponAfterRide();
}

// T5 _hardpoints::removeKillstreakWhenDone (takeWeapon) and
// changeWeaponAfterKillstreak (switch to lastNonKillstreakWeapon) run on
// "killstreak_done". The C1 dispatcher deliberately skips its own switch-back
// for ride killstreaks (_killstreaks::codolActivateReward), so the ride
// restores the weapon itself on every "rcbomb_done" path. Ownership of a
// second RC-XD is re-issued by the dispatcher's giveOwnedKillstreakItem.
restoreWeaponAfterRide()
{
    if ( !isDefined( self ) || !isAlive( self ) )
        return;
    rideWeapon = maps\mp\killstreaks\_killstreaks::getKillstreakWeapon( "rcbomb_mp" );
    if ( isDefined( rideWeapon ) && rideWeapon != "" && self hasWeapon( rideWeapon ) )
        self takeWeapon( rideWeapon );
    previous = self getLastWeapon();
    if ( !isDefined( previous ) || previous == "none" || !self hasWeapon( previous ) || maps\mp\_utility::isKillstreakWeapon( previous ) )
        previous = self maps\mp\killstreaks\_killstreaks::getFirstPrimaryWeapon();
    if ( isDefined( previous ) && previous != "none" )
        self switchToWeapon( previous );
}

useRCBomb( placement )
{
    self endon( "disconnect" );
    level endon( "game_ended" );

    hardpointtype = "rcbomb_mp";

    WaitTillSlowProcessAllowed();

    if ( !isDefined( self ) || !isAlive( self ) || ( self isRemoteControlling() ) )
        return false;

    if ( !isDefined( self.rcbomb ) )
    {
        self.rcbomb = self spawnRCBomb( placement, self.team );
        if ( !isDefined( self.rcbomb ) )
            return false;
        self.rcbomb thread carCleanupWaiter( self );
        self.rcbomb.team = self.team;

        self maps\mp\killstreaks\_rcbomb_watchers::addWeaponObjectToWatcher( "rcbomb", self.rcbomb );
    }

    if ( self maps\mp\killstreaks\_rcbomb_rules::killstreakStart( hardpointtype, self.team, undefined, false ) == false )
    {
        // T5 leaves the spawned car for the respawn watcher to clear. Here
        // the refused car is removed at once so it cannot linger unowned.
        self.rcbomb delete();
        return false;
    }

    self.enteringVehicle = true;

    self thread updateKillstreakOnDisconnect();
    self thread updateKillstreakOnDeletion( self.team );

    self freeze_player_controls( true );

    wait( 0.9 );

    if ( !isDefined( self ) || !isAlive( self ) || !isDefined( self.rcbomb ) )
    {
        if ( isDefined( self ) )
        {
            self.enteringVehicle = false;
            self freeze_player_controls( false );
            self notify( "weapon_object_destroyed" );
        }
        return false;
    }

    self thread givePlayerControlOfRCBomb();
    self.rcbomb thread watchForScramblers();
    self.killstreak_waitamount = 30000;
    self.enteringVehicle = false;
    self stopShellshock();

    // T5 _popups::DisplayKillstreakTeamMessageToAll: the C1 dispatcher's own
    // inbound/enemy dialog and splash already ran on acceptance.

    self updateRulesOnEnd();

    return true;
}

watchForScramblers() // self == rcbomb
{
    self endon( "death" );

    // If we enter the effect radius of the scrambler then stun. T5 reads
    // level.scramblerInnerRadiusSq (600^2 from _scrambler.gsc:13); the
    // flash-stun expiry set by _rcbomb_watchers::stun shares this predicate
    // so a stunned car inside a jammer is not re-enabled early.
    while ( true )
    {
        scrambled = self getClientFlag( level.const_flag_stunned );

        shouldScramble = isDefined( self.codolRCFlashUntil ) && self.codolRCFlashUntil > getTime();

        players = level.players;
        for ( i = 0; i < players.size; i++ )
        {
            if ( !isDefined( players[i] ) || !isDefined( players[i].scrambler ) )
                continue;

            player = players[i];
            scrambler = player.scrambler;

            // for teambased modes don't stun if the scrambler and rc are on the same team
            if ( level.teamBased && self.team == player.team )
                continue;

            // for non-teambased modes don't stun the rc if the owner's are the same player
            if ( !level.teamBased && self.owner == player )
                continue;

            if ( distanceSquared( scrambler.origin, self.origin ) < 600 * 600 )
            {
                shouldScramble = true;
                break;
            }
        }

        if ( shouldScramble == true && scrambled == false )
            self setClientFlag( level.const_flag_stunned );
        else if ( shouldScramble == false && scrambled == true )
            self clearClientFlag( level.const_flag_stunned );

        wait_delay = randomFloatRange( 0.25, 0.5 );
        wait( wait_delay );
    }
}

updateRulesOnEnd()
{
    team = self.rcbomb.team;
    //self endon("death");
    self endon( "disconnect" );

    self waittill( "rcbomb_done" );
    self maps\mp\killstreaks\_rcbomb_rules::killstreakStop( "rcbomb_mp", team );
}

updateKillstreakOnDisconnect()
{
    team = self.rcbomb.team;
    claim = self.codolRCClaim;
    self endon( "rcbomb_done" );
    self waittill( "disconnect" );
    // "disconnect" is delivered synchronously before the entity is freed;
    // the claim was captured above so the release never reads a freed player.
    maps\mp\killstreaks\_rcbomb_rules::release( claim );
}

//Protection on the edge case you are killed right as you drop the rc bomb but before you can link to it.
//The rc bomb will be deleted on respawn so ensure that the killstreak rules are updated.
updateKillstreakOnDeletion( team )
{
    self endon( "disconnect" );
    self endon( "rcbomb_done" );

    self waittill( "weapon_object_destroyed" );
    self maps\mp\killstreaks\_rcbomb_rules::killstreakStop( "rcbomb_mp", team );

    if ( isDefined( self.rcbomb ) )
        self.rcbomb delete();

    // The T5 use thread waits on "rcbomb_done" (givePlayerControlOfRCBomb and
    // updateRulesOnEnd). On this server the native remote link is released
    // silently at respawn, so no exit_vehicle arrives; complete the contract
    // here or the C1 dispatcher's activation lock would stay held.
    self.killstreak_waitamount = undefined;
    self notify( "rcbomb_done" );
}

carDetonateWaiter( vehicle )
{
    //self endon("death");
    self endon( "disconnect" );
    vehicle endon( "death" );

    watcher = maps\mp\killstreaks\_rcbomb_watchers::getWeaponObjectWatcher( "rcbomb" );
    if ( isDefined( level.disableRCBombTrigger ) && level.disableRCBombTrigger )
        watcher.disableDetonation = true;

    while ( ( !isDefined( vehicle.forceDetonation ) || !vehicle.forceDetonation ) && ( ( !self attackButtonPressed() ) || ( isDefined( level.disableRCBombTrigger ) && level.disableRCBombTrigger ) ) )
        wait 0.05;

    watcher.disableDetonation = false;
    watcher thread maps\mp\killstreaks\_rcbomb_watchers::waitAndDetonate( vehicle, 0 );
}

playerDisconnectWaiter( vehicle )
{
    vehicle endon( "death" );
    self endon( "rcbomb_done" );

    self waittill( "disconnect" );

    vehicle delete();
}

GameEndWatcher( vehicle )
{
    vehicle endon( "death" );
    self endon( "rcbomb_done" );

    level waittill( "game_ended" );

    watcher = maps\mp\killstreaks\_rcbomb_watchers::getWeaponObjectWatcher( "rcbomb" );
    watcher thread maps\mp\killstreaks\_rcbomb_watchers::waitAndDetonate( vehicle, 0 );
}

exitCarWaiter( vehicle )
{
    //self endon("death");
    self endon( "disconnect" );

    self waittill( "exit_vehicle" );

    self notify( "rcbomb_done" );
}

changeTeamWaiter( vehicle )
{
    self endon( "disconnect" );
    self endon( "rcbomb_done" );
    vehicle endon( "death" );

    self waittill_either( "joined_team", "joined_spectators" );

    vehicle.owner unlink();
    vehicle delete();
}

carDeathWaiter( vehicle )
{
    self endon( "disconnect" );
    self endon( "rcbomb_done" );

    self waittill( "death" );

    // T5 _hardpoints::removeUsedKillstreak( "rcbomb_mp" ) has no counterpart;
    // reward ownership on this server is settled by the dispatcher on the use
    // function's return. Kept unthreaded, as in T5.
    self notify( "rcbomb_done" );
}

carCleanupWaiter( owner ) // self == rcbomb
{
    self endon( "disconnect" );

    self waittill( "death" );

    // T5 assigns self.rcbomb on the car itself (a no-op); the intent is the
    // owner's reference. A deleted car already reads undefined in script.
    if ( isDefined( owner ) && isDefined( owner.rcbomb ) && owner.rcbomb == self )
        owner.rcbomb = undefined;
}

carTimer( vehicle )
{
    self endon( "disconnect" );
    vehicle endon( "death" );

    if ( getDvarInt( "scr_rcbomb_notimeout" ) )
        return;

    maps\mp\gametypes\_hostmigration::waitLongDurationWithHostMigrationPause( 20 );
    vehicle setClientFlag( level.const_flag_countdown );
    maps\mp\gametypes\_hostmigration::waitLongDurationWithHostMigrationPause( 6 );
    // this second state is needed for killcam
    vehicle setClientFlag( level.const_flag_timeout );
    maps\mp\gametypes\_hostmigration::waitLongDurationWithHostMigrationPause( 4 );

    watcher = maps\mp\killstreaks\_rcbomb_watchers::getWeaponObjectWatcher( "rcbomb" );
    watcher thread maps\mp\killstreaks\_rcbomb_watchers::waitAndDetonate( vehicle, 0 );
}

detonateIfTouchingSphere( origin, radius )
{
    if ( distanceSquared( self.origin, origin ) < radius * radius )
    {
        self rcbomb_force_explode();
    }
}

detonateAllIfTouchingSphere( origin, radius )
{
    rcbombs = getEntArray( "rcbomb", "targetname" );
    for ( index = 0; index < rcbombs.size; index++ )
    {
        rcbombs[index] detonateIfTouchingSphere( origin, radius );
    }
}

blowup( attacker )
{
    self.owner endon( "disconnect" );
    self endon( "death" );

    explosionOrigin = self.origin;

    if ( !isDefined( attacker ) )
        attacker = self.owner;

    origin = self.origin + (0,0,10);
    radius = 256;
    min_damage = 25;
    max_damage = 350;
    self radiusDamage( origin, radius, max_damage, min_damage, attacker, "MOD_EXPLOSIVE", "rcbomb_mp" );
    // C1 native contract has four args. Damage was delivered above once.
    physicsExplosionSphere( origin, radius, radius, 1 );
    // T5 _shellshock::rcbomb_earthquake is absent from the pinned T5 dump;
    // the generic rcbomb quake parameters are used.
    earthquake( 0.5, 0.5, origin, 512 );

    // T5 plays "mpl_sab_exp_suitcase_bomb_main" here; that alias is not in
    // the C1 asset inventory and no substitute is guessed.

    playFX( level._effect["rcbombexplosion"], explosionOrigin );

    self notify( "codol_rc_hidden" );
    self setModel( self.death_model );
    self hide();

    if ( isDefined( attacker ) && isPlayer( attacker ) && attacker != self.owner )
    {
        // T5: rcbombdestroy XP, destroyerRCBomb medal, _properks::destroyedKillstreak,
        // per-weapon destroyed stat. The dispatcher's destroyed-killstreak
        // credit consumes this single notify.
        attacker notify( "destroyed_killstreak", attacker.weaponUsedToDamage );
    }

    //self.owner endon("death");
    wait( 1 );
    if ( isDefined( self.neverDelete ) && self.neverDelete )
    {
        return;
    }

    self.owner unlink();
    self.owner.killstreak_waitamount = undefined;
    self delete();
}

RCCarAllowFriendlyFireDamage( eInflictor, eAttacker, sMeansOfDeath, sWeapon )
{
    if ( isDefined( eAttacker ) && eAttacker == self.owner )
        return true;

    if ( isDefined( eInflictor ) && eInflictor isLinkedTo( self ) )
        return true;

    return false;
}

// ---------------------------------------------------------------------------
// T5 script helpers imported for this module (T5-STEAM-GSC common_scripts/
// utility.gsc:799 vector_scale; maps/mp/_utility.gsc:2037 spread_array_thread,
// :2095 freeze_player_controls). The mounted IW4 utility has no counterparts.

vector_scale( vec, scale )
{
    vec = ( vec * scale );
    return vec;
}

spread_array_thread( entities, process, var1, var2, var3 )
{
    keys = getArrayKeys( entities );

    if ( isDefined( var3 ) )
    {
        for ( i = 0; i < keys.size; i++ )
        {
            entities[ keys[ i ] ] thread [[ process ]]( var1, var2, var3 );
            wait .1;
        }

        return;
    }

    if ( isDefined( var2 ) )
    {
        for ( i = 0; i < keys.size; i++ )
        {
            entities[ keys[ i ] ] thread [[ process ]]( var1, var2 );
            wait .1;
        }

        return;
    }

    if ( isDefined( var1 ) )
    {
        for ( i = 0; i < keys.size; i++ )
        {
            entities[ keys[ i ] ] thread [[ process ]]( var1 );
            wait .1;
        }

        return;
    }

    for ( i = 0; i < keys.size; i++ )
    {
        entities[ keys[ i ] ] thread [[ process ]]();
        wait .1;
    }
}

freeze_player_controls( boolean )
{
    assertEx( isDefined( boolean ), "'freeze_player_controls()' has not been passed an argument properly." );

    if ( boolean && isAlive( self ) )
    {
        self freezeControls( boolean );
    }
    //'!level.gameEnded' check prevents the player from having their controls unfrozen during the end of rounds
    else if ( !boolean && isAlive( self ) && !level.gameEnded )
    {
        self freezeControls( boolean );
    }
}

// ---------------------------------------------------------------------------

trigger_monitor_init()
{
    triggers = getEntArray( "trigger_hurt", "classname" );
    spread_array_thread( triggers, ::trigger_monitor );
}

trigger_monitor()
{
    for ( ;; )
    {
        self waittill( "trigger", entity );
        if ( isDefined( entity ) && isDefined( entity.targetname ) && entity.targetname == "rcbomb" )
            entity thread rcbomb_force_explode();
        wait 0.1;
    }
}

rcbomb_force_explode()
{
    self endon( "death" );
    while ( !isDefined( self getSeatOccupant( 0 ) ) )
        wait 0.1;
    self doDamage( 10, self.origin + (0,0,10), self.owner, self.owner, 0, "MOD_EXPLOSIVE", 0, "rcbomb_mp" );
}

// Original T5 placement, with native typed trace adapters and the T5 debug
// helpers. rcbomb_debug_box draws the twelve box edges through the registered
// `line` builtin; `box` is not a registered builtin on this server.

getPlacementStartHeight()
{
	startheight = 50;
	
	switch( self GetStance() )
	{
		case "crouch":
			startheight = 30;
			break;
		case "prone":
			startheight = 15;
			break;
	}
	return startheight;
}

calculateSpawnOrigin( origin, angles )
{

	distance_from_player = 70;

	startheight = getPlacementStartHeight();
	
	mins = (-5,-5,0); // keep the min z 0 so the return point is the exact height of the collision
	maxs = (5,5,10);

	//trace = bulletTrace( startPoint, endPoint, false, self );
	
	startPoints = [];
	startAngles = [];
	wheelCounts = [];
	testCheck = [];
	largestCount = 0;
	largestCountIndex = 0;
	
	testangles = [];
	testangles[0] = (0,0,0);
	testangles[1] = (0,20,0);
	testangles[2] = (0,-20,0);
	testangles[3] = (0,45,0);
	testangles[4] = (0,-45,0);
	
	heightoffset = 5;
	
	for (i = 0; i < testangles.size; i++ )
	{
		startAngles[i] = ( 0, angles[1], 0 );
		startPoint = origin + vector_scale( anglestoforward( startAngles[i] + testangles[i]), distance_from_player );
		endPoint = startPoint - (0,0,100);
		startPoint = startPoint + (0,0,startheight);

		// ignore water on this one
		mask =  3; // T5 PhysicsTraceMaskPhysics | Vehicle, as native codol_rcPlacementTrace categories
			
		// using physicstrace so we dont slip through small cracks
		trace = codol_rcPlacementTrace( startPoint, endPoint, mins, maxs, self, mask);
			
		// if any player intersection then skip
		if ( IsDefined(trace["entity"]) && IsPlayer(trace["entity"]))
			continue;
			 
		startPoints[i] = trace["position"] + (0,0,heightoffset);
		wheelCounts[i] = testWheelLocations(startPoints[i],startAngles[i],heightoffset);
		
		if ( positionWouldTelefrag( startPoints[i] ) )
			continue;
	
		if ( largestCount < wheelCounts[i] )
		{
			largestCount = wheelCounts[i];
			largestCountIndex = i;
		}
		
		testCheck[i] = false;
		
		// going to early out on the first I find with valid tire positions
		if ( wheelCounts[i] >=  3 )
		{
			testCheck[i] = true;

			if ( testSpawnOrigin( startPoints[i], startAngles[i] ) )
			{
				placement = SpawnStruct();
				placement.origin = startPoints[i];
				placement.angles = startAngles[i];
				return placement;
			}
		}
	}
	
	for (i = 0; i < testangles.size; i++ )
	{
		if ( !testCheck[i] )
		{
			if ( wheelCounts[i] >=  2 )
			{
				if ( testSpawnOrigin( startPoints[i], startAngles[i] ) )
					{
						placement = SpawnStruct();
						placement.origin = startPoints[i];
						placement.angles = startAngles[i];
						return placement;
					}
				}
		}
	}
	
	return undefined;
}

testWheelLocations( origin, angles, heightoffset )
{
	forward = 13;
	side = 10;
	
	wheels = [];
	wheels[0] = ( forward, side, 0 );
	wheels[1] = ( forward, -1 * side, 0 );
	wheels[2] = ( -1 * forward, -1 * side, 0 );
	wheels[3] = ( -1 * forward, side, 0 );

	height = 5;
	touchCount = 0;
	
	yawangles = (0,angles[1],0);
	
	for (i = 0; i < 4; i++ )
	{
		wheel = RotatePoint( wheels[i], yawangles  );
		startPoint = origin + wheel;
		endPoint = startPoint + (0,0,(-1 * height) - heightoffset);
		startPoint = startPoint + (0,0,height - heightoffset) ;
	
		trace = bulletTrace( startPoint, endPoint, false, self );
		if ( trace["fraction"] < 1 )
		{
			touchCount++;
			rcbomb_debug_line( startPoint, endPoint,(1,0,0) );
		}
		else
		{
			rcbomb_debug_line( startPoint, endPoint,(0,0,1) );
		}
	}
	
	return touchCount;
}

testSpawnOrigin( origin, angles )
{
	liftedorigin = origin + (0,0,5);
	size = 12;
	height = 15;
	mins = (-1 * size,-1 * size,0 );
	maxs = ( size,size,height );
	absmins = liftedorigin + mins;
	absmaxs = liftedorigin + maxs;
	
	// check to see if we would telefrag any players
	if ( codol_rcBoundsWouldTelefrag( absmins, absmaxs ) )
	{
		rcbomb_debug_box( liftedorigin, mins, maxs,(1,0,0) );
		return false;
	}
	else
	{
		rcbomb_debug_box( liftedorigin, mins, maxs,(0,0,1) );
	}
	
	startheight = getPlacementStartHeight();
	
	mask =  7; // T5 PhysicsTraceMaskPhysics | Vehicle | Water, as native codol_rcPlacementTrace categories
	// test the volume where we are going to place the car
	// note that this physics trace is not an  oriented box.
	trace = codol_rcPlacementTrace( liftedorigin, (origin +(0,0,1)), mins, maxs, self, mask);

	if ( trace["fraction"] < 1 )
	{
		rcbomb_debug_box( trace["position"], mins, maxs, (1,0,0) );
		return false;
	}
	else
	{
		rcbomb_debug_box( (origin +(0,0,1)), mins, maxs, (0,1,0) );
	}

	// swept trace of a small bounding box from head height to where we are placing the car
	// to make sure there is no wall between us and the car
	size = 2.5;
	height = size * 2;
	mins = (-1 * size,-1 * size,0 );
	maxs = ( size,size,height );
	
	sweeptrace = codol_rcPlacementTrace( (self.origin + (0,0,startheight)), liftedorigin, mins, maxs, self, mask);

	if ( sweeptrace["fraction"] < 1 )
	{
		rcbomb_debug_box( sweeptrace["position"], mins, maxs, (1,0,0) );
		return false;
	}
	
	return true;
}

//Starts a thread on my trigger_hurt's to check for rc_bombs. Self is the trigger.
rcbomb_debug_box( origin, mins, maxs, color )
{
/#
	debug_rcbomb = GetDvar( "debug_rcbomb" );
	if ( debug_rcbomb == "1" )
	{
		// T5 box(); drawn as its twelve edges with the registered line().
		a = origin + mins;
		b = origin + maxs;
		c = [];
		c[0] = ( a[0], a[1], a[2] ); c[1] = ( b[0], a[1], a[2] );
		c[2] = ( b[0], b[1], a[2] ); c[3] = ( a[0], b[1], a[2] );
		c[4] = ( a[0], a[1], b[2] ); c[5] = ( b[0], a[1], b[2] );
		c[6] = ( b[0], b[1], b[2] ); c[7] = ( a[0], b[1], b[2] );
		for ( i = 0; i < 4; i++ )
		{
			line( c[i], c[(i + 1) % 4], color, 1, 1, 300 );
			line( c[i + 4], c[((i + 1) % 4) + 4], color, 1, 1, 300 );
			line( c[i], c[i + 4], color, 1, 1, 300 );
		}
	}
#/
}

rcbomb_debug_line( start, end, color )
{
/#
	debug_rcbomb = GetDvar( "debug_rcbomb" );
	if ( debug_rcbomb == "1" )
	{
			line(start, end, color, 1, 1, 300);
	}
#/
}
