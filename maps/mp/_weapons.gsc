/*
=============================================================================
 gsc_overlay/maps/mp/_weapons.gsc  --  CODOL equipment GAMEPLAY authority
=============================================================================
 PROVENANCE
   ADAPTED from vendor/iw4/maps/mp/gametypes/_weapons.gsc (IW4 MP):
     watchGrenadeUsage()   :1041   -> watchEquipmentThrows()
     beginGrenadeTracking():1109   -> equipTimedDetonate()
     watchClaymores()      :1365 / claymoreDetonation() :1447 /
        shouldAffectClaymore() :1488       -> equipProximityMine()
     watchC4() :1269 / waitAndDetonate() :1617            -> equipTimedDetonate
     deleteC4AndClaymoresOnDisconnect() :1627 -> the endon("disconnect") guard
   Fuse / radius / damage numbers are the vendor weapon-file values (see
   equipment.py EQUIP_DEF and projectile_sim.EQUIPMENT, both harvested from the
   shipped type-28 WeaponDefs / vendor weapons/mp/<name>).

 WHY THIS FILE EXISTS  (architecture, 2026-09-02)
   The WIRE stays in Python -- weapon configstring publish + the offhand ammo
   netfields (equipment.py) -- and so does the one engine service GSC cannot
   perform, the CLIPMAP COLLISION TRACE (projectile_sim.clip_trace, exposed to
   script as the physicsTrace / bulletTrace builtins).  What moves HERE is the
   SCRIPTED behaviour: which item detonates when, what it damages, what sticks.

 HOW A THROW REACHES THIS SCRIPT
   The engine flies and COLLIDES the thrown missile (projectile_sim, MP #7);
   when it launches, the host raises the IW4 notify
        player notify( "grenade_fire", <missileEnt>, <weaponName> )
   (the exact channel killstreak_support.py already synthesises for airdrops,
   killstreak_support.py:609).  watchEquipmentThrows() below waits on it and
   forks the per-item lifecycle thread.  detonation calls the real
   radiusDamage() builtin, so damage flows through the same host damage queue
   as every other GSC weapon.
=============================================================================
*/

init()
{
    level.codol_equipment = [];

    // name, fuse(s|0), radius, dmgMax, dmgMin, kind, cone(1/0)
    //   kind: "timed"     - a fuse then a radius detonation (frag/semtex/...)
    //         "proximity" - placed, trips on a nearby enemy (claymore/betty)
    //         "impact"    - kills the body it strikes, no AoE (throwing knife)
    //         "beacon"    - placed, persists, never detonates (scrambler/flare)
    addEquip( "frag_grenade_mp",              3.5, 256, 150, 55, "timed",     0 );
    addEquip( "semtex_mp",                    2.0, 256, 200, 55, "timed",     0 );
    addEquip( "throwingknife_mp",             0,     0, 135,  0, "impact",    0 );
    addEquip( "tabun_gas_mp",                 1.0,   0,   0,  0, "timed",     0 );
    addEquip( "emp_grenade_mp",               1.0,   0,   0,  0, "timed",     0 );
    addEquip( "smoke_grenade_mp",             1.0,   0,   0,  0, "timed",     0 );
    addEquip( "flash_grenade_mp",             1.5, 800,   1,  1, "timed",     0 );
    addEquip( "concussion_grenade_mp",        1.0, 512,   1,  1, "timed",     0 );
    addEquip( "white_phosphorus_grenade_mp",  3.5,   0,   0,  0, "timed",     0 );
    addEquip( "claymore_mp",                  0,   256, 200, 50, "proximity", 1 );
    addEquip( "bouncingbetty_mp",             0,   256, 200, 50, "proximity", 0 );
    addEquip( "c4_mp",                        0,   256, 200, 50, "beacon",    0 );
    addEquip( "scrambler_mp",                 0,     0,   0,  0, "beacon",    0 );
    addEquip( "flare_mp",                     0,     0,   0,  0, "beacon",    0 );

    level thread onPlayerConnectEquip();
}

addEquip( name, fuse, radius, dmgMax, dmgMin, kind, cone )
{
    s = spawnstruct();
    s.name   = name;
    s.fuse   = fuse;
    s.radius = radius;
    s.dmgMax = dmgMax;
    s.dmgMin = dmgMin;
    s.kind   = kind;
    s.cone   = cone;
    level.codol_equipment[ name ] = s;
}

onPlayerConnectEquip()
{
    for ( ;; )
    {
        level waittill( "connected", player );
        player thread watchEquipmentThrows();
    }
}

// vendor watchGrenadeUsage() :1041 -- one waiter per life, forks a lifecycle
// thread for each thrown piece of equipment.
watchEquipmentThrows()
{
    self endon( "disconnect" );
    for ( ;; )
    {
        self waittill( "grenade_fire", ent, weapon );
        if ( !isdefined( ent ) || !isdefined( weapon ) )
            continue;
        spec = level.codol_equipment[ weapon ];
        if ( !isdefined( spec ) )
            continue;
        ent.owner = self;
        if ( spec.kind == "timed" )
            ent thread equipTimedDetonate( spec, self );
        else if ( spec.kind == "proximity" )
            ent thread equipProximityMine( spec, self );
        else if ( spec.kind == "impact" )
            ent thread equipImpact( spec, self );
        // "beacon": nothing to do -- the engine keeps it stuck in the world;
        // it is removed on the owner's respawn/disconnect (projectile_sim).
    }
}

