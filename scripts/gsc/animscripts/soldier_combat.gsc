// coop soldier animscript: soldier_combat.  Exposed fire, standing or
// kneeling.
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
// (exposed stand or crouch, aim at the enemy with the additive aim groups,
// fire bursts on %add_fire, reload when the clip runs dry) and the
// regular-difficulty numbers of CoD4/IW4 maps\_gameskill.gsc.  The clips are
// IW4's animset tables for the weapon in his hands (soldier_shared.gsc):
// rifle, pistol, pump shotgun or launcher, standing or crouched.
//
//  * THE SHOT.  self codol_actorShoot( weapon, muzzle, end ): the native
//    actor fire (src/codol/player/codol_actor_fire.c) -- Bullet_Fire owned by
//    the soldier and EV_FIRE_WEAPON on the soldier, whose entity state names
//    the weapon, so C1 plays that weapon's fire sound and its world muzzle
//    flash on the attached gun's tag_flash, and draws the tracer from it.
//    (magicBullet, used before, put the event on a temp entity C1 ignores:
//    its weapon word was never published, and CG_FireWeapon returns on 0.)
//  * BURSTS.  Per weapon class: burst length by range (shorter, then single
//    shots, the farther the target -- IW4 combat_utility's burst/semi
//    styles), the class's cyclic delay inside a burst, and a pause between
//    bursts (snipers: _gameskill regular min/max_sniper_burst_delay_time,
//    2..3 s).
//  * ACCURACY.  A round hits with
//        self.accuracy * scr_coop_ai_accuracy * range * time-on-target
//                      * stance * movement
//    range: 1 inside the class's near distance, falling linearly to
//      scr_coop_ai_far_accuracy at its far distance (IW4's aiVsPlayer
//      accuracy graphs fall the same way).
//    time on target: nothing hits for the first missTime seconds of a new
//      engagement (_gameskill missTimeConstant + distance *
//      missTimeDistanceFactor, regular's shape, scaled up so the first
//      burst always gives the player a warning), then accuracy ramps from
//      scr_coop_ai_ramp_start to 1 over scr_coop_ai_ramp_time seconds of
//      continuous fire at the same target.
//    stance: crouched 0.75, prone 0.5.  movement: a running player is
//      harder to hit, down to half at sprint speed.  class: automatic fire
//      sprays (LMG 0.65, SMG 0.85 a round).
//  * THE MISS.  A miss is never a hit: it is aimed 30..64 units wide of the
//    player's chest or into the ground beside him, and carries on past him,
//    so it cracks by and kicks up the wall or the dirt near him.
//  * DAMAGE per hit, invulnerability windows and regeneration are the
//    player's side: _coopmode Callback_PlayerDamage / healthRegen.
//
// Tuning dvars (read at the first engagement of the match; "" keeps the
// default): scr_coop_ai_accuracy 1.6, scr_coop_ai_far_accuracy 0.25,
// scr_coop_ai_ramp_time 3, scr_coop_ai_ramp_start 0.4,
// scr_coop_ai_misstime 0.3, scr_coop_ai_misstime_dist 0.0004 (s per unit),
// scr_coop_ai_burst_scale 1, scr_coop_ai_pause_scale 1.

#include animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self endon( "killanimscript" );
	// A helper endon terminates its own call frame; the owning behavior must
	// also end on death so it cannot evaluate a cancelled helper result.
	self endon( "death" );

	init_transitions();
	watch_player_grenades();
	update_cover_use();
	self.coop_in_cover = undefined;
	self.a.movement = "stop";

	// IW4 combat.gsc: at a cover node the node's own script runs
	// (cover_left / cover_right / cover_stand / cover_crouch); the native
	// actor says when he stands on the node it claimed for him.
	if ( self codol_actorCoverArrived() )
		animscripts\soldier_cover::main();

	self orientmode( "face current" );
	self animmode( "zonly_physics", false );
	choose_pose( true );

	for ( ;; )
	{
		if ( self codol_actorCoverArrived() )
		{
			stop_aiming();
			animscripts\soldier_cover::main();
			choose_pose( true );
			continue;
		}
		self animmode( "zonly_physics", false );
		if ( react_to_grenade() )
		{
			choose_pose( true );
			continue;
		}
		enemy = self.enemy;
		if ( isDefined( enemy ) && can_melee( enemy ) )
		{
			melee_strike( enemy );
			choose_pose( true );
			continue;
		}
		// IW4 combat.gsc: an enemy closing inside arm's reach plus a step is
		// backed away from (exposed_backpedal), weapon up.
		if ( isDefined( enemy ) && self.a.pose == "stand" && distanceSquared( enemy.origin, self.origin ) < 160 * 160 &&
		     ( !isDefined( self.coop_next_backpedal ) || getTime() >= self.coop_next_backpedal ) )
		{
			self.coop_next_backpedal = getTime() + 3000 + randomInt( 2000 );
			if ( backpedal( enemy ) )
				continue;
		}
		// IW4 TurnToFaceRelativeYaw: a target well off his facing is
		// turned to with the stance's turn clip; the aim tracks the rest.
		if ( isDefined( enemy ) )
		{
			turn_toward( enemy.origin, 45 );
			self orientmode( "face angle", vectorToYaw( enemy.origin - self.origin ) );
		}
		if ( isDefined( enemy ) && can_throw_grenade( enemy ) && randomInt( 6 ) == 0 &&
		     throw_grenade( enemy, exposed_grenade_anim() ) )
		{
			choose_pose( true );
			continue;
		}
		ShootAtEnemy();
		self.safeToChangeScript = true;
		wait( 0.05 );
	}
}

