// CoDOL combat-training bots: call in earned killstreaks.
//
// Behaviour adapted from Treyarch's T5 maps/mp/gametypes/_bot.gsc
// (bot_killstreak_think, bot_killstreak_location, bot_use_supply_drop,
// bot_turret_location, bot_crate_think; Black Ops 1 GSC dump). Every reward is
// used through the real dispatcher the way a C1 player uses it: the action
// slot is pressed ("codol_reward_slot_N", which names the record) and the
// reward's activation weapon is raised, so _killstreaks::killstreakUseWaiter
// sees the native weapon_change and runs codolActivateReward ->
// killstreakUsePressed -> the reward's own use function. Location rewards are
// answered with "confirm_location" as the C1 map selector answers them,
// markers are thrown with the real fire button (native codol_botFireAt), and
// carried turrets are put down with "place_sentry" once the placement check
// allows it. Nothing here grants an effect directly.
//
// Ride rewards (helicopter gunner/MK19, AC-130, predator missile, RC-XD) are
// left owned and never used: T5 removes the gunner rewards from its bots
// instead of driving them, and a bot cannot steer a rideable missile or car
// through C1's remote-control input here. scr_botkillstreaks 0 turns
// the whole behaviour off.
#include maps\mp\_utility;
#include common_scripts\utility;

onBotSpawned()
{
    self notify( "codol_bot_killstreaks" );
    self endon( "codol_bot_killstreaks" );
    self endon( "disconnect" );
    self endon( "death" );
    level endon( "game_ended" );
    if ( getDvar( "scr_botkillstreaks" ) == "" )
        setDvar( "scr_botkillstreaks", 1 );
    if ( !isDefined( self.codolBotStreakReady ) )
    {
        self.codolBotStreakReady = [];
        self.codolBotStreakTries = [];
    }
    self thread botCrateThink();
    wait 1;
    for ( ;; )
    {
        wait randomFloatRange( 1.0, 2.0 );
        if ( getDvarInt( "scr_botkillstreaks" ) <= 0 )
            continue;
        reward = self pickReward();
        if ( !isDefined( reward ) )
            continue;
        // A short, natural pause between earning a reward and calling it in.
        if ( !isDefined( self.codolBotStreakReady[reward.kID] ) )
        {
            self.codolBotStreakReady[reward.kID] = getTime() + randomIntRange( 1500, 4000 );
            continue;
        }
        if ( getTime() < self.codolBotStreakReady[reward.kID] )
            continue;
        kind = rewardKind( reward.streakName );
        if ( !self rewardUsableNow( kind, getTime() - self.codolBotStreakReady[reward.kID] ) )
            continue;
        self.codolBotStreakTries[reward.kID] = self.codolBotStreakTries[reward.kID] + 1;
        self useReward( reward, kind );
        // T5's crazy fail-safe: never stay on a reward weapon.
        self returnFromRewardWeapon( reward.streakName );
    }
}

// The newest owned reward a bot can use (pers["killstreaks"][0] is the newest).
pickReward()
{
    if ( !isDefined( self.pers["killstreaks"] ) )
        return undefined;
    for ( i = 0; i < self.pers["killstreaks"].size; i++ )
    {
        reward = self.pers["killstreaks"][i];
        if ( !isDefined( reward ) || !isDefined( reward.kID ) || !isDefined( reward.streakName ) )
            continue;
        if ( !isDefined( level.killstreakFuncs[reward.streakName] ) )
            continue;
        if ( rewardKind( reward.streakName ) == "ride" )
            continue;
        if ( !isDefined( self.codolBotStreakTries[reward.kID] ) )
            self.codolBotStreakTries[reward.kID] = 0;
        if ( self.codolBotStreakTries[reward.kID] >= 3 )
            continue;
        return reward;
    }
    return undefined;
}

