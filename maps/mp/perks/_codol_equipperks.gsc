// CoDOL perk effects on equipment and care packages: Hacker (C1 perk 15),
// Hacker Pro (30), Engineer (90) and Scavenger Pro (19).
//
// Every effect is keyed on the C1 ability modifier the selected perk carries
// (mp/coPerkStatsTable.csv -> mp/coAbilityModifiersTable.csv), never on an
// IW4 perk name, and reads its number through the native selected-perk value.
//
//   modCaptureEnemyEquipmentTime (62)  Hacker, Engineer: hold use on an enemy
//       claymore or C4 for <value> seconds (table default 4, each perk -2) to
//       take it over. Adapted from T5 maps/mp/gametypes/_weaponobjects.gsc
//       hackerInit/hackerThink/hackerResult/canHack (Treyarch); C1 hint text
//       MP_HACK_CLAYMORE / MP_HACK_C4 ("hold [use] to acquire"), MP_HACKING.
//   modCrateTrap (61)  Hacker: capturing an enemy care package leaves it in
//       place booby-trapped. T5 _supplydrop.gsc dropCrate /
//       spawn_explosive_crate / watch_explosive_crate.
//   modOpenCrateAsOwner (78)  Hacker Pro: an enemy crate opens in the owner's
//       time. T5 crateUseThink specialty_fastinteract (level.crateOwnerUseTime).
//   modExtraExplosive (93)  Scavenger Pro: one extra frag / semtex at spawn.
//       C1 PERKS_DESC_SCAVENGER_PRO; +1 is T5 _class.gsc specialty_twogrenades.
//
// set scr_codol_perklog 1 logs every effect as it happens.
#include maps\mp\_utility;
#include common_scripts\utility;
#include maps\mp\gametypes\_hud_util;

init()
{
    precacheString( &"MP_HACKING" );
    precacheString( &"MP_HACK_CLAYMORE" );
    precacheString( &"MP_HACK_C4" );
}

perkLog( message )
{
    if ( getDvarInt( "scr_codol_perklog" ) )
        codolPrintLn( "CoDOL perk: " + message );
}

nameOf( player )
{
    if ( isDefined( player ) && isDefined( player.name ) )
        return player.name;
    return "(gone)";
}

isEnemyOf( player, owner, team )
{
    if ( !isDefined( player ) || !isDefined( owner ) || player == owner )
        return false;
    if ( level.teamBased && isDefined( team ) && isDefined( player.team ) && player.team == team )
        return false;
    return true;
}

// self = native missile. A taken-over device credits its new owner.
detonateAsOwner()
{
    if ( isDefined( self.codolHackedBy ) && isDefined( self.owner ) && isPlayer( self.owner ) )
        self detonate( self.owner );
    else
        self detonate();
}

/**********************************************************
*   modCaptureEnemyEquipmentTime: take over enemy equipment
***********************************************************/

// Seconds this player needs to take over enemy equipment, or undefined when
// no selected perk grants it.
captureTime( player )
{
    if ( !isPlayer( player ) || !player HasAbilityModifier( "modCaptureEnemyEquipmentTime" ) )
        return undefined;
    return player getSelectedPerkModifierValue( "modCaptureEnemyEquipmentTime" );
}

// T5 canHack, reduced to the state this server tracks.
canCapture( player )
{
    if ( !isDefined( self ) || !isDefined( player ) || !isPlayer( player ) || !isReallyAlive( player ) )
        return false;
    if ( isDefined( self.codolPickupClosed ) )
        return false;
    if ( !isEnemyOf( player, self.owner, self.team ) )
        return false;
    if ( !isDefined( captureTime( player ) ) )
        return false;
    if ( isDefined( player.isDefusing ) && player.isDefusing )
        return false;
    if ( isDefined( player.isPlanting ) && player.isPlanting )
        return false;
    if ( isDefined( player.lastStand ) && player.lastStand )
        return false;
    if ( player isUsingRemote() )
        return false;
    return player isOnGround();
}

// self = claymore or C4 (native missile), once it has come to rest.
// kind is "claymore" or "c4".
hackable( kind )
{
    self endon( "death" );
    self endon( "codol_hacked" );
    level endon( "game_ended" );

    if ( isDefined( self.codolHackTrigger ) )
        self.codolHackTrigger delete();
    trigger = spawn( "script_origin", self.origin + ( 0, 0, 20 ) );
    trigger linkTo( self );
    self.codolHackTrigger = trigger;
    level thread deleteHackTriggerWith( self, trigger );
    trigger setCursorHint( "HINT_NOICON" );
    if ( kind == "claymore" )
        trigger setHintString( &"MP_HACK_CLAYMORE" );
    else
        trigger setHintString( &"MP_HACK_C4" );
    trigger makeUsable();
    self thread updateHackUsers( trigger );

    for ( ;; )
    {
        trigger waittill( "trigger", player );
        if ( !self canCapture( player ) )
            continue;
        if ( !self captureHold( trigger, player ) )
            continue;
        self thread takeOver( player, kind );
        return;
    }
}

