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
		self notify( "t5cap_mpick", zombie_attack );	// telemetry (maps/_zombiemode_t5cap.gsc)

		// T5 :117-:120: only an actor with noChangeDuringMelee (the director,
		// the monkeys) holds its animscript through the swing.  A regular
		// zombie leaves safeToChangeScript set, so the engine takes it off a
		// swing and back onto its path the moment the enemy is out of reach.
		// MEASURED (t5cap, Prototype, player walking at 120 u/s): T5 zombies
		// spent 14 % of their time in combat and landed 0.63 swings per
		// zombie-second in reach; CoDOL, which held every swing to the end,
		// 34 % and 1.66 -- they stood swinging at a player who had walked off.
		if ( is_true( self.noChangeDuringMelee ) )
			self.safeToChangeScript = false;

		// T5 zombie_melee.gsc:116 plays the attack at rate 1.
		self setFlaggedAnimKnobAllRestart( "meleeanim", zombie_attack, %body, 1, 0.2, 1 );

		swing_notetracks( zombie_attack );

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
// Every "fire" note swings (T5 zombie_melee.gsc:126-:150 calls melee() on
// each).  The double-swipe clips carry two notes about 0.35-0.4 s apart; a
// second hit lands when it falls outside Callback_PlayerDamage's 0.4 s
// per-attacker ignore window, as in T5, where one double swipe can down a
// player without Juggernog.
//
// T5 hit timing (2026-10-21, docs/CODOL_ZOMBIE_T5_MEASURED.md section 3).
// C1 re-timed the "fire" notes of several of these clips while keeping their
// lengths: MEASURED with getNotetrackTimes in real T5 against the C1 xanims,
// ai_zombie_attack_v1 hits at 0.462/0.722 of the clip in T5 and 0.204/0.574
// in C1, attack_forward_v1 at 0.414/0.6 vs 0.214/0.714, walk_attack_v1 at
// 0.187 vs 0.344, and walk_attack_v2/v4 carry a doubled note 0.1 s apart.  So
// a zombie hit sooner and more often than T5's (0.85-0.96 swings landing per
// zombie-second in reach vs T5's 0.75).  A clip with a T5 schedule
// (_zombiemode.gsc zombie_t5_melee_fire_times) lands its hits at T5's
// fractions of the clip and ignores the C1 "fire" notes.
swing_notetracks( zombie_attack )
{
	t5fires = undefined;
	if ( isDefined( zombie_attack ) )
		t5fires = maps\_zombiemode::zombie_t5_melee_fire_times( zombie_attack );
	if ( isDefined( t5fires ) )
		self thread t5_fire_schedule( zombie_attack, t5fires );

	for ( ;; )
	{
		self waittill( "meleeanim", note );

		if ( !isDefined( note ) )
			note = "undefined";

		if ( note == "end" || note == "finish" || note == "undefined" )
		{
			self notify( "t5_swing_done" );
			return;
		}

		if ( note == "fire" )
		{
			if ( isDefined( t5fires ) )
				continue;

			if ( !self swing_hit() )
			{
				self notify( "t5_swing_done" );
				return;
			}
			continue;
		}

		if ( note == "stop" )
		{
			if ( !CanContinueToMelee() )
			{
				self notify( "t5_swing_done" );
				return;
			}
			continue;
		}

		self HandleNoteTrack( note, "meleeanim" );
	}
}

// One "fire": T5 zombie_melee.gsc:130-:150.  False when the enemy is gone,
// which ends the swing as T5's break does.
swing_hit()
{
	if ( !isDefined( self.enemy ) )
		return false;

	oldhealth = self.enemy.health;
	self notify( "swing_hit" );	// telemetry (maps/_zombiemode_t5cap.gsc)
	self land_melee_hit();

	if ( !isDefined( self.enemy ) )
		return false;

	if ( self.enemy.health < oldhealth )
		self resetGiveUpTime();
	else if ( isDefined( level.melee_miss_func ) )
		self [[ level.melee_miss_func ]]();
	return true;
}

// T5's "fire" notes for this clip, as fractions of the clip; the clip plays
// at rate 1 (setFlaggedAnimKnobAllRestart above).
t5_fire_schedule( zombie_attack, fractions )
{
	self endon( "death" );
	self endon( "killanimscript" );
	self endon( "t5_swing_done" );

	length = getAnimLength( zombie_attack );
	elapsed = 0;
	for ( i = 0; i < fractions.size; i++ )
	{
		at = fractions[ i ] * length;
		if ( at > elapsed )
			wait( at - elapsed );
		elapsed = at;
		if ( !self swing_hit() )
			return;
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
			println( "CoDOL zm: a zombie has no aiweapon; its melee notetrack cannot damage anyone" );
		}
		return;
	}

	self melee();
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
