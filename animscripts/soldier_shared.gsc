// coop soldier animscripts: shared helpers.
//
// The coop species' counterpart of IW4's animscripts\shared.gsc,
// animscripts\animset.gsc and the exposed half of combat_utility.gsc.  It
// names no multiplayer or zm module: everything is local or a builtin, so the
// coop script closure stays its own.
//
// WHICH ANIMATIONS (MEASURED 2026-10-13, tools/codol_coop_anim_check.py):
// C1 composes a coop soldier on its generic_human tree from the animlists
// common, common_actor_shared and common_human (common_gametype_coop) plus
// the level's own list.  common_human is IW4's SP soldier set, node for node:
// all 184 animations IW4's init_animset_* tables name are in it, and so are
// IW4's exposed pain, death, run, walk, stance-change and twitch clips.  The
// tables below are those IW4 tables.  Nothing here may name an animation
// outside those lists: the actor animation channel sends server tree entry
// indices, and C1 prunes every node its lists do not name, so one extra name
// shifts every later index and the soldier plays the WRONG clip.  Run the
// checker after editing any %anim in the coop closure.

#using_animtree ( "generic_human" );

// IW4 animscripts\shared.gsc HandleNoteTrack, the subset whose effect this
// server can produce.
HandleNoteTrack( note, flagName )
{
	if ( isSubStr( note, "sndnt#" ) )
		return undefined;

	switch ( note )
	{
	case "end":
	case "finish":
	case "undefined":
		return note;
	case "start_ragdoll":
	case "ragdoll":
		if ( isDefined( self.coop_allow_ragdoll ) && self.coop_allow_ragdoll )
			self startRagdoll();
		break;
	case "pose_stand":
		self.a.pose = "stand";
		break;
	case "pose_crouch":
		self.a.pose = "crouch";
		break;
	case "no pain":
		self.allowpain = false;
		break;
	case "allow pain":
		self.allowpain = true;
		break;
	}
	return undefined;
}

DoNoteTracks( flagName )
{
	for ( ;; )
	{
		self waittill( flagName, note );
		if ( !isDefined( note ) )
			note = "undefined";
		val = self HandleNoteTrack( note, flagName );
		if ( isDefined( val ) )
			return val;
	}
}

DoNoteTracksForTime( time, flagName )
{
	pump = spawnStruct();
	self thread notetrack_pump( flagName, pump );
	wait( time );
	pump notify( "stop_notetracks" );
}

notetrack_pump( flagName, pump )
{
	self endon( "killanimscript" );
	self endon( "death" );
	pump endon( "stop_notetracks" );
	for ( ;; )
	{
		self waittill( flagName, note );
		if ( !isDefined( note ) )
			note = "undefined";
		self HandleNoteTrack( note, flagName );
	}
}

// ---------------------------------------------------------------------------
// Lists.  array() is not a registered builtin here.
// ---------------------------------------------------------------------------

list1( a )
{
	r = [];
	r[ 0 ] = a;
	return r;
}

list2( a, b )
{
	r = list1( a );
	r[ 1 ] = b;
	return r;
}

list3( a, b, c )
{
	r = list2( a, b );
	r[ 2 ] = c;
	return r;
}

list4( a, b, c, d )
{
	r = list3( a, b, c );
	r[ 3 ] = d;
	return r;
}

pick( list )
{
	return list[ randomInt( list.size ) ];
}

// ---------------------------------------------------------------------------
// The animsets: IW4 animscripts\animset.gsc init_animset_*, by weapon and pose.
// ---------------------------------------------------------------------------

init_animsets()
{
	if ( isDefined( level.coop_animsets ) )
		return;

	sets = [];
	sets[ "rifle_stand" ] = animset_rifle_stand();
	sets[ "rifle_crouch" ] = animset_rifle_crouch();
	sets[ "pistol_stand" ] = animset_pistol_stand();
	sets[ "shotgun_stand" ] = animset_shotgun_stand();
	sets[ "shotgun_crouch" ] = animset_shotgun_crouch();
	sets[ "rpg_stand" ] = animset_rpg_stand();
	sets[ "rpg_crouch" ] = animset_rpg_crouch();
	sets[ "cqb_stand" ] = animset_cqb_stand();
	level.coop_animsets = sets;
}

// IW4 set_animarray_burst_and_semi_fire_stand.
set_burst_and_semi_stand( a )
{
	a[ "burst2" ] = %exposed_shoot_burst3;
	a[ "burst3" ] = %exposed_shoot_burst3;
	a[ "burst4" ] = %exposed_shoot_burst4;
	a[ "burst5" ] = %exposed_shoot_burst5;
	a[ "burst6" ] = %exposed_shoot_burst6;
	a[ "semi2" ] = %exposed_shoot_semi2;
	a[ "semi3" ] = %exposed_shoot_semi3;
	a[ "semi4" ] = %exposed_shoot_semi4;
	a[ "semi5" ] = %exposed_shoot_semi5;
	return a;
}

// IW4 set_animarray_burst_and_semi_fire_crouch.
set_burst_and_semi_crouch( a )
{
	a[ "burst2" ] = %exposed_crouch_shoot_burst3;
	a[ "burst3" ] = %exposed_crouch_shoot_burst3;
	a[ "burst4" ] = %exposed_crouch_shoot_burst4;
	a[ "burst5" ] = %exposed_crouch_shoot_burst5;
	a[ "burst6" ] = %exposed_crouch_shoot_burst6;
	a[ "semi2" ] = %exposed_crouch_shoot_semi2;
	a[ "semi3" ] = %exposed_crouch_shoot_semi3;
	a[ "semi4" ] = %exposed_crouch_shoot_semi4;
	a[ "semi5" ] = %exposed_crouch_shoot_semi5;
	return a;
}

// One clip for every burst and semi length (IW4
// set_animarray_custom_burst_and_semi_fire_*).
set_custom_burst_and_semi( a, fireAnim )
{
	a[ "burst2" ] = fireAnim;
	a[ "burst3" ] = fireAnim;
	a[ "burst4" ] = fireAnim;
	a[ "burst5" ] = fireAnim;
	a[ "burst6" ] = fireAnim;
	a[ "semi2" ] = fireAnim;
	a[ "semi3" ] = fireAnim;
	a[ "semi4" ] = fireAnim;
	a[ "semi5" ] = fireAnim;
	return a;
}

// IW4 set_animarray_stance_change (stand and crouch; this species never goes
// prone).
set_stance_change( a )
{
	a[ "crouch_2_stand" ] = %exposed_crouch_2_stand;
	a[ "stand_2_crouch" ] = %exposed_stand_2_crouch;
	return a;
}

animset_rifle_stand()
{
	a = [];
	a[ "pose" ] = "stand";
	a[ "add_aim_up" ] = %exposed_aim_8;
	a[ "add_aim_down" ] = %exposed_aim_2;
	a[ "add_aim_left" ] = %exposed_aim_4;
	a[ "add_aim_right" ] = %exposed_aim_6;
	a[ "straight_level" ] = %exposed_aim_5;
	a[ "fire" ] = %exposed_shoot_auto_v3;
	a[ "single" ] = list1( %exposed_shoot_semi1 );
	a = set_burst_and_semi_stand( a );
	a[ "exposed_idle" ] = list3( %exposed_idle_alert_v1, %exposed_idle_alert_v2, %exposed_idle_alert_v3 );
	// IW4 animscripts\reactions.gsc: the standing exposed twitches.
	a[ "twitch" ] = list2( %exposed_idle_twitch, %exposed_idle_twitch_v4 );
	a[ "reload" ] = list2( %exposed_reload, %exposed_reloadb );
	a = set_stance_change( a );
	return a;
}

// IW4 animset init_animset_cqb_stand: a soldier a mission put in CQB
// (self.coop_cqb) aims and reloads with the CQB stand set.
animset_cqb_stand()
{
	a = animset_rifle_stand();
	a[ "add_aim_up" ] = %cqb_stand_aim8;
	a[ "add_aim_down" ] = %cqb_stand_aim2;
	a[ "add_aim_left" ] = %cqb_stand_aim4;
	a[ "add_aim_right" ] = %cqb_stand_aim6;
	a[ "straight_level" ] = %cqb_stand_aim5;
	a[ "reload" ] = list2( %cqb_stand_reload_steady, %cqb_stand_reload_knee );
	a[ "twitch" ] = list1( %cqb_stand_twitch );
	return a;
}

animset_rifle_crouch()
{
	a = [];
	a[ "pose" ] = "crouch";
	a[ "add_aim_up" ] = %exposed_crouch_aim_8;
	a[ "add_aim_down" ] = %exposed_crouch_aim_2;
	a[ "add_aim_left" ] = %exposed_crouch_aim_4;
	a[ "add_aim_right" ] = %exposed_crouch_aim_6;
	a[ "straight_level" ] = %exposed_crouch_aim_5;
	a[ "fire" ] = %exposed_crouch_shoot_auto_v2;
	a[ "single" ] = list1( %exposed_crouch_shoot_semi1 );
	a = set_burst_and_semi_crouch( a );
	a[ "exposed_idle" ] = list3( %exposed_crouch_idle_alert_v1, %exposed_crouch_idle_alert_v2, %exposed_crouch_idle_alert_v3 );
	a[ "twitch" ] = list2( %exposed_crouch_idle_twitch_v2, %exposed_crouch_idle_twitch_v3 );
	a[ "reload" ] = list1( %exposed_crouch_reload );
	a = set_stance_change( a );
	return a;
}

