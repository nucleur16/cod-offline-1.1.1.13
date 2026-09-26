// CoDOL combat-training integration. Native AI follows OpenBLOPS T5
// server_mp/sv_bot_mp.cpp semantics; player lifecycle uses the mounted IW4
// gametype callbacks. No scripted damage, origin assignment, or weapon proxy.
init()
{
    if ( isDefined( level.codolCombatBotsStarted ) )
        return;
    level.codolCombatBotsStarted = true;
    if ( getDvar( "scr_codol_bots" ) == "" )
        setDvar( "scr_codol_bots", 0 );
    // Independent of bots: it also drives a human client by number.
    level thread watchKillstreakTestDvar();
    maps\mp\gametypes\_codol_equiptest::init();
    maps\mp\gametypes\_codol_perktest::init();
    graph = "waypoints/" + getDvar( "mapname" ) + "_wp.csv";
    if ( loadBotWaypoints( graph ) <= 0 )
        return;
    level.codolBotSerial = 0;
    level thread maintainBots();
}

// Test hook for rewards bots never earn on their own:
//   set scr_codol_usekillstreak "<clientnum> <streakRef> [place]"
// runs level.killstreakFuncs[streakRef] on that player, exactly as a use
// would. "place" also sends the carry-placement notify a second later, for
// carried rewards (sentry, sam_turret) that wait on +attack. Empty = off.
watchKillstreakTestDvar()
{
    level endon( "game_ended" );
    setDvar( "scr_codol_usekillstreak", "" );
    for ( ;; )
    {
        wait 0.5;
        request = getDvar( "scr_codol_usekillstreak" );
        if ( request == "" )
            continue;
        setDvar( "scr_codol_usekillstreak", "" );
        if ( request == "list" )
        {
            reportKillstreakCoverage();
            continue;
        }
        if ( request == "rows" )
        {
            reportKillstreakRows();
            continue;
        }
        hurtTokens = strTok( request, " " );
        if ( hurtTokens[0] == "hurt" && hurtTokens.size > 1 )
        {
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( hurtTokens[1] ) && isAlive( player ) )
                {
                    codolPrintLn( "CoDOL killstreak test: hurt " + player.name );
                    player thread reportNextSpawn();
                    player doDamage( 1000, player.origin + ( 0, 0, 40 ), level.players[0] );
                    player codol_deathGates();
                }
            continue;
        }
        if ( request == "gates" )
        {
            foreach ( player in level.players )
                player codol_deathGates();
            continue;
        }
        // "perk <client> <perk>": grant a perk row for this life and report
        // the selected modifiers it drives. "assist <client> <victim>...": run
        // the real assist award per victim. "perkdmg <client> <mod> <damage>": report
        // what cac_modified_damage leaves of that damage.
        perkTokens = strTok( request, " " );
        if ( ( perkTokens[0] == "perk" || perkTokens[0] == "assist" || perkTokens[0] == "perkdmg" ) && perkTokens.size > 2 )
        {
            perkPlayer = undefined;
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( perkTokens[1] ) )
                    perkPlayer = player;
            if ( !isDefined( perkPlayer ) || !isAlive( perkPlayer ) )
            {
                codolPrintLn( "CoDOL perk test: client " + perkTokens[1] + " is not alive" );
                continue;
            }
            if ( perkTokens[0] == "perk" )
                perkPlayer thread testPerk( perkTokens[2] );
            else if ( perkTokens[0] == "perkdmg" && perkTokens.size > 3 )
            {
                left = maps\mp\perks\_perks::cac_modified_damage( perkPlayer, level.players[0], int( perkTokens[3] ), perkTokens[2], "frag_grenade_mp", perkPlayer.origin, ( 0, 0, 1 ), "none" );
                codolPrintLn( "CoDOL perk test: " + perkPlayer.name + " takes " + left + " of " + perkTokens[3] + " " + perkTokens[2] );
            }
            else if ( perkTokens[0] == "assist" )
            {
                for ( victimToken = 2; victimToken < perkTokens.size; victimToken++ )
                    foreach ( player in level.players )
                        if ( player getEntityNumber() == int( perkTokens[victimToken] ) )
                            perkPlayer thread maps\mp\gametypes\_gamescore::processAssist( player );
            }
            continue;
        }
        tiTokens = strTok( request, " " );
        if ( tiTokens[0] == "suicide" && tiTokens.size > 1 )
        {
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( tiTokens[1] ) )
                    player thread testSuicide();
            continue;
        }
        if ( tiTokens[0] == "ti" && tiTokens.size > 1 )
        {
            tiPlayer = undefined;
            foreach ( player in level.players )
                if ( player getEntityNumber() == int( tiTokens[1] ) )
                    tiPlayer = player;
            if ( isDefined( tiPlayer ) )
                tiPlayer thread testTacticalInsertion();
            continue;
        }
        tokens = strTok( request, " " );
        if ( tokens.size < 2 )
            continue;
        clientNum = int( tokens[0] );
        streakRef = tokens[1];
        if ( !isDefined( level.killstreakFuncs[streakRef] ) )
        {
            codolPrintLn( "CoDOL killstreak test: no handler for " + streakRef );
            continue;
        }
        target = undefined;
        foreach ( player in level.players )
        {
            if ( player getEntityNumber() == clientNum )
                target = player;
        }
        // A bot is often mid-respawn; give it a moment rather than dropping
        // the request.
        for ( tries = 0; isDefined( target ) && !isAlive( target ) && tries < 30; tries++ )
            wait 0.5;
        if ( !isDefined( target ) || !isAlive( target ) )
        {
            codolPrintLn( "CoDOL killstreak test: client " + clientNum + " is not alive" );
            continue;
        }
        codolPrintLn( "CoDOL killstreak test: " + target.name + " (" + target.team + ") uses " + streakRef );
        // Own it first, as a real use does: several rewards (the helicopter
        // queue, for one) reshuffle the owner's pers["killstreaks"] list and
        // raise a runtime error for a streak that is not in it.
        // Then use it through the real button path, which resolves the owned
        // reward record and passes its lifeId and kID (airdrops validate the
        // kID) instead of calling the handler bare.
        target maps\mp\killstreaks\_killstreaks::giveKillstreak( streakRef );
        target thread maps\mp\killstreaks\_killstreaks::killstreakUsePressed();
        if ( tokens.size > 2 && tokens[2] == "place" )
            target thread placeCarriedKillstreak();
    }
}

