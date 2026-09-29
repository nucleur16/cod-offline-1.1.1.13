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
// THE FIVE WEAPON-BACKED PICKUPS (2026-09-24).  Previously excluded from the
// bag; each now has its module and joins the bag only if what it needs
// resolves in the mounted zones:
//   kis_minigun_zm -- T5 :2459 minigun_weapon_powerup and its countdown /
//     replace / remove / downed helpers, over C1's minigun_zm.
//   kis_tesla_zm   -- T5 :2643 tesla_weapon_powerup and helpers, over C1's
//     tesla_gun_zm; the chain lightning that does its damage is
//     _zombiemode_weap_tesla.gsc (T5 _zombiemode_weap_tesla.gsc).
//   kis_sentry_zm  -- _zombiemode_auto_turret.gsc (T5
//     _zombiemode_auto_turret.gsc behaviour on C1's sentry_minigun_mp).
//   kis_rcbomb_zm  -- _zombiemode_rcbomb.gsc (T5 multiplayer RC-XD on the
//     native rc_car_medium_mp).
//   kis_pandora_zm -- T5 _zombiemode_weap_quantum_bomb.gsc's weighted result
//     table plus the results T5 _zombiemode_powerups.gsc:151 registers into
//     it, restricted to results whose content exists here (see the PANDORA
//     section below for which are in and why the rest are not).
// T5 :425 keeps tesla (and free_perk, bonus points) out of random drops; the
// C1 catalogue lists every kis_* as a drop pickup (the reason this port
// already drops free_perk and the bonus points), so tesla is in the bag with
// T5's one-at-a-time rule instead.
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

	// The weapon-backed pickups' own modules, before the registry reads
	// what they resolved.
	maps\mp\gametypes\_zombiemode_weap_tesla::init();
	maps\mp\gametypes\_zombiemode_auto_turret::init();
	maps\mp\gametypes\_zombiemode_rcbomb::init();

	init_powerups();
	init_pandora();

	level thread powerup_test_watch();

	if ( level.zombie_powerup_array.size < 1 )
	{
		codolPrintLn( "CoDOL zm: no kis_* power-up resolved out of the mounted zones; power-ups are off" );
		return;
	}

	codolPrintLn( "CoDOL zm: " + level.zombie_powerup_array.size + " power-ups resolved out of the mounted zones" );

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

	// The five weapon-backed pickups, each gated on its own content.
	precacheItem( "minigun_zm" );
	if ( codol_isWeaponMounted( "minigun_zm" ) )
		add_zombie_powerup( "minigun", "kis_minigun_zm" );

	if ( is_true( level.zm_tesla_ok ) )
		add_zombie_powerup( "tesla", "kis_tesla_zm" );

	if ( is_true( level.zm_sentry_ok ) )
		add_zombie_powerup( "sentry", "kis_sentry_zm" );

	if ( is_true( level.zm_rcbomb_ok ) )
		add_zombie_powerup( "rcbomb", "kis_rcbomb_zm" );

	add_zombie_powerup( "pandora", "kis_pandora_zm" );
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

		// T5 :460 minigun_no_drop, first half: not while anyone holds one.
		// (Its second half -- no drop before the power is on in solo --
		// reads T5's solo-lives bookkeeping, which this port does not keep.)
		if ( name == "minigun" && any_player_has( "zombie_powerup_minigun_on" ) )
			continue;

		// T5 :919 / :2843 tesla_powerup_active: one tesla at a time.
		if ( name == "tesla" && any_player_has( "zombie_powerup_tesla_on" ) )
			continue;

		// T5 _zombiemode_auto_turret.gsc:188 keeps at most two turrets up.
		if ( name == "sentry" && level.curr_auto_turrets_active >= level.max_auto_turrets_active )
			continue;

		return name;
	}

	return level.zombie_powerup_array[ 0 ];
}

// T5 keeps these in player.zombie_vars[]; this port's players have no such
// array, so the per-player power-up state lives in player.zm_powerup_state.
any_player_has( field )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] powerup_state( field ) )
			return true;
	}
	return false;
}

powerup_state( field )
{
	if ( !isDefined( self.zm_powerup_state ) )
		return false;
	return is_true( self.zm_powerup_state[ field ] );
}

