// Coop scene animation: the IW4 maps\_anim pattern for the coop missions.
//
// IW4 SP plays its scripted scenes from a per-level table,
// level.scr_anim[ animname ][ scene ] (each level's <level>_anim.gsc), with
// anim_single / anim_loop / anim_first_frame / anim_reach at a scene node.
// This is the same pattern on the coop actor (the native soldier species and
// animscripts\soldier_stop's cast pose), with one difference that matters:
//
// A MISSION'S SCENE CLIPS ARE NEVER NAMED WITH %anim.  Each mission's own
// clips (killhouse_gaz_idlea, estate_breach_hvt_cower, favela_tackle_ghost,
// crash_intro_allied_01, ...) are listed only in that mission's
// animlists/coop_<mode>_<level>.animlist, which C1 loads for that mission
// alone (codol_coopClientMapName).  Every coop mission module is compiled in
// every coop match, and a %anim adds its node to the SERVER tree in all of
// them; the actor channel sends the server tree's entry index, so a node the
// client lacks shifts every later index (tools/codol_coop_anim_check.py).
// Scene clips are looked up by name with the native codol_getAnim(), which
// answers only what the client's lists already put in the tree and adds
// nothing.  A %anim is still right for the common_human / common_actor_shared
// clips every coop client loads; they are the fallbacks here.
//
// Coop category only; names no multiplayer module.

#using_animtree ( "generic_human" );

// --------------------------------------------------------------------------
// The scene table.
// --------------------------------------------------------------------------

// The generic_human clip called <name> in this match's tree, or <fallback>
// (a common %anim, may be undefined) when this mission's client does not load
// it.  A miss is logged once per name.
scene_anim( name, fallback )
{
	a = codol_getAnim( "generic_human", name );
	if ( isDefined( a ) )
		return a;
	if ( !isDefined( level.coop_scene_missing ) )
		level.coop_scene_missing = [];
	if ( !isDefined( level.coop_scene_missing[ name ] ) )
	{
		level.coop_scene_missing[ name ] = true;
		how = "skipped";
		if ( isDefined( fallback ) )
			how = "using the common fallback";
		codolprintln( "CoDOL coop scene: clip " + name + " is not in this mission's animlists; " + how );
	}
	return fallback;
}

// True when this match's tree holds the clip (the mission's animlist names it).
has_scene_anim( name )
{
	return isDefined( codol_getAnim( "generic_human", name ) );
}

// level.coop_scr_anim[ animname ][ scene ] = the clip, as IW4 *_anim.gsc
// fills level.scr_anim.  An entry whose clip and fallback are both missing is
// not added, so has_anim() answers false and the scene is skipped.
add_anim( animname, scene, clip, fallback )
{
	if ( !isDefined( level.coop_scr_anim ) )
		level.coop_scr_anim = [];
	if ( !isDefined( level.coop_scr_anim[ animname ] ) )
		level.coop_scr_anim[ animname ] = [];
	a = scene_anim( clip, fallback );
	if ( isDefined( a ) )
		level.coop_scr_anim[ animname ][ scene ] = a;
	return a;
}

// Several clips for one scene (IW4's scr_anim[ name ][ scene ][ i ] idle
// variations): one is picked at random each time it is played.
add_anim_variants( animname, scene, clips, fallback )
{
	if ( !isDefined( level.coop_scr_anim ) )
		level.coop_scr_anim = [];
	if ( !isDefined( level.coop_scr_anim[ animname ] ) )
		level.coop_scr_anim[ animname ] = [];
	list = [];
	for ( i = 0; i < clips.size; i++ )
	{
		a = scene_anim( clips[ i ], undefined );
		if ( isDefined( a ) )
			list[ list.size ] = a;
	}
	if ( !list.size && isDefined( fallback ) )
		list[ 0 ] = fallback;
	if ( list.size )
		level.coop_scr_anim[ animname ][ scene ] = list;
	return list.size;
}

has_anim( animname, scene )
{
	return isDefined( level.coop_scr_anim ) && isDefined( level.coop_scr_anim[ animname ] ) &&
		isDefined( level.coop_scr_anim[ animname ][ scene ] );
}

// The clip for a scene (a random variant for a variant list), or undefined.
get_anim( animname, scene )
{
	if ( !has_anim( animname, scene ) )
		return undefined;
	a = level.coop_scr_anim[ animname ][ scene ];
	if ( isArray( a ) )
		return a[ randomInt( a.size ) ];
	return a;
}

guy_anim( guy, scene )
{
	name = guy.animname;
	if ( !isDefined( name ) )
		name = "generic";
	a = get_anim( name, scene );
	if ( !isDefined( a ) && name != "generic" )
		a = get_anim( "generic", scene );
	return a;
}

// --------------------------------------------------------------------------
// Placement.
// --------------------------------------------------------------------------

