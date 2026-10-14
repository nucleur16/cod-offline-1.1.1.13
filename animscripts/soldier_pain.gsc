// coop soldier animscript: soldier_pain.
//
// IW4 animscripts\pain.gsc getPainAnim, one flinch per hit, rate-limited
// (a soldier does not re-flinch inside 1.5 s): the running pains while he
// runs, the standing pains chosen by where he was hit (pistol carriers their
// own set), the crouched pains when he is down on a knee.  self.damageLocation
// comes from the "damage" notify (soldier_shared damage_watch), as IW4's
// code filled it.  The pains that end on a knee leave him crouched, as their
// IW4 anim_pose notetracks did.

#include animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self endon( "killanimscript" );
	self endon( "death" );

	self setFlashBanged( false );

	if ( !isDefined( self.allowpain ) || !self.allowpain )
		return;

	now = getTime();
	if ( isDefined( self.a.painTime ) && now - self.a.painTime < 1500 )
		return;
	self.a.painTime = now;

	stop_aiming();
	painAnim = get_pain_anim();

	self animmode( "gravity" );
	self setFlaggedAnimKnobAllRestart( "painanim", painAnim, %body, 1, 0.1, 1 );
	DoNoteTracksForTime( getAnimLength( painAnim ), "painanim" );
	if ( self.coop_pain_ends_crouched )
		self.a.pose = "crouch";
	self animmode( "normal", false );
}

get_pain_anim()
{
	self.coop_pain_ends_crouched = false;
	if ( !isDefined( self.a.pose ) )
		self.a.pose = "stand";
	// IW4 pain.gsc special pains: hidden at a cover node he flinches in its
	// pose and stays there.
	if ( isDefined( self.coop_in_cover ) )
	{
		p = cover_pain_anim();
		if ( isDefined( p ) )
		{
			stat_bump( "cover_pain_" + self.coop_in_cover );
			return p;
		}
	}
	if ( self.a.pose == "crouch" && can_crouch() )
	{
		self.a.movement = "stop";
		self.coop_pain_ends_crouched = true;
		return crouch_pain_anim();
	}
	self.a.pose = "stand";
	if ( self.a.movement == "run" )
		return running_pain_anim();
	self.a.movement = "stop";
	if ( weapon_kind() == "pistol" )
		return stand_pistol_pain_anim();
	return stand_pain_anim();
}

// IW4 getRunningForwardPainAnim, its medium and short pains (the long ones
// travel 300 units, more than an MP street has room for).
running_pain_anim()
{
	p = [];
	p[ p.size ] = %run_pain_fallonknee_02;
	p[ p.size ] = %run_pain_stomach;
	p[ p.size ] = %run_pain_stumble;
	p[ p.size ] = %run_pain_stomach_fast;
	p[ p.size ] = %run_pain_leg_fast;
	p[ p.size ] = %run_pain_fall;
	p[ p.size ] = %run_pain_fallonknee;
	p[ p.size ] = %run_pain_fallonknee_03;
	return pick( p );
}

// IW4 getStandPistolPainAnim.
stand_pistol_pain_anim()
{
	p = [];
	if ( damageLocationIsAny( "torso_upper", "torso_lower", "left_arm_upper", "right_arm_upper", "neck" ) )
		p[ p.size ] = %pistol_stand_pain_chest;
	if ( damageLocationIsAny( "torso_lower", "left_leg_upper", "right_leg_upper" ) )
		p[ p.size ] = %pistol_stand_pain_groin;
	if ( damageLocationIsAny( "head", "neck" ) )
		p[ p.size ] = %pistol_stand_pain_head;
	if ( damageLocationIsAny( "left_arm_lower", "left_arm_upper", "torso_upper" ) )
		p[ p.size ] = %pistol_stand_pain_leftshoulder;
	if ( damageLocationIsAny( "right_arm_lower", "right_arm_upper", "torso_upper" ) )
		p[ p.size ] = %pistol_stand_pain_rightshoulder;
	if ( p.size < 2 )
		p[ p.size ] = %pistol_stand_pain_chest;
	if ( p.size < 2 )
		p[ p.size ] = %pistol_stand_pain_groin;
	return pick( p );
}

