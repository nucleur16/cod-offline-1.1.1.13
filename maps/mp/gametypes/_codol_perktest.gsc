// CoDOL multiplayer perk test hooks (Hacker, Hacker Pro, Engineer, Scavenger
// Pro). Idle unless the dvar is set. Devices are thrown by the engine's own
// offhand code (_codol_equiptest throwEquipment) and captured by holding the
// real use button (codol_botPressButtons 0x28); nothing here applies an
// effect itself.
//
//   set scr_codol_perktest "hack <owner> <taker> <claymore_mp|c4_mp> [perk]"
//       owner throws the device; taker (an enemy) gets [perk] (default
//       specialty_hacker), stands at the device and holds use.
//   set scr_codol_perktest "trip <owner> <victim> <claymore_mp|bouncingbetty_mp> [perk]"
//       owner plants the device; victim gets [perk] (none = control) and is
//       walked into it. Reports trigger-to-explosion time.
//   set scr_codol_perktest "crate <owner> <taker> [perk] [opener]"
//       a uav care package lands next to owner; taker gets [perk] and holds use
//       on it; then [opener] (a client number) holds use on whatever is left.
//   set scr_codol_perktest "extra <client>"
//       gives specialty_extraammo and runs the spawn-time extra explosive grant.
#include maps\mp\_utility;
#include common_scripts\utility;

init()
{
    if ( isDefined( level.codolPerkTestStarted ) )
        return;
    level.codolPerkTestStarted = true;
    setDvar( "scr_codol_perktest", "" );
    level thread watchDvar();
}

watchDvar()
{
    level endon( "game_ended" );
    for ( ;; )
    {
        wait 0.5;
        request = getDvar( "scr_codol_perktest" );
        if ( request == "" )
            continue;
        setDvar( "scr_codol_perktest", "" );
        setDvar( "scr_codol_perklog", 1 );
        tokens = strTok( request, " " );
        if ( tokens.size < 2 )
            continue;
        a = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( tokens[1] ) );
        b = undefined;
        if ( tokens.size > 2 && tokens[2] == "e" && isDefined( a ) )
            b = enemyOf( a );
        else if ( tokens.size > 2 )
            b = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( tokens[2] ) );
        if ( !isDefined( a ) )
        {
            report( "no client " + tokens[1] );
            continue;
        }
        if ( tokens[0] == "extra" )
            a thread testExtra();
        else if ( tokens[0] == "hack" && isDefined( b ) && tokens.size > 3 )
            level thread testHack( a, b, tokens[3], optional( tokens, 4, "specialty_hacker" ) );
        else if ( tokens[0] == "trip" && isDefined( b ) && tokens.size > 3 )
            level thread testTrip( a, b, tokens[3], optional( tokens, 4, "none" ) );
        else if ( tokens[0] == "crate" && isDefined( b ) )
            level thread testCrate( a, b, optional( tokens, 3, "none" ), optional( tokens, 4, "none" ) );
        else
            report( "bad request " + request );
    }
}

// "e" in place of the second client number: the first enemy of the first.
enemyOf( player )
{
    foreach ( other in level.players )
    {
        if ( other == player || !isDefined( other.team ) || other.team == "spectator" )
            continue;
        if ( level.teamBased && other.team == player.team )
            continue;
        return other;
    }
    return undefined;
}

optional( tokens, index, fallback )
{
    if ( tokens.size > index )
        return tokens[index];
    return fallback;
}

report( message )
{
    codolPrintLn( "CoDOL perktest: " + message );
}

waitAlive()
{
    for ( tries = 0; isDefined( self ) && !isAlive( self ) && tries < 40; tries++ )
        wait 0.5;
    return isDefined( self ) && isAlive( self );
}

// A test client that stands still, only presses what the script presses, and
// survives stray fire from the other bots for the length of a test.
idle()
{
    self enableCombatBot( false );
    self freezeControls( false );
    self.health = 100000;
    self.maxhealth = 100000;
}