// set scr_codol_usekillstreak list: every catalogue row, whether a use
// handler is registered for it, and its activation weapon.
reportKillstreakCoverage()
{
    table = "mp/coKillstreakStatsTable.csv";
    missing = 0;
    for ( row = 0; true; row++ )
    {
        ref = tableLookupByRow( table, row, 1 );
        if ( !isDefined( ref ) || ref == "" )
            break;
        if ( ref == "none" )
            continue;
        handled = isDefined( level.killstreakFuncs[ref] );
        if ( !handled )
            missing++;
        codolPrintLn( "CoDOL killstreak coverage: row " + row + " id " + tableLookupByRow( table, row, 0 ) +
            " " + ref + " weapon " + tableLookupByRow( table, row, 13 ) + " base " + tableLookupByRow( table, row, 22 ) +
            " handler " + handled );
    }
    codolPrintLn( "CoDOL killstreak coverage: " + missing + " catalogue row(s) without a handler" );
}

// set scr_codol_usekillstreak rows: every column of every catalogue row.
reportKillstreakRows()
{
    table = "mp/coKillstreakStatsTable.csv";
    for ( row = 0; true; row++ )
    {
        ref = tableLookupByRow( table, row, 1 );
        if ( !isDefined( ref ) || ref == "" )
            break;
        line = "";
        for ( col = 0; col < 24; col++ )
            line = line + tableLookupByRow( table, row, col ) + "|";
        codolPrintLn( "CoDOL killstreak row " + row + ": " + line );
    }
}

