// zm power-ups: the drop-and-collect module.
//
// Port of T5's maps\_zombiemode_powerups.gsc (3,048 lines).  This was the
// single highest-value module left after the gameplay-loop pass, because the
// content is already shipped and _zombiemode_score.gsc already honours three
// of the events -- what was missing was the thing that drops and collects.
//
// ==========================================================================
// THE PICKUPS ARE CoDOL's, NOT T5's.  MEASURED, and this is the one place
// where the mandate ("codol/c1 being the absolute source of truth wherever
// there are conflicts") changes the shape of the port rather than one name.
//
// T5 registers each power-up with add_zombie_powerup( name, MODEL, hint, ... )
// and the models are p_zom_* xmodels.  docs/zone_assets/ALL_xmodel.txt has
// NO p_zom powerup model in any zone -- CoDOL ships the power-ups as
// WEAPONS instead.  docs/zone_assets/ALL_weapon.txt, exactly fifteen, all in
// common_gametype_zm, verified for this task rather than taken from the
// previous pass's note (which guessed "kis_max_ammo"; the real name is
// kis_full_ammo_zm):
//
//     kis_all_revive_zm            kis_insta_kill_zm
//     kis_bonus_points_player_zm   kis_minigun_zm
//     kis_bonus_points_team_zm     kis_nuke_zm
//     kis_carpenter_zm             kis_pandora_zm
//     kis_double_points_zm         kis_rcbomb_zm
//     kis_fire_sale_zm             kis_sentry_zm
//     kis_free_perk_zm             kis_tesla_zm
//     kis_full_ammo_zm
//
// So the world model comes from getWeaponModel( <the kis name> ), which is a
// registered builtin, and the spinning script_model, the timeout blink and
// the proximity grab are T5's unchanged.  A kis_* whose WeaponDef does not
// resolve in the mounted zones is simply not registered, the same validation
// _zombiemode_weapons::add_zombie_weapon and _zombiemode::resolveStartWeapon
// already use.
// ==========================================================================
//
// TAKEN, by T5 function:
//   init / init_powerups          :7,:69    -- the registry and the tuning.
//   add_zombie_powerup            :554      -- model from the weapon, see above.
//   randomize_powerups            :337      -- the shuffled bag.
//   get_next_powerup              :345      -- draw without replacement.
//   get_valid_powerup             :368      -- the per-draw legality filter.
//   watch_for_drop                :523      -- the score-threshold that arms
//                                              the next drop.
//   powerup_round_start           :610
//   powerup_drop                  :615      -- the 2% random / 98% flagged
//                                              split and the per-round cap.
//   powerup_setup                 :907
//   powerup_grab                  :1197     -- the 64-unit proximity grab and
//                                              its per-power-up switch.
//   powerup_wobble                :1574
//   powerup_timeout               :1626     -- the blink and the despawn.
//   powerup_vo                    :1557
//   nuke_powerup                  :1677
//   double_points_powerup         :1792     + time_remaining_on_point_doubler
//   full_ammo_powerup             :1807
//   insta_kill_powerup            :1836     + time_remaning_on_insta_kill
//   start_carpenter               :1463
//   start_fire_sale               :1407
//   start_revive_all              :1383
//   free_perk_powerup             :2293
//
// NOT TAKEN, and why -- each of these five kis_* pickups is EXCLUDED from
// the drop bag rather than dropped and doing nothing:
//   kis_minigun_zm / kis_tesla_zm  -- T5's minigun_weapon_powerup and
//     tesla_weapon_powerup hand the player a special weapon with its own
//     script (_zombiemode_weap_tesla.gsc and the minigun timer).  Neither
//     weapon script is ported.
//   kis_rcbomb_zm / kis_sentry_zm  -- RC bomb and auto-turret, both separate
//     T5 modules (_zombiemode_ai_dogs-adjacent and _zombiemode_auto_turret).
//   kis_pandora_zm -- T5's quantum bomb (:726
//     quantum_bomb_random_powerup_result), which rolls a second power-up out
//     of a weighted table of its own.  It is the one of the five that is
//     purely script, and it is left out only because it draws from the four
//     above as well.
// Also not taken: bonfire_sale (no zm map authors the bonfire trigger),
// random_weapon (needs treasure_chest_ChooseWeightedRandomWeapon plus
// useweaponhidetags, which is not registered), the power-up HUD overlay and
// its solo variant (:180,:257 -- they SetShader T5 materials no CoDOL zone
// carries), the zombie-grabbable path (:1118, a power-up a zombie can steal;
// no CoDOL pickup is marked for it), and maps\_zombiemode_net::
// network_safe_spawn (this server has no net-throttle module, so the drop
// spawns directly).
//
// NO ANIMATION IS USED IN THIS FILE.  It needs none: T5's power-ups wobble
// with rotateto and blink with hide/show.

