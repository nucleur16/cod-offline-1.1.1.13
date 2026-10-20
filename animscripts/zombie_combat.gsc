// zm zombie animscript: zombie_combat.  MELEE -- the hole the previous pass
// left open, now closed with T5's real mechanism.
//
// Ported from T5 animscripts\zombie_combat.gsc:290 (the range gate and the
// handoff) and animscripts\zombie_melee.gsc:13 MeleeCombat (673 lines).
//
// HOW THE DAMAGE HAPPENS, and why nothing is invented.  T5 zombies do not
// damage a player on a timer or on a proximity test.  The attack ANIMATION
// carries a "fire" notetrack, the animscript waits on it, and on that note it
// calls the AI melee builtin, which traces from the actor's eye at the enemy
// and applies the aiweapon's damage.  All three halves of that exist here:
//
//   1. the animations -- MEASURED, CoDOL ships %ai_zombie_attack_v1/v2/v4/v6,
//      %ai_zombie_attack_forward_v1/v2, %ai_zombie_walk_attack_v1..v4,
//      %ai_zombie_run_attack_v1..v3, %ai_zombie_attack_crawl and
//      %ai_zombie_attack_crawl_lunge, all in common_gametype_zm, all declared
//      in the shipped generic_human.atr;
//   2. the notetrack -- MEASURED, docs/CODOL_SCRIPTED_ANIMATION_20260920.md
//      section 3: XAnimProcessServerNotify already issues the animation's
//      notetracks as ordinary notifies on the flag the setter named;
//   3. the damage -- MEASURED, melee() is a registered method
//      (src/codol/player/codol_actor_lifecycle.cpp:1295 ActorMelee) and is
//      complete: it takes the eye tag, aims at CODOL_ActorEnemy, and calls
//      the retained Weapon_Melee_internal, which is the same trace-and-damage
//      body player melee runs (src/asmsource/_codol_actor_melee.asm).
//
// It requires self.aiweapon to name a real WeaponDef; _zombiemode_spawner
// resolves and assigns that ("zombie_melee", which ships in
// common_gametype_zm).  With no aiweapon the builtin raises a script error,
// so the call is guarded and the reason is printed once per actor rather than
// once per swing.
//
// TAKEN, by T5 function:
//   MeleeCombat        zombie_melee.gsc:13   -- the whole loop: face the
//                      enemy, pick the clip for the current move speed, play
//                      it with SetFlaggedAnimKnobAllRestart, pump its
//                      notetracks, land the hit on "fire", release on "end",
//                      and give the attack up after T5's give-up window.
//   resetGiveUpTime    zombie_melee.gsc:60   -- the two randomised windows.
//   pick_zombie_melee_anim zombie_melee.gsc:642 -- the walk/run/crawl/stumpy
//                      selection, unchanged.
//   CanContinueToMelee zombie_melee.gsc      -- reduced to the range test,
//                      which is the only clause a zm zombie can fail.
//
// NOT TAKEN, and why:
//   AiVsAiMeleeCombat -- level.allow_ai_vs_ai_melee is never set in zm.
//   okToMelee / IAmMeleeing / ImNotMeleeing (zombie_utility) -- T5's melee
//     reservation stops several AI charging one target.  Zombies are MEANT to
//     swarm; T5 itself only reserves so the charge animation does not stack,
//     and there is no charge animation here.
//   EyesAtEnemy -- needs setLookAtEntity, not a registered builtin.
//   battleChatter_ai::evaluateMeleeEvent, animscripts\debug, Print3d debug.
//   level.melee_miss_func -- no zm caller sets it; the hook is kept.
//
// C1-vs-T5 CONFLICT, C1 WINS: T5's exit path calls self ClearAnim( %root, 0.3 ).
// MEASURED, CoDOL's generic_human.atr declares no node named root at all (the
// tree's single top-level node is body), so %root would not compile.  %body is
// used everywhere T5 writes %root.


#include maps\_zombiemode_utility;
#include animscripts\zombie_shared;

#using_animtree ("generic_human");

main()
{
	self endon( "killanimscript" );

	for ( ;; )
	{
		self orientmode( "face enemy" );
		self animmode( "zonly_physics" );
		MeleeCombat();

		// Native re-selection leaves the current animscript running when its
		// function is still combat.  Parking here after a missed/given-up swing
		// could therefore strand a zombie in range with no further attacks.
		// Release the script so move/stop can replace it, and retry on the
		// next frame if the enemy remains in range.
		self.safeToChangeScript = true;
		self animmode( "normal" );
		wait( 0.05 );
	}
}

// T5 zombie_melee.gsc:13.
MeleeCombat()
{
	self endon( "killanimscript" );
	self endon( "death" );

	self resetGiveUpTime();

	for ( ;; )
	{
		if ( !isDefined( self.enemy ) )
			return;

		angles = vectorToAngles( self.enemy.origin - self.origin );
		self orientmode( "face angle", angles[ 1 ] );

		self play_sound_on_ent( "zombie_attack" );

		zombie_attack = pick_zombie_melee_anim();
		if ( !isDefined( zombie_attack ) )
			return;

		// T5 :117.  Hold the actor in this animscript until the swing is
		// over; CODOL_ActorRunFrame reads the same flag
		// (codol_actor_lifecycle.cpp:702 and :1397) and will not switch
		// scripts out from under a swing while it is clear.
		self.safeToChangeScript = false;

		self setFlaggedAnimKnobAllRestart( "meleeanim", zombie_attack, %body, 1, 0.2, self.animplaybackrate );

		swing_notetracks();

		self.safeToChangeScript = true;
		self orientmode( "face enemy" );

		if ( getTime() > self.giveUpOnMeleeTime )
			return;

		if ( !CanContinueToMelee() )
			return;
	}
}

