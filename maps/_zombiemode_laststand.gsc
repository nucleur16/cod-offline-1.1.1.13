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
	self maps\_zombiemode_score::player_downed_penalty();
	if ( self has_powerup_weapon() )
		self powerup_weapon_unlock();
	if ( getPlayers().size == 1 && self.pers[ "lives" ] > 0 )
		level thread solo_retreat_think( self );
}

// While the solo player self-revives, send hunting zombies and hounds to a
// map-authored spawner at least 640 units from the body.  The native actor
// perception pass otherwise keeps its old enemy and replaces script goals.
solo_retreat_think( player )
{
	points = level.enemy_spawns;
	if ( !isDefined( points ) )
		points = [];
	best = undefined;
	best_dist = 640 * 640;
	for ( i = 0; i < points.size; i++ )
	{
		d = distanceSquared( points[ i ].origin, player.origin );
		if ( d > best_dist )
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
				if ( d > best_dist )
				{
					best = points[ i ];
					best_dist = d;
				}
			}
		}
	}
	if ( !isDefined( best ) )
	{
		codolprintln( "CoDOL zm: no distant solo retreat spawner" );
		return;
	}

	goal = best.origin;
	codolprintln( "CoDOL zm: solo retreat to " + goal + " from " + player.origin );
	while ( isDefined( player ) && is_true( player.laststand ) && getPlayers().size == 1 && !level.gameEnded )
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
	codolprintln( "CoDOL zm laststand: " + reviver.name + " paid +" + points + " for reviving " + self.name );
}

// The health regen thread ends on "player_downed"; a revived player
// regenerates again, as in T5.
zm_revive_done()
{
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
	codolprintln( "CoDOL zm laststand: " + self.name + " solo self revive, " + self.pers[ "lives" ] + " spare lives left" );
}

// _zombiemode.gsc:4700: nobody left standing ends the game at once.
zm_all_down()
{
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
