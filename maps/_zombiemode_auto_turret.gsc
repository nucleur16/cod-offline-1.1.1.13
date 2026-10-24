// zm auto turret: the sentry the kis_sentry_zm power-up drops.
//
// Behaviour from T5's maps\_zombiemode_auto_turret.gsc (258 lines,
// D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\_zombiemode_auto_turret.gsc),
// hardware from C1.  Line numbers are into the T5 file unless named.
//
// WHY NOT maps\mp\killstreaks\_autosentry.  The multiplayer sentry precaches
// multiplayer assets and includes maps\mp\_utility / _hud_util, i.e. the
// multiplayer chain that zm's _callbacksetup.gsc exists to keep out of a zm
// compile.  Nothing here names a multiplayer module.
//
// WHICH TURRET.  MEASURED: C1 catalogue row 54 sentry_zm names
// killstreak_sentry_mp, the multiplayer sentry reward, and
// common_gametype_zm ships the whole multiplayer sentry kit alongside it:
// weapon sentry_minigun_mp, models sentry_minigun / sentry_minigun_destroyed
// (docs/zone_assets/ALL_weapon.txt:939, ALL_xmodel.txt), and
// explosions/sentry_gun_explosion, sentry_gun_plant, zmb_turret_startup,
// zmb_turret_down (ALL_fx.txt, ALL_sound.txt).  So the turret is the
// multiplayer sentry's weapon and model, which is what
// maps\mp\killstreaks\_autosentry.gsc:37-38 spawns for that reward.
//
// TAKEN from T5:
//   init                 :5    -- max 2 active, 30 s timeout.
//   auto_turret_think    :49   -- SetConvergenceTime( 0.3 ),
//                                 SetTurretTeam( "allies" ), unusable.
//   auto_turret_activate :176  -- evict the oldest when two are up; mode on.
//   auto_turret_deactivate :238
//   auto_turret_update_timeout :250
// NOT TAKEN: the use trigger, its cost, the power_on wait and the zapper
// handle (T5 buys map-authored turrets; C1 drops one as a power-up, so the
// power-up grab is the purchase).  The create_and_play_dialog lines
// (_zombiemode_audio is not ported).
//
// C1 / THIS-SERVER INTERPRETATION:
//   * Mode "auto_nonai", T5's own (:220).  It carries the native
//     auto-acquire bit (src/codol/weapon/codol_turret_heat.cpp
//     CODOL_TurretHeat_SetMode).  BY SOURCE, TurretAuto_Acquire
//     (src/codol/script/codol_script_turret_target.cpp) accepts zombies: the
//     zombie aitype calls setspawnerteam( "axis" )
//     (aitype/enemy_zombie_zm.gsc:68), CODOL_ActorAllocate registers each
//     one as an axis sentient with isActor set
//     (src/codol/player/codol_actor_lifecycle.cpp:167-184), and an "allies"
//     turret walks class mask 0xa = axis | team3 (CODOL_Sentient_ClassMask).
//   * MEASURED 2026-09-24 (parent probe, developer trace in
//     TurretAuto_Acquire): the native walk offers every zombie and accepts
//     the ones inside the weapon's arc.  sentry_minigun_mp turns only
//     -55..55 degrees of yaw and -45..20 of pitch about the facing it was
//     planted with, as the multiplayer sentry does, so zombies behind the
//     turret are not engaged.  That is the reward's design, not a gap; an
//     earlier script stand-in that handed out targets is gone.
//   * T5 fires with maps\_mgturret::burst_fire_unmanned.  The native think
//     here aims and runs IW4's fire-state machine, but the shot itself is the
//     script's shootTurret() while isFiringTurret() is true, and the state
//     machine only leaves "none" with the startFiring() flag set
//     (codol_script_turret_target.cpp TurretTarget_UpdateFireState).  That is
//     the IW4 sentry contract (maps\mp\killstreaks\_autosentry.gsc
//     sentry_attackTargets / sentry_burstFireStart), used here without its
//     multiplayer heat bookkeeping: the native CODOL_TurretHeat_CanFire gate
//     already refuses shots while overheated.
//   * The turret shoots as itself (TurretTarget_Attacker returns the turret
//     when it has no user), so its kills pay no points, as T5's do not.
//   * Placement: the native canPlayerPlaceSentry hull and four feet,
//     matching the MP sentry's supported floor/slope frame and view yaw.
//     Retry briefly when the grabber is against a wall or over an edge;
//     never plant at an unvalidated bullet-trace endpoint.

#include maps\_zombiemode_utility;

