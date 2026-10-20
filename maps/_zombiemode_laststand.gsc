// zm downs, bleedout and revive: the zombies configuration of the shared
// common_scripts/_laststand.gsc.
//
// The machine itself (the down, the revive trigger and hold, the bleed-out,
// the prompts and the HUD) is the shared module, which is this file's former
// body -- the port of T5 maps\_laststand.gsc -- with the zombies-only calls
// moved behind hooks.  T5 is built the same way: maps\_laststand.gsc serves
// both campaign co-op and Zombies, and maps\_zombiemode.gsc overrides it with
//   :2140 player_laststand      -> the points penalty, the weapon power-up's
//                                  release                       ("downed")
//   :2025 player_revive_monitor -> the reviver is paid back what the downed
//                                  player lost                   ("revived")
//   :4818 wait_and_revive       -> the solo Quick Revive life    ("spare_life",
//                                  "use_spare")
//   :4700                       -> everyone down ends the game   ("all_down")
//   :4176 level.can_revive      -> not with a power-up weapon or
//                                  a perk bottle in hand         ("can_revive")
// which are exactly the hooks installed below.  Every other zm script keeps
// calling this file's public names, which forward to the shared module.
//
// THE DOWN ITSELF IS THE ENGINE'S.  C1 ability bit 23 (modPistolDeath) is
// published for every zm player (src/codol/player/codol_perk_runtime.c), so a
// lethal hit enters the native last stand and Callback_PlayerLastStand ->
// player_laststand() here.
//
// C1 DATA OVER T5 NAMES: SCRIPT_COOP_REVIVE / _REVIVING / _BLEEDING_OUT /
// _BLEEDING_OUT_PARTNER (localized_common_gametype_zm, and the same strings in
// localized_common_gametype_coop), MPUI_BEING_REVIVED and ZOMBIE_REVIVING_SOLO
// (localized_code_post_gfx_mp), waypoint_revive and the progress bars
// (common_mp); the reviver's voice is <character>_whilerevivingteammate
// (localized_common_gametype_zm; lb / danby / hh / lg, column 1 of
// mp/cyborgclasstable.csv).

#include maps\_zombiemode_utility;

ROSTER_TABLE = "mp/cyborgclasstable.csv";

init()
{
	set_zombie_var( "zombie_bleedout_time", 45 );
	set_zombie_var( "zombie_solo_revive_time", 10 );
	set_zombie_var( "zombie_revive_time", 3 );
	set_zombie_var( "zombie_revive_trigger_radius", 75 );

	level.laststand_vars = [];
	level.laststand_vars[ "bleedout_time" ] = level.zombie_vars[ "zombie_bleedout_time" ];
	level.laststand_vars[ "solo_revive_time" ] = level.zombie_vars[ "zombie_solo_revive_time" ];
	level.laststand_vars[ "revive_time" ] = level.zombie_vars[ "zombie_revive_time" ];
	level.laststand_vars[ "trigger_radius" ] = level.zombie_vars[ "zombie_revive_trigger_radius" ];
	level.laststand_vars[ "log" ] = "CoDOL zm laststand";

	level.laststand_funcs = [];
	level.laststand_funcs[ "downed" ] = ::zm_downed;
	level.laststand_funcs[ "downed_post" ] = ::zm_downed_post;
	level.laststand_funcs[ "bled_out" ] = ::zm_bled_out;
	level.laststand_funcs[ "revived" ] = ::zm_revived;
	level.laststand_funcs[ "revive_done" ] = ::zm_revive_done;
	level.laststand_funcs[ "can_revive" ] = ::zm_can_revive;
	level.laststand_funcs[ "revive_time" ] = ::zm_revive_time;
	level.laststand_funcs[ "vox_code" ] = ::revive_vox_code;
	level.laststand_funcs[ "spare_life" ] = ::zm_spare_life;
	level.laststand_funcs[ "use_spare" ] = ::zm_use_spare;
	level.laststand_funcs[ "all_down" ] = ::zm_all_down;
	level.laststand_funcs[ "down_window" ] = maps\_zombiemode_tokens::begin_down;
	level.laststand_funcs[ "down_end" ] = maps\_zombiemode_tokens::end_down;
	level.zm_solo_retreat_active = false;
	maps\_zombiemode_tokens::init();
	maps\_zombiemode_revive_protection::init();

	// T5 _zombiemode.gsc:653 level.laststandpistol.  The resolved zm starting
	// weapon is the honest choice here: it is the one pistol this client is
	// recorded as having accepted live.
	if ( !isDefined( level.laststandpistol ) )
		level.laststandpistol = level.zombie_start_weapon;

	common_scripts\_laststand::init();
}

// --------------------------------------------------------------------------
// The public names every zm script calls.
// --------------------------------------------------------------------------

player_is_in_laststand()
{
	return self common_scripts\_laststand::player_is_in_laststand();
}

player_num_in_laststand()
{
	return common_scripts\_laststand::player_num_in_laststand();
}

