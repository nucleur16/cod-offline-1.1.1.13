// coop soldier animscripts: cover behaviour.
//
// Entered from soldier_combat once the native actor has put the soldier on
// the cover node it claimed for him (codol_actorCoverArrived; the node is
// codol_actorCoverNode, an IW4 pathnode of the base level's own SP node set
// -- src/codol/player/codol_actor_cover.c).  It plays IW4's cover
// behaviour for the node's type with C1's common_human clips:
//
//   Cover Left / Cover Right   animscripts\corner.gsc + cover_left/right.gsc:
//       hide side-on to the corner (alert idle, twitches, flinch), step out
//       to fire -- lean round the edge, or step out to the A or B position
//       depending on where the enemy is (getCornerMode) -- and step back,
//       blind fire round the corner when suppressed, look round it, reload,
//       throw a grenade round it, change stance now and then.
//   Cover Stand / Cover Crouch / Cover Crouch Window / Conceal Stand /
//   Conceal Crouch             animscripts\cover_wall.gsc: hide behind the
//       wall, pop up to fire (over the wall, stand up, lean, or round its
//       side), duck back, blind fire over it, peek, reload, grenades.
//
// The decision loop is IW4 cover_behavior.gsc main, reduced to what this
// server measures: an enemy seen from the firing position is shot at; a
// suppressed soldier (hit within 2 s, or a player firing at him) stays down,
// blind fires or reloads; an enemy out of sight is looked for, has a grenade
// lobbed at him, or is waited for.  The native frame gives the node up when
// the enemy flanks it, sees past the wall or closes in (the module is then
// replaced by the move module, which plays the cover exit); the soldier is
// "safe to change" only while hidden, so a step-out always finishes.
//
// Every clip is in C1's common_human animlist (tools/codol_coop_anim_check.py).

#include animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self endon( "killanimscript" );
	self endon( "death" );

	node = self codol_actorCoverNode();
	point = self codol_actorCoverPoint();
	if ( !isDefined( node ) || !isDefined( point ) )
		return;

	init_transitions();
	self.coop_cover_node = node;
	self.coop_cover_point = point;
	self.coop_cover_yaw = node.angles[ 1 ];
	trace_anim( "cover " + node.type + " at " + point );
	stat_bump( "cover_" + node.type );

	switch ( node.type )
	{
	case "Cover Left":
		corner_think( "left", 90 );
		break;
	case "Cover Right":
		corner_think( "right", -90 );
		break;
	case "Cover Stand":
	case "Conceal Stand":
		wall_think( "stand" );
		break;
	default:
		wall_think( "crouch" );
		break;
	}
}

// ---------------------------------------------------------------------------
// The clip sets: IW4 cover_left.gsc / cover_right.gsc set_animarray_*, and
// cover_wall.gsc setup_*_anim_array.
// ---------------------------------------------------------------------------

