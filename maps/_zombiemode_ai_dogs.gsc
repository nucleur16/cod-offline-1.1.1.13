// zm hellhound rounds.
//
// Port of T5 maps\_zombiemode_ai_dogs.gsc (D:\CoD\COD-GSC-Source\T5-STEAM-GSC):
// init :7, dog_round_tracker :444, dog_round_start :489, dog_round_stop :519,
// dog_round_spawning :117, waiting_for_next_dog_spawn :216, dog_round_aftermath
// :248, dog_spawn_fx :273, dog_spawn_factory_logic :349, get_favorite_enemy
// :369, dog_health_increase :400, play_dog_round :540, dog_init :556 and
// dog_death :656.
//
// WHAT C1 SAYS ABOUT HELLHOUNDS (all MEASURED from the shipped zones):
//   - CoDOL had them.  localized_code_post_gfx_mp PLATFORM_DYK_ZM_MSG10 is the
//     loading-screen tip "the last hound of a mechanical-hound round always
//     drops a Max Ammo" (T5's dog_round_aftermath), and every zm map zone
//     plus localized_code_post_gfx_mp carries ZOMBIE_WARNING_DOG_ARRIVED
//     ("Warning: mechanical hounds incoming").
//   - The actor: zombie_wolf (the three zm map zones, and the maps' own
//     actor_zombie_dog spawners), animtrees/zombie_dog.atr plus 60
//     zombie_dog_* xanims (common_gametype_zm), dog_bite_zm (map zones).
//     See aitype/enemy_zombie_dog_zm.gsc and the "zombie_dog" species row in
//     src/codol/player/codol_actor_scripts.cpp.
//   - FX, common_gametype_zm: maps/zombie/fx_zombie_dog_lightning_buildup,
//     _lightning_spawn, _eyes, _explosion, _fire_trail, _ash_trail.
//   - Sound, common_gametype_zm: zmb_hellhound_prespawn / _bolt / _spawn /
//     _explode / _loop_breath / _loop_fire / _vocals_*, the announcer's
//     zmb_vox_ann_dogstart, and mus_zombie_dog_start / mus_zombie_dog_end.
//   - Spawn points: every zm map authors T5's per-zone hound structs,
//     targetname "<zone volume target>_dog" (start_zone_spawners_dog,
//     west_downstairs_zone_spawners_dog, ...): T5 Factory's
//     level.enemy_dog_locations, read here from the zone manager's zones.
//   - The round HUD: C1's zombiemodescores menu draws @MPUI_SPECIAL_ROUND
//     ("Special round") while dvar ui_hud_special_round is 1 (see
//     _zombiemode.gsc updateRoundHud).
//
// Finite Easy/Medium/Hard matches have hounds on round 6, then 10 and 14
// as their length permits: exactly 1/2/3 hound rounds.  This leaves round 5
// available to the map's first boss.  Endless mode keeps
// T5's randomized first round 5..7 and 4..5-round intervals.  The extraction
// round itself is never a hound round.
//
// Developer verbs (developer servers only): scr_zm_force_dogs 1 makes the
// NEXT round a hound round (T5's force_dogs); scr_zm_dog_count overrides the
// hound count of a hound round.

#include maps\_zombiemode_utility;

init()
{
	level.dogs_enabled = true;
	level.dog_round_count = 1;
	level.dog_rounds_played = 0;
	level.dog_health = 100;
	level.enemy_dog_locations = [];
	level.dog_intermission = false;

	flag_init( "dog_round" );
	flag_init( "dog_clips" );

	// T5 keeps the hound rounds off unless the level enables them; every
	// CoDOL zm map authors hound structs, so they are on unless a server
	// turns them off.
	level.dog_rounds_enabled = true;
	if ( getDvar( "scr_zm_dog_rounds" ) != "" && getDvarInt( "scr_zm_dog_rounds" ) == 0 )
		level.dog_rounds_enabled = false;

	// T5 :18.
	precacheRumble( "explosion_generic" );

	// T5 :25-33.  scr_dog_run_distance is read by zombie_dog_move's
	// need_to_run (the stalking trot turns into the sprint inside it).
	dog_dvar_default( "scr_dog_health_walk_multiplier", "4.0" );
	dog_dvar_default( "scr_dog_run_distance", "500" );

	// T5 :43-47: the build-up effect is the whole strike (T5 loads no other
	// spawn effect; fx_zombie_dog_lightning_spawn ships in C1 but no T5
	// script plays it).
	level._effect[ "lightning_dog_spawn" ]	= loadfx( "maps/zombie/fx_zombie_dog_lightning_buildup" );
	level._effect[ "dog_eye_glow" ]			= loadfx( "maps/zombie/fx_zombie_dog_eyes" );
	level._effect[ "dog_gib" ]				= loadfx( "maps/zombie/fx_zombie_dog_explosion" );
	level._effect[ "dog_trail_fire" ]		= loadfx( "maps/zombie/fx_zombie_dog_fire_trail" );
	level._effect[ "dog_trail_ash" ]		= loadfx( "maps/zombie/fx_zombie_dog_ash_trail" );

	precacheString( &"ZOMBIE_WARNING_DOG_ARRIVED" );

	// T5's code-owned turn thresholds (OpenBLOPS bg_dog.cpp Dog_RegisterDvars),
	// which the ported zombie_dog_turn animscript reads through GetDvarFloat.
	// The multiplayer dog registers the same three in maps\mp\_dogs.
	dog_dvar_default( "dog_turn90_angle", "70" );
	dog_dvar_default( "dog_turn180_angle", "135" );
	dog_dvar_default( "dog_turn_min_goal_dist", "40" );

	// T5 set_zombie_var( "dog_fire_trail_percent", 50 ).
	level.dog_fire_trail_percent = 50;

	dog_spawner_init();

	// T5 :55.
	level thread dog_clip_monitor();

	if ( getDvarInt( "developer" ) > 0 )
	{
		setDvar( "scr_zm_force_dogs", "0" );
		if ( getDvar( "scr_zm_dog_count" ) == "" )
			setDvar( "scr_zm_dog_count", "0" );
	}

	if ( level.dog_rounds_enabled && level.dogs_enabled )
		level thread dog_round_tracker();
}