player_all_players_in_laststand()
{
	return common_scripts\_laststand::player_all_players_in_laststand();
}

player_laststand()
{
	self common_scripts\_laststand::player_laststand();
}

revive_success( reviver )
{
	self common_scripts\_laststand::revive_success( reviver );
}

checkForAllDead()
{
	common_scripts\_laststand::checkForAllDead();
}

// --------------------------------------------------------------------------
// The zombies hooks.
// --------------------------------------------------------------------------

// _zombiemode.gsc:2172 -- the downed player pays penalty_downed and remembers
// it in score_lost_when_downed for the reviver.  T5 :2141 calls the weapon
// power-up's last-stand hook before the weapons are stored; its hold
// restrictions go first so the pistol switch is not refused (the power-up
// watcher takes the weapon itself).
zm_downed()
{
	self maps\_zombiemode_revive_protection::clear();
	self maps\_zombiemode_score::player_downed_penalty();
	if ( self has_powerup_weapon() )
		self powerup_weapon_unlock();
	// Recovery needs breathing room when nobody remains a valid standing
	// target. Connected spectators/dead teammates must not suppress it, and
	// a standing teammate must retain normal combat. One level owner avoids
	// competing all-down callbacks clearing each other's actor markers.
	if ( no_standing_retreat_eligible( self ) && !is_true( level.zm_solo_retreat_active ) )
		level thread solo_retreat_think( self );
}

no_standing_retreat_eligible( player )
{
	return isDefined( player ) && is_true( player.laststand ) &&
		!is_true( level.gameEnded ) && get_number_of_valid_players() == 0;
}

// While no standing target remains, send hunting zombies and hounds to a
// map-authored spawner at least 640 units from the body.  The native actor
// perception pass otherwise keeps its old enemy and replaces script goals.
solo_retreat_think( player )
{
	if ( is_true( level.zm_solo_retreat_active ) || !no_standing_retreat_eligible( player ) )
		return;
	level.zm_solo_retreat_active = true;
	points = level.enemy_spawns;
	if ( !isDefined( points ) )
		points = [];
	best = undefined;
	best_dist = 640 * 640;
	for ( i = 0; i < points.size; i++ )
	{
		d = distanceSquared( points[ i ].origin, player.origin );
		if ( d > best_dist && findpath( player.origin, points[ i ].origin ) )
		{
			best = points[ i ];
			best_dist = d;
		}
	}
	// The open starting zone can have only nearby spawners.  If so, use a
	// farther authored point from another zone instead of keeping the horde
	// beside the downed player.
	if ( !isDefined( best ) )
	{
		points = get_struct_array( "zombie_spawner", "script_noteworthy" );
		if ( isDefined( points ) )
		{
			for ( i = 0; i < points.size; i++ )
			{
				d = distanceSquared( points[ i ].origin, player.origin );
				if ( d > best_dist && findpath( player.origin, points[ i ].origin ) )
				{
					best = points[ i ];
					best_dist = d;
				}
			}
		}
	}
	// Closed doors/windows can disconnect every distant spawn. Fall back to
	// the farthest reachable authored navigation point in the open area.
	// Spread the search over frames rather than stalling the down callback.
	if ( !isDefined( best ) )
	{
		points = codol_getwalkablewaypoints();
		best_dist = 128 * 128;
		for ( i = 0; i < points.size; i++ )
		{
			if ( i % 32 == 0 )
			{
				wait( 0.05 );
				if ( !no_standing_retreat_eligible( player ) )
				{
					level.zm_solo_retreat_active = false;
					return;
				}
			}
			d = distanceSquared( points[ i ], player.origin );
			if ( d > best_dist && findpath( player.origin, points[ i ] ) )
			{
				best = spawnstruct();
				best.origin = points[ i ];
				best_dist = d;
			}
		}
	}
	if ( !isDefined( best ) )
	{
		{}
		level.zm_solo_retreat_active = false;
		return;
	}

	goal = best.origin;
	{}
	while ( no_standing_retreat_eligible( player ) )
	{
		for ( i = 0; i < level.zombies.size; i++ )
		{
			z = level.zombies[ i ];
			if ( !isDefined( z ) || !isAlive( z ) || is_true( z.zm_solo_retreat ) )
				continue;
			// An actor at a boarded entrance owns that route until it starts
			// hunting.  Those actors are already away from the player.
			if ( !is_true( z.isdog ) && ( !isDefined( z.zm_path_phase ) || z.zm_path_phase != "hunt" ) )
				continue;
			z.zm_solo_retreat = true;
			z.ignoreall = true;
			z.goalradius = 96;
			z setGoalPos( goal );
		}
		wait( 0.1 );
	}

	for ( i = 0; i < level.zombies.size; i++ )
	{
		z = level.zombies[ i ];
		if ( !isDefined( z ) || !isAlive( z ) || !is_true( z.zm_solo_retreat ) )
			continue;
		z.zm_solo_retreat = false;
		z.ignoreall = false;
	}
	level.zm_solo_retreat_active = false;
}