set_powerup_state( field, value )
{
	if ( !isDefined( self.zm_powerup_state ) )
		self.zm_powerup_state = [];
	self.zm_powerup_state[ field ] = value;
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

powerup_drop( drop_point, outside_playable_area )
{
	if ( !isDefined( level.zombie_powerup_array ) || level.zombie_powerup_array.size < 1 )
		return;

	// Zombies spawn outside the playable area and keep ignoreme until
	// zombie_think finishes their first entrance.  A kill there must not
	// consume the armed drop or round quota for an unreachable pickup.
	if ( is_true( outside_playable_area ) )
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
	{
		name = powerup_override;

		// T5 :919: only one tesla at a time, give a minigun instead.
		if ( name == "tesla" && any_player_has( "zombie_powerup_tesla_on" ) && isDefined( level.zombie_powerups[ "minigun" ] ) )
			name = "minigun";
	}
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

			// T5 :1215: no weapon power-up for a player who is reviving (a
			// downed one already fails is_player_valid); the RC car is a
			// ride of the same kind.
			if ( is_weapon_powerup( self.powerup_name ) && player useButtonPressed() && player in_revive_trigger() )
				continue;

			if ( distanceSquared( player.origin, self.origin ) >= range_squared )
				continue;

			self notify( "powerup_grabbed" );
			codolPrintLn( "CoDOL zm powerup: " + self.powerup_name + " grabbed by " + player.name + " at " + self.origin );
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
			case "minigun":
				level thread minigun_weapon_powerup( player );
				break;
			case "tesla":
				level thread tesla_weapon_powerup( player );
				break;
			case "sentry":
				player thread maps\mp\gametypes\_zombiemode_auto_turret::sentry_powerup( self.origin );
				break;
			case "rcbomb":
				player thread maps\mp\gametypes\_zombiemode_rcbomb::rcbomb_powerup();
				break;
			case "pandora":
				player thread pandora_powerup( self.origin );
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
// Only the lines that correspond to a ported power-up are mapped; the rest of
// the shipped set (bonfiresale, dogstart, child_magicbox, magicbox) belongs
// to power-ups this module does not drop.
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
	case "minigun":
		// T5's announcer calls the minigun the Death Machine; CoDOL ships
		// that line as zmb_vox_ann_death_machine (localized_common_gametype_zm).
		alias = "zmb_vox_ann_death_machine";
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

// ==========================================================================
// WEAPON POWER-UPS.  T5 :2459 minigun_weapon_powerup and :2643
// tesla_weapon_powerup, with their countdown / replace / remove / downed
// helpers folded into one pair shared by both weapons (T5's two copies differ
// only in the weapon name, the per-player state names and the end condition).
//
// C1 interpretation, stated:
//   * Per-player state lives in player.zm_powerup_state[] (see
//     set_powerup_state) under T5's own zombie_vars key names.
//   * T5's increment_is_drinking / _show_solo_hud and the solo HUD icon
//     (:180/:257, zom_icon_minigun) are not taken: this port has no drinking
//     counter and no power-up HUD overlay (header).
//   * THE DOWN.  T5 routes it through level._zombie_*_powerup_last_stand_func,
//     called from _laststand before it stores the player's weapons.  This
//     port's _zombiemode_laststand::player_laststand stores them FIRST
//     (laststand_disable_player_weapons) and notifies "player_downed" after,
//     so the watcher below takes the power-up weapon back out of the stored
//     list, which is the same outcome with no hook in the shared file.
//   * setClientSysState( "levelNotify", "minis"/"minie" ) is T5 client-script
//     audio; the builtin is not registered and C1 has no client script.
// ==========================================================================

is_weapon_powerup( name )
{
	return ( name == "minigun" || name == "tesla" || name == "rcbomb" );
}

weapon_powerup_names( kind )
{
	s = spawnStruct();
	if ( kind == "minigun" )
	{
		s.weapon = "minigun_zm";
		s.on_field = "zombie_powerup_minigun_on";
		s.time_field = "zombie_powerup_minigun_time";
		s.over_notify = "minigun_time_over";
		s.default_time = 30;    // T5 :2465
	}
	else
	{
		s.weapon = "tesla_gun_zm";
		s.on_field = "zombie_powerup_tesla_on";
		s.time_field = "zombie_powerup_tesla_time";
		s.over_notify = "tesla_time_over";
		s.default_time = 11;    // T5 :2651, "no blink"
	}
	s.kind = kind;
	return s;
}

minigun_weapon_powerup( ent_player, time )
{
	weapon_powerup_give( ent_player, "minigun", time );
}

tesla_weapon_powerup( ent_player, time )
{
	weapon_powerup_give( ent_player, "tesla", time );
}

// T5 :2459 / :2643.
weapon_powerup_give( ent_player, kind, time )
{
	ent_player endon( "disconnect" );
	ent_player endon( "death" );
	ent_player endon( "player_downed" );

	w = weapon_powerup_names( kind );
	if ( !isDefined( time ) )
		time = w.default_time;

	if ( !isDefined( ent_player.zm_powerup_weapon_held ) )
		ent_player.zm_powerup_weapon_held = [];
	if ( !isDefined( ent_player.zm_powerup_time ) )
		ent_player.zm_powerup_time = [];
	if ( !isDefined( ent_player.zm_powerup_gun_before ) )
		ent_player.zm_powerup_gun_before = [];

	// T5 :2471 / :2655: just replenish if it is already running.
	if ( ent_player powerup_state( w.on_field ) &&
		( ent_player getCurrentWeapon() == w.weapon || is_true( ent_player.zm_powerup_weapon_held[ kind ] ) ) )
	{
		if ( kind == "tesla" )
			ent_player giveMaxAmmo( w.weapon );
		if ( !isDefined( ent_player.zm_powerup_time[ kind ] ) || ent_player.zm_powerup_time[ kind ] < time )
			ent_player.zm_powerup_time[ kind ] = time;
		codolPrintLn( "CoDOL zm powerup: " + kind + " topped up for " + ent_player.name );
		return;
	}

	// Ends the other weapon power-up, if any (T5's replace_weapon_powerup).
	ent_player notify( "replace_weapon_powerup" );

	ent_player.zm_powerup_weapon_held[ kind ] = true;
	ent_player.has_powerup_weapon = true;

	before = ent_player getCurrentWeapon();
	ent_player.zm_powerup_gun_before[ kind ] = before;

	ent_player giveWeapon( w.weapon );
	if ( kind == "tesla" )
		ent_player giveMaxAmmo( w.weapon );
	ent_player switchToWeapon( w.weapon );

	ent_player set_powerup_state( w.on_field, true );

	codolPrintLn( "CoDOL zm powerup: " + w.weapon + " given to " + ent_player.name + " (was " + before +
		"), clip " + ent_player getWeaponAmmoClip( w.weapon ) + " stock " + ent_player getWeaponAmmoStock( w.weapon ) );

	level thread weapon_powerup_downed( ent_player, w );
	level thread weapon_powerup_replace( ent_player, w );

	if ( kind == "minigun" )
		level thread minigun_weapon_powerup_countdown( ent_player, w, time );
	else
		level thread tesla_weapon_powerup_countdown( ent_player, w, time );
}

// T5 :2502.  Thirty seconds of minigun.
minigun_weapon_powerup_countdown( ent_player, w, time )
{
	ent_player endon( "death" );
	ent_player endon( "disconnect" );
	ent_player endon( "player_downed" );
	ent_player endon( w.over_notify );
	ent_player endon( "replace_weapon_powerup" );

	ent_player.zm_powerup_time[ w.kind ] = time;
	while ( ent_player.zm_powerup_time[ w.kind ] > 0 )
	{
		wait( 1.0 );
		ent_player.zm_powerup_time[ w.kind ]--;
	}

	level thread weapon_powerup_remove( ent_player, w, "time over" );
}

// T5 :2669.  The tesla lasts until its ammunition does.  T5 re-checks on
// weapon_fired / reload / zmb_max_ammo; the check is polled here, which
// reaches the same end state without depending on those notifies.
tesla_weapon_powerup_countdown( ent_player, w, time )
{
	ent_player endon( "death" );
	ent_player endon( "disconnect" );
	ent_player endon( "player_downed" );
	ent_player endon( w.over_notify );
	ent_player endon( "replace_weapon_powerup" );

	ent_player.zm_powerup_time[ w.kind ] = time;

	// Let the raise finish before an empty clip can end it.
	wait( 1.0 );

	for ( ;; )
	{
		wait( 0.1 );

		if ( !ent_player hasWeapon( w.weapon ) )
			break;

		if ( ent_player getWeaponAmmoStock( w.weapon ) )
		{
			ent_player.zm_powerup_time[ w.kind ] = 11;   // no blink
			continue;
		}

		clip_count = ent_player getWeaponAmmoClip( w.weapon );
		if ( !clip_count )
			break;
		else if ( clip_count == 1 )
			ent_player.zm_powerup_time[ w.kind ] = 1;    // blink fast
		else if ( clip_count == 3 )
			ent_player.zm_powerup_time[ w.kind ] = 6;    // blink
	}

	// T5 :2688 lets the last shot land before the switch-back.
	wait( 1.0 );

	level thread weapon_powerup_remove( ent_player, w, "out of ammunition" );
}

// T5 :2528 / :2712.  Another weapon power-up took this one's place.
weapon_powerup_replace( ent_player, w )
{
	ent_player endon( "death" );
	ent_player endon( "disconnect" );
	ent_player endon( "player_downed" );
	ent_player endon( w.over_notify );

	ent_player waittill( "replace_weapon_powerup" );

	ent_player takeWeapon( w.weapon );
	ent_player set_powerup_state( w.on_field, false );
	ent_player.zm_powerup_weapon_held[ w.kind ] = false;

	codolPrintLn( "CoDOL zm powerup: " + w.weapon + " replaced for " + ent_player.name );
}

// T5 :2547 / :2731.  Take it back and hand the player their gun.
weapon_powerup_remove( ent_player, w, reason )
{
	ent_player endon( "death" );
	ent_player endon( "player_downed" );
	ent_player endon( "disconnect" );

	ent_player takeWeapon( w.weapon );

	ent_player set_powerup_state( w.on_field, false );
	ent_player.zm_powerup_weapon_held[ w.kind ] = false;
	ent_player.has_powerup_weapon = false;

	ent_player notify( w.over_notify );

	restored = weapon_powerup_restore( ent_player, ent_player.zm_powerup_gun_before[ w.kind ] );

	codolPrintLn( "CoDOL zm powerup: " + w.weapon + " taken from " + ent_player.name + " (" + reason +
		"); switched back to " + restored );
}

// T5 :2566-:2597.  The gun held before, else the first primary.  T5's last
// resort, a melee weapon, does not apply: this port gives no melee weapon.
weapon_powerup_restore( ent_player, before )
{
	primaries = ent_player getWeaponsListPrimaries();

	if ( isDefined( before ) )
	{
		for ( i = 0; i < primaries.size; i++ )
		{
			if ( primaries[ i ] == before )
			{
				ent_player switchToWeapon( before );
				return before;
			}
		}
	}

	if ( primaries.size > 0 )
	{
		ent_player switchToWeapon( primaries[ 0 ] );
		return primaries[ 0 ];
	}

	return "none";
}

// T5 :2605 minigun_watch_gunner_downed / :2789 tesla_watch_gunner_downed.
// See WEAPON POWER-UPS above for why this edits the stored list.
weapon_powerup_downed( ent_player, w )
{
	ent_player endon( "disconnect" );
	ent_player endon( w.over_notify );
	ent_player endon( "replace_weapon_powerup" );

	ent_player waittill( "player_downed" );

	if ( ent_player hasWeapon( w.weapon ) )
		ent_player takeWeapon( w.weapon );

	if ( isDefined( ent_player.laststand_weapons ) )
	{
		kept = [];
		for ( i = 0; i < ent_player.laststand_weapons.size; i++ )
		{
			if ( ent_player.laststand_weapons[ i ] != w.weapon )
				kept[ kept.size ] = ent_player.laststand_weapons[ i ];
		}
		ent_player.laststand_weapons = kept;
	}

	if ( isDefined( ent_player.laststand_weapon_current ) && ent_player.laststand_weapon_current == w.weapon )
		ent_player.laststand_weapon_current = ent_player.zm_powerup_gun_before[ w.kind ];

	ent_player set_powerup_state( w.on_field, false );
	ent_player.zm_powerup_weapon_held[ w.kind ] = false;
	ent_player.has_powerup_weapon = false;

	codolPrintLn( "CoDOL zm powerup: " + w.weapon + " taken from " + ent_player.name + " (downed)" );
}

// ==========================================================================
// PANDORA.  kis_pandora_zm, the quantum bomb as a pickup: the grab rolls one
// result out of T5's weighted table at the pickup's position, with the
// grabber as self.  T5 _zombiemode_weap_quantum_bomb.gsc (qb below) :105
// quantum_bomb_register_result and :192 quantum_bomb_select_result, unchanged:
// every result whose chance beats a roll of RandomInt( 100 ) and whose
// validation passes is eligible, and one eligible result is drawn uniformly.
//
// REGISTERED (T5 file:line, chance):
//   random_powerup                    powerups :153/:726, 5
//   random_bonus_or_lose_points_powerup powerups :156/:825, 25 -- CoDOL ships
//        no lose_points pickup, so T5's own fall-through picks bonus points.
//   random_weapon_starburst           qb :15/:326, 75 -- ray_gun_zm and
//        ray_gun_upgraded_zm ship in common_gametype_zm; T5's other four
//        candidates do not, and T5's own switch falls through to ray_gun_zm.
//   pack_or_unpack_current_weapon     qb :16/:442, 10
//   auto_revive                       qb :17/:539, 60
//   zombie_speed_buff                 qb :24/:710, 2
//   zombie_add_to_total               qb :25/:837, 70
//   zombie_fling                      qb :29/:763, 100 (T5's default)
// NOT REGISTERED, with the reason:
//   random_lethal_grenade (qb :14) -- MagicGrenadeType is not registered.
//   player_teleport (qb :18) -- needs struct_black_hole_teleport structs and
//        T5's zone manager (get_current_zone); neither exists here.
//   random_weapon_powerup (powerups :155) -- the random_weapon pickup is not
//        ported (see the header).
//   random_zombie_grab_powerup (powerups :154) -- no zombie-grabbable pickup.
// C1 interpretation: zombie_fling kills with doDamage and cannot fling the
// corpse (LaunchRagdoll is not registered); the starburst fires from 20 units
// in front of the floating weapon model instead of its tag_flash (GetTagOrigin
// raises a script error for a missing tag); create_and_play_dialog lines are
// dropped (_zombiemode_audio is not ported).
// ==========================================================================

init_pandora()
{
	level.quantum_bomb_results = [];

	// All five ship in common_gametype_zm
	// (docs/CODOL_ZM_EQUIPMENT_EVIDENCE_20260924.md section 4).
	level._effect[ "quantum_bomb_area_effect" ]             = loadfx( "weapon/quantum_bomb/area_effect" );
	level._effect[ "quantum_bomb_player_effect" ]           = loadfx( "weapon/quantum_bomb/player_effect" );
	level._effect[ "quantum_bomb_player_position_effect" ]  = loadfx( "weapon/quantum_bomb/fx_player_position_effect" );
	level._effect[ "quantum_bomb_mystery_effect" ]          = loadfx( "weapon/quantum_bomb/fx_mystery_effect" );
	level._effect[ "zombie_fling_result" ]                  = loadfx( "maps/zombie_moon/fx_moon_qbomb_explo_distort" );

	quantum_bomb_register_result( "random_powerup", ::qb_random_powerup_result, 5 );
	if ( isDefined( level.zombie_powerups[ "bonus_points_team" ] ) )
		quantum_bomb_register_result( "random_bonus_or_lose_points_powerup", ::qb_random_bonus_or_lose_points_powerup_result, 25 );

	// The floating gun is a script_model, so its world model is precached.
	// MEASURED 2026-09-24 (probe w5): this server's MagicBullet raises
	// "Unhandled projectile weapClass" for ray_gun_zm, a projectile weapon
	// of a class other than rocket launcher / turret
	// (src/codol/script/codol_script_magicbullet.c), and a script error
	// shuts the server down.  The result is registered only for a weapon
	// the native builtin can fire; see qb_starburst_weapon_fireable.
	precacheItem( "ray_gun_zm" );
	precacheItem( "ray_gun_upgraded_zm" );
	if ( codol_isWeaponMounted( "ray_gun_zm" ) && qb_starburst_weapon_fireable( "ray_gun_zm" ) )
	{
		precacheModel( getWeaponModel( "ray_gun_zm" ) );
		if ( codol_isWeaponMounted( "ray_gun_upgraded_zm" ) )
			precacheModel( getWeaponModel( "ray_gun_upgraded_zm" ) );
		quantum_bomb_register_result( "random_weapon_starburst", ::qb_random_weapon_starburst_result, 75 );
	}

	quantum_bomb_register_result( "pack_or_unpack_current_weapon", ::qb_pack_or_unpack_current_weapon_result, 10, ::qb_pack_or_unpack_current_weapon_validation );
	quantum_bomb_register_result( "auto_revive", ::qb_auto_revive_result, 60, ::qb_auto_revive_validation );
	quantum_bomb_register_result( "zombie_speed_buff", ::qb_zombie_speed_buff_result, 2 );
	quantum_bomb_register_result( "zombie_add_to_total", ::qb_zombie_add_to_total_result, 70, ::qb_zombie_add_to_total_validation );
	quantum_bomb_register_result( "zombie_fling", ::qb_zombie_fling_result );

	keys = getArrayKeys( level.quantum_bomb_results );
	names = "";
	for ( i = 0; i < keys.size; i++ )
		names = names + " " + keys[ i ];
	ray = "not mounted";
	if ( codol_isWeaponMounted( "ray_gun_zm" ) )
		ray = weaponType( "ray_gun_zm" ) + "/" + weaponClass( "ray_gun_zm" );
	codolPrintLn( "CoDOL zm pandora: " + keys.size + " results registered:" + names + " (ray_gun_zm " + ray + ")" );
}

// qb :105.
quantum_bomb_register_result( name, result_func, chance, validation_func )
{
	result = spawnStruct();
	result.name = name;
	result.result_func = result_func;

	if ( !isDefined( chance ) )
		result.chance = 100;
	else
		result.chance = clamp( chance, 1, 100 );

	if ( !isDefined( validation_func ) )
		result.validation_func = ::qb_default_validation;
	else
		result.validation_func = validation_func;

	level.quantum_bomb_results[ name ] = result;
}

qb_default_validation( position )
{
	return true;
}

// qb :192.  self is the grabber.
quantum_bomb_select_result( position )
{
	level.quantum_bomb_cached_closest_zombies = undefined;

	eligible = [];
	chance = randomInt( 100 );
	keys = getArrayKeys( level.quantum_bomb_results );
	for ( i = 0; i < keys.size; i++ )
	{
		result = level.quantum_bomb_results[ keys[ i ] ];
		if ( result.chance > chance && self [[ result.validation_func ]]( position ) )
			eligible[ eligible.size ] = result.name;
	}

	// zombie_fling (chance 100, no validation) is always eligible, exactly as
	// in T5; the guard only protects against a table edited to lose it.
	if ( eligible.size < 1 )
		return undefined;

	return level.quantum_bomb_results[ eligible[ randomInt( eligible.size ) ] ];
}

// The grab.  self is the grabber.  qb :249-:253, the explode branch.
pandora_powerup( position, forced )
{
	self endon( "disconnect" );

	playSoundAtPosition( "wpn_quantum_exp", position );

	result = undefined;
	if ( isDefined( forced ) && isDefined( level.quantum_bomb_results[ forced ] ) )
		result = level.quantum_bomb_results[ forced ];
	else
		result = self quantum_bomb_select_result( position );

	if ( !isDefined( result ) )
		return;

	codolPrintLn( "CoDOL zm pandora: " + self.name + " rolled '" + result.name + "' at " + position );

	self thread [[ result.result_func ]]( position );
}

qb_get_cached_closest_zombies( position )
{
	if ( !isDefined( level.quantum_bomb_cached_closest_zombies ) )
		level.quantum_bomb_cached_closest_zombies = maps\mp\gametypes\_zombiemode_weap_tesla::tesla_sorted_zombies( position );
	return level.quantum_bomb_cached_closest_zombies;
}

// T5 _zombiemode_powerups.gsc:726.
qb_random_powerup_result( position )
{
	keys = getArrayKeys( level.zombie_powerups );
	while ( keys.size )
	{
		index = randomInt( keys.size );
		name = keys[ index ];
		skip = false;

		switch ( name )
		{
		case "bonus_points_player":
		case "bonus_points_team":
		case "pandora":   // a pandora rolling itself would be T5's random_weapon case: skipped
			skip = true;
			break;
		case "full_ammo":
		case "insta_kill":
		case "fire_sale":
		case "minigun":
			if ( randomInt( 4 ) )   // only 25% chance
				skip = true;
			break;
		case "free_perk":
		case "tesla":
			if ( randomInt( 20 ) )  // only 5% chance
				skip = true;
			break;
		}

		if ( skip )
		{
			keys = array_remove_value( keys, name );
			continue;
		}

		playFx( level._effect[ "quantum_bomb_player_position_effect" ], position );
		codolPrintLn( "CoDOL zm pandora: drops " + name );
		specific_powerup_drop( name, position );
		return;
	}
}

array_remove_value( array, value )
{
	out = [];
	for ( i = 0; i < array.size; i++ )
	{
		if ( array[ i ] != value )
			out[ out.size ] = array[ i ];
	}
	return out;
}

// T5 _zombiemode_powerups.gsc:825.
qb_random_bonus_or_lose_points_powerup_result( position )
{
	powerup = "bonus_points_team";
	rand = randomInt( 10 );
	// T5 cases 0-1 are lose_points_team, which CoDOL does not ship; T5's own
	// fall-through then lands on bonus_points_player (cases 2-4).
	if ( rand < 5 && isDefined( level.zombie_powerups[ "bonus_points_player" ] ) )
		powerup = "bonus_points_player";

	playFx( level._effect[ "quantum_bomb_player_position_effect" ], position );
	codolPrintLn( "CoDOL zm pandora: drops " + powerup );
	specific_powerup_drop( powerup, position );
}

// The weapon types CODOL_Scr_MagicBullet fires: bullets, and projectiles of
// the rocket-launcher or turret class.
qb_starburst_weapon_fireable( weapon )
{
	type = weaponType( weapon );
	if ( type == "bullet" )
		return true;
	if ( type != "projectile" )
		return false;
	wclass = weaponClass( weapon );
	return ( wclass == "rocketlauncher" || wclass == "turret" );
}

// qb :326.  A floating ray gun spins once, firing 36 shots.
qb_random_weapon_starburst_result( position )
{
	weapon = "ray_gun_zm";
	rand = randomInt( 20 );
	if ( rand < 3 && codol_isWeaponMounted( "ray_gun_upgraded_zm" ) && qb_starburst_weapon_fireable( "ray_gun_upgraded_zm" ) )
		weapon = "ray_gun_upgraded_zm";

	playFx( level._effect[ "quantum_bomb_player_position_effect" ], position );

	base_pos = position + ( 0, 0, 40 );
	start_yaw = vectorToAngles( base_pos - self.origin );
	start_yaw = ( 0, start_yaw[ 1 ], 0 );

	weapon_model = spawn( "script_model", position );
	weapon_model.angles = start_yaw;
	weapon_model setModel( getWeaponModel( weapon ) );

	weapon_model moveTo( base_pos, 1, 0.25, 0.25 );
	weapon_model waittill( "movedone" );

	for ( i = 0; i < 36; i++ )
	{
		yaw = start_yaw + ( randomIntRange( -3, 3 ), i * 10, 0 );
		weapon_model.angles = yaw;
		forward = anglesToForward( yaw );
		flash_pos = weapon_model.origin + vector_scale( forward, 20 );
		target_pos = flash_pos + vector_scale( forward, 40 );

		if ( isDefined( self ) )
			magicBullet( weapon, flash_pos, target_pos, self );
		else
			magicBullet( weapon, flash_pos, target_pos );

		wait_network_frame();
	}

	codolPrintLn( "CoDOL zm pandora: starburst of " + weapon + " done" );
	weapon_model delete();
}

// qb :419.
qb_pack_or_unpack_current_weapon_validation( position )
{
	pack_triggers = getEntArray( "zombie_vending_upgrade", "targetname" );

	range_squared = 180 * 180;
	for ( i = 0; i < pack_triggers.size; i++ )
	{
		if ( distanceSquared( pack_triggers[ i ].origin, position ) < range_squared )
			return true;
	}

	// If not near a machine, only a 1 in 5 chance.
	return !randomInt( 5 );
}

// qb :442.
qb_pack_or_unpack_current_weapon_result( position )
{
	playFx( level._effect[ "quantum_bomb_mystery_effect" ], position );

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		player = players[ i ];
		if ( !is_player_valid( player ) )
			continue;

		// T5 :457 requires a primary that the weapon table knows.  Here an
		// upgraded weapon is not a key of level.zombie_weapons (it is a base
		// weapon's upgrade_name), so both forms are accepted, and the name is
		// checked before weaponInventoryType sees it.
		weapon = player getCurrentWeapon();
		if ( !isDefined( weapon ) || weapon == "none" )
			continue;
		if ( !isDefined( level.zombie_weapons[ weapon ] ) &&
			!maps\mp\gametypes\_zombiemode_weapons::is_weapon_upgraded( weapon ) )
			continue;
		if ( weaponInventoryType( weapon ) != "primary" )
			continue;

		if ( maps\mp\gametypes\_zombiemode_weapons::is_weapon_upgraded( weapon ) )
		{
			if ( randomInt( 5 ) )   // only 20% chance of losing the packed weapon
				continue;

			keys = getArrayKeys( level.zombie_weapons );
			for ( k = 0; k < keys.size; k++ )
			{
				up = level.zombie_weapons[ keys[ k ] ].upgrade_name;
				if ( isDefined( up ) && up == weapon )
				{
					player takeWeapon( weapon );
					player maps\mp\gametypes\_zombiemode_weapons::weapon_give( keys[ k ] );
					playFx( level._effect[ "quantum_bomb_player_effect" ], player.origin );
					codolPrintLn( "CoDOL zm pandora: unpacked " + weapon + " for " + player.name );
					break;
				}
			}
		}
		else
		{
			up = maps\mp\gametypes\_zombiemode_weapons::get_upgrade_name( weapon );
			if ( !isDefined( up ) )
				continue;

			if ( !randomInt( 4 ) )  // 75% chance
				continue;

			// T5 :499: under the limit, weapon_give would not take the
			// current weapon, so take it here.
			primaries = player getWeaponsListPrimaries();
			if ( primaries.size < player maps\mp\gametypes\_zombiemode_weapons::player_weapon_limit() )
				player takeWeapon( weapon );

			player maps\mp\gametypes\_zombiemode_weapons::weapon_give( up );
			playFx( level._effect[ "quantum_bomb_player_effect" ], player.origin );
			codolPrintLn( "CoDOL zm pandora: packed " + weapon + " into " + up + " for " + player.name );
		}
	}
}

// qb :517.  T5 refuses in a solo game; the solo test here is the player count.
qb_auto_revive_validation( position )
{
	if ( getPlayers().size < 2 )
		return false;

	return ( maps\mp\gametypes\_zombiemode_laststand::player_num_in_laststand() > 0 );
}

// qb :539.  T5's auto_revive( self ) credits the thrower as reviver.
qb_auto_revive_result( position )
{
	playFx( level._effect[ "quantum_bomb_mystery_effect" ], position );

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		player = players[ i ];
		if ( player maps\mp\gametypes\_zombiemode_laststand::player_is_in_laststand() && randomInt( 3 ) )  // 66%
		{
			player maps\mp\gametypes\_zombiemode_laststand::revive_success( self );
			playFx( level._effect[ "quantum_bomb_player_effect" ], player.origin );
			codolPrintLn( "CoDOL zm pandora: revived " + player.name );
		}
	}
}