animset_pistol_stand()
{
	a = [];
	a[ "pose" ] = "stand";
	a[ "add_aim_up" ] = %pistol_stand_aim_8_add;
	a[ "add_aim_down" ] = %pistol_stand_aim_2_add;
	a[ "add_aim_left" ] = %pistol_stand_aim_4_add;
	a[ "add_aim_right" ] = %pistol_stand_aim_6_add;
	a[ "straight_level" ] = %pistol_stand_aim_5;
	a[ "fire" ] = %pistol_stand_fire_a;
	a[ "single" ] = list1( %pistol_stand_fire_a );
	a = set_custom_burst_and_semi( a, %pistol_stand_fire_a );
	a[ "exposed_idle" ] = list3( %exposed_idle_alert_v1, %exposed_idle_alert_v2, %exposed_idle_alert_v3 );
	a[ "twitch" ] = [];
	a[ "reload" ] = list1( %pistol_stand_reload_a );
	// IW4 never crouches a sidearm (set_animarray_crouching swaps to the
	// primary first); a pistol-only soldier stands.
	return a;
}

animset_shotgun_stand()
{
	a = [];
	a[ "pose" ] = "stand";
	a[ "add_aim_up" ] = %shotgun_aim_8;
	a[ "add_aim_down" ] = %shotgun_aim_2;
	a[ "add_aim_left" ] = %shotgun_aim_4;
	a[ "add_aim_right" ] = %shotgun_aim_6;
	a[ "straight_level" ] = %shotgun_aim_5;
	a[ "fire" ] = %exposed_shoot_auto_v3;
	a[ "single" ] = list2( %shotgun_stand_fire_1a, %shotgun_stand_fire_1b );
	a = set_burst_and_semi_stand( a );
	a[ "exposed_idle" ] = list3( %exposed_idle_alert_v1, %exposed_idle_alert_v2, %exposed_idle_alert_v3 );
	a[ "twitch" ] = list2( %exposed_idle_twitch, %exposed_idle_twitch_v4 );
	// ( C is standing, want it more often )
	a[ "reload" ] = list4( %shotgun_stand_reload_a, %shotgun_stand_reload_b, %shotgun_stand_reload_c, %shotgun_stand_reload_c );
	a = set_stance_change( a );
	return a;
}

animset_shotgun_crouch()
{
	a = animset_rifle_crouch();
	a[ "single" ] = list1( %shotgun_crouch_fire );
	a[ "reload" ] = list1( %shotgun_crouch_reload );
	return a;
}

animset_rpg_stand()
{
	a = [];
	a[ "pose" ] = "stand";
	a[ "add_aim_up" ] = %rpg_stand_aim_8;
	a[ "add_aim_down" ] = %rpg_stand_aim_2;
	a[ "add_aim_left" ] = %rpg_stand_aim_4;
	a[ "add_aim_right" ] = %rpg_stand_aim_6;
	a[ "straight_level" ] = %rpg_stand_aim_5;
	a[ "fire" ] = %rpg_stand_fire;
	a[ "single" ] = list1( %rpg_stand_fire );
	a = set_custom_burst_and_semi( a, %rpg_stand_fire );
	a[ "exposed_idle" ] = list1( %rpg_stand_idle );
	a[ "twitch" ] = [];
	a[ "reload" ] = list1( %rpg_stand_reload );
	a = set_stance_change( a );
	return a;
}

animset_rpg_crouch()
{
	a = [];
	a[ "pose" ] = "crouch";
	a[ "add_aim_up" ] = %rpg_crouch_aim_8;
	a[ "add_aim_down" ] = %rpg_crouch_aim_2;
	a[ "add_aim_left" ] = %rpg_crouch_aim_4;
	a[ "add_aim_right" ] = %rpg_crouch_aim_6;
	a[ "straight_level" ] = %rpg_crouch_aim_5;
	a[ "fire" ] = %rpg_crouch_fire;
	a[ "single" ] = list1( %rpg_crouch_fire );
	a = set_custom_burst_and_semi( a, %rpg_crouch_fire );
	a[ "exposed_idle" ] = list1( %rpg_crouch_idle );
	a[ "twitch" ] = [];
	a[ "reload" ] = list1( %rpg_crouch_reload );
	a = set_stance_change( a );
	return a;
}

// Which weapon family his hands hold: IW4 picks the set from the weapon in
// them (usingSidearm, usingRocketLauncher, weapon_pump_action_shotgun), not
// from the authored role -- the coop "rpg" role carries a rifle, and an
// automatic shotgun (aa12) aims and fires like a rifle in IW4.  A mission or
// aitype may force a family with self.coop_anim_kind.
weapon_kind()
{
	if ( isDefined( self.coop_anim_kind ) )
		return self.coop_anim_kind;

	kind = "rifle";
	w = self.coop_weapon;
	if ( isDefined( w ) && w != "" )
	{
		cls = weaponClass( w );
		if ( cls == "pistol" )
			kind = "pistol";
		else if ( cls == "rocketlauncher" )
			kind = "rpg";
		else if ( cls == "spread" && weaponIsBoltAction( w ) )
			kind = "shotgun";
	}
	else if ( isDefined( self.coop_role ) && self.coop_role == "pistol" )
	{
		kind = "pistol";
	}
	self.coop_anim_kind = kind;
	return kind;
}

can_crouch()
{
	return weapon_kind() != "pistol";
}

// IW4 set_animarray_standing / set_animarray_crouching.
set_anim_array( pose )
{
	init_animsets();
	if ( pose == "crouch" && !can_crouch() )
		pose = "stand";
	kind = weapon_kind();
	key = kind + "_" + pose;
	if ( isDefined( self.coop_cqb ) && kind == "rifle" && pose == "stand" )
		key = "cqb_stand";
	if ( !isDefined( level.coop_animsets[ key ] ) )
		key = "rifle_" + pose;
	self.a.array = level.coop_animsets[ key ];
	return pose;
}

animArray( name )
{
	return self.a.array[ name ];
}

animArrayPickRandom( name )
{
	return pick( self.a.array[ name ] );
}

// ---------------------------------------------------------------------------
// Exposed aiming: IW4 combat_utility setupAim, aimIdleThread and
// shared::trackLoop.
// ---------------------------------------------------------------------------

// The standing (or crouched) exposed pose, the soldier's weapon up.  The
// straight pose is a complete one under %exposed_aiming; the four aim
// directions are additive groups (aim_2/4/6/8) whose weights the tracking
// loop drives.
setup_aim( transTime )
{
	if ( !isDefined( transTime ) )
		transTime = 0.2;
	if ( !isDefined( self.a.array ) )
		self.a.pose = set_anim_array( self.a.pose );
	self setAnimKnobAll( %exposed_aiming, %body, 1, transTime, 1 );
	self setAnimKnobLimited( animArray( "straight_level" ), 1, transTime );
	self setAnimKnobLimited( animArray( "add_aim_up" ), 1, transTime );
	self setAnimKnobLimited( animArray( "add_aim_down" ), 1, transTime );
	self setAnimKnobLimited( animArray( "add_aim_left" ), 1, transTime );
	self setAnimKnobLimited( animArray( "add_aim_right" ), 1, transTime );
	self.coop_aim_pitch = 0;
	self.coop_aim_yaw = 0;
	self.coop_aim_w = [];
}

// The pose every non-moving state blends to (the old name; missions and the
// modules call it).
stand_pose( blend )
{
	if ( !isDefined( blend ) )
		blend = 0.2;
	if ( !isDefined( self.a.array ) )
		self.a.pose = set_anim_array( self.a.pose );
	setup_aim( blend );
}

// IW4 aimIdleThread: the alert idle plays in the additive %add_idle group
// over the aim pose, a new variant each time one ends.
aim_idle_thread()
{
	self endon( "killanimscript" );
	self endon( "death" );
	self endon( "end_aim_idle" );

	wait( 0.1 );
	self setAnimLimited( %add_idle, 1, 0.2 );
	// Unflagged: the fire layer zeroes %add_idle during a burst, and a flagged
	// set under a zero-weight parent is a fatal "0 effective goal weight"
	// script error.  The loop times itself by the clip's length instead.
	for ( ;; )
	{
		idle = animArrayPickRandom( "exposed_idle" );
		self setAnimKnobLimited( idle, 1, 0.2 );
		len = getAnimLength( idle );
		if ( len < 0.5 )
			len = 0.5;
		wait( len );
	}
}

end_aim_idle()
{
	self notify( "end_aim_idle" );
	self clearAnim( %add_idle, 0.2 );
}

// Where the soldier aims from and at.
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

shoot_from_height()
{
	if ( isDefined( self.a.pose ) && self.a.pose == "crouch" )
		return 40;
	return 58;
}

// IW4 trackLoop: the yaw and pitch from the soldier's facing to his target
// drive the additive aim groups (aim_4/aim_6 left/right, aim_2/aim_8
// down/up).  The weights are sent only when they change by more than a
// couple of degrees' worth, every 0.1 s, so a still target costs nothing.
aim_track_thread()
{
	self endon( "killanimscript" );
	self endon( "death" );
	self endon( "stop tracking" );

	if ( isDefined( self.a.pose ) && self.a.pose == "stand" )
	{
		up = 60;
		down = -60;
	}
	else
	{
		up = 45;
		down = -45;
	}
	right = 45;
	left = -45;

	for ( ;; )
	{
		pitch = 0;
		yaw = 0;
		enemy = self.enemy;
		if ( isDefined( enemy ) )
		{
			from = self.origin + ( 0, 0, shoot_from_height() );
			v = aim_point( enemy ) - from;
			ang = vectorToAngles( v );
			pitch = angleClamp180( 0 - ang[ 0 ] );
			yaw = angleClamp180( self.angles[ 1 ] - ang[ 1 ] );
			// IW4 anim.maxAngleCheckYawDelta: well off to the side, the body
			// turns instead; the arms go back to centre.
			if ( abs( yaw ) > 65 || abs( pitch ) > 65 )
			{
				pitch = 0;
				yaw = 0;
			}
			if ( yaw > right )
				yaw = right;
			if ( yaw < left )
				yaw = left;
			if ( pitch > up )
				pitch = up;
			if ( pitch < down )
				pitch = down;
		}
		set_aim_weights( pitch, yaw, up, down, left, right );
		wait( 0.1 );
	}
}