backpedal( enemy )
{
	self endon( "killanimscript" );
	clip = pick( list2( %exposed_backpedal, %exposed_backpedal_v2 ) );
	self orientmode( "face angle", vectorToYaw( enemy.origin - self.origin ), 1 );
	endPos = delta_end( self.origin, self.angles[ 1 ], getMoveDelta( clip, 0, 1 ) );
	if ( !may_move( self.origin, endPos ) )
		return false;
	trace_anim( "backpedal" );
	stat_bump( "backpedal" );
	stop_aiming();
	self.safeToChangeScript = false;
	self orientmode( "face current" );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "backpedal", clip, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( clip ), "backpedal" );
	self.safeToChangeScript = true;
	start_aiming( 0.2 );
	return true;
}

// IW4 animset grenade throws for the exposed pose he is in.
exposed_grenade_anim()
{
	if ( isDefined( self.a.pose ) && self.a.pose == "crouch" )
		return %crouch_grenade_throw;
	if ( isDefined( self.coop_cqb ) )
		return %cqb_stand_grenade_throw;
	return pick( list3( %stand_grenade_throw, %exposed_grenadethrowb, %exposed_grenadethrowc ) );
}

// IW4 combat.gsc exposed stance choice: a soldier who settles into a fire
// fight at range often drops to a knee (IW4 hesitates, then crouches if the
// crouched line of fire is clear), and now and then rises again.  Pistol
// carriers stay up (IW4 never crouches a sidearm).  The pose change plays
// IW4's exposed_stand_2_crouch / exposed_crouch_2_stand.
choose_pose( entering )
{
	self endon( "killanimscript" );

	if ( !isDefined( self.a.pose ) || self.a.pose == "prone" )
		self.a.pose = "stand";
	want = "stand";
	enemy = self.enemy;

	if ( can_crouch() && isDefined( enemy ) )
	{
		dist = distance( self.origin, enemy.origin );
		chance = 30;
		switch ( self.coop_role )
		{
		case "snpr":
			chance = 65;
			break;
		case "rpg":
			chance = 50;
			break;
		case "lmg":
			chance = 45;
			break;
		case "shgn":
			chance = 15;
			break;
		}
		if ( dist < 400 )
			chance = 0;
		if ( !entering )
			chance = int( chance * 0.6 );
		roll = randomInt( 100 );
		if ( roll < chance && crouch_line_clear( enemy ) )
			want = "crouch";
		else if ( roll < chance )
			trace_anim( "combat crouch refused: crouched line of fire blocked" );
		else if ( !entering && ( self.a.pose == "stand" || crouch_line_clear( enemy ) ) )
			want = self.a.pose;
	}

	self.coop_next_pose_check = getTime() + 8000 + randomInt( 6000 );
	trace_anim( "combat pose " + self.a.pose + " -> " + want + " (" + weapon_kind() + ", role " + self.coop_role + ")" );
	if ( want != self.a.pose )
	{
		change_pose( want );
		return;
	}
	self.a.pose = set_anim_array( self.a.pose );
	if ( entering || !isDefined( self.coop_aiming ) )
		start_aiming( 0.2 );
}

crouch_line_clear( enemy )
{
	return bulletTracePassed( self.origin + ( 0, 0, 40 ), aim_point( enemy ), false, self );
}

dvar_float( name, value )
{
	if ( getDvar( name ) == "" )
		return value;
	return getDvarFloat( name );
}

