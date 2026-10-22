// zm zombie animscript: zombie_jump.  The traversal (window / low-wall
// negotiation) module.
//
// Ported from T5 animscripts\traverse\zombie_mantle_over_40.gsc (the clip
// choice, wall_hop_zombie :28) and animscripts\traverse\shared.gsc
// (DoTraverse :85 / DoTraverse_Animation :254 / DoMainTraverse_Animation
// :321), for CoDOL's zm maps, whose every node_negotiation_begin carries
// animscript "zombie_mantle_over_40" (MEASURED: 12 on zm_prototype_sh, 20
// on zm_asylum_sh, 4 on zm_zombie_training, no other value).
//
// HOW IT IS REACHED.  T5 Actor_Negotiation_Think runs the node's animscript
// as the actor's anim-specific script while the path crosses a negotiation
// link.  This server's native negotiation state
// (src/codol/player/codol_actor_lifecycle.cpp, ActorStartNegotiation) does
// the same with this module: when the route reaches a begin node it aligns
// the actor to the node (T5's AnimScripted lerp) and enters main() with
//     begin_origin   the node_negotiation_begin origin (on the floor)
//     begin_angles   its authored angles (facing through the window)
//     end_origin     the node_negotiation_end origin
//     animscript     the node's "animscript" value
// and holds the mover until this thread returns (or 8 s pass).  The actor
// moves by this clip's root motion only, in T5's traverse mode
// nogravity + noclip (AI_ANIM_USE_BOTH_DELTAS_NOCLIP), which is what carries
// it over the 42-unit sill: MEASURED (obj/claude-workers/xanim_probe.py),
// ai_zombie_traverse_v1 carries +64.6 units forward -- the length of the
// window link -- v5 +62.7, v2 +70.1, v6 +108.6, crawl_v1 +76.8, and every
// one ends with a "gravity on" notetrack at t 0.85-0.98.
//
// TAKEN from T5: the speed -> clip table (walk v1/v2, run v5, sprint v6/v7,
// crawler crawl_v1/v4), "face angle" at the start node, nogravity+noclip,
// the knob-all restart at blend 0.2, the notetrack loop until "end", gravity
// back on at the end, and the zombie_start_traverse / zombie_end_traverse
// notifies _zombiemode_spawner waits on.
//
// NOT TAKEN: T5's aiming/ragdoll-death/toCover arms (no shooting AI here),
// teleportThread (dog-only height fix-up), and the traverse-death clips.
//
// The "gravity on" note does what T5's noteTrackGravity does: animmode back
// to "gravity" so the last frames settle the actor onto the floor behind the
// sill instead of leaving it hanging where the clip ended.


#include maps\_zombiemode_utility;
#include animscripts\zombie_shared;

#using_animtree ("generic_human");

main( begin_origin, begin_angles, end_origin, animscript )
{
	self endon( "killanimscript" );
	self endon( "death" );

	// Legacy entry (no negotiation): a level script may still hand this
	// module a clip through self.traverseAnim.
	if ( !isDefined( begin_origin ) )
	{
		if ( isDefined( self.traverseAnim ) )
		{
			play_traverse( self.traverseAnim );
			self.traverseAnim = undefined;
		}
		return;
	}

	anime = pick_traverse_anim();
	if ( !isDefined( anime ) )
	{
		codolprintln( "CoDOL zm: ent " + self getEntityNumber() + " traverse: no clip for move speed " + self.zombie_move_speed );
		return;
	}

	self notify( "zombie_start_traverse" );

	// T5 shared::DoTraverse :147, :161.
	if ( isDefined( begin_angles ) )
		self orientmode( "face angle", begin_angles[ 1 ], 1 );

	play_traverse( anime );

	// T5 DoTraverse :176: back on the run cycle, and 0.2 s of it, before
	// "zombie_end_traverse".  MEASURED (t5cap, Prototype round 1): T5's
	// window climb lasts the clip plus 0.2 s (median 2.15 s for traverse_v1).
	if ( isDefined( self.run_combatanim ) )
	{
		self setAnimKnobAllRestart( self.run_combatanim, %body, 1, 0.2, 1 );
		wait( 0.2 );
	}

	self notify( "zombie_end_traverse" );
}

// T5 zombie_mantle_over_40::wall_hop_zombie :28.
pick_traverse_anim()
{
	if ( is_true( self.has_legs ) )
	{
		speed = self.zombie_move_speed;
		if ( !isDefined( speed ) )
			speed = "walk";

		switch ( speed )
		{
		case "walk":
			pool = [];
			pool[ 0 ] = %ai_zombie_traverse_v1;
			pool[ 1 ] = %ai_zombie_traverse_v2;
			return pool[ randomInt( pool.size ) ];
		case "run":
			return %ai_zombie_traverse_v5;
		case "sprint":
			pool = [];
			pool[ 0 ] = %ai_zombie_traverse_v6;
			pool[ 1 ] = %ai_zombie_traverse_v7;
			return pool[ randomInt( pool.size ) ];
		default:
			return %ai_zombie_traverse_v1;
		}
	}

	pool = [];
	pool[ 0 ] = %ai_zombie_traverse_crawl_v1;
	pool[ 1 ] = %ai_zombie_traverse_v4;
	return pool[ randomInt( pool.size ) ];
}

// T5 shared::DoTraverse_Animation :254 / DoMainTraverse_Animation :321 for a
// single clip: nogravity + noclip, knob-all restart, notetracks until "end".
play_traverse( anime )
{
	self endon( "killanimscript" );
	self endon( "death" );

	self animmode( "nogravity" );
	self animmode( "noclip" );

	// T5 DoMainTraverse_Animation plays the clip at rate 1, not the
	// actor's jittered animplaybackrate.
	self setFlaggedAnimKnobAllRestart( "traverseanim", anime, %body, 1, 0.2, 1 );

	// Bounded by the clip: "end" arrives from XAnimProcessServerNotify when
	// the clip completes (codol_script_anim.c header).  The native state
	// also times out after 8 s should it never come.
	DoNoteTracks( "traverseanim", ::traverse_notetrack );

	self animmode( "gravity" );
}

// T5 animscripts\shared.gsc:565 anim.notetracks["gravity on"] =
// ::noteTrackGravity -- physics comes back for the landing frames.
traverse_notetrack( note )
{
	if ( note == "gravity on" )
		self animmode( "gravity" );
}
