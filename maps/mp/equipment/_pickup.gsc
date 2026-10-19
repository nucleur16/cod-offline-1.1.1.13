// Native equipment reclamation. IW5 equipmentwatchuse semantics, with
// capacity verification and lifecycle cancellation before entity deletion.
#include maps\mp\_utility;

// self is the deployed device. Callers own terminal cleanup after true.
waitForPickup( trigger, owner, weapon, hint )
{
    self endon( "death" );
    self endon( "mine_triggered" );
    self endon( "mine_destroyed" );
    self endon( "exploding" );
    self endon( "codol_pickup_closed" );
    // An enemy took the device over (perks/_codol_equipperks.gsc takeOver).
    self endon( "codol_hacked" );
    self.codolPickupTrigger = trigger;
    trigger setCursorHint( "HINT_NOICON" );
    trigger setHintString( hint );
    trigger setSelfUsable( owner );
    self thread pollOwnerUse( trigger, owner, weapon );
    for ( ;; )
    {
        trigger waittill( "trigger", player );
        source = "use";
        if ( isDefined( trigger.codolPolledUse ) )
            source = "use-button poll";
        trigger.codolPolledUse = undefined;
        if ( !isDefined( owner ) || !isDefined( player ) || player != owner || !isAlive( player ) )
            continue;
        if ( isDefined( self.codolPickupClosed ) || isDefined( self.codolPickupClaimed ) )
            return false;
        // A different class must not silently acquire the previous class's item.
        if ( !player hasWeapon( weapon ) )
        {
            codolPrintLn( "CoDOL pickup: " + player.name + " cannot reclaim " + weapon + " (" + source + "): not in the current loadout" );
            continue;
        }
        before = player getWeaponAmmoStock( weapon );
        player setWeaponAmmoStock( weapon, before + 1 );
        after = player getWeaponAmmoStock( weapon );
        // Native setter clamps at clip/stock capacity. Keep device at capacity.
        if ( after != before + 1 )
        {
            codolPrintLn( "CoDOL pickup: " + player.name + " cannot reclaim " + weapon + " (" + source + "): already full (" +
                before + " -> " + after + ")" );
            continue;
        }
        self.codolPickupClaimed = true;
        trigger makeUnusable();
        codolPrintLn( "CoDOL pickup: " + player.name + " reclaimed " + weapon + " (" + source + ")" );
        return true;
    }
}

// The owner's use press reclaims the device even when the native use search
// (cursor hint -> Player_UpdateActivate) did not deliver it: a press next to
// the device while facing it raises the same "trigger" the native use would.
// The native path runs first in the frame, so this only acts when it did not.
pollOwnerUse( trigger, owner, weapon )
{
    self endon( "death" );
    self endon( "mine_triggered" );
    self endon( "mine_destroyed" );
    self endon( "exploding" );
    self endon( "codol_pickup_closed" );
    self endon( "codol_hacked" );
    trigger endon( "death" );
    if ( !isDefined( owner ) )
        return;
    owner endon( "disconnect" );
    // A press already held when the device lands (the throw itself) is not a use.
    held = true;
    for ( ;; )
    {
        wait 0.05;
        if ( !isDefined( trigger ) || isDefined( self.codolPickupClaimed ) || isDefined( self.codolPickupClosed ) )
            return;
        if ( !isAlive( owner ) )
        {
            held = true;
            continue;
        }
        pressed = owner useButtonPressed();
        rising = pressed && !held;
        held = pressed;
        if ( !rising || !owner pollCanReach( self, trigger ) )
            continue;
        // Give the native use one more frame to land first.
        wait 0.05;
        if ( !isDefined( trigger ) || isDefined( self.codolPickupClaimed ) || isDefined( self.codolPickupClosed ) )
            return;
        codolPrintLn( "CoDOL pickup: native use did not reach " + owner.name + "'s " + weapon + " pickup; the use button poll takes it" );
        trigger.codolPolledUse = true;
        trigger notify( "trigger", owner );
    }
}

// self is the owner: within the native use reach of the device and facing it.
pollCanReach( device, trigger )
{
    if ( isDefined( self.lastStand ) && self.lastStand )
        return false;
    if ( self isUsingRemote() )
        return false;
    eye = self getEye();
    // The device lies between its origin and the raised pickup trigger.
    target = ( device.origin + trigger.origin ) * 0.5;
    if ( distanceSquared( eye, target ) > 96 * 96 )
        return false;
    forward = anglesToForward( self getPlayerAngles() );
    return vectorDot( forward, vectorNormalize( target - eye ) ) >= 0.5;
}

close()
{
    self.codolPickupClosed = true;
    if ( isDefined( self.codolPickupTrigger ) ) self.codolPickupTrigger makeUnusable();
    self notify( "codol_pickup_closed" );
}

// C4/claymore remain native missiles. Use a linked script_origin because
// missile receivers themselves route through native grenade throwback use.
missilePickup( owner, weapon, hint )
{
    self endon( "death" );
    self endon( "codol_hacked" );
    if ( isDefined( self.codolPickupClosed ) ) return;
    trigger = spawn( "script_origin", self.origin + (0,0,20) );
    trigger linkTo( self );
    level thread deleteTriggerOnDeath( self, trigger );
    pickedUp = self waitForPickup( trigger, owner, weapon, hint );
    if ( isDefined( pickedUp ) && pickedUp )
        level thread reclaimMissile( self, trigger );
}

deleteTriggerOnDeath( device, trigger )
{
    device waittill( "death" );
    if ( isDefined( trigger ) ) trigger delete();
}

reclaimMissile( device, trigger )
{
    if ( !isDefined( device ) ) return;
    device notify( "death" );
    if ( isDefined( trigger ) ) trigger delete();
    // Native G_FreeEntity cancels think/fuse and removes the missile entirely.
    device delete();
}
