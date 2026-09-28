// sh1x C1 mortar. Selection/ballistics adapted from Treyarch T5 _mortar.gsc:
// https://github.com/AtomicLizard99/Black-Ops-1-GSC-Raw-Dump
// Native codol_launchmortarbomb creates the actual catalogue projectile.
#include maps\mp\_utility;
#include common_scripts\utility;

init()
{
    precacheLocationSelector( "map_mortar_selector" );
    precacheItem( "mortar_mp" );
    level.killstreakFuncs["mortar_mp"] = ::tryUseMortar;
}

tryUseMortar( lifeId, kID )
{
    if ( !isDefined( kID ) )
        return false;
    if ( isDefined( self.selectingLocation ) && self.selectingLocation )
        return false;
    if ( !isAlive( self ) || self isEMPed() )
        return false;
    if ( !isDefined( level.mapCenter ) || !isDefined( level.spawnMins ) ||
         !isDefined( level.spawnMaxs ) )
        return false;
    self.codolMortarPending = undefined;
    selection = spawnStruct();
    // Dedicated action slots can select an older reward. Preserve the exact
    // dispatcher-owned identity across all three confirmations and commit.
    selection.kID = kID;
    selection.lifeId = lifeId;
    selection.targets = [];
    self.codolMortarSelection = selection;
    self.selectingLocation = true;
    self beginLocationSelection( "map_mortar_selector", false, 800 );
    self setBlurForPlayer( 4, 0.3 );
    self thread cancelOn( selection, "cancel_location" );
    self thread cancelOn( selection, "death" );
    self thread cancelOn( selection, "disconnect" );
    self thread cancelOn( selection, "joined_team" );
    self thread cancelOn( selection, "joined_spectators" );
    self thread cancelOnGameEnd( selection );
    self thread cancelOnEMP( selection );
    self thread cancelOnWeaponChange( selection );
    // Keep all endons off the persistent killstreak-use waiter's call stack.
    self thread selectTargets( selection );
    self waittill( "codol_mortar_selection_done" );
    if ( !isDefined( selection.result ) || !selection.result )
        return false;
    self.codolMortarPending = selection;
    return true;
}

selectTargets( selection )
{
    self endon( "codol_mortar_selection_done" );
    while ( selection.targets.size < 3 )
    {
        self waittill( "confirm_location", position );
        // T5 selectMortarLocation also receives an undefined confirmation
        // when selection is cancelled. Complete the owned cleanup before
        // indexing position, leaving the reward unconsumed.
        if ( !isDefined( position ) )
        {
            level thread finishSelection( self, selection, false );
            return;
        }
        // C1 selector supplies XY. Resolve real ground from below map sky,
        // following IW4 airstrike altitude and T5 mortar ground tracing.
        sky = bulletTrace( level.mapCenter + (0,0,1000000), level.mapCenter, false, undefined );
        top = (position[0], position[1], sky["position"][2] - 514);
        ground = bulletTrace( top, top - (0,0,100000), false, undefined );
        if ( ground["fraction"] >= 1 )
            continue;
        selection.targets[selection.targets.size] = ground["position"];
    }
    level thread finishSelection( self, selection, true );
}

cancelOn( selection, event )
{
    self endon( "codol_mortar_selection_done" );
    self waittill( event );
    // Detached cleanup survives the notification that ends these waiters.
    level thread finishSelection( self, selection, false );
}

cancelOnGameEnd( selection )
{
    self endon( "codol_mortar_selection_done" );
    level waittill( "game_ended" );
    level thread finishSelection( self, selection, false );
}

// T5 _airsupport::endSelectionThink also ends the selection on
// "weapon_change": a player who puts the tablet away without confirming
// three targets must not keep an open selection, which refused every other
// reward ("a location selection is already open") and held the dispatcher's
// activation lock until the next death.
cancelOnWeaponChange( selection )
{
    self endon( "codol_mortar_selection_done" );
    tablet = maps\mp\killstreaks\_killstreaks::getKillstreakWeapon( "mortar_mp" );
    for ( ;; )
    {
        self waittill( "weapon_change", newWeapon );
        if ( isDefined( newWeapon ) && newWeapon == tablet )
            continue;
        level thread finishSelection( self, selection, false );
        return;
    }
}

cancelOnEMP( selection )
{
    self endon( "codol_mortar_selection_done" );
    for ( ;; )
    {
        level waittill( "emp_update" );
        if ( self isEMPed() )
        {
            level thread finishSelection( self, selection, false );
            return;
        }
    }
}

finishSelection( player, selection, accepted )
{
    if ( !isDefined( player ) || !isDefined( player.codolMortarSelection ) ||
         player.codolMortarSelection != selection )
        return;
    player.codolMortarSelection = undefined;
    selection.result = accepted;
    player.selectingLocation = undefined;
    player endLocationSelection();
    player setBlurForPlayer( 0, 0.3 );
    player notify( "codol_mortar_selection_done" );
}