// IW4 getStandPainAnim.  pc / ec mark the entries of p / e that end on a
// knee.
stand_pain_anim()
{
	p = [];
	e = [];
	pc = [];
	ec = [];

	if ( damageLocationIsAny( "torso_upper", "torso_lower" ) )
	{
		e[ e.size ] = %stand_exposed_extendedpain_gut;
		e[ e.size ] = %stand_exposed_extendedpain_stomach;
	}
	if ( damageLocationIsAny( "torso_upper", "head", "helmet", "neck" ) )
	{
		p[ p.size ] = %exposed_pain_face;
		p[ p.size ] = %stand_exposed_extendedpain_neck;
		ec[ e.size ] = true;
		e[ e.size ] = %stand_exposed_extendedpain_head_2_crouch;
	}
	if ( damageLocationIsAny( "right_arm_upper", "right_arm_lower" ) )
		p[ p.size ] = %exposed_pain_right_arm;
	if ( damageLocationIsAny( "left_arm_lower", "left_arm_upper" ) )
	{
		p[ p.size ] = %stand_exposed_extendedpain_shoulderswing;
		ec[ e.size ] = true;
		e[ e.size ] = %stand_exposed_extendedpain_shoulder_2_crouch;
	}
	if ( damageLocationIsAny( "torso_lower", "left_leg_upper", "right_leg_upper" ) )
	{
		p[ p.size ] = %exposed_pain_groin;
		p[ p.size ] = %stand_exposed_extendedpain_hip;
		ec[ e.size ] = true;
		e[ e.size ] = %stand_exposed_extendedpain_hip_2_crouch;
		ec[ e.size ] = true;
		e[ e.size ] = %stand_exposed_extendedpain_feet_2_crouch;
		e[ e.size ] = %stand_exposed_extendedpain_stomach;
	}
	if ( damageLocationIsAny( "left_foot", "right_foot", "left_leg_lower", "right_leg_lower" ) )
	{
		p[ p.size ] = %stand_exposed_extendedpain_thigh;
		ec[ e.size ] = true;
		e[ e.size ] = %stand_exposed_extendedpain_feet_2_crouch;
	}
	if ( p.size < 2 )
	{
		pc[ p.size ] = true;
		p[ p.size ] = %exposed_pain_2_crouch;
		p[ p.size ] = %stand_extendedpainb;
	}
	if ( e.size < 2 )
	{
		e[ e.size ] = %stand_extendedpainc;
		e[ e.size ] = %stand_exposed_extendedpain_chest;
	}

	index = randomInt( p.size + e.size );
	if ( index < p.size )
	{
		self.coop_pain_ends_crouched = isDefined( pc[ index ] );
		return p[ index ];
	}
	index = index - p.size;
	self.coop_pain_ends_crouched = isDefined( ec[ index ] );
	return e[ index ];
}

// IW4 getCrouchPainAnim.
crouch_pain_anim()
{
	p = [];
	p[ p.size ] = %exposed_crouch_extendedpaina;
	p[ p.size ] = %exposed_crouch_pain_chest;
	p[ p.size ] = %exposed_crouch_pain_headsnap;
	p[ p.size ] = %exposed_crouch_pain_flinch;
	if ( damageLocationIsAny( "left_hand", "left_arm_lower", "left_arm_upper" ) )
		p[ p.size ] = %exposed_crouch_pain_left_arm;
	if ( damageLocationIsAny( "right_hand", "right_arm_lower", "right_arm_upper" ) )
		p[ p.size ] = %exposed_crouch_pain_right_arm;
	return pick( p );
}

// IW4 pain.gsc getCornerStandLPain / getCornerStandRPain / the crouched
// corner pains / cover_stand and cover_crouch pains.
cover_pain_anim()
{
	crouched = isDefined( self.a.pose ) && self.a.pose == "crouch";
	self.a.movement = "stop";
	switch ( self.coop_in_cover )
	{
	case "left":
		if ( crouched )
		{
			self.coop_pain_ends_crouched = true;
			return %cornercrl_painb;
		}
		return pick( list4( %corner_standl_painb, %corner_standl_painc, %corner_standl_paind, %corner_standl_paine ) );
	case "right":
		if ( crouched )
		{
			self.coop_pain_ends_crouched = true;
			return pick( list2( %cornercrr_alert_paina, %cornercrr_alert_painc ) );
		}
		return pick( list3( %corner_standr_pain, %corner_standr_painb, %corner_standr_painc ) );
	case "stand":
		return pick( list2( %coverstand_pain_groin, %coverstand_pain_leg ) );
	case "crouch":
		self.coop_pain_ends_crouched = true;
		return pick( list3( %covercrouch_pain_front, %covercrouch_pain_left_3, %covercrouch_pain_right ) );
	}
	return undefined;
}