tuning()
{
	if ( isDefined( level.coop_ai ) )
		return level.coop_ai;
	t = spawnStruct();
	t.accuracy = dvar_float( "scr_coop_ai_accuracy", 1.6 );
	t.far_accuracy = dvar_float( "scr_coop_ai_far_accuracy", 0.25 );
	t.ramp_time = dvar_float( "scr_coop_ai_ramp_time", 3 );
	t.ramp_start = dvar_float( "scr_coop_ai_ramp_start", 0.4 );
	t.misstime = dvar_float( "scr_coop_ai_misstime", 0.3 );
	t.misstime_dist = dvar_float( "scr_coop_ai_misstime_dist", 0.0004 );
	t.burst_scale = dvar_float( "scr_coop_ai_burst_scale", 1 );
	t.pause_scale = dvar_float( "scr_coop_ai_pause_scale", 1 );
	if ( t.ramp_time < 0.1 )
		t.ramp_time = 0.1;
	level.coop_ai = t;
	return t;
}

// Rounds in one burst at this range.
role_burst( dist )
{
	if ( projectile_weapon() )
		return 1;
	n = 1;
	switch ( self.coop_role )
	{
	case "lmg":
		if ( dist < 1000 )
			n = 5 + randomInt( 6 );
		else
			n = 4 + randomInt( 4 );
		break;
	case "smg":
		if ( dist < 500 )
			n = 4 + randomInt( 4 );
		else
			n = 2 + randomInt( 3 );
		break;
	case "shgn":
		n = 1 + randomInt( 2 );
		break;
	case "pistol":
		n = 1 + randomInt( 3 );
		break;
	case "snpr":
		return 1;
	default:
		if ( dist < 600 )
			n = 3 + randomInt( 3 );
		else if ( dist < 1100 )
			n = 2 + randomInt( 3 );
		else
			n = 1 + randomInt( 2 );
		break;
	}
	n = int( n * tuning().burst_scale + 0.5 );
	if ( n < 1 )
		n = 1;
	return n;
}

// Seconds between rounds inside a burst.
role_fire_delay()
{
	if ( projectile_weapon() )
	{
		delay = weaponFireTime( self.coop_weapon );
		if ( delay < 0.05 )
			delay = 0.05;
		return delay;
	}
	switch ( self.coop_role )
	{
	case "lmg":
		return 0.09;
	case "smg":
		return 0.075;
	case "shgn":
		return 0.4;
	case "snpr":
		return 0.1;
	case "pistol":
		return 0.25;
	}
	return 0.1;
}

// Seconds between bursts.
role_pause()
{
	lo = 0.5;
	hi = 1.1;
	switch ( self.coop_role )
	{
	case "lmg":
		lo = 0.6;
		hi = 1.2;
		break;
	case "smg":
		lo = 0.4;
		hi = 0.9;
		break;
	case "shgn":
		lo = 0.9;
		hi = 1.5;
		break;
	case "pistol":
		lo = 0.5;
		hi = 1.0;
		break;
	case "snpr":
		lo = 2.0;
		hi = 3.0;
		break;
	}
	return randomFloatRange( lo, hi ) * tuning().pause_scale;
}

// Where the class's accuracy starts and stops falling with range.
role_range( which )
{
	near = 256;
	far = 1600;
	switch ( self.coop_role )
	{
	case "smg":
		near = 200;
		far = 1200;
		break;
	case "shgn":
		near = 150;
		far = 650;
		break;
	case "pistol":
		near = 150;
		far = 900;
		break;
	case "snpr":
		near = 800;
		far = 4000;
		break;
	case "lmg":
		near = 300;
		far = 1800;
		break;
	}
	if ( which == "near" )
		return near;
	return far;
}

// Per-round accuracy of the class: automatic fire sprays (an LMG's long
// bursts would otherwise land several rounds a burst).
role_accuracy()
{
	switch ( self.coop_role )
	{
	case "lmg":
		return 0.65;
	case "smg":
		return 0.85;
	case "pistol":
		return 0.9;
	}
	return 1;
}

role_clip()
{
	if ( isAI( self ) )
		return self codol_actorClipCapacity();
	switch ( self.coop_role )
	{
	case "lmg":
		return 100;
	case "shgn":
		return 8;
	case "snpr":
		return 5;
	case "pistol":
		return 15;
	}
	return 30;
}

clip_remaining()
{
	if ( isAI( self ) )
		return self codol_actorClipRemaining();
	return role_clip() - self.a.burstShots;
}