dog_dvar_default( name, value )
{
	if ( getDvar( name ) == "" )
		setDvar( name, value );
}

// The template every hound is duplicated from.  The same two-step as the
// humanoid zombie's (_zombiemode_spawner::init_spawner): the maps' own
// actor_zombie_dog entities are never instantiated as actor spawners, so the
// template is built from the aitype.
dog_spawner_init()
{
	level.dog_spawner = spawnActorSpawner( "actor_enemy_zombie_dog_zm", ( 0, 0, 0 ) );
	if ( !isDefined( level.dog_spawner ) )
	{
		// The native side has printed a "CoDOL actor:" line naming the cause.
		codolprintln( "CoDOL zm dogs: spawnactorspawner produced no hellhound spawner; hound rounds are off" );
		level.dogs_enabled = false;
		return;
	}
	level.dog_spawner.targetname = "zombie_dog_spawner";
	codolprintln( "CoDOL zm dogs: hellhound spawner ready" );
}

// The hound structs of the zones a player can reach (T5 Factory
// dog_spawn_factory_logic over level.enemy_dog_locations, which T5's zone
// manager fills from the active zones).
refresh_dog_locations()
{
	locations = [];
	if ( isDefined( level.zm_spawn_zones ) )
	{
		for ( i = 0; i < level.zm_spawn_zones.size; i++ )
		{
			zone = level.zm_spawn_zones[ i ];
			if ( !zone.enabled )
				continue;
			if ( !isDefined( zone.dog_locations ) )
				zone.dog_locations = zone_dog_locations( zone );
			for ( j = 0; j < zone.dog_locations.size; j++ )
				locations[ locations.size ] = zone.dog_locations[ j ];
		}
	}

	// A map whose volumes did not instantiate: every hound struct with a floor.
	if ( locations.size < 1 )
	{
		if ( !isDefined( level.zm_all_dog_locations ) )
			level.zm_all_dog_locations = all_dog_locations();
		locations = level.zm_all_dog_locations;
	}

	level.enemy_dog_locations = locations;
	return locations;
}

zone_dog_locations( zone )
{
	kept = [];
	if ( !isDefined( zone.spawner_target ) )
		return kept;
	structs = get_struct_array( zone.spawner_target + "_dog", "targetname" );
	if ( !isDefined( structs ) )
		return kept;
	for ( i = 0; i < structs.size; i++ )
	{
		if ( dog_location_has_floor( structs[ i ] ) )
			kept[ kept.size ] = structs[ i ];
	}
	return kept;
}

all_dog_locations()
{
	kept = [];
	if ( !isDefined( level.zm_spawn_zones ) )
		return kept;
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
	{
		zone = level.zm_spawn_zones[ i ];
		if ( !isDefined( zone.dog_locations ) )
			zone.dog_locations = zone_dog_locations( zone );
		for ( j = 0; j < zone.dog_locations.size; j++ )
			kept[ kept.size ] = zone.dog_locations[ j ];
	}
	return kept;
}

// zm_zombie_training carries Prototype's hound structs at Prototype's
// coordinates; the ones with nothing under them are never used (the zone
// manager does the same for the zombie spawners).
dog_location_has_floor( loc )
{
	trace = bulletTrace( loc.origin + ( 0, 0, 16 ), loc.origin - ( 0, 0, 128 ), false, undefined );
	return trace[ "fraction" ] < 1;
}

// --------------------------------------------------------------------------
// T5 :444 dog_round_tracker, driven by _zombiemode::round_think's
// "between_round_over" (sent after round_number has advanced).
// --------------------------------------------------------------------------

