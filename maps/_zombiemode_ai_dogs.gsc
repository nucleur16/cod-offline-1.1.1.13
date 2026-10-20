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
// round itself is never a hound round; every difficulty mixes hounds into its horde.
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

	// T5 keeps the hound rounds off unless the level enables them; every
	// CoDOL zm map authors hound structs, so they are on unless a server
	// turns them off.
	level.dog_rounds_enabled = true;
	if ( getDvar( "scr_zm_dog_rounds" ) != "" && getDvarInt( "scr_zm_dog_rounds" ) == 0 )
		level.dog_rounds_enabled = false;

	level._effect[ "lightning_dog_spawn" ]	= loadfx( "maps/zombie/fx_zombie_dog_lightning_buildup" );
	level._effect[ "dog_lightning_strike" ]	= loadfx( "maps/zombie/fx_zombie_dog_lightning_spawn" );
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
		{}
		level.dogs_enabled = false;
		return;
	}
	level.dog_spawner.targetname = "zombie_dog_spawner";
	{}
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

	// An empty enabled group is not permission to spawn in closed rooms.
	// dog_spawn_location falls back to inside entrances/path nodes instead.

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
	if ( trace[ "fraction" ] <= 0 || trace[ "fraction" ] >= 1 )
		return false;
	// A trace can hit a wall/steep face, not a walkable floor.
	return trace[ "normal" ][ 2 ] >= 0.7;
}

dog_location_is_open( loc )
{
	if ( !dog_location_has_floor( loc ) )
		return false;
	// Navigation nodes also exist outside windows and behind unopened doors.
	// A floor alone does not make either a fair hellhound spawn.
	if ( isDefined( level.zm_spawn_zones ) && level.zm_spawn_zones.size > 0 )
	{
		zone = maps\_zombiemode_zone_manager::zone_at_point( loc.origin );
		if ( !isDefined( zone ) || !zone.enabled )
			return false;
	}
	return true;
}

open_dog_locations( locations )
{
	kept = [];
	for ( i = 0; i < locations.size; i++ )
	{
		if ( dog_location_is_open( locations[ i ] ) )
			kept[ kept.size ] = locations[ i ];
	}
	return kept;
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
	{}

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

			if ( level.zm_round_cap > 0 )
			{
				level.next_dog_round = 0;
				if ( level.dog_rounds_played < level.zm_hound_rounds_goal &&
					level.round_number + 4 < level.zm_round_cap )
					level.next_dog_round = level.round_number + 4;
			}
			else
				level.next_dog_round = level.round_number + randomIntRange( 4, 6 );
			{}
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

// T5 :489.  T5 also swaps the AI melee dvars for the round; this server's
// actor melee reads its own actorMelee* cvars and the hound's meleeAttackDist
// (zombie_dog_init setMeleeAttackDist), so nothing is swapped here.
dog_round_start()
{
	flag_set( "dog_round" );
	level notify( "dog_round_starting" );
}

// T5 :519.
dog_round_stop()
{
	flag_clear( "dog_round" );
	level notify( "dog_round_ending" );
}

// T5 :540 play_dog_round, with C1's assets: the announcer line, and C1's own
// warning text (ZOMBIE_WARNING_DOG_ARRIVED) in place of T5's clientscript
// fog/vision change.  The dog-round music is _zombiemode_audio's
// "dog_start" state, played by its round watcher (level.music_round_override).
play_dog_round()
{
	self endon( "disconnect" );

	self thread dog_round_notice();

	wait( 1 );
	self playLocalSound( "zmb_vox_ann_dogstart" );
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
	level endon( "zm_stop_spawning" );

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ].hunted_by = 0;

	if ( maps\_zombiemode_spawner::spawning_closed() )
		return;

	if ( !is_true( level.dogs_enabled ) )
	{
		level.zombie_total = 0;
		{}
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

	{}

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
				{}
				level.zombie_total = 0;
				return;
			}
			wait( 1 );
			continue;
		}

		waiting_for_next_dog_spawn( count, max );
	}
}