zm_downed_post()
{
	level notify( "zm_cortex_event", "death" );   // _zombiemode_cortex reaction
}

// The other players are penalised for not reviving.  T5
// _zombiemode_score.gsc:222, from zombify_player.
zm_bled_out()
{
	self maps\_zombiemode_score::player_died_penalty();
}

// player_revive_monitor: the reviver gets back what the downed player lost
// going down.
zm_revived( reviver )
{
	points = 0;
	if ( isDefined( self.score_lost_when_downed ) )
		points = self.score_lost_when_downed;
	reviver maps\_zombiemode_score::player_add_points( "reviver", points );
	self.score_lost_when_downed = 0;
	{}
}

// The health regen thread ends on "player_downed"; a revived player
// regenerates again, as in T5.
zm_revive_done()
{
	self maps\_zombiemode_revive_protection::start_if_token();
	self.zmLastDamageTime = getTime();
	self notify( "zm_regen_restart" );
	self thread maps\_zombiemode::zm_player_health_regen();
}

// T5 _zombiemode.gsc:4176 (level.can_revive): not with a power-up weapon,
// and not with the perk bottle in his hands (T5 makes the same exclusion
// through the syrette swap, :494).
zm_can_revive( revivee )
{
	if ( self has_powerup_weapon() )
		return false;
	if ( is_true( self.is_drinking ) )
		return false;
	return true;
}

// T5 :643 -- halved by Quick Revive.
zm_revive_time( base )
{
	if ( self hasPerk( "specialty_quickrevive" ) )
		return base / 2;
	return base;
}

// The solo Quick Revive life (_zombiemode.gsc:4818 wait_and_revive).
zm_spare_life()
{
	return isDefined( self.pers ) && isDefined( self.pers[ "lives" ] ) && self.pers[ "lives" ] > 0;
}

zm_use_spare()
{
	self.pers[ "lives" ]--;
	{}
}

// C1 January 2014 footage: all-down recovery has a fifteen-second ending
// countdown in the existing death POV. Token inventory/receipt authority and
// the three-use cap remain owned by _zombiemode_tokens and the lobby.
zm_all_down()
{
	if ( level.gameEnded || is_true( level.zm_all_down_active ) )
		return;
	level.zm_all_down_active = true;
	level thread all_down_countdown();
}

all_down_recovered()
{
	players = getPlayers();
	if ( players.size == 0 )
		return true;
	for ( i = 0; i < players.size; i++ )
		if ( isAlive( players[ i ] ) && !players[ i ] common_scripts\_laststand::player_is_in_laststand() )
			return true;
	return false;
}

clear_all_down_countdown()
{
	if ( isDefined( level.zm_all_down_timer ) )
		level.zm_all_down_timer destroy();
	level.zm_all_down_timer = undefined;
	level.zm_all_down_active = false;
}

all_down_countdown()
{
	timer = newHudElem();
	timer.alignX = "center";
	timer.alignY = "middle";
	timer.horzAlign = "center";
	timer.vertAlign = "middle";
	timer.y = -100;
	timer.font = "objective";
	timer.fontScale = 1.6;
	timer.foreground = true;
	// Same native inline localized-label + timer representation used by the
	// extraction countdown; no separate second text row.
	timer.label = &"ZOMBIE_GAME_OVER_IN";
	timer setTimer( 15 );
	level.zm_all_down_timer = timer;
	deadline = getTime() + 15000;
	last_second = 15;
	tick_available = maps\_zombiemode_audio::alias_exists( "ui_mp_timer_countdown" );
	while ( !level.gameEnded && !all_down_recovered() && getTime() < deadline )
	{
		// Tick only when a displayed positive second has elapsed (14..1).
		// No initial15 or final0 tick, and no catch-up burst after a stall.
		second = int( ( deadline - getTime() + 999 ) / 1000 );
		if ( second < last_second && second > 0 )
		{
			last_second = second;
			if ( tick_available )
				foreach ( player in getPlayers() )
					player playLocalSound( "ui_mp_timer_countdown" );
		}
		wait( 0.05 );
	}
	// Re-check at expiry: an authenticated revive completed on this frame wins
	// over ending the match. No token request/receipt is synthesized here.
	ended = !level.gameEnded && !all_down_recovered();
	clear_all_down_countdown();
	if ( ended )
		level notify( "end_game" );
}

// The character code (column 1 of the roster, lb / danby / hh / lg) of this
// player's zm character, which _zombiemode_equipment keys by column 0.
revive_vox_code()
{
	if ( !isDefined( self.zombie_character ) )
		return undefined;

	for ( row = 0; row < 16; row++ )
	{
		key = maps\_zombiemode_equipment::trim( tableLookupByRow( ROSTER_TABLE, row, 0 ) );
		if ( key == "" )
			break;
		if ( key != self.zombie_character )
			continue;
		code = maps\_zombiemode_equipment::trim( tableLookupByRow( ROSTER_TABLE, row, 1 ) );
		if ( code == "" )
			return undefined;
		return code;
	}
	return undefined;
}
