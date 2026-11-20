#include common_scripts\utility;

// C1 SR71 satellite reveal. Lifetime uses the primary IW5 directional UAV's
// 30 seconds until a C1 script duration is recovered; this is not a pro alias.
init()
{
    level.killstreakFuncs["sr71_mp"] = ::tryUseSR71;
    level.sr71TeamUntil = [];
    level.sr71TeamPrevious = [];
    level.sr71TeamUntil["allies"] = 0;
    level.sr71TeamUntil["axis"] = 0;
    level.sr71TeamPrevious["allies"] = 0;
    level.sr71TeamPrevious["axis"] = 0;
    level thread updateSR71();
    level thread clearSR71OnGameEnd();
}

tryUseSR71( lifeId )
{
    if ( !isAlive( self ) || !isDefined( self.pers["team"] ) )
        return false;
    if ( self.pers["team"] != "allies" && self.pers["team"] != "axis" )
        return false;

    deadline = getTime() + 30000;
    if ( level.teamBased )
    {
        team = self.pers["team"];
        if ( !level.sr71TeamUntil[team] )
            level.sr71TeamPrevious[team] = getTeamSatellite( team );
        level.sr71TeamUntil[team] = deadline;
        setTeamSatellite( team, 1 );
    }
    else
    {
        if ( !isDefined( self.pers["sr71Until"] ) || !self.pers["sr71Until"] )
            self.pers["sr71Previous"] = self.hasSatellite;
        self.pers["sr71Until"] = deadline;
        self.pers["sr71Team"] = self.pers["team"];
        self.hasSatellite = 1;
    }
    self maps\mp\_matchdata::logKillstreakEvent( "sr71_mp", self.origin );
    self notify( "used_sr71" );
    return true;
}

updateSR71()
{
    level endon( "game_ended" );
    for ( ;; )
    {
        now = getTime();
        foreach ( team, deadline in level.sr71TeamUntil )
        {
            if ( deadline && deadline <= now )
            {
                // Do not overwrite a newer external mode/type assignment.
                if ( getTeamSatellite( team ) == 1 )
                    setTeamSatellite( team, level.sr71TeamPrevious[team] );
                level.sr71TeamUntil[team] = 0;
            }
        }
        foreach ( player in level.players )
        {
            if ( !isDefined( player.pers["sr71Until"] ) || !player.pers["sr71Until"] )
                continue;
            if ( player.pers["team"] != player.pers["sr71Team"] )
            {
                if ( player.hasSatellite == 1 )
                    player.hasSatellite = player.pers["sr71Previous"];
                player.pers["sr71Until"] = 0;
                continue;
            }
            if ( player.pers["sr71Until"] > now )
            {
                // Native respawn clears the individual input; the earned
                // timed effect persists independently of the owner's life.
                player.hasSatellite = 1;
            }
            else
            {
                if ( player.hasSatellite == 1 )
                    player.hasSatellite = player.pers["sr71Previous"];
                player.pers["sr71Until"] = 0;
            }
        }
        wait( 0.05 );
    }
}

clearSR71OnGameEnd()
{
    level waittill( "game_ended" );
    foreach ( team, deadline in level.sr71TeamUntil )
    {
        if ( deadline && getTeamSatellite( team ) == 1 )
            setTeamSatellite( team, level.sr71TeamPrevious[team] );
        level.sr71TeamUntil[team] = 0;
    }
    foreach ( player in level.players )
    {
        if ( !isDefined( player.pers["sr71Until"] ) || !player.pers["sr71Until"] )
            continue;
        if ( player.hasSatellite == 1 )
            player.hasSatellite = player.pers["sr71Previous"];
        player.pers["sr71Until"] = 0;
    }
}
