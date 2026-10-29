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
	trace_anim( "death by " + self.damageMod + " at " + self.damageLocation + " pose " + self.a.pose + " moving " + self.a.movement );
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
    if ( isDefined( self.coop_wounded_death_anim ) )
        return self.coop_wounded_death_anim;
    if ( isDefined( self.coop_wounded_kind ) )
    {
        if ( self.coop_wounded_kind == "crawl" )
            return pick( list2( %dying_crawl_death_v1, %dying_crawl_death_v2 ) );
        return pick( list4( %dying_back_death_v1, %dying_back_death_v2, %dying_back_death_v3, %dying_back_death_v4 ) );
    }

	if ( !isDefined( self.a.pose ) )
		self.a.pose = "stand";
	if ( !isDefined( self.damageLocation ) )
		self.damageLocation = "none";
	yaw = iw4_damage_yaw();

	// IW4 traverse\shared handleTraverseDeathNotetrack: killed on a wall or
	// a car, the crossing's own death for the half he was in.
	if ( isDefined( self.coop_traverse_death ) )
	{
		phase = 0;
		if ( isDefined( self.coop_traverse_phase ) )
			phase = self.coop_traverse_phase;
		stat_bump( "traverse_death" );
		return pick( self.coop_traverse_death[ phase ] );
	}

	if ( was_damaged_by_explosive() && self.damageLocation == "none" )
		return explode_death_anim( yaw );

	// IW4 death.gsc a.special: hidden at a cover node he dies in its pose.
	if ( isDefined( self.coop_in_cover ) )
	{
		d = cover_death_anim();
		if ( isDefined( d ) )
		{
			stat_bump( "cover_death_" + self.coop_in_cover );
			return d;
		}
	}

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

// IW4 death.gsc special deaths at cover (a.special cover_left / cover_right
// / cover_stand / cover_crouch), standing or crouched at a corner.
cover_death_anim()
{
	crouched = isDefined( self.a.pose ) && self.a.pose == "crouch";
	switch ( self.coop_in_cover )
	{
	case "left":
		if ( crouched )
			return pick( list2( %cornercrl_death_side, %cornercrl_death_back ) );
		return pick( list2( %corner_standl_deatha, %corner_standl_deathb ) );
	case "right":
		if ( crouched )
			return pick( list2( %cornercrr_alert_death_slideout, %cornercrr_alert_death_back ) );
		return pick( list2( %corner_standr_deatha, %corner_standr_deathb ) );
	case "stand":
		return pick( list2( %coverstand_death_left, %coverstand_death_right ) );
	case "crouch":
		return pick( list3( %covercrouch_death_1, %covercrouch_death_2, %covercrouch_death_3 ) );
	}
	return undefined;
}

// IW4 pain.gsc crawlingPain is selected by the PAIN script while the soldier
// still has low positive health. Native wounded state holds combat movement,
// but preserves damage, live actor identity, targeting and wave membership.
// A finishing hit or the sequence's real bleedout damage causes death once.
try_wounded()
{
    self endon( "death" );
    if ( !isAlive( self ) || self.health > 100 || self.health >= self.maxhealth * 0.4 )
        return false;
	self endon( "killanimscript" );
	if ( getDvar( "scr_coop_ai_long_death" ) == "0" )
		return false;
	if ( isDefined( self.coop_traverse_death ) || isDefined( self.coop_in_cover ) )
		return false;
	if ( !isDefined( self.damageMod ) || ( self.damageMod != "MOD_RIFLE_BULLET" && self.damageMod != "MOD_PISTOL_BULLET" ) )
		return false;
	// IW4 pain.gsc crawlingPain: not a head or hand hit, no player within
	// 175 units, and at most one every 3 s across the match.
	if ( damageLocationIsAny( "head", "helmet", "neck", "right_hand", "left_hand" ) )
		return false;
	players = getEntArray( "player", "classname" );
	for ( i = 0; i < players.size; i++ )
		if ( isAlive( players[ i ] ) && distance( self.origin, players[ i ].origin ) < 175 )
			return false;
	if ( isDefined( level.coop_next_long_death ) && getTime() < level.coop_next_long_death )
		return false;
	if ( randomInt( 100 ) >= 25 )
		return false;
	level.coop_next_long_death = getTime() + 3000;

	crouched = isDefined( self.a.pose ) && self.a.pose == "crouch";
	crawl = randomInt( 2 ) == 0;
	if ( crawl )
	{
		down = pick( list3( %dying_stand_2_crawl_v1, %dying_stand_2_crawl_v2, %dying_stand_2_crawl_v3 ) );
		if ( crouched )
			down = %dying_crouch_2_crawl;
	}
	else
	{
		down = pick( list2( %dying_stand_2_back_v1, %dying_stand_2_back_v2 ) );
		if ( crouched )
			down = %dying_crouch_2_back;
	}
	kind = "back";
	if ( crawl )
		kind = "crawl";
	// IW4 isCrawlDeltaAllowed: room on the ground for the fall.
	if ( !may_move( self.origin, delta_end( self.origin, self.angles[ 1 ], getMoveDelta( down, 0, 1 ) ) ) )
		return false;
    if ( !self codol_actorBeginWounded() )
        return false;
    self.coop_wounded_kind = kind;
	trace_anim( "wounded (" + kind + ")" );
	stat_bump( "wounded" );
	self animmode( "gravity" );
	self orientmode( "face current" );
	self setFlaggedAnimKnobAllRestart( "painanim", down, %body, 1, 0.1, 1 );
	DoNoteTracksForTime( getAnimLength( down ), "painanim" );
	if ( crawl )
	{
		cycles = 1 + randomInt( 3 );
		self setFlaggedAnimKnobAll( "painanim", %dying_crawl, %body, 1, 0.2, 1 );
		DoNoteTracksForTime( getAnimLength( %dying_crawl ) * cycles, "painanim" );
		final = pick( list2( %dying_crawl_death_v1, %dying_crawl_death_v2 ) );
	}
	else
	{
		self setFlaggedAnimKnobAll( "painanim", %dying_back_idle, %body, 1, 0.2, 1 );
		DoNoteTracksForTime( 1.0 + randomFloat( 1.5 ), "painanim" );
		twitch = pick( list2( %dying_back_twitch_a, %dying_back_twitch_b ) );
		self setFlaggedAnimKnobAllRestart( "painanim", twitch, %body, 1, 0.2, 1 );
		DoNoteTracksForTime( getAnimLength( twitch ), "painanim" );
		final = pick( list4( %dying_back_death_v1, %dying_back_death_v2, %dying_back_death_v3, %dying_back_death_v4 ) );
	}
    self.coop_wounded_death_anim = final;
    self codol_actorFinishWounded();
    // A refused real damage (e.g. script invulnerability) is not death.
    if ( isAlive( self ) )
    {
        self.coop_wounded_kind = undefined;
        self.coop_wounded_death_anim = undefined;
        self animmode( "normal", false );
    }
    return true;
}
