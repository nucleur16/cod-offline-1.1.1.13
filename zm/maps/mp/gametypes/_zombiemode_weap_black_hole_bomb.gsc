// zm black hole bomb (Gersh device): character zm3's special
// (mp/cyborgclasstable.csv column 8).
//
// C1 SF_UI_ZOMBIE_BLACK_HOLE_BOMB_DESC: "Pulls robot zombies into a black
// hole and crushes them."  C1 also ships the patch challenge
// CHALLENGE_ZM_BLACKHOLE_TELE ("black hole teleport",
// patch_localized_code_post_gfx_mp), so T5's jump-through teleport is part
// of the item and is ported too.
//
// Port of T5 maps\_zombiemode_weap_black_hole_bomb.gsc:
//   init                                    :7
//   player_handle_black_hole_bomb           :61   per grenade here, off the
//                                                 grenade_fire registry
//   black_hole_bomb_cleanup                 :192
//   do_black_hole_bomb_sound                :239
//   black_hole_bomb_initial_attract_func    :355  the pull: slow walk in the
//                                                 outer ring, fast inside
//                                                 1024, crushed inside 128
//   black_hole_bomb_attract_walk / _run     :506 / :556
//   black_hole_bomb_death_while_attracted   :625
//   black_hole_bomb_arrival_attract_func    :642
//   black_hole_bomb_event_horizon_death     :666
//   black_hole_bomb_corpse_hide             :691
//   black_hole_bomb_escaped_zombie_reset    :712
//   black_hole_bomb_teleport_init .. black_hole_teleport :907-:1073
//   black_hole_bomb_kill_counter            :1161
//   black_hole_bomb_create_exit_portal      :1186
//   black_hole_bomb_anim_init               :1261
//
// C1 ASSETS (docs/zone_assets/common_gametype_zm.txt, all MEASURED present):
//   fx  maps/zombie/fx_zmb_blackhole_looping, _exit, _implode,
//       _zombie_flare, _flare_marker, _trap_end, fx_blackhole_zombie_breakup,
//       fx_zombie_dog_explosion
//   snd wpn_gersh_device_exp/_implode/_kill/_loop_close,
//       zmb_gersh_teleporter_go/_out
//   xanim every ai_zombie_blackhole_* clip T5 names (:3144-:3164), all nodes
//       of the mounted generic_human.atr (patch_common_mp extract, :2638-:3011)
// NOT SHIPPED, so left out: fx_zmb_blackhole_zombie_death (T5's soul fx,
// which T5 loads but never plays), zmb_vox_monkey_explode, zmb_vox_zombie_groan.
//
// ADAPTED FOR C1 / this server:
//   * The grenade is the POI and stays visible (C1's world model is
//     t5_bh_bomb_world, the same model T5 links over its hidden grenade).
//   * T5 draws the vortex client side off a script-mover client flag
//     (SetClientFlag 10, zombie_cosmodrome csc).  C1's client scripts are not
//     this server's to drive, so the portal is spawnFx/triggerFx of the same
//     looping effect at the landing point.
//   * T5's anim-change throttle (:840, seven zombies per frame) exists
//     because T5 re-blends the run knob per change.  Here a change is a field
//     write the move animscript picks up on its next cycle, so no throttle.
//   * resetmissiledetonationtime (T5 :129) is not registered here; the
//     engine's fuse runs from the throw.  MEASURED 2026-09-24: fuse 8000 ms
//     (getWeaponFuseTime); the shipped definition's explosion does no damage
//     (tools/codol_weapondef_zone_diff.py: radius 100, inner/outer damage
//     0/0 at +0x780/+0x784), so -- as in T5 -- every kill is the pull's.
//   * check_point_in_active_zone / black_hole_bomb_stolen_by_sam: no zone
//     system in this port, so no "Samantha steals it".
//   * The pulled-in death plays T5's clip through animscripted (native, see
//     _zombiemode_spawner tear_into_building) and the crushed body is hidden
//     with the gib burst, as black_hole_bomb_corpse_hide does.
//   * The teleport polls the thrower's team every 0.1 s instead of spawning
//     a trigger_radius (same 64/70 cylinder), and uses T5's
//     "struct_black_hole_teleport" structs.  A map without them has no
//     teleport, exactly as in T5 (:972).  MEASURED 2026-09-24 (init log):
//     zm_prototype_sh, zm_zombie_training and zm_asylum_sh carry none, so
//     on the shipped maps the teleport never fires; where C1 sends the
//     player for CHALLENGE_ZM_BLACKHOLE_TELE is not established.

