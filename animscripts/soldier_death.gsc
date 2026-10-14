// coop soldier animscript: soldier_death.
//
// IW4 animscripts\death.gsc getDeathAnim: the explosive deaths by the side
// the blast came from, the strong-hit deaths (a shotgun at close range, a
// blast of damage), the running deaths, the standing deaths chosen by where
// he was hit and from which side (pistol carriers their own), the crouched
// deaths.  The native frame runs this module once health reaches zero and
// turns the actor into a corpse when the thread ends
// (CODOL_ActorRunFrame / ActorBecomeCorpse), so the clip is played to its
// length and the module returns.  self.damageLocation / damageMod come from
// the "damage" notify of the killing hit (soldier_shared damage_watch).

#include animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self endon( "killanimscript" );

	if ( isDefined( self.a ) && isDefined( self.a.nodeath ) && self.a.nodeath )
	{
		wait( 0.1 );
		return;
	}

	stop_aiming();
	deathAnim = get_death_anim();

	self animmode( "gravity" );
	self orientmode( "face current" );
	self setFlaggedAnimKnobAllRestart( "deathanim", deathAnim, %body, 1, 0.05, 1 );

	length = getAnimLength( deathAnim );
	if ( !animHasNotetrack( deathAnim, "start_ragdoll" ) )
		self thread ragdoll_after( length * 0.6 );

	DoNoteTracksForTime( length, "deathanim" );
}

ragdoll_after( delay )
{
	self endon( "killanimscript" );
	wait( delay );
	if ( isDefined( self.coop_allow_ragdoll ) && self.coop_allow_ragdoll )
		self startRagdoll();
}

get_death_anim()
{
	if ( !isDefined( self.a.pose ) )
		self.a.pose = "stand";
	if ( !isDefined( self.damageLocation ) )
		self.damageLocation = "none";
	yaw = iw4_damage_yaw();

	if ( was_damaged_by_explosive() && self.damageLocation == "none" )
		return explode_death_anim( yaw );

	if ( strong_bullet_damage() )
	{
		d = strong_bullet_death_anim( yaw );
		if ( isDefined( d ) )
			return d;
	}

	if ( self.a.pose == "crouch" )
		return crouch_death_anim();
	if ( self.a.movement == "run" )
		return running_death_anim();
	return stand_death_anim( yaw );
}

// IW4 shouldDoStrongBulletDamage, for the hits this server can tell apart: a
// huge hit, or a shotgun from inside 512 units.
strong_bullet_damage()
{
	if ( self.a.pose == "prone" )
		return false;
	if ( isDefined( self.damageMod ) && self.damageMod == "MOD_MELEE" )
		return false;
	if ( isDefined( self.damageTaken ) && self.damageTaken > 500 )
		return true;
	if ( self.a.movement == "run" && randomInt( 100 ) < 65 )
		return false;
	if ( isDefined( self.damageWeapon ) && self.damageWeapon != "" && self.damageWeapon != "none" &&
	     weaponClass( self.damageWeapon ) == "spread" && isDefined( self.coop_last_attacker ) &&
	     distance( self.origin, self.coop_last_attacker.origin ) < 512 )
		return true;
	return false;
}

// IW4 getStrongBulletDamageDeathAnim (may return undefined).
strong_bullet_death_anim( yaw )
{
	a = abs( yaw );
	if ( a < 45 )
		return undefined;
	d = [];
	if ( a > 150 )
	{
		if ( damageLocationIsAny( "left_leg_upper", "left_leg_lower", "right_leg_upper", "right_leg_lower", "left_foot", "right_foot" ) )
		{
			d[ d.size ] = %death_shotgun_legs;
			d[ d.size ] = %death_stand_sniper_leg;
		}
		else
		{
			if ( self.damageLocation == "torso_lower" )
			{
				d[ d.size ] = %death_shotgun_legs;
				d[ d.size ] = %death_stand_sniper_leg;
			}
			d[ d.size ] = %death_shotgun_back_v1;
			d[ d.size ] = %exposed_death_blowback;
			d[ d.size ] = %death_stand_sniper_chest1;
			d[ d.size ] = %death_stand_sniper_chest2;
			d[ d.size ] = %death_stand_sniper_spin1;
		}
	}
	else if ( yaw < 0 )
	{
		d = list4( %death_shotgun_spinl, %death_stand_sniper_spin1, %death_stand_sniper_chest1, %death_stand_sniper_chest2 );
	}
	else
	{
		d = list4( %death_shotgun_spinr, %death_stand_sniper_spin2, %death_stand_sniper_chest1, %death_stand_sniper_chest2 );
	}
	return pick( d );
}

// IW4 getRunningForwardDeathAnim.
running_death_anim()
{
	return pick( list4( %run_death_facedown, %run_death_roll, %run_death_fallonback, %run_death_flop ) );
}

// IW4 getStandPistolDeathAnim.
stand_pistol_death_anim( yaw )
{
	d = [];
	if ( abs( yaw ) < 50 )
	{
		d[ d.size ] = %pistol_death_2;
		return pick( d );
	}
	if ( abs( yaw ) < 110 )
		d[ d.size ] = %pistol_death_2;
	if ( damageLocationIsAny( "torso_lower", "torso_upper", "left_leg_upper", "left_leg_lower", "right_leg_upper", "right_leg_lower" ) )
	{
		d[ d.size ] = %pistol_death_3;
		if ( !damageLocationIsAny( "torso_upper" ) )
			d[ d.size ] = %pistol_death_3;
	}
	if ( !damageLocationIsAny( "head", "neck", "helmet", "left_foot", "right_foot", "left_hand" ) &&
	     !damageLocationIsAny( "right_hand", "gun" ) && randomInt( 2 ) == 0 )
		d[ d.size ] = %pistol_death_4;
	if ( d.size == 0 || damageLocationIsAny( "torso_lower", "torso_upper", "neck", "head", "helmet", "right_arm_upper" ) ||
	     damageLocationIsAny( "left_arm_upper" ) )
		d[ d.size ] = %pistol_death_1;
	return pick( d );
}