dog_round_tracker()
{
	level endon( "end_game" );

	level.dog_round_count = 1;

	if ( level.zm_round_cap > 0 )
	{
		level.next_dog_round = 0;
		if ( level.zm_round_cap > 6 && level.zm_hound_rounds_goal > 0 )
			level.next_dog_round = 6;
	}
	else
		level.next_dog_round = randomIntRange( 5, 8 );
	codolprintln( "CoDOL zm dogs: first hound round is round " + level.next_dog_round );

	old_spawn_func = level.round_spawn_func;
	old_wait_func = level.round_wait_func;

	for ( ;; )
	{
		level waittill( "between_round_over" );

		if ( getDvarInt( "developer" ) > 0 && getDvarInt( "scr_zm_force_dogs" ) > 0 )
		{
			setDvar( "scr_zm_force_dogs", "0" );
			level.next_dog_round = level.round_number;
		}

		// A developer-forced round still cannot replace extraction.
		if ( level.round_number == level.next_dog_round && maps\_zombiemode::is_final_round() )
			level.next_dog_round = 0;

		if ( level.round_number == level.next_dog_round )
		{
			if ( !flag( "dog_round" ) )
			{
				old_spawn_func = level.round_spawn_func;
				old_wait_func = level.round_wait_func;
			}
			level.music_round_override = true;
			dog_round_start();
			level.dog_rounds_played++;
			level.round_spawn_func = ::dog_round_spawning;
			// T5 _zombiemode.gsc:4148 round_wait's hound-round arm, installed
			// through the hook T5's tracker already saves and restores.
			level.round_wait_func = ::dog_round_wait;

			if ( level.zm_round_cap > 0 )
			{
				level.next_dog_round = 0;
				if ( level.dog_rounds_played < level.zm_hound_rounds_goal &&
					level.round_number + 4 < level.zm_round_cap )
					level.next_dog_round = level.round_number + 4;
			}
			else
				level.next_dog_round = level.round_number + randomIntRange( 4, 6 );
			codolprintln( "CoDOL zm dogs: round " + level.round_number + " is hound round " +
				level.dog_round_count + "; the next is round " + level.next_dog_round );
		}
		else if ( flag( "dog_round" ) )
		{
			dog_round_stop();
			level.round_spawn_func = old_spawn_func;
			level.round_wait_func = old_wait_func;
			level.music_round_override = false;
			level.dog_round_count += 1;
		}

		maps\_zombiemode::publishRoundHud();
	}
}

// T5 :489.  T5 also swaps the AI melee dvars for the round: the native
// actor melee (codol_actor_lifecycle.cpp ActorMelee) reads the same
// ai_meleeRange / ai_meleeWidth / ai_meleeHeight, and zombie_dog_stop bites a
// player inside ai_meleeRange (T5 zombie_dog_stop.gsc:86).
dog_round_start()
{
	flag_set( "dog_round" );
	flag_set( "dog_clips" );
	level notify( "dog_round_starting" );

	if ( !isDefined( level.melee_range_sav ) )
	{
		// T5 :35-37 saves them at init; the defaults of the native cvars.
		level.melee_range_sav = getDvar( "ai_meleeRange" );
		level.melee_width_sav = getDvar( "ai_meleeWidth" );
		level.melee_height_sav = getDvar( "ai_meleeHeight" );
	}
	if ( isDefined( level.dog_melee_range ) )
		setDvar( "ai_meleeRange", level.dog_melee_range );
	else
		setDvar( "ai_meleeRange", "100" );
	setDvar( "ai_meleeWidth", "0" );
	setDvar( "ai_meleeHeight", "0" );
}

// T5 :519.
dog_round_stop()
{
	flag_clear( "dog_round" );
	flag_clear( "dog_clips" );
	level notify( "dog_round_ending" );

	// T5 :536-538.
	if ( isDefined( level.melee_range_sav ) )
	{
		setDvar( "ai_meleeRange", level.melee_range_sav );
		setDvar( "ai_meleeWidth", level.melee_width_sav );
		setDvar( "ai_meleeHeight", level.melee_height_sav );
	}
}

// T5 _zombiemode.gsc:4146-4155, round_wait during a hound round: the round
// does not end with the last hound but when dog_round_aftermath releases
// level.dog_intermission, 8 s after the last hound fell -- the time to pick
// up its Max Ammo before the next round's countdown.
dog_round_wait()
{
	level endon( "end_game" );

	wait( 1 );
	wait( 7 );

	// CoDOL: a hound round whose budget was released without a last kill
	// (the hounds could not be placed, or a developer endround) never sends
	// last_dog_down, so the lull is also over once nothing is left for 10 s.
	empty_since = undefined;
	while ( level.dog_intermission )
	{
		if ( flag( "end_round_wait" ) )
		{
			flag_clear( "end_round_wait" );
			level.dog_intermission = false;
			return;
		}

		if ( live_enemy_count() == 0 && level.zombie_total <= 0 && !is_true( level.dog_aftermath_running ) )
		{
			if ( !isDefined( empty_since ) )
				empty_since = getTime();
			else if ( getTime() - empty_since > 10000 )
				level.dog_intermission = false;
		}
		else
			empty_since = undefined;

		wait( 0.5 );
	}
}

// T5 :540 play_dog_round, with C1's assets: the announcer line, and C1's own
// warning text (ZOMBIE_WARNING_DOG_ARRIVED) in place of T5's clientscript
// fog/vision change.  The dog-round music is _zombiemode_audio's
// "dog_start" state, played by its round watcher (level.music_round_override).
play_dog_round()
{
	self endon( "disconnect" );

	// T5 :544 plays zmb_dog_round_start here; no C1 zone ships that alias
	// (or any renamed copy of it), so the announcer is the first sound.
	self thread dog_round_notice();

	wait( 1 );
	self playLocalSound( "zmb_vox_ann_dogstart" );

	// T5 :550-554: a random player calls the hounds out (plr_vox general
	// dog_spawn, "spawn_dog").  C1's character vox sets (<code>_<line>,
	// _zombiemode_audio vox_alias) carry no hound line, so nothing is said.
}