// qb :710.  T5 swaps in sprint5/sprint6 clips; this port's own
// set_zombie_run_cycle( "sprint" ) is the fastest bucket it registers.
qb_zombie_speed_buff_result( position )
{
	playFx( level._effect[ "quantum_bomb_mystery_effect" ], position );

	zombies = qb_get_cached_closest_zombies( position );
	count = 0;
	for ( i = 0; i < zombies.size; i++ )
	{
		zombie = zombies[ i ];
		if ( !isDefined( zombie ) || !isAlive( zombie ) )
			continue;

		zombie maps\mp\gametypes\_zombiemode_spawner::set_zombie_run_cycle( "sprint" );
		zombie.needs_run_update = false;
		zombie maps\mp\gametypes\_zombiemode_spawner::apply_move_speed_scale();
		count++;
	}

	codolPrintLn( "CoDOL zm pandora: " + count + " zombies sprint" );
}

// qb :825.
qb_zombie_add_to_total_validation( position )
{
	if ( level.zombie_total )
		return false;

	zombies = qb_get_cached_closest_zombies( position );
	return ( zombies.size < level.zombie_ai_limit );
}

// qb :837.
qb_zombie_add_to_total_result( position )
{
	playFx( level._effect[ "quantum_bomb_mystery_effect" ], position );

	level.zombie_total += level.zombie_ai_limit;

	codolPrintLn( "CoDOL zm pandora: " + level.zombie_ai_limit + " more zombies this round" );
}

