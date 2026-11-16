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
//   set scr_codol_scavtest "track <client|all> <seconds> [walk|drain|none]"
//       client keeps a Scavenger Pro class (perk1 specialty_extraammo through
//       _class::loadoutAllPerks) on every spawn while the bots fight. Every
//       kill it makes is logged: whether a bag dropped, whether the killer
//       could see it, how many dropped items were out, how long the bag
//       lived and why it went (picked up, by whom, or freed without a
//       pickup, i.e. reclaimed by the dropped-item ring), and whether it was
//       still there when the killer could have reached it (distance / 190
//       units per second). "walk" (the default) then moves the killer onto
//       the bag at that moment and reports the pickup with ammo before and
//       after; "drain" empties his stock first; "none" never moves him.
//       "all" does this for every player at once, "first<N>" for the players
//       numbered below N while the others keep their own classes. Every bag
//       in the level is
//       also counted: how many were picked up and how many were freed
//       without a pickup, and how soon.
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
        else if ( tokens[0] == "track" && tokens.size > 2 )
        {
            level.scavTrackFirst = 64;
            if ( getSubStr( tokens[1], 0, 5 ) == "first" )
                level.scavTrackFirst = int( getSubStr( tokens[1], 5 ) );
            if ( tokens[1] == "all" || getSubStr( tokens[1], 0, 5 ) == "first" )
                player = level;
            else
                player = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( tokens[1] ) );
            mode = "walk";
            if ( tokens.size > 3 )
                mode = tokens[3];
            if ( isDefined( player ) )
                level thread scavTrackRun( player, float( tokens[2] ), mode );
            else
                report( "bad client in " + request );
        }
        else
            report( "bad request " + request );
    }
}