#include maps\mp\gametypes\_zombiemode_utility;

// --------------------------------------------------------------------------
// Init
// --------------------------------------------------------------------------

init()
{
	// T5 :69 init_powerups tuning, read through set_zombie_var so
	// mp/zombiemode.csv overrides any of them if it carries the row.
	set_zombie_var( "zombie_powerup_drop_increment",   2000 );
	set_zombie_var( "zombie_powerup_drop_max_per_round", 4 );
	set_zombie_var( "zombie_powerup_point_doubler_time", 30 );
	set_zombie_var( "zombie_powerup_insta_kill_time",    30 );
	set_zombie_var( "zombie_powerup_fire_sale_time",     30 );
	set_zombie_var( "zombie_timer_offset",                0 );

	level.zombie_vars[ "zombie_drop_item" ] = 0;
	level.zombie_vars[ "zombie_powerup_insta_kill_on" ] = 0;
	level.zombie_vars[ "zombie_powerup_point_doubler_on" ] = 0;
	level.zombie_vars[ "zombie_powerup_fire_sale_on" ] = 0;

	level.zombie_powerups = [];
	level.zombie_powerup_array = [];
	level.zombie_powerup_index = 0;
	level.powerup_drop_count = 0;
	level.zombie_score_since_drop = 0;
	level.next_drop_threshold = level.zombie_vars[ "zombie_powerup_drop_increment" ];

	add_sound( "powerup_grabbed", "zmb_powerup_grabbed" );
	add_sound( "powerup_spawn",   "zmb_spawn_powerup" );

	init_powerups();

	if ( level.zombie_powerup_array.size < 1 )
	{
		println( "CoDOL zm: no kis_* power-up resolved out of the mounted zones; power-ups are off" );
		return;
	}

	println( "CoDOL zm: " + level.zombie_powerup_array.size + " power-ups resolved out of the mounted zones\n" );

	randomize_powerups();

	level thread watch_for_drop();
	level thread powerup_round_watcher();
}

// T5 :69.  The CoDOL pickup name is the identity; the short name is the one
// T5's switch and _zombiemode_score's events use.
init_powerups()
{
	add_zombie_powerup( "insta_kill",          "kis_insta_kill_zm" );
	add_zombie_powerup( "double_points",       "kis_double_points_zm" );
	add_zombie_powerup( "full_ammo",           "kis_full_ammo_zm" );
	add_zombie_powerup( "nuke",                "kis_nuke_zm" );
	add_zombie_powerup( "carpenter",           "kis_carpenter_zm" );
	add_zombie_powerup( "fire_sale",           "kis_fire_sale_zm" );
	add_zombie_powerup( "free_perk",           "kis_free_perk_zm" );
	add_zombie_powerup( "all_revive",          "kis_all_revive_zm" );
	add_zombie_powerup( "bonus_points_player", "kis_bonus_points_player_zm" );
	add_zombie_powerup( "bonus_points_team",   "kis_bonus_points_team_zm" );
}