corner_set( dir, pose )
{
	a = [];
	if ( dir == "left" && pose == "stand" )
	{
		a[ "alert_idle" ] = %corner_standl_alert_idle;
		t = [];
		t[ 0 ] = %corner_standl_alert_twitch01;
		t[ 1 ] = %corner_standl_alert_twitch02;
		t[ 2 ] = %corner_standl_alert_twitch03;
		t[ 3 ] = %corner_standl_alert_twitch04;
		t[ 4 ] = %corner_standl_alert_twitch05;
		t[ 5 ] = %corner_standl_alert_twitch06;
		t[ 6 ] = %corner_standl_alert_twitch07;
		a[ "alert_idle_twitch" ] = t;
		a[ "alert_idle_flinch" ] = list1( %corner_standl_flinch );
		a[ "alert_to_A" ] = list1( %corner_standl_trans_alert_2_a );
		a[ "alert_to_B" ] = list1( %corner_standl_trans_alert_2_b_v2 );
		a[ "A_to_alert" ] = list1( %corner_standl_trans_a_2_alert_v2 );
		a[ "A_to_B" ] = list1( %corner_standl_trans_a_2_b_v2 );
		a[ "B_to_A" ] = list1( %corner_standl_trans_b_2_a_v2 );
		a[ "B_to_alert" ] = list1( %corner_standl_trans_b_2_alert_v2 );
		a[ "B_to_alert_reload" ] = list1( %corner_standl_reload_b_2_alert );
		a[ "lean_to_alert" ] = list1( %cornerstndl_lean_2_alert );
		a[ "alert_to_lean" ] = list1( %cornerstndl_alert_2_lean );
		a[ "reload" ] = list1( %corner_standl_reload_v1 );
		a[ "grenade_exposed" ] = %corner_standl_grenade_a;
		a[ "grenade_safe" ] = %corner_standl_grenade_b;
		a[ "blind_fire" ] = list2( %corner_standl_blindfire_v1, %corner_standl_blindfire_v2 );
		a[ "alert_to_look" ] = %corner_standl_alert_2_look;
		a[ "look_to_alert" ] = %corner_standl_look_2_alert;
		a[ "look_to_alert_fast" ] = %corner_standl_look_2_alert_fast_v1;
		a[ "look_idle" ] = %corner_standl_look_idle;
		a[ "look" ] = %corner_standl_look;
		a[ "stance_change" ] = %cornercrl_stand_2_alert;
		a[ "lean_aim_down" ] = %cornerstndl_lean_aim_2;
		a[ "lean_aim_left" ] = %cornerstndl_lean_aim_4;
		a[ "lean_aim_straight" ] = %cornerstndl_lean_aim_5;
		a[ "lean_aim_right" ] = %cornerstndl_lean_aim_6;
		a[ "lean_aim_up" ] = %cornerstndl_lean_aim_8;
		a[ "lean_idle" ] = list1( %cornerstndl_lean_idle );
		a[ "lean_single" ] = %cornerstndl_lean_fire;
		a[ "lean_fire" ] = %cornerstndl_lean_auto;
		a[ "react" ] = list1( %cornerstndl_react_a );
		return a;
	}
	if ( dir == "right" && pose == "stand" )
	{
		a[ "alert_idle" ] = %corner_standr_alert_idle;
		t = [];
		t[ 0 ] = %corner_standr_alert_twitch01;
		t[ 1 ] = %corner_standr_alert_twitch02;
		t[ 2 ] = %corner_standr_alert_twitch04;
		t[ 3 ] = %corner_standr_alert_twitch05;
		t[ 4 ] = %corner_standr_alert_twitch06;
		t[ 5 ] = %corner_standr_alert_twitch07;
		a[ "alert_idle_twitch" ] = t;
		a[ "alert_idle_flinch" ] = list2( %corner_standr_flinch, %corner_standr_flinchb );
		a[ "alert_to_A" ] = list2( %corner_standr_trans_alert_2_a, %corner_standr_trans_alert_2_a_v2 );
		a[ "alert_to_B" ] = list3( %corner_standr_trans_alert_2_b, %corner_standr_trans_alert_2_b_v2, %corner_standr_trans_alert_2_b_v3 );
		a[ "A_to_alert" ] = list1( %corner_standr_trans_a_2_alert_v2 );
		a[ "A_to_B" ] = list2( %corner_standr_trans_a_2_b, %corner_standr_trans_a_2_b_v2 );
		a[ "B_to_A" ] = list2( %corner_standr_trans_b_2_a, %corner_standr_trans_b_2_a_v2 );
		a[ "B_to_alert" ] = list3( %corner_standr_trans_b_2_alert, %corner_standr_trans_b_2_alert_v2, %corner_standr_trans_b_2_alert_v3 );
		a[ "B_to_alert_reload" ] = list1( %corner_standr_reload_b_2_alert );
		a[ "lean_to_alert" ] = list1( %cornerstndr_lean_2_alert );
		a[ "alert_to_lean" ] = list1( %cornerstndr_alert_2_lean );
		a[ "reload" ] = list1( %corner_standr_reload_v1 );
		a[ "grenade_exposed" ] = %corner_standr_grenade_a;
		a[ "grenade_safe" ] = %corner_standr_grenade_b;
		a[ "blind_fire" ] = list2( %corner_standr_blindfire_v1, %corner_standr_blindfire_v2 );
		a[ "alert_to_look" ] = %corner_standr_alert_2_look;
		a[ "look_to_alert" ] = %corner_standr_look_2_alert;
		a[ "look_to_alert_fast" ] = %corner_standr_look_2_alert_fast;
		a[ "look_idle" ] = %corner_standr_look_idle;
		a[ "look" ] = %corner_standr_look;
		a[ "stance_change" ] = %cornercrr_stand_2_alert;
		a[ "lean_aim_down" ] = %cornerstndr_lean_aim_2;
		a[ "lean_aim_left" ] = %cornerstndr_lean_aim_4;
		a[ "lean_aim_straight" ] = %cornerstndr_lean_aim_5;
		a[ "lean_aim_right" ] = %cornerstndr_lean_aim_6;
		a[ "lean_aim_up" ] = %cornerstndr_lean_aim_8;
		a[ "lean_idle" ] = list1( %cornerstndr_lean_idle );
		a[ "lean_single" ] = %cornerstndr_lean_fire;
		a[ "lean_fire" ] = %cornerstndr_lean_auto;
		a[ "react" ] = list1( %cornerstndr_react_a );
		return a;
	}
	if ( dir == "left" )
	{
		a[ "alert_idle" ] = %cornercrl_alert_idle;
		a[ "alert_idle_twitch" ] = [];
		a[ "alert_idle_flinch" ] = [];
		a[ "alert_to_A" ] = list1( %cornercrl_trans_alert_2_a );
		a[ "alert_to_B" ] = list1( %cornercrl_trans_alert_2_b );
		a[ "A_to_alert" ] = list1( %cornercrl_trans_a_2_alert );
		a[ "A_to_B" ] = list1( %cornercrl_trans_a_2_b );
		a[ "B_to_A" ] = list1( %cornercrl_trans_b_2_a );
		a[ "B_to_alert" ] = list1( %cornercrl_trans_b_2_alert );
		a[ "lean_to_alert" ] = list1( %cornercrl_lean_2_alert );
		a[ "alert_to_lean" ] = list1( %cornercrl_alert_2_lean );
		a[ "alert_to_over" ] = list1( %cornercrl_alert_2_over );
		a[ "over_to_alert" ] = list1( %cornercrl_over_2_alert );
		a[ "reload" ] = list2( %cornercrl_reloada, %cornercrl_reloadb );
		a[ "grenade_safe" ] = %cornercrl_grenadea;
		a[ "grenade_exposed" ] = %cornercrl_grenadeb;
		a[ "blind_fire" ] = [];
		a[ "look" ] = %cornercrl_look_fast;
		a[ "stance_change" ] = %cornercrl_alert_2_stand;
		a[ "lean_aim_down" ] = %cornercrl_lean_aim_2;
		a[ "lean_aim_left" ] = %cornercrl_lean_aim_4;
		a[ "lean_aim_straight" ] = %cornercrl_lean_aim_5;
		a[ "lean_aim_right" ] = %cornercrl_lean_aim_6;
		a[ "lean_aim_up" ] = %cornercrl_lean_aim_8;
		a[ "lean_idle" ] = list1( %cornercrl_lean_idle );
		a[ "lean_single" ] = %cornercrl_lean_fire;
		a[ "lean_fire" ] = %cornercrl_lean_auto;
		return a;
	}
	a[ "alert_idle" ] = %cornercrr_alert_idle;
	a[ "alert_idle_twitch" ] = list3( %cornercrr_alert_twitch_v1, %cornercrr_alert_twitch_v2, %cornercrr_alert_twitch_v3 );
	a[ "alert_idle_flinch" ] = [];
	a[ "alert_to_A" ] = list1( %cornercrr_trans_alert_2_a );
	a[ "alert_to_B" ] = list1( %cornercrr_trans_alert_2_b );
	a[ "A_to_alert" ] = list1( %cornercrr_trans_a_2_alert );
	a[ "A_to_B" ] = list1( %cornercrr_trans_a_2_b );
	a[ "B_to_A" ] = list1( %cornercrr_trans_b_2_a );
	a[ "B_to_alert" ] = list1( %cornercrr_trans_b_2_alert );
	a[ "lean_to_alert" ] = list1( %cornercrr_lean_2_alert );
	a[ "alert_to_lean" ] = list1( %cornercrr_alert_2_lean );
	a[ "alert_to_over" ] = list1( %cornercrr_alert_2_over );
	a[ "over_to_alert" ] = list1( %cornercrr_over_2_alert );
	a[ "reload" ] = list2( %cornercrr_reloada, %cornercrr_reloadb );
	a[ "grenade_safe" ] = %cornercrr_grenadea;
	a[ "grenade_exposed" ] = %cornercrr_grenadea;
	a[ "blind_fire" ] = [];
	a[ "alert_to_look" ] = %cornercrr_alert_2_look;
	a[ "look_to_alert" ] = %cornercrr_look_2_alert;
	a[ "look_to_alert_fast" ] = %cornercrr_look_2_alert_fast;
	a[ "look_idle" ] = %cornercrr_look_idle;
	a[ "stance_change" ] = %cornercrr_alert_2_stand;
	a[ "lean_aim_down" ] = %cornercrr_lean_aim_2;
	a[ "lean_aim_left" ] = %cornercrr_lean_aim_4;
	a[ "lean_aim_straight" ] = %cornercrr_lean_aim_5;
	a[ "lean_aim_right" ] = %cornercrr_lean_aim_6;
	a[ "lean_aim_up" ] = %cornercrr_lean_aim_8;
	a[ "lean_idle" ] = list1( %cornercrr_lean_idle );
	a[ "lean_single" ] = %cornercrr_lean_fire;
	a[ "lean_fire" ] = %cornercrr_lean_auto;
	return a;
}

