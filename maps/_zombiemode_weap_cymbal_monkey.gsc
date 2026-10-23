// zm cymbal monkey: Danby's special (mp/cyborgclasstable.csv zm2 column 8).
//
// C1 SF_UI_ZOMBIE_CYMBAL_MONKEY_DESC: "Attracts robot zombies' attention,
// explodes after a few seconds."
//
// Port of T5 maps\_zombiemode_weap_cymbal_monkey.gsc:
//   init                          :6    the glow effect
//   player_handle_cymbal_monkey   :24   the throw -> land -> lure -> explode
//                                        flow, run here per grenade by
//                                        _zombiemode_equipment's grenade_fire
//                                        registry instead of a per-player loop
//   monitor_zombie_groans         :217  zombies near the monkey groan
//   do_monkey_sound               :142  the song, cleared on "explode"
//
// ADAPTED FOR C1 (each measured in docs/CODOL_ZM_EQUIPMENT_EVIDENCE_20260924.md
// or docs/zone_assets/common_gametype_zm.txt):
//   * The grenade itself stays visible.  T5 hides it and links an animated
//     weapon_zombie_monkey_bomb script_model (:59-:63); C1's world model for
//     zombie_cymbal_monkey IS the monkey (viewmodel_zombie_cymbal_monkey,
//     codol_checkweapon), and o_monkey_bomb lives in the zombie_cymbal_monkey
//     animtree, which this server declares no client correspondence for
//     (codol_script_model_animation.c report_static_prop) -- a linked copy
//     would be a second static monkey, so none is made.
//   * The glow is spawnFx/triggerFx at the landing point, not PlayFxOnTag on
//     "origin_animate_jnt" (a tag of T5's animated model).
//   * Sounds: zmb_monkey_song ships (common_gametype_zm:2743).
//     zmb_vox_monkey_explode, zmb_vox_monkey_scream and zmb_vox_zombie_groan
//     do not ship in any zm zone, so T5's delayed explode vox and the groans
//     are left out rather than played as missing aliases.
//   * The explosion is the weapon's own.  MEASURED 2026-09-24
//     (tools/codol_weapondef_zone_diff.py dump zombie_cymbal_monkey --from
//     common_gametype_zm): fuse 8000 ms (getWeaponFuseTime), explosion radius
//     100 (+0x778), inner/outer damage 5000/450 (+0x780/+0x784) -- every zombie
//     a lure ring brings within 100 units dies.  T5 :95 resets the fuse when
//     the monkey lands; resetmissiledetonationtime is not a registered builtin
//     here, so the 8 s run from the throw (the monkey lands in about 1 s).
//   * check_point_in_active_zone / check_point_in_playable_area (T5 :98):
//     this port has no zone system.  A monkey is a valid POI when at least
//     one attractor ring position could be traced around it; otherwise the
//     zombies walk to the monkey itself.

#include maps\_zombiemode_utility;

// A function, not a file constant: this compiler substitutes a constant
// for every identifier of the same name, case-insensitively (a parameter
// called "weapon" included).
monkey_weapon()
{
	return "zombie_cymbal_monkey";
}

init()
{
	if ( getWeaponIndexFromName( monkey_weapon() ) <= 0 )
	{
		{}
		return;
	}

	if ( !isDefined( level._effect ) )
		level._effect = [];
	level._effect[ "monkey_glow" ] = loadfx( "maps/zombie/fx_zombie_monkey_light" );

	// T5 :30-:47 defaults.
	if ( !isDefined( level.monkey_attract_dist_diff ) )
		level.monkey_attract_dist_diff = 45;
	if ( !isDefined( level.num_monkey_attractors ) )
		level.num_monkey_attractors = 96;
	if ( !isDefined( level.monkey_attract_dist ) )
		level.monkey_attract_dist = 1536;

	level.monkey_fuse_ms = getWeaponFuseTime( monkey_weapon() );
	{}

	maps\_zombiemode_equipment::register_equipment( monkey_weapon(), ::monkey_fired );
}

describe_ms( v )
{
	if ( !isDefined( v ) )
		return "(unknown)";
	return "" + v + " ms";
}

