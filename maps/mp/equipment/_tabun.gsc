// CoDOL adaptation of the pinned T5 maps/mp/_tabun.gsc.
// Keep native grenade physics/explosion FX/sound; this owns the gas area.
#include maps\mp\_utility;
#include common_scripts\utility;

init()
{
    if ( isDefined( level.codolGasClouds ) ) return;
    precacheShellShock( "tabun_gas_mp" );
    precacheShellShock( "tabun_gas_nokick_mp" );
    // T5 plays the cloud through five FX-only weapons (tabun_center_mp and
    // tabun_tiny/small/medium/large_mp) with SpawnTimedFX. C1 ships those
    // weapons in common_gametype_mp, each carrying one effect as its
    // projExplosionEffect (+7b8), and registers the same five effects in
    // cgMedia (C1 0x44f07d). Stream order of common_gametype_mp pairs them:
    // tiny _25r, small _50r, medium _75r, large _125r, center unsized.
    level.codolGasCenterFX = loadFx( "weapon/grenade/fx_gas_poison_mp" );
    level.codolGasSizeFX = [];
    level.codolGasSizeFX[0] = loadFx( "weapon/grenade/fx_gas_poison_mp_25r" );
    level.codolGasSizeFX[1] = loadFx( "weapon/grenade/fx_gas_poison_mp_50r" );
    level.codolGasSizeFX[2] = loadFx( "weapon/grenade/fx_gas_poison_mp_75r" );
    level.codolGasSizeFX[3] = loadFx( "weapon/grenade/fx_gas_poison_mp_125r" );
    level.codolGasClouds = [];
    level.codolGasEnded = false;
    level.codolGasDuration = setting( "scr_tabunGasDuration", 8 );
    level.codolPoisonDuration = setting( "scr_poisonDuration", 8 );
    level.codolPoisonDamage = setting( "scr_poisonDamage", 13 );
    level.codolPoisonDamageHardcore = setting( "scr_poisonDamageHardcore", 5 );
    level.codolGasRadius = setting( "scr_tabun_effect_radius", 185 );
    level.codolGasHeight = setting( "scr_tabun_shock_height", 20 );
    level.codolGasRadii = [];
    level.codolGasRadii[0] = setting( "scr_fx_tabun_radius0", 55 );
    level.codolGasRadii[1] = setting( "scr_fx_tabun_radius1", 55 );
    level.codolGasRadii[2] = setting( "scr_fx_tabun_radius2", 50 );
    level.codolGasRadii[3] = setting( "scr_fx_tabun_radius3", 25 );
    // scr_tabun_trace 1 logs each throw, cloud and poison tick.
    level.codolGasTrace = getDvarInt( "scr_tabun_trace" );
    level thread connected();
    level thread endGame();
}

setting( name, fallback )
{
    if ( getDvar( name ) == "" )
        setDvar( name, fallback );
    return max( 0, getDvarInt( name ) );
}

connected()
{
    level endon( "game_ended" );
    for ( ;; )
    {
        level waittill( "connected", player );
        // A single connection-owned worker controls exposure, HUD and damage.
        // Death does not terminate it; no old death worker can restore a new HUD.
        player thread throws();
        player thread exposure();
    }
}

throws()
{
    self endon( "disconnect" );
    level endon( "game_ended" );
    for ( ;; )
    {
        self waittill( "grenade_fire", missile, weapon );
        if ( weapon == "tabun_gas_mp" )
        {
            gasTrace( "throw by " + self.name );
            missile thread detonation( self );
        }
    }
}

detonation( owner )
{
    // No endon( "death" ): G_ExplodeMissile notifies "explode" and then
    // "death" in the same frame, and the death endon kills this thread before
    // the explode waittill resumes, so no cloud ever formed. T5
    // watchTabunGrenadeDetonation has no death endon either.
    level endon( "game_ended" );
    self waittill( "explode", position );
    gasTrace( "explode at " + position );
    // Retained explode notification carries position only. Query the same
    // native water contents used by G_ExplodeMissile, not a terrain name guess.
    if ( ( !isDefined( level.water_duds ) || level.water_duds ) && codol_isPointInWater( position ) )
        return;
    if ( !isDefined( owner ) )
        return;
    cloud = spawnStruct();
    cloud.owner = owner;
    cloud.areas = [];
    cloud.fxs = [];
    cloud.expires = getTime() + 1000 * level.codolGasDuration;
    cloud.removeAt = getTime() + 1000 * max( level.codolGasDuration, level.codolPoisonDuration );
    cloud.origin = position;
    cloud.killcam = spawn( "script_model", position + (0,0,60) );
    cloud.killcam.startTime = getTime();
    // _damage reads birthtime as the killcam entity's start (T5 killcam).
    cloud.killcam.birthtime = cloud.killcam.startTime;
    buildAreas( cloud, position + (0,0,12) );
    // T5 _tabun.gsc:111-112 stuns dogs caught in the cloud when it forms.
    foreach ( area in cloud.areas )
        owner thread maps\mp\_dogs::flash_dogs( area );
    level.codolGasClouds[level.codolGasClouds.size] = cloud;
    // C1 tabun_gas_mp has NULL explosion FX at +7b8/+7bc, so script owns the
    // shipped poison effects (buildAreas). Native +7c0 owns tabun_gas_explode;
    // do not double its sound. T5's wpn_gas_hiss_* aliases ship only in the
    // zombies zones, so MP plays no hiss loop.
    gasTrace( "cloud at " + position + " areas " + cloud.areas.size + " fx " + cloud.fxs.size );
    level thread expire( cloud );
}