wall_set( kind )
{
	a = [];
	if ( kind == "stand" )
	{
		a[ "hide_idle" ] = %coverstand_hide_idle;
		t = [];
		t[ 0 ] = %coverstand_hide_idle_twitch01;
		t[ 1 ] = %coverstand_hide_idle_twitch02;
		t[ 2 ] = %coverstand_hide_idle_twitch03;
		t[ 3 ] = %coverstand_hide_idle_twitch04;
		t[ 4 ] = %coverstand_hide_idle_twitch05;
		a[ "hide_idle_twitch" ] = t;
		a[ "hide_idle_flinch" ] = list4( %coverstand_react01, %coverstand_react02, %coverstand_react03, %coverstand_react04 );
		a[ "hide_2_stand" ] = %coverstand_hide_2_aim;
		a[ "stand_2_hide" ] = %coverstand_aim_2_hide;
		a[ "hide_2_over" ] = %coverstand_2_coverstandaim;
		a[ "over_2_hide" ] = %coverstandaim_2_coverstand;
		a[ "blind_fire" ] = list2( %coverstand_blindfire_1, %coverstand_blindfire_2 );
		a[ "reload" ] = %coverstand_reloada;
		a[ "look" ] = list2( %coverstand_look_quick, %coverstand_look_quick_v2 );
		a[ "grenade_safe" ] = list2( %coverstand_grenadea, %coverstand_grenadeb );
		a[ "hide_to_look" ] = %coverstand_look_moveup;
		a[ "look_idle" ] = %coverstand_look_idle;
		a[ "look_to_hide" ] = %coverstand_look_movedown;
		a[ "look_to_hide_fast" ] = %coverstand_look_movedown_fast;
		a[ "react" ] = list2( %stand_cover_reaction_a, %stand_cover_reaction_b );
		return a;
	}
	a[ "hide_idle" ] = %covercrouch_hide_idle;
	a[ "hide_idle_twitch" ] = list4( %covercrouch_twitch_1, %covercrouch_twitch_2, %covercrouch_twitch_3, %covercrouch_twitch_4 );
	a[ "hide_idle_flinch" ] = [];
	a[ "hide_2_crouch" ] = %covercrouch_hide_2_aim;
	a[ "hide_2_stand" ] = %covercrouch_hide_2_stand;
	a[ "hide_2_lean" ] = %covercrouch_hide_2_lean;
	a[ "hide_2_right" ] = %covercrouch_hide_2_right;
	a[ "hide_2_left" ] = %covercrouch_hide_2_left;
	a[ "crouch_2_hide" ] = %covercrouch_aim_2_hide;
	a[ "stand_2_hide" ] = %covercrouch_stand_2_hide;
	a[ "lean_2_hide" ] = %covercrouch_lean_2_hide;
	a[ "right_2_hide" ] = %covercrouch_right_2_hide;
	a[ "left_2_hide" ] = %covercrouch_left_2_hide;
	a[ "blind_fire" ] = list4( %covercrouch_blindfire_1, %covercrouch_blindfire_2, %covercrouch_blindfire_3, %covercrouch_blindfire_4 );
	a[ "reload" ] = %covercrouch_reload_hide;
	a[ "look" ] = list1( %covercrouch_hide_look );
	a[ "grenade_safe" ] = list2( %covercrouch_grenadea, %covercrouch_grenadeb );
	a[ "react" ] = list2( %crouch_cover_reaction_a, %crouch_cover_reaction_b );
	return a;
}

