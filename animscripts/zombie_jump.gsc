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
//
// OTHER TRAVERSALS (zm_factory_sh, zm_theater_sh).  Der Riese and Kino also
// author climb and drop links, not only window sills (MEASURED from the
// zones' entity strings, begin -> end):
//   zm_factory_sh  zombie_jump_up_to_climb x6 (+90.6 / +95.6 up, 48.5 fwd:
//                  the tall walls and fences zombies climb onto),
//                  zombie_jump_down_72 x7, _96 x2, _127 x5, _184 x3, _190 x2
//   zm_theater_sh  zombie_jump_down_40/_72/_120/_127/_190/_240/_286
// T5 runs a separate animscripts\traverse\<animscript>.gsc for each; until
// 2026-10-26 this module played the window clips for all of them, so a
// zombie climbing a 96-unit fence did the 40-unit window mount and was then
// snapped onto the top.  pick_traverse_anim now follows the node's
// animscript the way T5 does.  Every clip used is C1's own, from the
// common_zombie animlist (common_gametype_zm; no T5 clip is added), and its
// root motion matches the link (MEASURED, xanim_probe, common_gametype_zm):
//   ai_zombie_jump_up_2_climb        +24.3 fwd  +98.4 up   3.73 s
//   ai_zombie_crawl_jump_up_2_climb  +34.9 fwd  +96.9 up   5.97 s
//   ai_zombie_jump_down_72/_96       +58.9 fwd  -71.0 / -95.1
//   ai_zombie_jump_down_127/184/190  +56.7 fwd  -128.0 / -184.9 / -189.9
//   ai_zombie_crawl_jump_down_72/97/127/184/189  +70..78 fwd, -72..-189
//   ai_zombie_jump_down_40 +32.0 fwd -38.8; _120 +56.7 -116.1;
//   _240 / _286 +56.7 -240.0 / -286.2; crawl _240 / _286 +82.9 -240 / -286
// The climb clip carries no "gravity on" note: gravity comes back when it
// ends, the clip having lifted the root 3-8 units above the top.


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

	anime = pick_traverse_anim( animscript );
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

// T5 animscripts\traverse\<animscript>::main, by the node's animscript.
pick_traverse_anim( animscript )
{
	if ( isDefined( animscript ) )
	{
		anime = pick_climb_or_drop_anim( animscript );
		if ( isDefined( anime ) )
			return anime;
	}
	return pick_window_anim();
}

// T5 zombie_jump_up_to_climb::low_wall_zombie / low_wall_crawler and
// zombie_jump_down_<n>::jump_down_zombie / jump_down_crawler.  C1's list
// lacks T5's crawl_jump_down_120: crawl_jump_down_97 stands in (a clip that
// drops short ends on gravity; one that drops too far would sink through the
// floor in noclip).  T5 has no crawler clip for jump_down_40.  Anything
// without a clip here (zombie_mantle_over_40, a crawler on jump_down_40,
// Kino's zombie_wall_crawl_drop) returns undefined: the window clips.
pick_climb_or_drop_anim( animscript )
{
	legs = is_true( self.has_legs );
	switch ( animscript )
	{
	case "zombie_jump_up_to_climb":
		if ( legs )
			return %ai_zombie_jump_up_2_climb;
		return %ai_zombie_crawl_jump_up_2_climb;
	case "zombie_jump_down_40":
		if ( legs )
			return %ai_zombie_jump_down_40;
		return undefined;
	case "zombie_jump_down_72":
		if ( legs )
			return %ai_zombie_jump_down_72;
		return %ai_zombie_crawl_jump_down_72;
	case "zombie_jump_down_96":
		if ( legs )
			return %ai_zombie_jump_down_96;
		return %ai_zombie_crawl_jump_down_97;
	case "zombie_jump_down_120":
		if ( legs )
			return %ai_zombie_jump_down_120;
		return %ai_zombie_crawl_jump_down_97;
	case "zombie_jump_down_127":
		if ( legs )
			return %ai_zombie_jump_down_127;
		return %ai_zombie_crawl_jump_down_127;
	case "zombie_jump_down_184":
		if ( legs )
			return %ai_zombie_jump_down_184;
		return %ai_zombie_crawl_jump_down_184;
	case "zombie_jump_down_190":
		if ( legs )
			return %ai_zombie_jump_down_190;
		return %ai_zombie_crawl_jump_down_189;
	case "zombie_jump_down_240":
		if ( legs )
			return %ai_zombie_jump_down_240;
		return %ai_zombie_crawl_jump_down_240;
	case "zombie_jump_down_286":
		if ( legs )
			return %ai_zombie_jump_down_286;
		return %ai_zombie_crawl_jump_down_286;
	}
	return undefined;
}

// T5 zombie_mantle_over_40::wall_hop_zombie :28.
pick_window_anim()
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