set_aim_weight( group, weight )
{
	if ( isDefined( self.coop_aim_w[ group ] ) && abs( self.coop_aim_w[ group ] - weight ) < 0.04 )
		return;
	self.coop_aim_w[ group ] = weight;
	if ( group == "2" )
		self setAnimLimited( %aim_2, weight, 0.1, 1 );
	else if ( group == "4" )
		self setAnimLimited( %aim_4, weight, 0.1, 1 );
	else if ( group == "6" )
		self setAnimLimited( %aim_6, weight, 0.1, 1 );
	else
		self setAnimLimited( %aim_8, weight, 0.1, 1 );
}

// IW4 trackLoop_setAnimWeights.
set_aim_weights( pitch, yaw, up, down, left, right )
{
	if ( yaw >= 0 )
	{
		set_aim_weight( "4", 0 );
		set_aim_weight( "6", yaw / right );
	}
	else
	{
		set_aim_weight( "6", 0 );
		set_aim_weight( "4", yaw / left );
	}
	if ( pitch >= 0 )
	{
		set_aim_weight( "2", 0 );
		set_aim_weight( "8", pitch / up );
	}
	else
	{
		set_aim_weight( "8", 0 );
		set_aim_weight( "2", pitch / down );
	}
}

start_aiming( transTime )
{
	self notify( "stop tracking" );
	self notify( "end_aim_idle" );
	setup_aim( transTime );
	self thread aim_track_thread();
	self thread aim_idle_thread();
	self.coop_aiming = true;
}

stop_aiming()
{
	self.coop_aiming = undefined;
	self notify( "stop tracking" );
	end_aim_idle();
	self clearAnim( %add_fire, 0.1 );
}

// IW4 stance change in the exposed state (combat.gsc transitionTo).  Returns
// the pose he ends in.
change_pose( newPose )
{
	if ( newPose == "crouch" && !can_crouch() )
		newPose = "stand";
	if ( !isDefined( self.a.pose ) )
		self.a.pose = "stand";
	if ( self.a.pose == newPose )
	{
		set_anim_array( newPose );
		return newPose;
	}

	stop_aiming();
	// The set of the pose he is leaving (unset when combat is his first
	// module since spawning).
	self.a.pose = set_anim_array( self.a.pose );
	if ( self.a.pose == newPose )
	{
		start_aiming( 0.2 );
		return newPose;
	}
	transAnim = animArray( self.a.pose + "_2_" + newPose );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "trans", transAnim, %body, 1, 0.2, 1.2 );
	DoNoteTracksForTime( getAnimLength( transAnim ) / 1.2, "trans" );
	self.a.pose = newPose;
	set_anim_array( newPose );
	start_aiming( 0.2 );
	return newPose;
}

// ---------------------------------------------------------------------------
// Idles away from a fight: IW4 animscripts\utility setEnv idleAnimArray and
// _patrol's bored idles.
// ---------------------------------------------------------------------------

// One looping idle and its twitches, chosen once per soldier, as IW4 picks
// one idle set per AI; a soldier who has just walked a patrol stops with the
// bored patrol idles (IW4 _patrol).
casual_idle_set( fromPatrol )
{
	if ( isDefined( fromPatrol ) && fromPatrol )
	{
		// IW4 _patrol: the bored idle, or its smoking / phone variants.
		s = [];
		s[ "idle" ] = pick( list3( %patrol_bored_idle, %patrol_bored_idle_smoke, %patrol_bored_idle_cellphone ) );
		s[ "twitch" ] = list3( %patrol_bored_twitch_stretch, %patrol_bored_twitch_checkphone, %patrol_bored_twitch_bug );
		return s;
	}
	if ( isDefined( self.coop_casual_set ) )
		return self.coop_casual_set;
	s = [];
	if ( randomInt( 2 ) )
	{
		s[ "idle" ] = %casual_stand_idle;
		s[ "twitch" ] = list2( %casual_stand_idle_twitch, %casual_stand_idle_twitchb );
	}
	else
	{
		s[ "idle" ] = %casual_stand_v2_idle;
		s[ "twitch" ] = list3( %casual_stand_v2_twitch_radio, %casual_stand_v2_twitch_shift, %casual_stand_v2_twitch_talk );
	}
	self.coop_casual_set = s;
	return s;
}

// A soldier with no living enemy and no route stands at ease; with an enemy
// he holds the exposed aim with its alert idle.
idle_pose( blend )
{
	if ( !isDefined( blend ) )
		blend = 0.3;
	if ( isDefined( self.enemy ) )
	{
		stand_pose( blend );
		return;
	}
	if ( isDefined( self.a.pose ) && self.a.pose == "crouch" )
		self setFlaggedAnimKnobAll( "idleanim", %casual_crouch_idle, %body, 1, blend, 1 );
	else
		self setFlaggedAnimKnobAll( "idleanim", casual_idle_set()[ "idle" ], %body, 1, blend, 1 );
}

// A cast member's pose (_cooptraining.gsc): the gesture in coop_pose_anim
// when one is set, played from its start, else the looping idle in
// coop_idle_anim.  Both come from animlists every coop client loads
// (common_human / common_actor_shared), never from the playlist animlist.
cast_pose( blend )
{
	if ( isDefined( self.coop_pose_anim ) )
	{
		self setFlaggedAnimKnobAll( "idleanim", self.coop_pose_anim, %body, 1, blend, 1 );
		self setAnimTime( self.coop_pose_anim, 0 );
		return;
	}
	idle = %casual_stand_idle;
	if ( isDefined( self.coop_idle_anim ) )
		idle = self.coop_idle_anim;
	self setFlaggedAnimKnobAll( "idleanim", idle, %body, 1, blend, 1 );
}

// ---------------------------------------------------------------------------
// Where he was hit: the actor "damage" notify names the struck bone, which
// IW4's code turned into self.damageLocation for pain.gsc and death.gsc.
// ---------------------------------------------------------------------------

damage_watch()
{
	self endon( "death" );
	self notify( "coop_damage_watch" );
	self endon( "coop_damage_watch" );
	self.damageLocation = "none";
	self.damageMod = "MOD_UNKNOWN";
	for ( ;; )
	{
		// (amount, attacker, dir, point, mod, model, tag, part, dflags,
		// weapon): _g_combat_mp.asm G_Damage's CoDOL arm.
		self waittill( "damage", amount, attacker, dir, point, mod, modelName, tagName, partName, iDFlags, weapon );
		record_damage( amount, attacker, mod, partName, weapon );
	}
}

record_damage( amount, attacker, mod, partName, weapon )
{
	self.damageLocation = bone_location( partName );
	if ( isDefined( mod ) )
		self.damageMod = mod;
	self.damageTaken = amount;
	self.coop_hit_time = getTime();
	self.damageWeapon = "none";
	if ( isDefined( weapon ) )
		self.damageWeapon = weapon;
	self.coop_last_attacker = attacker;
}

// IW hit locations from the struck bone of the soldier's skeleton.
bone_location( part )
{
	if ( !isDefined( part ) || part == "" )
		return "none";
	switch ( part )
	{
	case "j_head":
	case "j_head_end":
	case "j_helmet":
	case "tag_eye":
	case "j_jaw":
		return "head";
	case "j_neck":
		return "neck";
	case "j_spine4":
	case "j_spineupper":
	case "j_clavicle_le":
	case "j_clavicle_ri":
	case "tag_inhand":
		return "torso_upper";
	case "j_spinelower":
	case "j_mainroot":
	case "pelvis":
		return "torso_lower";
	case "j_shoulder_le":
		return "left_arm_upper";
	case "j_shoulder_ri":
		return "right_arm_upper";
	case "j_elbow_le":
		return "left_arm_lower";
	case "j_elbow_ri":
		return "right_arm_lower";
	case "j_wrist_le":
		return "left_hand";
	case "j_wrist_ri":
		return "right_hand";
	case "j_hip_le":
		return "left_leg_upper";
	case "j_hip_ri":
		return "right_leg_upper";
	case "j_knee_le":
		return "left_leg_lower";
	case "j_knee_ri":
		return "right_leg_lower";
	case "j_ankle_le":
	case "j_ball_le":
		return "left_foot";
	case "j_ankle_ri":
	case "j_ball_ri":
		return "right_foot";
	}
	if ( isSubStr( part, "head" ) )
		return "head";
	if ( isSubStr( part, "spine" ) )
		return "torso_upper";
	return "torso_lower";
}

damageLocationIsAny( a, b, c, d, e, f )
{
	loc = self.damageLocation;
	if ( !isDefined( loc ) )
		return false;
	if ( isDefined( a ) && loc == a )
		return true;
	if ( isDefined( b ) && loc == b )
		return true;
	if ( isDefined( c ) && loc == c )
		return true;
	if ( isDefined( d ) && loc == d )
		return true;
	if ( isDefined( e ) && loc == e )
		return true;
	if ( isDefined( f ) && loc == f )
		return true;
	return false;
}

// IW4's self.damageyaw is the yaw of the damage DIRECTION relative to his
// facing (+-180 = from the front).  The native field is the yaw TOWARD the
// attacker, 0..360 (codol_actor_lifecycle.cpp ActorRecordDamage).
iw4_damage_yaw()
{
	return angleClamp180( self.damageyaw + 180 );
}