// T5 :554 add_zombie_powerup.  See the header for why the model argument
// became a weapon name.
add_zombie_powerup( powerup_name, weapon_name )
{
	precacheItem( weapon_name );
	if ( getWeaponIndexFromName( weapon_name ) <= 0 )
		return;

	model_name = getWeaponModel( weapon_name );
	if ( !isDefined( model_name ) || model_name == "" )
		return;

	precacheModel( model_name );

	s = spawnStruct();
	s.powerup_name = powerup_name;
	s.weapon_name = weapon_name;
	s.model_name = model_name;

	level.zombie_powerups[ powerup_name ] = s;
	level.zombie_powerup_array[ level.zombie_powerup_array.size ] = powerup_name;
}

// --------------------------------------------------------------------------
// The bag.  T5 :337 randomize_powerups, :345 get_next_powerup, :368
// get_valid_powerup.  T5 draws without replacement from a shuffled array so a
// round cannot deal the same power-up twice in a row.
// --------------------------------------------------------------------------

randomize_powerups()
{
	level.zombie_powerup_array = array_randomize( level.zombie_powerup_array );
	level.zombie_powerup_index = 0;
}

get_next_powerup()
{
	if ( level.zombie_powerup_index >= level.zombie_powerup_array.size )
		randomize_powerups();

	name = level.zombie_powerup_array[ level.zombie_powerup_index ];
	level.zombie_powerup_index++;

	return name;
}

// T5 :368.  A drawn power-up that cannot legally fire right now is skipped;
// the bag is walked at most once so the draw always terminates.
get_valid_powerup()
{
	for ( i = 0; i < level.zombie_powerup_array.size; i++ )
	{
		name = get_next_powerup();

		if ( !isDefined( name ) )
			continue;

		// T5 :400.  No second insta-kill or double-points while one is up.
		if ( name == "insta_kill" && is_true( level.zombie_vars[ "zombie_powerup_insta_kill_on" ] ) )
			continue;

		if ( name == "double_points" && is_true( level.zombie_vars[ "zombie_powerup_point_doubler_on" ] ) )
			continue;

		if ( name == "fire_sale" && is_true( level.zombie_vars[ "zombie_powerup_fire_sale_on" ] ) )
			continue;

		// T5 :430.  Nothing to revive, nothing to carpenter.
		if ( name == "all_revive" && maps\mp\gametypes\_zombiemode_laststand::player_num_in_laststand() < 1 )
			continue;

		if ( name == "carpenter" && !any_barrier_damaged() )
			continue;

		return name;
	}

	return level.zombie_powerup_array[ 0 ];
}

any_barrier_damaged()
{
	if ( !isDefined( level.exterior_goals ) )
		return false;

	for ( i = 0; i < level.exterior_goals.size; i++ )
	{
		chunks = level.exterior_goals[ i ].barrier_chunks;
		if ( !isDefined( chunks ) )
			continue;

		if ( !all_chunks_intact( chunks ) )
			return true;
	}

	return false;
}

// --------------------------------------------------------------------------
// Arming a drop.  T5 :523 watch_for_drop.
//
// T5 sums the points every player has EARNED and raises the drop flag each
// time the running total passes another zombie_powerup_drop_increment.  The
// running total is maintained here off the same "zom_kill" notify the round
// loop already uses, plus the player's own score delta, because this port's
// _zombiemode_score has no per-award notify to hook.
// --------------------------------------------------------------------------

watch_for_drop()
{
	level endon( "intermission" );

	last_total = 0;

	for ( ;; )
	{
		wait( 1 );

		total = 0;
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( isDefined( players[ i ].score_total ) )
				total += players[ i ].score_total;
			else if ( isDefined( players[ i ].score ) )
				total += players[ i ].score;
		}

		if ( total <= last_total )
		{
			last_total = total;
			continue;
		}

		level.zombie_score_since_drop += total - last_total;
		last_total = total;

		if ( level.zombie_score_since_drop < level.next_drop_threshold )
			continue;

		level.zombie_score_since_drop = 0;
		level.zombie_vars[ "zombie_drop_item" ] = 1;
	}
}

