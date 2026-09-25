// CoDOL equipment test hooks, shared by multiplayer and zombies.
//
// No C1 client is needed: a test client is made to press the real offhand
// button through codol_botPressButtons, so the engine's own weapon code does
// the throw and raises the real "grenade_fire". Nothing here simulates an
// effect. Every hook is idle unless its dvar is set.
//
//   set scr_codol_equiptest "<clientnum> <slot> <weapon>"
//       slot 0 lethal (frag button), 1 tactical, 2 equipment. Arms <weapon>
//       in that slot with the other two kept, then holds the slot's button.
//   set scr_codol_equiptest "addbots <n>"
//       adds n test clients (zombies only; multiplayer has scr_codol_bots).
//   set scr_codol_equiptest "down <clientnum>"
//       puts a client into last stand through level.codolEquipTestDown
//       (zombies sets it; idle elsewhere).
//   set scr_codol_equiptest "place <clientnum> <anchor clientnum> <dist>"
//       moves a client <dist> units in front of the anchor client, on the
//       ground (a target for thrown equipment).
//   set scr_codol_equiptest "warp <clientnum> <x> <y> <z>"
//       moves a client to a position.
//   set scr_codol_equiptest "togrenade <clientnum>"
//       moves a client to where the last thrown grenade is (or came to rest).
//   set scr_codol_equiptest "tozombie <clientnum> <dist>"
//       stands a client <dist> units west of the nearest live zombie in
//       level.zombies, facing it (a target for a thrown lethal).
//
// Deliberately free of maps\mp\killstreaks and maps\mp\gametypes\_class so
// the zombies chain can include it without compiling the multiplayer one.

init()
{
    if ( isDefined( level.codolEquipTestStarted ) )
        return;
    level.codolEquipTestStarted = true;
    setDvar( "scr_codol_equiptest", "" );
    level thread watchDvar();
    level thread logGrenadeFire();
}

watchDvar()
{
    level endon( "game_ended" );
    for ( ;; )
    {
        wait 0.5;
        request = getDvar( "scr_codol_equiptest" );
        if ( request == "" )
            continue;
        setDvar( "scr_codol_equiptest", "" );
        tokens = strTok( request, " " );
        if ( tokens[0] == "addbots" && tokens.size > 1 )
        {
            level thread addTestBots( int( tokens[1] ) );
            continue;
        }
        if ( tokens[0] == "table" && tokens.size > 1 )
        {
            dumpTable( tokens[1] );
            continue;
        }
        if ( tokens[0] == "down" || tokens[0] == "place" || tokens[0] == "warp" || tokens[0] == "togrenade" ||
            tokens[0] == "tozombie" )
        {
            positionVerb( tokens );
            continue;
        }
        // "perkpop <client> <verb> ...": zombies perk-pop hooks, reached
        // through a pointer so this shared file names no zombies module.
        if ( tokens[0] == "perkpop" && tokens.size > 2 )
        {
            player = playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) && isDefined( level.codolEquipTestPerkpop ) )
                level thread [[ level.codolEquipTestPerkpop ]]( player, tokens );
            else
                codolPrintLn( "CoDOL equiptest: perkpop needs a zombies match and client " + tokens[1] );
            continue;
        }
        if ( tokens.size < 3 )
            continue;
        player = playerByNumber( int( tokens[0] ) );
        if ( !isDefined( player ) )
        {
            codolPrintLn( "CoDOL equiptest: no client " + tokens[0] );
            continue;
        }
        player thread throwEquipment( int( tokens[1] ), tokens[2] );
    }
}

playerByNumber( clientNum )
{
    foreach ( player in level.players )
    {
        if ( player getEntityNumber() == clientNum )
            return player;
    }
    return undefined;
}

// C1 offhand slot buttons (codol_weapon_state.c CODOL_OFFHAND_BUTTON_*).
slotButton( slot )
{
    if ( slot == 1 )
        return 32768;   // 0x8000 tactical / special grenade
    if ( slot == 2 )
        return 65536;   // 0x10000 equipment
    return 16384;       // 0x4000 lethal / frag
}

throwEquipment( slot, weapon )
{
    self endon( "disconnect" );
    for ( tries = 0; !isAlive( self ) && tries < 30; tries++ )
        wait 0.5;
    if ( !isAlive( self ) )
    {
        codolPrintLn( "CoDOL equiptest: " + self.name + " never came back alive" );
        return;
    }
    if ( getWeaponIndexFromName( weapon ) <= 0 )
    {
        codolPrintLn( "CoDOL equiptest: weapon " + weapon + " is not loaded on this map" );
        return;
    }
    slots = [];
    for ( i = 0; i < 3; i++ )
        slots[i] = self getOffhandEquipment( i );
    slots[slot] = weapon;
    self giveWeapon( weapon );
    self giveStartAmmo( weapon );
    self setOffhandEquipment( slots[0], slots[1], slots[2] );
    codolPrintLn( "CoDOL equiptest: " + self.name + " throws " + weapon + " from slot " + slot +
        " (ammo " + self getAmmoCount( weapon ) + ")" );
    wait 0.5;
    self pressUntilThrown( slotButton( slot ), weapon );
}

