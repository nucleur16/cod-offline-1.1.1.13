// coop soldier animscript: soldier_jump.  Traversals.
//
// In a coop match the soldiers route over the base level's own IW4 node
// graph (src/codol/player/codol_bot_nav.c), whose node_negotiation_begin ->
// node_negotiation_end pairs carry IW4's traversal animscripts:
//   mp_estate_tropical  wall_hop x6, jumpdown_40 x7, jumpdown_96 x3,
//                       jump_up_30 x4, ladder_up x2, slide_across_car,
//                       stepup_52
//   mp_fav_tropical     jumpdown_40, jumpdown_96, ladder_up, jump_up_30 (x2
//                       each), wall_over_40
//   mp_crash_tropical   mantle_on_56 x3, jumpdown_56 x3, wall_hop x2,
//                       ladder_up, ladder_down, ladder_window_up x2,
//                       ladder_window_down x2
//   mp_killhouse_sh     wall_hop x4, jumpdown_40 x3, jumpdown_56 x3,
//                       trench_jump_out x3, wall_over_40 x2
// (MEASURED 2026-10-14, the entity lumps of the base zones).  The native
// negotiation state (ActorStartNegotiation) aligns the soldier on the begin
// node and runs this module with
//     main( begin_origin, begin_angles, end_origin, animscript )
// holding the mover until it returns.  Each animscript plays IW4's clip for
// it (animscripts\traverse\<name>.gsc: wall_hop -> traverse_wallhop /
// traverse_wallhop_3, jumpdown_N -> traverse_jumpdown_N, stepup_52 ->
// traverse_stepup_52, wall_over_40 -> traverse40, slide_across_car,
// trench_jump_out, ladder_up/down -> ladder_climbon/climbup/climbdown/
// climboff ...) in traverseMode noclip + nogravity with gravity back at the
// clip's "gravity on" notetrack, as IW4 shared::DoTraverse does.  A name C1
// carries that IW4 has no script for (jump_up_30, mantle_on_56, the window
// ladders) takes the clip of that shape C1 ships (step_up_low_wall,
// ai_mantle_on_56, ladder_up_end_window / ladder_down_start_window).
//
// IW4 lifts or drops the actor during the clip by the difference between
// the clip's height and the real one (shared::teleportThread, the
// traverse_height key); C1's nodes carry no traverse_height, so the clip is
// fitted to the pair's own begin -> end displacement, spread over the clip,
// which also lands him on the end node.

#include animscripts\soldier_shared;

#using_animtree ( "generic_human" );

// The native traversal starts this with four arguments, as dog_jump; a
// script that declares none is a fatal "too many parameters" error.
main( begin, begin_angles, end, animscript )
{
	self endon( "killanimscript" );
	self endon( "death" );

	stop_aiming();
	self.coop_in_cover = undefined;
	if ( !isDefined( begin ) || !isDefined( end ) )
	{
		self animmode( "gravity" );
		stand_pose( 0.1 );
		wait( 0.1 );
		self animmode( "normal" );
		return;
	}
	yaw = vectorToYaw( end - begin );
	if ( isDefined( begin_angles ) )
		yaw = begin_angles[ 1 ];
	name = clean_name( animscript );
	dz = end[ 2 ] - begin[ 2 ];
	trace_anim( "traverse '" + name + "' dz " + int( dz ) + " over " + int( distance2d( begin, end ) ) );
	stat_bump( "traverse_" + name );

	self.a.movement = "run";
	switch ( name )
	{
	case "ladder_up":
		ladder_up( yaw, end, %ladder_climboff );
		break;
	case "ladder_window_up":
		ladder_up( yaw, end, %ladder_up_end_window );
		break;
	case "ladder_down":
		ladder_down( yaw, end, %ladder_climbon );
		break;
	case "ladder_window_down":
		ladder_down( yaw, end, %ladder_down_start_window );
		break;
	default:
		clip = traverse_clip( name, dz );
		do_traverse( clip, yaw, end, death_anims( name ) );
		break;
	}
	self.coop_traverse_death = undefined;
	self animmode( "gravity" );
	self.a.pose = "stand";
	self.a.movement = "run";
}

// The node's animscript as the levels spell it, lower case and stripped of
// stray bytes (mp_crash_tropical carries "ladder_window\x1f_down").
clean_name( animscript )
{
	if ( !isDefined( animscript ) )
		return "";
	s = tolower( animscript );
	out = "";
	ok = "abcdefghijklmnopqrstuvwxyz0123456789_";
	for ( i = 0; i < s.size; i++ )
	{
		c = s[ i ];
		if ( isSubStr( ok, c ) )
			out = out + c;
	}
	return out;
}