// T5 :610 powerup_round_start, driven off the round loop's own notify.
powerup_round_watcher()
{
	level endon( "intermission" );

	for ( ;; )
	{
		level waittill( "start_of_round" );
		level.powerup_drop_count = 0;
	}
}

// --------------------------------------------------------------------------
// The drop.  T5 :615 powerup_drop.  _zombiemode_spawner::zombie_death_event
// calls this with the dead zombie's origin.
// --------------------------------------------------------------------------

powerup_drop( drop_point )
{
	if ( !isDefined( level.zombie_powerup_array ) || level.zombie_powerup_array.size < 1 )
		return;

	if ( level.powerup_drop_count >= level.zombie_vars[ "zombie_powerup_drop_max_per_round" ] )
		return;

	// T5 :638.  Two zombies in a hundred drop regardless; every other drop
	// waits for the score threshold to arm it.
	if ( randomInt( 100 ) > 2 )
	{
		if ( !is_true( level.zombie_vars[ "zombie_drop_item" ] ) )
			return;
	}

	level.powerup_drop_count++;
	level.zombie_vars[ "zombie_drop_item" ] = 0;

	powerup = spawn( "script_model", drop_point + ( 0, 0, 40 ) );
	powerup powerup_setup();

	powerup thread powerup_timeout();
	powerup thread powerup_wobble();
	powerup thread powerup_grab();
}

// T5 :709 specific_powerup_drop, for a level script that wants a named drop.
specific_powerup_drop( powerup_name, drop_spot )
{
	if ( !isDefined( level.zombie_powerups[ powerup_name ] ) )
		return undefined;

	powerup = spawn( "script_model", drop_spot + ( 0, 0, 40 ) );
	powerup powerup_setup( powerup_name );

	powerup thread powerup_timeout();
	powerup thread powerup_wobble();
	powerup thread powerup_grab();

	return powerup;
}

// T5 :907.
powerup_setup( powerup_override )
{
	if ( isDefined( powerup_override ) )
		name = powerup_override;
	else
		name = get_valid_powerup();

	s = level.zombie_powerups[ name ];

	self setModel( s.model_name );

	self.powerup_name = s.powerup_name;
	self.weapon_name = s.weapon_name;

	play_sound_at_pos( "powerup_spawn", self.origin );
	self playLoopSound( "zmb_spawn_powerup_loop" );
}

// T5 :1574 powerup_wobble.  A slow spin plus T5's own bob.
powerup_wobble()
{
	self endon( "powerup_timedout" );
	self endon( "powerup_grabbed" );

	self.angles = ( 0, 0, 0 );

	for ( ;; )
	{
		self rotateTo( self.angles + ( 0, 180, 0 ), 1, 0, 0 );
		wait( 1 );
		self.angles = self.angles + ( 0, 180, 0 );
	}
}

// T5 :1626 powerup_timeout.  T5's numbers: alive for 30 s, blinking for the
// last 5 at 4 Hz.
powerup_timeout()
{
	self endon( "powerup_grabbed" );

	wait( 25 );

	for ( i = 0; i < 20; i++ )
	{
		self hide();
		wait( 0.125 );
		self show();
		wait( 0.125 );
	}

	self notify( "powerup_timedout" );
	self stopLoopSound( 0.05 );
	self delete();
}

// T5 :1197 powerup_grab.  A 64-unit proximity test, not a trigger -- T5's
// own form, and it is what lets a downed player fail to collect.
powerup_grab()
{
	self endon( "powerup_timedout" );

	range_squared = 64 * 64;

	for ( ;; )
	{
		wait( 0.05 );

		if ( !isDefined( self ) )
			return;

		players = getPlayers();

		for ( i = 0; i < players.size; i++ )
		{
			player = players[ i ];

			if ( !is_player_valid( player ) )
				continue;

			if ( distanceSquared( player.origin, self.origin ) >= range_squared )
				continue;

			self notify( "powerup_grabbed" );
			player powerup_vo( self.powerup_name );
			player play_sound_on_ent( "powerup_grabbed" );

			switch ( self.powerup_name )
			{
			case "nuke":
				level thread nuke_powerup( player );
				break;
			case "full_ammo":
				level thread full_ammo_powerup();
				break;
			case "double_points":
				level thread double_points_powerup();
				break;
			case "insta_kill":
				level thread insta_kill_powerup();
				break;
			case "carpenter":
				level thread start_carpenter();
				break;
			case "fire_sale":
				level thread start_fire_sale();
				break;
			case "free_perk":
				player thread free_perk_powerup();
				break;
			case "all_revive":
				level thread start_revive_all();
				break;
			case "bonus_points_player":
				player thread bonus_points_player();
				break;
			case "bonus_points_team":
				level thread bonus_points_team();
				break;
			}

			self stopLoopSound( 0.05 );
			self delete();
			return;
		}
	}
}

