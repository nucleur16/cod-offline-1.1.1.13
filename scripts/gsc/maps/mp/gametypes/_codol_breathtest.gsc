// CoDOL hold-breath test hook. Idle unless the dvar is set, and only on a
// developer server.
//
//   set scr_breathtest "<client> <weapon> <adsSeconds> <holdSeconds>"
//       gives <client> (a bot) <weapon>, switches to it, holds ADS (0x800)
//       and, once scoped, ADS plus the breath button (0x2000) for
//       <holdSeconds>. It then keeps ADS alone for one more second so the
//       release and the gasp show in the server's "CoDOL breath" trace. The
//       engine's own pmove produces the state; nothing here writes it.
#include maps\mp\_utility;
#include common_scripts\utility;

init()
{
    if ( isDefined( level.codolBreathTestStarted ) )
        return;
    level.codolBreathTestStarted = true;
    setDvar( "scr_breathtest", "" );
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
        request = getDvar( "scr_breathtest" );
        if ( request == "" )
            continue;
        setDvar( "scr_breathtest", "" );
        tokens = strTok( request, " " );
        if ( tokens.size < 4 )
            continue;
        player = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( tokens[0] ) );
        if ( !isDefined( player ) || !isAlive( player ) )
        {
            codolPrintLn( "CoDOL breath test: no live client " + tokens[0] );
            continue;
        }
        player thread holdBreath( tokens[1], float( tokens[2] ), float( tokens[3] ) );
    }
}

holdBreath( weapon, adsSeconds, holdSeconds )
{
    self endon( "death" );
    self endon( "disconnect" );
    self setScriptGoal( self.origin, 8 );
    self giveWeapon( weapon );
    // A test client's own command names its held weapon every frame, so
    // setSpawnWeapon is what actually raises the new one.
    self setSpawnWeapon( weapon );
    self switchToWeapon( weapon );
    wait 1.5;
    codolPrintLn( "CoDOL breath test: " + self.name + " holds " + self getCurrentWeapon() + " (has " + weapon + ": " + self hasWeapon( weapon ) + ")" );
    self codol_botPressButtons( 2048, adsSeconds );
    wait adsSeconds;
    codolPrintLn( "CoDOL breath test: breath button down" );
    self codol_botPressButtons( 2048 + 8192, holdSeconds );
    wait holdSeconds;
    codolPrintLn( "CoDOL breath test: breath button up" );
    self codol_botPressButtons( 2048, 1.5 );
    wait 1.5;
    self codol_botPressButtons( 0, 0 );
    self clearScriptGoal();
    codolPrintLn( "CoDOL breath test: done" );
}