// qb :763.  Everything within 300 units dies, closest first.
qb_zombie_fling_result( position )
{
	playFx( level._effect[ "zombie_fling_result" ], position );

	range = 300;
	range_squared = range * range;
	killed = 0;

	zombies = qb_get_cached_closest_zombies( position );
	for ( i = 0; i < zombies.size; i++ )
	{
		zombie = zombies[ i ];
		if ( !isDefined( zombie ) || !isAlive( zombie ) )
			continue;

		test_origin = zombie.origin + ( 0, 0, 40 );
		if ( distanceSquared( position, test_origin ) > range_squared )
			break;

		if ( isDefined( self ) )
			zombie doDamage( zombie.health + 666, position, self );
		else
			zombie doDamage( zombie.health + 666, position );
		killed++;

		if ( i && !( i % 10 ) )
		{
			wait_network_frame();
			wait_network_frame();
			wait_network_frame();
		}
	}

	codolPrintLn( "CoDOL zm pandora: fling killed " + killed + " zombies" );
}

// ==========================================================================
// TEST HOOK, idle unless the dvar is set.  No C1 client is needed:
//   set scr_zm_powerup_test "drop <clientnum> <powerup>"
//       drops <powerup> at the client's feet, where the proximity grab takes it.
//   set scr_zm_powerup_test "pandora <clientnum> <result>"
//       runs one named pandora result at the client, bypassing the roll.
//   set scr_zm_powerup_test "fire <clientnum> <seconds> [weapon]"
//       turns the client to the nearest zombie and holds the fire button
//       (codol_botPressButtons), re-aiming every 0.1 s.  With a weapon the
//       client owns, setSpawnWeapon puts it in hand first: a test client
//       that is not a combat bot sends no usercmd while no button is held,
//       so a switchToWeapon on it is not carried out on its own.
//   set scr_zm_powerup_test "zap <clientnum> x"
//       a tesla_gun_zm projectile hit (doDamage, MOD_PROJECTILE) on the
//       nearest zombie, through the native damage path.
//   set scr_zm_powerup_test "state <clientnum> x" / "sentrydiag <clientnum> x"
//       log the client's held weapon / every active sentry's view.
//   set scr_zm_powerup_test "gather <clientnum> <radius>"
//       forceTeleports every live zombie onto a ring around the client, at
//       points the client can see, so a test does not wait on pathing.
// ==========================================================================