rewardKind( streakName )
{
    if ( maps\mp\killstreaks\_killstreaks::isRideKillstreak( streakName ) )
        return "ride";
    if ( maps\mp\killstreaks\_killstreaks::isCarryKillstreak( streakName ) )
        return "carry";
    base = maps\mp\killstreaks\_killstreaks::getKillstreakBaseRef( streakName );
    switch ( base )
    {
        case "airstrike":
        case "precision_airstrike":
        case "super_airstrike":
        case "harrier_airstrike":
        case "stealth_airstrike":
            return "locate";
        case "mortar_mp":
            return "mortar";
        case "airdrop":
        case "airdrop_mega":
        case "airdrop_sentry_minigun":
        case "airdrop_sam_turret":
        case "airdrop_predator_missile":
            return "marker";
        default:
            return "instant";
    }
}

// T5 bot_is_idle before a supply drop, a turret or a laptop: not mid-fight.
// A bot that stays busy for 15 s calls its reward in anyway.
rewardUsableNow( kind, waited )
{
    if ( !isAlive( self ) || self isUsingRemote() || isDefined( self.selectingLocation ) )
        return false;
    if ( isDefined( self.codolRewardActivationBusy ) && self.codolRewardActivationBusy )
        return false;
    if ( !self isWeaponEnabled() || self codol_botOffhandBusy() )
        return false;
    if ( isDefined( self.lastStand ) )
        return false;
    // Planting, defusing or capturing: raising the reward weapon drops the
    // use weapon, and _gameobjects::useHoldThinkLoop gives up on a use whose
    // weapon is not raised within 1.5 s, so a bomb carrier who called in a
    // UAV on the site never planted.
    if ( isDefined( self.claimTrigger ) || self useButtonPressed() )
        return false;
    if ( kind == "instant" )
        return true;
    if ( !self isOnGround() )
        return false;
    return self codol_botIsIdle() || waited > 15000;
}

// Press the reward's action slot and raise its weapon, as C1 does: the slot
// press names the record (codolRewardActionWaiter), the weapon_change uses it.
activateReward( reward )
{
    weapon = maps\mp\killstreaks\_killstreaks::getKillstreakWeapon( reward.streakName );
    if ( !isDefined( weapon ) || weapon == "" || !self hasWeapon( weapon ) )
        return false;
    if ( isDefined( reward.actionSlot ) )
        self notify( "codol_reward_slot_" + reward.actionSlot );
    // The bot's usercmd names the requested weapon (codol_bot.c), so the
    // engine drops the held weapon and raises this one as it does for C1.
    if ( self getCurrentWeapon() != weapon )
        self switchToWeapon( weapon );
    return true;
}

// T5 bot_killstreak_location waits for "weapon_change" before it freezes
// the bot: the engine (like C1 PM_Weapon_CheckForChange 0x41b119) starts no
// weapon switch while the controls are frozen.
waitForRewardWeapon( weapon, seconds )
{
    for ( i = 0; i < seconds * 20 && self getCurrentWeapon() != weapon; i++ )
        wait 0.05;
    return self getCurrentWeapon() == weapon;
}

rewardOwned( kID )
{
    foreach ( reward in self.pers["killstreaks"] )
        if ( isDefined( reward ) && reward.kID == kID )
            return true;
    return false;
}

useReward( reward, kind )
{
    streakName = reward.streakName;
    kID = reward.kID;
    if ( getDvarInt( "developer" ) > 0 )
        codolPrintLn( "CoDOL bot killstreak: " + self.name + " calls in " + streakName + "#" + kID + " (" + kind + ")" );
    if ( kind == "locate" || kind == "mortar" )
    {
        self useLocationReward( reward, kind );
        return;
    }
    if ( kind == "marker" )
    {
        self useMarkerReward( reward );
        return;
    }
    if ( !self activateReward( reward ) )
        return;
    if ( kind == "carry" )
    {
        self placeCarriedReward();
        return;
    }
    // Instant rewards: the dispatcher consumes the record when the use returns.
    for ( i = 0; i < 40 && self rewardOwned( kID ); i++ )
        wait 0.1;
}