// The aim and fire set for a firing position (IW4 corner
// set_standing/crouching_animarray_aiming, cover_wall setup_additive_aim):
// the lean aims round a corner, the cover aims over a wall, else the
// exposed set of his weapon family for the pose he stands in.
aim_set( cover, mode, pose )
{
	kind = weapon_kind();
	key = kind + "_" + pose;
	if ( !isDefined( level.coop_animsets[ key ] ) )
		key = "rifle_" + pose;
	base = level.coop_animsets[ key ];
	a = [];
	keys = getArrayKeys( base );
	for ( i = 0; i < keys.size; i++ )
		a[ keys[ i ] ] = base[ keys[ i ] ];

	if ( mode == "lean" && isDefined( cover[ "lean_aim_straight" ] ) )
	{
		a[ "straight_level" ] = cover[ "lean_aim_straight" ];
		a[ "add_aim_up" ] = cover[ "lean_aim_up" ];
		a[ "add_aim_down" ] = cover[ "lean_aim_down" ];
		a[ "add_aim_left" ] = cover[ "lean_aim_left" ];
		a[ "add_aim_right" ] = cover[ "lean_aim_right" ];
		a = set_custom_fire( a, cover[ "lean_fire" ], cover[ "lean_single" ] );
		a[ "exposed_idle" ] = cover[ "lean_idle" ];
		return a;
	}
	if ( mode == "crouch" || mode == "left" || mode == "right" )
	{
		// cover_wall setup_additive_aim: kneeling over (or round) the wall.
		a[ "straight_level" ] = %covercrouch_aim5;
		a[ "add_aim_up" ] = %covercrouch_aim8_add;
		a[ "add_aim_down" ] = %covercrouch_aim2_add;
		a[ "add_aim_left" ] = %covercrouch_aim4_add;
		a[ "add_aim_right" ] = %covercrouch_aim6_add;
		return a;
	}
	if ( mode == "wall_lean" )
	{
		a[ "straight_level" ] = %covercrouch_lean_aim5;
		a[ "add_aim_up" ] = %exposed_aim_8;
		a[ "add_aim_down" ] = %exposed_aim_2;
		a[ "add_aim_left" ] = %exposed_aim_4;
		a[ "add_aim_right" ] = %exposed_aim_6;
		return a;
	}
	if ( mode == "over" )
	{
		a[ "straight_level" ] = %coverstandaim_aim5;
		a[ "add_aim_up" ] = %coverstandaim_aim8_add;
		a[ "add_aim_down" ] = %coverstandaim_aim2_add;
		a[ "add_aim_left" ] = %coverstandaim_aim4_add;
		a[ "add_aim_right" ] = %coverstandaim_aim6_add;
		a = set_custom_fire( a, %coverstandaim_autofire, %coverstandaim_fire );
		return a;
	}
	return a;
}

set_custom_fire( a, autoAnim, singleAnim )
{
	a[ "fire" ] = autoAnim;
	a[ "single" ] = list1( singleAnim );
	a[ "semi2" ] = singleAnim;
	a[ "semi3" ] = singleAnim;
	a[ "semi4" ] = singleAnim;
	a[ "semi5" ] = singleAnim;
	a[ "burst2" ] = autoAnim;
	a[ "burst3" ] = autoAnim;
	a[ "burst4" ] = autoAnim;
	a[ "burst5" ] = autoAnim;
	a[ "burst6" ] = autoAnim;
	return a;
}

// ---------------------------------------------------------------------------
// Shared cover pieces.
// ---------------------------------------------------------------------------

// IW4 GoToCover: face the node's hide direction and settle onto the node.
go_to_cover( hideAnim, hideYaw )
{
	self endon( "killanimscript" );
	self orientmode( "face angle", hideYaw );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAll( "coveranim", hideAnim, %body, 1, 0.3, 1 );
	if ( distance2d( self.origin, self.coop_cover_point ) > 1 )
		slide_to( self.coop_cover_point, 0.3 );
	for ( i = 0; i < 10 && abs( angleClamp180( self.angles[ 1 ] - hideYaw ) ) > 2; i++ )
		wait( 0.05 );
}

// IW4 GetYawToSpot from the node (positive to the node's RIGHT).
node_yaw_to( point )
{
	return angleClamp180( self.coop_cover_yaw - vectorToYaw( point - self.coop_cover_point ) );
}

// Where he would stand to shoot: a corner's step-out point, or up over
// the wall (codol_actor_cover.c fire_point).
fire_point( cornerDir, height )
{
	p = self.coop_cover_point + ( 0, 0, height );
	if ( !isDefined( cornerDir ) )
		return p;
	right = anglesToRight( ( 0, self.coop_cover_yaw, 0 ) );
	if ( cornerDir == "left" )
		return p - right * 36;
	return p + right * 36;
}

can_see_enemy_from( point )
{
	enemy = self.enemy;
	if ( !isDefined( enemy ) || !isAlive( enemy ) )
		return false;
	return sightTracePassed( point, aim_point( enemy ), false, self );
}

hide_idle( idleAnim, twitches, time )
{
	self endon( "killanimscript" );
	self.safeToChangeScript = true;
	self.coop_exit_time = getTime();
	if ( isDefined( twitches ) && twitches.size > 0 && randomInt( 3 ) == 0 )
	{
		twitch = pick( twitches );
		self setFlaggedAnimKnobAllRestart( "idle", twitch, %body, 1, 0.2, 1 );
		len = getAnimLength( twitch );
		end = getTime() + int( len * 1000 );
		while ( getTime() < end )
		{
			self.coop_exit_time = getTime();
			wait( 0.1 );
		}
	}
	else
	{
		self setFlaggedAnimKnobAll( "idle", idleAnim, %body, 1, 0.2, 1 );
		end = getTime() + int( time * 1000 );
		while ( getTime() < end )
		{
			self.coop_exit_time = getTime();
			wait( 0.1 );
		}
	}
	self.coop_exit_time = getTime();
}

// Shoot from the position he stands in until the enemy is out of his
// sight, the clip runs dry or the time is up (IW4 shootAsTold).
shoot_from_here( maxTime )
{
	self endon( "killanimscript" );
	end = getTime() + int( maxTime * 1000 );
	shotAny = false;
	while ( getTime() < end )
	{
		enemy = self.enemy;
		if ( !animscripts\soldier_combat::can_shoot( enemy ) )
			break;
		animscripts\soldier_combat::begin_engagement( enemy );
		shots = animscripts\soldier_combat::role_burst( distance( self.origin, enemy.origin ) );
		fireAnim = animscripts\soldier_combat::fire_anim_for( shots );
		self setAnimLimited( %add_idle, 0, 0.2 );
		self setAnim( %add_fire, 1, 0.1 );
		self setFlaggedAnimKnobRestart( "fireanim", fireAnim, 1, 0, 1 );
		delay = animscripts\soldier_combat::role_fire_delay();
		for ( i = 0; i < shots; i++ )
		{
			if ( !isDefined( enemy ) || !isAlive( enemy ) )
				break;
			animscripts\soldier_combat::fire_shot( enemy );
			self.coop_aim_last = getTime();
			wait( delay );
		}
		shotAny = true;
		self clearAnim( %add_fire, 0.2 );
		self setAnimLimited( %add_idle, 1, 0.2 );
		if ( self.a.burstShots >= animscripts\soldier_combat::role_clip() )
			break;
		wait( animscripts\soldier_combat::role_pause() * 0.7 );
	}
	return shotAny;
}