// The node's origin and angles (an entity, a struct, or undefined: the
// actor's own spot).
node_origin( node, guy )
{
	if ( isDefined( node ) && isDefined( node.origin ) )
		return node.origin;
	return guy.origin;
}

node_angles( node, guy )
{
	if ( isDefined( node ) && isDefined( node.angles ) )
		return node.angles;
	if ( isDefined( node ) && isDefined( node.origin ) )
		return ( 0, 0, 0 );
	return guy.angles;
}

// IW4 anim_teleport / anim_first_frame placement: the actor at the scene's
// start spot and facing.
place_at_start( guy, clip, node )
{
	org = node_origin( node, guy );
	ang = node_angles( node, guy );
	start = getStartOrigin( org, ang, clip );
	angles = getStartAngles( org, ang, clip );
	guy forceTeleport( start, ( 0, angles[ 1 ], 0 ) );
}

// --------------------------------------------------------------------------
// Playing.
// --------------------------------------------------------------------------

// IW4 anim_loop_solo: the actor holds the scene's looping idle at his spot
// (or at the node's start spot) until anim_stop / release.  It is the cast
// pose of animscripts\soldier_stop: the stop animscript keeps playing
// coop_idle_anim while coop_cast is set, and switches to the gesture in
// coop_pose_anim while one is set.  A soldier whose stop animscript is
// already running in its own (non-cast) loop is moved into the cast loop by
// one in-place animscripted cycle of the idle, after which the animscript
// restarts and finds coop_cast set.
anim_loop_solo( guy, scene, node )
{
	a = guy_anim( guy, scene );
	if ( !isDefined( a ) || !isAlive( guy ) )
		return false;
	if ( isDefined( node ) )
		place_at_start( guy, a, node );
	variants = undefined;
	name = guy.animname;
	if ( !isDefined( name ) || !has_anim( name, scene ) )
		name = "generic";
	if ( has_anim( name, scene ) && isArray( level.coop_scr_anim[ name ][ scene ] ) )
		variants = level.coop_scr_anim[ name ][ scene ];
	guy loop_idle( a, variants );
	return true;
}

// The actor holds <clip> as his idle (an animation value), or one of
// <variants> picked at random each cycle.
//
// Most authored idles in the C1 zones are NOT looping clips (their xanim
// loop flag is off: killhouse_*_idle, humvee_idle_*, the breach and hostage
// idles, ...), so held as one knob they stop on their last frame.  IW4's
// anim_loop replays each cycle from its first frame; so does this, through
// the cast pose's one-shot slot (coop_pose_anim is played from its start each
// time it is set), pausing while a gesture plays (play_anim in place).
loop_idle( clip, variants )
{
	if ( !isDefined( clip ) || !isAlive( self ) )
		return;
	if ( !isDefined( self.coop_cast ) )
	{
		self.coop_cast = "scene";
		self.coop_scene_cast = true;
	}
	self.coop_idle_anim = clip;
	self.coop_idle_variants = variants;
	self.coop_pose_anim = clip;
	self notify( "coop_cast_pose" );
	self thread idle_replay();
	if ( !isDefined( self.coop_scene_kicked ) )
	{
		self.coop_scene_kicked = true;
		self thread kick_cast_loop( clip );
	}
}

idle_replay()
{
	self endon( "death" );
	self notify( "coop_idle_replay" );
	self endon( "coop_idle_replay" );
	self endon( "coop_scene_stop" );
	first = true;
	for ( ;; )
	{
		if ( !isDefined( self.coop_scene_busy ) )
		{
			clip = self.coop_idle_anim;
			if ( isDefined( self.coop_idle_variants ) && self.coop_idle_variants.size > 0 )
			{
				clip = self.coop_idle_variants[ randomInt( self.coop_idle_variants.size ) ];
				self.coop_idle_anim = clip;
			}
			if ( !first )
			{
				self.coop_pose_anim = clip;
				self notify( "coop_cast_pose" );
			}
			first = false;
			len = getAnimLength( clip );
			if ( len < 0.5 )
				len = 0.5;
			self.coop_idle_next = getTime() + int( len * 1000 );
		}
		self idle_replay_wait();
	}
}

idle_replay_wait()
{
	self endon( "coop_idle_resume" );
	for ( ;; )
	{
		wait( 0.1 );
		if ( !isDefined( self.coop_scene_busy ) && isDefined( self.coop_idle_next ) && getTime() >= self.coop_idle_next )
			return;
	}
}

kick_cast_loop( clip )
{
	self endon( "death" );
	self endon( "coop_scene_scripted" );
	if ( isDefined( self.coop_scene_scripted_on ) )
		return;
	self setGoalPos( self.origin );
	self animscripted( "coop_scene_kick", self.origin, self.angles, clip, "normal", %body, 1, 0.3 );
	wait( 0.1 );
	self stopanimscripted();
}