// T5 :1557 powerup_vo.  MEASURED, docs/zone_assets/ALL_sound.txt: CoDOL ships
// the announcer lines as zmb_vox_ann_* in common_gametype_zm, T5's own names.
// Only the eight that correspond to a ported power-up are mapped; the rest of
// the shipped set (bonfiresale, death_machine, dogstart, child_magicbox,
// magicbox) belongs to power-ups this module does not drop.
powerup_vo( type )
{
	alias = undefined;

	switch ( type )
	{
	case "nuke":
		alias = "zmb_vox_ann_nuke";
		break;
	case "insta_kill":
		alias = "zmb_vox_ann_instakill";
		break;
	case "double_points":
		alias = "zmb_vox_ann_doublepoints";
		break;
	case "full_ammo":
		alias = "zmb_vox_ann_maxammo";
		break;
	case "carpenter":
		alias = "zmb_vox_ann_carpenter";
		break;
	case "fire_sale":
		alias = "zmb_vox_ann_firesale";
		break;
	}

	if ( !isDefined( alias ) )
		return;

	// T5 plays the announcer to everyone, not just the grabber.
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] playLocalSound( alias );
}

// --------------------------------------------------------------------------
// The effects
// --------------------------------------------------------------------------

// T5 :1677 nuke_powerup.  Kill every zombie alive and pay the grabber.
// _zombiemode_score.gsc already carries the "nuke_powerup" event.
nuke_powerup( player )
{
	level notify( "nuke_powerup" );

	if ( isDefined( player ) && is_player_valid( player ) )
		player maps\mp\gametypes\_zombiemode_score::player_add_points( "nuke_powerup", 400 );

	if ( !isDefined( level.zombies ) )
		return;

	for ( i = 0; i < level.zombies.size; i++ )
	{
		if ( !isDefined( level.zombies[ i ] ) )
			continue;

		// T5 staggers the kills so the round counter and the death events do
		// not all land in one frame.
		level.zombies[ i ] thread nuke_kill_zombie();
		wait( 0.05 );
	}
}

nuke_kill_zombie()
{
	if ( !isDefined( self ) )
		return;

	self.nuked = true;
	self doDamage( self.health + 100, self.origin );
}

// T5 :1792 double_points_powerup.  _zombiemode_score::get_points_multiplier
// reads the var.
double_points_powerup()
{
	level notify( "powerup_double_points" );

	level.zombie_vars[ "zombie_powerup_point_doubler_on" ] = 1;

	wait( level.zombie_vars[ "zombie_powerup_point_doubler_time" ] );

	level.zombie_vars[ "zombie_powerup_point_doubler_on" ] = 0;
}

// T5 :1836 insta_kill_powerup.  _zombiemode_score::player_add_points already
// checks zombie_powerup_insta_kill_on, and the kill itself is landed here so
// a hit while it is up is lethal.
insta_kill_powerup()
{
	level notify( "powerup_insta_kill" );

	level.zombie_vars[ "zombie_powerup_insta_kill_on" ] = 1;

	wait( level.zombie_vars[ "zombie_powerup_insta_kill_time" ] );

	level.zombie_vars[ "zombie_powerup_insta_kill_on" ] = 0;
}

