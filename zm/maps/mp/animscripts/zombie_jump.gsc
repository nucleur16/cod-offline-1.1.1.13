// zm zombie animscript: zombie_jump.  Traversals.
//
// STATE OF THIS MODULE, stated plainly rather than stubbed:
//
// 1. MEASURED, src/codol/player/codol_actor_lifecycle.cpp:866: the native
//    frame selects only pain, combat, move, stop and death.  Nothing in this
//    server ever selects <animscript>_jump.  The module exists because
//    CODOL_ActorScriptsLoadType refuses a species whose nine modules are not
//    all present (codol_actor_scripts.cpp:136).
//
// 2. T5 reaches the zombie traversal set (%ai_zombie_traverse_v1..v7,
//    %ai_zombie_jump_down_*, %ai_zombie_jump_up_2_climb -- all MEASURED
//    present in common_gametype_zm and in generic_human.atr) through
//    AnimScripted, which aligns the clip's root motion onto the traversal
//    node's start and end points.  animscripted is deliberately NOT a
//    registered builtin (docs/CODOL_SCRIPTED_ANIMATION_20260920.md section 5)
//    and stubbing it is forbidden.  Without it a traversal clip would play
//    the pose while the actor stayed where the native mover put it.
//
// So: the pose half is implemented and reachable by anything that sets
// self.traverseAnim, and the alignment half is absent and named.  Nothing
// here pretends the traversal works.


#include maps\mp\gametypes\_zombiemode_utility;
#include maps\mp\animscripts\zombie_shared;

#using_animtree ("generic_human");

main()
{
	self endon( "killanimscript" );
	self endon( "death" );

	if ( isDefined( self.traverseAnim ) )
	{
		self animmode( "nogravity" );
		self setFlaggedAnimKnobAllRestart( "traverseanim", self.traverseAnim, %body, 1, 0.2, self.animplaybackrate );

		DoNoteTracksForTime( getAnimLength( self.traverseAnim ), "traverseanim" );

		self.traverseAnim = undefined;
		self animmode( "normal" );
	}

	// Hold rather than return: returning would leave CODOL_ActorSwitchScript
	// holding a dead thread handle for the state it just entered.
	for ( ;; )
		wait( 0.05 );
}
