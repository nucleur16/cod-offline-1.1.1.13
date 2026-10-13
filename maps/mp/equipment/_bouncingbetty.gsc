// Native CoDOL adaptation of plutoniummod/iw5-scripts _weapons mine lifecycle.
// IW5 source decompiled by xensik/gsc-tool; staged for parent review.
#include maps\mp\_utility;
#include common_scripts\utility;

init()
{
    precacheModel( "projectile_bouncing_betty_grenade" );
    level.codolMineLaunch = loadFx( "impacts/bouncing_betty_launch_dirt" );
    level.codolMineSpin = loadFx( "dust/bouncing_betty_swirl" );
    level.codolMineExplosion = loadFx( "explosions/bouncing_betty_explosion" );
    level thread connected();
}
connected()
{
    level endon( "game_ended" );
    for ( ;; )
    {
        level waittill( "connected", player );
        player.equipmentMines = [];
        player thread monitor();
    }
}
monitor()
{
    self endon( "disconnect" );
    level endon( "game_ended" );
    for ( ;; )
    {
        self waittill( "grenade_fire", missile, weapon );
        if ( weapon == "bouncingbetty_mp" || weapon == "bouncingbetty" )
            missile thread deploy( self );
    }
}
deploy( owner )
{
    self endon( "death" );
    level endon( "game_ended" );
    owner thread watchPending( self );
    self waittill( "stationary" );
    if ( !isDefined( owner ) )
    {
        self delete();
        return;
    }
    trace = bulletTrace( self.origin + (0,0,4), self.origin - (0,0,4), false, self );
    origin = trace["position"];
    if ( trace["fraction"] == 1 )
    {
        origin = getGroundPosition( self.origin, 12, 0, 32 );
        trace["normal"] *= -1;
    }
    mine = spawn( "script_model", origin );
    mine.angles = vectorToAngles( vectorNormalize( trace["normal"] ) ) + (90,0,0);
    mine setModel( "projectile_bouncing_betty_grenade" );
    mine.owner = owner;
    mine.team = owner.pers["team"];
    mine.weaponName = "bouncingbetty_mp";
    mine.health = 100000;
    mine setCanDamage( 1 );
    cleanOwner( owner );
    limit = max( getIntProperty( "scr_maxPerPlayerExplosives", 2 ), 1 );
    if ( owner.equipmentMines.size >= limit )
        destroy( owner.equipmentMines[0] );
    owner.equipmentMines[owner.equipmentMines.size] = mine;
    // IW5 mine deployment uses the same owner/team marker as claymores.
    mine thread maps\mp\gametypes\_weapons::setClaymoreTeamHeadIcon( mine.team );
    mine.pickup = spawn( "script_origin", origin + (0,0,25) );
    mine thread pickup();
    mine thread damage();
    mine thread empDisable();
    mine thread proximity();
    owner thread watchOwner( mine );
    owner thread watchSpawn( mine );
    level thread watchEnd( mine );
    self delete();
}
cleanOwner( owner )
{
    clean = [];
    foreach ( mine in owner.equipmentMines )
        if ( isDefined( mine ) )
            clean[clean.size] = mine;
    owner.equipmentMines = clean;
}
destroy( mine )
{
    // A caller may endon this mine's death. Independent cleanup must own
    // the notification and delete, so the caller cannot interrupt deletion.
    level thread destroyNow( mine );
}
destroyNow( mine )
{
    if ( !isDefined( mine ) ) return;
    mine notify( "death" );
    if ( isDefined( mine.sensor ) ) mine.sensor delete();
    if ( isDefined( mine.pickup ) ) mine.pickup delete();
    owner = mine.owner;
    mine delete();
    if ( isDefined( owner ) ) cleanOwner( owner );
}
watchPending( missile )
{
    missile endon( "death" );
    waittill_any( "disconnect", "joined_team", "joined_spectators" );
    if ( isDefined( missile ) ) missile delete();
}
watchOwner( mine )
{
    mine endon( "death" );
    waittill_any( "disconnect", "joined_team", "joined_spectators" );
    destroy( mine );
}
watchSpawn( mine )
{
    mine endon( "death" );
    self endon( "disconnect" );
    self waittill( "spawned_player" );
    if ( getIntProperty( "scr_deleteexplosivesonspawn", 1 ) == 1 ) destroy( mine );
}
watchEnd( mine )
{
    mine endon( "death" );
    level waittill( "game_ended" );
    destroy( mine );
}
pickup()
{
    pickedUp = self maps\mp\equipment\_pickup::waitForPickup( self.pickup, self.owner, "bouncingbetty_mp", &"WEAPON_BOUNCING_BETTY_PICKUP" );
    if ( isDefined( pickedUp ) && pickedUp )
        destroy( self );
}