dog_round_notice()
{
	self endon( "disconnect" );

	elem = newClientHudElem( self );
	elem.alignX = "center";
	elem.alignY = "middle";
	elem.horzAlign = "center";
	elem.vertAlign = "top";
	elem.x = 0;
	elem.y = 110;
	elem.font = "objective";
	elem.fontScale = 1.6;
	elem.color = ( 1, 0.35, 0.2 );
	elem.glowColor = ( 0.6, 0.1, 0.0 );
	elem.glowAlpha = 0.6;
	elem.foreground = true;
	elem.hidewheninmenu = true;
	elem.alpha = 0;
	elem setText( &"ZOMBIE_WARNING_DOG_ARRIVED" );

	elem fadeOverTime( 0.5 );
	elem.alpha = 1;
	wait( 5 );
	if ( !isDefined( elem ) )
		return;
	elem fadeOverTime( 1 );
	elem.alpha = 0;
	wait( 1 );
	if ( isDefined( elem ) )
		elem destroy();
}

// --------------------------------------------------------------------------
// T5 :117 dog_round_spawning.  Installed as level.round_spawn_func for the
// hound round; _zombiemode::round_think's round_wait ends the round when the
// budget is spent and every hound is dead, exactly as for zombies.
// --------------------------------------------------------------------------

dog_round_spawning()
{
	level endon( "intermission" );
	level endon( "end_game" );

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ].hunted_by = 0;

	if ( is_true( level.intermission ) )
		return;

	if ( !is_true( level.dogs_enabled ) )
	{
		level.zombie_total = 0;
		codolprintln( "CoDOL zm dogs: hound round " + level.round_number + " has no hellhound spawner; nothing to spawn" );
		return;
	}

	level.dog_intermission = true;
	level thread dog_round_aftermath();
	array_thread_1( players, ::play_dog_round );

	// T5 publishes the hound budget before the 7 s build-up, so round_wait
	// cannot see an empty round in between.
	if ( level.dog_round_count < 3 )
		max = players.size * 6;
	else
		max = players.size * 8;

	if ( getDvarInt( "developer" ) > 0 && getDvarInt( "scr_zm_dog_count" ) > 0 )
		max = getDvarInt( "scr_zm_dog_count" );

	level.zombie_total = max;
	level.zm_round_budget = max;
	level.zm_round_spawned = 0;
	dog_health_increase();

	codolprintln( "CoDOL zm dogs: hound round " + level.dog_round_count + " (round " + level.round_number +
		"): " + max + " hellhounds at " + level.dog_health + " health" );

	wait( 7 );

	count = 0;
	failures = 0;
	while ( count < max )
	{
		num_player_valid = get_number_of_valid_players();
		if ( num_player_valid < 1 )
			num_player_valid = 1;

		while ( get_enemy_count() >= num_player_valid * 2 )
		{
			wait( 2 );
			num_player_valid = get_number_of_valid_players();
			if ( num_player_valid < 1 )
				num_player_valid = 1;
		}

		favorite_enemy = get_favorite_enemy();
		if ( !isDefined( favorite_enemy ) )
		{
			wait( 1 );
			continue;
		}

		spawn_loc = dog_spawn_location( favorite_enemy );
		ai = undefined;
		if ( isDefined( spawn_loc ) )
			ai = spawn_dog( spawn_loc, favorite_enemy );

		if ( isDefined( ai ) )
		{
			level.zombie_total--;
			level.zm_round_spawned++;
			count++;
			failures = 0;
		}
		else
		{
			// A hound that cannot be placed is not owed forever: after a run
			// of failures the round's remaining budget is released, so
			// round_wait can close the round (T5 asserts instead).
			failures++;
			if ( failures >= 10 )
			{
				codolprintln( "CoDOL zm dogs: could not place a hellhound 10 times; releasing " + ( max - count ) + " from the budget" );
				level.zombie_total = 0;
				return;
			}
			wait( 1 );
			continue;
		}

		waiting_for_next_dog_spawn( count, max );
	}
}

// T5 :216.
waiting_for_next_dog_spawn( count, max )
{
	default_wait = 1.5;

	if ( level.dog_round_count == 1 )
		default_wait = 3;
	else if ( level.dog_round_count == 2 )
		default_wait = 2.5;
	else if ( level.dog_round_count == 3 )
		default_wait = 2;

	default_wait = default_wait - ( count / max );
	if ( level.zm_round_mode == 0 )
		default_wait = default_wait * 1.20;
	else if ( level.zm_round_mode >= 2 )
		default_wait = default_wait * 0.80;
	if ( default_wait < 0.1 )
		default_wait = 0.1;

	wait( default_wait );
}

// T5 :248.  The last hound drops a Max Ammo where it died: C1's own tip
// (PLATFORM_DYK_ZM_MSG10) promises exactly this.
dog_round_aftermath()
{
	level endon( "end_game" );

	level waittill( "last_dog_down" );
	level.dog_aftermath_running = true;

	level thread maps\_zombiemode_audio::change_zombie_music( "dog_end" );

	power_up_origin = level.last_dog_origin;
	if ( isDefined( power_up_origin ) )
	{
		codolprintln( "CoDOL zm dogs: last hellhound down at " + power_up_origin + "; dropping Max Ammo" );
		level thread maps\_zombiemode_powerups::specific_powerup_drop( "full_ammo", power_up_origin );
	}

	// T5 :259-262: clientnotify "dog_stop" after 2 s (the map fog goes back,
	// see zm_sumpf_sh sumpf_fog), the round ends 6 s after that.
	wait( 2 );
	level notify( "dog_stop" );
	wait( 6 );
	level.dog_intermission = false;
	level.dog_aftermath_running = false;
}