// IW4 shoot_behavior / combat_utility FireUntilOutOfAmmo: full-auto
// weapons play the set's "fire" loop, semi-automatics and snipers the
// "single"/"semiN" clips, everything else the "burstN" clip of the burst's
// length -- each from the soldier's own weapon family set (soldier_shared
// animsets), so a pistol fires pistol_stand_fire_A and a pump shotgun
// shotgun_stand_fire_1A/1B, as in IW4.
fire_anim_for( shots )
{
	if ( !isDefined( self.a.array ) )
		set_anim_array( "stand" );
	if ( projectile_weapon() )
		return animArrayPickRandom( "single" );
	switch ( self.coop_role )
	{
	case "lmg":
	case "smg":
		return animArray( "fire" );
	case "snpr":
	case "shgn":
	case "pistol":
		if ( shots <= 1 )
			return animArrayPickRandom( "single" );
		if ( shots > 5 )
			shots = 5;
		return animArray( "semi" + shots );
	}
	if ( shots <= 1 )
		return animArrayPickRandom( "single" );
	if ( shots > 6 )
		shots = 6;
	return animArray( "burst" + shots );
}

// The muzzle: the gun's own flash tag when the soldier carries the model,
// else a point ahead of the chest.  codol_actorShoot passes the soldier's
// own capsule and hit boxes, so the round may start inside them.
muzzle_point( target )
{
	if ( isDefined( self.coop_weapon_model ) )
		return self getTagOrigin( "tag_flash" );
	chest = self.origin + ( 0, 0, shoot_from_height() - 6 );
	dir = vectorNormalize( target - chest );
	return chest + dir * 16;
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

clamp01( value )
{
	if ( value < 0 )
		return 0;
	if ( value > 1 )
		return 1;
	return value;
}

hit_chance( enemy, dist )
{
	t = tuning();

	chance = self.accuracy;
	if ( !isDefined( chance ) )
		chance = 0.3;
	chance = chance * t.accuracy * role_accuracy();
	// Firing on the run (soldier_move run and gun) is wilder.
	if ( isDefined( self.coop_moving_fire ) )
		chance = chance * 0.5;

	// Range.
	near = role_range( "near" );
	far = role_range( "far" );
	if ( dist > near )
	{
		f = clamp01( ( dist - near ) / ( far - near ) );
		chance = chance * ( 1 - ( 1 - t.far_accuracy ) * f );
	}

	// Time on target: the warning, then the ramp.
	onTarget = ( getTime() - self.coop_aim_start ) / 1000;
	missTime = t.misstime + dist * t.misstime_dist;
	if ( onTarget < missTime )
		return 0;
	ramp = clamp01( ( onTarget - missTime ) / t.ramp_time );
	chance = chance * ( t.ramp_start + ( 1 - t.ramp_start ) * ramp );

	if ( isPlayer( enemy ) )
	{
		stance = enemy getStance();
		if ( stance == "crouch" )
			chance = chance * 0.75;
		else if ( stance == "prone" )
			chance = chance * 0.5;
		v = enemy getVelocity();
		speed = length( ( v[ 0 ], v[ 1 ], 0 ) );
		chance = chance * ( 1 - 0.5 * clamp01( ( speed - 30 ) / 220 ) );
	}
	return chance;
}

// A point the round is aimed at that is NOT the player: wide of his chest,
// or the ground beside his feet.
miss_point( enemy, target, start )
{
	dir = vectorNormalize( target - start );
	side = vectorNormalize( ( 0 - dir[ 1 ], dir[ 0 ], 0 ) );
	sign = 1;
	if ( randomInt( 2 ) )
		sign = -1;
	if ( randomFloat( 1 ) < 0.35 )
	{
		p = enemy.origin + side * ( randomFloatRange( 30, 72 ) * sign ) + dir * randomFloatRange( -48, 48 );
		return ( p[ 0 ], p[ 1 ], enemy.origin[ 2 ] + 1 );
	}
	return target + side * ( randomFloatRange( 30, 64 ) * sign ) + ( 0, 0, randomFloatRange( -28, 24 ) );
}

fire_shot( enemy )
{
	if ( !isDefined( self.coop_weapon ) || self.coop_weapon == "" )
		return;

	target = aim_point( enemy );
	start = muzzle_point( target );

	// Hold fire with a squadmate in the line.
	trace = bulletTrace( start, target, true, self );
	if ( isDefined( trace[ "entity" ] ) && trace[ "entity" ] != enemy && isAI( trace[ "entity" ] ) )
	{
		// A Survival wave crowds the player (dogs, walking bombs): the ones
		// behind shoot past them, at his head or a shoulder.
		if ( !isDefined( self.survival_ref ) )
			return;
		target = past_squadmate( start, enemy, target );
		if ( !isDefined( target ) )
		{
			self.survival_blocked = getTime();
			return;
		}
	}

	dist = distance( start, target );
	hit = randomFloat( 1 ) < hit_chance( enemy, dist );
	if ( hit )
		target = target + ( randomFloatRange( -3, 3 ), randomFloatRange( -3, 3 ), randomFloatRange( -4, 4 ) );
	else
		target = miss_point( enemy, target, start );

	// Past the aim point, so a miss carries on into whatever is behind him.
	end = target;
	if ( !projectile_weapon() )
		end = start + vectorNormalize( target - start ) * ( distance( start, target ) + 384 );
	self codol_actorShoot( self.coop_weapon, start, end );

	if ( isDefined( self.coop_stat_shots ) )
	{
		self.coop_stat_shots++;
		if ( hit )
			self.coop_stat_aimed_hits++;
		else
		{
			// Where the miss was aimed: how far from his chest, and whether
			// at the ground by his feet (probe harness, _cooptest duel).
			chest = aim_point( enemy );
			self.coop_stat_miss_dist += distance( chest, target );
			if ( target[ 2 ] < enemy.origin[ 2 ] + 4 )
				self.coop_stat_miss_ground++;
		}
	}
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

past_squadmate( start, enemy, target )
{
	dir = vectorNormalize( target - start );
	side = ( 0 - dir[ 1 ], dir[ 0 ], 0 );
	points = [];
	points[ 0 ] = enemy getEye();
	points[ 1 ] = target + side * 12;
	points[ 2 ] = target - side * 12;
	for ( i = 0; i < points.size; i++ )
	{
		trace = bulletTrace( start, points[ i ], true, self );
		if ( !isDefined( trace[ "entity" ] ) || trace[ "entity" ] == enemy || !isAI( trace[ "entity" ] ) )
			return points[ i ];
	}
	return undefined;
}

// A new target, or one not shot at for four seconds (longer than any
// class's pause between bursts), starts a new engagement: the miss time and
// the ramp begin again.
begin_engagement( enemy )
{
	if ( !isDefined( self.coop_aim_target ) || self.coop_aim_target != enemy ||
	     !isDefined( self.coop_aim_last ) || getTime() - self.coop_aim_last > 4000 )
	{
		self.coop_aim_target = enemy;
		self.coop_aim_start = getTime();
	}
}

ShootAtEnemy()
{
	self endon( "killanimscript" );
	self endon( "death" );

	for ( ;; )
	{
		enemy = self.enemy;
		if ( !can_shoot( enemy ) )
			return;

		self.safeToChangeScript = false;
		self orientmode( "face enemy" );

		if ( getTime() >= self.coop_next_pose_check )
			choose_pose( false );
		if ( self codol_actorCoverArrived() )
		{
			self.safeToChangeScript = true;
			return;
		}

		begin_engagement( enemy );

		// Turn to face him before the first round of a burst leaves the barrel.
		wait( 0.15 );
		if ( !isDefined( enemy ) || !isAlive( enemy ) )
		{
			self.safeToChangeScript = true;
			return;
		}

		shots = role_burst( distance( self.origin, enemy.origin ) );
		remaining = clip_remaining();
		if ( shots > remaining )
			shots = remaining;
		fireAnim = fire_anim_for( shots );

		// IW4 showFireHideAimIdle: the idle additive gives way to the fire one.
		self setAnimLimited( %add_idle, 0, 0.2 );
		self setAnim( %add_fire, 1, 0.1 );
		self setFlaggedAnimKnobRestart( "fireanim", fireAnim, 1, 0, 1 );

		delay = role_fire_delay();
		for ( i = 0; i < shots; i++ )
		{
			if ( !isDefined( enemy ) || !isAlive( enemy ) )
				break;
			fire_shot( enemy );
			self.coop_aim_last = getTime();
			wait( delay );
		}
		// IW4 hideFireShowAimIdle.
		self clearAnim( %add_fire, 0.2 );
		self setAnimLimited( %add_idle, 1, 0.2 );
		if ( isDefined( self.coop_stat_bursts ) )
			self.coop_stat_bursts++;

		if ( clip_remaining() <= 0 )
			Reload();

		self.safeToChangeScript = true;
		wait( role_pause() );
	}
}

// IW4 combat_utility Reload: the weapon family's reload for the pose he is
// in, then back to the aim.
Reload()
{
	self endon( "killanimscript" );

	reloadAnim = animArrayPickRandom( "reload" );
	stop_aiming();
	self setFlaggedAnimKnobAllRestart( "reloadanim", reloadAnim, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( reload_duration( reloadAnim ), "reloadanim" );
	self codol_actorReloadComplete();
	self.a.burstShots = 0;
	start_aiming( 0.2 );
}