damage()
{
    self endon( "death" );
    self endon( "mine_triggered" );
    for ( ;; )
    {
        self waittill( "damage", amount, attacker, direction, point, mod, model, part, tag, flags, weapon );
        if ( !isDefined( attacker ) || !isPlayer( attacker ) ) continue;
        if ( isDefined( weapon ) && (weapon == "bouncingbetty_mp" || weapon == "smoke_grenade_mp" ||
             weapon == "emp_grenade_mp") ) continue;
        if ( !maps\mp\gametypes\_weapons::friendlyFireCheck( self.owner, attacker ) ) continue;
        self maps\mp\equipment\_pickup::close();
        self notify( "mine_destroyed" );
        self thread explode( attacker );
        return;
    }
}
// An EMP grenade disables the mine for its duration instead of destroying it
// (empExplodeWaiter in _weapons.gsc sends emp_damage); proximity ignores
// everyone while self.disabled is set.
empDisable()
{
    self endon( "death" );
    self endon( "mine_triggered" );
    for ( ;; )
    {
        self waittill( "emp_damage", attacker, duration );
        playFx( getfx( "sentry_explode_mp" ), self.origin );
        self.disabled = true;
        self notify( "disabled" );
        wait( duration );
        self.disabled = undefined;
        self notify( "enabled" );
    }
}
proximity()
{
    self endon( "death" );
    self endon( "mine_destroyed" );
    wait 2;
    self.sensor = spawn( "trigger_radius", self.origin, 0, 100, 20 );
    for ( ;; )
    {
        self.sensor waittill( "trigger", player );
        if ( isDefined( self.disabled ) ) continue;
        if ( !isPlayer( player ) || !isAlive( player ) ) continue;
        if ( getDvarInt( "scr_minesKillOwner" ) != 1 )
        {
            if ( player == self.owner ) continue;
            if ( !maps\mp\gametypes\_weapons::friendlyFireCheck( self.owner, player, 0 ) ) continue;
        }
        // Only player recipients are admitted, so native getVelocity is exact.
        if ( lengthSquared( player getVelocity() ) < 10 ) continue;
        if ( player damageConeTrace( self.origin, self ) <= 0 ) continue;
        break;
    }
    self maps\mp\equipment\_pickup::close();
    self notify( "mine_triggered" );
    self.sensor delete();
    self.sensor = undefined;
    if ( isDefined( self.pickup ) ) self.pickup delete();
    self.pickup = undefined;
    self playSound( "mine_betty_click" );
    // C1 Engineer's modDelayTriggeredExplosive on the triggering player (the
    // IW4/IW5 name specialty_delaymine is C1 Stalker Pro). T5's rule: the
    // normal delay, then perk_delayExplosiveTime more.
    wait 0.3;
    if ( player HasAbilityModifier( "modDelayTriggeredExplosive" ) )
    {
        extra = maps\mp\perks\_codol_equipperks::delayExplosiveTime();
        maps\mp\perks\_codol_equipperks::perkLog( "betty " + self getEntityNumber() + " triggered by " + player.name + " (modDelayTriggeredExplosive): +" + extra + "s delay" );
        player notify( "triggered_mine" );
        wait extra;
    }
    self playSound( "mine_betty_spin" );
    playFx( level.codolMineLaunch, self.origin );
    self moveTo( self.origin + (0,0,64), 0.7, 0, 0.65 );
    self rotateVelocity( (0,750,32), 0.7, 0, 0.65 );
    self thread spin();
    wait 0.65;
    self thread explode( self.owner );
}
spin()
{
    self endon( "death" );
    self endon( "exploding" );
    for ( ;; )
    {
        wait 0.05;
        playFxOnTag( level.codolMineSpin, self, "tag_fx_spin1" );
        playFxOnTag( level.codolMineSpin, self, "tag_fx_spin3" );
        wait 0.05;
        playFxOnTag( level.codolMineSpin, self, "tag_fx_spin2" );
        playFxOnTag( level.codolMineSpin, self, "tag_fx_spin4" );
    }
}
explode( attacker )
{
    self endon( "death" );
    self notify( "exploding" );
    self playSound( "grenade_explode_metal" );
    playFxOnTag( level.codolMineExplosion, self, "tag_fx" );
    wait 0.05;
    self hide();
    // C1 shipped Betty WeaponDef radius/inner/outer: 256/100/70.
    self radiusDamage( self.origin, 256, 100, 70, attacker, "MOD_EXPLOSIVE", "bouncingbetty_mp" );
    wait 0.2;
    destroy( self );
}
// REQUIRED damage callback integration: reject outside this vertical band,
// then halve accepted damage for crouch/prone as IW5 _damage does.
damageHeightPassed( inflictor, victim )
{
    if ( !isDefined( inflictor ) ) return true;
    center = victim.origin;
    if ( isPlayer( victim ) && isAlive( victim ) )
    {
        stance = victim getStance();
        if ( stance == "prone" ) center += (0,0,10);
        else if ( stance == "crouch" ) center += (0,0,24);
        else center += (0,0,32);
    }
    else if ( victim.classname == "misc_turret" ) center += (0,0,32);
    return abs( center[2] - inflictor.origin[2] ) <= 46;
}