#include maps\mp\gametypes\_zombiemode_utility;

#using_animtree( "generic_human" );

// A function, not a file constant: this compiler substitutes a constant
// for every identifier of the same name, case-insensitively (a parameter
// called "weapon" included).
bhb_weapon()
{
	return "zombie_black_hole_bomb";
}

init()
{
	if ( getWeaponIndexFromName( bhb_weapon() ) <= 0 )
	{
		codolprintln( "CoDOL zm black hole: " + bhb_weapon() + " is not loaded on this map; not registered" );
		return;
	}

	if ( !isDefined( level._effect ) )
		level._effect = [];
	level._effect[ "black_hole_bomb_portal" ] = loadfx( "maps/zombie/fx_zmb_blackhole_looping" );
	level._effect[ "black_hole_bomb_portal_exit" ] = loadfx( "maps/zombie/fx_zmb_blackhole_exit" );
	level._effect[ "black_hole_bomb_zombie_destroy" ] = loadfx( "maps/zombie/fx_zmb_blackhole_zombie_flare" );
	level._effect[ "black_hole_bomb_zombie_gib" ] = loadfx( "maps/zombie/fx_zombie_dog_explosion" );
	level._effect[ "black_hole_bomb_event_horizon" ] = loadfx( "maps/zombie/fx_zmb_blackhole_implode" );
	level._effect[ "black_hole_bomb_zombie_pull" ] = loadfx( "maps/zombie/fx_blackhole_zombie_breakup" );
	level._effect[ "black_hole_bomb_marker_flare" ] = loadfx( "maps/zombie/fx_zmb_blackhole_flare_marker" );
	// Carrier of the zombie-pull and exit-portal effects, see bhb_fx_carrier.
	precacheModel( "tag_origin" );

	// T5 :67-:84 defaults.
	if ( !isDefined( level.num_black_hole_bomb_attractors ) )
		level.num_black_hole_bomb_attractors = 15;
	if ( !isDefined( level.black_hole_bomb_attract_dist ) )
		level.black_hole_bomb_attract_dist = 2056;

	black_hole_bomb_anim_init();

	level.black_hole_teleport_structs = get_struct_array( "struct_black_hole_teleport", "targetname" );

	level.black_hole_bomb_fuse_ms = getWeaponFuseTime( bhb_weapon() );
	fuse = "(unknown)";
	if ( isDefined( level.black_hole_bomb_fuse_ms ) )
		fuse = "" + level.black_hole_bomb_fuse_ms + " ms";
	codolprintln( "CoDOL zm black hole: registered; weapon fuse " + fuse + ", pull radius " + level.black_hole_bomb_attract_dist + ", " + level.black_hole_teleport_structs.size + " teleport destinations on " + getDvar( "mapname" ) );

	maps\mp\gametypes\_zombiemode_equipment::register_equipment( bhb_weapon(), ::black_hole_bomb_fired );
}

// T5 :1261.  Kept in its own table: _zombiemode::init_standard_zombie_anims
// rebuilds level.scr_anim from scratch.
black_hole_bomb_anim_init()
{
	a = [];
	a[ "slow_pull_1" ] = %ai_zombie_blackhole_walk_slow_v1;
	a[ "slow_pull_2" ] = %ai_zombie_blackhole_walk_slow_v2;
	a[ "slow_pull_3" ] = %ai_zombie_blackhole_walk_slow_v3;

	a[ "fast_pull_1" ] = %ai_zombie_blackhole_walk_fast_v1;
	a[ "fast_pull_2" ] = %ai_zombie_blackhole_walk_fast_v2;
	a[ "fast_pull_3" ] = %ai_zombie_blackhole_walk_fast_v3;

	// T5: every one of these carries a "bhb_burst" notetrack.
	a[ "black_hole_death_1" ] = %ai_zombie_blackhole_death_v1;
	a[ "black_hole_death_2" ] = %ai_zombie_blackhole_death_v2;
	a[ "black_hole_death_3" ] = %ai_zombie_blackhole_death_v3;

	a[ "crawler_slow_pull_1" ] = %ai_zombie_blackhole_crawl_slow_v1;
	a[ "crawler_slow_pull_2" ] = %ai_zombie_blackhole_crawl_slow_v2;

	a[ "crawler_fast_pull_1" ] = %ai_zombie_blackhole_crawl_fast_v1;
	a[ "crawler_fast_pull_2" ] = %ai_zombie_blackhole_crawl_fast_v2;
	a[ "crawler_fast_pull_3" ] = %ai_zombie_blackhole_crawl_fast_v3;

	a[ "crawler_black_hole_death_1" ] = %ai_zombie_blackhole_crawl_death_v1;
	a[ "crawler_black_hole_death_2" ] = %ai_zombie_blackhole_crawl_death_v2;
	a[ "crawler_black_hole_death_3" ] = %ai_zombie_blackhole_crawl_death_v3;

	a[ "attracted_death_1" ] = %ai_zombie_blackhole_death_preburst_v1;
	a[ "attracted_death_2" ] = %ai_zombie_blackhole_death_preburst_v2;
	a[ "attracted_death_3" ] = %ai_zombie_blackhole_death_preburst_v3;
	a[ "attracted_death_4" ] = %ai_zombie_blackhole_death_preburst_v4;

	level._black_hole_bomb_anims = a;
}