// Called by the reward owner AFTER its stillOwned check and queue removal.
// No timer guesses and no projectile exists while selection can be cancelled.
commitMortar( lifeId, kID )
{
    if ( !isDefined( self.codolMortarPending ) )
        return;
    pending = self.codolMortarPending;
    self.codolMortarPending = undefined;
    if ( pending.kID != kID || pending.lifeId != lifeId )
        return;
    self thread fireMortar( pending.targets );
}

fireMortar( targets )
{
    self endon( "disconnect" );
    level endon( "game_ended" );
    for ( i = 0; i < targets.size; i++ )
    {
        self thread fireCannon( targets[i], bestDirection( targets[i], self.team ) );
        if ( i + 1 < targets.size )
            wait 2.5;
    }
}

fireCannon( target, yaw )
{
    self endon( "disconnect" );
    level endon( "game_ended" );
    // T5 _tweakables: mortarCanonCount3, mortarCanonShellCount2.
    for ( shell = 0; shell < 2; shell++ )
    {
        launchShell( self, target, yaw );
        if ( shell == 0 )
            wait randomFloatRange( 1.1, 1.5 );
    }
}

launchShell( owner, target, yaw )
{
    // Stock T5 trajectory:75 degrees,6000 horizontal units, start30% into
    // flight. Query the native trajectory evaluator used by the actual bomb.
    gravity = codol_getProjectileGravity();
    distance = 6000;
    ratio = 0.3;
    pitch = 75;
    speed = sqrt( gravity * distance / sin( 2 * pitch ) );
    horizontal = cos( pitch ) * speed;
    vertical = sin( pitch ) * speed;
    elapsed = (distance / horizontal) * ratio;
    forward = anglesToForward( (0,yaw,0) );
    start = target - scaleVector( forward, distance * (1-ratio) );
    height = vertical * elapsed - 0.5 * gravity * elapsed * elapsed;
    start += (0,0,height);
    velocity = scaleVector( forward, horizontal ) + (0,0,vertical-gravity*elapsed);
    bomb = owner codol_launchmortarbomb( "mortar_mp", start, velocity );
    // Real projectile/explosion owns damage, native surface normal, visual
    // effect and sound. T5 incoming aliases are absent from shipped C1 MP.
}

bestDirection( target, team )
{
    entities = getEntArray( "mortar_" + team, "targetname" );
    origins = [];
    foreach ( entity in entities )
        origins[origins.size] = entity.origin;
    if ( origins.size == 0 )
    {
        // T5's six candidate origins outside the longest map axis.
        mins = level.spawnMins - (1000,1000,0);
        maxs = level.spawnMaxs + (1000,1000,0);
        major = 0;
        minor = 1;
        if ( maxs[1]-mins[1] > maxs[0]-mins[0] )
        {
            major = 1;
            minor = 0;
        }
        for ( i = 0; i < 3; i++ )
        {
            // Use distances, not absolute signed coordinates; the stock
            // formula can put a supposed outside origin inside negative maps.
            low = mins[major] - randomFloatRange( 0, level.mapCenter[major]-mins[major] ) * 2;
            high = maxs[major] + randomFloatRange( 0, maxs[major]-level.mapCenter[major] ) * 2;
            cross = randomFloatRange( mins[minor], maxs[minor] );
            if ( major == 0 )
            {
                origins[origins.size] = (low,cross,level.mapCenter[2]);
                origins[origins.size] = (high,cross,level.mapCenter[2]);
            }
            else
            {
                origins[origins.size] = (cross,low,level.mapCenter[2]);
                origins[origins.size] = (cross,high,level.mapCenter[2]);
            }
        }
    }
    best = 0;
    bestDistance = undefined;
    foundNegative = false;
    for ( i = 0; i < origins.size; i++ )
    {
        line = origins[i] - level.mapCenter;
        lengthSquared = vectorDot( line, line );
        if ( lengthSquared <= 0 )
            continue;
        t = vectorDot( target-level.mapCenter, line ) / lengthSquared;
        if ( t > 0 && foundNegative )
            continue;
        delta = target - (level.mapCenter + scaleVector( line, t ));
        distanceSquared = vectorDot( delta, delta );
        if ( !isDefined( bestDistance ) || distanceSquared < bestDistance || (!foundNegative && t < 0) )
        {
            best = i;
            bestDistance = distanceSquared;
            if ( t < 0 )
                foundNegative = true;
        }
    }
    angles = vectorToAngles( level.mapCenter - origins[best] );
    return angles[1];
}

scaleVector( vector, scale )
{
    return (vector[0]*scale,vector[1]*scale,vector[2]*scale);
}