report( message )
{
    {}
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

// ---------------------------------------------------------------------------
// track: natural kills by one Scavenger Pro player.

scavRoomLine()
{
    // Whether any held weapon could take ammo, approximated in script from
    // the quantities C1 0x40c5b0 compares (stock below max, or an offhand's
    // clip below its size).
    foreach ( weapon in self getWeaponsListPrimaries() )
        if ( self getWeaponAmmoStock( weapon ) < weaponMaxAmmo( weapon ) )
            return "room";
    foreach ( weapon in self getWeaponsListOffhands() )
        if ( self getWeaponAmmoClip( weapon ) < weaponClipSize( weapon ) )
            return "room";
    return "full";
}

scavDroppedItemCount()
{
    count = 0;
    foreach ( ent in getEntArray() )
        if ( isDefined( ent.classname ) && getSubStr( ent.classname, 0, 7 ) == "weapon_" )
            count++;
    return count;
}

scavTrackClass()
{
    self endon( "disconnect" );
    level endon( "codol_scavtrack_end" );
    for ( ;; )
    {
        if ( isAlive( self ) )
        {
            self _clearPerks();
            self maps\mp\gametypes\_class::loadoutAllPerks( "specialty_null", "specialty_extraammo", "specialty_null", "specialty_null" );
        }
        self waittill( "spawned_player" );
        wait 0.05;
    }
}

scavTrackBags()
{
    level endon( "codol_scavtrack_end" );
    level.scavTrackBags = [];
    for ( ;; )
    {
        seen = [];
        foreach ( ent in getEntArray() )
        {
            if ( !isDefined( ent.classname ) || !isSubStr( ent.classname, "scavenger" ) )
                continue;
            key = "" + ent getEntityNumber();
            seen[key] = true;
            if ( isDefined( level.scavTrackBags[key] ) && level.scavTrackBags[key].ent == ent )
                continue;
            entry = spawnStruct();
            entry.ent = ent;
            entry.born = getTime();
            entry.origin = ent.origin;
            entry.claimed = false;
            level.scavTrackBags[key] = entry;
            level.scavTrack.allBags++;
            ent thread scavTrackPickup( entry );
        }
        foreach ( key, entry in level.scavTrackBags )
        {
            if ( isDefined( seen[key] ) )
                continue;
            life = getTime() - entry.born;
            if ( isDefined( entry.pickedBy ) )
                level.scavTrack.allPicked++;
            else
            {
                level.scavTrack.allFreed++;
                if ( life <= 100 )
                    level.scavTrack.allFreedUnder100ms++;
                else if ( life < 1000 )
                    level.scavTrack.allFreedUnder1s++;
                else if ( life < 10000 )
                    level.scavTrack.allFreedUnder10s++;
            }
            level.scavTrackBags[key] = undefined;
        }
        wait 0.05;
    }
}

scavTrackVictims( killer )
{
    level endon( "codol_scavtrack_end" );
    watched = [];
    for ( ;; )
    {
        foreach ( player in level.players )
        {
            if ( player == killer )
                continue;
            key = "" + player getEntityNumber();
            if ( isDefined( watched[key] ) && watched[key] == player )
                continue;
            watched[key] = player;
            player thread scavTrackVictim( killer );
            if ( killer == level && player getEntityNumber() < level.scavTrackFirst )
                player thread scavTrackClass();
        }
        wait 0.5;
    }
}

scavTrackVictim( killer )
{
    self endon( "disconnect" );
    level endon( "codol_scavtrack_end" );
    for ( ;; )
    {
        self waittill( "death", attacker );
        if ( !isDefined( attacker ) || !isPlayer( attacker ) || attacker == self )
            continue;
        if ( killer == level && attacker getEntityNumber() >= level.scavTrackFirst )
            continue;
        if ( killer == level || attacker == killer )
            level thread scavTrackKill( attacker, self, self.origin, getTime() );
    }
}

scavTrackKill( killer, victim, deathSpot, deathTime )
{
    level.scavTrack.kills++;
    n = level.scavTrack.kills;
    visible = killer HasAbilityModifier( "modScavenger" );
    if ( !visible )
        level.scavTrack.noAbility++;
    items = scavDroppedItemCount();
    killerSpot = killer.origin;
    killerName = killer.name;
    victimName = victim.name;
    wait 0.1;
    bag = undefined;
    why = "";
    if ( isDefined( level.scavTrackDropHook ) )
    {
        // _weapons.gsc dropScavengerForDeath announced its drop.
        if ( !isDefined( victim ) || !isDefined( victim.scavTestDropTime ) || victim.scavTestDropTime < deathTime - 50 )
            why = "dropScavengerForDeath dropped nothing";
        else if ( !isDefined( victim.scavTestDrop ) )
            why = "dropScavengerBag returned undefined";
        else
            bag = victim.scavTestDrop;
    }
    else
    {
        best = 300;
        foreach ( key, entry in level.scavTrackBags )
        {
            if ( entry.claimed || entry.born < deathTime - 50 || !isDefined( entry.ent ) )
                continue;
            d = distance( entry.origin, deathSpot );
            if ( d < best )
            {
                best = d;
                bag = entry;
            }
        }
        why = "no new bag seen near the body";
    }
    if ( !isDefined( bag ) )
    {
        level.scavTrack.noBag++;
        report( "kill " + n + " " + killerName + " -> " + victimName + " at " + deathTime + ": NO BAG, " + why + " (grace " + level.inGracePeriod + ", items " + items + ")" );
        return;
    }
    bag.claimed = true;
    level.scavTrack.bags++;
    ent = bag.ent;
    if ( !isDefined( ent ) )
        level.scavTrack.goneIn100ms++;
    wait 1.0;
    if ( isDefined( ent ) )
        bag.rest = ent.origin;
    else
        bag.rest = bag.origin;
    eta = distance( killerSpot, bag.rest ) / 190.0;
    if ( eta > 1.0 )
        wait ( eta - 1.0 );
    arrived = isDefined( ent ) && !isDefined( bag.pickedBy );
    if ( arrived )
        level.scavTrack.thereOnArrival++;
    result = "";
    if ( arrived && level.scavTrack.mode != "none" && isAlive( killer ) )
    {
        if ( level.scavTrack.mode == "drain" )
            foreach ( w in killer getWeaponsListPrimaries() )
                killer setWeaponAmmoStock( w, 0 );
        room = killer scavRoomLine();
        before = killer ammoLine();
        killer setOrigin( ent.origin );
        for ( i = 0; i < 6 && isDefined( ent ) && !isDefined( bag.pickedBy ); i++ )
            wait 0.05;
        level.scavTrack.walks++;
        if ( room == "room" )
            level.scavTrack.walksWithRoom++;
        if ( isDefined( bag.pickedBy ) && bag.pickedBy == killer )
        {
            level.scavTrack.walkPickups++;
            if ( room == "room" )
                level.scavTrack.walkPickupsWithRoom++;
            result = "walked on (" + room + "): PICKED UP; before " + before + "after " + killer ammoLine();
        }
        else if ( isDefined( ent ) )
            result = "walked on (" + room + "): bag left in place; ammo " + before;
        else
            result = "walked on (" + room + "): bag gone, not by killer";
    }
    else if ( !arrived )
        result = "bag gone before arrival";
    else
        result = "killer dead or not moved";
    while ( isDefined( ent ) && !isDefined( bag.pickedBy ) )
        wait 0.05;
    life = getTime() - bag.born;
    if ( isDefined( bag.pickedBy ) )
    {
        cause = "picked up by " + bag.pickedBy.name;
        level.scavTrack.pickedUp++;
    }
    else
    {
        cause = "freed without pickup";
        level.scavTrack.freed++;
        if ( life < 1000 )
            level.scavTrack.freedUnder1s++;
    }
    report( "kill " + n + " " + killerName + " -> " + victimName + ": bag visible " + visible + ", items " + items +
        ", eta " + int( eta * 1000 ) + " ms, there on arrival " + arrived + ", life " + life + " ms, " + cause + "; " + result );
}

// Optional: _weapons.gsc dropScavengerForDeath may call this right after
// dropScavengerBag, so a bag that is gone before the census polls (taken in
// the next frame, or reclaimed by the ring) is still attributed.
scavTrackRegisterDrop( victim, bag )
{
    if ( !isDefined( level.scavTrack ) || !isDefined( level.scavTrackBags ) )
        return;
    level.scavTrackDropHook = true;
    victim.scavTestDropTime = getTime();
    victim.scavTestDrop = undefined;
    if ( !isDefined( bag ) )
        return;
    entry = spawnStruct();
    entry.ent = bag;
    entry.born = getTime();
    entry.origin = bag.origin;
    entry.claimed = false;
    victim.scavTestDrop = entry;
    level.scavTrackBags["" + bag getEntityNumber()] = entry;
    level.scavTrack.allBags++;
    bag thread scavTrackPickup( entry );
}

scavTrackPickup( bag )
{
    self waittill( "scavenger", who );
    bag.pickedBy = who;
}

scavTrackRun( player, seconds, mode )
{
    if ( isDefined( level.scavTrack ) )
        level notify( "codol_scavtrack_end" );
    wait 0.1;
    level.scavTrack = spawnStruct();
    level.scavTrack.mode = mode;
    level.scavTrack.kills = 0;
    level.scavTrack.noAbility = 0;
    level.scavTrack.noBag = 0;
    level.scavTrack.bags = 0;
    level.scavTrack.thereOnArrival = 0;
    level.scavTrack.walks = 0;
    level.scavTrack.walksWithRoom = 0;
    level.scavTrack.walkPickups = 0;
    level.scavTrack.walkPickupsWithRoom = 0;
    level.scavTrack.pickedUp = 0;
    level.scavTrack.freed = 0;
    level.scavTrack.freedUnder1s = 0;
    level.scavTrack.allBags = 0;
    level.scavTrack.goneIn100ms = 0;
    level.scavTrack.allPicked = 0;
    level.scavTrack.allFreed = 0;
    level.scavTrack.allFreedUnder100ms = 0;
    level.scavTrack.allFreedUnder1s = 0;
    level.scavTrack.allFreedUnder10s = 0;
    name = "everyone";
    if ( player != level )
        name = player.name;
    report( "track " + name + " for " + seconds + " s, mode " + mode + ", g_maxDroppedWeapons " + getDvarInt( "g_maxDroppedWeapons" ) );
    level thread scavTrackBags();
    level thread scavTrackVictims( player );
    if ( player != level )
        player thread scavTrackClass();
    wait seconds;
    level notify( "codol_scavtrack_end" );
    wait 3;
    t = level.scavTrack;
    report( "track summary: kills " + t.kills + ", killer lacked modScavenger " + t.noAbility + ", no bag " + t.noBag + ", bags " + t.bags +
        ", gone within 100 ms " + t.goneIn100ms + ", there on arrival " + t.thereOnArrival + ", walked " + t.walks + " (with room " + t.walksWithRoom + "), picked up on walk " + t.walkPickups +
        " (with room " + t.walkPickupsWithRoom + "), bags picked up " + t.pickedUp + ", freed without pickup " + t.freed + " (under 1 s " + t.freedUnder1s + ")" );
    report( "track all bags: seen " + t.allBags + ", picked up " + t.allPicked + ", freed without pickup " + t.allFreed + " (within 100 ms " + t.allFreedUnder100ms +
        ", under 1 s " + t.allFreedUnder1s + ", under 10 s " + t.allFreedUnder10s + ")" );
}
