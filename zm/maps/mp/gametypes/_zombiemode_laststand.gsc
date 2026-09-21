// zm downs, bleedout and revive.
//
// Port of T5's maps\_laststand.gsc (1,028 lines) plus the zombies-specific
// overrides in maps\_zombiemode.gsc:2140 player_laststand and :4818
// wait_and_revive.
//
// TAKEN, by T5 function (maps\_laststand.gsc unless noted):
//   player_is_in_laststand        :45
//   player_num_in_laststand       :51
//   player_all_players_in_laststand :68
//   PlayerLastStand               :80    -- the downs counter, the health-to-1
//                                           clamp, the revive trigger, the
//                                           weapon strip, the pistol, the
//                                           ignoreme flag and the bleedout.
//   laststand_disable_player_weapons :201
//   laststand_enable_player_weapons  :271
//   laststand_give_pistol         :326
//   Laststand_Bleedout            :366
//   revive_trigger_spawn          :431
//   revive_trigger_think          :452
//   revive_give_back_weapons      :529
//   can_revive                    :556
//   is_facing                     :627
//   revive_success                :833
//   _zombiemode.gsc player_laststand :2140 -- the points penalty, the offhand
//                                           disable, the perk teardown notify.
//   _zombiemode.gsc checkForAllDead  :1567 -- everyone down ends the game.
//
// ================= WHY THIS IS A SCRIPT-SIDE DOWN =========================
//
// This server HAS a native last stand.  src/codol/script/codol_script_laststand.c
// documents it in full: the retained damage path enters last stand on a lethal
// hit, sets gclient.lastStand + a 500 ms grace deadline, calls
// CodeCallback_PlayerLastStand, forces ps.pm_type 7, and `laststandrevive()`
// is the only thing that clears it.
//
// But CODOL_LastStand_CanEnter (that file, :100-110) gates entry on
// `state->abilities.masks[0] & 0x00800000` -- C1 ability bit 23,
// modPistolDeath, carried in the playerState EXTENSION.  Nothing in GSC sets
// that bit: setPerk writes the perk table state, not the ability mask, and the
// ability publication path is native and lives under src/, which this
// workstream does not own.  So the engine never routes a zm player into last
// stand on its own.
//
// The down is therefore entered from script: Callback_PlayerDamage vetoes the
// lethal hit and calls player_laststand() here.  Callback_PlayerLastStand is
// still wired to the same function, so if the ability bit is ever published
// natively the engine's own entry lands in exactly this code and nothing here
// has to change.  revive_success calls laststandrevive() unconditionally for
// the same reason: it is a no-op when the native flag was never set, and the
// correct teardown when it was.
//
// HANDBACK FOR THE PARENT (native, one line, not applied here because src/ is
// not this workstream's): publishing ability bit 23 for a zm player at spawn
// would let the ENGINE own the down -- correct pm_type, correct damage grace
// window, correct wire state -- instead of the script approximation below.
//
// FOUR BUILTINS T5 USES HERE THAT THIS SERVER DOES NOT REGISTER (MEASURED
// 2026-09-20): VisionSetLastStand, setClientSysState, depthinwater, and the
// reviveProgressBar's maps\_hud_util::createPrimaryProgressBar chain.  The
// downed vision blend, the client "lsm" state and the revive progress bar are
// therefore absent.  The down is instead signalled by the movement clamp
// below and by the revive trigger's hint string.
//
// THE DOWNED POSE.  T5 gets it from ps.pm_type 7 (the engine's own last-stand
// movement mode).  Script cannot write pm_type, so the clamp here is
// setStance("prone") + setMoveSpeedScale + allowSprint/allowJump false, which
// are registered.  It reads as a crawl rather than a true last stand; it is
// the closest a script can get, and the native handback above is the fix.

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	set_zombie_var( "zombie_bleedout_time", 45 );
	set_zombie_var( "zombie_revive_time", 4 );
	set_zombie_var( "zombie_revive_trigger_radius", 72 );

	// T5 _zombiemode.gsc:653 level.laststandpistol.  The resolved zm starting
	// weapon is the honest choice here: it is the one pistol this client is
	// recorded as having accepted live.
	if ( !isDefined( level.laststandpistol ) )
		level.laststandpistol = level.zombie_start_weapon;
}

// T5 :45.
player_is_in_laststand()
{
	return is_true( self.laststand );
}

// T5 :51.
player_num_in_laststand()
{
	players = getPlayers();
	num = 0;
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] player_is_in_laststand() )
			num++;
	}
	return num;
}

// T5 :68.
player_all_players_in_laststand()
{
	players = getPlayers();
	if ( players.size < 1 )
		return false;

	for ( i = 0; i < players.size; i++ )
	{
		if ( !players[ i ] player_is_in_laststand() && isAlive( players[ i ] ) )
			return false;
	}
	return true;
}