bhb_anim( name )
{
	if ( !isDefined( level._black_hole_bomb_anims ) )
		return undefined;
	return level._black_hole_bomb_anims[ name ];
}

// --------------------------------------------------------------------------
// The device.  self = the grenade the engine just threw.  T5 :86-:183.
// --------------------------------------------------------------------------

black_hole_bomb_fired( player, weapon )
{
	grenade = self;
	thrower = "(gone)";
	if ( isDefined( player ) )
		thrower = player.name;
	codolprintln( "CoDOL zm black hole: thrown by " + thrower + " at " + grenade.origin );

	// T5 :91.
	if ( isDefined( player ) && player maps\mp\gametypes\_zombiemode_laststand::player_is_in_laststand() )
	{
		grenade delete();
		return;
	}

	info = spawnStruct();
	info.thrown_time = getTime();
	info.last_origin = grenade.origin;
	info.player = player;
	info.exploded = false;
	info.kills = 0;
	info.pulled = 0;

	grenade thread track_grenade( info );
	grenade thread watch_explode( info );

	landed = grenade wait_for_rest( info, 10 );
	if ( !isDefined( grenade ) || info.exploded )
	{
		codolprintln( "CoDOL zm black hole: exploded before it came to rest (" + landed + ")" );
		return;
	}

	origin = grenade.origin;
	info.land_origin = origin;
	codolprintln( "CoDOL zm black hole: opened at " + origin + " after " + ( getTime() - info.thrown_time ) + " ms (" + landed + ")" );

	// T5 :125-:127, :163.  The bomb itself is the POI: the tosser for the
	// kill credit, a fresh ground trace under it for every zombie.
	grenade._black_hole_bomb_player = player;
	grenade._black_hole_bomb_info = info;
	grenade._new_ground_trace = true;
	grenade maps\mp\gametypes\_zombiemode_poi::create_zombie_point_of_interest( level.black_hole_bomb_attract_dist, level.num_black_hole_bomb_attractors, 0, true, ::black_hole_bomb_initial_attract_func, ::black_hole_bomb_arrival_attract_func );

	// The vortex (T5 SetClientFlag 10 -> fx_zmb_blackhole_looping).
	if ( isDefined( level._effect[ "black_hole_bomb_portal" ] ) )
	{
		info.portal_fx = spawnFx( level._effect[ "black_hole_bomb_portal" ], origin + ( 0, 0, 20 ) );
		triggerFx( info.portal_fx );
	}

	// T5 do_black_hole_bomb_sound :248-:249.
	grenade playSound( "wpn_gersh_device_exp" );
	grenade playLoopSound( "wpn_gersh_device_loop_close" );

	if ( isDefined( player ) )
		player thread black_hole_bomb_kill_counter( grenade, info );
	level thread black_hole_bomb_teleport_watch( info );

	while ( !info.exploded )
		wait( 0.05 );

	black_hole_bomb_collapse( info );
}