gasTrace( text )
{
    if ( level.codolGasTrace )
        codolPrintLn( "CoDOL tabun: " + text );
}

// T5 SpawnTimedFX: the effect lives until the cloud is removed.
addFx( cloud, fx, origin )
{
    ent = spawnFx( fx, origin );
    triggerFx( ent );
    cloud.fxs[cloud.fxs.size] = ent;
}

addArea( cloud, origin, radius, height )
{
    if ( radius <= 0 || height <= 0 )
        return;
    area = spawn( "trigger_radius", origin, 0, radius, height );
    gasTrace( "area at " + origin + " radius " + radius + " height " + height );
    cloud.areas[cloud.areas.size] = area;
}

buildAreas( cloud, start )
{
    if ( setting( "scr_enable_new_tabun", 1 ) == 0 )
    {
        addWhole( cloud, start-(0,0,12) );
        return;
    }
    // T5 eight wall-clipped spokes, including its authored 0.707 diagonals.
    dirs = [];
    dirs[0]=(1,0,0); dirs[1]=(.707,.707,0); dirs[2]=(0,1,0); dirs[3]=(-.707,.707,0);
    dirs[4]=(-1,0,0); dirs[5]=(-.707,-.707,0); dirs[6]=(0,-1,0); dirs[7]=(.707,-.707,0);
    points=[]; spans=[]; sizes=[]; centroid=(0,0,0);
    for ( i=0; i<8; i++ )
    {
        trace=bulletTrace( start, start+dirs[i]*220, false, undefined );
        spans[i]=distanceSquared( start, trace["position"] );
        points[i]=(start+trace["position"])*.5-(0,0,12);
        centroid+=points[i]*.125;
    }
    whole=true; freePass=false;
    for ( i=0; i<8; i++ )
    {
        sizes[i]=classify( spans[i], spans[(i+7)%8], spans[(i+1)%8] );
        // T5 spawnAllLocs compares the chosen radii, not the size classes:
        // with the default radius0 == radius1 every size 0 or 1 spoke passes,
        // and one spoke of radius1 is forgiven when the radii differ.
        radius=0;
        if ( sizes[i]>=0 ) radius=level.codolGasRadii[sizes[i]];
        if ( radius == level.codolGasRadii[0] )
            continue;
        if ( radius == level.codolGasRadii[1] && !freePass )
            freePass=true;
        else
            whole=false;
    }
    if ( whole )
    {
        addWhole( cloud, start-(0,0,12) );
        return;
    }
    // T5: the killcam sits over the spokes' centroid; the large effect plays
    // at the centre and each surviving spoke gets its sized effect and area.
    cloud.killcam.origin=centroid+(0,0,60);
    addFx( cloud, level.codolGasSizeFX[3], start-(0,0,12) );
    for ( i=0; i<8; i++ )
        if ( sizes[i]>=0 )
        {
            addFx( cloud, level.codolGasSizeFX[sizes[i]], points[i] );
            addArea( cloud, points[i], level.codolGasRadii[sizes[i]], level.codolGasRadii[sizes[i]] );
        }
}

// T5 singleLocation: one centre effect and one 185 x 20 area.
addWhole( cloud, origin )
{
    cloud.killcam.origin=origin+(0,0,60);
    addFx( cloud, level.codolGasCenterFX, origin );
    addArea( cloud, origin, level.codolGasRadius, level.codolGasHeight );
}

classify( span, previous, next )
{
    r=level.codolGasRadii;
    if ( span>r[0]*r[0] && previous>r[1]*r[1] && next>r[1]*r[1] ) return 0;
    if ( span>r[1]*r[1] && previous>r[2]*r[2] && next>r[2]*r[2] ) return 1;
    if ( span>r[2]*r[2] && previous>r[3]*r[3] && next>r[3]*r[3] ) return 2;
    // T5 really compares adjacent squared distances to unsquared radius here.
    if ( span>r[3]*r[3] && previous>r[3] && next>r[3] ) return 3;
    return -1;
}

expire( cloud )
{
    while ( !level.codolGasEnded && getTime()<cloud.removeAt && isDefined( cloud.owner ) )
        wait .1;
    gasTrace( "cloud at " + cloud.origin + " removed" );
    removeCloud( cloud );
}

removeCloud( cloud )
{
    if ( isDefined( cloud.removed ) ) return;
    cloud.removed=true;
    foreach ( area in cloud.areas )
        if ( isDefined( area ) ) area delete();
    cloud.areas=[];
    if ( isDefined( cloud.killcam ) ) cloud.killcam delete();
    foreach ( fx in cloud.fxs )
        if ( isDefined( fx ) ) fx delete();
    cloud.fxs=[];
    remaining=[];
    foreach ( other in level.codolGasClouds )
        if ( other!=cloud ) remaining[remaining.size]=other;
    level.codolGasClouds=remaining;
}