// set scr_codol_usekillstreak "ti <clientnum>": give the tactical insertion
// perk, throw its flare_mp through the real tactical button, then kill the
// player and report whether the respawn used the beacon.
testTacticalInsertion()
{
    self endon( "disconnect" );
    for ( tries = 0; !isAlive( self ) && tries < 30; tries++ )
        wait 0.5;
    if ( !isAlive( self ) )
        return;
    // As _class::codol_giveEquipment does for a class carrying it: the perk,
    // then flare_mp with its start ammo. flare_mp is offhand class 5, so it is
    // equipment (slot 2, button 0x10000), like the betty.
    // NOTE: C1 ships flare_mp with shared ammo cap WEAPON_FRAGGRENADE, so a
    // player already holding a frag gets 0 flares from giveStartAmmo (the cap
    // is full) and the flare leaves the inventory; codol_dumpOwnedAmmo() shows
    // the pools. Open question for C1's own cap routine, 2026-09-24.
    self maps\mp\perks\_perks::givePerk( "specialty_tacticalinsertion" );
    self giveWeapon( "flare_mp" );
    self giveStartAmmo( "flare_mp" );
    if ( !self hasWeapon( "flare_mp" ) )
    {
        codolPrintLn( "CoDOL killstreak test: TI for " + self.name + ": flare_mp was not kept (shared frag ammo cap)" );
        return;
    }
    slots = [];
    for ( i = 0; i < 3; i++ )
        slots[i] = self getOffhandEquipment( i );
    slots[2] = "flare_mp";
    self setOffhandEquipment( slots[0], slots[1], slots[2] );
    codolPrintLn( "CoDOL killstreak test: TI slots for " + self.name + ": " + self getOffhandEquipment( 0 ) + " / " +
        self getOffhandEquipment( 1 ) + " / " + self getOffhandEquipment( 2 ) + ", flare ammo " + self getAmmoCount( "flare_mp" ) );
    wait 1.0;
    self maps\mp\gametypes\_codol_equiptest::pressUntilThrown( 65536, "flare_mp" );
    wait 1.0;
    if ( !isDefined( self.setSpawnPoint ) )
    {
        codolPrintLn( "CoDOL killstreak test: TI for " + self.name + " placed no beacon (TISpawnPosition " + isDefined( self.TISpawnPosition ) + ")" );
        return;
    }
    beacon = self.setSpawnPoint.origin;
    codolPrintLn( "CoDOL killstreak test: TI beacon for " + self.name + " at " + beacon );
    self suicide();
    self waittill( "spawned_player" );
    codolPrintLn( "CoDOL killstreak test: TI respawn for " + self.name + " at " + self.origin + ", " + distance( self.origin, beacon ) + " from the beacon" );
}

testPerk( perk )
{
    self endon( "disconnect" );
    maps\mp\perks\_perks::givePerk( perk );
    // Perk set callbacks run as threads.
    wait 0.1;
    names = strTok( "modFinalStandTime modPistolDeath modKillstreakAssists modDeathstreakShift modObjectiveUseTimeScale", " " );
    report = "CoDOL perk test: " + self.name + " +" + perk;
    foreach ( name in names )
    {
        value = self getSelectedPerkModifierValue( name );
        if ( isDefined( value ) )
            report += " " + name + "=" + value;
    }
    report += " rowDeathstreakShift=" + maps\mp\perks\_perks::perkTableModifierDelta( perk, "modDeathstreakShift" );
    report += " modBlastShield=" + self hasAbilityModifier( "modBlastShield" );
    report += " modNoRadar=" + self hasAbilityModifier( "modNoRadar" );
    report += " _specialty_blastshield=" + maps\mp\_utility::_hasPerk( "_specialty_blastshield" );
    unlocked = self codol_tryIsItemUnlocked( perk );
    if ( isDefined( unlocked ) )
        report += " unlocked=" + unlocked;
    else
        report += " unlocked=unresolved";
    codolPrintLn( report );
}

testSuicide()
{
    self endon( "disconnect" );
    for ( tries = 0; !isAlive( self ) && tries < 30; tries++ )
        wait 0.5;
    if ( !isAlive( self ) )
    {
        codolPrintLn( "CoDOL killstreak test: " + self.name + " never came back alive" );
        return;
    }
    codolPrintLn( "CoDOL killstreak test: suicide " + self.name );
    self codol_deathGates();
    self thread reportNextSpawn();
    self suicide();
    self codol_deathGates();
}

reportNextSpawn()
{
    self endon( "disconnect" );
    self thread reportAliveAfterSuicide();
    self waittill( "spawned_player" );
    codolPrintLn( "CoDOL killstreak test: " + self.name + " respawned at " + self.origin );
}

reportAliveAfterSuicide()
{
    self endon( "disconnect" );
    for ( i = 0; i < 8; i++ )
    {
        wait 1.0;
        codolPrintLn( "CoDOL killstreak test: " + self.name + " t+" + ( i + 1 ) + "s alive " + isAlive( self ) + " health " + self.health + " session " + self.sessionstate );
    }
}