init()
{
	level.zm_sentry_ok = false;

	level.zm_sentry_weapon = "sentry_minigun_mp";
	level.zm_sentry_model = "sentry_minigun";
	level.zm_sentry_model_destroyed = "sentry_minigun_destroyed";

	precacheItem( level.zm_sentry_weapon );
	if ( !codol_isWeaponMounted( level.zm_sentry_weapon ) )
		return;

	precacheModel( level.zm_sentry_model );
	precacheModel( level.zm_sentry_model_destroyed );
	precacheString( &"SENTRY_CANNOT_PLACE" );

	level._effect[ "zm_sentry_explode" ] = loadfx( "explosions/sentry_gun_explosion" );

	// T5 :21-:36.
	level.curr_auto_turrets_active = 0;
	if ( !isDefined( level.max_auto_turrets_active ) )
		level.max_auto_turrets_active = 2;
	if ( !isDefined( level.auto_turret_timeout ) )
		level.auto_turret_timeout = 30;

	level.zm_sentries = [];
	level.zm_sentry_ok = true;
	level thread auto_turret_game_end_watch();
}

// Zombies emits end_game, not the MP game_ended notification. Stop native
// firing and all active turret workers before intermission parks players.
auto_turret_game_end_watch()
{
	level waittill( "end_game" );
	turrets = level.zm_sentries;
	for ( i = 0; i < turrets.size; i++ )
	{
		if ( isDefined( turrets[ i ] ) )
			turrets[ i ] auto_turret_deactivate( "end_game" );
	}
}

// The power-up grab.  self is the grabber.
sentry_powerup( drop_origin )
{
	if ( !is_true( level.zm_sentry_ok ) || is_true( level.intermission ) )
		return;

	self endon( "disconnect" );
	self endon( "death" );
	level endon( "end_game" );

	// Use the engine's real sentry hull/four-foot placement, as the MP
	// sentry does. A bullet point trace can hit a prop and gives no support
	// or slope validation; its miss endpoint is not a grounded position.
	placement = self canPlayerPlaceSentry();
	for ( attempt = 0; !placement[ "result" ] && attempt < 100; attempt++ )
	{
		wait( 0.1 );
		placement = self canPlayerPlaceSentry();
	}
	if ( !placement[ "result" ] )
	{
		self iPrintLn( &"SENTRY_CANNOT_PLACE" );
		{}
		return;
	}
	spot = placement[ "origin" ];
	angles = placement[ "angles" ];
	yaw = angles[ 1 ];
	face = sentry_visible_zombie( spot, undefined );
	turret = spawnTurret( "misc_turret", spot, level.zm_sentry_weapon );
	if ( !isDefined( turret ) )
	{
		{}
		return;
	}

	turret.angles = angles;
	turret setModel( level.zm_sentry_model );
	turret makeUnusable();
	turret makeTurretInoperable();
	turret.owner = self;
	turret.zm_kills = 0;

	// T5 :80-:81.
	turret setConvergenceTime( 0.3 );
	turret setTurretTeam( "allies" );

	turret playSound( "sentry_gun_plant" );

	{}

	turret thread auto_turret_activate( self );
}

// A placed multiplayer sentry is aimed by its owner.  This pickup plants
// immediately, so face the nearest live zombie with a clear path to its
// native acquisition point (origin + 2).  Before the turret has a model,
// its muzzle is approximated at ground + 40; the native test later uses
// tag_aim and remains authoritative for cover and target checks.
sentry_visible_zombie( spot, ignore )
{
	if ( !isDefined( level.zombies ) )
		return undefined;

	best = undefined;
	best_dist = 8192 * 8192;
	from = spot + ( 0, 0, 40 );
	for ( i = 0; i < level.zombies.size; i++ )
	{
		zombie = level.zombies[ i ];
		if ( !isDefined( zombie ) || !isAlive( zombie ) || zombie getDontTarget() )
			continue;

		d = distanceSquared( spot, zombie.origin );
		if ( d >= best_dist )
			continue;
		trace = bulletTrace( from, zombie.origin + ( 0, 0, 2 ), false, ignore );
		if ( trace[ "fraction" ] < 1 &&
			( !isDefined( trace[ "entity" ] ) || trace[ "entity" ] != zombie ) )
			continue;

		best = zombie;
		best_dist = d;
	}
	return best;
}