// T5 :349 dog_spawn_factory_logic: 400..1000 units from the hunted player,
// never the previous struct twice; else the nearest struct past 300 units;
// else any.
dog_spawn_location( favorite_enemy )
{
	locs = refresh_dog_locations();
	if ( locs.size < 1 )
	{
		// No hound struct with a floor in an open zone -- zm_zombie_training,
		// whose hound structs are Prototype's, at Prototype's coordinates.
		// The landing points inside the open windows are on the walkable floor
		// of the open zones by construction (the zombies' own way in); the
		// minigun boss arrives at the same points.  MEASURED on the probe: the
		// zombie spawners outside the windows left hounds stuck at the boards.
		locs = window_inside_locations();
	}
	if ( locs.size < 1 )
	{
		// Neither: the map's own path nodes around the hunted player (zm_
		// zombie_training's start room has one zombie window, which the zone
		// manager files under a closed zone).
		locs = path_node_locations( favorite_enemy );
	}
	if ( locs.size < 1 )
		return undefined;
	locs = array_randomize( locs );

	for ( i = 0; i < locs.size; i++ )
	{
		if ( isDefined( level.old_dog_spawn ) && level.old_dog_spawn == locs[ i ] )
			continue;

		dist_squared = distanceSquared( locs[ i ].origin, favorite_enemy.origin );
		if ( dist_squared > ( 400 * 400 ) && dist_squared < ( 1000 * 1000 ) )
		{
			level.old_dog_spawn = locs[ i ];
			return locs[ i ];
		}
	}

	// Small rooms (Prototype's start room is ~900 units across): the nearest
	// struct that still keeps the strike off the player.
	best = undefined;
	best_dist = 0;
	for ( i = 0; i < locs.size; i++ )
	{
		d = distanceSquared( locs[ i ].origin, favorite_enemy.origin );
		if ( d < 300 * 300 )
			continue;
		if ( !isDefined( best ) || d < best_dist )
		{
			best = locs[ i ];
			best_dist = d;
		}
	}
	if ( isDefined( best ) )
	{
		level.old_dog_spawn = best;
		return best;
	}

	return locs[ 0 ];
}

window_inside_locations()
{
	if ( !isDefined( level.zm_dog_window_locs ) )
		level.zm_dog_window_locs = [];

	locs = [];
	if ( !isDefined( level.exterior_goals ) )
		return locs;
	for ( i = 0; i < level.exterior_goals.size; i++ )
	{
		goal = level.exterior_goals[ i ];
		if ( !maps\_zombiemode_zone_manager::entrance_available( goal ) )
			continue;
		point = maps\_zombiemode_zone_manager::entrance_inside_point( goal );
		if ( !isDefined( point ) )
			continue;
		// Only the windows of zones the players have opened.
		zone = maps\_zombiemode_zone_manager::zone_for_point( point, 200 );
		if ( isDefined( zone ) && !zone.enabled )
			continue;
		if ( !isDefined( level.zm_dog_window_locs[ i ] ) )
		{
			loc = spawnStruct();
			loc.origin = point;
			level.zm_dog_window_locs[ i ] = loc;
		}
		locs[ locs.size ] = level.zm_dog_window_locs[ i ];
	}
	return locs;
}

// node_pathnode points 300..1000 units from the player, on a floor and at
// roughly the player's height: the actor's own navigation graph, so a hound
// placed there can always path.
path_node_locations( player )
{
	locs = [];
	nodes = getAllNodes();
	for ( i = 0; isDefined( nodes ) && i < nodes.size && locs.size < 24; i++ )
	{
		n = nodes[ i ];
		if ( !isDefined( n.classname ) || n.classname != "node_pathnode" )
			continue;
		d = distanceSquared( n.origin, player.origin );
		if ( d < 300 * 300 || d > 1000 * 1000 )
			continue;
		dz = n.origin[ 2 ] - player.origin[ 2 ];
		if ( dz > 64 || dz < -64 )
			continue;
		loc = spawnStruct();
		loc.origin = n.origin;
		locs[ locs.size ] = loc;
	}
	return locs;
}

// T5 :369.
get_favorite_enemy()
{
	dog_targets = getPlayers();
	least_hunted = undefined;
	for ( i = 0; i < dog_targets.size; i++ )
	{
		if ( !isDefined( dog_targets[ i ].hunted_by ) )
			dog_targets[ i ].hunted_by = 0;

		if ( !is_player_valid( dog_targets[ i ] ) || is_true( dog_targets[ i ].zm_extracted ) )
			continue;

		if ( !isDefined( least_hunted ) || dog_targets[ i ].hunted_by < least_hunted.hunted_by )
			least_hunted = dog_targets[ i ];
	}

	if ( isDefined( least_hunted ) )
		least_hunted.hunted_by += 1;

	return least_hunted;
}

// T5 :400.  T5 then multiplies by scr_dog_health_walk_multiplier for its
// walking hounds; CoDOL's hounds always sprint, so the table value stands.
dog_health_increase()
{
	if ( level.dog_round_count == 1 )
		level.dog_health = 400;
	else if ( level.dog_round_count == 2 )
		level.dog_health = 900;
	else if ( level.dog_round_count == 3 )
		level.dog_health = 1300;
	else
		level.dog_health = 1600;
}