placeCarriedKillstreak()
{
    self endon( "death" );
    self endon( "disconnect" );
    // Placement is refused while the carrier is airborne or the spot is bad
    // (updateSentryPlacement), so keep asking for a while as a player would.
    for ( i = 0; i < 20; i++ )
    {
        wait 0.5;
        self notify( "place_sentry" );
    }
    turrets = getEntArray( "misc_turret", "classname" );
    codolPrintLn( "CoDOL killstreak test: " + turrets.size + " misc_turret(s) after placement for " + self.name +
        ", onGround " + self isOnGround() );
    foreach ( turret in turrets )
    {
        if ( !isDefined( turret.sentryType ) )
            continue;
        codolPrintLn( "CoDOL killstreak test:   " + turret.sentryType + " at " + turret.origin +
            " carried " + isDefined( turret.carriedBy ) + " canBePlaced " + turret.canBePlaced );
    }
}

// Two modes:
//   scr_codol_botfill 0 (default): keep scr_codol_bots bots (training); lowering
//       the count stops replenishment and never kicks.
//   scr_codol_botfill N > 0: fill around the real players. With no real player
//       there are no bots; otherwise N - humans bots, never more than
//       scr_codol_botmax (default 11), adding and kicking as players come and go.
maintainBots()
{
    level endon( "game_ended" );
    if ( getDvar( "scr_codol_botfill" ) == "" )
        setDvar( "scr_codol_botfill", 0 );
    if ( getDvar( "scr_codol_botmax" ) == "" )
        setDvar( "scr_codol_botmax", 11 );
    for ( ;; )
    {
        bots = [];
        humans = 0;
        players = getEntArray( "player", "classname" );
        foreach ( player in players )
        {
            if ( isDefined( player.codolCombatBot ) && player.codolCombatBot )
                bots[bots.size] = player;
            else if ( !player.isbot )
                humans++;
        }
        // Test hook, idle unless set: pretend this many more real players are on.
        humans += getDvarInt( "scr_codol_botfill_testhumans" );
        fill = getDvarInt( "scr_codol_botfill" );
        if ( fill > 0 )
        {
            desired = 0;
            if ( humans > 0 )
                desired = fill - humans;
            botMax = getDvarInt( "scr_codol_botmax" );
            if ( desired > botMax )
                desired = botMax;
            if ( desired < 0 )
                desired = 0;
            if ( desired > bots.size )
                spawnBots( desired - bots.size );
            else if ( desired < bots.size )
                removeBots( bots, bots.size - desired );
        }
        else
        {
            desired = getDvarInt( "scr_codol_bots" );
            if ( desired > 16 )
                desired = 16;
            if ( desired > bots.size )
                spawnBots( desired - bots.size );
        }
        wait 1;
    }
}

// Kick `count` bots, dead ones first (nobody is watching them), newest first.
removeBots( bots, count )
{
    for ( pass = 0; pass < 2 && count > 0; pass++ )
    {
        for ( i = bots.size - 1; i >= 0 && count > 0; i-- )
        {
            bot = bots[i];
            if ( !isDefined( bot ) || ( pass == 0 && isAlive( bot ) ) )
                continue;
            kick( bot getEntityNumber() );
            bots[i] = undefined;
            count--;
        }
    }
}

spawnBots( count )
{
    level endon( "game_ended" );
    while ( !isDefined( level.autoassign ) || !isDefined( level.class ) )
        wait 0.05;
    for ( i = 0; i < count; i++ )
    {
        bot = addTestClient( "Training " + ( level.codolBotSerial + 1 ) );
        if ( !isDefined( bot ) )
            return;
        bot.codolCombatBot = true;
        bot.codolBotClass = level.codolBotSerial % 9;
        level.codolBotSerial++;
        bot thread enterGame();
        wait 0.25;
    }
}

enterGame()
{
    self endon( "disconnect" );
    level endon( "game_ended" );
    // Let normal connected/menu threads establish pers and class state.
    wait 1;
    self thread watchSpawns();
    self [[level.autoassign]]();
    wait 0.25;
    self.selectedClass = true;
    self [[level.class]]( "class0" );
}

watchSpawns()
{
    self endon( "disconnect" );
    level endon( "game_ended" );
    for ( ;; )
    {
        self waittill( "spawned_player" );
        self enableCombatBot( true );
        self thread watchWedged();
    }
}

// Native movement reports a player that nudge, repath and regoal could not
// move one unit (spawned in solid, or on unwalkable collision). A bot standing
// inside a wall forever is worse than one death: respawn it elsewhere.
watchWedged()
{
    self endon( "disconnect" );
    self endon( "death" );
    level endon( "game_ended" );
    self waittill( "bot_wedged" );
    if ( isAlive( self ) )
        self suicide();
}
