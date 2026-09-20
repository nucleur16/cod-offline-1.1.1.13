// zm zombie animscript: zombie_combat.
//
// The native actor path (src/codol/player/codol_actor_scripts.cpp) loads nine
// modules per species -- <animscript>_init/_stop/_move/_combat/_death/_pain/
// _flashed/_turn/_jump -- and refuses the species outright if any one of them
// is missing, so all nine exist.  The species row names the prefix
// "maps/mp/animscripts/zombie"; these resolve out of the zm CATEGORY root and
// are never compiled into an mp or coop match.
//
// WHY THESE ARE NOT PORTS OF T5's ZOMBIE ANIMSCRIPTS.  T5 animates humanoid AI
// (maps/_zombiemode*.gsc, animscripts/*) entirely from GSC, by referencing
// %anim tokens out of #using_animtree("generic_human") and handing them to
// setflaggedanimknoball / animscripted / DoNoteTracks.  MEASURED 2026-09-20:
// this server registers none of those builtins (no "setanim", "setanimknob"
// or "animscripted" in src/), and OpenBLOPS contains a native clip-state
// machine for the DOG only (Game/Shared/bgame/bg_dog_animations_mp.cpp) --
// there is no native humanoid equivalent to port.  So the zombie species row
// carries a NULL animation-state backend, setanimstate() refuses for it by
// name, and these modules keep the actor alive and oriented without claiming
// to animate it.  Filling them in is the scripted-animation workstream, not
// this one; see docs/CODOL_ACTOR_SPECIES_20260920.md section 5.
//
// In melee range of the enemy.

main()
{
	self endon( "killanimscript" );

	self orientmode( "face enemy" );

	// Park until the native AI switches this actor to another animscript.
	// Returning immediately would leave CODOL_ActorSwitchScript holding a dead
	// thread handle for the state it just entered.
	for ( ;; )
		wait( 0.05 );
}