// IW4 animscripts\traverse\<name>.gsc, by name; by height for the rest.
traverse_clip( name, dz )
{
	switch ( name )
	{
	case "wall_hop":
		if ( randomInt( 100 ) < 30 )
			return %traverse_wallhop_3;
		return %traverse_wallhop;
	case "wall_over_40":
	case "window_over_40":
		return %traverse40;
	case "wall_over_96":
		return %traverse90;
	case "jumpdown_40":
	case "jump_down_40":
	case "crouch_jump_down_40":
		return %traverse_jumpdown_40;
	case "jumpdown_56":
	case "jump_down_56":
		return %traverse_jumpdown_56;
	case "jumpdown_96":
	case "jump_down_96":
		return %traverse_jumpdown_96;
	case "jumpdown_130":
	case "jump_down_130":
		return %traverse_jumpdown_130;
	case "stepup_52":
		return %traverse_stepup_52;
	case "step_up":
	case "step_up_12":
	case "jump_up_30":
	case "jump_up_40":
		return %step_up_low_wall;
	case "step_down":
		return %step_down_low_wall;
	case "mantle_on_40":
		return %ai_mantle_on_40;
	case "mantle_on_56":
		return %ai_mantle_on_56;
	case "mantle_over_36":
		return %ai_mantle_over_36;
	case "mantle_over_40":
		return %ai_mantle_over_40;
	case "slide_across_car":
		return %slide_across_car;
	case "trench_jump_out":
	case "trench_jumpout":
		return %trench_jump_out;
	case "jump_across_72":
		return %jump_across_72;
	case "jump_across_100":
		return pick( list2( %jump_across_100_spring, %jump_across_100_lunge ) );
	case "jump_over_high_wall":
		return %jump_over_high_wall;
	case "jump_over_low_wall":
		return %jump_over_low_wall;
	case "window":
	case "window_2":
	case "window_over_quick":
		return %traverse_window_quick;
	case "window_divethrough_36":
		return %traverse_window_m_2_dive;
	case "window_down":
		return %traverse_window_m_2_stop;
	case "stairs_up":
		return %traverse_stair_run;
	case "stairs_down":
		return %traverse_stair_run_down;
	case "windowclimb":
		return %windowclimb;
	}
	if ( dz > 44 )
		return %ai_mantle_on_56;
	if ( dz > 20 )
		return %ai_mantle_on_40;
	if ( dz < -100 )
		return %traverse_jumpdown_130;
	if ( dz < -72 )
		return %traverse_jumpdown_96;
	if ( dz < -46 )
		return %traverse_jumpdown_56;
	if ( dz < -18 )
		return %traverse_jumpdown_40;
	return %traverse_wallhop;
}

// IW4 traverseData["interruptDeathAnim"]: the deaths for the first and the
// second half of a wall or window crossing (soldier_death plays them).
death_anims( name )
{
	d = undefined;
	switch ( name )
	{
	case "wall_over_40":
	case "window_over_40":
		d = [];
		d[ 0 ] = list2( %traverse40_death_start, %traverse40_death_start_2 );
		d[ 1 ] = list2( %traverse40_death_end, %traverse40_death_end_2 );
		break;
	case "wall_over_96":
		d = [];
		d[ 0 ] = list1( %traverse90_start_death );
		d[ 1 ] = list1( %traverse90_end_death );
		break;
	case "window":
	case "window_2":
	case "window_over_quick":
	case "window_divethrough_36":
	case "window_down":
		d = [];
		d[ 0 ] = list1( %traverse_window_death_start );
		d[ 1 ] = list1( %traverse_window_death_end );
		break;
	case "slide_across_car":
		d = [];
		d[ 0 ] = list1( %slide_across_car_death );
		d[ 1 ] = list1( %slide_across_car_death );
		break;
	}
	return d;
}