// T5 :80 PlayerLastStand + _zombiemode.gsc:2140 player_laststand.
player_laststand()
{
	if ( self player_is_in_laststand() )
		return;

	if ( !isDefined( self.downs ) )
		self.downs = 0;
	self.downs++;

	self.laststand = true;
	self.health = 1;

	// T5 :117 ends the game the instant the last player up goes down, because
	// nobody is left to revive.  That is NOT done here, deliberately: T5's
	// solo rules live in a module this port does not take (T5 routes solo
	// through maps\_zombiemode_solo and zombiemode_solo_last_stand_pistol,
	// _zombiemode.gsc:2728), and with only that check a one-player match would
	// end on the first down, before the quick-revive life could ever be spent.
	// The wipe is resolved by the bleedout instead: it burns a life if there
	// is one, otherwise laststand_die() kills the player and
	// checkForAllDead() ends the game from Callback_PlayerKilled.

	// _zombiemode.gsc:2172 -- the downed player pays penalty_downed.
	self maps\mp\gametypes\_zombiemode_score::player_downed_penalty();

	// _zombiemode.gsc:2176.
	self disableOffhandWeapons();

	self allowSprint( false );
	self allowJump( false );
	self allowAds( false );
	self setMoveSpeedScale( 0.15 );
	self setStance( "prone" );

	// T5 :149 -- the AI stops caring about a downed player.
	self.ignoreme = true;

	self laststand_disable_player_weapons();
	self laststand_give_pistol();
	self revive_trigger_spawn();

	self thread laststand_bleedout( level.zombie_vars[ "zombie_bleedout_time" ] );

	// Every perk_think and every drink is waiting on this.
	self notify( "player_downed" );
}

// T5 :201.  The downed player keeps nothing but the last-stand pistol.
laststand_disable_player_weapons()
{
	self.laststand_weapons = [];
	self.laststand_weapon_current = self getCurrentWeapon();

	weapons = self getWeaponsListPrimaries();
	for ( i = 0; i < weapons.size; i++ )
	{
		self.laststand_weapons[ self.laststand_weapons.size ] = weapons[ i ];
		self takeWeapon( weapons[ i ] );
	}
}

// T5 :271 / :529 revive_give_back_weapons.
laststand_enable_player_weapons()
{
	if ( isDefined( level.laststandpistol ) && self hasWeapon( level.laststandpistol ) )
	{
		// Only take it back when it was not the player's own gun to begin
		// with; T5 makes the same distinction at :539.
		keep = false;
		if ( isDefined( self.laststand_weapons ) )
		{
			for ( i = 0; i < self.laststand_weapons.size; i++ )
			{
				if ( self.laststand_weapons[ i ] == level.laststandpistol )
					keep = true;
			}
		}

		if ( !keep )
			self takeWeapon( level.laststandpistol );
	}

	if ( isDefined( self.laststand_weapons ) )
	{
		for ( i = 0; i < self.laststand_weapons.size; i++ )
		{
			self giveWeapon( self.laststand_weapons[ i ] );
			self giveMaxAmmo( self.laststand_weapons[ i ] );
		}
	}

	if ( isDefined( self.laststand_weapon_current ) && self.laststand_weapon_current != "none" &&
		self hasWeapon( self.laststand_weapon_current ) )
	{
		self switchToWeapon( self.laststand_weapon_current );
	}

	self.laststand_weapons = undefined;
	self.laststand_weapon_current = undefined;
}

// T5 :326.
laststand_give_pistol()
{
	if ( !isDefined( level.laststandpistol ) )
		return;

	if ( !self hasWeapon( level.laststandpistol ) )
		self giveWeapon( level.laststandpistol );

	self giveMaxAmmo( level.laststandpistol );
	self switchToWeapon( level.laststandpistol );
}

// T5 :366.
laststand_bleedout( delay )
{
	self endon( "player_revived" );
	self endon( "disconnect" );

	self.bleedout_time = delay;

	while ( self.bleedout_time > 0 )
	{
		self.bleedout_time -= 1;
		wait( 1 );

		// T5 :404 -- do not bleed out mid-revive.
		while ( isDefined( self.revivetrigger ) && is_true( self.revivetrigger.beingRevived ) )
			wait( 0.1 );
	}

	self notify( "bled_out" );

	// T5 hands the bled-out player to level.player_becomes_zombie on a zombies
	// level.  The playable-zombie mode (_zombiemode.gsc:4187 zombify_player)
	// is not ported -- it is animation-first -- so the solo-life rule is used
	// instead: quick revive's extra life self-revives, otherwise the player
	// dies and the game-over check runs.
	if ( isDefined( self.pers ) && isDefined( self.pers[ "lives" ] ) && self.pers[ "lives" ] > 0 )
	{
		self.pers[ "lives" ]--;
		self revive_success( undefined );
		return;
	}

	self laststand_die();
}