// IW4 blindfire: the weapon held up over or round the cover, firing blind
// at the clip's "fire" notetracks; rounds go near, rarely into, the enemy.
blind_fire( clips )
{
	self endon( "killanimscript" );
	if ( !isDefined( clips ) || clips.size == 0 || !isDefined( self.coop_weapon ) || self.coop_weapon == "" )
		return false;
	enemy = self.enemy;
	if ( !isDefined( enemy ) || !isAlive( enemy ) )
		return false;
	clip = pick( clips );
	trace_anim( "cover blind fire" );
	stat_bump( "cover_blindfire" );
	self.safeToChangeScript = false;
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "blindfire", clip, %body, 1, 0.2, 1 );
	len = getAnimLength( clip );
	times = [];
	if ( animHasNotetrack( clip, "fire" ) )
		times = getNotetrackTimes( clip, "fire" );
	if ( times.size == 0 )
	{
		for ( i = 0; i < 4; i++ )
			times[ i ] = 0.35 + i * 0.08;
	}
	elapsed = 0;
	for ( i = 0; i < times.size; i++ )
	{
		at = times[ i ] * len;
		if ( at > elapsed )
		{
			wait( at - elapsed );
			elapsed = at;
		}
		enemy = self.enemy;
		if ( !isDefined( enemy ) || !isAlive( enemy ) )
			continue;
		target = aim_point( enemy );
		start = animscripts\soldier_combat::muzzle_point( target );
		if ( randomInt( 100 ) >= 10 )
			target = animscripts\soldier_combat::miss_point( enemy, target, start ) + ( randomFloatRange( -40, 40 ), randomFloatRange( -40, 40 ), randomFloatRange( 0, 60 ) );
		end = start + vectorNormalize( target - start ) * ( distance( start, target ) + 384 );
		self codol_actorShoot( self.coop_weapon, start, end );
		self.a.burstShots++;
	}
	if ( len > elapsed )
		wait( len - elapsed );
	return true;
}

// A grenade from cover (IW4 tryThrowingGrenade with the cover's own throw).
cover_grenade( throwAnim )
{
	if ( !isDefined( throwAnim ) )
		return false;
	enemy = self.enemy;
	if ( !can_throw_grenade( enemy ) )
		return false;
	self.safeToChangeScript = false;
	threw = throw_grenade( enemy, throwAnim );
	return threw;
}

// IW4 reactions.gsc coverReaction: the first rounds to crack past a
// hidden soldier make him flinch into the wall.
cover_reaction( cover )
{
	self endon( "killanimscript" );
	if ( !isDefined( cover[ "react" ] ) || cover[ "react" ].size == 0 )
		return false;
	if ( isDefined( self.coop_next_react ) && getTime() < self.coop_next_react )
		return false;
	self.coop_next_react = getTime() + 6000 + randomInt( 6000 );
	react = pick( cover[ "react" ] );
	trace_anim( "cover reaction" );
	stat_bump( "cover_reaction" );
	self.safeToChangeScript = false;
	self setFlaggedAnimKnobAllRestart( "reactanim", react, %body, 1, 0.1, 1 );
	DoNoteTracksForTime( getAnimLength( react ), "reactanim" );
	return true;
}

cover_reload( reloadAnim )
{
	self endon( "killanimscript" );
	if ( !isDefined( reloadAnim ) )
		return false;
	trace_anim( "cover reload" );
	stat_bump( "cover_reload" );
	self.safeToChangeScript = false;
	self.a.burstShots = 0;
	self setFlaggedAnimKnobAllRestart( "reloadanim", reloadAnim, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( reloadAnim ), "reloadanim" );
	return true;
}

// IW4 cover_behavior getBoredOfThisNodeTime / cantFindAnythingToDo: a
// soldier who has not seen his enemy from this node for a while gives it up
// (avoided for 20 s) and the native actor finds him one with a view.
bored_of_node( seenTime )
{
	if ( getTime() - seenTime < self.coop_cover_patience )
		return false;
	trace_anim( "bored of cover: no sight of the enemy for " + int( ( getTime() - seenTime ) / 1000 ) + " s" );
	stat_bump( "cover_bored" );
	self codol_actorLeaveCover( 20000 );
	self.safeToChangeScript = true;
	return true;
}

needs_reload( threshold )
{
	clip = animscripts\soldier_combat::role_clip();
	return self.a.burstShots >= clip * ( 1 - threshold );
}

// ---------------------------------------------------------------------------
// Corners: IW4 corner.gsc corner_think.
// ---------------------------------------------------------------------------