// --------------------------------------------------------------------------
// The hound.  T5 :273 dog_spawn_fx + :556 dog_init.  T5 spawns the actor
// hidden behind magic_bullet_shield and reveals it after the strike; here
// the strike plays first and the actor is spawned into it, which is the same
// thing on screen without an invulnerable invisible actor in between.
// --------------------------------------------------------------------------

spawn_dog( spawn_loc, favorite_enemy )
{
	if ( !isDefined( level.dog_spawner ) )
		return undefined;

	origin = spawn_loc.origin;

	// T5 dog_spawn_fx :281-288: the build-up strike and the pre-spawn crackle,
	// 1.5 s, then the bolt, the quake, the rumble and the arrival.
	playFx( level._effect[ "lightning_dog_spawn" ], origin );
	playSoundAtPosition( "zmb_hellhound_prespawn", origin );
	wait( 1.5 );
	playSoundAtPosition( "zmb_hellhound_bolt", origin );
	earthquake( 0.5, 0.75, origin, 1000 );
	playRumbleOnPosition( "explosion_generic", origin );
	playSoundAtPosition( "zmb_hellhound_spawn", origin );

	ai = level.dog_spawner spawnActor();
	if ( !isDefined( ai ) )
	{
		codolprintln( "CoDOL zm dogs: spawnactor returned no hellhound (actor budget, aitype or animscripts)" );
		return undefined;
	}

	// T5 :291-293: face the hunted player.
	angles = ( 0, randomInt( 360 ), 0 );
	if ( isDefined( favorite_enemy ) )
	{
		face = vectorToAngles( favorite_enemy.origin - origin );
		angles = ( 0, face[ 1 ], 0 );
	}
	ai forceTeleport( origin, angles );

	ai dog_init( favorite_enemy );
	ai zombie_setup_attack_properties_dog();

	level.zombies[ level.zombies.size ] = ai;

	// T5 :304-307: "dog should come out running after this wait".
	wait( 0.05 );
	if ( isDefined( ai ) && isAlive( ai ) )
	{
		ai show();
		ai.ignoreme = false;
		ai notify( "visible" );
	}

	if ( isDefined( ai ) && ( !isDefined( level.zm_dog_spawn_reported ) || level.zm_dog_spawn_reported != level.round_number ) )
	{
		level.zm_dog_spawn_reported = level.round_number;
		codolprintln( "CoDOL zm dogs: round " + level.round_number + " first hellhound spawned at " +
			ai.origin + " (ent " + ai getEntityNumber() + ", " + ai.health + " hp) hunting " + favorite_enemy.name );
	}

	return ai;
}

// T5 :558 dog_init, the fields that exist on this server's actor.
dog_init( favorite_enemy )
{
	self.targetname = "zombie_dog";
	self.animname = "zombie_dog";
	self.isdog = true;
	self.allowdeath = true;
	// T5 :566 / :619: hellhounds never flinch (native T5 allowPain gate).
	self.allowpain = false;
	if ( isDefined( self.a ) )
		self.a.disablePain = true;
	self.force_gib = true;
	// _zombiemode::Callback_PlayerDamage treats an is_zombie attacker's hit as
	// worth its meleeDamage; T5 dog_init :625 self.meleeDamage = 40.
	self.is_zombie = true;
	self.meleeDamage = 40;
	self.has_legs = true;
	self.gibbed = false;
	self.head_gibbed = false;
	self.default_goalheight = 40;
	self.grenadeawareness = 0;
	self.badplaceawareness = 0;
	self.ignoreSuppression = true;
	self.suppressionThreshold = 1;
	self.noDodgeMove = true;
	self.dontShootWhileMoving = true;
	self.pathenemylookahead = 0;
	self.chatInitialized = false;
	self.dropweapon = false;
	self.freezegun_damage = 0;
	self.flame_damage_time = 0;
	self.zombie_move_speed = "sprint";

	// T5 :627-628.  thundergun_zm ships in zm_prototype_sh; whichever script
	// ports its blast reads these hooks for a hound.
	self.thundergun_disintegrate_func = ::dog_thundergun_disintegrate;
	self.thundergun_knockdown_func = ::dog_thundergun_knockdown;

	self.aiweapon = character\character_zm_zombie_dog::bite_weapon();

	// T5 :595-602 multiplies by scr_dog_health_walk_multiplier, and
	// dog_run_think :939 takes it back down to level.dog_health on "visible"
	// (0.05 s later); the hound is never hittable in between, so it starts
	// at level.dog_health here.
	self.maxhealth = level.dog_health;
	self.health = level.dog_health;

	// T5 :563-564: blind and unseen until the strike reveals it.
	self.ignoreall = true;
	self.ignoreme = true;
	// T5 dog_round_spawning :185 ai.favoriteenemy: the least-hunted player is
	// this hound's quarry (native threat pass, T5 actor_threat.cpp:317).
	self.favoriteenemy = favorite_enemy;
	self.zm_dog_round = level.dog_round_count;

	self thread dog_run_think();
	self thread dog_stalk_audio();
	self dog_fx_trail_type();

	level thread dog_death( self );
	self thread maps\_zombiemode_spawner::zombie_damage_watch();
	self thread dog_failsafe();
}