// vendor beginGrenadeTracking() :1109 / waitAndDetonate() :1617
equipTimedDetonate( spec, owner )
{
    self endon( "death" );
    self endon( "explode" );
    wait ( spec.fuse );
    self equipExplode( spec, owner );
}

// vendor claymoreDetonation() :1447 + shouldAffectClaymore() :1488
equipProximityMine( spec, owner )
{
    self endon( "death" );
    self endon( "explode" );
    wait ( 0.5 );                          // arm delay
    for ( ;; )
    {
        wait ( 0.1 );
        players = getplayers();
        for ( i = 0; i < players.size; i++ )
        {
            p = players[ i ];
            if ( !isdefined( p ) || !isAlive( p ) )
                continue;
            if ( isdefined( owner ) && p == owner )
                continue;
            if ( distance( self.origin, p.origin ) > spec.radius * 0.75 )
                continue;
            if ( spec.cone && !mineSeesTarget( self, p ) )
                continue;
            wait ( 0.75 );                 // grace, vendor CLAYMORE grace
            self equipExplode( spec, owner );
            return;
        }
    }
}

// vendor shouldAffectClaymore() forward-cone test (dot > cos 70).  Bouncing
// betty passes cone==0 and never calls this (full 360).
mineSeesTarget( mine, target )
{
    if ( !isdefined( mine.angles ) )
        return true;
    forward = anglestoforward( mine.angles );
    to = target.origin - mine.origin;
    to = vectornormalize( to );
    return ( vectordot( forward, to ) > 0.342 );
}

// vendor throwing-knife direct impact.  The engine already reports the body
// the knife struck via projectile_sim's impact pass; this mirrors the score
// path so a GSC-authored build stays the authority.  (No AoE.)
equipImpact( spec, owner )
{
    self endon( "death" );
    self waittill( "impact", victim );
    if ( isdefined( victim ) && isAlive( victim ) )
        // codol_damageBot damages the REAL rendered bot the knife struck and
        // queues by client number, so the one damage owner applies it.
        codol_damageBot( victim, spec.dmgMax, owner, "MOD_IMPACT", spec.name,
                         "torso_upper", self.origin );
}

equipExplode( spec, owner )
{
    self notify( "explode" );
    if ( spec.radius > 0 && spec.dmgMax > 0 )
        // codol_radiusDamage: team-filtered linear falloff over the REAL
        // rendered bots (+ the human), crediting the owner -- the SAME GSC
        // damage authority the bullet path uses.  Replaces the engine
        // radiusDamage(), which scanned level.players only and could TK.
        codol_radiusDamage( self.origin, spec.radius, spec.dmgMax, spec.dmgMin,
                            owner, spec.name, "MOD_GRENADE_SPLASH" );
    self notify( "detonated" );
}

// ===========================================================================
//  BULLET / HITSCAN COMBAT  --  the GSC combat authority
// ===========================================================================
//  Migrated from Python damage_events.HitScan (2026-09-02).  The split:
//    PYTHON keeps  : decoding the fire event (attack edge + weapon +
//                    serverTime + the server-simulated eye origin/viewangles),
//                    and the clipmap bullet TRACE (codol_bulletHitBot's LOS +
//                    the ray-box geometry, both server-only).
//    GSC   owns    : on that event, decide WHO is hit (codol_bulletHitBot),
//                    HOW MUCH damage (weapon table + range falloff), the
//                    HEADSHOT multiplier, the means-of-death, and the damage
//                    application (codol_damageBot).  Death, kill credit,
//                    obituary and score all follow from the queued damage
//                    through the single damage owner -> on_kill ->
//                    CodeCallback_PlayerKilled + _rank scoring.
//
//  runtime.fire_bullet( cn, eyeOrigin, forward, weaponName ) runs THIS on the
//  shooter once per round the ammo model let out of the barrel.
// ===========================================================================
codol_bulletFire( eyeOrigin, forward, weapon )
{
    rng = codol_weaponRange( weapon );
    hit = codol_bulletHitBot( eyeOrigin, forward, rng, self );
    if ( !isdefined( hit ) || !hit[ "hit" ] )
        return undefined;

    dmg    = codol_weaponDamage( weapon, hit[ "distance" ] );
    mod    = codol_bulletMod( weapon );
    hitloc = hit[ "hitloc" ];

    // HEADSHOT.  The trace hands back which part of the hull the ray met; the
    // multiplier and the MOD are the DECISION, so they live here.
    if ( hitloc == "head" || hitloc == "helmet" || hitloc == "neck" )
    {
        dmg = int( dmg * codol_headMultiplier( weapon ) );
        mod = "MOD_HEAD_SHOT";
    }

    codol_damageBot( hit[ "entity" ], dmg, self, mod, weapon, hitloc,
                     hit[ "position" ] );

    // echo the resolved numbers back so runtime.fire_bullet can drive the
    // Python-side HUD (hit marker / kill feedback) off the GSC decision.
    hit[ "damage" ] = dmg;
    hit[ "mod" ]    = mod;
    return hit;
}

