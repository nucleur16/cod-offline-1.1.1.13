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
    self.codolPickupTrigger = trigger;
    trigger setCursorHint( "HINT_NOICON" );
    trigger setHintString( hint );
    trigger setSelfUsable( owner );
    for ( ;; )
    {
        trigger waittill( "trigger", player );
        if ( !isDefined( owner ) || !isDefined( player ) || player != owner || !isAlive( player ) )
            continue;
        if ( isDefined( self.codolPickupClosed ) || isDefined( self.codolPickupClaimed ) )
            return false;
        // A different class must not silently acquire the previous class's item.
        if ( !player hasWeapon( weapon ) ) continue;
        before = player getWeaponAmmoStock( weapon );
        player setWeaponAmmoStock( weapon, before + 1 );
        after = player getWeaponAmmoStock( weapon );
        // Native setter clamps at clip/stock capacity. Keep device at capacity.
        if ( after != before + 1 ) continue;
        self.codolPickupClaimed = true;
        trigger makeUnusable();
        return true;
    }
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