was_damaged_by_explosive()
{
	if ( !isDefined( self.damageMod ) )
		return false;
	switch ( self.damageMod )
	{
	case "MOD_GRENADE":
	case "MOD_GRENADE_SPLASH":
	case "MOD_PROJECTILE":
	case "MOD_PROJECTILE_SPLASH":
	case "MOD_EXPLOSIVE":
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Dev trace (+set scr_coop_anim_trace 1): one line per state a soldier
// enters (the clip names themselves come from the native trace,
// +set codol_animTrace 1, which also tags a clip that resolved to the null
// animation).
// ---------------------------------------------------------------------------

// Probe counters (+set scr_coop_test 1; _cooptest "animstats" prints them).
stat_bump( name )
{
	if ( !getDvarInt( "scr_coop_test" ) )
		return;
	if ( !isDefined( level.coop_anim_stats ) )
		level.coop_anim_stats = [];
	if ( !isDefined( level.coop_anim_stats[ name ] ) )
		level.coop_anim_stats[ name ] = 0;
	level.coop_anim_stats[ name ]++;
}

trace_anim( what )
{
	if ( getDvarInt( "scr_coop_anim_trace" ) == 0 )
		return;
	codolprintln( "CoDOL coop anim: ent " + self getEntityNumber() + " " + what );
}

// ---------------------------------------------------------------------------
// Cover use.  The native actor picks and claims cover nodes
// (codol_actorUseCover / codol_actorCoverNode, codol_actor_cover.c) for an
// enemy soldier the missions are not scripting; a cast member a mission
// drives (coop_cast), a friendly, or one marked coop_no_cover fights where
// he is told.  scr_coop_ai_cover 0 turns cover off for everyone.
// ---------------------------------------------------------------------------

update_cover_use()
{
	allowed = true;
	if ( getDvar( "scr_coop_ai_cover" ) == "0" )
		allowed = false;
	else if ( isDefined( self.coop_cast ) || isDefined( self.coop_no_cover ) )
		allowed = false;
	else if ( !isDefined( self.team ) || self.team != "axis" )
		allowed = false;
	if ( !isDefined( self.coop_cover_allowed ) || self.coop_cover_allowed != allowed )
	{
		self.coop_cover_allowed = allowed;
		self codol_actorUseCover( allowed );
	}
	return allowed;
}

// ---------------------------------------------------------------------------
// IW4 animscripts\init_move_transitions.gsc: the cover arrivals (coverTrans),
// the exits (coverExit) and the exposed starts and stops, with each clip's
// root motion and yaw change, built once per match.  Direction indices are
// IW4's numpad layout seen from the node: 2 is straight in from the front,
// 8 from behind, 4/6 from the sides, 1/3/7/9 the diagonals.
// ---------------------------------------------------------------------------

init_transitions()
{
	if ( isDefined( level.coop_trans ) )
		return;

	t = spawnStruct();
	t.trans = [];
	t.exit = [];

	t.trans[ "right" ] = [];
	t.trans[ "right" ][ 1 ] = %corner_standr_trans_in_1;
	t.trans[ "right" ][ 2 ] = %corner_standr_trans_in_2;
	t.trans[ "right" ][ 3 ] = %corner_standr_trans_in_3;
	t.trans[ "right" ][ 4 ] = %corner_standr_trans_in_4;
	t.trans[ "right" ][ 6 ] = %corner_standr_trans_in_6;
	t.trans[ "right" ][ 8 ] = %corner_standr_trans_in_8;
	t.trans[ "right" ][ 9 ] = %corner_standr_trans_in_9;

	t.trans[ "right_crouch" ] = [];
	t.trans[ "right_crouch" ][ 1 ] = %cornercrr_trans_in_ml;
	t.trans[ "right_crouch" ][ 2 ] = %cornercrr_trans_in_m;
	t.trans[ "right_crouch" ][ 3 ] = %cornercrr_trans_in_mr;
	t.trans[ "right_crouch" ][ 4 ] = %cornercrr_trans_in_l;
	t.trans[ "right_crouch" ][ 6 ] = %cornercrr_trans_in_r;
	t.trans[ "right_crouch" ][ 8 ] = %cornercrr_trans_in_f;
	t.trans[ "right_crouch" ][ 9 ] = %cornercrr_trans_in_mf;

	t.trans[ "right_cqb" ] = [];
	t.trans[ "right_cqb" ][ 1 ] = %corner_standr_trans_cqb_in_1;
	t.trans[ "right_cqb" ][ 2 ] = %corner_standr_trans_cqb_in_2;
	t.trans[ "right_cqb" ][ 3 ] = %corner_standr_trans_cqb_in_3;
	t.trans[ "right_cqb" ][ 4 ] = %corner_standr_trans_cqb_in_4;
	t.trans[ "right_cqb" ][ 6 ] = %corner_standr_trans_cqb_in_6;
	t.trans[ "right_cqb" ][ 8 ] = %corner_standr_trans_cqb_in_8;
	t.trans[ "right_cqb" ][ 9 ] = %corner_standr_trans_cqb_in_9;

	t.trans[ "right_crouch_cqb" ] = [];
	t.trans[ "right_crouch_cqb" ][ 1 ] = %cornercrr_cqb_trans_in_1;
	t.trans[ "right_crouch_cqb" ][ 2 ] = %cornercrr_cqb_trans_in_2;
	t.trans[ "right_crouch_cqb" ][ 3 ] = %cornercrr_cqb_trans_in_3;
	t.trans[ "right_crouch_cqb" ][ 4 ] = %cornercrr_cqb_trans_in_4;
	t.trans[ "right_crouch_cqb" ][ 6 ] = %cornercrr_cqb_trans_in_6;
	t.trans[ "right_crouch_cqb" ][ 8 ] = %cornercrr_cqb_trans_in_8;
	t.trans[ "right_crouch_cqb" ][ 9 ] = %cornercrr_cqb_trans_in_9;

	t.trans[ "left" ] = [];
	t.trans[ "left" ][ 1 ] = %corner_standl_trans_in_1;
	t.trans[ "left" ][ 2 ] = %corner_standl_trans_in_2;
	t.trans[ "left" ][ 3 ] = %corner_standl_trans_in_3;
	t.trans[ "left" ][ 4 ] = %corner_standl_trans_in_4;
	t.trans[ "left" ][ 6 ] = %corner_standl_trans_in_6;
	t.trans[ "left" ][ 7 ] = %corner_standl_trans_in_7;
	t.trans[ "left" ][ 8 ] = %corner_standl_trans_in_8;

	t.trans[ "left_crouch" ] = [];
	t.trans[ "left_crouch" ][ 1 ] = %cornercrl_trans_in_ml;
	t.trans[ "left_crouch" ][ 2 ] = %cornercrl_trans_in_m;
	t.trans[ "left_crouch" ][ 3 ] = %cornercrl_trans_in_mr;
	t.trans[ "left_crouch" ][ 4 ] = %cornercrl_trans_in_l;
	t.trans[ "left_crouch" ][ 6 ] = %cornercrl_trans_in_r;
	t.trans[ "left_crouch" ][ 7 ] = %cornercrl_trans_in_mf;
	t.trans[ "left_crouch" ][ 8 ] = %cornercrl_trans_in_f;

	t.trans[ "left_cqb" ] = [];
	t.trans[ "left_cqb" ][ 1 ] = %corner_standl_trans_cqb_in_1;
	t.trans[ "left_cqb" ][ 2 ] = %corner_standl_trans_cqb_in_2;
	t.trans[ "left_cqb" ][ 3 ] = %corner_standl_trans_cqb_in_3;
	t.trans[ "left_cqb" ][ 4 ] = %corner_standl_trans_cqb_in_4;
	t.trans[ "left_cqb" ][ 6 ] = %corner_standl_trans_cqb_in_6;
	t.trans[ "left_cqb" ][ 7 ] = %corner_standl_trans_cqb_in_7;
	t.trans[ "left_cqb" ][ 8 ] = %corner_standl_trans_cqb_in_8;

	t.trans[ "left_crouch_cqb" ] = [];
	t.trans[ "left_crouch_cqb" ][ 1 ] = %cornercrl_cqb_trans_in_1;
	t.trans[ "left_crouch_cqb" ][ 2 ] = %cornercrl_cqb_trans_in_2;
	t.trans[ "left_crouch_cqb" ][ 3 ] = %cornercrl_cqb_trans_in_3;
	t.trans[ "left_crouch_cqb" ][ 4 ] = %cornercrl_cqb_trans_in_4;
	t.trans[ "left_crouch_cqb" ][ 6 ] = %cornercrl_cqb_trans_in_6;
	t.trans[ "left_crouch_cqb" ][ 7 ] = %cornercrl_cqb_trans_in_7;
	t.trans[ "left_crouch_cqb" ][ 8 ] = %cornercrl_cqb_trans_in_8;

	t.trans[ "crouch" ] = [];
	t.trans[ "crouch" ][ 1 ] = %covercrouch_run_in_ml;
	t.trans[ "crouch" ][ 2 ] = %covercrouch_run_in_m;
	t.trans[ "crouch" ][ 3 ] = %covercrouch_run_in_mr;
	t.trans[ "crouch" ][ 4 ] = %covercrouch_run_in_l;
	t.trans[ "crouch" ][ 6 ] = %covercrouch_run_in_r;

	t.trans[ "stand" ] = [];
	t.trans[ "stand" ][ 1 ] = %coverstand_trans_in_ml;
	t.trans[ "stand" ][ 2 ] = %coverstand_trans_in_m;
	t.trans[ "stand" ][ 3 ] = %coverstand_trans_in_mr;
	t.trans[ "stand" ][ 4 ] = %coverstand_trans_in_l;
	t.trans[ "stand" ][ 6 ] = %coverstand_trans_in_r;

	t.trans[ "exposed" ] = [];
	t.trans[ "exposed" ][ 1 ] = %cqb_stop_1;
	t.trans[ "exposed" ][ 2 ] = %run_2_stand_f_6;
	t.trans[ "exposed" ][ 3 ] = %cqb_stop_3;
	t.trans[ "exposed" ][ 4 ] = %run_2_stand_90l;
	t.trans[ "exposed" ][ 6 ] = %run_2_stand_90r;
	t.trans[ "exposed" ][ 7 ] = %cqb_stop_7;
	t.trans[ "exposed" ][ 8 ] = %run_2_stand_180l;
	t.trans[ "exposed" ][ 9 ] = %cqb_stop_9;

	t.trans[ "exposed_crouch" ] = [];
	t.trans[ "exposed_crouch" ][ 1 ] = %cqb_crouch_stop_1;
	t.trans[ "exposed_crouch" ][ 2 ] = %run_2_crouch_f;
	t.trans[ "exposed_crouch" ][ 3 ] = %cqb_crouch_stop_3;
	t.trans[ "exposed_crouch" ][ 4 ] = %run_2_crouch_90l;
	t.trans[ "exposed_crouch" ][ 6 ] = %run_2_crouch_90r;
	t.trans[ "exposed_crouch" ][ 7 ] = %cqb_crouch_stop_7;
	t.trans[ "exposed_crouch" ][ 8 ] = %run_2_crouch_180l;
	t.trans[ "exposed_crouch" ][ 9 ] = %cqb_crouch_stop_9;

	t.trans[ "exposed_cqb" ] = [];
	t.trans[ "exposed_cqb" ][ 1 ] = %cqb_stop_1;
	t.trans[ "exposed_cqb" ][ 2 ] = %cqb_stop_2;
	t.trans[ "exposed_cqb" ][ 3 ] = %cqb_stop_3;
	t.trans[ "exposed_cqb" ][ 4 ] = %cqb_stop_4;
	t.trans[ "exposed_cqb" ][ 6 ] = %cqb_stop_6;
	t.trans[ "exposed_cqb" ][ 7 ] = %cqb_stop_7;
	t.trans[ "exposed_cqb" ][ 8 ] = %cqb_stop_8;
	t.trans[ "exposed_cqb" ][ 9 ] = %cqb_stop_9;

	t.trans[ "exposed_crouch_cqb" ] = [];
	t.trans[ "exposed_crouch_cqb" ][ 1 ] = %cqb_crouch_stop_1;
	t.trans[ "exposed_crouch_cqb" ][ 2 ] = %cqb_crouch_stop_2;
	t.trans[ "exposed_crouch_cqb" ][ 3 ] = %cqb_crouch_stop_3;
	t.trans[ "exposed_crouch_cqb" ][ 4 ] = %cqb_crouch_stop_4;
	t.trans[ "exposed_crouch_cqb" ][ 6 ] = %cqb_crouch_stop_6;
	t.trans[ "exposed_crouch_cqb" ][ 7 ] = %cqb_crouch_stop_7;
	t.trans[ "exposed_crouch_cqb" ][ 8 ] = %cqb_crouch_stop_8;
	t.trans[ "exposed_crouch_cqb" ][ 9 ] = %cqb_crouch_stop_9;

	t.exit[ "right" ] = [];
	t.exit[ "right" ][ 1 ] = %corner_standr_trans_out_1;
	t.exit[ "right" ][ 2 ] = %corner_standr_trans_out_2;
	t.exit[ "right" ][ 3 ] = %corner_standr_trans_out_3;
	t.exit[ "right" ][ 4 ] = %corner_standr_trans_out_4;
	t.exit[ "right" ][ 6 ] = %corner_standr_trans_out_6;
	t.exit[ "right" ][ 8 ] = %corner_standr_trans_out_8;
	t.exit[ "right" ][ 9 ] = %corner_standr_trans_out_9;

	t.exit[ "right_crouch" ] = [];
	t.exit[ "right_crouch" ][ 1 ] = %cornercrr_trans_out_ml;
	t.exit[ "right_crouch" ][ 2 ] = %cornercrr_trans_out_m;
	t.exit[ "right_crouch" ][ 3 ] = %cornercrr_trans_out_mr;
	t.exit[ "right_crouch" ][ 4 ] = %cornercrr_trans_out_l;
	t.exit[ "right_crouch" ][ 6 ] = %cornercrr_trans_out_r;
	t.exit[ "right_crouch" ][ 8 ] = %cornercrr_trans_out_f;
	t.exit[ "right_crouch" ][ 9 ] = %cornercrr_trans_out_mf;

	t.exit[ "right_cqb" ] = [];
	t.exit[ "right_cqb" ][ 1 ] = %corner_standr_trans_cqb_out_1;
	t.exit[ "right_cqb" ][ 2 ] = %corner_standr_trans_cqb_out_2;
	t.exit[ "right_cqb" ][ 3 ] = %corner_standr_trans_cqb_out_3;
	t.exit[ "right_cqb" ][ 4 ] = %corner_standr_trans_cqb_out_4;
	t.exit[ "right_cqb" ][ 6 ] = %corner_standr_trans_cqb_out_6;
	t.exit[ "right_cqb" ][ 8 ] = %corner_standr_trans_cqb_out_8;
	t.exit[ "right_cqb" ][ 9 ] = %corner_standr_trans_cqb_out_9;

	t.exit[ "right_crouch_cqb" ] = [];
	t.exit[ "right_crouch_cqb" ][ 1 ] = %cornercrr_cqb_trans_out_1;
	t.exit[ "right_crouch_cqb" ][ 2 ] = %cornercrr_cqb_trans_out_2;
	t.exit[ "right_crouch_cqb" ][ 3 ] = %cornercrr_cqb_trans_out_3;
	t.exit[ "right_crouch_cqb" ][ 4 ] = %cornercrr_cqb_trans_out_4;
	t.exit[ "right_crouch_cqb" ][ 6 ] = %cornercrr_cqb_trans_out_6;
	t.exit[ "right_crouch_cqb" ][ 8 ] = %cornercrr_cqb_trans_out_8;
	t.exit[ "right_crouch_cqb" ][ 9 ] = %cornercrr_cqb_trans_out_9;

	t.exit[ "left" ] = [];
	t.exit[ "left" ][ 1 ] = %corner_standl_trans_out_1;
	t.exit[ "left" ][ 2 ] = %corner_standl_trans_out_2;
	t.exit[ "left" ][ 3 ] = %corner_standl_trans_out_3;
	t.exit[ "left" ][ 4 ] = %corner_standl_trans_out_4;
	t.exit[ "left" ][ 6 ] = %corner_standl_trans_out_6;
	t.exit[ "left" ][ 7 ] = %corner_standl_trans_out_7;
	t.exit[ "left" ][ 8 ] = %corner_standl_trans_out_8;

	t.exit[ "left_crouch" ] = [];
	t.exit[ "left_crouch" ][ 1 ] = %cornercrl_trans_out_ml;
	t.exit[ "left_crouch" ][ 2 ] = %cornercrl_trans_out_m;
	t.exit[ "left_crouch" ][ 3 ] = %cornercrl_trans_out_mr;
	t.exit[ "left_crouch" ][ 4 ] = %cornercrl_trans_out_l;
	t.exit[ "left_crouch" ][ 6 ] = %cornercrl_trans_out_r;
	t.exit[ "left_crouch" ][ 7 ] = %cornercrl_trans_out_mf;
	t.exit[ "left_crouch" ][ 8 ] = %cornercrl_trans_out_f;

	t.exit[ "left_cqb" ] = [];
	t.exit[ "left_cqb" ][ 1 ] = %corner_standl_trans_cqb_out_1;
	t.exit[ "left_cqb" ][ 2 ] = %corner_standl_trans_cqb_out_2;
	t.exit[ "left_cqb" ][ 3 ] = %corner_standl_trans_cqb_out_3;
	t.exit[ "left_cqb" ][ 4 ] = %corner_standl_trans_cqb_out_4;
	t.exit[ "left_cqb" ][ 6 ] = %corner_standl_trans_cqb_out_6;
	t.exit[ "left_cqb" ][ 7 ] = %corner_standl_trans_cqb_out_7;
	t.exit[ "left_cqb" ][ 8 ] = %corner_standl_trans_cqb_out_8;

	t.exit[ "left_crouch_cqb" ] = [];
	t.exit[ "left_crouch_cqb" ][ 1 ] = %cornercrl_cqb_trans_out_1;
	t.exit[ "left_crouch_cqb" ][ 2 ] = %cornercrl_cqb_trans_out_2;
	t.exit[ "left_crouch_cqb" ][ 3 ] = %cornercrl_cqb_trans_out_3;
	t.exit[ "left_crouch_cqb" ][ 4 ] = %cornercrl_cqb_trans_out_4;
	t.exit[ "left_crouch_cqb" ][ 6 ] = %cornercrl_cqb_trans_out_6;
	t.exit[ "left_crouch_cqb" ][ 7 ] = %cornercrl_cqb_trans_out_7;
	t.exit[ "left_crouch_cqb" ][ 8 ] = %cornercrl_cqb_trans_out_8;

	t.exit[ "crouch" ] = [];
	t.exit[ "crouch" ][ 1 ] = %covercrouch_run_out_ml;
	t.exit[ "crouch" ][ 2 ] = %covercrouch_run_out_m;
	t.exit[ "crouch" ][ 3 ] = %covercrouch_run_out_mr;
	t.exit[ "crouch" ][ 4 ] = %covercrouch_run_out_l;
	t.exit[ "crouch" ][ 6 ] = %covercrouch_run_out_r;

	t.exit[ "stand" ] = [];
	t.exit[ "stand" ][ 1 ] = %coverstand_trans_out_ml;
	t.exit[ "stand" ][ 2 ] = %coverstand_trans_out_m;
	t.exit[ "stand" ][ 3 ] = %coverstand_trans_out_mr;
	t.exit[ "stand" ][ 4 ] = %coverstand_trans_out_l;
	t.exit[ "stand" ][ 6 ] = %coverstand_trans_out_r;

	t.exit[ "exposed" ] = [];
	t.exit[ "exposed" ][ 1 ] = %cqb_start_1;
	t.exit[ "exposed" ][ 2 ] = %stand_2_run_180l;
	t.exit[ "exposed" ][ 3 ] = %cqb_start_3;
	t.exit[ "exposed" ][ 4 ] = %stand_2_run_l;
	t.exit[ "exposed" ][ 6 ] = %stand_2_run_r;
	t.exit[ "exposed" ][ 7 ] = %cqb_start_7;
	t.exit[ "exposed" ][ 8 ] = %surprise_start_v1;
	t.exit[ "exposed" ][ 9 ] = %cqb_start_9;

	t.exit[ "exposed_crouch" ] = [];
	t.exit[ "exposed_crouch" ][ 1 ] = %cqb_crouch_start_1;
	t.exit[ "exposed_crouch" ][ 2 ] = %crouch_2run_180;
	t.exit[ "exposed_crouch" ][ 3 ] = %cqb_crouch_start_3;
	t.exit[ "exposed_crouch" ][ 4 ] = %crouch_2run_l;
	t.exit[ "exposed_crouch" ][ 6 ] = %crouch_2run_r;
	t.exit[ "exposed_crouch" ][ 7 ] = %cqb_crouch_start_7;
	t.exit[ "exposed_crouch" ][ 8 ] = %crouch_2run_f;
	t.exit[ "exposed_crouch" ][ 9 ] = %cqb_crouch_start_9;

	t.exit[ "exposed_cqb" ] = [];
	t.exit[ "exposed_cqb" ][ 1 ] = %cqb_start_1;
	t.exit[ "exposed_cqb" ][ 2 ] = %cqb_start_2;
	t.exit[ "exposed_cqb" ][ 3 ] = %cqb_start_3;
	t.exit[ "exposed_cqb" ][ 4 ] = %cqb_start_4;
	t.exit[ "exposed_cqb" ][ 6 ] = %cqb_start_6;
	t.exit[ "exposed_cqb" ][ 7 ] = %cqb_start_7;
	t.exit[ "exposed_cqb" ][ 8 ] = %cqb_start_8;
	t.exit[ "exposed_cqb" ][ 9 ] = %cqb_start_9;

	t.exit[ "exposed_crouch_cqb" ] = [];
	t.exit[ "exposed_crouch_cqb" ][ 1 ] = %cqb_crouch_start_1;
	t.exit[ "exposed_crouch_cqb" ][ 2 ] = %cqb_crouch_start_2;
	t.exit[ "exposed_crouch_cqb" ][ 3 ] = %cqb_crouch_start_3;
	t.exit[ "exposed_crouch_cqb" ][ 4 ] = %cqb_crouch_start_4;
	t.exit[ "exposed_crouch_cqb" ][ 6 ] = %cqb_crouch_start_6;
	t.exit[ "exposed_crouch_cqb" ][ 7 ] = %cqb_crouch_start_7;
	t.exit[ "exposed_crouch_cqb" ][ 8 ] = %cqb_crouch_start_8;
	t.exit[ "exposed_crouch_cqb" ][ 9 ] = %cqb_crouch_start_9;

	// IW4 anim.maxDirections / anim.excludeDir by approach type, and the
	// stance each arrival ends in (anim.arrivalEndStance).
	t.maxDir = [];
	t.excludeDir = [];
	t.endStance = [];
	types = strTok( "left right left_crouch right_crouch crouch stand exposed exposed_crouch left_cqb right_cqb left_crouch_cqb right_crouch_cqb exposed_cqb exposed_crouch_cqb", " " );
	for ( i = 0; i < types.size; i++ )
	{
		t.maxDir[ types[ i ] ] = 9;
		t.excludeDir[ types[ i ] ] = -1;
		t.endStance[ types[ i ] ] = "stand";
		if ( isSubStr( types[ i ], "crouch" ) )
			t.endStance[ types[ i ] ] = "crouch";
	}
	t.maxDir[ "crouch" ] = 6;
	t.maxDir[ "stand" ] = 6;
	t.excludeDir[ "left" ] = 9;
	t.excludeDir[ "left_crouch" ] = 9;
	t.excludeDir[ "left_cqb" ] = 9;
	t.excludeDir[ "left_crouch_cqb" ] = 9;
	t.excludeDir[ "right" ] = 7;
	t.excludeDir[ "right_crouch" ] = 7;
	t.excludeDir[ "right_cqb" ] = 7;
	t.excludeDir[ "right_crouch_cqb" ] = 7;

	// Root motion and yaw change of each clip.  An exit's motion counts to
	// its code_move notetrack, where IW4 hands the actor back to the path.
	t.transDist = [];
	t.transAngle = [];
	t.exitDist = [];
	t.exitAngle = [];
	t.exitMoveFrac = [];
	t.longest = [];
	for ( j = 0; j < types.size; j++ )
	{
		type = types[ j ];
		t.transDist[ type ] = [];
		t.transAngle[ type ] = [];
		t.exitDist[ type ] = [];
		t.exitAngle[ type ] = [];
		t.exitMoveFrac[ type ] = [];
		t.longest[ type ] = 0;
		for ( i = 1; i <= 9; i++ )
		{
			if ( i == 5 )
				continue;
			if ( isDefined( t.trans[ type ] ) && isDefined( t.trans[ type ][ i ] ) )
			{
				t.transDist[ type ][ i ] = getMoveDelta( t.trans[ type ][ i ], 0, 1 );
				t.transAngle[ type ][ i ] = getAngleDelta( t.trans[ type ][ i ], 0, 1 );
				len = length( t.transDist[ type ][ i ] );
				if ( len > t.longest[ type ] )
					t.longest[ type ] = len;
			}
			if ( isDefined( t.exit[ type ] ) && isDefined( t.exit[ type ][ i ] ) )
			{
				frac = 1;
				if ( animHasNotetrack( t.exit[ type ][ i ], "code_move" ) )
				{
					times = getNotetrackTimes( t.exit[ type ][ i ], "code_move" );
					if ( isDefined( times ) && times.size > 0 )
						frac = times[ 0 ];
				}
				t.exitMoveFrac[ type ][ i ] = frac;
				t.exitDist[ type ][ i ] = getMoveDelta( t.exit[ type ][ i ], 0, frac );
				t.exitAngle[ type ][ i ] = getAngleDelta( t.exit[ type ][ i ], 0, 1 );
			}
		}
	}
	level.coop_trans = t;
}

// IW4 cover_arrival getNodeStanceYawOffset: a soldier at a corner hides
// side-on to it.
node_stance_yaw_offset( type )
{
	if ( type == "left" || type == "left_crouch" || type == "left_cqb" || type == "left_crouch_cqb" )
		return 90;
	if ( type == "right" || type == "right_crouch" || type == "right_cqb" || type == "right_crouch_cqb" )
		return -90;
	return 0;
}

// IW4 node spawnflags: 4 = no stand, 8 = no crouch.
node_allows( node, stance )
{
	flags = 0;
	if ( isDefined( node.spawnflags ) )
		flags = node.spawnflags;
	if ( stance == "stand" )
		return ( flags & 4 ) == 0;
	if ( stance == "crouch" )
		return ( flags & 8 ) == 0;
	return false;
}

// IW4 anim.approach_types: the arrival / exit family for a node type and
// the stance the soldier holds it in.
cover_trans_type( nodeType, pose )
{
	type = "exposed";
	switch ( nodeType )
	{
	case "Cover Left":
		type = "left";
		if ( pose == "crouch" )
			type = "left_crouch";
		break;
	case "Cover Right":
		type = "right";
		if ( pose == "crouch" )
			type = "right_crouch";
		break;
	case "Cover Crouch":
	case "Cover Crouch Window":
	case "Conceal Crouch":
		return "crouch";
	case "Cover Stand":
	case "Conceal Stand":
		return "stand";
	default:
		if ( pose == "crouch" )
			type = "exposed_crouch";
		break;
	}
	if ( isDefined( self.coop_cqb ) && isDefined( level.coop_trans.trans[ type + "_cqb" ] ) )
		type = type + "_cqb";
	return type;
}

// The stance a soldier takes a node in: a corner that allows both is
// held standing more often than not (IW4 corner mainLoopStart); a wall
// node decides its own stance.
cover_pose_for( node )
{
	if ( node.type != "Cover Left" && node.type != "Cover Right" )
	{
		if ( node.type == "Cover Stand" || node.type == "Conceal Stand" )
			return "stand";
		return "crouch";
	}
	if ( !can_crouch() || !node_allows( node, "crouch" ) )
		return "stand";
	if ( !node_allows( node, "stand" ) )
		return "crouch";
	if ( randomInt( 100 ) < 30 )
		return "crouch";
	return "stand";
}

// IW4 calculateNodeTransitionAngles + sortNodeTransitionAngles, best only:
// the direction whose clip best matches moving along dir.  For an arrival
// the clip ends facing yaw; for an exit it starts at yaw.
best_transition( type, isArrival, yaw, dir, onlyFront )
{
	t = level.coop_trans;
	if ( isArrival )
		angles = t.transAngle[ type ];
	else
		angles = t.exitAngle[ type ];
	best = -1;
	bestDot = -2;
	maxDir = t.maxDir[ type ];
	for ( i = 1; i <= maxDir; i++ )
	{
		if ( i == 5 || i == t.excludeDir[ type ] || !isDefined( angles[ i ] ) )
			continue;
		if ( isDefined( onlyFront ) && onlyFront && i > 6 )
			continue;
		if ( isArrival )
			d = anglesToForward( ( 0, yaw - angles[ i ], 0 ) );
		else
			d = anglesToForward( ( 0, yaw + angles[ i ] + 180, 0 ) );
		dot = vectorDot( dir, d );
		if ( dot > bestDot )
		{
			bestDot = dot;
			best = i;
		}
	}
	self.coop_trans_dot = bestDot;
	return best;
}

// The point a clip whose root motion is delta, played facing yaw from
// start, ends at.
delta_end( start, yaw, delta )
{
	f = anglesToForward( ( 0, yaw, 0 ) );
	r = anglesToRight( ( 0, yaw, 0 ) );
	return start + f * delta[ 0 ] - r * delta[ 1 ];
}

// IW4 maymovefrompointtopoint: a standing hull walks the straight line
// (players' clip and the world stop it; bodies do not).
may_move( from, to )
{
	a = from + ( 0, 0, 18 );
	b = to + ( 0, 0, 18 );
	end = playerPhysicsTrace( a, b );
	return distanceSquared( end, b ) < 4;
}

// Slide onto a point over a few frames (IW4 shared::moveToOriginOverTime):
// the arrival ends within a few units of the node; this lands him on it.
slide_to( point, time )
{
	self endon( "killanimscript" );
	self endon( "death" );
	steps = int( time * 20 );
	if ( steps < 1 )
		steps = 1;
	start = self.origin;
	for ( i = 1; i <= steps; i++ )
	{
		p = vectorLerp( start, point, i / steps );
		self teleport( ( p[ 0 ], p[ 1 ], self.origin[ 2 ] ) );
		wait( 0.05 );
	}
}

// ---------------------------------------------------------------------------
// Turning in place: IW4 animset set_animarray_standing_turns /
// crouching_turns / pistol turns, and combat.gsc TurnToFaceRelativeYaw.
// The clip's own rotation turns him ("face current" lets it).
// ---------------------------------------------------------------------------

// yaw: counter-clockwise positive (to his left).
turn_anim( yaw )
{
	kind = weapon_kind();
	if ( kind == "pistol" )
	{
		if ( yaw > 135 || yaw < -135 )
			return %pistol_stand_turn180l;
		if ( yaw > 67.5 )
			return %pistol_stand_turn90l;
		if ( yaw > 0 )
			return %pistol_stand_turn45l;
		if ( yaw < -67.5 )
			return %pistol_stand_turn90r;
		return %pistol_stand_turn45r;
	}
	if ( isDefined( self.a.pose ) && self.a.pose == "crouch" )
	{
		if ( yaw > 135 )
			return %exposed_crouch_turn_180_left;
		if ( yaw < -135 )
			return %exposed_crouch_turn_180_right;
		if ( yaw > 0 )
			return %exposed_crouch_turn_90_left;
		return %exposed_crouch_turn_90_right;
	}
	if ( yaw > 157.5 )
		return %exposed_tracking_turn180l;
	if ( yaw < -157.5 )
		return %exposed_tracking_turn180r;
	if ( yaw > 112.5 )
		return %exposed_tracking_turn135l;
	if ( yaw < -112.5 )
		return %exposed_tracking_turn135r;
	if ( yaw > 67.5 )
		return %exposed_tracking_turn90l;
	if ( yaw < -67.5 )
		return %exposed_tracking_turn90r;
	if ( yaw > 0 )
		return %exposed_tracking_turn45l;
	return %exposed_tracking_turn45r;
}

// Turn to face a point when it is more than threshold degrees off his
// facing.  Returns true when he turned.
turn_toward( point, threshold )
{
	self endon( "killanimscript" );
	if ( !isDefined( threshold ) )
		threshold = 45;
	yaw = angleClamp180( vectorToYaw( point - self.origin ) - self.angles[ 1 ] );
	if ( abs( yaw ) < threshold )
		return false;
	turnAnim = turn_anim( yaw );
	wasAiming = isDefined( self.coop_aiming );
	stop_aiming();
	trace_anim( "turn " + int( yaw ) + " in place" );
	stat_bump( "turn_in_place" );
	self orientmode( "face current" );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "turnanim", turnAnim, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( getAnimLength( turnAnim ) * 0.85, "turnanim" );
	if ( wasAiming )
		start_aiming( 0.2 );
	return true;
}

// ---------------------------------------------------------------------------
// Grenades: IW4 combat_utility TryGrenade and the _gameskill grenade
// timers, regular difficulty.  The throw is a real grenade owned by the
// soldier (codol_actorThrowGrenade), on a ballistic arc that clears the
// world, released at the clip's grenade_throw notetrack.
// ---------------------------------------------------------------------------

grenade_weapon()
{
	// Survival's AI table owns its offhand: empty for easy infantry, frag
	// for regular troops, tabun for chemical/hazmat. Coop keeps its role pool.
	if ( isDefined( self.survival_grenade ) )
		return self.survival_grenade;
	if ( !isDefined( level.coop_ai_grenade ) )
	{
		level.coop_ai_grenade = "";
		names = strTok( "fraggrenade_so frag_grenade_so frag_grenade_mp", " " );
		for ( i = 0; i < names.size; i++ )
		{
			if ( codol_isWeaponMounted( names[ i ] ) )
			{
				level.coop_ai_grenade = names[ i ];
				break;
			}
		}
		codolprintln( "CoDOL coop soldier: grenade weapon '" + level.coop_ai_grenade + "'" );
	}
	return level.coop_ai_grenade;
}

// May this soldier throw at that enemy now?  One AI grenade every 8..15 s
// across the match (IW4 anim.grenadeTimers, regular), one or two per
// soldier, between 250 and 1250 units, never at a target a storey above.
can_throw_grenade( enemy )
{
	if ( getDvar( "scr_coop_ai_grenades" ) == "0" )
		return false;
	if ( !isDefined( enemy ) || !isAlive( enemy ) || grenade_weapon() == "" )
		return false;
	if ( !isDefined( self.coop_grenades ) )
		self.coop_grenades = 1 + randomInt( 2 );
	if ( self.coop_grenades <= 0 )
		return false;
	if ( isDefined( level.coop_next_ai_grenade ) && getTime() < level.coop_next_ai_grenade )
		return false;
	dist = distance( self.origin, enemy.origin );
	if ( dist < 250 || dist > 1250 )
		return false;
	if ( enemy.origin[ 2 ] - self.origin[ 2 ] > 160 )
		return false;
	return true;
}

// The velocity that carries a grenade from start to target in t seconds
// (gravity 800).
grenade_velocity( start, target, t )
{
	d = target - start;
	return ( d[ 0 ] / t, d[ 1 ] / t, ( d[ 2 ] + 400 * t * t ) / t );
}

// A flight time whose arc clears the world, or undefined.
grenade_arc( start, target )
{
	dist = distance2d( start, target );
	times = [];
	times[ 0 ] = 0.6 + dist / 900;
	times[ 1 ] = 0.9 + dist / 700;
	times[ 2 ] = 1.2 + dist / 600;
	for ( k = 0; k < times.size; k++ )
	{
		t = times[ k ];
		v = grenade_velocity( start, target, t );
		prev = start;
		clear = true;
		for ( s = 1; s <= 6 && clear; s++ )
		{
			tt = t * s / 6;
			p = start + ( v[ 0 ] * tt, v[ 1 ] * tt, v[ 2 ] * tt - 400 * tt * tt );
			if ( s == 6 )
				p = p + ( 0, 0, 12 );
			if ( !bulletTracePassed( prev, p, false, self ) )
				clear = false;
			prev = p;
		}
		if ( clear )
			return t;
	}
	return undefined;
}

// The release time of a throw clip, as a fraction of its length.
notetrack_frac( clip, note, fallback )
{
	if ( !animHasNotetrack( clip, note ) )
		return fallback;
	times = getNotetrackTimes( clip, note );
	if ( isDefined( times ) && times.size > 0 )
		return times[ 0 ];
	return fallback;
}

// Play throwAnim and release a grenade at enemy at its throw notetrack.
// Returns true when a grenade left his hand.
throw_grenade( enemy, throwAnim )
{
	self endon( "killanimscript" );
	self endon( "death" );

	target = enemy.origin + ( randomFloatRange( -48, 48 ), randomFloatRange( -48, 48 ), 2 );
	start = self.origin + ( 0, 0, 64 ) + anglesToForward( self.angles ) * 12;
	flight = grenade_arc( start, target );
	if ( !isDefined( flight ) )
		return false;

	level.coop_next_ai_grenade = getTime() + 8000 + randomInt( 7000 );
	self.coop_grenades--;

	stop_aiming();
	trace_anim( "grenade at ent " + enemy getEntityNumber() + " (" + int( distance( self.origin, enemy.origin ) ) + " units)" );
	stat_bump( "grenade_throw" );
	releaseFrac = notetrack_frac( throwAnim, "grenade_throw", 0.55 );
	len = getAnimLength( throwAnim );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "throwanim", throwAnim, %body, 1, 0.2, 1 );
	DoNoteTracksForTime( len * releaseFrac, "throwanim" );

	if ( isAlive( enemy ) )
	{
		hand = self getTagOrigin( "tag_inhand" );
		if ( !isDefined( hand ) )
			hand = start;
		target = enemy.origin + ( randomFloatRange( -40, 40 ), randomFloatRange( -40, 40 ), 2 );
		flight = grenade_arc( hand, target );
		if ( !isDefined( flight ) )
			flight = 0.9 + distance2d( hand, target ) / 700;
		v = grenade_velocity( hand, target, flight );
		grenade = self codol_actorThrowGrenade( grenade_weapon(), hand, v, flight + 0.6 + randomFloat( 0.6 ) );
		if ( isDefined( grenade ) && isDefined( self.survival_grenade_on_explode ) )
			grenade thread [[self.survival_grenade_on_explode]]();
		if ( getDvarInt( "scr_coop_test" ) )
			codolprintln( "CoDOL coop soldier: ent " + self getEntityNumber() + " throws a grenade at ent " +
				enemy getEntityNumber() + " (" + int( distance( self.origin, enemy.origin ) ) + " units, flight " + flight + " s)" );
	}
	DoNoteTracksForTime( len * ( 1 - releaseFrac ), "throwanim" );
	return true;
}

// ---------------------------------------------------------------------------
// Live grenades the players threw (IW4 code's grenade danger, for
// grenade_cower / grenade_return_throw): every player's "grenade_fire" is
// recorded, and a soldier asks for the nearest one.
// ---------------------------------------------------------------------------

watch_player_grenades()
{
	if ( isDefined( level.coop_grenade_watch ) )
		return;
	level.coop_grenade_watch = true;
	level.coop_live_grenades = [];
	level thread grenade_watch_players();
}

grenade_watch_players()
{
	for ( ;; )
	{
		players = getEntArray( "player", "classname" );
		for ( i = 0; i < players.size; i++ )
		{
			if ( !isDefined( players[ i ].coop_grenade_watched ) )
			{
				players[ i ].coop_grenade_watched = true;
				players[ i ] thread grenade_watch_player();
			}
		}
		wait( 1 );
	}
}

grenade_watch_player()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "grenade_fire", grenade, weaponName );
		if ( !isDefined( grenade ) )
			continue;
		grenade.coop_thrown_time = getTime();
		grenade.coop_thrower = self;
		live = [];
		for ( i = 0; i < level.coop_live_grenades.size; i++ )
			if ( isDefined( level.coop_live_grenades[ i ] ) )
				live[ live.size ] = level.coop_live_grenades[ i ];
		live[ live.size ] = grenade;
		level.coop_live_grenades = live;
	}
}