deleteHackTriggerWith( device, trigger )
{
    device waittill_any( "death", "codol_hacked" );
    if ( isDefined( trigger ) )
        trigger delete();
}

// Only enemies holding the modifier can use the take-over trigger; the owner
// keeps the separate pickup trigger (equipment/_pickup.gsc).
updateHackUsers( trigger )
{
    self endon( "death" );
    self endon( "codol_hacked" );
    trigger endon( "death" );
    level endon( "game_ended" );
    for ( ;; )
    {
        foreach ( player in level.players )
        {
            if ( self canCapture( player ) )
                trigger enablePlayerUse( player );
            else
                trigger disablePlayerUse( player );
        }
        wait 0.5;
    }
}

// T5 hackerResult: hold use for the whole capture time.
captureHold( trigger, player )
{
    hackTime = captureTime( player );
    perkLog( player.name + " starts taking over " + self.weaponName + " " + self getEntityNumber() + " of " +
        nameOf( self.owner ) + " (modCaptureEnemyEquipmentTime " + hackTime + "s)" );
    startTime = getTime();
    success = true;

    player _disableWeapon();
    bar = undefined;
    text = undefined;
    if ( hackTime > 0 )
    {
        bar = player createPrimaryProgressBar();
        bar updateBar( 0.01, 1 / hackTime );
        text = player createPrimaryProgressBarText();
        text setText( &"MP_HACKING" );
    }
    for ( ;; )
    {
        if ( !isDefined( self ) || !isDefined( trigger ) || !self canCapture( player ) || !player useButtonPressed() )
        {
            success = false;
            break;
        }
        if ( getTime() - startTime >= hackTime * 1000 )
            break;
        wait 0.05;
    }
    if ( isDefined( bar ) )
        bar destroyElem();
    if ( isDefined( text ) )
        text destroyElem();
    if ( isReallyAlive( player ) )
        player _enableWeapon();
    if ( isDefined( self ) )
        perkLog( nameOf( player ) + " take-over of " + self.weaponName + " " + self getEntityNumber() + " after " +
            ( getTime() - startTime ) + "ms: " + success );
    return success;
}

// T5 hackerThink: the device changes owner and team in place.
takeOver( player, kind )
{
    self notify( "codol_hacked" );
    previous = self.owner;
    if ( isDefined( self.codolPickupTrigger ) )
        self.codolPickupTrigger delete();
    self.codolPickupTrigger = undefined;
    // claymoreDetonation's detection volume belongs to the old owner's thread.
    if ( isDefined( self.codolDetectionArea ) )
        self.codolDetectionArea delete();
    self.codolDetectionArea = undefined;

    if ( kind == "claymore" )
    {
        if ( isDefined( previous ) && isDefined( previous.claymorearray ) )
            previous.claymorearray = array_remove( previous.claymorearray, self );
        if ( !isDefined( player.claymorearray ) )
            player.claymorearray = [];
        player.claymorearray = array_removeUndefined( player.claymorearray );
        if ( player.claymorearray.size >= level.maxPerPlayerExplosives )
            player.claymorearray[0] detonate();
        player.claymorearray[player.claymorearray.size] = self;
    }
    else
    {
        if ( isDefined( previous ) && isDefined( previous.c4array ) )
            previous.c4array = array_remove( previous.c4array, self );
        if ( !isDefined( player.c4array ) )
            player.c4array = [];
        player.c4array = array_removeUndefined( player.c4array );
        if ( player.c4array.size >= level.maxPerPlayerExplosives )
            player.c4array[0] detonate();
        // watchC4 starts the double-tap detonator with the first C4 a player
        // throws; a player who took one over may not have thrown any.
        if ( !player.c4array.size )
            player thread maps\mp\gametypes\_weapons::watchC4AltDetonate();
        player.c4array[player.c4array.size] = self;
    }

    self.owner = player;
    self.team = player.team;
    self.codolHackedBy = player;
    self playSound( "claymore_activated" );
    perkLog( player.name + " took over " + self.weaponName + " " + self getEntityNumber() + " from " +
        nameOf( previous ) + "; it now belongs to team " + self.team );

    if ( isDefined( previous ) )
        previous notify( "destroyed_explosive" );
    level notify( "codol_equipment_hacked", self, player );

    if ( kind == "claymore" )
    {
        self thread maps\mp\gametypes\_weapons::setClaymoreTeamHeadIcon( player.pers["team"] );
        self thread maps\mp\gametypes\_weapons::claymoreDetonation();
    }
    else
    {
        self thread maps\mp\equipment\_pickup::missilePickup( player, self.weaponName, &"WEAPON_C4_PICKUP" );
        self thread hackable( "c4" );
    }
}

