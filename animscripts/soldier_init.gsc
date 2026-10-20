// coop soldier animscript: soldier_init.
//
// The native actor path loads nine modules per species --
// <animscript>_init/_stop/_move/_combat/_death/_pain/_flashed/_turn/_jump --
// and refuses the species if any one is missing
// (src/codol/player/codol_actor_scripts.cpp LoadSpeciesScripts).  The species
// row names the prefix "animscripts/soldier" (the top-level IW4 singleplayer
// animscripts/ namespace); only the coop species row loads them.
//
// Behaviour reference: IW4 animscripts\init.gsc (the self.a state block and
// the per-actor fields) for a standing, rifle-carrying human.  The clip names
// are CoDOL's: every %anim used by this set is a node of the generic_human
// tree common_mp ships AND an xanim common_gametype_coop ships
// (docs/CODOL_CAMPAIGN_MODE.md section 5).
//
// This module carries the species' #using_animtree declaration, which is what
// registers "generic_human" in the server animtree registry for a coop match
// (zm's zombie_init does the same for zm; no mp module is compiled here).

#include animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self.a = spawnStruct();
	self.a.pose = "stand";
	self.a.movement = "stop";
	self.a.painTime = 0;
	self.a.nodeath = false;
	self.a.lastShootTime = 0;
	self.a.burstShots = 0;

	self.allowpain = true;
	self.ignoreSuppression = false;
	self.noDodgeMove = true;
	self.disableArrivals = true;

	self.animplaybackrate = 0.95 + randomFloat( 0.1 );
	self.moveplaybackrate = 1;

	self.stopAnimDistSq = 4096;
	self.pathEnemyFightDist = 128;
	self.coop_allow_ragdoll = true;

	self setTalkToSpecies( "soldier" );

	// The bootstrap pose cannot be set here: this runs inside
	// CODOL_ActorScriptsInitialize before the server DObj owns the tree (see
	// zm's zombie_init.gsc).  _coopmission::spawn_soldier sets it once
	// spawnActor() has returned.
}