// T5 do_black_hole_bomb_sound :254-:256 and black_hole_bomb_cleanup :192.
black_hole_bomb_collapse( info )
{
	origin = info.land_origin;
	if ( isDefined( info.explode_origin ) )
		origin = info.explode_origin;

	playSoundAtPosition( "wpn_gersh_device_implode", origin );
	if ( isDefined( level._effect[ "black_hole_bomb_event_horizon" ] ) )
		playFx( level._effect[ "black_hole_bomb_event_horizon" ], origin + ( 0, 0, 20 ) );

	if ( isDefined( info.portal_fx ) )
		info.portal_fx delete();

	maps\mp\gametypes\_zombiemode_poi::compact_pois();

	near = [];
	if ( isDefined( info.near ) )
		near = info.near;
	wait( 0.25 );
	blast = maps\mp\gametypes\_zombiemode_poi::count_dead( near );

	codolprintln( "CoDOL zm black hole: collapsed at " + origin + " " + ( getTime() - info.thrown_time ) + " ms after the throw; " + info.pulled + " zombies pulled, " + info.kills + " crushed; of " + near.size + " within 256 at the collapse, " + blast + " are dead" );
}

track_grenade( info )
{
	grenade = self;
	while ( isDefined( grenade ) )
	{
		info.last_origin = grenade.origin;
		info.near = maps\mp\gametypes\_zombiemode_poi::zombies_in_radius( info.last_origin, 256 );
		wait( 0.05 );
	}
	info.exploded = true;
}

watch_explode( info )
{
	self waittill( "explode", position );
	if ( isDefined( position ) )
		info.explode_origin = position;
	info.exploded = true;
}

// T5 :108-:119 polls velocity to zero; "stationary" is the native
// BounceMissile notify (codol_equipment_lifecycle.c).
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

// T5 :1161.  Five crushed by one device is T5's achievement notify.
black_hole_bomb_kill_counter( grenade, info )
{
	self endon( "disconnect" );

	notified = false;
	while ( isDefined( grenade ) && !info.exploded )
	{
		if ( !notified && info.kills >= 5 )
		{
			notified = true;
			self notify( "black_hole_kills_achievement" );
		}
		wait( 0.1 );
	}
}

// --------------------------------------------------------------------------
// The pull.  self = a zombie; threaded once by _zombiemode_poi when this
// bomb becomes the zombie's POI.
// --------------------------------------------------------------------------

// T5 :355.
black_hole_bomb_initial_attract_func( ent_poi )
{
	self endon( "death" );

	if ( is_true( self._bhb_pulled ) || !isDefined( ent_poi ) )
		return;

	pulled_in_range = 128 * 128;
	inner_range = 1024 * 1024;
	outer_edge = 2056 * 2056;

	info = ent_poi._black_hole_bomb_info;
	if ( isDefined( info ) )
		info.pulled++;

	self._bhb_pulled = true;
	self._black_hole_bomb_collapse_death = 0;
	self._black_hole_attract_walk = 0;
	self._black_hole_attract_run = 0;
	self._current_black_hole_bomb_origin = ent_poi.origin;
	self._black_hole_bomb_tosser = ent_poi._black_hole_bomb_player;
	self._black_hole_bomb_info = info;

	// T5 :391, :399: the special death while being pulled, and the cycle to
	// restore if the zombie gets away.
	self._bhb_saved_run_combat = self.run_combatanim;
	self._bhb_saved_run_noncombat = self.run_noncombatanim;
	self._bhb_saved_crawl = self.crawl_anim;
	self._bhb_saved_deathanim = self.deathanim;
	self._bhb_saved_moverate = self.moveplaybackrate;
	self._bhb_pull_anim = undefined;
	if ( is_true( self.has_legs ) )
	{
		death = bhb_anim( "attracted_death_" + randomIntRange( 1, 5 ) );
		if ( isDefined( death ) )
			self.deathanim = death;
	}

	codolprintln( "CoDOL zm black hole: ent " + self getEntityNumber() + " caught by the pull " + int( distance( self.origin, ent_poi.origin ) ) + " units out" );

	while ( isDefined( ent_poi ) && isDefined( self.zombie_poi_current ) && self.zombie_poi_current == ent_poi )
	{
		d = distanceSquared( self.origin, self._current_black_hole_bomb_origin );

		// T5 :411.  Outer ring: dragged at a stumble.
		if ( self._black_hole_attract_walk == 0 && d < outer_edge && d > inner_range )
			self black_hole_bomb_attract_walk();

		// T5 :425.  Inside 1024: dragged fast.
		if ( self._black_hole_attract_run == 0 && d < inner_range && d > pulled_in_range )
			self black_hole_bomb_attract_run();

		// T5 :438-:463.  Inside 128: the event horizon.
		if ( d < pulled_in_range )
		{
			self._black_hole_bomb_collapse_death = 1;
			self black_hole_bomb_event_horizon_death( self._current_black_hole_bomb_origin, ent_poi );
			return;
		}

		// The round's speed-up (_zombiemode_spawner zombie_speed_up ->
		// needs_run_update -> set_zombie_run_cycle) re-rolls the cycle and
		// its rate from zombie_think; the pull keeps hold of both.
		if ( isDefined( self._bhb_pull_anim ) )
			self set_pull_anim( self._bhb_pull_anim );

		wait( 0.1 );
	}

	// T5 :470.  The hole closed (or another POI took over) first.
	self black_hole_bomb_escaped_zombie_reset();
}