returnFromRewardWeapon( streakName )
{
    if ( !isAlive( self ) || isDefined( self.selectingLocation ) || self isUsingRemote() )
        return;
    if ( isDefined( self.codolRewardActivationBusy ) && self.codolRewardActivationBusy )
        return;
    current = self getCurrentWeapon();
    if ( !isDefined( level.killstreakWeapons[current] ) )
        return;
    primary = self maps\mp\killstreaks\_killstreaks::getFirstPrimaryWeapon();
    if ( isDefined( primary ) && self hasWeapon( primary ) )
        self switchToWeapon( primary );
}

// ---- location rewards (T5 bot_killstreak_location) -----------------------

useLocationReward( reward, kind )
{
    self endon( "death" );
    count = 1;
    if ( kind == "mortar" )
        count = 3;
    targets = self pickStrikeTargets( count );
    if ( targets.size == 0 )
        return;
    if ( !self activateReward( reward ) )
        return;
    weapon = maps\mp\killstreaks\_killstreaks::getKillstreakWeapon( reward.streakName );
    if ( !self waitForRewardWeapon( weapon, 3 ) )
        return;
    // T5 freezes the bot while it works the map.
    self freezeControls( true );
    self thread unfreezeAfter( 10 );
    for ( i = 0; i < 20 && !isDefined( self.selectingLocation ); i++ )
        wait 0.1;
    if ( !isDefined( self.selectingLocation ) )
    {
        self notify( "codol_bot_unfreeze" );
        return;
    }
    wait randomFloatRange( 1.0, 2.0 );
    for ( i = 0; i < count && isDefined( self.selectingLocation ); i++ )
    {
        target = targets[i % targets.size];
        if ( i >= targets.size )
            target = target + ( randomIntRange( -256, 256 ), randomIntRange( -256, 256 ), 0 );
        yaw = vectorToAngles( target - self.origin )[1];
        if ( getDvarInt( "developer" ) > 0 )
            codolPrintLn( "CoDOL bot killstreak: " + self.name + " targets " + target + " yaw " + yaw + " for " + reward.streakName );
        self notify( "confirm_location", target, yaw );
        wait 0.4;
    }
    self notify( "codol_bot_unfreeze" );
}

unfreezeAfter( seconds )
{
    self endon( "disconnect" );
    self waittill_any_timeout( seconds, "codol_bot_unfreeze", "death" );
    self freezeControls( false );
}

// Areas with the most enemies and the fewest friends, best first. T5 picks a
// random enemy and scatters by 512; this aims at the densest group instead.
pickStrikeTargets( count )
{
    radius = 450;
    scored = [];
    foreach ( enemy in level.players )
    {
        if ( !self isStrikeEnemy( enemy ) )
            continue;
        score = 0;
        sum = ( 0, 0, 0 );
        n = 0;
        foreach ( other in level.players )
        {
            if ( !isAlive( other ) || other.sessionstate != "playing" )
                continue;
            if ( distance2D( other.origin, enemy.origin ) > radius )
                continue;
            if ( self isStrikeEnemy( other ) )
            {
                score += 1;
                sum = sum + other.origin;
                n++;
            }
            else
                score -= 1.5;
        }
        if ( n == 0 || score <= 0 )
            continue;
        entry = spawnStruct();
        entry.score = score + randomFloat( 0.5 );
        entry.origin = sum * ( 1.0 / n ) + ( randomIntRange( -96, 96 ), randomIntRange( -96, 96 ), 0 );
        scored[scored.size] = entry;
    }
    // Best first; a target on top of a better one adds nothing.
    targets = [];
    while ( targets.size < count && scored.size > 0 )
    {
        bestIndex = 0;
        for ( i = 1; i < scored.size; i++ )
            if ( scored[i].score > scored[bestIndex].score )
                bestIndex = i;
        entry = scored[bestIndex];
        keep = [];
        for ( i = 0; i < scored.size; i++ )
            if ( i != bestIndex )
                keep[keep.size] = scored[i];
        scored = keep;
        distinct = true;
        foreach ( chosen in targets )
            if ( distance2D( chosen, entry.origin ) < 300 )
                distinct = false;
        if ( distinct )
            targets[targets.size] = entry.origin;
    }
    return targets;
}