corner_think( dir, hideYawOffset )
{
	self endon( "killanimscript" );
	self endon( "death" );

	node = self.coop_cover_node;
	pose = self.coop_cover_pose;
	if ( !isDefined( pose ) || ( pose != "stand" && pose != "crouch" ) )
		pose = cover_pose_for( node );
	if ( pose == "crouch" && !can_crouch() )
		pose = "stand";
	self.a.pose = pose;
	set_anim_array( pose );
	cover = corner_set( dir, pose );
	hideYaw = self.coop_cover_yaw + hideYawOffset;
	self.coop_exit_type = cover_trans_type( node.type, pose );
	self.coop_exit_yaw = self.coop_cover_yaw;

	go_to_cover( cover[ "alert_idle" ], hideYaw );
	self.coop_in_cover = dir;
	seenTime = getTime();
	self.coop_cover_patience = 8000 + randomInt( 7000 );
	nextStance = getTime() + 5000 + randomInt( 15000 );
	nextLook = getTime() + 2000 + randomInt( 4000 );
	nextBlind = getTime();

	for ( ;; )
	{
		self.coop_in_cover = dir;
		self.coop_exit_type = cover_trans_type( node.type, self.a.pose );
		enemy = self.enemy;
		if ( react_to_grenade() )
		{
			go_to_cover( cover[ "alert_idle" ], hideYaw );
			continue;
		}
		if ( isDefined( enemy ) && can_melee( enemy ) )
		{
			melee_strike( enemy );
			self.coop_cover_pose = self.a.pose;
			return;
		}
		if ( !isDefined( enemy ) )
		{
			hide_idle( cover[ "alert_idle" ], cover[ "alert_idle_twitch" ], 0.5 );
			continue;
		}

		// IW4 shouldChangeStanceForFun: a corner that allows both stances.
		if ( getTime() > nextStance && node_allows( node, "stand" ) && node_allows( node, "crouch" ) && can_crouch() )
		{
			nextStance = getTime() + 5000 + randomInt( 15000 );
			self.safeToChangeScript = false;
			newPose = "crouch";
			if ( self.a.pose == "crouch" )
				newPose = "stand";
			trace_anim( "corner stance " + self.a.pose + " -> " + newPose );
			stat_bump( "corner_stance_change" );
			changeAnim = cover[ "stance_change" ];
			self setFlaggedAnimKnobAllRestart( "changeStance", changeAnim, %body, 1, 0.2, 1 );
			DoNoteTracksForTime( getAnimLength( changeAnim ), "changeStance" );
			self.a.pose = newPose;
			set_anim_array( newPose );
			cover = corner_set( dir, newPose );
			continue;
		}

		eye = fire_point( dir, 60 );
		if ( self.a.pose == "crouch" )
			eye = fire_point( dir, 40 );
		visible = can_see_enemy_from( eye );
		suppressed = is_suppressed();
		if ( visible || suppressed )
			seenTime = getTime();
		else if ( bored_of_node( seenTime ) )
		{
			hide_idle( cover[ "alert_idle" ], undefined, 2 );
			continue;
		}

		if ( suppressed )
		{
			// IW4 suppressedBehavior: reload, blind fire, or keep down.
			if ( cover_reaction( cover ) )
				continue;
			if ( needs_reload( 0 ) && cover_reload( pick( cover[ "reload" ] ) ) )
				continue;
			if ( getTime() >= nextBlind && cover[ "blind_fire" ].size > 0 && randomInt( 2 ) )
			{
				nextBlind = getTime() + 3000 + randomInt( 9000 );
				blind_fire( cover[ "blind_fire" ] );
				continue;
			}
			if ( cover[ "alert_idle_flinch" ].size > 0 && randomInt( 3 ) == 0 )
			{
				self.safeToChangeScript = false;
				flinch = pick( cover[ "alert_idle_flinch" ] );
				self setFlaggedAnimKnobAllRestart( "idle", flinch, %body, 1, 0.1, 1 );
				DoNoteTracksForTime( getAnimLength( flinch ), "idle" );
				continue;
			}
			hide_idle( cover[ "alert_idle" ], undefined, 0.6 );
			continue;
		}
		if ( needs_reload( 0.1 ) && cover_reload( pick( cover[ "reload" ] ) ) )
			continue;

		if ( visible )
		{
			// IW4 attackVisibleEnemy: a far enemy may get a grenade first.
			if ( distance( self.origin, enemy.origin ) > 500 && randomInt( 4 ) == 0 &&
			     cover_grenade( cover[ "grenade_exposed" ] ) )
				continue;
			if ( corner_step_out_and_shoot( dir, cover, hideYaw ) )
				continue;
			hide_idle( cover[ "alert_idle" ], cover[ "alert_idle_twitch" ], 0.5 );
			continue;
		}

		// IW4 attackNothingToDo: grenade round the corner, look, wait.
		if ( randomInt( 3 ) == 0 && cover_grenade( cover[ "grenade_safe" ] ) )
			continue;
		if ( getTime() >= nextLook )
		{
			nextLook = getTime() + 4000 + randomInt( 11000 );
			if ( corner_look( cover ) )
				continue;
		}
		hide_idle( cover[ "alert_idle" ], cover[ "alert_idle_twitch" ], 0.5 + randomFloat( 1.0 ) );
	}
}

// IW4 getCornerMode: lean when the enemy is just round the edge, else
// step out to B (near the corner) or A (farther round).
corner_mode( dir )
{
	yaw = node_yaw_to( self.enemy.origin );
	cutoff = 38;
	if ( self.a.pose == "crouch" )
		cutoff = 31;
	if ( dir == "left" )
	{
		if ( yaw >= -40 && yaw <= 0 && ( randomInt( 3 ) == 0 || is_suppressed() ) )
			return "lean";
		if ( yaw < 14 )
		{
			if ( yaw < 0 - cutoff )
				return "A";
			return "B";
		}
		if ( yaw <= 0 )
			return "lean";
		return undefined;
	}
	if ( yaw >= 0 && yaw <= 40 && ( randomInt( 3 ) == 0 || is_suppressed() ) )
		return "lean";
	if ( yaw > -12 )
	{
		if ( yaw > cutoff )
			return "A";
		return "B";
	}
	if ( yaw >= 0 )
		return "lean";
	return undefined;
}

