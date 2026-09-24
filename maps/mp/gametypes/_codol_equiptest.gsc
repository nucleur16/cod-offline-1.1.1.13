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
    self codol_botPressButtons( slotButton( slot ), 0.4 );
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
        where = "(no entity)";
        if ( isDefined( grenade ) )
            where = "" + grenade.origin;
        codolPrintLn( "CoDOL equiptest: grenade_fire " + weapon + " by " + self.name + " at " + where );
    }
}

// set scr_codol_equiptest "table <name>": every row of a mounted string table.
dumpTable( table )
{
    for ( row = 0; row < 256; row++ )
    {
        first = tableLookupByRow( table, row, 0 );
        if ( !isDefined( first ) || first == "" )
            break;
        line = "";
        for ( col = 0; col < 16; col++ )
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
