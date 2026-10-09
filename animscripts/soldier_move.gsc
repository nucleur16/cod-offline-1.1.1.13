// coop soldier animscript: soldier_move.  The run / patrol-walk cycle.
//
// The native frame (CODOL_ActorRunFrame) selects <animscript>_move whenever
// the actor has a route and is not in combat or pain; the native mover owns
// translation from the clip's root motion, this module owns the pose.
//
// IW4 animscripts\run.gsc MoveStandCombatNormal plays run_lowready_F, the
// tree's own "primary run animation" (generic_human.atr combatrun_forward);
// a soldier without an enemy patrols at a walk (IW4 _patrol's
// patrol_bored_patrolwalk).  Both clips ship in common_gametype_coop.

#include animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self endon( "killanimscript" );

	self orientmode( "face motion" );
	self animmode( "normal" );

	for ( ;; )
	{
		if ( isDefined( self.enemy ) || isDefined( self.coop_hunting ) )
		{
			self.a.movement = "run";
			moveAnim = %run_lowready_F;
		}
		else
		{
			self.a.movement = "walk";
			moveAnim = %patrol_bored_patrolwalk;
		}

		self setFlaggedAnimKnobAll( "runanim", moveAnim, %body, 1, 0.2, self.moveplaybackrate );
		DoNoteTracksForTime( 0.4, "runanim" );
	}
}
