// zm zombie animscript: zombie_init.
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
// This module also carries the species' #using_animtree declaration, which is
// the ONLY thing that registers "generic_human" in the server animtree
// registry; without it CODOL_ActorPrepareAnimation cannot find the tree and
// refuses every spawn (that is exactly how the dog failed before 2026-09-20).
// maps/mp/_dogs.gsc:97 plays the same role for "dog".

#using_animtree ("generic_human");

main()
{
	self.a = spawnStruct();
	self.a.pose = "stand";
	self.a.movement = "walk";
	self.a.nextStandingHitDying = false;

	self.ignoreSuppression = true;
	self.suppressionThreshold = 1;
	self.chatInitialized = false;
	self.noDodgeMove = true;
	self.disableArrivals = true;

	// Native fields (Scr_AddClassField, codol_actor_lifecycle.cpp).
	self.animplaybackrate = 1;
	self.moveplaybackrate = 1;

	// T5 zombies claw at contact range; the dog's 102 units is a lunge.
	self.meleeAttackDist = 48;
	self.stopAnimDistSq = 1024;
	self.pathEnemyFightDist = 64;

	self setTalkToSpecies( "zombie" );
}