// The engine refuses an offhand while the weapon is busy (firing, reloading,
// switching, sprinting), exactly as it would a player's press; keep pressing
// until it throws, as a player would.
pressUntilThrown( button, weapon )
{
    self endon( "disconnect" );
    self endon( "death" );
    for ( attempt = 1; attempt <= 6; attempt++ )
    {
        self.codolLastThrown = undefined;
        self codol_botPressButtons( button, 0.4 );
        wait 1.5;
        if ( isDefined( self.codolLastThrown ) && self.codolLastThrown == weapon )
            return true;
    }
    codolPrintLn( "CoDOL equiptest: " + self.name + " never threw " + weapon );
    return false;
}

logGrenadeFire()
{
    level endon( "game_ended" );
    for ( ;; )
    {
        level waittill( "connected", player );
        player thread logPlayerGrenadeFire();
    }
}

logPlayerGrenadeFire()
{
    self endon( "disconnect" );
    for ( ;; )
    {
        self waittill( "grenade_fire", grenade, weapon );
        self.codolLastThrown = weapon;
        where = "(no entity)";
        if ( isDefined( grenade ) )
            where = "" + grenade.origin;
        if ( isDefined( grenade ) )
            level thread trackLastGrenade( grenade );
        codolPrintLn( "CoDOL equiptest: grenade_fire " + weapon + " by " + self.name + " at " + where );
    }
}

// set scr_codol_equiptest "table <name>": every row (first 24 columns) of a mounted string table.
dumpTable( table )
{
    for ( row = 0; row < 256; row++ )
    {
        first = tableLookupByRow( table, row, 0 );
        if ( !isDefined( first ) || first == "" )
            break;
        line = "";
        for ( col = 0; col < 24; col++ )
            line = line + tableLookupByRow( table, row, col ) + "|";
        codolPrintLn( "CoDOL equiptest: " + table + " row " + row + ": " + line );
    }
}

addTestBots( count )
{
    level endon( "game_ended" );
    for ( i = 0; i < count; i++ )
    {
        bot = addTestClient( "Test " + ( i + 1 ) );
        if ( !isDefined( bot ) )
        {
            codolPrintLn( "CoDOL equiptest: addTestClient failed" );
            return;
        }
        bot.codolEquipTestBot = true;
        // Honoured by the zombies damage callback only; see _zombiemode.gsc.
        bot.codolTestInvulnerable = true;
        codolPrintLn( "CoDOL equiptest: added test client " + bot getEntityNumber() );
        wait 0.5;
    }
}

// The last thrown grenade's position, kept after the engine removes it.
trackLastGrenade( grenade )
{
    level notify( "codol_equiptest_new_grenade" );
    level endon( "codol_equiptest_new_grenade" );
    while ( isDefined( grenade ) )
    {
        level.codolEquipTestGrenadePos = grenade.origin;
        wait 0.05;
    }
}

positionVerb( tokens )
{
    if ( tokens.size < 2 )
        return;
    player = playerByNumber( int( tokens[1] ) );
    if ( !isDefined( player ) || !isAlive( player ) )
    {
        codolPrintLn( "CoDOL equiptest: no live client " + tokens[1] );
        return;
    }
    if ( tokens[0] == "down" )
    {
        if ( !isDefined( level.codolEquipTestDown ) )
        {
            codolPrintLn( "CoDOL equiptest: this gametype has no down hook" );
            return;
        }
        codolPrintLn( "CoDOL equiptest: " + player.name + " goes down at " + player.origin );
        player thread [[ level.codolEquipTestDown ]]();
        return;
    }
    target = undefined;
    if ( tokens[0] == "warp" && tokens.size > 4 )
        target = ( float( tokens[2] ), float( tokens[3] ), float( tokens[4] ) );
    else if ( tokens[0] == "togrenade" )
        target = level.codolEquipTestGrenadePos;
    else if ( tokens[0] == "tozombie" && tokens.size > 2 )
    {
        zombie = nearestZombie( player.origin );
        if ( isDefined( zombie ) )
        {
            target = zombie.origin - ( float( tokens[2] ), 0, 0 );
            player setPlayerAngles( ( 20, 0, 0 ) );
            codolPrintLn( "CoDOL equiptest: nearest zombie ent " + zombie getEntityNumber() + " at " + zombie.origin +
                " health " + zombie.health );
        }
    }
    else if ( tokens[0] == "place" && tokens.size > 3 )
    {
        anchor = playerByNumber( int( tokens[2] ) );
        if ( isDefined( anchor ) )
        {
            forward = anglesToForward( ( 0, anchor.angles[1], 0 ) );
            spot = anchor.origin + ( forward[0] * float( tokens[3] ), forward[1] * float( tokens[3] ), 0 );
            trace = bulletTrace( spot + ( 0, 0, 48 ), spot - ( 0, 0, 256 ), false, player );
            target = trace["position"];
        }
    }
    if ( !isDefined( target ) )
    {
        codolPrintLn( "CoDOL equiptest: " + tokens[0] + " has no target position" );
        return;
    }
    codolPrintLn( "CoDOL equiptest: " + player.name + " moved from " + player.origin + " to " + target );
    player setOrigin( target );
}

nearestZombie( origin )
{
    if ( !isDefined( level.zombies ) )
        return undefined;
    best = undefined;
    bestDsq = 0;
    for ( i = 0; i < level.zombies.size; i++ )
    {
        z = level.zombies[i];
        if ( !isDefined( z ) || !isAlive( z ) )
            continue;
        dsq = distanceSquared( origin, z.origin );
        if ( !isDefined( best ) || dsq < bestDsq )
        {
            best = z;
            bestDsq = dsq;
        }
    }
    return best;
}
