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
    graph = "waypoints/" + getDvar( "mapname" ) + "_wp.csv";
    if ( loadBotWaypoints( graph ) <= 0 )
        return;
    level.codolBotSerial = 0;
    level thread maintainBots();
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
