// IW5 player EMP lifecycle adapted to native C1 SetEMPJammed publication.
// Reference: obj/reference/iw5-scripts/maps/mp/_empgrenade.gsc.
// The ten-second duration is reference policy, not recovered C1 script timing.
#include maps\mp\_utility;
#include common_scripts\utility;

init()
{
    precacheShellshock( "flashbang_mp" );
    level thread onConnect();
}

onConnect()
{
    level endon( "game_ended" );
    for ( ;; )
    {
        level waittill( "connected", player );
        player thread lifecycle();
        level thread endCleanup( player );
    }
}

lifecycle()
{
    self endon( "disconnect" );
    level endon( "game_ended" );
    for ( ;; )
    {
        self waittill( "spawned_player" );
        if ( !isDefined( self.codolEMPGeneration ) ) self.codolEMPGeneration = 0;
        self.codolEMPGeneration++;
        self notify( "codol_emp_monitor" );
        self notify( "codol_emp_apply" );
        self.empGrenaded = false;
        self.empEndTime = 0;
        self setEMPJammed( self globalEMPActive() );
        self thread monitor();
        self thread deathCleanup( self.codolEMPGeneration );
    }
}

monitor()
{
    self endon( "codol_emp_monitor" );
    self endon( "death" );
    self endon( "disconnect" );
    level endon( "game_ended" );
    for ( ;; )
    {
        self waittill( "emp_grenaded", attacker );
        if ( !isAlive( self ) || isDefined( self.usingRemote ) ) continue;
        if ( self _hasPerk( "specialty_empimmune" ) ) continue;
        affectVictim = true;
        affectAttacker = false;
        if ( level.teamBased && isDefined( attacker ) && isPlayer( attacker ) &&
             attacker != self && attacker.team == self.team )
        {
            if ( level.friendlyfire == 0 ) continue;
            if ( level.friendlyfire == 2 ) affectVictim = false;
            if ( level.friendlyfire == 2 || level.friendlyfire == 3 ) affectAttacker = true;
        }
        else if ( isDefined( attacker ) && isPlayer( attacker ) )
            attacker notify( "emp_hit" );
        if ( affectVictim ) self thread applyEMP();
        if ( affectAttacker && isAlive( attacker ) ) attacker thread applyEMP();
    }
}

applyEMP()
{
    self notify( "codol_emp_apply" );
    self endon( "codol_emp_apply" );
    self endon( "death" );
    self endon( "disconnect" );
    level endon( "game_ended" );
    self.empGrenaded = true;
    self.empEndTime = getTime() + 10000;
    self setEMPJammed( true );
    self shellshock( "flashbang_mp", 1 );
    self thread rumble( 0.75 );
    wait 10;
    self clearPersonalEMP();
}

// IW5 emprumbleloop: heavy damage rumble for the first 0.75 s.
// rumble/damage_heavy ships in C1 common_gametype_mp; _gamelogic precaches it.
rumble( duration )
{
    self notify( "codol_emp_rumble" );
    self endon( "codol_emp_rumble" );
    self endon( "death" );
    self endon( "disconnect" );
    endTime = getTime() + duration * 1000;
    while ( getTime() < endTime )
    {
        self playRumbleOnEntity( "damage_heavy" );
        wait 0.05;
    }
}

deathCleanup( generation )
{
    self endon( "codol_emp_monitor" );
    self endon( "disconnect" );
    // One waiter survives the death event that terminates applyEMP.
    level endon( "game_ended" );
    self waittill_any( "death", "joined_team", "joined_spectators" );
    if ( !isDefined( self ) || self.codolEMPGeneration != generation ) return;
    self notify( "codol_emp_apply" );
    self clearPersonalEMP();
}

clearPersonalEMP()
{
    self.empGrenaded = false;
    self.empEndTime = 0;
    self setEMPJammed( self globalEMPActive() );
}

globalEMPActive()
{
    if ( self.team == "spectator" ) return false;
    if ( level.teamBased )
        return isDefined( level.teamEMPed[self.team] ) && level.teamEMPed[self.team];
    return isDefined( level.empPlayer ) && level.empPlayer != self;
}

isEMPGrenaded()
{
    return isDefined( self.empGrenaded ) && self.empGrenaded;
}

endCleanup( player )
{
    player endon( "disconnect" );
    level waittill( "game_ended" );
    if ( isDefined( player ) ) player clearPersonalEMP();
}