grant( perk )
{
    if ( perk == "none" )
        return;
    foreach ( name in strTok( perk, "," ) )
        self maps\mp\perks\_perks::givePerk( name );
    captureTime = self getSelectedPerkModifierValue( "modCaptureEnemyEquipmentTime" );
    if ( !isDefined( captureTime ) )
        captureTime = "none";
    report( self.name + " given " + perk + ": modCaptureEnemyEquipmentTime " + self HasAbilityModifier( "modCaptureEnemyEquipmentTime" ) +
        " (" + captureTime + ")" +
        ", modCrateTrap " + self HasAbilityModifier( "modCrateTrap" ) +
        ", modOpenCrateAsOwner " + self HasAbilityModifier( "modOpenCrateAsOwner" ) +
        ", modDelayTriggeredExplosive " + self HasAbilityModifier( "modDelayTriggeredExplosive" ) +
        ", modExtraExplosive " + self HasAbilityModifier( "modExtraExplosive" ) );
}

// Stand dist units from spot, on the ground, looking at spot.
// Tries eight directions, starting at yaw, for floor near the spot's height.
standAt( spot, dist, yaw )
{
    for ( attempt = 0; attempt < 8; attempt++ )
    {
        offset = anglesToForward( ( 0, yaw + attempt * 45, 0 ) );
        pos = spot + ( offset[0] * dist, offset[1] * dist, 0 );
        trace = bulletTrace( pos + ( 0, 0, 48 ), pos - ( 0, 0, 128 ), false, self );
        self setOrigin( trace["position"] + ( 0, 0, 2 ) );
        for ( i = 0; i < 20 && !self isOnGround(); i++ )
            wait 0.05;
        if ( self isOnGround() && abs( self.origin[2] - spot[2] ) < 40 )
            break;
    }
    wait 0.1;
    eye = self.origin + ( 0, 0, 60 );
    self setPlayerAngles( vectorToAngles( spot - eye ) );
}

holdUse( seconds )
{
    self codol_botPressButtons( 40, seconds );
}

lastDevice( weapon )
{
    list = [];
    if ( weapon == "claymore_mp" && isDefined( self.claymorearray ) )
        list = self.claymorearray;
    else if ( weapon == "c4_mp" && isDefined( self.c4array ) )
        list = self.c4array;
    else if ( weapon == "bouncingbetty_mp" && isDefined( self.equipmentMines ) )
        list = self.equipmentMines;
    device = undefined;
    foreach ( item in list )
        if ( isDefined( item ) )
            device = item;
    return device;
}

testExtra()
{
    if ( !self waitAlive() )
        return;
    foreach ( offhand in self getWeaponsListOffhands() )
        report( self.name + " offhand " + offhand + " ammo " + self getAmmoCount( offhand ) + " clip " + self getWeaponAmmoClip( offhand ) +
            " stock " + self getWeaponAmmoStock( offhand ) + " start " + weaponStartAmmo( offhand ) + " max " + weaponMaxAmmo( offhand ) +
            " clipsize " + weaponClipSize( offhand ) );
    self grant( "specialty_extraammo" );
    self maps\mp\perks\_codol_equipperks::giveExtraExplosive();
    foreach ( offhand in self getWeaponsListOffhands() )
        report( self.name + " after: offhand " + offhand + " ammo " + self getAmmoCount( offhand ) );
}