// T5 :506.
black_hole_bomb_attract_walk()
{
	if ( is_true( self.has_legs ) )
		self set_pull_anim( bhb_anim( "slow_pull_" + randomIntRange( 1, 4 ) ) );
	else
		self set_pull_anim( bhb_anim( "crawler_slow_pull_" + randomIntRange( 1, 3 ) ) );
	self._black_hole_attract_walk = 1;
}

// T5 :556.
black_hole_bomb_attract_run()
{
	rand = randomIntRange( 1, 4 );
	if ( is_true( self.has_legs ) )
		self set_pull_anim( bhb_anim( "fast_pull_" + rand ) );
	else
		self set_pull_anim( bhb_anim( "crawler_fast_pull_" + rand ) );
	self._black_hole_attract_run = 1;
}

// The fields zombie_move.gsc reads each cycle (MoveStandRun current_run_anim,
// MoveCrawl self.crawl_anim), as _zombiemode_spawner::set_run_anim writes them.
//
// T5 plays the pull clips at their own rate: the pull is the clip's root
// motion.  This server scales every move cycle by self.moveplaybackrate
// (_zombiemode_spawner apply_move_speed_scale, 0.5 for a round-1 walker), so
// the pull is put back on the actor's base rate (animplaybackrate) while it
// lasts, and the saved rate is restored if the zombie escapes.
set_pull_anim( pull )
{
	if ( !isDefined( pull ) )
		return;

	self._bhb_pull_anim = pull;
	base = 1;
	if ( isDefined( self.animplaybackrate ) )
		base = self.animplaybackrate;
	self.moveplaybackrate = base;

	if ( is_true( self.has_legs ) )
	{
		self.run_combatanim = pull;
		self.run_noncombatanim = pull;
		self.walk_combatanim = pull;
		self.walk_noncombatanim = pull;
		if ( isDefined( self.a ) )
			self.a.combatrunanim = pull;
	}
	else
		self.crawl_anim = pull;
}

// T5 :642.  Reaching the hole's ground point is the same death.  T5 waits on
// "goal"; this server's mover does not reliably send it, so the distance to
// the goal the POI gave is polled.
black_hole_bomb_arrival_attract_func( ent_poi )
{
	self endon( "death" );
	self endon( "zombie_poi_changed" );

	for ( ;; )
	{
		wait( 0.1 );
		if ( !isDefined( ent_poi ) )
			return;
		if ( is_true( self._bhb_dying ) )
			return;
		goal = self.zombie_poi_goal;
		if ( isDefined( goal ) && distanceSquared( self.origin, goal ) < 40 * 40 )
		{
			self._black_hole_bomb_collapse_death = 1;
			self black_hole_bomb_event_horizon_death( ent_poi.origin, ent_poi );
			return;
		}
	}
}

