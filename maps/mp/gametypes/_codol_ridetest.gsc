// CoDOL radar / gunship / announcer test hook. Idle unless the dvar is set,
// and only on a developer server. Output lines start "CoDOL ride test:".
//
//   set scr_codol_ridetest "radar"
//       each player's guid, radar-blocked input and hasRadar, plus the
//       free-for-all UAV / counter-UAV counts _uav.gsc keys by that guid.
//   set scr_codol_ridetest "calm <client>"
//       turns a bot's combat AI off (it keeps re-selecting its own weapon,
//       so a scripted switch such as the gunship's never sticks).
//   set scr_codol_ridetest "aim <gunner> <target>"
//       points an AC-130 gunner's view from the gunship camera at a player.
//   set scr_codol_ridetest "fire <client> <seconds> [weapon]"
//       a bot holds attack (0x1) for <seconds>, holding <weapon> first when
//       one is named; prints each weapon_fired with the body and gunship
//       camera origins, the projectiles in flight and each projectile_impact.
//   set scr_codol_ridetest "subs"
//       every leader-dialog alias of both factions with the subtitle
//       reference soundAliasSubtitle picks for it.
#include maps\mp\_utility;
#include common_scripts\utility;

init()
{
    if ( isDefined( level.codolRideTestStarted ) )
        return;
    level.codolRideTestStarted = true;
    setDvar( "scr_codol_ridetest", "" );
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
        request = getDvar( "scr_codol_ridetest" );
        if ( request == "" )
            continue;
        setDvar( "scr_codol_ridetest", "" );
        tokens = strTok( request, " " );
        if ( tokens[0] == "radar" )
            reportRadar();
        else if ( tokens[0] == "fire" && tokens.size > 2 )
        {
            player = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) )
                player thread holdFire( float( tokens[2] ), tokens[3] );
        }
        else if ( tokens[0] == "calm" && tokens.size > 1 )
        {
            // The combat AI keeps choosing its own weapon; a scripted
            // switch (the gunship's) only sticks on a bot without it.
            player = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( tokens[1] ) );
            if ( isDefined( player ) )
            {
                player enableCombatBot( false );
                {}
            }
        }
        else if ( tokens[0] == "aim" && tokens.size > 2 )
        {
            // Point a gunship gunner's view at another player, from the
            // gunship camera it fires from.
            shooter = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( tokens[1] ) );
            target = maps\mp\gametypes\_codol_equiptest::playerByNumber( int( tokens[2] ) );
            if ( isDefined( shooter ) && isDefined( target ) && isDefined( level.ac130 ) )
            {
                camera = level.ac130 getTagOrigin( "tag_player" );
                target.maxhealth = 100;
                target.health = 100;
                shooter setPlayerAngles( vectorToAngles( target.origin - camera ) );
                {}
            }
        }
        else if ( tokens[0] == "subs" )
            reportSubtitles();
    }
}

reportRadar()
{
    foreach ( player in level.players )
    {
        line = "CoDOL ride test: radar " + player.name + " guid " + player.guid + " team " + player.team +
            " blocked " + player.isRadarBlocked + " hasRadar " + player.hasRadar;
        if ( !level.teamBased && isDefined( player.guid ) )
        {
            if ( isDefined( level.activeCounterUAVs[ player.guid ] ) )
                line = line + " counterUAVs " + level.activeCounterUAVs[ player.guid ];
            if ( isDefined( level.activeUAVs[ player.guid ] ) )
                line = line + " UAVs " + level.activeUAVs[ player.guid ];
        }
        {}
    }
}

holdFire( seconds, weapon )
{
    self endon( "disconnect" );
    self notify( "codol_ridetest_fire" );
    self endon( "codol_ridetest_fire" );
    if ( isDefined( weapon ) )
    {
        self switchToWeapon( weapon );
        for ( i = 0; i < 40 && self getCurrentWeapon() != weapon; i++ )
            wait 0.1;
        if ( self getCurrentWeapon() != weapon )
        {
            // A bot's scripted switch does not complete; put the weapon in
            // its hands directly, then keep the button held so its commands
            // keep naming that weapon.
            {}
            self setSpawnWeapon( weapon );
            self codol_botPressButtons( 8192, 1 );
            wait 0.3;
        }
        if ( self getCurrentWeapon() != weapon )
        {
            {}
            return;
        }
    }
    {}
    self thread logFire( seconds + 12 );
    self codol_botPressButtons( 1, seconds );
}

logFire( seconds )
{
    self endon( "disconnect" );
    self endon( "codol_ridetest_fire" );
    self thread logImpacts( seconds );
    endTime = getTime() + seconds * 1000;
    while ( getTime() < endTime )
    {
        self waittill( "weapon_fired" );
        camera = "none";
        if ( isDefined( level.ac130 ) )
            camera = level.ac130 getTagOrigin( "tag_player" );
        {}
        foreach ( rocket in getEntArray( "rocket", "classname" ) )
            {}
    }
}

logImpacts( seconds )
{
    self endon( "disconnect" );
    self endon( "codol_ridetest_fire" );
    endTime = getTime() + seconds * 1000;
    while ( getTime() < endTime )
    {
        self waittill( "projectile_impact", weaponName, position, radius );
        {}
    }
}

reportSubtitles()
{
    teams = [];
    teams[0] = "allies";
    teams[1] = "axis";
    foreach ( key, dialog in game[ "dialog" ] )
        foreach ( team in teams )
        {
            alias = game[ "voice" ][ team ] + dialog;
            subtitle = soundAliasSubtitle( alias );
            if ( !isDefined( subtitle ) )
                subtitle = "<none>";
            {}
        }
}
