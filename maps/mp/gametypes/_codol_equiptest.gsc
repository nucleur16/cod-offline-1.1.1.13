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
//   set scr_codol_equiptest "state <clientnum>"
//       prints a client's alive flag, session state, health and team.
//   set scr_codol_equiptest "weapflags <clientnum>" | "weapflags name <weapon>"
//       prints the C1 attachment flags (thermal, gps, idlock, hpbt) and
//       script mods of each held weapon, or of one weapon name.
//   set scr_codol_equiptest "gpsdrop <owner> <picker> <weapon>"
//       <owner> drops <weapon> through the death-drop path and <picker>
//       picks it up with the use button (multiplayer only).
//   set scr_codol_equiptest "gpstrack <holder> [<owner> <weapon>]"
//       starts a GPS track directly, or reports a holder's track abilities.
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
	// Test hooks (they can set health, freeze players and grant rewards) only run
	// on a developer server; the live server runs developer 0.
	if ( getDvarInt( "developer" ) <= 0 )
		return;
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
        if ( tokens[0] == "perk" && tokens.size > 2 )
        {
            player = playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) )
                player thread keepPerk( tokens[2] );
            else
                {}
            continue;
        }
        if ( tokens[0] == "state" && tokens.size > 1 )
        {
            player = playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) )
            {
                team = "none";
                if ( isDefined( player.team ) )
                    team = player.team;
                {}
            }
            continue;
        }
        // "akimbo <client> <C1 button mask> <seconds> <weapon ...>": arm the
        // weapon (a composed name may contain spaces), hold the buttons and
        // log each hand's magazine every server frame. See akimboTrial.
        if ( tokens[0] == "akimbo" && tokens.size > 4 )
        {
            player = playerByNumber( int( tokens[1] ) );
            weapon = tokens[4];
            for ( i = 5; i < tokens.size; i++ )
                weapon = weapon + " " + tokens[i];
            if ( isDefined( player ) )
                player thread akimboTrial( weapon, int( tokens[2] ), float( tokens[3] ) );
            else
                {}
            continue;
        }
        // "akimbosprint <client> <delay> <seconds>": empty the held akimbo
        // weapon's left gun, let its reload run <delay> seconds, then sprint
        // forward for <seconds>, logging both magazines. See akimboSprintTrial.
        if ( tokens[0] == "akimbosprint" && tokens.size > 3 )
        {
            player = playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) )
                player thread akimboSprintTrial( float( tokens[2] ), float( tokens[3] ) );
            else
                {}
            continue;
        }
        // "weapflags <client>": each held weapon with the C1 script-only
        // definition flags and script mods.
        // "weapflags name <weapon ...>": the same for one weapon name.
        if ( tokens[0] == "weapflags" && tokens.size > 2 && tokens[1] == "name" )
        {
            weapon = tokens[2];
            for ( i = 3; i < tokens.size; i++ )
                weapon = weapon + " " + tokens[i];
            weaponFlagsLine( "name", weapon );
            continue;
        }
        if ( tokens[0] == "weapflags" && tokens.size > 1 )
        {
            player = playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) )
                player weaponFlagsReport();
            else
                {}
            continue;
        }
        // "gpstrack <holder> <owner> <weapon ...>": give <holder> the weapon
        // (a composed name may contain spaces) and start the GPS track that a
        // pickup of <owner>'s dropped weapon starts; "gpstrack <holder>" alone
        // reports the holder's two compass abilities.
        if ( tokens[0] == "gpstrack" && tokens.size > 1 )
        {
            holder = playerByNumber( int( tokens[1] ) );
            if ( !isDefined( holder ) )
            {
                {}
                continue;
            }
            if ( tokens.size > 3 && isDefined( level.codolEquipTestGpsTrack ) )
            {
                owner = playerByNumber( int( tokens[2] ) );
                weapon = tokens[3];
                for ( i = 4; i < tokens.size; i++ )
                    weapon = weapon + " " + tokens[i];
                if ( isDefined( owner ) )
                {
                    holder giveWeapon( weapon );
                    holder switchToWeapon( weapon );
                    holder thread [[ level.codolEquipTestGpsTrack ]]( owner, weapon );
                }
            }
            wait 0.3;
            {}
            continue;
        }
        // "gpsdrop <owner> <picker> <weapon ...>": <owner> drops <weapon>
        // through the death-drop path, then <picker> stands on it and holds
        // use, so the real pickup notify drives the GPS track.
        if ( tokens[0] == "gpsdrop" && tokens.size > 3 && isDefined( level.codolEquipTestDropWeapon ) )
        {
            owner = playerByNumber( int( tokens[1] ) );
            picker = playerByNumber( int( tokens[2] ) );
            weapon = tokens[3];
            for ( i = 4; i < tokens.size; i++ )
                weapon = weapon + " " + tokens[i];
            if ( isDefined( owner ) && isDefined( picker ) )
                level thread gpsDropTrial( owner, picker, weapon );
            continue;
        }
        if ( tokens[0] == "turretwatch" && tokens.size > 1 )
        {
            level thread turretWatch( float( tokens[1] ) );
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
                {}
            continue;
        }
        if ( tokens.size < 3 )
            continue;
        player = playerByNumber( int( tokens[0] ) );
        if ( !isDefined( player ) )
        {
            {}
            continue;
        }
        player thread throwEquipment( int( tokens[1] ), tokens[2] );
    }
}