// T5 :666.
black_hole_bomb_event_horizon_death( vec_black_hole_org, grenade )
{
	self endon( "death" );

	if ( is_true( self._bhb_dying ) )
		return;
	self._bhb_dying = true;

	self maps\mp\gametypes\_zombiemode_spawner::zombie_eye_glow_stop();
	self playSound( "wpn_gersh_device_kill" );

	// T5 csc :398 plays the pull on a tag_origin that is deleted with the
	// zombie (:425 black_hole_bomb_pulled_in_fx_clean).
	if ( isDefined( level._effect[ "black_hole_bomb_zombie_pull" ] ) )
		level thread bhb_fx_carrier( level._effect[ "black_hole_bomb_zombie_pull" ], self.origin + ( 0, 0, 40 ), 10, self );

	// T5 :676-:680.  The pulled-in clip; "bhb_burst" is where it ends.
	if ( is_true( self.has_legs ) )
		pulled_in_anim = bhb_anim( "black_hole_death_" + randomIntRange( 1, 4 ) );
	else
		pulled_in_anim = bhb_anim( "crawler_black_hole_death_" + randomIntRange( 1, 4 ) );

	if ( isDefined( pulled_in_anim ) )
	{
		rate = 1;
		if ( isDefined( self.animplaybackrate ) )
			rate = self.animplaybackrate;
		self animscripted( "pulled_in_complete", self.origin, self.angles, pulled_in_anim, "normal", %body, rate, 0.2 );
		self thread pulled_in_timeout( getAnimLength( pulled_in_anim ) + 0.25 );
		for ( ;; )
		{
			self waittill( "pulled_in_complete", note );
			if ( !isDefined( note ) || note == "bhb_burst" || note == "end" || note == "timeout" )
				break;
		}
	}

	// T5 :683 soul flare, :698 corpse gib and hide (black_hole_bomb_corpse_hide).
	if ( isDefined( level._effect[ "black_hole_bomb_zombie_destroy" ] ) )
		playFx( level._effect[ "black_hole_bomb_zombie_destroy" ], self.origin + ( 0, 0, 40 ) );
	if ( isDefined( level._effect[ "black_hole_bomb_zombie_gib" ] ) )
		playFx( level._effect[ "black_hole_bomb_zombie_gib" ], self.origin + ( 0, 0, 30 ) );
	self hide();

	// T5 :685.
	info = self._black_hole_bomb_info;
	if ( isDefined( info ) )
		info.kills++;
	if ( isDefined( grenade ) )
		grenade notify( "black_hole_bomb_kill" );

	codolprintln( "CoDOL zm black hole: ent " + self getEntityNumber() + " crushed at " + self.origin );

	// T5 :687: DoDamage( health + 50, origin + (0,0,50), tosser,
	// "zombie_black_hole_bomb", "MOD_CRUSH" ), in this server's MP
	// doDamage order (damage, source, attacker, inflictor, headshot, mod,
	// flags, weapon).  The tosser may have left; then the world kills it.
	tosser = self._black_hole_bomb_tosser;
	if ( isDefined( tosser ) && isPlayer( tosser ) )
		self doDamage( self.health + 50, self.origin + ( 0, 0, 50 ), tosser, tosser, 0, "MOD_CRUSH", 0, bhb_weapon() );
	else
		self doDamage( self.health + 50, self.origin + ( 0, 0, 50 ) );
}

pulled_in_timeout( t )
{
	self endon( "death" );
	wait( t );
	self notify( "pulled_in_complete", "timeout" );
}

// T5 :712.  Back to the zombie's own cycle and death.
black_hole_bomb_escaped_zombie_reset()
{
	if ( !is_true( self._bhb_pulled ) )
		return;

	if ( isDefined( self._bhb_saved_run_combat ) )
	{
		self.run_combatanim = self._bhb_saved_run_combat;
		self.walk_combatanim = self._bhb_saved_run_combat;
		if ( isDefined( self.a ) )
			self.a.combatrunanim = self._bhb_saved_run_combat;
	}
	if ( isDefined( self._bhb_saved_run_noncombat ) )
	{
		self.run_noncombatanim = self._bhb_saved_run_noncombat;
		self.walk_noncombatanim = self._bhb_saved_run_noncombat;
	}
	if ( isDefined( self._bhb_saved_crawl ) )
		self.crawl_anim = self._bhb_saved_crawl;
	if ( isDefined( self._bhb_saved_deathanim ) )
		self.deathanim = self._bhb_saved_deathanim;
	if ( isDefined( self._bhb_saved_moverate ) )
		self.moveplaybackrate = self._bhb_saved_moverate;

	self._bhb_pull_anim = undefined;
	self._bhb_pulled = undefined;
	self._black_hole_attract_walk = 0;
	self._black_hole_attract_run = 0;
	self._black_hole_bomb_info = undefined;
	codolprintln( "CoDOL zm black hole: ent " + self getEntityNumber() + " escaped the pull" );
}

