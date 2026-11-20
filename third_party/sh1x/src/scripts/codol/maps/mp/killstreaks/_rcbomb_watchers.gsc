// RC-specific adapter for T5 _weaponobjects.gsc at 70689b8288e60079d48926657e670c41aab4abdf.
// Retains one-shot detonation, damage filtering and detached watcher ownership.
#include common_scripts\utility;

createRCBombWatcher()
{
    if ( isDefined( self.codolRCWatcher ) )
        return self.codolRCWatcher;
    watcher = spawnStruct();
    watcher.objectArray = [];
    watcher.owner = self;
    watcher.name = "rcbomb";
    watcher.weapon = "rcbomb_mp";
    watcher.stunTime = 5;
    watcher.detonate = maps\mp\killstreaks\_rcbomb::blowup;
    self.codolRCWatcher = watcher;
    return watcher;
}

getWeaponObjectWatcher( name )
{
    if ( name != "rcbomb" )
        return undefined;
    return createRCBombWatcher();
}

addWeaponObjectToWatcher( name, object )
{
    watcher = getWeaponObjectWatcher( name );
    assert( isDefined( watcher ) && isDefined( object ) );
    watcher.objectArray[watcher.objectArray.size] = object;
    object.owner = self;
    object.name = watcher.weapon;
    object.detonated = false;
    object.ownerGetsAssist = true;
    object thread weaponObjectDamage( watcher );
    level thread pruneOnDeath( watcher, object );
    if ( !isDefined( self.codolRCWatcherRespawn ) )
    {
        self.codolRCWatcherRespawn = true;
        self thread respawnWatcher( watcher );
    }
}

// T5 _weaponobjects::createWeaponObjectWatcher, re-run from the player spawn
// path: with scr_deleteexplosivesonspawn (default 1) it notifies the owner
// "weapon_object_destroyed" and deletes the watcher's objects. _rcbomb's
// updateKillstreakOnDeletion consumes that notify to settle the rule slot.
respawnWatcher( watcher )
{
    self endon( "disconnect" );
    if ( getDvar( "scr_deleteexplosivesonspawn" ) == "" )
        setDvar( "scr_deleteexplosivesonspawn", "1" );
    for ( ;; )
    {
        self waittill( "spawned_player" );
        if ( getDvarInt( "scr_deleteexplosivesonspawn" ) != 1 )
            continue;
        self notify( "weapon_object_destroyed" );
        deleteWeaponObjectArray( watcher );
    }
}

deleteWeaponObjectArray( watcher )
{
    foreach ( object in watcher.objectArray )
    {
        if ( !isDefined( object ) )
            continue;
        if ( isDefined( object.owner ) && object.owner isRemoteControlling() )
            object.owner unlink();
        object delete();
    }
    watcher.objectArray = [];
}

pruneOnDeath( watcher, object )
{
    object waittill( "death" );
    remaining = [];
    foreach ( entry in watcher.objectArray )
        if ( isDefined( entry ) && entry != object )
            remaining[remaining.size] = entry;
    watcher.objectArray = remaining;
}

waitAndDetonate( object, delay, attacker )
{
    if ( !isDefined( object ) )
        return;
    object endon( "death" );
    object endon( "hacked" );
    if ( delay )
        wait delay;
    // T5's deduplication precedes the callback's first yield.
    if ( isDefined( object.detonated ) && object.detonated )
        return;
    object.detonated = true;
    if ( isDefined( attacker ) && isPlayer( attacker ) && isDefined( object.owner ) )
        if ( ( level.teamBased && attacker.team != object.owner.team ) ||
             ( !level.teamBased && attacker != object.owner ) )
            attacker notify( "destroyed_explosive" );
    object [[self.detonate]]( attacker );
}

weaponObjectDamage( watcher )
{
    self endon( "death" );
    self endon( "hacked" );
    self setCanDamage( true );
    self.maxhealth = 100000;
    self.health = self.maxhealth;
    for ( ;; )
    {
        // C1 native notification order is flags THEN weapon; T5 is reversed.
        self waittill( "damage", damage, attacker, direction, point, type,
                       modelName, tagName, partName, flags, weapon );
        if ( !isDefined( attacker ) || !isPlayer( attacker ) )
            continue;
        if ( weapon == "flash_grenade_mp" || weapon == "concussion_grenade_mp" )
        {
            self thread stun( watcher.stunTime );
            continue;
        }
        if ( weapon == "white_phosphorus_grenade_mp" )
            continue;
        // T5 _globallogic_vehicle::AllowFriendlyFireDamage: a vehicle's
        // allowFriendlyFireDamageOverride decides team damage before the
        // default rule. C1's damage notify carries no inflictor entity.
        if ( level.teamBased && !level.hardcoreMode &&
             self.owner.team == attacker.team && self.owner != attacker )
        {
            if ( !isDefined( self.allowFriendlyFireDamageOverride ) ||
                 !self [[ self.allowFriendlyFireDamageOverride ]]( undefined, attacker, type, weapon ) )
                continue;
        }
        attacker maps\mp\gametypes\_damagefeedback::updateDamageFeedback( "" );
        self.wasDamaged = true;
        if ( type == "MOD_GRENADE_SPLASH" || type == "MOD_GRENADE" || type == "MOD_EXPLOSIVE" )
            self.wasChained = true;
        attacker.weaponUsedToDamage = weapon;
        // Preserve T5's delayed destruction; detached struct owns the callback.
        watcher thread waitAndDetonate( self, 0.05, attacker );
        return;
    }
}

stun( duration )
{
    self notify( "codol_rc_stun_restart" );
    self endon( "codol_rc_stun_restart" );
    self endon( "death" );
    self.codolRCFlashUntil = getTime() + int( duration * 1000 );
    self SetClientFlag( level.const_flag_stunned );
    // The original RC jammer watcher also owns this flag. Its combined
    // flash-expiry/jammer predicate performs the clear; clearing here could
    // re-enable controls while the car is still inside an enemy jammer.
}