testHack( owner, taker, weapon, perk )
{
    if ( !owner waitAlive() || !taker waitAlive() )
        return;
    if ( level.teamBased && owner.team == taker.team )
    {
        report( owner.name + " and " + taker.name + " are on one team" );
        return;
    }
    owner idle();
    taker idle();
    owner thread maps\mp\gametypes\_codol_equiptest::throwEquipment( 2, weapon );
    device = undefined;
    for ( i = 0; i < 40 && !isDefined( device ); i++ )
    {
        wait 0.25;
        device = owner lastDevice( weapon );
    }
    if ( !isDefined( device ) )
    {
        report( owner.name + " planted no " + weapon );
        return;
    }
    wait 2;
    if ( !isDefined( device ) )
    {
        report( weapon + " went away before the take-over" );
        return;
    }
    report( weapon + " " + device getEntityNumber() + " of " + owner.name + " (" + owner.team + ") at " + device.origin );
    taker grant( perk );
    // Claymores face away from their owner; approach from behind.
    yaw = owner.angles[1] + 180;
    taker standAt( device.origin, 40, yaw );
    start = getTime();
    taker holdUse( 6 );
    for ( i = 0; i < 160; i++ )
    {
        wait 0.05;
        if ( !isDefined( device ) || ( isDefined( device.owner ) && device.owner == taker ) )
            break;
    }
    taker codol_botPressButtons( 0, 0 );
    if ( isDefined( device ) && device.owner != taker )
        report( taker.name + " at " + taker.origin + " onGround " + taker isOnGround() + " angles " + taker getPlayerAngles() +
            "; " + weapon + " at " + device.origin + " hack trigger " + isDefined( device.codolHackTrigger ) );
    if ( !isDefined( device ) )
    {
        report( weapon + " is gone after " + ( getTime() - start ) + "ms" );
        return;
    }
    inArray = false;
    if ( isDefined( taker lastDevice( weapon ) ) && taker lastDevice( weapon ) == device )
        inArray = true;
    report( weapon + " " + device getEntityNumber() + " owner " + device.owner.name + " team " + device.team +
        " after " + ( getTime() - start ) + "ms; in " + taker.name + "'s list " + inArray );
    wait 1;
    // The previous owner is now an enemy of the device.
    owner thread reportDamage();
    device thread reportExplosion( weapon );
    if ( weapon == "claymore_mp" )
    {
        taker setOrigin( taker.origin + anglesToForward( ( 0, yaw, 0 ) ) * 300 );
        owner.health = 100000;
        owner.maxhealth = 100000;
        owner standAt( device.origin, 150, device.angles[1] );
        owner walkInto( device );
        // Stay in the blast while the (possibly delayed) claymore goes off.
        for ( i = 0; i < 80 && isDefined( device ) && distance( owner.origin, device.origin ) > 48; i++ )
        {
            dir = vectorNormalize( ( device.origin[0] - owner.origin[0], device.origin[1] - owner.origin[1], 0 ) );
            owner setVelocity( dir * 120 );
            wait 0.05;
        }
        wait 4;
        return;
    }
    owner standAt( device.origin, 60, yaw + 90 );
    taker setOrigin( taker.origin + anglesToForward( ( 0, yaw, 0 ) ) * 300 );
    wait 0.5;
    // watchC4AltDetonate: two short use presses.
    owner thread reportDamage();
    report( taker.name + " double-taps use to detonate; " + owner.name + " is " + distance( owner.origin, device.origin ) +
        " from it with health " + owner.health );
    taker holdUse( 0.1 );
    wait 0.25;
    taker holdUse( 0.1 );
    wait 2;
    report( owner.name + " health after the detonation " + owner.health );
}

reportDamage()
{
    self endon( "disconnect" );
    self notify( "codol_perktest_damage" );
    self endon( "codol_perktest_damage" );
    self endon( "death" );
    self thread stopReportDamage();
    for ( ;; )
    {
        self waittill( "damage", amount, attacker, direction, point, type );
        if ( !isDefined( type ) )
            type = "(no type)";
        name = "(none)";
        if ( isDefined( attacker ) && isDefined( attacker.name ) )
            name = attacker.name;
        report( self.name + " (" + self.team + ") took " + amount + " " + type + " damage from " + name );
    }
}

stopReportDamage()
{
    self endon( "codol_perktest_damage" );
    wait 8;
    if ( isDefined( self ) )
        self notify( "codol_perktest_damage" );
}

walkInto( device )
{
    for ( i = 0; i < 60 && isDefined( device ) && !isDefined( device.codolPickupClosed ); i++ )
    {
        dir = vectorNormalize( ( device.origin[0] - self.origin[0], device.origin[1] - self.origin[1], 0 ) );
        self setVelocity( dir * 120 );
        wait 0.05;
    }
    if ( isDefined( device ) && isDefined( device.codolPickupClosed ) )
        report( device.weaponName + " " + device getEntityNumber() + " triggered by " + self.name + " (modDelayTriggeredExplosive " +
            self HasAbilityModifier( "modDelayTriggeredExplosive" ) + ")" );
}