// IW4 anim_single_solo: the actor plays the scene once and returns to what he
// was doing (his scene idle, his cast idle, or the soldier animscripts).
//   node: the scene node (undefined: in place, at his spot and facing)
//   mode: "inplace" (default without a node: the clip is played over the
//         stop animscript's cast pose, no root motion), or an animscripted
//         mode: "zonly_physics" (default with a node: the clip's own root
//         motion, gravity and collision, the actor first placed at the
//         scene's start spot), "noclip" (exact root motion, no collision:
//         rides, ropes, stairs)
// Returns the clip's length, or 0 when the scene is not available.  Waits
// for the clip unless nowait is set.
anim_single_solo( guy, scene, node, mode, nowait )
{
	a = guy_anim( guy, scene );
	if ( !isDefined( a ) || !isAlive( guy ) )
		return 0;
	return guy play_anim( a, node, mode, nowait );
}

// The same with an animation value.
play_anim( clip, node, mode, nowait )
{
	if ( !isDefined( clip ) || !isAlive( self ) )
		return 0;
	length = getAnimLength( clip );
	if ( !isDefined( mode ) )
	{
		if ( isDefined( node ) )
			mode = "zonly_physics";
		else
			mode = "inplace";
	}
	trace_scene( clip, mode );
	if ( mode == "inplace" )
		self thread play_gesture( clip, length );
	else
		self thread play_scripted( clip, node, mode, length );
	if ( !isDefined( nowait ) || !nowait )
		self waittill_scene_done( length );
	return length;
}

trace_scene( clip, mode )
{
	if ( getDvarInt( "scr_coop_scene_trace" ) )
	{
		who = "ent " + self getEntityNumber();
		if ( isDefined( self.animname ) )
			who = self.animname;
		codolprintln( "CoDOL coop scene: " + who + " plays a " + getAnimLength( clip ) + " s clip (" + mode + ")" );
	}
}

// Waits out a clip; returns early (never ends the caller's thread) when the
// actor dies or his scene is stopped (release).
waittill_scene_done( length )
{
	start = getTime();
	stop = start + int( length * 1000 );
	while ( getTime() < stop )
	{
		if ( !isDefined( self ) || !isAlive( self ) )
			return;
		if ( isDefined( self.coop_scene_stopped_at ) && self.coop_scene_stopped_at > start )
			return;
		wait( 0.05 );
	}
}

// One playing of a clip through the cast pose (soldier_stop plays
// coop_pose_anim from its start), then back to the idle.
play_gesture( clip, length )
{
	self endon( "death" );
	self notify( "coop_gesture" );
	self endon( "coop_gesture" );
	self endon( "coop_scene_stop" );
	if ( !isDefined( self.coop_cast ) )
	{
		self.coop_cast = "scene";
		self.coop_scene_cast = true;
		if ( !isDefined( self.coop_idle_anim ) )
			self.coop_idle_anim = %casual_stand_idle;
	}
	self.coop_scene_busy = true;
	self.coop_pose_anim = clip;
	self notify( "coop_cast_pose" );
	if ( !isDefined( self.coop_scene_kicked ) )
	{
		self.coop_scene_kicked = true;
		self thread kick_cast_loop( clip );
	}
	self waittill_scene_done( length );
	self.coop_scene_busy = undefined;
	if ( isDefined( self.coop_idle_next ) )
	{
		// The idle replay resumes at once with a fresh cycle.
		self.coop_idle_next = 0;
		self notify( "coop_idle_resume" );
	}
	else
	{
		self.coop_pose_anim = undefined;
		self notify( "coop_cast_pose" );
	}
}

// The clip through animscripted, with its root motion: the actor placed at
// the scene's start (when a node is given) and handed back to his animscripts
// at the end.
play_scripted( clip, node, mode, length )
{
	self endon( "death" );
	// A newer scene started on this actor (even in the frame this one ends)
	// owns the scripted state: this one must not stop it.
	self notify( "coop_scene_scripted" );
	self endon( "coop_scene_scripted" );
	self notify( "coop_gesture" );
	if ( isDefined( node ) )
		place_at_start( self, clip, node );
	self setGoalPos( self.origin );
	self.coop_scene_scripted_on = true;
	self animscripted( "coop_scene", self.origin, self.angles, clip, mode, %body, 1, 0.2 );
	self waittill_scene_done( length );
	self.coop_scene_scripted_on = undefined;
	if ( isAlive( self ) )
	{
		self stopanimscripted();
		self setGoalPos( self.origin );
	}
}