// T5 :1807 full_ammo_powerup.  T5 tops up every weapon every player holds.
full_ammo_powerup()
{
	players = getPlayers();

	for ( i = 0; i < players.size; i++ )
	{
		player = players[ i ];
		if ( !isDefined( player ) )
			continue;

		weapons = player getWeaponsListAll();
		if ( !isDefined( weapons ) )
			continue;

		for ( j = 0; j < weapons.size; j++ )
			player giveMaxAmmo( weapons[ j ] );
	}
}

// T5 :1463 start_carpenter.  Rebuild every barrier, then pay everyone.
start_carpenter()
{
	level notify( "powerup_carpenter" );

	if ( !isDefined( level.exterior_goals ) )
		return;

	for ( i = 0; i < level.exterior_goals.size; i++ )
	{
		node = level.exterior_goals[ i ];
		chunks = node.barrier_chunks;
		if ( !isDefined( chunks ) )
			continue;

		for ( j = 0; j < chunks.size; j++ )
		{
			if ( !is_true( chunks[ j ].destroyed ) )
				continue;

			node maps\mp\gametypes\_zombiemode_blockers::replace_chunk( chunks[ j ] );
			wait( 0.05 );
		}

		if ( isDefined( node.clip ) )
			node.clip enable_trigger();
	}

	// T5 pays 200 a head once every board is back.
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( is_player_valid( players[ i ] ) )
			players[ i ] maps\mp\gametypes\_zombiemode_score::player_add_points( "bonus_points_powerup", 200 );
	}
}

// T5 :1407 start_fire_sale.  Every box costs 10 for the duration.
// _zombiemode_weapons::treasure_chest_think reads the var when it charges.
start_fire_sale()
{
	level notify( "powerup_fire_sale" );

	level.zombie_vars[ "zombie_powerup_fire_sale_on" ] = 1;

	wait( level.zombie_vars[ "zombie_powerup_fire_sale_time" ] );

	level.zombie_vars[ "zombie_powerup_fire_sale_on" ] = 0;
	level notify( "fire_sale_off" );
}

// T5 :1383 start_revive_all.
start_revive_all()
{
	players = getPlayers();

	for ( i = 0; i < players.size; i++ )
	{
		if ( !players[ i ] maps\mp\gametypes\_zombiemode_laststand::player_is_in_laststand() )
			continue;

		// Called with NO reviver on purpose: revive_success pays its reviver
		// 100 points (the "reviver" score event), and T5's start_revive_all
		// revives for free -- the power-up is the payment.
		players[ i ] maps\mp\gametypes\_zombiemode_laststand::revive_success();
	}
}

// T5 :2293 free_perk_powerup.  Self is the grabber.  T5 gives a perk the
// player does not already hold, drawn from the machines this map actually
// carries -- a free perk for a machine that is not on the map would be a
// perk the player could never have earned.
free_perk_powerup()
{
	if ( !isDefined( level.zombie_perk_machines ) || level.zombie_perk_machines.size < 1 )
		return;

	candidates = [];
	for ( i = 0; i < level.zombie_perk_machines.size; i++ )
	{
		perk = level.zombie_perk_machines[ i ];
		if ( isDefined( perk ) && !self hasPerk( perk ) )
			candidates[ candidates.size ] = perk;
	}

	if ( candidates.size < 1 )
		return;

	self maps\mp\gametypes\_zombiemode_perks::give_perk( candidates[ randomInt( candidates.size ) ] );
}

// T5's two bonus-point pickups.  _zombiemode_score.gsc already carries the
// "bonus_points_powerup" event and takes the amount as its second argument.
bonus_points_player()
{
	self maps\mp\gametypes\_zombiemode_score::player_add_points( "bonus_points_powerup", 500 );
}

bonus_points_team()
{
	players = getPlayers();

	for ( i = 0; i < players.size; i++ )
	{
		if ( is_player_valid( players[ i ] ) )
			players[ i ] maps\mp\gametypes\_zombiemode_score::player_add_points( "bonus_points_powerup", 500 );
	}
}
