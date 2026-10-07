// coop soldier animscript: soldier_combat.  Exposed standing fire.
//
// The native frame hands the actor this module once its enemy is inside
// self.meleeAttackDist AND CODOL_ActorMeleeRangeGoal finds the knee (+20) and
// head (+72) lines to him clear of world/clip -- the soldier's line-of-fire
// test (src/codol/player/codol_dog_combat.c).  It holds the module while
// self.safeToChangeScript is false, and re-evaluates every frame it is true,
// so a burst is uninterruptible and the pause between bursts is where the
// soldier gives up a lost target and goes back to moving.
//
// Behaviour reference: IW4 animscripts\combat.gsc / combat_utility.gsc
// (exposed stand, aim at the enemy, fire bursts, reload when the clip runs
// dry).  Two things are this server's own and say so:
//
//  * THE SHOT.  IW4 fires an AI weapon natively (the shoot() builtin and the
//    animscript "fire" notetrack).  This server registers no AI shoot, so a
//    shot is magicBullet( weapon, muzzle, aim point ) -- the native hitscan
//    with its EV_FIRE_WEAPON event, so C1 plays the weapon's own fire sound
//    and tracer (src/codol/script/codol_script_magicbullet.c).  The muzzle is
//    a point 32 units in front of the chest along the aim line, outside the
//    soldier's own 15-unit capsule and locational hit boxes, because the
//    hitscan's pass entity is the world, not the shooter.
//  * THE MISS.  IW4 scales AI accuracy through the difficulty tables
//    (maps\_gameskill).  Here a shot hits with probability self.accuracy,
//    falling off with range, and otherwise is aimed a body-width wide of the
//    target -- so misses still crack past the player and hit the wall behind.

#include maps\mp\animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self endon( "killanimscript" );

	for ( ;; )
	{
		self orientmode( "face enemy" );
		self animmode( "zonly_physics" );
		ShootAtEnemy();
		self.safeToChangeScript = true;
		self animmode( "normal" );
		wait( 0.05 );
	}
}

role_burst()
{
	switch ( self.coop_role )
	{
	case "lmg":
		return 6 + randomInt( 5 );
	case "smg":
		return 3 + randomInt( 4 );
	case "shgn":
	case "snpr":
	case "pistol":
		return 1;
	}
	return 3 + randomInt( 3 );
}

role_fire_delay()
{
	switch ( self.coop_role )
	{
	case "lmg":
		return 0.08;
	case "smg":
		return 0.07;
	case "shgn":
		return 0.9;
	case "snpr":
		return 2.0;
	case "pistol":
		return 0.4;
	}
	return 0.1;
}

role_fire_anim()
{
	switch ( self.coop_role )
	{
	case "shgn":
	case "snpr":
	case "pistol":
		return %exposed_shoot_semi1;
	case "lmg":
		return %exposed_shoot_auto_v2;
	}
	return %exposed_shoot_burst3;
}

aim_point( enemy )
{
	// The chest of a standing player; a crouched or prone one is lower.
	height = 48;
	if ( isPlayer( enemy ) )
	{
		stance = enemy getStance();
		if ( stance == "crouch" )
			height = 32;
		else if ( stance == "prone" )
			height = 10;
	}
	return enemy.origin + ( 0, 0, height );
}

muzzle_point( target )
{
	chest = self.origin + ( 0, 0, 52 );
	dir = vectorNormalize( target - chest );
	return chest + dir * 32;
}

can_shoot( enemy )
{
	if ( !isDefined( enemy ) || !isAlive( enemy ) )
		return false;
	if ( isDefined( self.ignoreall ) && self.ignoreall )
		return false;
	reach = self.meleeAttackDist + 64;
	if ( distanceSquared( self.origin, enemy.origin ) > reach * reach )
		return false;
	target = aim_point( enemy );
	return bulletTracePassed( muzzle_point( target ), target, false, self );
}

fire_shot( enemy )
{
	if ( !isDefined( self.coop_weapon ) || self.coop_weapon == "" )
		return;

	target = aim_point( enemy );
	start = muzzle_point( target );

	// Hold fire with a squadmate in the line: the round is world-owned, so
	// nothing else would stop it killing him (MEASURED on the first probe:
	// three of six soldiers died to each other's bursts).
	trace = bulletTrace( start, target, true, self );
	if ( isDefined( trace[ "entity" ] ) && trace[ "entity" ] != enemy && isAI( trace[ "entity" ] ) )
		return;

	dist = distance( start, target );
	chance = self.accuracy;
	if ( !isDefined( chance ) )
		chance = 0.3;
	// Halve the hit chance at the far edge of the engagement band.
	if ( self.meleeAttackDist > 0 )
		chance = chance * ( 1 - 0.5 * clamp01( dist / self.meleeAttackDist ) );

	if ( randomFloat( 1 ) > chance )
	{
		// Wide by more than half a body, in the plane across the aim line.
		dir = vectorNormalize( target - start );
		side = vectorNormalize( ( 0 - dir[ 1 ], dir[ 0 ], 0 ) );
		miss = 20 + randomFloat( 40 );
		if ( randomInt( 2 ) )
			miss = 0 - miss;
		target = target + side * miss + ( 0, 0, randomFloatRange( -16, 24 ) );
	}

	// Past the target, so a miss carries on into whatever is behind him.
	end = start + vectorNormalize( target - start ) * ( dist + 512 );
	magicBullet( self.coop_weapon, start, end );
	// One line per soldier for the probe harness (+set scr_coop_test 1).
	if ( !isDefined( self.coop_logged_fire ) && getDvarInt( "scr_coop_test" ) )
	{
		self.coop_logged_fire = true;
		codolprintln( "CoDOL coop soldier: ent " + self getEntityNumber() + " (" + self.coop_role + ") fires " +
			self.coop_weapon + " at ent " + enemy getEntityNumber() + " from " + int( dist ) + " units" );
	}
	self.a.lastShootTime = getTime();
	self.a.burstShots++;
}

clamp01( value )
{
	if ( value < 0 )
		return 0;
	if ( value > 1 )
		return 1;
	return value;
}

ShootAtEnemy()
{
	self endon( "killanimscript" );
	self endon( "death" );

	stand_pose( 0.2 );

	for ( ;; )
	{
		enemy = self.enemy;
		if ( !can_shoot( enemy ) )
			return;

		self.safeToChangeScript = false;
		self orientmode( "face enemy" );

		// Turn to face him before the first round of a burst leaves the barrel.
		wait( 0.15 );
		if ( !isDefined( enemy ) || !isAlive( enemy ) )
		{
			self.safeToChangeScript = true;
			return;
		}

		self setAnim( %add_fire, 1, 0.1, 1 );
		self setFlaggedAnimKnobRestart( "fireanim", role_fire_anim(), 1, 0.05, 1 );

		shots = role_burst();
		delay = role_fire_delay();
		for ( i = 0; i < shots; i++ )
		{
			if ( !isDefined( enemy ) || !isAlive( enemy ) )
				break;
			fire_shot( enemy );
			wait( delay );
		}
		self clearAnim( %add_fire, 0.2 );

		if ( self.a.burstShots >= 30 )
			Reload();

		self.safeToChangeScript = true;
		wait( 0.3 + randomFloat( 0.6 ) );
	}
}

Reload()
{
	self.a.burstShots = 0;
	self setFlaggedAnimKnobAllRestart( "reloadanim", %exposed_reload, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( %exposed_reload ), "reloadanim" );
	stand_pose( 0.2 );
}
