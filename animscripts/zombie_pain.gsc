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
//   :28  / :374 playThundergunPainAnim -- the thundergun knockdown and
//        getup (SetFlaggedAnimKnobAllRestart, as T5).  T5 tests
//        self.damageweapon; _zombiemode_weap_thundergun::zombie_knockdown
//        sets .thundergun_knockdown for the hit instead.  Taken before the
//        disablePain gate, as T5 :28 is.
//   the SetFlaggedAnim + notetrack wait
//
// NOT TAKEN, and why:
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


#include maps\_zombiemode_utility;
#include animscripts\zombie_shared;

#using_animtree ("generic_human");

main()
{
	self endon( "killanimscript" );
	self endon( "death" );

	self setFlashBanged( false );

	// T5 :28.
	if ( is_true( self.thundergun_knockdown ) )
	{
		self.thundergun_knockdown = undefined;
		if ( !is_true( self.isdog ) )
		{
			self playThundergunPainAnim();
			return;
		}
	}

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

// T5 zombie_pain.gsc:374 playThundergunPainAnim.  The fall is chosen by the
// side the blast came from; a zombie with legs then gets up.  Each wait is
// bounded by its clip's length (see the contract above).
playThundergunPainAnim()
{
	self notify( "end_play_thundergun_pain_anim" );
	self endon( "end_play_thundergun_pain_anim" );

	if ( is_true( self.marked_for_death ) )
		return;
	if ( !isDefined( level._zombie_knockdowns ) || !isDefined( level._zombie_knockdowns[ "zombie" ] ) )
		return;

	// C1's client tree has no thundergun knockdown clips (tg_anim, in
	// _zombiemode_weap_thundergun.gsc): no knockdown is played then.
	if ( !isDefined( codol_getAnim( "generic_human", "ai_zombie_thundergun_hit_armslegsforward" ) ) )
		return;

	knockdowns = level._zombie_knockdowns[ "zombie" ];
	getups = level._zombie_getups[ "zombie" ];

	// This server's damageyaw is the yaw towards the attacker, 0..360; T5's is
	// the yaw of the damage direction, -180..180.
	yaw = self.damageyaw + 180;
	while ( yaw > 180 )
		yaw -= 360;
	while ( yaw < -180 )
		yaw += 360;

	if ( yaw <= -135 || yaw >= 135 )
	{
		if ( !is_true( self.has_legs ) )
			pool = knockdowns[ "front" ][ "no_legs" ];
		else
			pool = knockdowns[ "front" ][ "has_legs" ];

		fallAnim = common_scripts\utility::random( pool );
		if ( isDefined( level.thundergun_last_knockdown_anim ) && level.thundergun_last_knockdown_anim == fallAnim )
			fallAnim = common_scripts\utility::random( pool );

		level.thundergun_last_knockdown_anim = fallAnim;
		if ( tg_is( fallAnim, "ai_zombie_thundergun_hit_deadfallknee" ) || tg_is( fallAnim, "ai_zombie_thundergun_hit_forwardtoface" ) )
			getupAnim = common_scripts\utility::random( getups[ "belly" ][ "early" ] );
		else
			getupAnim = common_scripts\utility::random( getups[ "back" ][ "early" ] );
	}
	else if ( yaw > -135 && yaw < -45 )
	{
		fallAnim = common_scripts\utility::random( knockdowns[ "left" ] );
		getupAnim = common_scripts\utility::random( getups[ "belly" ][ "early" ] );
	}
	else if ( yaw > 45 && yaw < 135 )
	{
		fallAnim = common_scripts\utility::random( knockdowns[ "right" ] );
		getupAnim = common_scripts\utility::random( getups[ "belly" ][ "early" ] );
	}
	else
	{
		fallAnim = common_scripts\utility::random( knockdowns[ "back" ] );
		getupAnim = common_scripts\utility::random( getups[ "belly" ][ "early" ] );
	}

	codolPrintLn( "CoDOL zm thundergun: zombie " + self getEntityNumber() + " knockdown, blast yaw " + int( yaw ) );

	self animmode( "gravity" );
	self setFlaggedAnimKnobAllRestart( "painanim", fallAnim, %body, 1, 0.2, self.animplaybackrate );
	DoNoteTracksForTime( getAnimLength( fallAnim ), "painanim", self.thundergun_handle_pain_notetracks );

	if ( !isDefined( self ) || !isAlive( self ) || !is_true( self.has_legs ) || is_true( self.marked_for_death ) )
	{
		self animmode( "normal" );
		return; // guy died on us
	}

	if ( tg_is( getupAnim, "ai_zombie_thundergun_getup_quick_a" ) && isDefined( self.a.gib_ref ) &&
		( self.a.gib_ref == "left_arm" || self.a.gib_ref == "right_arm" ) )
	{
		self animmode( "normal" );
		return;
	}

	self setFlaggedAnimKnobAllRestart( "painanim", getupAnim, %body, 1, 0.2, self.animplaybackrate );
	DoNoteTracksForTime( getAnimLength( getupAnim ), "painanim" );

	self clearAnim( getupAnim, 0.2 );
	self animmode( "normal" );
}

// Name comparison without a %anim (see _zombiemode_weap_thundergun.gsc tg_anim).
tg_is( a, name )
{
	b = codol_getAnim( "generic_human", name );
	return isDefined( a ) && isDefined( b ) && a == b;
}