// Mixed final horde on every difficulty: ordinary admissions own the AI cap and
// budget. Do not enable dog_round, replace round functions, run aftermath or
// claim a separate hound-round budget/music state. Reuse the survival round's
// two-live-hounds-per-standing-player ceiling; other slots remain humanoids.
final_round_dog_due()
{
	if ( !is_true( level.zm_extraction_active ) || !maps\_zombiemode::is_final_round() )
		return false;
	if ( !is_true( level.dogs_enabled ) || !isDefined( level.dog_spawner ) ||
		maps\_zombiemode_spawner::spawning_closed() )
		return false;

	players = getPlayers();
	standing = 0;
	for ( i = 0; i < players.size; i++ )
	{
		if ( is_player_valid( players[ i ] ) && !is_true( players[ i ].zm_extracted ) )
			standing++;
	}
	if ( standing < 1 )
		return false;

	live_dogs = 0;
	for ( i = 0; i < level.zombies.size; i++ )
	{
		ai = level.zombies[ i ];
		if ( isDefined( ai ) && isAlive( ai ) && is_true( ai.isdog ) )
			live_dogs++;
	}
	return live_dogs < standing * 2;
}

spawn_final_round_dog()
{
	if ( !final_round_dog_due() )
		return undefined;
	target = get_favorite_enemy();
	if ( !isDefined( target ) )
		return undefined;
	loc = dog_spawn_location( target );
	if ( !isDefined( loc ) )
		return undefined;
	// The existing count advances after each completed survival hound round.
	// Keep that same health progression for the final horde's ordinary hounds.
	dog_health_increase();
	return spawn_dog( loc, target );
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
	// A local skip can end admission before even one hound spawned. Do not
	// let that old round's Max Ammo/intermission waiter survive into a new one.
	level endon( "end_of_round" );
	level endon( "zm_stop_spawning" );

	level waittill( "last_dog_down" );

	// The audio round watcher owns the hound ending at end_of_round. A second
	// cue here races the ordinary ending and can duplicate or interrupt music.

	power_up_origin = level.last_dog_origin;
	if ( isDefined( power_up_origin ) )
	{
		{}
		level thread maps\_zombiemode_powerups::specific_powerup_drop( "full_ammo", power_up_origin );
	}

	wait( 8 );
	level.dog_intermission = false;
}