findExposure()
{
    if ( !isAlive( self ) || self.sessionstate!="playing" ) return undefined;
    // CoDOL perk table upgrade2 ID20 supplies modGasMask. T5's perk name
    // specialty_gas_mask does not exist in this game's selected perk table.
    if ( self hasAbilityModifier( "modGasMask" ) ) return undefined;
    foreach ( cloud in level.codolGasClouds )
    {
        if ( isDefined( cloud.removed ) || !isDefined( cloud.owner ) ) continue;
        continuing=isDefined( self.codolGasCloud ) && self.codolGasCloud==cloud;
        if ( getTime()>=cloud.expires && !continuing ) continue;
        if ( level.friendlyfire==0 && self!=cloud.owner )
        {
            if ( !isDefined( cloud.owner.team ) ) continue;
            if ( level.teambased && self.team==cloud.owner.team ) continue;
        }
        foreach ( area in cloud.areas )
        {
            if ( getTime()>=cloud.expires && ( !isDefined( self.codolGasArea ) || self.codolGasArea!=area ) ) continue;
            if ( !isDefined( area ) || !self isTouching( area ) ) continue;
            trace=bulletTrace( area.origin, self.origin+(0,0,12), false, self );
            if ( trace["fraction"]==1 )
            {
                result=spawnStruct(); result.cloud=cloud; result.area=area;
                return result;
            }
        }
    }
    return undefined;
}

exposure()
{
    self endon( "disconnect" );
    // Replace an older connection monitor synchronously, before publishing
    // the new worker's identity. Its delayed cleanup cannot restore this HUD.
    self notify( "codol_tabun_monitor" );
    self endon( "codol_tabun_monitor" );
    self clearExposure();
    worker=spawnStruct();
    self.codolGasWorker=worker;
    self thread transitions( worker );
    self.codolGasHUD=false;
    self.codolGasPoison=false;
    nextDamage=0; shock=0; clearAt=0;
    while ( !level.codolGasEnded )
    {
        hit=self findExposure();
        if ( isDefined( hit ) )
        {
            clearAt=0;
            self.codolGasCloud=hit.cloud;
            self.codolGasArea=hit.area;
            if ( !self.codolGasPoison )
            {
                self startPoisoning(); self.codolGasPoison=true;
            }
            if ( getTime()>=nextDamage )
            {
                nextDamage=getTime()+1000;
                damage=level.codolPoisonDamage;
                if ( level.hardcoreMode ) damage=level.codolPoisonDamageHardcore;
                self.lastPoisonedBy=hit.cloud.owner;
                gasTrace( "poison " + self.name + " (" + self.team + ") health " + self.health + " damage " + damage + " by " + hit.cloud.owner.name );
                self DoDamage( damage, hit.area.origin, hit.cloud.owner, hit.cloud.killcam, 0, "MOD_GAS", 0, "tabun_gas_mp" );
                if ( isAlive( self ) && self codol_mayApplyScreenEffect() )
                {
                    if ( shock==0 ) self shellShock( "tabun_gas_mp", 1 );
                    else self shellShock( "tabun_gas_nokick_mp", 1 );
                    shock=1-shock;
                    if ( !self.codolGasHUD )
                    {
                        self setClientDvar( "hud_drawHud", 0 );
                        self.codolGasHUD=true;
                    }
                }
            }
        }
        else if ( self.codolGasPoison )
        {
            if ( clearAt==0 ) clearAt=getTime()+1000;
            if ( getTime()>=clearAt )
            {
                gasTrace( "clear " + self.name );
                self clearExposure( worker ); nextDamage=0; shock=0;
            }
        }
        if ( !isAlive( self ) || self.sessionstate!="playing" )
        {
            self clearExposure( worker ); nextDamage=0; clearAt=0; shock=0;
        }
        wait .1;
    }
    self clearExposure( worker );
}

clearExposure( worker )
{
    if ( isDefined( worker ) && ( !isDefined( self.codolGasWorker ) || self.codolGasWorker!=worker ) ) return;
    if ( isDefined( self.codolGasPoison ) && self.codolGasPoison )
    {
        self stopPoisoning(); self.codolGasPoison=false;
    }
    if ( isDefined( self.codolGasHUD ) && self.codolGasHUD )
    {
        self setClientDvar( "hud_drawHud", 1 ); self.codolGasHUD=false;
    }
    self.codolGasCloud=undefined;
    self.codolGasArea=undefined;
}

transitions( worker )
{
    self endon( "disconnect" );
    self endon( "codol_tabun_monitor" );
    level endon( "game_ended" );
    for ( ;; )
    {
        self waittill_any( "death", "spawned_player" );
        self clearExposure( worker );
    }
}

endGame()
{
    level waittill( "game_ended" );
    level.codolGasEnded=true;
    foreach ( player in getEntArray( "player", "classname" ) )
        player clearExposure();
    clouds=level.codolGasClouds;
    foreach ( cloud in clouds ) removeCloud( cloud );
}