isStrikeEnemy( player )
{
    if ( !isDefined( player ) || player == self || !isAlive( player ) || player.sessionstate != "playing" )
        return false;
    if ( !isDefined( player.team ) || player.team == "spectator" )
        return false;
    if ( level.teamBased && player.team == self.team )
        return false;
    return true;
}

// ---- care packages (T5 bot_use_supply_drop) -----------------------------

useMarkerReward( reward )
{
    self endon( "death" );
    weapon = maps\mp\killstreaks\_killstreaks::getKillstreakWeapon( reward.streakName );
    drop = self findDropPoint();
    if ( !isDefined( drop ) )
        return;
    if ( !self activateReward( reward ) )
        return;
    for ( i = 0; i < 30 && self getCurrentWeapon() != weapon; i++ )
        wait 0.1;
    if ( self getCurrentWeapon() != weapon )
        return;
    self.codolBotMarkerThrown = undefined;
    self thread watchMarkerThrow( weapon );
    for ( attempt = 0; attempt < 2 && !isDefined( self.codolBotMarkerThrown ); attempt++ )
    {
        // The marker leaves along the bot's view; codol_botFireAt turns to the
        // drop point on a grenade arc and pulls the trigger once.
        if ( !self codol_botFireAt( drop ) )
        {
            wait 0.5;
            continue;
        }
        for ( i = 0; i < 30 && !isDefined( self.codolBotMarkerThrown ); i++ )
            wait 0.1;
    }
    self notify( "codol_bot_marker_watch_end" );
    if ( isDefined( self.codolBotMarkerThrown ) )
        self.codolBotCrateWanted = getTime() + 30000;
}

watchMarkerThrow( weapon )
{
    self endon( "disconnect" );
    self endon( "death" );
    self endon( "codol_bot_marker_watch_end" );
    for ( ;; )
    {
        self waittill( "grenade_fire", grenade, weapname );
        if ( weapname == weapon )
        {
            self.codolBotMarkerThrown = true;
            return;
        }
    }
}

// Open ground under open sky, 250..400 units out, that the bot can see.
findDropPoint()
{
    eye = self getEye();
    baseYaw = self.angles[1];
    for ( step = 0; step < 8; step++ )
    {
        yaw = baseYaw + step * 45;
        forward = anglesToForward( ( 0, yaw, 0 ) );
        for ( dist = 400; dist >= 250; dist -= 150 )
        {
            point = self.origin + forward * dist + ( 0, 0, 32 );
            if ( !sightTracePassed( eye, point, false, self ) )
                continue;
            if ( !sightTracePassed( point, point + ( 0, 0, 2048 ), false, undefined ) )
                continue;
            ground = bulletTrace( point, point - ( 0, 0, 256 ), false, self );
            if ( ground["fraction"] >= 1 || abs( ground["position"][2] - self.origin[2] ) > 96 )
                continue;
            return ground["position"];
        }
    }
    return undefined;
}

// T5 bot_crate_think: go open care packages. A bot always goes for its own
// (it called it in); other crates within 1024 units sometimes, a teammate's
// only rarely.
botCrateThink()
{
    self endon( "disconnect" );
    self endon( "death" );
    level endon( "game_ended" );
    for ( ;; )
    {
        wait randomFloatRange( 3, 5 );
        if ( getDvarInt( "scr_botkillstreaks" ) <= 0 || self isUsingRemote() || isDefined( self.selectingLocation ) )
            continue;
        crate = self pickCrate();
        if ( !isDefined( crate ) )
            continue;
        // Someone else's crate only between fights; its own crate the bot
        // fetches as soon as it lands, as a player who called it in does.
        if ( ( !isDefined( crate.owner ) || crate.owner != self ) && !self codol_botIsIdle() )
            continue;
        self openCrate( crate );
    }
}