// IW4 anim_first_frame_solo: the actor placed at the scene's start spot and
// facing, waiting there.  IW4 freezes the clip's first frame; this actor
// waits in <idle_scene>'s loop (the scene's own idle, as IW4's waiting
// actors mostly do) or in his current idle when none is given.
anim_first_frame_solo( guy, scene, node, idle_scene )
{
	a = guy_anim( guy, scene );
	if ( !isDefined( a ) || !isAlive( guy ) )
		return false;
	place_at_start( guy, a, node );
	if ( isDefined( idle_scene ) )
		anim_loop_solo( guy, idle_scene );
	return true;
}

// IW4 anim_reach_solo: the actor walks to the scene's start spot (goal
// radius <radius>, default 16) and turns to its start facing; times out after
// <timeout> seconds (default 15) and is then placed there.
anim_reach_solo( guy, scene, node, radius, timeout )
{
	a = guy_anim( guy, scene );
	if ( !isDefined( a ) || !isAlive( guy ) )
		return false;
	org = node_origin( node, guy );
	ang = node_angles( node, guy );
	start = getStartOrigin( org, ang, a );
	angles = getStartAngles( org, ang, a );
	guy reach_spot( start, angles[ 1 ], radius, timeout );
	return true;
}

reach_spot( spot, yaw, radius, timeout )
{
	self endon( "death" );
	if ( !isDefined( radius ) )
		radius = 16;
	if ( !isDefined( timeout ) )
		timeout = 15;
	if ( isDefined( self.coop_scene_cast ) )
	{
		self.coop_cast = undefined;
		self.coop_scene_cast = undefined;
	}
	self.goalradius = radius;
	self setGoalPos( spot );
	for ( t = 0; t < timeout; t += 0.1 )
	{
		if ( distance2D( self.origin, spot ) <= radius + 4 )
			break;
		wait( 0.1 );
	}
	if ( distance2D( self.origin, spot ) > radius + 4 )
		self forceTeleport( spot, ( 0, yaw, 0 ) );
	else
		self forceTeleport( self.origin, ( 0, yaw, 0 ) );
}

// IW4 anim_stop / anim_stopanimscripted: stop the scene and hand the actor
// back to his animscripts (combat, if he is not a cast member).
release( guy )
{
	if ( !isDefined( guy ) || !isAlive( guy ) )
		return;
	guy notify( "coop_scene_stop" );
	guy notify( "coop_scene_scripted" );
	guy.coop_scene_scripted_on = undefined;
	guy.coop_scene_stopped_at = getTime();
	guy stopanimscripted();
	guy.coop_pose_anim = undefined;
	guy.coop_scene_busy = undefined;
	guy.coop_idle_next = undefined;
	guy.coop_idle_variants = undefined;
	if ( isDefined( guy.coop_scene_cast ) )
	{
		guy.coop_cast = undefined;
		guy.coop_scene_cast = undefined;
		guy.coop_idle_anim = undefined;
	}
	guy.coop_scene_kicked = undefined;
	guy notify( "coop_cast_pose" );
}

// A scene's actors all at once (IW4 anim_single on an array): each plays his
// own animname's clip for <scene> at the node; waits for the longest.
anim_single( guys, scene, node, mode )
{
	longest = 0;
	for ( i = 0; i < guys.size; i++ )
	{
		if ( !isDefined( guys[ i ] ) )
			continue;
		len = anim_single_solo( guys[ i ], scene, node, mode, true );
		if ( len > longest )
			longest = len;
	}
	if ( longest > 0 )
		wait( longest );
	return longest;
}

anim_loop( guys, scene, node )
{
	for ( i = 0; i < guys.size; i++ )
		if ( isDefined( guys[ i ] ) )
			anim_loop_solo( guys[ i ], scene, node );
}

// --------------------------------------------------------------------------
// Ambient soldiers: IW4's idle-at-a-spot behaviours.
// --------------------------------------------------------------------------

// The IW4 generic idle a placed soldier stands in, from an authored
// script_animation / animation key (IW4 _patrol / _idle / "smoke", "phone",
// "lean" ...), common to every mission: the patrol and casual sets ship in
// common_human for every coop client.
generic_idle( key )
{
	if ( !isDefined( key ) )
		return %patrol_bored_idle;
	switch ( toLower( key ) )
	{
	case "smoke":
	case "smoking":
	case "patrol_idle_smoke":
		return %patrol_bored_idle_smoke;
	case "phone":
	case "cellphone":
	case "patrol_idle_phone":
		return %patrol_bored_idle_cellphone;
	case "lean":
	case "lean_smoke":
		return %parabolic_leaning_guy_smoking_idle;
	case "crouch":
	case "sit":
		return %casual_crouch_idle;
	case "casual":
	case "stand":
		return %casual_stand_idle;
	case "talk":
		return %casual_stand_v2_twitch_talk;
	}
	a = scene_anim( key, undefined );
	if ( isDefined( a ) )
		return a;
	return %patrol_bored_idle;
}