corner_step_out_and_shoot( dir, cover, hideYaw )
{
	self endon( "killanimscript" );
	mode = corner_mode( dir );
	if ( !isDefined( mode ) )
		return false;
	if ( self.a.pose == "crouch" && mode != "lean" && isDefined( cover[ "alert_to_over" ] ) && randomInt( 3 ) == 0 )
		mode = "over";
	key = "alert_to_" + mode;
	if ( !isDefined( cover[ key ] ) || cover[ key ].size == 0 )
		return false;
	stepAnim = pick( cover[ key ] );
	// IW4 isPathClear: the step-out must not walk into a wall.
	if ( mode != "lean" && mode != "over" )
	{
		endPos = delta_end( self.origin, self.angles[ 1 ], getMoveDelta( stepAnim, 0, 1 ) );
		if ( !may_move( self.origin, endPos ) )
			return false;
	}

	trace_anim( "corner step out " + mode );
	stat_bump( "corner_stepout_" + mode );
	self.safeToChangeScript = false;
	self.coop_in_cover = undefined;
	self orientmode( "face current" );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "stepout", stepAnim, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( stepAnim ), "stepout" );

	aimMode = mode;
	if ( mode == "over" )
		aimMode = "crouch";
	self.a.array = aim_set( cover, aimMode, self.a.pose );
	if ( mode != "lean" )
		self orientmode( "face enemy" );
	start_aiming( 0.2 );
	shoot_from_here( 2.0 + randomFloat( 2.5 ) );
	// IW4 changeStepOutPos: the enemy moved across the A/B line -- step
	// over to the other position and keep firing.
	if ( ( mode == "A" || mode == "B" ) && isDefined( self.enemy ) && !needs_reload( 0.3 ) )
	{
		other = corner_mode( dir );
		if ( isDefined( other ) && ( other == "A" || other == "B" ) && other != mode && isDefined( cover[ mode + "_to_" + other ] ) )
		{
			switchAnim = pick( cover[ mode + "_to_" + other ] );
			trace_anim( "corner step " + mode + " -> " + other );
			stat_bump( "corner_switch_" + mode + "_to_" + other );
			stop_aiming();
			self orientmode( "face current" );
			self setFlaggedAnimKnobAllRestart( "changeStepOutPos", switchAnim, %body, 1, 0.2, 1 );
			DoNoteTracksForTime( getAnimLength( switchAnim ), "changeStepOutPos" );
			mode = other;
			self.a.array = aim_set( cover, mode, self.a.pose );
			self orientmode( "face enemy" );
			start_aiming( 0.2 );
			shoot_from_here( 1.5 + randomFloat( 2.0 ) );
		}
	}
	stop_aiming();
	set_anim_array( self.a.pose );

	// IW4 returnToCover, the reload variant when the clip ran low.
	backKey = mode + "_to_alert";
	reloading = false;
	if ( mode == "B" && needs_reload( 0.3 ) && isDefined( cover[ "B_to_alert_reload" ] ) && randomInt( 4 ) < 3 )
	{
		backKey = "B_to_alert_reload";
		reloading = true;
	}
	backAnim = pick( cover[ backKey ] );
	self orientmode( "face current" );
	self setFlaggedAnimKnobAllRestart( "hide", backAnim, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( backAnim ), "hide" );
	if ( reloading )
		self.a.burstShots = 0;
	go_to_cover( cover[ "alert_idle" ], hideYaw );
	self.coop_in_cover = dir;
	return true;
}

