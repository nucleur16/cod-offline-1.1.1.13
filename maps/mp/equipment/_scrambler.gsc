// Native CoDOL deployment, based on plutoniummod/iw5-scripts
// maps/mp/gametypes/_scrambler.gsc (decompiled by xensik/gsc-tool).
// Uses existing native projectile physics, script models, damage and use.
#include maps\mp\_utility;
#include common_scripts\utility;

init()
{
    precacheModel( "weapon_jammer" );
    level.scramblers = [];
    level thread onConnect();
}

onConnect()
{
    level endon( "game_ended" );
    for ( ;; )
    {
        level waittill( "connected", player );
        player thread monitorUse();
    }
}

monitorUse()
{
    self notify( "codol_scrambler_monitor" );
    self endon( "codol_scrambler_monitor" );
    self endon( "disconnect" );
    level endon( "game_ended" );
    for ( ;; )
    {
        self waittill( "grenade_fire", missile, weapon );
        if ( weapon != "scrambler_mp" && weapon != "scrambler" )
            continue;
        missile thread deploy( self );
    }
}

deploy( owner )
{
    self endon( "death" );
    level endon( "game_ended" );
    if ( !isDefined( owner ) || !isAlive( owner ) )
    {
        self delete();
        return;
    }
    owner thread watchPending( self );
    self hide();
    // IW5 waits missile_stuck. The native server emits stationary exactly
    // on moving->settled transition; it does not use a polling proxy.
    self waittill( "stationary" );
    if ( !isDefined( owner ) || !isAlive( owner ) )
    {
        self delete();
        return;
    }
    if ( distanceSquared( self.origin, owner.origin ) > 1600 )
    {
        trace = bulletTrace( owner.origin, owner.origin - (0,0,40), false, owner );
        if ( trace["fraction"] == 1 )
        {
            owner setWeaponAmmoStock( "scrambler_mp", owner getWeaponAmmoStock( "scrambler_mp" ) + 1 );
            self delete();
            return;
        }
        self.origin = trace["position"];
    }
    if ( isDefined( owner.deployedScrambler ) )
        destroy( owner.deployedScrambler );
    device = spawn( "script_model", self.origin );
    device setModel( "weapon_jammer" );
    device.owner = owner;
    device.team = owner.team;
    device.health = 999999;
    device.maxhealth = 100;
    device.damageTaken = 0;
    device setCanDamage( 1 );
    device makeScrambler( owner );
    owner.deployedScrambler = device;
    level.scramblers[level.scramblers.size] = device;
    device thread damageListener();
    device thread useListener();
    device thread beeps();
    owner thread watchOwner( device );
    level thread watchEnd( device );
    self delete();
}

destroy( device )
{
    if ( !isDefined( device ) || isDefined( device.codolScramblerDestroying ) )
        return;
    device.codolScramblerDestroying = true;
    device maps\mp\equipment\_pickup::close();
    // Callers listen for this device's death. Detach so that notifying them
    // cannot terminate deletion and list cleanup halfway through.
    level thread destroyDetached( device );
}

destroyDetached( device )
{
    if ( !isDefined( device ) )
        return;
    if ( isDefined( device.owner ) && isDefined( device.owner.deployedScrambler ) && device.owner.deployedScrambler == device )
        device.owner.deployedScrambler = undefined;
    device notify( "death" );
    device delete();
    remaining = [];
    foreach ( item in level.scramblers )
        if ( isDefined( item ) )
            remaining[remaining.size] = item;
    level.scramblers = remaining;
}

watchPending( missile )
{
    missile endon( "death" );
    waittill_any( "death", "disconnect", "joined_team", "joined_spectators" );
    if ( isDefined( missile ) )
        missile delete();
}

watchOwner( device )
{
    device endon( "death" );
    waittill_any( "death", "disconnect", "joined_team", "joined_spectators" );
    destroy( device );
}

watchEnd( device )
{
    device endon( "death" );
    level waittill( "game_ended" );
    destroy( device );
}

beeps()
{
    self endon( "death" );
    for ( ;; )
    {
        wait 3;
        self playSound( "scrambler_beep" );
    }
}

damageListener()
{
    self endon( "death" );
    for ( ;; )
    {
        self waittill( "damage", damage, attacker, direction, point, mod, model, tag, part, flags, weapon );
        if ( isDefined( attacker ) && !maps\mp\gametypes\_weapons::friendlyFireCheck( self.owner, attacker ) )
            continue;
        if ( isDefined( weapon ) && (weapon == "concussion_grenade_mp" || weapon == "flash_grenade_mp" || weapon == "smoke_grenade_mp") )
            continue;
        // IW5 retains these fields for damage/challenge consumers and sends
        // hit feedback for every accepted player hit, not just destruction.
        self.wasdamaged = 1;
        // Native damage notify passes g_shared.h iDFLAGS_PENETRATION (8).
        if ( isDefined( flags ) && (flags & 8) )
            self.wasdamagedfrombulletpenetration = 1;
        if ( mod == "MOD_MELEE" )
            self.damageTaken += self.maxhealth;
        self.damageTaken += damage;
        if ( isDefined( attacker ) && isPlayer( attacker ) )
            attacker maps\mp\gametypes\_damagefeedback::updateDamageFeedback( "scrambler" );
        if ( self.damageTaken >= self.maxhealth )
        {
            if ( isDefined( attacker ) && isPlayer( attacker ) && attacker != self.owner )
                attacker notify( "destroyed_explosive" );
            destroy( self );
            return;
        }
    }
}

useListener()
{
    pickedUp = self maps\mp\equipment\_pickup::waitForPickup( self, self.owner, "scrambler_mp", &"WEAPON_PICKUP_SCRAMBLER" );
    if ( isDefined( pickedUp ) && pickedUp )
        destroy( self );
}