// T5 :746 zombie_setup_attack_properties_dog.
zombie_setup_attack_properties_dog()
{
	self thread dog_behind_audio();

	// allows the hound to attack
	self.ignoreall = false;

	self.pathEnemyFightDist = 64;
	self.meleeAttackDist = 64;

	// turn off transition anims
	self.disableArrivals = true;
	self.disableExits = true;
}

// T5 :929 dog_run_think: on "visible" the eye glow and the trail start.
dog_run_think()
{
	self endon( "death" );

	self waittill( "visible" );

	if ( self.health > level.dog_health )
	{
		self.maxhealth = level.dog_health;
		self.health = level.dog_health;
	}

	self dog_fx_eye_glow();
	playFxOnTag( self.fx_dog_trail_type, self, "tag_origin" );
	self playLoopSound( self.fx_dog_trail_sound );
}

// T5 :955 dog_stalk_audio: the ambient snarl while the hound stalks (trots);
// it stops when it breaks into the sprint or attacks.
dog_stalk_audio()
{
	self endon( "death" );
	self endon( "dog_running" );
	self endon( "dog_combat" );

	for ( ;; )
	{
		self.dog_stalk_vox_playing = true;
		self playSound( "zmb_hellhound_vocals_amb", "stalk_vox_done" );
		self waittill( "stalk_vox_done" );
		self.dog_stalk_vox_playing = false;
		wait( randomFloatRange( 1, 4 ) );
	}
}

// T5 :766 stop_dog_sound_on_death.
stop_dog_sound_on_death()
{
	self waittill( "death" );
	if ( isDefined( self ) )
		self stopSounds();
}

// T5 :772 dog_behind_audio: once it runs, a hound behind a standing player
// (more than 90 degrees off his view, farther than 100 units) growls at him.
dog_behind_audio()
{
	self thread stop_dog_sound_on_death();

	self endon( "death" );
	self common_scripts\utility::waittill_any( "dog_running", "dog_combat" );

	// This VM allows one playsound notification per entity at a time (C1
	// 0x54d312 throws on a second one); let the last stalking snarl finish.
	if ( is_true( self.dog_stalk_vox_playing ) )
		self waittill( "stalk_vox_done" );

	self playSound( "zmb_hellhound_vocals_close", "sounddone" );
	self waittill( "sounddone" );

	for ( ;; )
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( !isDefined( players[ i ] ) )
				continue;
			dogAngle = angleClamp180( vectorToAngles( self.origin - players[ i ].origin )[ 1 ] - players[ i ].angles[ 1 ] );

			if ( isAlive( players[ i ] ) && !isDefined( players[ i ].revivetrigger ) )
			{
				if ( abs( dogAngle ) > 90 && distance2d( self.origin, players[ i ].origin ) > 100 )
				{
					self playSound( "zmb_hellhound_vocals_close", "sounddone" );
					self waittill( "sounddone" );
				}
			}
		}

		wait( 0.75 );
	}
}

// T5 round_spawn_failsafe (_zombiemode.gsc:4059) for a hound: one that fell
// out of the world, or has not moved 24 units in 30 s, dies so the round can
// end.  Unlike a zombie at its window, a hound standing still is usually
// biting: MEASURED on the first probe, the zombie failsafe culled two hounds
// at full health that were mauling a stationary player.  A hound with a
// valid player inside 160 units is never culled.
dog_failsafe()
{
	self endon( "death" );

	prevorigin = self.origin;
	for ( ;; )
	{
		wait( 30 );

		if ( self.origin[ 2 ] < level.zombie_vars[ "below_world_check" ] )
		{
			self doDamage( self.health + 100, self.origin );
			return;
		}

		if ( distanceSquared( self.origin, prevorigin ) < 576 && !dog_near_player( 160 ) )
		{
			codolprintln( "CoDOL zm dogs: hellhound " + self getEntityNumber() + " stuck at " + self.origin + "; culled" );
			self doDamage( self.health + 100, self.origin );
			return;
		}

		prevorigin = self.origin;
	}
}

dog_near_player( radius )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( is_player_valid( players[ i ] ) && distanceSquared( players[ i ].origin, self.origin ) < radius * radius )
			return true;
	}
	return false;
}

// Live enemies: level.zombies still holds a dying actor until its entity is
// freed, so get_enemy_count() counts the hound that just died.
live_enemy_count()
{
	count = 0;
	for ( i = 0; i < level.zombies.size; i++ )
	{
		z = level.zombies[ i ];
		if ( isDefined( z ) && isAlive( z ) && !is_true( z.ignore_enemy_count ) )
			count++;
	}
	return count;
}

// T5 :639 dog_fx_eye_glow links a tag_origin model to J_EyeBall_LE so the
// clientscript can play on it; the server plays the glow on the bone itself
// and stops it there (dog_death).
dog_fx_eye_glow()
{
	playFxOnTag( level._effect[ "dog_eye_glow" ], self, "j_eyeball_le" );
}