// IW4 getStandDeathAnim.  yaw: IW4's damageyaw (+-180 = hit from the front).
stand_death_anim( yaw )
{
	if ( weapon_kind() == "pistol" )
		return stand_pistol_death_anim( yaw );
	if ( weapon_kind() == "rpg" )
		return pick( list2( %rpg_stand_death, %rpg_stand_death_stagger ) );

	d = [];
	e = [];

	if ( damageLocationIsAny( "torso_lower", "left_leg_upper", "left_leg_lower", "right_leg_lower", "right_leg_upper" ) )
	{
		d[ d.size ] = %exposed_death_groin;
		d[ d.size ] = %stand_death_leg;
		e[ e.size ] = %stand_death_crotch;
		e[ e.size ] = %stand_death_guts;
	}
	if ( damageLocationIsAny( "head", "helmet" ) )
	{
		d[ d.size ] = %exposed_death_headshot;
		d[ d.size ] = %exposed_death_flop;
	}
	if ( damageLocationIsAny( "neck" ) )
		d[ d.size ] = %exposed_death_neckgrab;
	if ( damageLocationIsAny( "torso_upper", "left_arm_upper" ) )
	{
		d[ d.size ] = %exposed_death_twist;
		d[ d.size ] = %stand_death_shoulder_spin;
		d[ d.size ] = %stand_death_shoulderback;
	}
	if ( damageLocationIsAny( "torso_upper" ) )
	{
		d[ d.size ] = %stand_death_tumbleforward;
		d[ d.size ] = %stand_death_stumbleforward;
		e[ e.size ] = %stand_death_fallside;
	}

	if ( yaw > 135 || yaw <= -135 )
	{
		// Front quadrant.
		if ( damageLocationIsAny( "neck", "head", "helmet" ) )
		{
			d[ d.size ] = %stand_death_face;
			d[ d.size ] = %stand_death_headshot_slowfall;
			e[ e.size ] = %stand_death_head_straight_back;
		}
		if ( damageLocationIsAny( "torso_upper" ) )
		{
			d[ d.size ] = %stand_death_tumbleback;
			e[ e.size ] = %stand_death_chest_stunned;
		}
	}
	else if ( yaw > -45 && yaw <= 45 )
	{
		// Back quadrant.
		d[ d.size ] = %exposed_death_falltoknees;
		d[ d.size ] = %exposed_death_falltoknees_02;
	}

	if ( d.size == 0 || randomInt( 100 ) < 15 )
	{
		d[ d.size ] = %exposed_death_02;
		d[ d.size ] = %exposed_death_nerve;
	}
	// IW4's firing deaths need the clip's own fire notetracks to shoot; this
	// species does not fire from notetracks, so they are left out.
	if ( d.size == 0 )
		d[ d.size ] = %exposed_death;

	index = randomInt( d.size + e.size );
	if ( index < d.size )
		return d[ index ];
	return e[ index - d.size ];
}

// IW4 getCrouchDeathAnim.
crouch_death_anim()
{
	d = [];
	if ( damageLocationIsAny( "head", "neck" ) )
		d[ d.size ] = %exposed_crouch_death_fetal;
	if ( damageLocationIsAny( "torso_upper", "torso_lower", "left_arm_upper", "right_arm_upper", "neck" ) )
		d[ d.size ] = %exposed_crouch_death_flip;
	if ( d.size < 2 )
		d[ d.size ] = %exposed_crouch_death_twist;
	if ( d.size < 2 )
		d[ d.size ] = %exposed_crouch_death_flip;
	return pick( d );
}

// IW4 playExplodeDeathAnim: thrown away from the blast.
explode_death_anim( yaw )
{
	if ( self.a.movement != "run" )
	{
		if ( yaw > 135 || yaw <= -135 )
			return pick( list4( %death_explosion_stand_b_v1, %death_explosion_stand_b_v2, %death_explosion_stand_b_v3, %death_explosion_stand_b_v4 ) );
		if ( yaw > 45 && yaw <= 135 )
			return pick( list3( %death_explosion_stand_l_v1, %death_explosion_stand_l_v2, %death_explosion_stand_l_v3 ) );
		if ( yaw > -45 && yaw <= 45 )
			return pick( list4( %death_explosion_stand_f_v1, %death_explosion_stand_f_v2, %death_explosion_stand_f_v3, %death_explosion_stand_f_v4 ) );
		return pick( list2( %death_explosion_stand_r_v1, %death_explosion_stand_r_v2 ) );
	}
	if ( yaw > 135 || yaw <= -135 )
		return pick( list2( %death_explosion_run_b_v1, %death_explosion_run_b_v2 ) );
	if ( yaw > 45 && yaw <= 135 )
		return pick( list2( %death_explosion_run_l_v1, %death_explosion_run_l_v2 ) );
	if ( yaw > -45 && yaw <= 45 )
		return pick( list4( %death_explosion_run_f_v1, %death_explosion_run_f_v2, %death_explosion_run_f_v3, %death_explosion_run_f_v4 ) );
	return pick( list2( %death_explosion_run_r_v1, %death_explosion_run_r_v2 ) );
}
