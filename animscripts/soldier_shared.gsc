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
		s = [];
		s[ "idle" ] = %patrol_bored_idle;
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

trace_anim( what )
{
	if ( getDvarInt( "scr_coop_anim_trace" ) == 0 )
		return;
	codolprintln( "CoDOL coop anim: ent " + self getEntityNumber() + " " + what );
}
