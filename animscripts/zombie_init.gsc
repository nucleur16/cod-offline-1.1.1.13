// zm zombie animscript: zombie_init.
//
// The native actor path (src/codol/player/codol_actor_scripts.cpp:136) loads
// nine modules per species -- <animscript>_init/_stop/_move/_combat/_death/
// _pain/_flashed/_turn/_jump -- and refuses the species outright if any one of
// them is missing, so all nine exist.  The species row names the prefix
// "animscripts/zombie" (T5's own top-level animscripts/zombie_*); only the zm
// species row loads them, so they are never compiled into an mp or coop match.
//
// 2026-09-20: THESE ARE NOW REAL PORTS OF T5's ZOMBIE ANIMSCRIPTS.  The
// previous pass shipped nine parked stubs because this server registered no
// animation builtins.  Commit d4548385 registered twenty
// (docs/CODOL_SCRIPTED_ANIMATION_20260920.md section 8) and MEASURED that the
// %anim reference system and the notetrack producer were already complete, so
// the set below animates through setflaggedanimknoball / setflaggedanim and
// reads notetracks back through animscripts\zombie_shared.
//
// STILL ABSENT, and therefore ported AROUND rather than with, by name:
// animscripted / animscriptedskiprestart / stopanimscripted /
// startscriptedanim (T5's scripted root-motion alignment), useanimtree,
// animcustom, setproneanimnodes, setidleanimoverride, setaiminganims.  Every
// place a T5 path wanted one is called out in the file that wanted it.
//
// ANIMATION NAMES ARE CoDOL's, NOT T5's.  Per the standing mandate, C1 wins
// every conflict.  MEASURED 2026-09-20 by extracting the rawfile
// animtrees/generic_human.atr out of patch_common_mp (5,345 lines, 386
// distinct ai_zombie_* nodes) and intersecting it with the xanim assets the
// zm zone set actually mounts (common_mp, patch_common_mp,
// common_gametype_zm, patch_common_gametype_zm and the three zm map zones;
// docs/zone_assets/ALL_xanim.txt): 225 of the 386 ai_zombie_* tree nodes have
// a shipped xanim behind them in a zm match.  Only those 225 are referenced.
// The two T5 names that lose: %ai_zombie_run_v4 and %ai_zombie_idle_v1_delta
// are declared in CoDOL's tree but ship no xanim, so they are replaced by
// %ai_zombie_run_v1 and %ai_zombie_idle_v1, which do.  See
// docs/CODOL_ZOMBIE_ANIMSCRIPTS_20260920.md section 3.
//
// This module also carries the species' #using_animtree declaration, which is
// what registers "generic_human" in the server animtree registry; without it
// CODOL_ActorPrepareAnimation cannot find the tree and refuses every spawn.
// maps/mp/_dogs.gsc:97 plays the same role for "dog".
//
// NOTE ON WHICH generic_human.atr IS LIVE.  MEASURED, src/cscr_parser.cpp:567:
// Scr_ReadFile intercepts any "animtrees/" path and returns the MOUNTED CoDOL
// archive's copy, falling back to the script roots only when no archive has
// one.  docs/zone_assets/ALL_animtree.txt records generic_human.atr in
// common_mp and patch_common_mp, so the tree that resolves every %anim below
// is CoDOL's, NOT the vendor IW4 copy in bin/home/codol_rawfiles/animtrees/
// (which carries zero ai_zombie nodes and is never consulted in a zm match).


#include maps\_zombiemode_utility;
#include animscripts\zombie_shared;

#using_animtree ("generic_human");

// T5 animscripts\zombie_init.gsc:63 firstInit(), reduced to the level state
// the ported modules read.  Called from main() under a level guard because
// this server has no separate first-frame hook for a species.
firstInit()
{
	if ( isDefined( level.zombie_animscripts_ready ) )
		return;

	level.zombie_animscripts_ready = true;

	init_notetracks();

	// T5 zombie_init.gsc:112.  How long a looping cycle plays before the move
	// script re-picks one.
	level.zombie_run_anim_update_time = 0.4;

	// T5 zombie_melee.gsc:60 resetGiveUpTime(): a zombie that has been in
	// melee this long without landing a hit gives the attack up and re-paths.
	level.zombie_melee_give_up_time = 3;

	// T5 zombie_pain.gsc:31.  Minimum gap between two pain reactions.
	level.zombie_pain_interval = 1.5;

	init_zombie_sounds();
}