// --------------------------------------------------------------------------
// The teleport.  T5 :907-:1073: a player who JUMPS into the hole (not on
// the ground, inside a 64-radius, 70-high cylinder, with a clear line from
// the eye to 65 units above the hole) comes out at a random
// struct_black_hole_teleport.
// --------------------------------------------------------------------------

black_hole_bomb_teleport_watch( info )
{
	if ( !isDefined( level.black_hole_teleport_structs ) || level.black_hole_teleport_structs.size == 0 )
		return;

	origin = info.land_origin;
	while ( !info.exploded )
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( !is_player_valid( p ) )
				continue;
			if ( is_true( p._bhb_teleporting ) )
				continue;
			if ( distance2d( p.origin, origin ) > 64 )
				continue;
			dz = p.origin[ 2 ] - origin[ 2 ];
			if ( dz < -8 || dz > 70 )
				continue;
			if ( p isOnGround() )
				continue;
			if ( !bulletTracePassed( p getEye(), origin + ( 0, 0, 65 ), false, p ) )
				continue;
			p thread black_hole_time_before_teleport( origin );
		}
		wait( 0.1 );
	}
}

// T5 :952.  self = player.
black_hole_time_before_teleport( hole_origin )
{
	self endon( "disconnect" );
	self endon( "death" );

	structs = array_randomize( level.black_hole_teleport_structs );
	chosen = undefined;
	for ( i = 0; i < structs.size; i++ )
	{
		// T5 :991 requires an active zone other than the player's own; with
		// no zone system the stand-in is "not where the player already is".
		if ( distanceSquared( structs[ i ].origin, self.origin ) > 512 * 512 )
		{
			chosen = structs[ i ];
			break;
		}
	}
	if ( !isDefined( chosen ) )
		return;

	playSoundAtPosition( "zmb_gersh_teleporter_out", hole_origin );
	self black_hole_teleport( chosen );
}

// T5 :1020.
black_hole_teleport( struct_dest )
{
	self._bhb_teleporting = true;

	destination = struct_dest.origin;
	stance = self getStance();
	if ( stance == "prone" )
		destination = destination + ( 0, 0, 49 );
	else if ( stance == "crouch" )
		destination = destination + ( 0, 0, 20 );

	black_hole_bomb_create_exit_portal( struct_dest.origin );

	self freezeControls( true );
	self disableOffhandWeapons();
	self disableWeapons();

	from = self.origin;
	self setOrigin( destination );
	if ( isDefined( struct_dest.angles ) )
		self setPlayerAngles( struct_dest.angles );

	self enableOffhandWeapons();
	self enableWeapons();
	self freezeControls( false );

	codolprintln( "CoDOL zm black hole: " + self.name + " teleported from " + from + " to " + destination );
	self notify( "black_hole_teleported" );

	wait( 1 );
	self._bhb_teleporting = undefined;
}

// T5 :1186, with the T5 :1195 clean-up four seconds later.
black_hole_bomb_create_exit_portal( pos )
{
	if ( isDefined( level._effect[ "black_hole_bomb_portal_exit" ] ) )
		level thread bhb_fx_carrier( level._effect[ "black_hole_bomb_portal_exit" ], pos, 4, undefined );
	playSoundAtPosition( "zmb_gersh_teleporter_go", pos );
}

// fx_blackhole_zombie_breakup and fx_zmb_blackhole_exit loop forever
// (MEASURED, FxEffectDef msecLoopingLife 0x7fffffff in common_gametype_zm).
// playFx at a point would leave them on every client for the rest of the map,
// so each rides a tag_origin carrier that is stopped and deleted after
// <seconds>, or earlier once <owner> (if given) is dead or gone.  A level
// thread that polls: a thread waiting on the owner would die with it.
bhb_fx_carrier( fx, pos, seconds, owner )
{
	carrier = spawn( "script_model", pos );
	carrier setModel( "tag_origin" );
	playFxOnTag( fx, carrier, "tag_origin" );

	had_owner = isDefined( owner );
	end_time = getTime() + int( seconds * 1000 );
	while ( getTime() < end_time )
	{
		if ( had_owner && ( !isDefined( owner ) || !isAlive( owner ) ) )
			break;
		wait( 0.05 );
	}

	stopFxOnTag( fx, carrier, "tag_origin" );
	wait_network_frame();
	carrier delete();
}
