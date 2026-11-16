// zm zombie animscript: zombie_turn.
//
// T5 HAS NO zombie_turn.  MEASURED: animscripts\ carries zombie_dog_turn.gsc
// but no humanoid zombie turn module, because T5's humanoid zombies turn by
// steering inside the walk cycle rather than by playing a turn-in-place clip.
// This server's native path still demands nine modules per species
// (src/codol/player/codol_actor_scripts.cpp:136) and will refuse the species
// if one is missing, so this module exists and expresses T5's behaviour:
// keep the current cycle running and let the orient mode do the turning.
//
// MEASURED, the shipped ai_zombie set contains no turn-in-place clip of any
// kind (no ai_zombie_turn_*, no ai_zombie_180_*), which is the shipped data
// agreeing with T5 that the humanoid zombie has no turn animation.  Inventing
// one out of another family's clips is exactly what the mandate forbids, so
// nothing is played here.
//
// getdeltaturnyaw is a registered method (codol_actor_lifecycle.cpp:1320) and
// is read so a level script can see how far the turn was, matching what T5's
// dog turn module does with the same value.

#include maps\_zombiemode_utility;

main()
{
	self endon( "killanimscript" );

	self orientmode( "face motion" );

	self.a.lastTurnYaw = self getDeltaTurnYaw();

	// Hold the pose the previous script left running; the native orient does
	// the turning.  Returning immediately would leave CODOL_ActorSwitchScript
	// holding a dead thread handle for the state it just entered.
	for ( ;; )
		wait( 0.05 );
}