// T5 :349 dog_spawn_factory_logic: 400..1000 units from the hunted player,
// never the previous struct twice; else the nearest struct past 300 units;
// else any.
dog_spawn_location( favorite_enemy )
{
	locs = open_dog_locations( refresh_dog_locations() );
	if ( locs.size < 1 )
	{
		// No hound struct with a floor in an open zone -- zm_zombie_training,
		// whose hound structs are Prototype's, at Prototype's coordinates.
		// The landing points inside the open windows are on the walkable floor
		// of the open zones by construction (the zombies' own way in); the
		// minigun boss arrives at the same points.  MEASURED on the probe: the
		// zombie spawners outside the windows left hounds stuck at the boards.
		locs = open_dog_locations( window_inside_locations() );
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
		zone = maps\_zombiemode_zone_manager::zone_at_point( point );
		if ( !isDefined( zone ) || !zone.enabled )
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
		if ( dog_location_is_open( loc ) )
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
	if ( !dog_location_is_open( spawn_loc ) )
		return undefined;

	// Place feet on the tested floor, not an authored point floating above it.
	trace = bulletTrace( spawn_loc.origin + ( 0, 0, 16 ), spawn_loc.origin - ( 0, 0, 128 ), false, undefined );
	origin = trace[ "position" ] + ( 0, 0, 1 );

	// T5 dog_spawn_fx: the build-up, 1.5 s, then the bolt.
	playFx( level._effect[ "lightning_dog_spawn" ], origin );
	playSoundAtPosition( "zmb_hellhound_prespawn", origin );
	wait( 1.5 );
	playSoundAtPosition( "zmb_hellhound_bolt", origin );
	playFx( level._effect[ "dog_lightning_strike" ], origin );
	earthquake( 0.5, 0.75, origin, 1000 );
	playSoundAtPosition( "zmb_hellhound_spawn", origin );

	ai = level.dog_spawner spawnActor();
	if ( !isDefined( ai ) )
	{
		{}
		return undefined;
	}

	// Face the hunted player.
	angles = ( 0, randomInt( 360 ), 0 );
	if ( isDefined( favorite_enemy ) )
	{
		face = vectorToAngles( favorite_enemy.origin - origin );
		angles = ( 0, face[ 1 ], 0 );
	}
	ai forceTeleport( origin, angles );
	ai show();

	ai dog_init( favorite_enemy );

	level.zombies[ level.zombies.size ] = ai;

	if ( !isDefined( level.zm_dog_spawn_reported ) || level.zm_dog_spawn_reported != level.round_number )
	{
		level.zm_dog_spawn_reported = level.round_number;
		{}
	}

	return ai;
}

// T5 :556 dog_init, the fields that exist on this server's actor.
dog_init( favorite_enemy )
{
	self.targetname = "zombie_dog";
	self.animname = "zombie_dog";
	self.isdog = true;
	// _zombiemode::Callback_PlayerDamage treats an is_zombie attacker's hit as
	// worth its meleeDamage; T5 dog_init :641 self.meleeDamage = 40.
	self.is_zombie = true;
	self.meleeDamage = 40;
	self.allowdeath = true;
	self.ignoreSuppression = true;
	self.suppressionThreshold = 1;
	self.noDodgeMove = true;
	self.grenadeawareness = 0;
	self.badplaceawareness = 0;
	self.dropweapon = false;

	self.aiweapon = character\character_zm_zombie_dog::bite_weapon();

	self.maxhealth = level.dog_health;
	self.health = level.dog_health;

	// A hound hunts at once (T5 reveals it with ignoreme/ignoreall cleared);
	// the perfect-info species row hands it the nearest valid player.
	self.ignoreall = false;
	self.ignoreme = false;
	self.favoriteenemy = favorite_enemy;
	self.zm_dog_round = level.dog_round_count;

	self dog_fx_eye_glow();
	self dog_fx_trail();

	level thread dog_death( self );
	self thread maps\_zombiemode_spawner::zombie_damage_watch();
	self thread dog_failsafe();
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
			{}
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

// T5 :608 dog_fx_eye_glow links a tag_origin model to J_EyeBall_LE so the
// clientscript can play on it; the server plays the glow on the bone itself.
dog_fx_eye_glow()
{
	playFxOnTag( level._effect[ "dog_eye_glow" ], self, "j_eyeball_le" );
}

// T5 :618 dog_fx_trail: half the hounds burn and explode on death, the rest
// trail ash.
dog_fx_trail()
{
	if ( randomInt( 100 ) > level.dog_fire_trail_percent )
	{
		self.fx_dog_trail_type = level._effect[ "dog_trail_ash" ];
		self.fx_dog_trail_sound = "zmb_hellhound_loop_breath";
	}
	else
	{
		self.fire_dog = true;
		self.fx_dog_trail_type = level._effect[ "dog_trail_fire" ];
		self.fx_dog_trail_sound = "zmb_hellhound_loop_fire";
	}

	playFxOnTag( self.fx_dog_trail_type, self, "tag_origin" );
	self playLoopSound( self.fx_dog_trail_sound );
}

// T5 :656 dog_death.  The kill pays the "death" points like a zombie's and
// counts toward the match's kills; the last hound of the round drops the Max
// Ammo (dog_round_aftermath).  T5's hellhounds roll no random power-up.
dog_death( dog )
{
	dog waittill( "death", attacker );

	level.total_zombies_killed++;

	origin = undefined;
	if ( isDefined( dog ) )
	{
		origin = dog.origin;
		dog stopLoopSound();
	}

	// The same frame-end wait as _zombiemode_spawner::zombie_death_event, so
	// zombie_damage_watch has recorded the killing hit.
	waittillframeend;

	if ( !isPlayer( attacker ) && isDefined( dog ) && isDefined( dog.attacker ) )
		attacker = dog.attacker;

	if ( isDefined( origin ) )
	{
		// T5 dog_death :702: every hound bursts on death (fire hounds also skip
		// the death animation; T5 deletes them).
		playFx( level._effect[ "dog_gib" ], origin );
		playSoundAtPosition( "zmb_hellhound_explode", origin );
	}

	zombie_list_compact();

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
	}

	level notify( "zom_kill" );

	// A burning hound is gone in its explosion (T5 :702 self delete()).
	if ( isDefined( dog ) && is_true( dog.fire_dog ) )
	{
		wait( 0.1 );
		if ( isDefined( dog ) )
			dog hide();
	}
}