testTrip( owner, victim, weapon, perk )
{
    if ( !owner waitAlive() || !victim waitAlive() )
        return;
    if ( level.teamBased && owner.team == victim.team )
    {
        report( owner.name + " and " + victim.name + " are on one team" );
        return;
    }
    owner idle();
    victim idle();
    owner thread maps\mp\gametypes\_codol_equiptest::throwEquipment( 2, weapon );
    device = undefined;
    for ( i = 0; i < 40 && !isDefined( device ); i++ )
    {
        wait 0.25;
        device = owner lastDevice( weapon );
    }
    if ( !isDefined( device ) )
    {
        report( owner.name + " planted no " + weapon );
        return;
    }
    wait 3;
    if ( !isDefined( device ) )
        return;
    // Keep the owner away from the blast.
    owner setOrigin( owner.origin + anglesToForward( ( 0, owner.angles[1] + 180, 0 ) ) * 400 );
    victim grant( perk );
    victim.health = 100000;
    victim.maxhealth = 100000;
    yaw = device.angles[1];
    if ( weapon != "claymore_mp" )
        yaw = owner.angles[1];
    victim standAt( device.origin, 150, yaw );
    report( victim.name + " (" + victim.team + ", perk " + perk + ") walks at " + weapon + " " + device getEntityNumber() + " of " + owner.name );
    device thread reportExplosion( weapon );
    victim walkInto( device );
}

reportExplosion( weapon )
{
    number = self getEntityNumber();
    triggered = undefined;
    while ( isDefined( self ) )
    {
        if ( !isDefined( triggered ) && isDefined( self.codolPickupClosed ) )
            triggered = getTime();
        wait 0.05;
    }
    if ( isDefined( triggered ) )
        report( weapon + " " + number + " exploded " + ( getTime() - triggered ) + "ms after it triggered" );
    else
        report( weapon + " " + number + " went away without triggering" );
}

testCrate( owner, taker, perk, openerNumber )
{
    if ( !owner waitAlive() || !taker waitAlive() )
        return;
    owner idle();
    taker idle();
    spot = owner.origin + anglesToForward( ( 0, owner.angles[1], 0 ) ) * 96;
    trace = bulletTrace( spot + ( 0, 0, 64 ), spot - ( 0, 0, 256 ), false, owner );
    // Delivery shortcut: the helicopter flight is skipped; the crate is built
    // and set up by _airdrop's own functions exactly as physicsWaiter does.
    crate = owner maps\mp\killstreaks\_airdrop::createAirDropCrate( owner, "airdrop", "uav", trace["position"] + ( 0, 0, 16 ) );
    crate.angles = ( 0, 0, 0 );
    crate show();
    crate thread [[ level.crateFuncs["airdrop"]["uav"] ]]( "airdrop" );
    wait 1;
    report( "crate " + crate getEntityNumber() + " (uav) of " + owner.name + " (" + owner.team + ") at " + crate.origin );
    taker grant( perk );
    taker standAt( crate.origin, 50, owner.angles[1] );
    start = getTime();
    taker holdUse( 5 );
    captured = false;
    for ( i = 0; i < 120; i++ )
    {
        wait 0.05;
        if ( !isDefined( crate ) || isDefined( crate.codolTrapper ) )
        {
            captured = true;
            break;
        }
    }
    taker codol_botPressButtons( 0, 0 );
    if ( !captured )
    {
        report( taker.name + " did not capture the crate in " + ( getTime() - start ) + "ms" );
        return;
    }
    if ( !isDefined( crate ) )
    {
        report( taker.name + " captured the crate in " + ( getTime() - start ) + "ms; the crate is gone" );
        return;
    }
    report( taker.name + " captured the crate in " + ( getTime() - start ) + "ms; it stays, trapped by " + crate.codolTrapper.name );
    if ( openerNumber == "none" )
        return;
    opener = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( openerNumber ) );
    if ( !isDefined( opener ) || !opener waitAlive() )
        return;
    opener idle();
    taker setOrigin( taker.origin + anglesToForward( ( 0, owner.angles[1], 0 ) ) * 600 );
    wait 1;
    opener standAt( crate.origin, 50, owner.angles[1] + 180 );
    healthBefore = opener.health;
    opener thread reportDamage();
    opener holdUse( 5 );
    for ( i = 0; i < 120 && isDefined( crate ); i++ )
        wait 0.05;
    wait 0.2;
    report( opener.name + " (" + opener.team + ") opened the trapped crate: crate gone " + !isDefined( crate ) + ", alive " +
        isAlive( opener ) + ", health " + healthBefore + " -> " + opener.health );
}