// IW4 shared::DoTraverse for one clip, fitted to the node pair.  The clip
// plays in noclip from start to end: the fit lands him exactly on the end
// node, so gravity only comes back once he stands there (a gravity switch
// at the clip's "gravity on" note left a soldier whose clip was shorter
// than the gap overlapping the ledge, where the physics refuses every move
// -- MEASURED 2026-10-14, mp_crash_tropical jumpdown_56: 24 units short).
do_traverse( clip, yaw, end, deaths )
{
	self endon( "killanimscript" );
	self endon( "death" );

	self orientmode( "face angle", yaw, 1 );
	self animmode( "nogravity" );
	self animmode( "noclip" );

	delta = getMoveDelta( clip, 0, 1 );
	clipEnd = delta_end( self.origin, yaw, delta ) + ( 0, 0, delta[ 2 ] );
	fix = end - clipEnd;
	len = getAnimLength( clip );
	trace_anim( "traverse clip delta " + delta + " fit " + fix );
	if ( length( fix ) > 160 )
		fix = ( 0, 0, 0 );

	self.coop_traverse_death = deaths;
	self.coop_traverse_phase = 0;
	self setFlaggedAnimKnobAllRestart( "traverseAnim", clip, %body, 1, 0.15, 1 );
	self thread fit_traverse( fix, len * 0.85 );
	DoNoteTracksForTime( len * 0.5, "traverseAnim" );
	self.coop_traverse_phase = 1;
	DoNoteTracksForTime( len * 0.5 - 0.05, "traverseAnim" );
	land_on( end );
	self animmode( "gravity" );
}

// The blend into a traversal clip costs it a few units of its own root
// motion; the last of the gap is closed in one step, as IW4's traversals
// end with a teleport onto the end node.
land_on( end )
{
	if ( distanceSquared( self.origin, end ) < 40 * 40 )
		self teleport( end );
}

// IW4 teleportThreadEx: nudge him by fix in even steps over time.
fit_traverse( fix, time )
{
	self endon( "killanimscript" );
	self endon( "death" );
	if ( lengthSquared( fix ) < 1 )
		return;
	frames = int( time * 20 );
	if ( frames < 1 )
		frames = 1;
	step = ( fix[ 0 ] / frames, fix[ 1 ] / frames, fix[ 2 ] / frames );
	for ( i = 0; i < frames; i++ )
	{
		self teleport( self.origin + step );
		wait( 0.05 );
	}
}

// IW4 traverse\ladder_up.gsc: the climb cycle for as long as the height
// takes, then the clip that steps off at the top.
ladder_up( yaw, end, offAnim )
{
	self endon( "killanimscript" );
	self endon( "death" );
	self orientmode( "face angle", yaw, 1 );
	self animmode( "nogravity" );
	self animmode( "noclip" );
	climbAnim = %ladder_climbup;
	offDelta = getMoveDelta( offAnim, 0, 1 );
	cycleDelta = getMoveDelta( climbAnim, 0, 1 );
	rate = cycleDelta[ 2 ] / getAnimLength( climbAnim );
	climbTime = 0;
	if ( rate > 1 )
		climbTime = ( end[ 2 ] - offDelta[ 2 ] - self.origin[ 2 ] ) / rate;
	self.a.pose = "stand";
	if ( climbTime > 0 )
	{
		self setFlaggedAnimKnobAllRestart( "climbanim", climbAnim, %body, 1, 0.1, 1 );
		DoNoteTracksForTime( climbTime, "climbanim" );
	}
	offEnd = delta_end( self.origin, yaw, offDelta ) + ( 0, 0, offDelta[ 2 ] );
	self setFlaggedAnimKnobAllRestart( "climbanim", offAnim, %body, 1, 0.1, 1 );
	self thread fit_traverse( end - offEnd, getAnimLength( offAnim ) * 0.85 );
	DoNoteTracksForTime( getAnimLength( offAnim ), "climbanim" );
	land_on( end );
}

// IW4 traverse\ladder_down.gsc: onto the ladder, then down the cycle.
ladder_down( yaw, end, onAnim )
{
	self endon( "killanimscript" );
	self endon( "death" );
	self orientmode( "face angle", yaw, 1 );
	self animmode( "nogravity" );
	self animmode( "noclip" );
	self setFlaggedAnimKnobAllRestart( "climbanim", onAnim, %body, 1, 0.1, 1 );
	DoNoteTracksForTime( getAnimLength( onAnim ), "climbanim" );
	climbAnim = %ladder_climbdown;
	cycleDelta = getMoveDelta( climbAnim, 0, 1 );
	rate = cycleDelta[ 2 ] / getAnimLength( climbAnim );
	if ( rate < -1 )
	{
		climbTime = ( end[ 2 ] - self.origin[ 2 ] ) / rate;
		if ( climbTime > 0 )
		{
			self setFlaggedAnimKnobAllRestart( "climbanim", climbAnim, %body, 1, 0.1, 1 );
			// Step off at the foot of the ladder onto the end node.
			self thread fit_traverse( ( end[ 0 ] - self.origin[ 0 ], end[ 1 ] - self.origin[ 1 ], 0 ), climbTime );
			DoNoteTracksForTime( climbTime, "climbanim" );
		}
	}
	fit_traverse( end - self.origin, 0.2 );
	land_on( end );
}