// ===========================================================================
// ★ REBUILD STEP 3 (2026-09-02) -- THE DAMAGE NUMBER, ASKED FOR BY THE ENGINE.
//
// combat.py's Bullet_Fire owns the ENGINE half (spread, the clipmap+entity
// trace over the step-2 table, which body, where on it, how far).  It then
// asks THIS function for the two things the script owns: how much damage that
// round does, and its means-of-death.  No damage number and no MOD string
// exists anywhere in Python -- that is the whole of BASE.md 2.1's rule.
//
// Reuses codol_weaponDamage / codol_headMultiplier / codol_bulletMod verbatim,
// so the numbers are exactly the ones codol_bulletFire already shipped.
// ===========================================================================
codol_bulletDamage( weapon, dist, hitloc )
{
    res = spawnstruct();
    dmg = codol_weaponDamage( weapon, dist );
    mod = codol_bulletMod( weapon );

    if ( hitloc == "head" || hitloc == "helmet" || hitloc == "neck" )
    {
        dmg = int( dmg * codol_headMultiplier( weapon ) );
        mod = "MOD_HEAD_SHOT";
    }

    res.damage = int( dmg );
    res.mod    = mod;
    return res;
}

// Max bullet reach handed to the trace (world units).  Falloff below caps the
// EFFECTIVE damage; this only bounds the ray.
codol_weaponRange( weapon )
{
    if ( issubstr( weapon, "cheytac" ) || issubstr( weapon, "barrett" ) ||
         issubstr( weapon, "wa2000" )  || issubstr( weapon, "m21" ) ||
         issubstr( weapon, "sniper" ) )
        return 12000;
    if ( codol_isShotgun( weapon ) )
        return 1200;
    return 8192;
}

codol_isShotgun( weapon )
{
    return ( issubstr( weapon, "model1887" ) || issubstr( weapon, "spas12" ) ||
             issubstr( weapon, "striker" )   || issubstr( weapon, "aa12" ) ||
             issubstr( weapon, "ranger" )    || issubstr( weapon, "shotgun" ) );
}

codol_isPistol( weapon )
{
    return ( issubstr( weapon, "usp" )       || issubstr( weapon, "44magnum" ) ||
             issubstr( weapon, "coltanaconda" ) || issubstr( weapon, "deserteagle" ) ||
             issubstr( weapon, "desert_eagle" ) || issubstr( weapon, "beretta" ) ||
             issubstr( weapon, "pp2000" )    || issubstr( weapon, "glock" ) ||
             issubstr( weapon, "m9" ) );
}

// Linear range falloff, the engine's own rule: NEAR damage out to minRange,
// lerped to FAR damage at maxRange (vendor weapon-file bulletDamage /
// damageRange fields; _airstrike.gsc:395 lerp form).  Numbers here are the
// DECISION and are tunable in GSC without touching Python.
codol_weaponDamage( weapon, dist )
{
    // defaults reproduce the previously-live hitscan feel (body ~34 at the
    // engagement ranges, head lethal via codol_headMultiplier).  rmin is the
    // range at which NEAR damage still holds in full, so a close/mid-range
    // headshot stays a one-shot kill, matching the retired head_damage=100.
    near = 34;  far = 24;  rmin = 1200;  rmax = 3000;

    if ( issubstr( weapon, "cheytac" ) || issubstr( weapon, "barrett" ) ||
         issubstr( weapon, "wa2000" )  || issubstr( weapon, "m21" ) ||
         issubstr( weapon, "sniper" ) )
    {
        near = 95;  far = 70;  rmin = 2000;  rmax = 12000;
    }
    else if ( codol_isShotgun( weapon ) )
    {
        near = 90;  far = 20;  rmin = 100;  rmax = 900;
    }
    else if ( codol_isPistol( weapon ) )
    {
        near = 40;  far = 25;  rmin = 600;  rmax = 2000;
    }

    if ( dist <= rmin )
        return near;
    if ( dist >= rmax )
        return far;
    frac = ( dist - rmin ) / ( rmax - rmin );
    return int( near + ( far - near ) * frac );
}

// Head-hit damage multiplier (DECISION).  Default 3.0 makes a clean head hit
// lethal against a 100-HP body, matching the retired Python head_damage=100.
codol_headMultiplier( weapon )
{
    if ( codol_isPistol( weapon ) )
        return 2.0;
    return 3.0;
}

// Means-of-death for a body/limb bullet hit (headshots override to
// MOD_HEAD_SHOT in codol_bulletFire).
codol_bulletMod( weapon )
{
    if ( codol_isPistol( weapon ) )
        return "MOD_PISTOL_BULLET";
    return "MOD_RIFLE_BULLET";
}