/**********************************************************
*   Care packages: modOpenCrateAsOwner and modCrateTrap
***********************************************************/

// self = crate. The capture time for a non-owner, or undefined for the
// default (_airdrop useHoldThink's 3000 ms).
crateStealTime( player )
{
    if ( !isEnemyOf( player, self.owner, self.team ) )
        return undefined;
    if ( !player HasAbilityModifier( "modOpenCrateAsOwner" ) )
        return undefined;
    perkLog( player.name + " steals crate " + self getEntityNumber() + " (" + self.crateType + ") of " + nameOf( self.owner ) +
        " in the owner's 500ms (modOpenCrateAsOwner)" );
    // T5 level.crateOwnerUseTime; the same 500 ms as _airdrop crateOwnerCaptureThink.
    return 500;
}

// self = crate the player has just captured (and been given the contents of).
// True when the crate stays in the world as a trap.
crateTrapArmed( player )
{
    if ( isDefined( self.codolTrapper ) || !isDefined( self.owner ) || !isDefined( self.crateType ) || self.crateType == "nuke" )
        return false;
    if ( !isEnemyOf( player, self.owner, self.team ) || !player HasAbilityModifier( "modCrateTrap" ) )
        return false;
    self.codolTrapper = player;
    self thread updateTrapUsers();
    perkLog( player.name + " booby-trapped crate " + self getEntityNumber() + " (" + self.crateType + ") of " +
        nameOf( self.owner ) + " (modCrateTrap)" );
    return true;
}

// T5 spawn_explosive_crate: only the crate's own side can still open it.
updateTrapUsers()
{
    self endon( "death" );
    level endon( "game_ended" );
    for ( ;; )
    {
        foreach ( player in level.players )
        {
            if ( player == self.codolTrapper || ( level.teamBased && player.team != self.team ) )
                self disablePlayerUse( player );
            else
                self enablePlayerUse( player );
        }
        level waittill_any( "joined_team", "player_spawned", "connected" );
    }
}

// T5 watch_explosive_crate: whoever opens a trapped crate sets it off, with
// the trapper as attacker. T5 RadiusDamage 256 / 300 / 75, MOD_EXPLOSIVE; C1
// has no supplydrop_mp, so the care package marker names the weapon.
springCrateTrap( player )
{
    trapper = self.codolTrapper;
    if ( !isDefined( trapper ) )
        trapper = self;
    origin = self.origin;
    perkLog( player.name + " opened booby-trapped crate " + self getEntityNumber() + " set by " + nameOf( self.codolTrapper ) );
    self playSound( "grenade_explode_metal" );
    if ( isDefined( level.c4Death ) )
        playFx( level.c4Death, origin );
    self radiusDamage( origin + ( 0, 0, 16 ), 256, 300, 75, trapper, "MOD_EXPLOSIVE", "airdrop_marker_mp" );
    self maps\mp\killstreaks\_airdrop::deleteCrate();
}

/**********************************************************
*   modExtraExplosive: extra starting lethal grenade
***********************************************************/

// After codol_giveEquipment. The frag / semtex identities are the same the
// scavenger exclusion uses (gametypes/_weapons.gsc handleScavengerBagPickup),
// which keeps this grenade from being resupplied.
giveExtraExplosive()
{
    if ( !self HasAbilityModifier( "modExtraExplosive" ) )
        return;
    foreach ( offhand in self getWeaponsListOffhands() )
    {
        base = codol_getWeaponBaseName( offhand );
        if ( base != "frag_grenade_mp" && base != "frag_grenade_short_mp" && base != "semtex_mp" )
            continue;
        before = self getAmmoCount( offhand );
        self setWeaponAmmoClip( offhand, self getWeaponAmmoClip( offhand ) + 1 );
        perkLog( self.name + " modExtraExplosive " + offhand + " " + before + " -> " + self getAmmoCount( offhand ) );
    }
}