// self = the grenade the engine just threw.  T5 :49-:117.
monkey_fired( player, weapon )
{
	grenade = self;
	thrower = "(gone)";
	if ( isDefined( player ) )
		thrower = player.name;
	{}

	// T5 :54.  A downed player's monkey is taken away.
	if ( isDefined( player ) && player maps\_zombiemode_laststand::player_is_in_laststand() )
	{
		grenade delete();
		return;
	}

	info = spawnStruct();
	info.thrown_time = getTime();
	info.last_origin = grenade.origin;
	info.player = player;
	info.exploded = false;

	grenade thread watch_explode( info );

	// T5 :71-:72.  The monkey lures from the moment it leaves the hand,
	// with the zombies walking at the monkey itself until it lands.
	grenade maps\_zombiemode_poi::create_zombie_point_of_interest( level.monkey_attract_dist, level.num_monkey_attractors, 10000 );
	grenade.attract_to_origin = true;

	// T5 :74-:85.  Wait for it to come to rest.
	landed = grenade wait_for_rest( info, 10 );
	if ( !isDefined( grenade ) )
	{
		monkey_finish( info );
		return;
	}

	origin = grenade.origin;
	info.land_origin = origin;
	{}

	// Glow.  T5 :96 PlayFxOnTag( monkey_glow, model, "origin_animate_jnt" ).
	if ( isDefined( level._effect[ "monkey_glow" ] ) )
	{
		info.fx = spawnFx( level._effect[ "monkey_glow" ], origin + ( 0, 0, 8 ) );
		triggerFx( info.fx );
	}

	// T5 :98-:113.  Ring positions so the crowd surrounds the monkey.
	grenade thread monkey_attractor_positions( info );

	// T5 do_monkey_sound :154.
	grenade playSound( "zmb_monkey_song" );

	// T5 do_monkey_sound :159: everything after the bang.
	while ( !info.exploded )
		wait( 0.05 );

	monkey_finish( info );
}

// Fills info.last_origin every frame so the explosion point is known after
// the engine has freed the grenade, and catches the engine's "explode".
watch_explode( info )
{
	self thread track_origin( info );
	self waittill( "explode", position );
	if ( isDefined( position ) )
		info.explode_origin = position;
	info.exploded = true;
}

track_origin( info )
{
	grenade = self;
	while ( isDefined( grenade ) )
	{
		info.last_origin = grenade.origin;
		info.near = maps\_zombiemode_poi::zombies_in_radius( info.last_origin, 256 );
		wait( 0.05 );
	}
	info.exploded = true;
}

// Returns why the wait ended.  "stationary" is the native BounceMissile
// notify (codol_equipment_lifecycle.c); the velocity poll is T5's own test.
wait_for_rest( info, timeout )
{
	info.stationary = false;
	self thread forward_stationary( info );

	deadline = getTime() + int( timeout * 1000 );
	old = self.origin;
	for ( ;; )
	{
		wait( 0.05 );
		if ( !isDefined( self ) )
			return "freed";
		if ( info.exploded )
			return "exploded";
		if ( info.stationary )
			return "stationary";
		if ( distanceSquared( self.origin, old ) == 0 )
			return "at rest";
		old = self.origin;
		if ( getTime() > deadline )
			return "timeout";
	}
}

forward_stationary( info )
{
	self waittill( "stationary" );
	info.stationary = true;
}

monkey_attractor_positions( info )
{
	self maps\_zombiemode_poi::create_zombie_point_of_interest_attractor_positions( 4, level.monkey_attract_dist_diff );
	if ( !isDefined( self ) )
		return;

	count = 0;
	if ( isDefined( self.attractor_positions ) )
		count = self.attractor_positions.size;
	if ( count == 0 )
		self.attract_to_origin = true;
	{}
}

monkey_finish( info )
{
	origin = info.last_origin;
	if ( isDefined( info.explode_origin ) )
		origin = info.explode_origin;

	if ( isDefined( info.fx ) )
		info.fx delete();

	// The POI is already gone with the freed grenade; the next chase pass
	// hands every lured zombie back to flesh.
	maps\_zombiemode_poi::compact_pois();

	// The blast is the weapon's own (the engine's grenade explosion); what
	// it did is read back from the zombies that stood within 256 units of the
	// monkey in its last frame.
	near = [];
	if ( isDefined( info.near ) )
		near = info.near;
	wait( 0.25 );

	killed = maps\_zombiemode_poi::count_dead( near );
	retargets = 0;
	if ( isDefined( level.zombie_poi_retargets ) )
		retargets = level.zombie_poi_retargets;
	{}
}