laststand_die()
{
	self.laststand = false;

	if ( isDefined( self.revivetrigger ) )
	{
		self.revivetrigger delete();
		self.revivetrigger = undefined;
	}

	// The other players are penalised for not reviving.  T5
	// _zombiemode_score.gsc:412.
	self maps\mp\gametypes\_zombiemode_score::player_died_penalty();

	self.ignoreme = false;
	self suicide();

	level notify( "player_bled_out", self );
}

// T5 :431.
revive_trigger_spawn()
{
	radius = level.zombie_vars[ "zombie_revive_trigger_radius" ];

	self.revivetrigger = spawn( "trigger_radius", self.origin, 0, radius, radius );
	self.revivetrigger setHintString( "" );
	self.revivetrigger setCursorHint( "HINT_NOICON" );
	self.revivetrigger enableLinkTo();
	self.revivetrigger linkTo( self );
	self.revivetrigger.beingRevived = 0;

	self thread revive_trigger_think();
	self thread revive_trigger_cleanup();
}

revive_trigger_cleanup()
{
	self waittill_either( "player_revived", "disconnect" );

	if ( isDefined( self.revivetrigger ) )
	{
		self.revivetrigger delete();
		self.revivetrigger = undefined;
	}
}

// T5 :452.  self is the downed player.
revive_trigger_think()
{
	self endon( "disconnect" );
	self endon( "player_revived" );
	self endon( "bled_out" );

	revive_time = level.zombie_vars[ "zombie_revive_time" ];

	for ( ;; )
	{
		wait( 0.1 );

		if ( !isDefined( self.revivetrigger ) )
			return;

		players = getPlayers();

		hint = "";
		for ( i = 0; i < players.size; i++ )
		{
			if ( players[ i ] can_revive( self ) )
			{
				hint = "Hold ^3[{+activate}]^7 to Revive";
				break;
			}
		}
		self.revivetrigger setHintString( hint );

		for ( i = 0; i < players.size; i++ )
		{
			reviver = players[ i ];

			if ( !reviver can_revive( self ) )
				continue;

			if ( !reviver useButtonPressed() )
				continue;

			// Hold for revive_time; any break cancels.
			self.revivetrigger.beingRevived = 1;
			elapsed = 0;
			cancelled = false;

			while ( elapsed < revive_time )
			{
				wait( 0.1 );
				elapsed += 0.1;

				if ( !reviver can_revive( self ) || !reviver useButtonPressed() )
				{
					cancelled = true;
					break;
				}
			}

			self.revivetrigger.beingRevived = 0;

			if ( cancelled )
				continue;

			self revive_success( reviver );
			return;
		}
	}
}

// T5 :556 can_revive + :627 is_facing.  revivee is the downed player.
can_revive( revivee )
{
	if ( !isDefined( revivee ) || !isDefined( revivee.revivetrigger ) )
		return false;

	if ( self == revivee )
		return false;

	if ( !is_player_valid( self ) )
		return false;

	if ( !self isTouching( revivee.revivetrigger ) )
		return false;

	return self is_facing( revivee );
}

// T5 :627.
is_facing( facee )
{
	forward = anglesToForward( self.angles );
	to_facee = facee.origin - self.origin;

	forward = vectorNormalize( ( forward[ 0 ], forward[ 1 ], 0 ) );
	to_facee = vectorNormalize( ( to_facee[ 0 ], to_facee[ 1 ], 0 ) );

	dot = vectorDot( forward, to_facee );
	return dot > 0.4;
}

// T5 :833.  reviver is undefined for a self-revive (solo quick revive).
revive_success( reviver )
{
	self.laststand = false;
	self.ignoreme = false;

	// See the header: a no-op when the native flag was never latched, the
	// correct teardown when it was.
	self laststandrevive();

	self.health = self.maxhealth;

	self allowSprint( true );
	self allowJump( true );
	self allowAds( true );
	self setMoveSpeedScale( 1 );
	self setStance( "stand" );
	self enableOffhandWeapons();

	self laststand_enable_player_weapons();

	if ( isDefined( self.revivetrigger ) )
	{
		self.revivetrigger delete();
		self.revivetrigger = undefined;
	}

	if ( isDefined( reviver ) )
	{
		// T5 _zombiemode_score.gsc "reviver" event.
		reviver maps\mp\gametypes\_zombiemode_score::player_add_points( "reviver", 100 );
		if ( !isDefined( reviver.revives ) )
			reviver.revives = 0;
		reviver.revives++;
	}

	self notify( "player_revived" );
}

// T5 _zombiemode.gsc:1567 checkForAllDead, called from the kill callback.
checkForAllDead()
{
	players = getPlayers();
	if ( players.size < 1 )
		return;

	for ( i = 0; i < players.size; i++ )
	{
		if ( isAlive( players[ i ] ) && !players[ i ] player_is_in_laststand() )
			return;
	}

	level notify( "end_game" );
}