// The nearest player grenade within radius at about his height (or
// undefined).
grenade_danger( radius )
{
	if ( !isDefined( level.coop_live_grenades ) )
		return undefined;
	best = undefined;
	bestDist = radius * radius;
	for ( i = 0; i < level.coop_live_grenades.size; i++ )
	{
		g = level.coop_live_grenades[ i ];
		if ( !isDefined( g ) )
			continue;
		d = distanceSquared( g.origin, self.origin );
		if ( d < bestDist && abs( g.origin[ 2 ] - self.origin[ 2 ] ) < 80 )
		{
			best = g;
			bestDist = d;
		}
	}
	return best;
}

// IW4 grenade_return_throw / grenade_cower: a grenade at his feet early in
// its fuse is picked up and thrown back at whoever threw it; otherwise he
// dives away from it.  Returns true when he reacted.
react_to_grenade()
{
	self endon( "killanimscript" );
	self endon( "death" );
	if ( getDvar( "scr_coop_ai_grenade_react" ) == "0" )
		return false;
	g = grenade_danger( 256 );
	if ( !isDefined( g ) )
		return false;
	if ( isDefined( self.coop_grenade_reacted ) && self.coop_grenade_reacted == g )
		return false;
	self.coop_grenade_reacted = g;

	stop_aiming();
	self.safeToChangeScript = false;
	dist = distance( g.origin, self.origin );
	age = getTime() - g.coop_thrown_time;
	thrower = g.coop_thrower;
	if ( dist < 96 && age < 1800 && isDefined( thrower ) && isAlive( thrower ) && grenade_weapon() != "" && randomInt( 100 ) < 60 )
	{
		// IW4 grenade_return_throw: pick it up and throw it back.
		returnAnim = pick( list2( %grenade_return_standing_throw_forward_1, %grenade_return_standing_throw_overhand_forward ) );
		if ( self.a.movement == "run" )
			returnAnim = %grenade_return_running_throw_forward;
		trace_anim( "grenade return" );
		stat_bump( "grenade_return" );
		self orientmode( "face point", g.origin );
		self animmode( "zonly_physics", false );
		self setFlaggedAnimKnobAllRestart( "returnanim", returnAnim, %body, 1, 0.2, 1 );
		len = getAnimLength( returnAnim );
		pickFrac = notetrack_frac( returnAnim, "grenade_pickup", 0.35 );
		throwFrac = notetrack_frac( returnAnim, "grenade_throw", 0.65 );
		if ( throwFrac < pickFrac )
			throwFrac = pickFrac;
		DoNoteTracksForTime( len * pickFrac, "returnanim" );
		if ( isDefined( g ) )
		{
			g delete();
			if ( isAlive( thrower ) )
				self orientmode( "face point", thrower.origin );
			DoNoteTracksForTime( len * ( throwFrac - pickFrac ), "returnanim" );
			if ( isAlive( thrower ) )
			{
				hand = self getTagOrigin( "tag_inhand" );
				target = thrower.origin + ( 0, 0, 2 );
				flight = grenade_arc( hand, target );
				if ( !isDefined( flight ) )
					flight = 0.9 + distance2d( hand, target ) / 700;
				self codol_actorThrowGrenade( grenade_weapon(), hand, grenade_velocity( hand, target, flight ), flight + 0.5 );
				if ( getDvarInt( "scr_coop_test" ) )
					codolprintln( "CoDOL coop soldier: ent " + self getEntityNumber() + " returns a grenade to ent " + thrower getEntityNumber() );
			}
			DoNoteTracksForTime( len * ( 1 - throwFrac ), "returnanim" );
		}
		else
			DoNoteTracksForTime( len * ( 1 - pickFrac ), "returnanim" );
	}
	else
	{
		// IW4 grenade_cower: dive away from it (forward or back), or a
		// crouched soldier squats and covers up.
		away = vectorNormalize( self.origin - g.origin );
		forward = anglesToForward( self.angles );
		squat = isDefined( self.a.pose ) && self.a.pose == "crouch";
		if ( squat )
			diveAnim = %exposed_squat_down_grenade_f;
		else if ( vectorDot( away, forward ) >= 0 )
			diveAnim = %exposed_dive_grenade_f;
		else
			diveAnim = %exposed_dive_grenade_b;
		trace_anim( "grenade dive" );
		stat_bump( "grenade_dive" );
		self orientmode( "face current" );
		self animmode( "zonly_physics", false );
		self setFlaggedAnimKnobAllRestart( "diveanim", diveAnim, %body, 1, 0.1, 1 );
		DoNoteTracksForTime( getAnimLength( diveAnim ), "diveanim" );
		if ( squat )
		{
			self setFlaggedAnimKnobAllRestart( "diveanim", %exposed_squat_idle_grenade_f, %body, 1, 0.2, 1 );
			DoNoteTracksForTime( 1.0, "diveanim" );
		}
		self.a.pose = "crouch";
	}
	self animmode( "normal", false );
	self.safeToChangeScript = true;
	return true;
}