pickCrate()
{
    best = undefined;
    foreach ( crate in getEntArray( "care_package", "targetname" ) )
    {
        if ( !isDefined( crate ) || !isDefined( crate.crateType ) )
            continue;
        if ( isDefined( crate.codolBotClaim ) && isDefined( crate.codolBotClaimTime ) && getTime() < crate.codolBotClaimTime )
            continue;
        // A crate that came to rest on a roof or a ledge is out of reach.
        if ( abs( crate.origin[2] - self.origin[2] ) > 160 )
            continue;
        mine = isDefined( crate.owner ) && crate.owner == self;
        if ( !mine )
        {
            if ( distanceSquared( self.origin, crate.origin ) > 1024 * 1024 )
                continue;
            if ( level.teamBased && isDefined( crate.owner ) && isDefined( crate.owner.team ) && crate.owner.team == self.team && randomInt( 100 ) > 30 )
                continue;
            if ( randomInt( 100 ) > 60 )
                continue;
        }
        if ( !isDefined( best ) || distanceSquared( self.origin, crate.origin ) < distanceSquared( self.origin, best.origin ) )
            best = crate;
    }
    return best;
}

openCrate( crate )
{
    self endon( "disconnect" );
    self endon( "death" );
    // Still under the helicopter or falling: wait for it to settle first.
    start = crate.origin;
    wait 0.5;
    if ( !isDefined( crate ) || distanceSquared( start, crate.origin ) > 16 * 16 )
        return;
    if ( getDvarInt( "developer" ) > 0 )
        codolPrintLn( "CoDOL bot killstreak: " + self.name + " goes for the " + crate.crateType + " crate at " + crate.origin );
    crate.codolBotClaim = self;
    crate.codolBotClaimTime = getTime() + 20000;
    self.codolBotTask = "crate";
    self setScriptGoal( crate.origin + ( 0, 0, 12 ), 48 );
    result = self waittill_any_timeout( 15, "goal", "bad_path" );
    if ( isDefined( crate ) && distanceSquared( self.origin, crate.origin ) < 110 * 110 )
    {
        holdTime = 4.0;
        if ( isDefined( crate.owner ) && crate.owner == self )
            holdTime = 1.0;
        self setPlayerAngles( vectorToAngles( crate.origin - self getEye() ) );
        for ( i = 0; i < 3 && isDefined( crate ); i++ )
        {
            self pressUseButton( holdTime );
            wait holdTime + 0.3;
        }
    }
    if ( getDvarInt( "developer" ) > 0 )
        codolPrintLn( "CoDOL bot killstreak: " + self.name + " crate run " + result + ", crate taken " + !isDefined( crate ) );
    // Could not get there: leave it to someone else for a minute.
    if ( isDefined( crate ) && result != "goal" )
        crate.codolBotClaimTime = getTime() + 60000;
    self clearScriptGoal();
    self.codolBotTask = undefined;
}

// ---- carried turrets (T5 bot_turret_location) ---------------------------

placeCarriedReward()
{
    self endon( "death" );
    wait 0.5;
    carried = undefined;
    for ( i = 0; i < 20 && !isDefined( carried ); i++ )
    {
        foreach ( turret in getEntArray( "misc_turret", "classname" ) )
            if ( isDefined( turret.carriedBy ) && turret.carriedBy == self )
                carried = turret;
        if ( !isDefined( carried ) )
            wait 0.1;
    }
    if ( !isDefined( carried ) )
        return;
    // Placement is refused in the air or on a bad spot; the bot keeps moving
    // until the carried turret reports a placeable position, as a player
    // walks until the outline turns green.
    for ( i = 0; i < 60 && isDefined( carried ) && isDefined( carried.carriedBy ); i++ )
    {
        wait 0.25;
        if ( i < 4 )
            continue;
        if ( isDefined( carried.canBePlaced ) && carried.canBePlaced && self isOnGround() )
            self notify( "place_sentry" );
    }
    if ( isDefined( carried ) && isDefined( carried.carriedBy ) && carried.carriedBy == self )
        self notify( "cancel_sentry" );
}
