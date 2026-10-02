// CoDOL multiplayer Scavenger test hook. Idle unless developer is set and the
// dvar names a request. Kills go through the gametype's own damage callback,
// so the bag is dropped by _damage.gsc / _weapons.gsc exactly as in a match;
// pickups are real touches (the test only moves players onto the bag).
//
//   set scr_codol_scavtest "run <killer> <victim> <outsider> [perk]"
//       killer gets [perk] (default specialty_scavenger; "specialty_scavenger,
//       specialty_extraammo" for Pro) and kills victim. The outsider, whose
//       scavenger is removed, stands on the bag first and must not take it;
//       then the killer, with its stock emptied, stands on it. Ammo before and
//       after is reported. [perk] "class:<name>" gives the killer a class whose
//       perk1 is <name> through _class::loadoutAllPerks instead.
//   set scr_codol_scavtest "census <seconds>"
//       while the bots fight, reports how long each scavenger bag lives.
#include maps\mp\_utility;
#include common_scripts\utility;

init()
{
    if ( isDefined( level.codolScavTestStarted ) )
        return;
    level.codolScavTestStarted = true;
    setDvar( "scr_codol_scavtest", "" );
    level thread watchDvar();
}

watchDvar()
{
    if ( getDvarInt( "developer" ) <= 0 )
        return;
    level endon( "game_ended" );
    for ( ;; )
    {
        wait 0.5;
        request = getDvar( "scr_codol_scavtest" );
        if ( request == "" )
            continue;
        setDvar( "scr_codol_scavtest", "" );
        tokens = strTok( request, " " );
        if ( tokens[0] == "run" && tokens.size > 3 )
        {
            killer = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( tokens[1] ) );
            victim = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( tokens[2] ) );
            outsider = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( tokens[3] ) );
            perk = "specialty_scavenger";
            if ( tokens.size > 4 )
                perk = tokens[4];
            if ( isDefined( killer ) && isDefined( victim ) && isDefined( outsider ) )
                level thread testRun( killer, victim, outsider, perk );
            else
                report( "bad clients in " + request );
        }
        else if ( tokens[0] == "census" && tokens.size > 1 )
            level thread census( float( tokens[1] ) );
        else
            report( "bad request " + request );
    }
}

report( message )
{
    codolPrintLn( "CoDOL scavtest: " + message );
}

hold()
{
    self enableCombatBot( false );
    self freezeControls( false );
    self.health = 100000;
    self.maxhealth = 100000;
}

ammoLine()
{
    line = "";
    foreach ( weapon in self getWeaponsListPrimaries() )
        line += weapon + " clip " + self getWeaponAmmoClip( weapon ) + " stock " + self getWeaponAmmoStock( weapon ) + "; ";
    foreach ( weapon in self getWeaponsListOffhands() )
        line += weapon + " " + self getAmmoCount( weapon ) + "; ";
    return line;
}

findBag( near )
{
    best = undefined;
    foreach ( ent in getEntArray() )
    {
        if ( !isDefined( ent.classname ) || !isSubStr( ent.classname, "scavenger" ) )
            continue;
        if ( !isDefined( best ) || distance( ent.origin, near ) < distance( best.origin, near ) )
            best = ent;
    }
    return best;
}

testRun( killer, victim, outsider, perk )
{
    for ( tries = 0; ( !isAlive( killer ) || !isAlive( victim ) || !isAlive( outsider ) ) && tries < 40; tries++ )
        wait 0.5;
    killer hold();
    outsider hold();
    if ( isSubStr( perk, "class:" ) )
    {
        // The loadout path a profile class takes: perk1 names this perk.
        selected = getSubStr( perk, 6 );
        killer _clearPerks();
        killer maps\mp\gametypes\_class::loadoutAllPerks( "specialty_null", selected, "specialty_null", "specialty_null" );
    }
    else
    {
        foreach ( name in strTok( perk, "," ) )
            killer maps\mp\perks\_perks::givePerk( name );
    }
    if ( outsider _hasPerk( "specialty_scavenger" ) )
        outsider _unsetPerk( "specialty_scavenger" );
    report( killer.name + " _hasPerk scavenger " + killer _hasPerk( "specialty_scavenger" ) + " hasPerk " + killer hasPerk( "specialty_scavenger" ) +
        " modScavenger " + killer HasAbilityModifier( "modScavenger" ) + " modExtraAmmo " + killer HasAbilityModifier( "modExtraAmmo" ) );
    report( outsider.name + " hasPerk scavenger " + outsider hasPerk( "specialty_scavenger" ) + " modScavenger " + outsider HasAbilityModifier( "modScavenger" ) );

    killer thread watchPickupNotify();
    deathSpot = victim.origin;
    weapon = killer getCurrentWeapon();
    victim [[level.callbackPlayerDamage]]( killer, killer, 100000, 0, "MOD_RIFLE_BULLET", weapon, victim.origin, ( 0, 0, 1 ), "torso_upper", 0 );
    wait 1.5;
    bag = findBag( deathSpot );
    if ( !isDefined( bag ) )
    {
        report( "FAIL no bag near " + deathSpot + " after " + victim.name + " died (alive " + isAlive( victim ) + ")" );
        return;
    }
    near = 0;
    foreach ( ent in getEntArray() )
        if ( isDefined( ent.classname ) && isSubStr( ent.classname, "scavenger" ) && distance( ent.origin, bag.origin ) < 64 )
            near++;
    report( "bag " + bag.classname + " at " + bag.origin + " (death at " + deathSpot + "); bags within 64: " + near );
    spot = bag.origin;

    outsider setOrigin( spot );
    wait 1.0;
    report( "outsider " + outsider.name + " stood on bag: bag still defined " + isDefined( bag ) );
    outsider setOrigin( spot + ( 0, 0, 2000 ) );
    wait 0.2;
    outsider suicide();
    if ( !isDefined( bag ) )
        return;

    foreach ( w in killer getWeaponsListPrimaries() )
        killer setWeaponAmmoStock( w, 0 );
    report( "before " + killer ammoLine() );
    killer setOrigin( spot );
    wait 1.0;
    report( "killer " + killer.name + " stood on bag: bag still defined " + isDefined( bag ) );
    report( "after " + killer ammoLine() );
}

watchPickupNotify()
{
    self endon( "disconnect" );
    self waittill( "scavenger_pickup" );
    report( self.name + " got scavenger_pickup" );
}

census( seconds )
{
    born = [];
    where = [];
    lifetimes = [];
    endTime = getTime() + int( seconds * 1000 );
    while ( getTime() < endTime )
    {
        seen = [];
        foreach ( ent in getEntArray() )
        {
            if ( !isDefined( ent.classname ) || !isSubStr( ent.classname, "scavenger" ) )
                continue;
            key = "" + ent getEntityNumber();
            seen[key] = true;
            if ( !isDefined( born[key] ) || where[key] != ent.origin && distance( where[key], ent.origin ) > 200 )
            {
                born[key] = getTime();
                where[key] = ent.origin;
            }
        }
        gone = [];
        foreach ( key, t in born )
        {
            if ( isDefined( seen[key] ) )
                continue;
            lifetimes[lifetimes.size] = getTime() - t;
            gone[gone.size] = key;
        }
        foreach ( key in gone )
            born[key] = undefined;
        wait 0.25;
    }
    line = "";
    foreach ( ms in lifetimes )
        line += ms + " ";
    report( "census: " + lifetimes.size + " bags gone, " + born.size + " still out; lifetimes ms: " + line );
}
