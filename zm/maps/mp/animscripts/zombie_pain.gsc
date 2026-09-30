// zm zombie animscript: zombie_pain.  The flinch.
//
// Ported from T5 animscripts\zombie_pain.gsc:9 main (618 lines), the ordinary
// (non-thundergun, non-special) arm.
//
// CONTRACT WITH THE NATIVE PATH.  MEASURED,
// src/codol/player/codol_actor_lifecycle.cpp:863-869: the frame keeps
// <animscript>_pain selected for as long as THIS THREAD IS ALIVE
// (painRunning), and only then re-evaluates move/combat/stop.  So the flinch
// ends when this function returns, and the function must always return.
// Every wait below is therefore bounded by the clip's own length rather than
// by waiting for an "end" that a missing notetrack would never deliver.
//
// TAKEN, by T5 line:
//   :11  SetFlashBanged(false)
//   :36  the self.a.disablePain gate
//   :44  self.a.painTime = GetTime()
//   :51  the nextStandingHitDying rule (the next standing hit kills)
//   :71  AnimMode("gravity")
//   :90  getPainAnim, reduced to the two pools CoDOL ships
//   the SetFlaggedAnim + notetrack wait
//
// NOT TAKEN, and why:
//   playThundergunPainAnim (:22, :28) -- the thundergun knockdown/getup set.
//     MEASURED: %ai_zombie_thundergun_* IS shipped, but T5 drives the
//     knockdown through AnimScripted (root-motion alignment onto a landing
//     spot), which is not a registered builtin.  Left for whoever ports the
//     thundergun weapon script; this file's hook is self.pain_override.
//   anim.pain_test, self.a.flamepainTime, specialPain, helmetPop -- SP
//     systems with no zm caller.
//   animscripts\face::SayGenericDialogue -- no face tree.
//   the location-specific additive "hit" pose (T5's second animation) --
//     it is played with SetFlaggedAnimLimited onto an additive node chosen
//     from self.damageLocation.  CoDOL's shipped ai_zombie set has exactly
//     two arm-hit clips and no per-bone additive. Play the selected reaction
//     as a complete branch while the native pain state holds the actor.
//
// C1-vs-T5 CONFLICT, C1 WINS: T5's pain pool for animname "zombie" is
// level.scr_anim["zombie"]["pain*"], fed from clips CoDOL does not ship.
// The clips CoDOL DOES ship for a zombie reacting to a hit are
// %ai_zombie_shot_arm_left / %ai_zombie_shot_arm_right (standing) and
// %ai_zombie_walk_on_hands_shot_a / _b (crawling) -- MEASURED present in
// common_gametype_zm and declared in generic_human.atr.  Those are the pool.


#include maps\mp\gametypes\_zombiemode_utility;
#include maps\mp\animscripts\zombie_shared;

#using_animtree ("generic_human");

main()
{
	self endon( "killanimscript" );
	self endon( "death" );

	self setFlashBanged( false );

	if ( is_true( self.a.disablePain ) || !is_true( self.allowpain ) )
		return;

	// T5 :51.  A zombie marked by the previous hit dies on this one.
	if ( is_true( self.a.nextStandingHitDying ) )
		self.health = 1;

	// T5 :44, plus the rate limit T5 gets from anim.pain_test.
	now = getTime();
	if ( isDefined( self.a.painTime ) && now - self.a.painTime < level.zombie_pain_interval * 1000 )
		return;
	self.a.painTime = now;

	if ( isDefined( self.pain_override ) )
	{
		self [[ self.pain_override ]]();
		return;
	}

	self animmode( "gravity" );

	painAnim = getPainAnim();
	if ( !isDefined( painAnim ) )
		return;

	// The shipped ai_zombie_idle_v1 lives beneath add_idle : additive, while
	// these reaction clips live beneath the separate body > scripted > zombie
	// branch.  A limited anim sets only the leaf weight; the zombie branch
	// remains inactive and the client displays a bind/T pose on every hit.
	// Complete the reaction's ancestor chain as the primary pain animation.
	self setFlaggedAnimKnobAll( "painanim", painAnim, %body, 1, 0.1, self.animplaybackrate );

	DoNoteTracksForTime( getAnimLength( painAnim ), "painanim" );

	self clearAnim( painAnim, 0.2 );
	self animmode( "normal" );
}

// T5 zombie_pain.gsc:90 getPainAnim, retargeted -- see the header.
getPainAnim()
{
	if ( is_true( self.has_legs ) )
	{
		if ( randomInt( 2 ) == 0 )
			return %ai_zombie_shot_arm_left;
		return %ai_zombie_shot_arm_right;
	}

	if ( randomInt( 2 ) == 0 )
		return %ai_zombie_walk_on_hands_shot_a;
	return %ai_zombie_walk_on_hands_shot_b;
}