// "perk <client> <perkName>": the native C1 setPerk on that client now and
// after each later spawn (the loadout clears perks on every spawn), so an
// effect of that perk can be observed across a bot's lives.
keepPerk( perkName )
{
    self endon( "disconnect" );
    self notify( "codol_equiptest_keepperk_" + perkName );
    self endon( "codol_equiptest_keepperk_" + perkName );
    for ( ;; )
    {
        if ( isAlive( self ) )
        {
            self setPerk( perkName );
            {}
        }
        self waittill( "spawned_player" );
        wait 0.1;
    }
}

gpsDropTrial( owner, picker, weapon )
{
    owner giveWeapon( weapon );
    owner giveMaxAmmo( weapon );
    owner switchToWeapon( weapon );
    wait 1.0;
    owner.lastDroppableWeapon = weapon;
    owner.droppedDeathWeapon = undefined;
    owner [[ level.codolEquipTestDropWeapon ]]( undefined );
    wait 0.5;
    items = getEntArray( "weapon_" + weapon, "classname" );
    {}
    if ( !items.size )
        return;
    for ( i = 0; i < 12; i++ )
    {
        if ( !isDefined( items[0] ) || !isAlive( picker ) )
            break;
        picker setOrigin( items[0].origin );
        picker codol_botPressButtons( 40, 0.5 );   // 0x08 activate | 0x20 use-reload
        wait 0.25;
    }
    {}
}

weaponFlagsReport()
{
    foreach ( weapon in self GetWeaponsListAll() )
        weaponFlagsLine( self.name, weapon );
}

weaponFlagsLine( label, weapon )
{
    {}
}

// "turretwatch <seconds>": twice a second, every live turret in level.turrets
// and the target it is tracking, with that target's modNoSentryGuns bit.
turretWatch( seconds )
{
    level endon( "game_ended" );
    for ( elapsed = 0; elapsed < seconds; elapsed += 0.5 )
    {
        if ( isDefined( level.turrets ) )
        {
            foreach ( turret in level.turrets )
            {
                if ( !isDefined( turret ) )
                    continue;
                target = turret getTurretTarget( false );
                if ( !isDefined( target ) )
                    continue;
                report = "CoDOL equiptest: turret " + turret getEntityNumber() + " targets ent " + target getEntityNumber();
                if ( isPlayer( target ) )
                    report += " " + target.name + " (" + target.team + ") noSentry " + target hasAbilityModifier( "modNoSentryGuns" );
                {}
            }
        }
        wait 0.5;
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
        {}
        return;
    }
    if ( getWeaponIndexFromName( weapon ) <= 0 )
    {
        {}
        return;
    }
    slots = [];
    for ( i = 0; i < 3; i++ )
        slots[i] = self getOffhandEquipment( i );
    slots[slot] = weapon;
    self giveWeapon( weapon );
    self giveStartAmmo( weapon );
    self setOffhandEquipment( slots[0], slots[1], slots[2] );
    {}
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
    {}
    return false;
}

logGrenadeFire()
{
    level endon( "game_ended" );
    for ( ;; )
    {
        level waittill( "connected", player );
        player thread logPlayerGrenadeFire();
        player thread logPlayerFlashbang();
    }
}

// The native flash blast (C1 522580) notifies (origin, distance, angle,
// attacker, team); log it so a thrown flash can be checked end to end.
logPlayerFlashbang()
{
    self endon( "disconnect" );
    for ( ;; )
    {
        self waittill( "flashbang", origin, amountDistance, amountAngle, attacker, team );
        who = "undefined";
        if ( isDefined( attacker ) && isPlayer( attacker ) )
            who = attacker.name;
        {}
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
        {}
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
        {}
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
            {}
            return;
        }
        bot.codolEquipTestBot = true;
        // Honoured by the zombies damage callback only; see _zombiemode.gsc.
        bot.codolTestInvulnerable = true;
        {}
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
        {}
        return;
    }
    if ( tokens[0] == "down" )
    {
        if ( !isDefined( level.codolEquipTestDown ) )
        {
            {}
            return;
        }
        {}
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
            {}
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
        {}
        return;
    }
    {}
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