corner_look( cover )
{
	self endon( "killanimscript" );
	if ( isDefined( cover[ "alert_to_look" ] ) )
	{
		trace_anim( "corner look" );
		stat_bump( "corner_look" );
		self.safeToChangeScript = false;
		self setFlaggedAnimKnobAllRestart( "looking_start", cover[ "alert_to_look" ], %body, 1, 0.2, 1 );
		DoNoteTracksForTime( getAnimLength( cover[ "alert_to_look" ] ), "looking_start" );
		self setFlaggedAnimKnobAll( "looking", cover[ "look_idle" ], %body, 1, 0.1, 1 );
		end = getTime() + 1000 + randomInt( 1500 );
		while ( getTime() < end )
		{
			if ( isDefined( self.enemy ) && can_see_enemy_from( self getEye() ) )
				break;
			wait( 0.1 );
		}
		back = cover[ "look_to_alert" ];
		if ( is_suppressed() )
			back = cover[ "look_to_alert_fast" ];
		self setFlaggedAnimKnobAllRestart( "looking_end", back, %body, 1, 0.1, 1 );
		DoNoteTracksForTime( getAnimLength( back ), "looking_end" );
		return true;
	}
	if ( isDefined( cover[ "look" ] ) )
	{
		trace_anim( "corner fast look" );
		stat_bump( "corner_fastlook" );
		self.safeToChangeScript = false;
		self setFlaggedAnimKnobAllRestart( "look", cover[ "look" ], %body, 1, 0.1, 1 );
		DoNoteTracksForTime( getAnimLength( cover[ "look" ] ), "look" );
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Walls: IW4 cover_wall.gsc cover_wall_think.
// ---------------------------------------------------------------------------

wall_think( kind )
{
	self endon( "killanimscript" );
	self endon( "death" );

	node = self.coop_cover_node;
	if ( kind == "crouch" && !can_crouch() )
		kind = "stand";
	self.a.pose = kind;
	set_anim_array( kind );
	cover = wall_set( kind );
	hideYaw = self.coop_cover_yaw;
	self.coop_exit_type = cover_trans_type( node.type, kind );
	self.coop_exit_yaw = self.coop_cover_yaw;

	go_to_cover( cover[ "hide_idle" ], hideYaw );
	self.coop_in_cover = kind;
	seenTime = getTime();
	self.coop_cover_patience = 8000 + randomInt( 7000 );
	nextLook = getTime() + 2000 + randomInt( 4000 );
	nextBlind = getTime();

	for ( ;; )
	{
		self.coop_in_cover = kind;
		enemy = self.enemy;
		if ( react_to_grenade() )
		{
			go_to_cover( cover[ "hide_idle" ], hideYaw );
			continue;
		}
		if ( isDefined( enemy ) && can_melee( enemy ) )
		{
			melee_strike( enemy );
			return;
		}
		if ( !isDefined( enemy ) )
		{
			hide_idle( cover[ "hide_idle" ], cover[ "hide_idle_twitch" ], 0.5 );
			continue;
		}

		height = 64;
		if ( kind == "crouch" )
			height = 46;
		visible = can_see_enemy_from( fire_point( undefined, height ) );
		suppressed = is_suppressed();
		if ( visible || suppressed )
			seenTime = getTime();
		else if ( bored_of_node( seenTime ) )
		{
			hide_idle( cover[ "hide_idle" ], undefined, 2 );
			continue;
		}

		if ( suppressed )
		{
			if ( cover_reaction( cover ) )
				continue;
			if ( needs_reload( 0 ) && cover_reload( cover[ "reload" ] ) )
				continue;
			if ( getTime() >= nextBlind && randomInt( 2 ) )
			{
				nextBlind = getTime() + 3000 + randomInt( 9000 );
				blind_fire( cover[ "blind_fire" ] );
				continue;
			}
			if ( cover[ "hide_idle_flinch" ].size > 0 && randomInt( 3 ) == 0 )
			{
				self.safeToChangeScript = false;
				flinch = pick( cover[ "hide_idle_flinch" ] );
				self setFlaggedAnimKnobAllRestart( "idle", flinch, %body, 1, 0.1, 1 );
				DoNoteTracksForTime( getAnimLength( flinch ), "idle" );
				continue;
			}
			hide_idle( cover[ "hide_idle" ], undefined, 0.6 );
			continue;
		}
		if ( needs_reload( 0.1 ) && cover_reload( cover[ "reload" ] ) )
			continue;

		if ( visible )
		{
			if ( distance( self.origin, enemy.origin ) > 500 && randomInt( 4 ) == 0 &&
			     cover_grenade( pick( cover[ "grenade_safe" ] ) ) )
				continue;
			if ( wall_pop_up_and_shoot( kind, cover, hideYaw ) )
				continue;
			hide_idle( cover[ "hide_idle" ], cover[ "hide_idle_twitch" ], 0.5 );
			continue;
		}

		if ( randomInt( 3 ) == 0 && cover_grenade( pick( cover[ "grenade_safe" ] ) ) )
			continue;
		if ( getTime() >= nextLook )
		{
			nextLook = getTime() + 4000 + randomInt( 11000 );
			if ( wall_look( cover ) )
				continue;
		}
		hide_idle( cover[ "hide_idle" ], cover[ "hide_idle_twitch" ], 0.5 + randomFloat( 1.0 ) );
	}
}

// IW4 getBestCoverMode / getShootPosPitch.
wall_mode( kind )
{
	if ( kind == "stand" )
	{
		if ( randomInt( 2 ) )
			return "over";
		return "stand";
	}
	enemy = self.enemy;
	pitch = angleClamp180( vectorToAngles( aim_point( enemy ) - ( self.coop_cover_point + ( 0, 0, 40 ) ) )[ 0 ] );
	// Negative pitch looks up.
	if ( pitch < -30 )
		return "lean";
	if ( pitch < -15 )
		return "stand";
	r = randomInt( 10 );
	if ( r < 6 )
		return "crouch";
	if ( r < 8 )
		return "left";
	return "right";
}

wall_pop_up_and_shoot( kind, cover, hideYaw )
{
	self endon( "killanimscript" );
	mode = wall_mode( kind );
	upAnim = cover[ "hide_2_" + mode ];
	if ( !isDefined( upAnim ) )
		return false;
	endPos = delta_end( self.origin, self.angles[ 1 ], getMoveDelta( upAnim, 0, 1 ) );
	if ( distance2d( endPos, self.origin ) > 8 && !may_move( self.origin, endPos ) )
		return false;

	trace_anim( "wall pop up " + mode );
	stat_bump( "wall_popup_" + kind + "_" + mode );
	self.safeToChangeScript = false;
	self.coop_in_cover = undefined;
	self orientmode( "face current" );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "pop_up", upAnim, %body, 1, 0.1, 1 );
	DoNoteTracksForTime( getAnimLength( upAnim ), "pop_up" );

	aimPose = "crouch";
	aimMode = mode;
	if ( mode == "stand" )
		aimPose = "stand";
	if ( kind == "stand" )
		aimPose = "stand";
	if ( mode == "lean" )
		aimMode = "wall_lean";
	self.a.pose = aimPose;
	self.a.array = aim_set( cover, aimMode, aimPose );
	self orientmode( "face enemy" );
	start_aiming( 0.2 );
	shoot_from_here( 2.0 + randomFloat( 2.5 ) );
	stop_aiming();

	downAnim = cover[ mode + "_2_hide" ];
	self orientmode( "face current" );
	self setFlaggedAnimKnobAll( "go_to_hide", downAnim, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( downAnim ), "go_to_hide" );
	self.a.pose = kind;
	set_anim_array( kind );
	go_to_cover( cover[ "hide_idle" ], hideYaw );
	self.coop_in_cover = kind;
	return true;
}

wall_look( cover )
{
	self endon( "killanimscript" );
	self.safeToChangeScript = false;
	if ( isDefined( cover[ "hide_to_look" ] ) )
	{
		trace_anim( "wall look" );
		stat_bump( "wall_look" );
		self setFlaggedAnimKnobAll( "looking_start", cover[ "hide_to_look" ], %body, 1, 0.2, 1 );
		DoNoteTracksForTime( getAnimLength( cover[ "hide_to_look" ] ), "looking_start" );
		self setFlaggedAnimKnobAll( "looking", cover[ "look_idle" ], %body, 1, 0.1, 1 );
		end = getTime() + 1000 + randomInt( 1500 );
		while ( getTime() < end )
		{
			if ( isDefined( self.enemy ) && can_see_enemy_from( self getEye() ) )
				break;
			wait( 0.1 );
		}
		back = cover[ "look_to_hide" ];
		if ( is_suppressed() )
			back = cover[ "look_to_hide_fast" ];
		self setFlaggedAnimKnobAllRestart( "looking_end", back, %body, 1, 0.1, 1 );
		DoNoteTracksForTime( getAnimLength( back ), "looking_end" );
		return true;
	}
	look = pick( cover[ "look" ] );
	trace_anim( "wall fast look" );
	stat_bump( "wall_fastlook" );
	self setFlaggedAnimKnobAllRestart( "look", look, %body, 1, 0.1, 1 );
	DoNoteTracksForTime( getAnimLength( look ), "look" );
	return true;
}