// T5 :650 dog_fx_trail: half the hounds burn (and burst on death, a.nodeath),
// the rest trail ash.  The effect itself starts on "visible" (dog_run_think).
dog_fx_trail_type()
{
	if ( randomInt( 100 ) > level.dog_fire_trail_percent )
	{
		self.fx_dog_trail_type = level._effect[ "dog_trail_ash" ];
		self.fx_dog_trail_sound = "zmb_hellhound_loop_breath";
	}
	else
	{
		// fire dogs will explode during death
		self.fire_dog = true;
		if ( isDefined( self.a ) )
			self.a.nodeath = true;
		self.fx_dog_trail_type = level._effect[ "dog_trail_fire" ];
		self.fx_dog_trail_sound = "zmb_hellhound_loop_fire";
	}
}

// T5 :674 dog_death.  The kill pays the "death" points like a zombie's and
// counts toward the match's kills; the last hound of the round drops the Max
// Ammo (dog_round_aftermath).  T5's hellhounds roll no random power-up.
dog_death( dog )
{
	dog waittill( "death", attacker );

	level.total_zombies_killed++;

	origin = undefined;
	fire_dog = false;
	if ( isDefined( dog ) )
	{
		origin = dog.origin;
		fire_dog = is_true( dog.fire_dog );
		// T5 :710: sound.
		dog stopLoopSound();
		// T5 :713-717: the eye and trail carriers are deleted with the hound.
		stopFxOnTag( level._effect[ "dog_eye_glow" ], dog, "j_eyeball_le" );
		if ( isDefined( dog.fx_dog_trail_type ) )
			stopFxOnTag( dog.fx_dog_trail_type, dog, "tag_origin" );
	}

	// The same frame-end wait as _zombiemode_spawner::zombie_death_event, so
	// zombie_damage_watch has recorded the killing hit.
	waittillframeend;

	if ( !isPlayer( attacker ) && isDefined( dog ) && isDefined( dog.attacker ) )
		attacker = dog.attacker;

	zombie_list_compact();

	// T5 :678.
	if ( live_enemy_count() == 0 && level.zombie_total <= 0 && flag( "dog_round" ) )
	{
		level.last_dog_origin = origin;
		level notify( "last_dog_down" );
	}

	if ( isDefined( attacker ) && isPlayer( attacker ) )
	{
		mod = undefined;
		hit_location = undefined;
		if ( isDefined( dog ) )
		{
			mod = dog.damagemod;
			hit_location = dog.damagelocation;
		}
		attacker maps\_zombiemode_score::player_add_points( "death", mod, hit_location );

		if ( !isDefined( attacker.kills ) )
			attacker.kills = 0;
		attacker.kills = attacker.kills + 1;
		attacker notify( "zom_kill", dog );

		// T5 :697-700: 20% of kills draw the player's "kill_hellhound" line;
		// no C1 character vox set has a hound line, so nothing is said.
	}

	level notify( "zom_kill" );

	if ( !isDefined( origin ) )
		return;

	if ( fire_dog )
	{
		// T5 :719-722: a burning hound bursts (dog_explode_fx) and is deleted
		// outright; zombie_dog_death skips its animation (a.nodeath).  The
		// burst deals no damage in T5.
		// Deleted here, at the end of its death frame (waittillframeend
		// above), as T5 deletes it on its death frame; the native free hook
		// (CODOL_ActorForget) ends its animscript threads.
		level thread dog_explode_fx( origin );
		if ( isDefined( dog ) )
			dog delete();
	}
	else if ( isDefined( dog ) )
	{
		// T5 :726: an ash hound falls with its death animation and a yelp.
		dog playSound( "zmb_hellhound_vocals_death" );
	}
}

// T5 :731 dog_explode_fx.
dog_explode_fx( origin )
{
	playFx( level._effect[ "dog_gib" ], origin );
	playSoundAtPosition( "zmb_hellhound_explode", origin );
}

// T5 :806 dog_clip_monitor: the map's dog_clips brushes stand while a hound
// is alive or a hound round runs (none of the shipped zm maps author any).
dog_clip_monitor()
{
	level.dog_clips = getEntArray( "dog_clips", "targetname" );
	if ( !isDefined( level.dog_clips ) || level.dog_clips.size == 0 )
		return;

	for ( ;; )
	{
		for ( i = 0; i < level.dog_clips.size; i++ )
		{
			level.dog_clips[ i ] common_scripts\utility::trigger_off();
			level.dog_clips[ i ] connectPaths();
		}

		flag_wait( "dog_clips" );

		if ( is_true( level.no_dog_clip ) )
			return;

		for ( i = 0; i < level.dog_clips.size; i++ )
		{
			level.dog_clips[ i ] common_scripts\utility::trigger_on();
			level.dog_clips[ i ] disconnectPaths();
			wait( 0.05 );
		}

		dog_is_alive = true;
		while ( dog_is_alive || flag( "dog_round" ) )
		{
			dog_is_alive = false;
			dogs = getEntArray( "zombie_dog", "targetname" );
			for ( i = 0; i < dogs.size; i++ )
			{
				if ( isAlive( dogs[ i ] ) )
					dog_is_alive = true;
			}
			wait( 1 );
		}

		flag_clear( "dog_clips" );
		wait( 1 );
	}
}

// T5 :969.
dog_thundergun_disintegrate( player )
{
	self endon( "death" );

	self doDamage( self.health + 666, player.origin, player );
}

// T5 :976.
dog_thundergun_knockdown( player, gib )
{
	self endon( "death" );

	damage = int( self.maxhealth * 0.5 );
	self doDamage( damage, player.origin, player );
}