// T5 zombie_melee.gsc:126-:166, the inner note loop.
//
// One swing animation lands at most one hit.  The double-swipe attacks carry
// two "fire" notes spaced wider than the 0.4 s ignore window in
// Callback_PlayerDamage, so both connected and a full-health player went down
// from a single attack (two 60s against 100).  The second note now only
// connects when the first one missed.
swing_notetracks()
{
	landed = false;

	for ( ;; )
	{
		self waittill( "meleeanim", note );

		if ( !isDefined( note ) )
			note = "undefined";

		if ( note == "end" || note == "finish" || note == "undefined" )
			return;

		if ( note == "fire" )
		{
			if ( !isDefined( self.enemy ) )
				return;

			if ( landed )
				continue;

			// melee() returns the entity the native trace actually contacted.
			// Another player can stand between this zombie and self.enemy;
			// that contact still consumes this swing, even if damage is blocked.
			hit = self land_melee_hit();

			if ( !isDefined( self.enemy ) )
				return;

			if ( isDefined( hit ) )
			{
				landed = true;
				self resetGiveUpTime();
			}
			else if ( isDefined( level.melee_miss_func ) )
				self [[ level.melee_miss_func ]]();

			continue;
		}

		if ( note == "stop" )
		{
			if ( !CanContinueToMelee() )
				return;
			continue;
		}

		self HandleNoteTrack( note, "meleeanim" );
	}
}

// The guarded call.  See the header: melee() needs a real aiweapon.
land_melee_hit()
{
	if ( !isDefined( self.aiweapon ) || self.aiweapon == "" )
	{
		if ( !isDefined( self.melee_weapon_warned ) )
		{
			self.melee_weapon_warned = true;
			{}
		}
		return;
	}

	return self melee();
}

// T5 zombie_melee.gsc:60.
resetGiveUpTime()
{
	give_up = level.zombie_melee_give_up_time * 1000;

	if ( !isDefined( self.enemy ) )
	{
		self.giveUpOnMeleeTime = getTime() + give_up;
		return;
	}

	// T5 splits on anim.chargeRangeSq (the distance at which an AI would
	// charge rather than swing).  There is no charge here, so the split is on
	// the actor's own attack distance, which is the same question: am I
	// already in reach, or am I swinging at the edge of it.
	reach = self.meleeAttackDist * self.meleeAttackDist;

	if ( distanceSquared( self.origin, self.enemy.origin ) > reach )
		self.giveUpOnMeleeTime = getTime() + randomIntRange( 2700, 3300 );
	else
		self.giveUpOnMeleeTime = getTime() + randomIntRange( 1700, 2300 );
}

// T5 zombie_melee.gsc CanMeleeInternal, reduced to the range clause.
CanContinueToMelee()
{
	if ( !isDefined( self.enemy ) )
		return false;

	if ( !isAlive( self.enemy ) )
		return false;

	// T5 allows a little slack over the attack distance before it gives up,
	// so a player stepping back half a pace does not cancel the combo.
	reach = ( self.meleeAttackDist + 24 ) * ( self.meleeAttackDist + 24 );

	return distanceSquared( self.origin, self.enemy.origin ) <= reach;
}

// T5 zombie_melee.gsc:642 pick_zombie_melee_anim, unchanged apart from being
// a method on the zombie instead of taking it as an argument, and from
// array_combine (not a registered builtin, and _zombiemode_utility has no
// counterpart) being replaced by picking the pool first and then the clip.
pick_zombie_melee_anim()
{
	if ( !isDefined( self.animname ) )
		return undefined;

	if ( is_true( self.has_legs ) )
	{
		speed = self.zombie_move_speed;
		if ( !isDefined( speed ) )
			speed = "walk";

		// T5 combines the standing set with the moving set and picks from the
		// union, i.e. a split between the two pools weighted by their sizes.
		// Picking the pool first with the same weighting and then uniformly
		// inside it is the identical distribution, and is the only form
		// available without array_combine.
		if ( speed == "walk" )
			pool = pick_pool( level._zombie_melee[ self.animname ], level._zombie_walk_melee[ self.animname ] );
		else
			pool = pick_pool( level._zombie_melee[ self.animname ], level._zombie_run_melee[ self.animname ] );

		return pick_from( pool );
	}

	if ( isDefined( self.a.gib_ref ) && self.a.gib_ref == "no_legs" )
		return pick_from( level._zombie_stumpy_melee[ self.animname ] );

	return pick_from( level._zombie_melee_crawl[ self.animname ] );
}

pick_pool( a, b )
{
	if ( !isDefined( b ) || b.size < 1 )
		return a;
	if ( !isDefined( a ) || a.size < 1 )
		return b;

	total = a.size + b.size;
	if ( randomInt( total ) < a.size )
		return a;
	return b;
}

pick_from( pool )
{
	if ( !isDefined( pool ) || pool.size < 1 )
		return undefined;
	return pool[ randomInt( pool.size ) ];
}