powerup_test_watch()
{
	setDvar( "scr_zm_powerup_test", "" );

	for ( ;; )
	{
		wait( 0.5 );

		request = getDvar( "scr_zm_powerup_test" );
		if ( request == "" )
			continue;
		setDvar( "scr_zm_powerup_test", "" );

		tokens = strTok( request, " " );
		if ( tokens.size < 3 )
			continue;

		player = powerup_test_player( int( tokens[ 1 ] ) );
		if ( !isDefined( player ) || !isAlive( player ) )
		{
			codolPrintLn( "CoDOL zm powerup test: no live client " + tokens[ 1 ] );
			continue;
		}

		switch ( tokens[ 0 ] )
		{
		case "drop":
			if ( !isDefined( level.zombie_powerups[ tokens[ 2 ] ] ) )
			{
				codolPrintLn( "CoDOL zm powerup test: " + tokens[ 2 ] + " is not registered" );
				break;
			}
			codolPrintLn( "CoDOL zm powerup test: dropping " + tokens[ 2 ] + " at " + player.name );
			specific_powerup_drop( tokens[ 2 ], player.origin - ( 0, 0, 40 ) );
			break;
		case "pandora":
			if ( !isDefined( level.quantum_bomb_results[ tokens[ 2 ] ] ) )
			{
				codolPrintLn( "CoDOL zm powerup test: pandora result " + tokens[ 2 ] + " is not registered" );
				break;
			}
			player thread pandora_powerup( player.origin, tokens[ 2 ] );
			break;
		case "fire":
			if ( tokens.size > 3 && player hasWeapon( tokens[ 3 ] ) )
				player setSpawnWeapon( tokens[ 3 ] );
			player thread powerup_test_fire( float( tokens[ 2 ] ) );
			break;
		case "state":
			w = player getCurrentWeapon();
			codolPrintLn( "CoDOL zm powerup test: " + player.name + " holds " + w + ", primaries " + player getWeaponsListPrimaries().size +
				", remote " + player isRemoteControlling() );
			break;
		case "zap":
			// A tesla_gun_zm projectile impact on the nearest zombie, delivered
			// through the native G_Damage path (doDamage with the weapon and
			// MOD_PROJECTILE), so the actor "damage" notify and the spawner's
			// tesla hook run exactly as for a real hit.
			z = get_closest( player.origin, maps\mp\gametypes\_zombiemode_weap_tesla::tesla_sorted_zombies( player.origin ) );
			if ( !isDefined( z ) )
			{
				codolPrintLn( "CoDOL zm powerup test: no zombie to zap" );
				break;
			}
			codolPrintLn( "CoDOL zm powerup test: tesla projectile hit on zombie " + z getEntityNumber() + " from " + player.name );
			z doDamage( 1, player.origin, player, player, 0, "MOD_PROJECTILE", 0, "tesla_gun_zm" );
			break;
		case "sentrydiag":
			if ( isDefined( level.zm_sentries ) )
			{
				for ( k = 0; k < level.zm_sentries.size; k++ )
					level.zm_sentries[ k ] maps\mp\gametypes\_zombiemode_auto_turret::auto_turret_diag( level.zm_sentries[ k ] getTurretTarget( false ) );
			}
			break;
		case "gather":
			player powerup_test_gather( float( tokens[ 2 ] ) );
			break;
		}
	}
}