// "akimbosprint" trial: run after an "akimbo" trial has armed the weapon.
// Fires the left gun (hand 1, C1 trigger 0x1) one round per press until its
// magazine is empty, waits <delay> seconds into the reload that starts, then
// holds sprint (0x2) with the stick forward for <seconds>. C1's sprint check
// (0x41d3e0 -> 0x41d070) puts every hand into sprint-in, so the left reload is
// dropped: the left magazine must stay empty through the sprint and refill
// only after it, by a fresh reload.
akimboSprintTrial( delay, seconds )
{
    self endon( "disconnect" );
    if ( !isAlive( self ) || !self isDualWielding() )
    {
        {}
        return;
    }
    weapon = self getCurrentWeapon();
    self giveMaxAmmo( weapon );
    for ( shots = 0; self getCurrentWeaponClipAmmo( "left" ) > 0 && shots < 60; shots++ )
    {
        self codol_botPressButtons( 1, 0.1 );
        wait 0.25;
    }
    {}
    // Keep the bot's commands running (stance-held bit only) through the delay.
    self codol_botPressButtons( 4096, delay );
    wait delay;
    {}
    self codol_botPressButtons( 2, seconds, 1 );
    start = getTime();
    lastLeft = -1;
    lastRight = -1;
    while ( getTime() - start < seconds * 1000 + 4000 )
    {
        if ( getTime() - start >= seconds * 1000 && getTime() - start < seconds * 1000 + 100 )
            self codol_botPressButtons( 4096, 4 );
        right = self getCurrentWeaponClipAmmo( "right" );
        left = self getCurrentWeaponClipAmmo( "left" );
        if ( right != lastRight || left != lastLeft )
            {}
        lastRight = right;
        lastLeft = left;
        wait 0.05;
    }
    {}
}

// "akimbo" trial: the held weapon's per-hand magazines ("right" is hand 0, the
// retained machine; "left" is hand 1) sampled every frame while a test client
// holds <mask>. C1 fires an akimbo hand 0 on 0x100000 and hand 1 on 0x1
// (0x41b1f0), and never aims down the sights while dual wielding (0x418140),
// so a trial shows which hand each trigger fires, whether a semi-automatic hand
// pins after one round, and whether ADS was entered.
akimboTrial( weapon, mask, seconds )
{
    self endon( "disconnect" );
    for ( tries = 0; !isAlive( self ) && tries < 30; tries++ )
        wait 0.5;
    if ( !isAlive( self ) )
        return;
    if ( weapon == "held" )
    {
        // Whichever owned weapon dual wields (a loadout akimbo secondary).
        foreach ( candidate in self getWeaponsListPrimaries() )
        {
            if ( self isDualWielding() )
                break;
            {}
            self switchToWeapon( candidate );
            for ( tries = 0; self getCurrentWeapon() != candidate && tries < 40; tries++ )
                wait 0.1;
            wait 1.5;
        }
        weapon = self getCurrentWeapon();
        self giveMaxAmmo( weapon );
    }
    // A test client that is not a combat bot only runs commands while a
    // scripted hold is active; hold the stance-held bit (0x1000, no weapon
    // or movement effect) so the switch below is actually executed.
    self codol_botPressButtons( 4096, 4 );
    if ( self getCurrentWeapon() != weapon )
    {
        self giveWeapon( weapon );
        self giveMaxAmmo( weapon );
        // setSpawnWeapon raises it directly: a test client's own command names
        // no weapon until the server has published one for it.
        self setSpawnWeapon( weapon );
        self switchToWeapon( weapon );
        {}
        for ( tries = 0; self getCurrentWeapon() != weapon && tries < 40; tries++ )
            wait 0.1;
        wait 1.5;
    }
    {}
    self codol_botPressButtons( mask, seconds );
    start = getTime();
    lastRight = -1;
    lastLeft = -1;
    lastAds = -1;
    while ( getTime() - start < seconds * 1000 + 500 )
    {
        right = self getCurrentWeaponClipAmmo( "right" );
        left = self getCurrentWeaponClipAmmo( "left" );
        ads = self playerADS();
        if ( right != lastRight || left != lastLeft || ads != lastAds )
            {}
        lastRight = right;
        lastLeft = left;
        lastAds = ads;
        wait 0.05;
    }
    {}
}
