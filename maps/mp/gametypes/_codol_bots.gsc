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

maintainBots()
{
    level endon( "game_ended" );
    for ( ;; )
    {
        desired = getDvarInt( "scr_codol_bots" );
        if ( desired > 16 )
            desired = 16;
        existing = 0;
        players = getEntArray( "player", "classname" );
        foreach ( player in players )
        {
            if ( isDefined( player.codolCombatBot ) && player.codolCombatBot )
                existing++;
        }
        if ( desired > existing )
            spawnBots( desired - existing );
        // Lowering the target stops replenishment; it does not kick players.
        wait 1;
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