// T5 :176.  self is the turret.
auto_turret_activate( owner )
{
	self endon( "turret_deactivated" );
	self endon( "death" );

	// T5 :188: evict the turret with the least time left.
	while ( level.curr_auto_turrets_active >= level.max_auto_turrets_active )
	{
		worst = undefined;
		for ( i = 0; i < level.zm_sentries.size; i++ )
		{
			other = level.zm_sentries[ i ];
			if ( !isDefined( other ) || other == self )
				continue;
			if ( !isDefined( worst ) || other.curr_time < worst.curr_time )
				worst = other;
		}

		if ( !isDefined( worst ) )
			break;

		worst auto_turret_deactivate( "evicted" );
	}

	level.curr_auto_turrets_active++;
	level.zm_sentries[ level.zm_sentries.size ] = self;
	self.turret_active = true;
	self.zm_shots = 0;

	// setsentryowner requires an auto or sentry mode, so it follows setmode.
	self setMode( "auto_nonai" );
	if ( isDefined( owner ) )
		self setSentryOwner( owner );
	self startFiring();

	self playSound( "zmb_turret_startup" );

	{}
	level thread auto_turret_lifetime_watch( self, self getEntityNumber() );

	self.curr_time = level.auto_turret_timeout;
	self thread auto_turret_update_timeout();
	self thread auto_turret_fire();
	self thread auto_turret_watch_targets();
	if ( getDvarInt( "zm_sentry_diag" ) > 0 )
		{}

	wait( level.auto_turret_timeout );

	self auto_turret_deactivate( "timeout" );
}

// Two opt-in snapshots after the native target walk has run.  This avoids
// querying model tags, which can raise a script error when a model is missing.
auto_turret_acquire_diag()
{}

// T5 :238, plus the removal a dropped (not map-authored) turret needs.
auto_turret_deactivate( reason )
{
	if ( !is_true( self.turret_active ) )
		return;

	self.turret_active = false;
	self.curr_time = -1;
	level.curr_auto_turrets_active--;

	remaining = [];
	for ( i = 0; i < level.zm_sentries.size; i++ )
	{
		if ( isDefined( level.zm_sentries[ i ] ) && level.zm_sentries[ i ] != self )
			remaining[ remaining.size ] = level.zm_sentries[ i ];
	}
	level.zm_sentries = remaining;

	self stopFiring();
	self setMode( "sentry_offline" );

	{}

	// Intermission cleanup is quiet; no explosion over the outcome cue.
	if ( reason == "end_game" )
	{
		self notify( "turret_deactivated" );
		self delete();
		return;
	}

	playSoundAtPosition( "zmb_turret_down", self.origin );
	playFx( level._effect[ "zm_sentry_explode" ], self.origin );
	self setModel( level.zm_sentry_model_destroyed );

	self thread auto_turret_remove();

	// Last: auto_turret_activate, which calls this on timeout, ends on it.
	self notify( "turret_deactivated" );
}

auto_turret_remove()
{
	wait( 2 );
	if ( isDefined( self ) )
		self delete();
}

// Reports a turret that disappears without being deactivated.
auto_turret_lifetime_watch( turret, num )
{
	while ( isDefined( turret ) && is_true( turret.turret_active ) )
		wait( 1 );

	if ( !isDefined( turret ) )
		{}
}

// set scr_zm_powerup_test "sentrydiag <client> x": what the native
// acquisition is looking at, for the handback in the header.
auto_turret_diag( target )
{}

// T5 :250.
auto_turret_update_timeout()
{
	self endon( "turret_deactivated" );
	self endon( "death" );

	while ( self.curr_time > 0 )
	{
		wait( 1 );
		self.curr_time--;
	}
}

// The shot loop.  isFiringTurret() is the native fire state (not "none"
// once the gun has converged on an acquired target), and turretstatechange
// is raised whenever that answer flips.
auto_turret_fire()
{
	self endon( "turret_deactivated" );
	self endon( "death" );

	fire_time = weaponFireTime( level.zm_sentry_weapon );
	if ( !isDefined( fire_time ) || fire_time <= 0 )
		fire_time = 0.05;

	for ( ;; )
	{
		if ( self isFiringTurret() )
		{
			self shootTurret();
			if ( !isDefined( self.zm_shots ) )
				self.zm_shots = 0;
			self.zm_shots++;
			wait( fire_time );
			continue;
		}

		self waittill( "turretstatechange" );
	}
}

// Evidence, and the kill count the deactivation line reports: which zombie
// the native acquisition is tracking, and whether the previous one died.
auto_turret_watch_targets()
{
	self endon( "turret_deactivated" );
	self endon( "death" );

	last = undefined;
	logged = 0;

	for ( ;; )
	{
		wait( 0.1 );

		target = self getTurretTarget( false );

		if ( isDefined( last ) && !isAlive( last ) )
		{
			self.zm_kills++;
			if ( logged < 12 )
			{
				logged++;
				{}
			}
			last = undefined;
		}

		if ( !isDefined( target ) )
			continue;

		// A corpse the native tracker has not dropped yet is not a new target.
		if ( !isAlive( target ) )
			continue;

		if ( isDefined( last ) && target == last )
			continue;

		last = target;
		if ( logged < 12 )
		{
			logged++;
			{}
		}
	}
}