powerup_test_player( client_num )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] getEntityNumber() == client_num )
			return players[ i ];
	}
	return undefined;
}

powerup_test_fire( seconds )
{
	self endon( "disconnect" );

	weapon = self getCurrentWeapon();
	codolPrintLn( "CoDOL zm powerup test: " + self.name + " fires " + weapon + " for " + seconds + " s" );
	killed_before = level.total_zombies_killed;

	end_time = getTime() + int( seconds * 1000 );
	while ( getTime() < end_time )
	{
		target = get_closest( self.origin, maps\mp\gametypes\_zombiemode_weap_tesla::tesla_sorted_zombies( self.origin ) );
		if ( isDefined( target ) )
		{
			eye = self.origin + ( 0, 0, 60 );
			self setPlayerAngles( vectorToAngles( ( target.origin + ( 0, 0, 40 ) ) - eye ) );
		}
		self codol_botPressButtons( 1, 0.15 );
		wait( 0.1 );
	}

	weapon = self getCurrentWeapon();
	ammo = "";
	if ( weapon != "none" )
		ammo = " clip " + self getWeaponAmmoClip( weapon ) + " stock " + self getWeaponAmmoStock( weapon );
	codolPrintLn( "CoDOL zm powerup test: " + self.name + " stopped firing, holding " + weapon + ammo +
		"; zombies killed meanwhile " + ( level.total_zombies_killed - killed_before ) );
}

powerup_test_gather( radius )
{
	eye = self.origin + ( 0, 0, 60 );
	moved = 0;
	zombies = maps\mp\gametypes\_zombiemode_weap_tesla::tesla_sorted_zombies( self.origin );
	for ( i = 0; i < zombies.size; i++ )
	{
		for ( tries = 0; tries < 12; tries++ )
		{
			yaw = ( ( i * 5 + tries ) * 30 ) % 360;
			spot = groundpos( self.origin + vector_scale( anglesToForward( ( 0, yaw, 0 ) ), radius ) + ( 0, 0, 30 ) );
			if ( abs( spot[ 2 ] - self.origin[ 2 ] ) > 40 )
				continue;
			if ( !bulletTracePassed( eye, spot + ( 0, 0, 40 ), false, self ) )
				continue;
			zombies[ i ] forceTeleport( spot, ( 0, yaw + 180, 0 ) );
			moved++;
			break;
		}
	}
	codolPrintLn( "CoDOL zm powerup test: gathered " + moved + " of " + zombies.size + " zombies " + radius + " units from " + self.name );
}