// The aliases the animscripts and the spawner drive by hand, as opposed to
// the ones the client plays off "sndnt#" notetracks on its own.
//
// MEASURED, docs/zone_assets/ALL_sound.txt: every alias below ships in
// common_gametype_zm.  An alias that did not would be a silent return, not an
// error (_zombiemode_utility::play_sound_on_ent), so this list is safe to
// extend as more of the zm sound bank is identified.
init_zombie_sounds()
{
	add_sound( "zombie_spawn",        "zmb_zombie_spawn" );
	add_sound( "zombie_ambient",      "zmb_zombie_amb" );
	add_sound( "crawler_ambient",     "zmb_crawler_vocals_amb" );
	add_sound( "zombie_attack",       "zmb_zombie_nt_vocals_attack" );
	add_sound( "zombie_swing",        "zmb_zombie_nt_vocals_swing" );
	add_sound( "zombie_taunt",        "zmb_zombie_nt_vocals_taunt" );
	add_sound( "zombie_crawl",        "zmb_zombie_nt_vocals_crawl" );
	add_sound( "zombie_step",         "zmb_zombie_nt_vocals_step" );
	add_sound( "zombie_bodyfall",     "zmb_zombie_nt_vocals_land" );
	add_sound( "zombie_death",        "zmb_death_vox" );
	add_sound( "zombie_death_mvmt",   "zmb_death_mvmt" );
	add_sound( "zombie_death_gibs",   "zmb_death_gibs" );
	add_sound( "zombie_behind",       "zmb_zombie_behind" );

	// Barricade tear, driven from the "board" notetrack -- see
	// _zombiemode_blockers.gsc.
	add_sound( "break_barrier_piece", "zmb_break_board" );
	add_sound( "board_slam",          "zmb_board_slam" );
}

main()
{
	firstInit();

	// T5 zombie_init.gsc:170.  self.a is the per-actor animation state block
	// every module in this directory reads.
	self.a = spawnStruct();
	self.a.pose = "stand";
	self.a.movement = "walk";
	self.a.nextStandingHitDying = false;
	self.a.nodeath = false;
	self.a.meleeState = "right";
	self.a.painTime = 0;
	// T5 _zombiemode_spawner.gsc:192 zombie_spawn_init: zombies never play a
	// hit flinch (zombie_pain.gsc returns on this flag; only the thundergun
	// and lander knockdowns, through pain_override paths, ever animate).
	self.a.disablePain = true;

	self.allowpain = true;
	self.has_legs = true;

	// T5 animscripts\zombie_death.gsc:79 waitForRagdoll: every regular zombie
	// goes to ragdoll 35% into a death clip that has no start_ragdoll note.
	self.allow_ragdoll = true;

	self.ignoreSuppression = true;
	self.suppressionThreshold = 1;
	self.chatInitialized = false;
	self.noDodgeMove = true;
	self.disableArrivals = true;

	// T5 animscripts\zombie_init.gsc:316-320 set_anim_playback_rate: only
	// animplaybackrate is jittered (0.9-1.1, idles, pain, death); every move
	// cycle plays at moveplaybackrate 1, so a zombie travels at its clip's
	// own root-motion speed (walk ~36 u/s, run ~109, sprint ~140).
	self.animplaybackrate = 0.9 + randomFloat( 0.2 );
	self.moveplaybackrate = 1;

	// T5 zombies claw at contact range; the dog's 102 units is a lunge.
	// T5 _zombiemode_spawner.gsc:3908 zombie_setup_attack_properties sets 64
	// once the zombie is in attack mode; this is the pre-attack value.
	self.meleeAttackDist = 48;
	self.stopAnimDistSq = 1024;
	self.pathEnemyFightDist = 64;

	self setTalkToSpecies( "zombie" );

	// NOT DONE HERE: the bootstrap idle pose.  MEASURED,
	// src/codol/player/codol_actor_scripts.cpp:341-361: this function runs
	// inside CODOL_ActorScriptsInitialize, BETWEEN CODOL_ActorPrepareAnimation
	// and CODOL_ActorFinishInitialization -- and it is FinishInitialization
	// that establishes the server DObj owning the tree, which is exactly the
	// precondition anim_receiver() tests (codol_script_anim.c:143-150).  An
	// animation call from here would be refused with "requires an entity with
	// a server animtree".  _zombiemode_spawner::spawn_zombie plays the
	// bootstrap pose instead, after spawnActor() has returned a fully
	// initialised actor.
}