// ---------------------------------------------------------------------------
// Melee: IW4 animscripts\melee.gsc, the plain strike (melee_1), from a
// stand (stand_2_melee_1) or on the run (run_2_melee_charge).  The blow is
// struck at the clip's "fire" notetrack when he is still in reach and
// facing the enemy.
// ---------------------------------------------------------------------------

can_melee( enemy )
{
	if ( getDvar( "scr_coop_ai_melee" ) == "0" )
		return false;
	if ( !isDefined( enemy ) || !isAlive( enemy ) )
		return false;
	if ( isDefined( self.coop_next_melee ) && getTime() < self.coop_next_melee )
		return false;
	if ( abs( enemy.origin[ 2 ] - self.origin[ 2 ] ) > 40 )
		return false;
	return distanceSquared( enemy.origin, self.origin ) < 72 * 72;
}

melee_strike( enemy )
{
	self endon( "killanimscript" );
	self endon( "death" );
	self.coop_next_melee = getTime() + 2500;
	stop_aiming();
	self.safeToChangeScript = false;
	strike = %melee_1;
	if ( self.a.movement == "run" )
		strike = %run_2_melee_charge;
	else if ( randomInt( 2 ) )
		strike = %stand_2_melee_1;
	trace_anim( "melee at ent " + enemy getEntityNumber() );
	stat_bump( "melee" );
	self orientmode( "face enemy" );
	self animmode( "zonly_physics", false );
	self setFlaggedAnimKnobAllRestart( "meleeanim", strike, %body, 1, 0.1, 1 );
	len = getAnimLength( strike );
	hitFrac = notetrack_frac( strike, "fire", 0.45 );
	DoNoteTracksForTime( len * hitFrac, "meleeanim" );
	if ( isAlive( enemy ) && distanceSquared( enemy.origin, self.origin ) < 84 * 84 )
	{
		toEnemy = vectorNormalize( enemy.origin - self.origin );
		if ( vectorDot( toEnemy, anglesToForward( self.angles ) ) > 0.5 )
		{
			enemy doDamage( 80, self.origin, self, self, 0, "MOD_MELEE" );
			if ( getDvarInt( "scr_coop_test" ) )
				codolprintln( "CoDOL coop soldier: ent " + self getEntityNumber() + " melee hits ent " + enemy getEntityNumber() );
		}
	}
	DoNoteTracksForTime( len * ( 1 - hitFrac ), "meleeanim" );
	self animmode( "normal", false );
	self.a.pose = "stand";
	self.a.movement = "stop";
	self.safeToChangeScript = true;
}

// ---------------------------------------------------------------------------
// Suppression (IW4 code's isSuppressed): rounds cracking past him.  A
// soldier is suppressed for two seconds after a hit and while a player is
// firing on him (aimed within a few degrees and the trigger held).
// ---------------------------------------------------------------------------

is_suppressed()
{
	if ( isDefined( self.coop_hit_time ) && getTime() - self.coop_hit_time < 2000 )
		return true;
	enemy = self.enemy;
	if ( !isDefined( enemy ) || !isPlayer( enemy ) || !isAlive( enemy ) )
		return false;
	if ( !enemy attackButtonPressed() )
		return false;
	look = anglesToForward( enemy getPlayerAngles() );
	to = vectorNormalize( ( self.origin + ( 0, 0, 40 ) ) - enemy getEye() );
	return vectorDot( look, to ) > 0.985;
}
