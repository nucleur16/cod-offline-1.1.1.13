// CoDOL combat-training bots: placed equipment.
//
// Grenade throwing and planting run natively (src/codol/player/
// codol_bot_offhand.c, after OpenBLOPS sv_bot_mp.cpp Bot_UpdateGrenadeThrow
// and T5 maps/mp/gametypes/_bot.gsc bot_equipment_think): lethal and tactical
// grenades go through the real C1 offhand buttons at enemies the bot sees or
// has just lost around a corner, and placed equipment goes down at the
// waypoint graph's authored "claymore" spots or at route corners.
//
// This script adds the two parts that need gametype knowledge:
//  - mp/botclasstable.csv leaves every bot class's equipment slot
//    (loadoutTactical) empty, so a bot carries no placed equipment of its own.
//    scr_codol_botequipment 1 (default) gives each bot a claymore or a
//    bouncing betty in that otherwise empty slot, whichever this map mounts.
//  - Near an objective (bomb site, domination/HQ flag) an idle bot plants its
//    equipment facing away from it, toward where attackers come from.
#include common_scripts\utility;

onBotSpawned()
{
    self notify( "codol_bot_equipment" );
    self endon( "codol_bot_equipment" );
    self endon( "disconnect" );
    self endon( "death" );
    level endon( "game_ended" );
    if ( getDvar( "scr_codol_botequipment" ) == "" )
        setDvar( "scr_codol_botequipment", 1 );
    // giveLoadout runs before "spawned_player"; let its equipment settle.
    wait 0.1;
    if ( getDvarInt( "scr_codol_botequipment" ) <= 0 )
        return;
    weapon = self giveBotEquipment();
    if ( !isDefined( weapon ) )
        return;
    self thread objectivePlantThink( weapon );
}

giveBotEquipment()
{
    current = self getOffhandEquipment( 2 );
    if ( isDefined( current ) && current != "none" && current != "" )
        return current;
    choices = [];
    choices[0] = "claymore_mp";
    choices[1] = "bouncingbetty_mp";
    classIndex = 0;
    if ( isDefined( self.codolBotClass ) )
        classIndex = self.codolBotClass;
    for ( i = 0; i < choices.size; i++ )
    {
        weapon = choices[( classIndex + i ) % choices.size];
        if ( !codol_isWeaponMounted( weapon ) )
            continue;
        self giveWeapon( weapon );
        self giveStartAmmo( weapon );
        if ( !self hasWeapon( weapon ) || self getAmmoCount( weapon ) <= 0 )
        {
            // Shares a full ammo pool with an offhand already held (see the
            // flare_mp note in _codol_bots testTacticalInsertion).
            if ( self hasWeapon( weapon ) )
                self takeWeapon( weapon );
            continue;
        }
        self setOffhandEquipment( self getOffhandEquipment( 0 ), self getOffhandEquipment( 1 ), weapon );
        if ( getDvarInt( "developer" ) > 0 )
            codolPrintLn( "CoDOL bot equipment: " + self.name + " carries " + weapon + " x" + self getAmmoCount( weapon ) );
        return weapon;
    }
    return undefined;
}

objectiveSpots()
{
    spots = [];
    // Team deathmatch and free-for-all leave other modes' objective entities
    // in the map; they mark nothing worth holding there.
    if ( isDefined( level.gameType ) && ( level.gameType == "war" || level.gameType == "dm" ) )
        return spots;
    names = [];
    names[0] = "bombzone";
    names[1] = "flag_primary";
    names[2] = "flag_secondary";
    names[3] = "radiotrigger";
    foreach ( name in names )
        foreach ( ent in getEntArray( name, "targetname" ) )
            spots[spots.size] = ent.origin;
    return spots;
}

equipmentNear( point, radius )
{
    foreach ( grenade in getEntArray( "grenade", "classname" ) )
        if ( isDefined( grenade ) && distanceSquared( grenade.origin, point ) < radius * radius )
            return true;
    return false;
}

objectivePlantThink( weapon )
{
    self endon( "disconnect" );
    self endon( "death" );
    level endon( "game_ended" );
    spots = objectiveSpots();
    if ( spots.size == 0 )
        return;
    planted = [];
    for ( ;; )
    {
        wait randomFloatRange( 3, 5 );
        if ( !self hasWeapon( weapon ) || self getAmmoCount( weapon ) <= 0 )
            return;
        if ( !self codol_botIsIdle() || !self isOnGround() )
            continue;
        for ( i = 0; i < spots.size; i++ )
        {
            if ( isDefined( planted[i] ) )
                continue;
            spot = spots[i];
            if ( distance2D( self.origin, spot ) > 450 || distance2D( self.origin, spot ) < 96 )
                continue;
            if ( equipmentNear( self.origin, 192 ) )
                continue;
            // Face away from the objective, down the approach to it.
            away = vectorNormalize( ( self.origin[0] - spot[0], self.origin[1] - spot[1], 0 ) );
            if ( self codol_botThrowOffhand( 2, self.origin + away * 256 ) )
            {
                planted[i] = true;
                if ( getDvarInt( "developer" ) > 0 )
                    codolPrintLn( "CoDOL bot equipment: " + self.name + " plants " + weapon + " near objective " + spot );
            }
            break;
        }
    }
}
