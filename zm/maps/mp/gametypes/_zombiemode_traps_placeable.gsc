// zm placeable traps: the equipment-slot item every zombies character carries.
//
// C1-ORIGINAL ITEMS.  None of the four has a T5 script of the same name; the
// shipped C1 descriptions (localized_code_post_gfx_mp, GBK, translated in
// docs/CODOL_ZM_EQUIPMENT_EVIDENCE_20260924.md section 2) are the spec:
//
//   trap_fire_zm   (zm3) "Place a fire trap on the ground.  Each robot zombie
//                  burned to death extends the trap by 2 seconds."
//   trap_freeze_zm (zm2) "Place an ice trap on the ground that freezes
//                  approaching robot zombies."
//   trap_shock_zm  (zm1) "Place a shock trap on the ground; shock traps link
//                  with other shock traps into a larger electric grid."
//   trap_shrink_zm (zm4) "Place a dwarf trap on the ground; approaching robot
//                  zombies shrink into robot dwarves that can be kicked away."
//
// MEASURED (codol_checkweapon, evidence doc section 3): all four are thrown
// offhands, type grenade, offhand class 2, clip 2 / max 2, world models
// t5_tf / t5_tz / t5_ts / t5_tk _bomb_world.  The weapon data carries no
// lifetime, radius or damage, so those come from T5's own zombies code:
//
//   lifetime 40 s          T5 maps\_zombiemode_traps.gsc:31 elec_trap_time,
//                          :500 / :546 _trap_duration (electric and fire).
//   fire kill              T5 :888 zombie_trap_death: the zombie is ignited
//                          and dies after randomfloat( 1.25 ) (:915 / :939).
//   fire extension 2 s     C1 text above.
//   freeze radius 120      T5 _zombiemode_weap_freezegun.gsc:21
//                          freezegun_cylinder_radius.
//   frozen zombie shatters T5 freezegun :409 freezegun_do_shatter (a frozen
//   on any hit             zombie that is shot shatters).
//   shock arc / link 300   T5 _zombiemode_weap_tesla.gsc:34 tesla_radius_start
//                          (how far an arc jumps); arc travel 0.5 s (:37),
//                          bolt carried on a moving tag_origin (:341-:382).
//   shock kill             T5 tesla :303 and traps :952 (health + 666).
//   shrink radius 84       T5 _zombiemode_weap_shrink_ray.gsc:672 (upgraded
//                          cylinder radius).
//   shrink time 5-6 s      T5 shrink ray :205-:212 (2.5 + rand 0.5, doubled
//                          when upgraded).
//   shrunk health 1        T5 :357.
//   kick / step-on         T5 :506 watch_for_kicked (30-unit trigger on the
//                          zombie, player must be moving; facing it and
//                          moving forward kicks, anything else steps on it),
//                          :582 kicked_death, :636 shrink_death.
//
// C1-SPECIFIC INTERPRETATION (no C1 data states these; labelled, not
// measured): fire radius 72 and shock radius 64 (the trap models are small
// ground devices, smaller than the freezegun cylinder); freeze hold 5 s and a
// zombie is frozen at most once per trap, so a trap cannot pin a zombie until
// round_spawn_failsafe culls it; a zombie is shrunk at most once per trap;
// traps never hurt players (C1 zombies is co-op; T5's map traps hurt players
// because anyone can walk into a map hazard); two live traps per player per
// weapon (the weapon's max ammo is 2), the oldest removed first; a kill is
// credited to the player who placed the trap (T5 map traps credit nobody, but
// T5's tesla and shrink ray -- player-owned devices -- credit the player).
//
// DWARF BODY: T5 shrinks a zombie by swapping to a mini body model (shrink
// ray :340 level.shrink_models).  CoDOL ships no mini zombie model
// (docs/zone_assets/ALL_xmodel.txt has none for the five c_zom_* families),
// so the zombie's own model is drawn at half size instead: C1's entityState
// carries a model scale at +0x54, read by the client at 0x441532, which the
// native setModelScale sets.  Half size is this port's choice; no C1 data
// gives a shrink scale.  Only the drawing shrinks; the hull does not.
//
// NOT TAKEN: T5 launchragdoll on the kick (not a registered builtin; the
// kick kills with damage directed away from the kicker instead), T5's
// per-actor meleeDamage 5 while shrunk (no such actor field here), and the
// freezegun shatter's radius damage to neighbours.

#include maps\mp\gametypes\_zombiemode_utility;

#using_animtree ( "generic_human" );

// Callback_StartGameType, after _zombiemode_equipment::init(): the precache
// window.
init()
{
	level.zm_traps = [];
	level.zm_trap_next_id = 1;

	level.zm_trap_models = [];
	level.zm_trap_models[ "trap_fire_zm" ] = "t5_tf_bomb_world";
	level.zm_trap_models[ "trap_freeze_zm" ] = "t5_tz_bomb_world";
	level.zm_trap_models[ "trap_shock_zm" ] = "t5_ts_bomb_world";
	level.zm_trap_models[ "trap_shrink_zm" ] = "t5_tk_bomb_world";

	keys = getArrayKeys( level.zm_trap_models );
	for ( i = 0; i < keys.size; i++ )
		precacheModel( level.zm_trap_models[ keys[ i ] ] );
	precacheModel( "tag_origin" );

	// Every name below is MEASURED present in common_gametype_zm
	// (docs/zone_assets/common_gametype_zm.txt, evidence doc section 4).
	level._effect[ "zm_trap_fire_loop" ] = loadfx( "maps/zombie/fx_zombie_fire_trap_med" );
	level._effect[ "zm_trap_fire_torso" ] = loadfx( "env/fire/fx_fire_player_torso" );
	level._effect[ "zm_trap_fire_md" ] = loadfx( "env/fire/fx_fire_player_md" );
	level._effect[ "zm_trap_freeze_loop" ] = loadfx( "maps/zombie/fx_freeze_trap_active_loop" );
	level._effect[ "zm_trap_freeze_torso" ] = loadfx( "maps/zombie/fx_zombie_freeze_torso" );
	level._effect[ "zm_trap_freeze_md" ] = loadfx( "maps/zombie/fx_zombie_freeze_md" );
	level._effect[ "zm_trap_freeze_shatter" ] = loadfx( "weapon/freeze_gun/fx_freezegun_shatter" );
	level._effect[ "zm_trap_freeze_crumple" ] = loadfx( "weapon/freeze_gun/fx_freezegun_crumple" );
	level._effect[ "zm_trap_shock_spark" ] = loadfx( "env/electrical/fx_elec_short_oneshot" );
	level._effect[ "zm_trap_shock_burst" ] = loadfx( "env/electrical/fx_elec_wire_spark_burst" );
	level._effect[ "zm_trap_shock_bolt" ] = loadfx( "maps/zombie/fx_zombie_tesla_bolt_secondary" );
	level._effect[ "zm_trap_shock_zombie" ] = loadfx( "maps/zombie/fx_zombie_tesla_shock" );
	level._effect[ "zm_trap_shock_zombie2" ] = loadfx( "maps/zombie/fx_zombie_tesla_shock_secondary" );
	level._effect[ "zm_trap_shrink_loop" ] = loadfx( "maps/zombie/fx_zombie_shrink_trap_looping" );
	level._effect[ "zm_trap_shrink" ] = loadfx( "weapon/shrink_ray/zombie_shrink" );
	level._effect[ "zm_trap_unshrink" ] = loadfx( "weapon/shrink_ray/zombie_unshrink" );
	level._effect[ "zm_trap_squish" ] = loadfx( "maps/zombie_temple/fx_ztem_zombie_mini_squish" );

	register = [];
	register[ 0 ] = "trap_fire_zm";
	register[ 1 ] = "trap_freeze_zm";
	register[ 2 ] = "trap_shock_zm";
	register[ 3 ] = "trap_shrink_zm";
	for ( i = 0; i < register.size; i++ )
		maps\mp\gametypes\_zombiemode_equipment::register_equipment( register[ i ], ::on_trap_thrown );

	setDvar( "scr_codol_traptest", "" );
	level thread trap_test_watch();

	codolprintln( "CoDOL zm trap: fire/freeze/shock/shrink placeable traps registered" );
}

// ==========================================================================
// Placement.  self = the thrown grenade (engine grenade_fire).
// ==========================================================================

on_trap_thrown( player, weapon )
{
	// The grenade is deleted below; run the placement off the level so this
	// thread's self going away cannot end it.
	level thread place_trap( self, player, weapon );
}

place_trap( grenade, player, weapon )
{
	origin = grenade.origin;
	grenade thread flag_stationary();

	// Bounded: a grenade that never settles is placed where it last was.
	for ( t = 0; t < 80; t++ )
	{
		if ( !isDefined( grenade ) )
			break;
		origin = grenade.origin;
		if ( is_true( grenade.zm_trap_stationary ) )
			break;
		wait( 0.05 );
	}

	if ( isDefined( grenade ) )
		grenade delete();

	spawn_trap( origin, player, weapon );
}

// Stands a trap on the ground at origin and starts it.  Used by the throw
// above; the probe place/grid test hooks call it directly.
spawn_trap( origin, player, weapon )
{
	trace = bulletTrace( origin + ( 0, 0, 16 ), origin - ( 0, 0, 64 ), false, undefined );
	if ( trace[ "fraction" ] < 1 )
		origin = trace[ "position" ];

	trap = spawn( "script_model", origin );
	trap setModel( level.zm_trap_models[ weapon ] );
	trap.angles = ( 0, randomInt( 360 ), 0 );
	trap.zm_trap_id = level.zm_trap_next_id;
	level.zm_trap_next_id++;
	trap.zm_trap_weapon = weapon;
	trap.zm_trap_owner = player;
	trap.zm_trap_kills = 0;
	trap.zm_trap_start = getTime();
	trap.zm_trap_end = getTime() + 40000;
	trap.zm_trap_fx = [];

	owner_name = "(gone)";
	if ( isDefined( player ) )
	{
		owner_name = player.name;
		player trap_owner_add( trap );

		// CHALLENGE_ZM_TRAPS "Trap expert: activate &&1 traps".  Counted
		// here; no zm challenge tracker exists yet to consume the notify.
		if ( !isDefined( player.zm_traps_activated ) )
			player.zm_traps_activated = 0;
		player.zm_traps_activated = player.zm_traps_activated + 1;
		player notify( "zm_trap_activated", weapon );
	}

	level.zm_traps[ level.zm_traps.size ] = trap;
	level notify( "zm_trap_placed", trap );

	codolprintln( "CoDOL zm trap: " + weapon + " #" + trap.zm_trap_id + " placed by " + owner_name +
		" at " + origin + ", lifetime 40 s" );

	switch ( weapon )
	{
	case "trap_fire_zm":
		trap thread fire_trap_think();
		break;
	case "trap_freeze_zm":
		trap thread freeze_trap_think();
		break;
	case "trap_shock_zm":
		trap thread shock_trap_think();
		break;
	case "trap_shrink_zm":
		trap thread shrink_trap_think();
		break;
	}
}

// T5 g_missile BounceMissile's "stationary" (codol_equipment_lifecycle.c).
flag_stationary()
{
	self endon( "death" );
	self waittill( "stationary" );
	self.zm_trap_stationary = true;
}

// self = player.  At most two live traps of one kind per player.
trap_owner_add( trap )
{
	if ( !isDefined( self.zm_traps ) )
		self.zm_traps = [];

	live = [];
	same = 0;
	for ( i = 0; i < self.zm_traps.size; i++ )
	{
		if ( !isDefined( self.zm_traps[ i ] ) || is_true( self.zm_traps[ i ].zm_trap_done ) )
			continue;
		live[ live.size ] = self.zm_traps[ i ];
		if ( self.zm_traps[ i ].zm_trap_weapon == trap.zm_trap_weapon )
			same++;
	}

	if ( same >= 2 )
	{
		for ( i = 0; i < live.size; i++ )
		{
			if ( live[ i ].zm_trap_weapon == trap.zm_trap_weapon )
			{
				codolprintln( "CoDOL zm trap: #" + live[ i ].zm_trap_id + " replaced by a newer " + trap.zm_trap_weapon );
				live[ i ].zm_trap_end = 0;
				break;
			}
		}
	}

	live[ live.size ] = trap;
	self.zm_traps = live;
}

// self = trap.  True while the trap should keep running.
trap_alive()
{
	if ( is_true( level.intermission ) )
		return false;
	return getTime() < self.zm_trap_end;
}

// self = trap.  Looping FX on the trap for its lifetime.
trap_start_fx( fx_ref, sound_start, sound_loop )
{
	fx = spawnFx( level._effect[ fx_ref ], self.origin + ( 0, 0, 2 ) );
	triggerFx( fx );
	self.zm_trap_fx[ self.zm_trap_fx.size ] = fx;

	if ( isDefined( sound_start ) )
		self playSound( sound_start );
	if ( isDefined( sound_loop ) )
		self playLoopSound( sound_loop );
}

// self = trap.  Tears the trap down and logs its record.
trap_finish( sound_end )
{
	self.zm_trap_done = true;
	self notify( "zm_trap_done" );

	for ( i = 0; i < self.zm_trap_fx.size; i++ )
	{
		if ( isDefined( self.zm_trap_fx[ i ] ) )
			self.zm_trap_fx[ i ] delete();
	}

	if ( isDefined( sound_end ) )
		playSoundAtPosition( sound_end, self.origin );

	codolprintln( "CoDOL zm trap: " + self.zm_trap_weapon + " #" + self.zm_trap_id + " expired after " +
		( ( getTime() - self.zm_trap_start ) / 1000 ) + " s; zombies affected " + self.zm_trap_kills );

	self stopLoopSound();

	live = [];
	for ( i = 0; i < level.zm_traps.size; i++ )
	{
		if ( isDefined( level.zm_traps[ i ] ) && level.zm_traps[ i ] != self )
			live[ live.size ] = level.zm_traps[ i ];
	}
	level.zm_traps = live;

	self delete();
}

// A copy of the live zombie list: the spawner compacts level.zombies from
// other threads, and a zombie can die between any two waits.
live_zombies()
{
	result = [];
	if ( !isDefined( level.zombies ) )
		return result;
	for ( i = 0; i < level.zombies.size; i++ )
	{
		z = level.zombies[ i ];
		if ( isDefined( z ) && isAlive( z ) )
			result[ result.size ] = z;
	}
	return result;
}

// self = trap.  The live zombies whose feet are within radius of the trap.
zombies_in_radius( radius )
{
	result = [];
	zombies = live_zombies();
	radius_sq = radius * radius;
	for ( i = 0; i < zombies.size; i++ )
	{
		z = zombies[ i ];
		if ( abs( z.origin[ 2 ] - self.origin[ 2 ] ) > 72 )
			continue;
		if ( distanceSquared( ( z.origin[ 0 ], z.origin[ 1 ], 0 ), ( self.origin[ 0 ], self.origin[ 1 ], 0 ) ) > radius_sq )
			continue;
		result[ result.size ] = z;
	}
	return result;
}

// Kills a zombie in a trap's name: the owner is credited (zombie_death_event
// pays the kill) when still connected.  A plain function over captured
// values, because the trap itself may have expired and been deleted while
// the zombie burned or crackled.
trap_kill_zombie( z, owner, origin, trap )
{
	if ( !isDefined( z ) || !isAlive( z ) )
		return false;

	if ( isDefined( trap ) )
		level notify( "trap_kill", z, trap );
	else
		level notify( "trap_kill", z );

	if ( isDefined( owner ) && isPlayer( owner ) )
		z doDamage( z.health + 666, origin, owner );
	else
		z doDamage( z.health + 666, origin );

	return !isDefined( z ) || !isAlive( z );
}

// One-shot FX that may be a looping effect: spawned, then removed.
fx_for_time( fx_ref, origin, seconds )
{
	fx = spawnFx( level._effect[ fx_ref ], origin );
	triggerFx( fx );
	wait( seconds );
	if ( isDefined( fx ) )
		fx delete();
}

ent_desc( z )
{
	if ( !isDefined( z ) )
		return "(gone)";
	return "ent " + z getEntityNumber();
}

// ==========================================================================
// Fire.  T5 zombie_trap_death :888 fire arm; C1's 2 s per burn kill.
// ==========================================================================

fire_trap_think()
{
	self trap_start_fx( "zm_trap_fire_loop", "zmb_firetrap_start", "zmb_firetrap_loop" );

	while ( self trap_alive() )
	{
		zombies = self zombies_in_radius( 72 );
		for ( i = 0; i < zombies.size; i++ )
		{
			if ( is_true( zombies[ i ].marked_for_death ) )
				continue;
			zombies[ i ].marked_for_death = true;
			self thread fire_burn_zombie( zombies[ i ] );
		}
		wait( 0.1 );
	}

	self trap_finish( "zmb_firetrap_end" );
}

// self = trap.  T5 :909-:915: ignite, burn for up to 1.25 s, die.
fire_burn_zombie( z )
{
	trap = self;
	trap_id = self.zm_trap_id;
	owner = self.zm_trap_owner;
	origin = self.origin;

	z playSound( "zmb_ignite" );
	level thread fx_for_time( "zm_trap_fire_torso", z.origin + ( 0, 0, 40 ), 3 );
	level thread fx_for_time( "zm_trap_fire_md", z.origin + ( 0, 0, 16 ), 3 );

	wait( randomFloat( 1.25 ) );

	if ( !isDefined( z ) || !isAlive( z ) )
		return;

	who = ent_desc( z );
	if ( !trap_kill_zombie( z, owner, origin, trap ) )
	{
		if ( isDefined( z ) )
			z.marked_for_death = undefined;
		return;
	}

	if ( !isDefined( trap ) || is_true( trap.zm_trap_done ) )
	{
		codolprintln( "CoDOL zm trap: fire #" + trap_id + " burned zombie " + who + " to death after the trap went out" );
		return;
	}

	trap.zm_trap_kills++;
	trap.zm_trap_end = trap.zm_trap_end + 2000;
	codolprintln( "CoDOL zm trap: fire #" + trap_id + " burned zombie " + who + " to death; extended by 2 s, " +
		( ( trap.zm_trap_end - getTime() ) / 1000 ) + " s left" );
}

// ==========================================================================
// Freeze.  The approaching zombie is held in place, iced, for 5 s; a hit
// while frozen shatters it (T5 freezegun :409).
// ==========================================================================

freeze_trap_think()
{
	self trap_start_fx( "zm_trap_freeze_loop", undefined, "zmb_freeze_trap_loop" );

	while ( self trap_alive() )
	{
		zombies = self zombies_in_radius( 120 );
		for ( i = 0; i < zombies.size; i++ )
		{
			z = zombies[ i ];
			if ( is_true( z.trap_frozen ) || is_true( z.trap_shrunk ) )
				continue;
			if ( isDefined( z.zm_freeze_trap_id ) && z.zm_freeze_trap_id == self.zm_trap_id )
				continue;
			// animscripted() raises on an actor whose animscripts have not
			// started; a zombie tearing a board already owns the scripted
			// state (tear_into_building).
			if ( !isDefined( z.a ) || isDefined( z.attacking_spot ) )
				continue;
			z.zm_freeze_trap_id = self.zm_trap_id;
			self.zm_trap_kills++;
			z thread freeze_zombie( self.zm_trap_id, 5 );
		}
		wait( 0.1 );
	}

	self trap_finish( undefined );
}

// self = zombie.  Holds the actor in T5's AIS_SCRIPTEDANIM state (the native
// animscripted: no goal, no route, no selection, so no melee) on its current
// move pose at 1/100 speed, which is a still pose for the whole hold.
freeze_zombie( trap_id, seconds )
{
	self endon( "death" );

	self.trap_frozen = true;
	if ( !isDefined( level.zm_trap_freeze_count ) )
		level.zm_trap_freeze_count = 0;
	level.zm_trap_freeze_count++;
	self notify( "zm_trap_frozen" );
	who = ent_desc( self );

	pose = %ai_zombie_idle_v1;
	if ( isDefined( self.run_noncombatanim ) )
		pose = self.run_noncombatanim;

	self animscripted( "zm_trap_freeze", self.origin, self.angles, pose, "normal", undefined, 0.01, 0.1 );
	self playSound( "wpn_freezegun_freeze_zombie" );

	torso = spawnFx( level._effect[ "zm_trap_freeze_torso" ], self.origin + ( 0, 0, 40 ) );
	triggerFx( torso );
	legs = spawnFx( level._effect[ "zm_trap_freeze_md" ], self.origin + ( 0, 0, 12 ) );
	triggerFx( legs );

	codolprintln( "CoDOL zm trap: freeze #" + trap_id + " froze zombie " + who + " at " + self.origin + " for " + seconds + " s" );

	self thread freeze_cleanup_fx( torso, legs );
	self thread freeze_watch_shatter( trap_id, who );

	self freeze_wait( seconds );

	self notify( "zm_trap_thaw" );
	self.trap_frozen = false;

	if ( isAlive( self ) )
	{
		self stopanimscripted();
		playFx( level._effect[ "zm_trap_freeze_crumple" ], self.origin + ( 0, 0, 30 ) );
		codolprintln( "CoDOL zm trap: freeze #" + trap_id + " zombie " + who + " thawed" );
	}
}

freeze_wait( seconds )
{
	self endon( "zm_trap_shattered" );
	wait( seconds );
}

// Not endon("death"): the ice must come off a zombie that died frozen too.
freeze_cleanup_fx( torso, legs )
{
	zombie = self;
	for ( t = 0; t < 200; t++ )
	{
		if ( !isDefined( zombie ) || !isAlive( zombie ) || !is_true( zombie.trap_frozen ) )
			break;
		wait( 0.1 );
	}
	if ( isDefined( torso ) )
		torso delete();
	if ( isDefined( legs ) )
		legs delete();
}

// self = zombie.  Any player hit while frozen shatters it.
freeze_watch_shatter( trap_id, who )
{
	self endon( "death" );
	self endon( "zm_trap_thaw" );

	for ( ;; )
	{
		self waittill( "damage", amount, attacker );
		if ( !isDefined( attacker ) || !isPlayer( attacker ) )
			continue;

		origin = self.origin;
		self notify( "zm_trap_shattered" );
		playFx( level._effect[ "zm_trap_freeze_shatter" ], origin + ( 0, 0, 40 ) );
		playSoundAtPosition( "wpn_freezegun_shatter_zombie", origin );
		codolprintln( "CoDOL zm trap: freeze #" + trap_id + " zombie " + who + " shattered by " + attacker.name );
		self doDamage( self.health + 666, attacker.origin, attacker );
		return;
	}
}

// ==========================================================================
// Shock.  A shock trap kills a zombie at its feet; two shock traps within
// arc range (T5 tesla_radius_start 300) link, and the line between them is
// live too -- the "larger electric grid".
// ==========================================================================

shock_trap_think()
{
	self trap_start_fx( "zm_trap_shock_burst", "zmb_elec_start", "zmb_elec_loop" );
	self.zm_shock_links = [];
	level notify( "zm_shock_grid_changed" );

	next_arc = 0;
	while ( self trap_alive() )
	{
		self shock_update_links();

		// Zombies at this trap.
		zombies = self zombies_in_radius( 64 );
		for ( i = 0; i < zombies.size; i++ )
			self shock_zombie( zombies[ i ], self, "trap" );

		// Zombies on a live segment to a linked trap (each pair once: the
		// lower id owns the segment).
		links = self.zm_shock_links;
		for ( j = 0; j < links.size; j++ )
		{
			other = links[ j ];
			if ( !isDefined( other ) || other.zm_trap_id < self.zm_trap_id )
				continue;
			zombies = live_zombies();
			for ( i = 0; i < zombies.size; i++ )
			{
				if ( dist_to_segment_sq( zombies[ i ].origin, self.origin, other.origin ) <= 32 * 32 )
					self shock_zombie( zombies[ i ], other, "grid #" + self.zm_trap_id + "-#" + other.zm_trap_id );
			}
		}

		// The grid is visible: a bolt runs along each segment.
		if ( getTime() >= next_arc )
		{
			next_arc = getTime() + 1500;
			playFx( level._effect[ "zm_trap_shock_spark" ], self.origin + ( 0, 0, 4 ) );
			for ( j = 0; j < links.size; j++ )
			{
				if ( isDefined( links[ j ] ) && links[ j ].zm_trap_id > self.zm_trap_id )
					level thread shock_arc_fx( self.origin + ( 0, 0, 8 ), links[ j ].origin + ( 0, 0, 8 ) );
			}
		}

		wait( 0.1 );
	}

	self.zm_shock_links = [];
	level notify( "zm_shock_grid_changed" );
	self trap_finish( undefined );
}

// self = shock trap.  Recomputes which live shock traps are in arc range and
// logs each new link once.
shock_update_links()
{
	links = [];
	for ( i = 0; i < level.zm_traps.size; i++ )
	{
		other = level.zm_traps[ i ];
		if ( !isDefined( other ) || other == self || is_true( other.zm_trap_done ) )
			continue;
		if ( other.zm_trap_weapon != "trap_shock_zm" )
			continue;
		if ( distanceSquared( other.origin, self.origin ) > 300 * 300 )
			continue;
		links[ links.size ] = other;

		key = "" + other.zm_trap_id;
		if ( !isDefined( self.zm_shock_logged ) )
			self.zm_shock_logged = [];
		if ( !isDefined( self.zm_shock_logged[ key ] ) && other.zm_trap_id > self.zm_trap_id )
		{
			self.zm_shock_logged[ key ] = true;
			codolprintln( "CoDOL zm trap: shock #" + self.zm_trap_id + " linked with shock #" + other.zm_trap_id +
				" (" + int( distance( other.origin, self.origin ) ) + " units): electric grid of " + ( self shock_grid_size() ) + " traps" );
			playSoundAtPosition( "zmb_elec_arc", self.origin );
		}
	}
	self.zm_shock_links = links;
}

// self = shock trap.  The number of traps connected to this one.
shock_grid_size()
{
	seen = [];
	seen[ 0 ] = self;
	for ( k = 0; k < seen.size; k++ )
	{
		for ( i = 0; i < level.zm_traps.size; i++ )
		{
			t = level.zm_traps[ i ];
			if ( !isDefined( t ) || is_true( t.zm_trap_done ) || t.zm_trap_weapon != "trap_shock_zm" )
				continue;
			if ( distanceSquared( t.origin, seen[ k ].origin ) > 300 * 300 )
				continue;
			found = false;
			for ( j = 0; j < seen.size; j++ )
			{
				if ( seen[ j ] == t )
					found = true;
			}
			if ( !found )
				seen[ seen.size ] = t;
		}
	}
	return seen.size;
}

dist_to_segment_sq( p, a, b )
{
	ab = b - a;
	ap = p - a;
	len_sq = vectorDot( ab, ab );
	if ( len_sq <= 0 )
		return distanceSquared( p, a );
	t = vectorDot( ap, ab ) / len_sq;
	if ( t < 0 )
		t = 0;
	if ( t > 1 )
		t = 1;
	return distanceSquared( p, a + vector_scale( ab, t ) );
}

// self = shock trap.  T5 tesla_do_damage :218 minus the death-anim pick
// (CoDOL ships no tesla death clip): arc, shock FX, kill.
shock_zombie( z, source, how )
{
	if ( !isDefined( z ) || !isAlive( z ) || is_true( z.zm_shocked ) )
		return;
	z.zm_shocked = true;
	self thread shock_zombie_run( z, source, how );
}

// self = shock trap.  Captured values: the trap may expire mid-shock.
shock_zombie_run( z, source, how )
{
	trap = self;
	trap_id = self.zm_trap_id;
	owner = self.zm_trap_owner;
	origin = self.origin;
	who = ent_desc( z );
	from = source.origin + ( 0, 0, 8 );
	to = z.origin + ( 0, 0, 40 );

	level thread shock_arc_fx( from, to );
	playFx( level._effect[ "zm_trap_shock_zombie" ], to );
	z playSound( "zmb_elec_jib_zombie" );

	// T5 :939, the electrocution beat before the kill.
	wait( randomFloat( 0.5 ) );

	if ( !isDefined( z ) || !isAlive( z ) )
		return;
	playFx( level._effect[ "zm_trap_shock_zombie2" ], z.origin + ( 0, 0, 40 ) );
	playSoundAtPosition( "zmb_zombie_arc", z.origin );
	if ( !trap_kill_zombie( z, owner, origin, trap ) )
	{
		if ( isDefined( z ) )
			z.zm_shocked = undefined;
		return;
	}
	if ( isDefined( trap ) && !is_true( trap.zm_trap_done ) )
		trap.zm_trap_kills++;
	codolprintln( "CoDOL zm trap: shock #" + trap_id + " electrocuted zombie " + who + " via " + how );
}

// T5 tesla_play_arc_fx :341: the bolt rides a tag_origin from source to
// target in tesla_arc_travel_time.
shock_arc_fx( from, to )
{
	if ( distanceSquared( from, to ) < 16 * 16 )
		return;
	carrier = spawn( "script_model", from );
	carrier setModel( "tag_origin" );
	playFxOnTag( level._effect[ "zm_trap_shock_bolt" ], carrier, "tag_origin" );
	playSoundAtPosition( "zmb_elec_arc", from );
	carrier moveTo( to, 0.5 );
	wait( 0.6 );
	if ( isDefined( carrier ) )
		carrier delete();
}

// ==========================================================================
// Shrink.  T5 _zombiemode_weap_shrink_ray.gsc shrink_zombie :158 driven by a
// ground trap instead of a ray.
// ==========================================================================

shrink_trap_think()
{
	self trap_start_fx( "zm_trap_shrink_loop", undefined, "zmb_shrink_trap_lp" );

	while ( self trap_alive() )
	{
		zombies = self zombies_in_radius( 84 );
		for ( i = 0; i < zombies.size; i++ )
		{
			z = zombies[ i ];
			if ( is_true( z.trap_shrunk ) || is_true( z.trap_frozen ) )
				continue;
			if ( isDefined( z.zm_shrink_trap_id ) && z.zm_shrink_trap_id == self.zm_trap_id )
				continue;
			z.zm_shrink_trap_id = self.zm_trap_id;
			self.zm_trap_kills++;
			z thread shrink_zombie( self.zm_trap_id, self.zm_trap_owner );
		}
		wait( 0.1 );
	}

	self trap_finish( undefined );
}

// self = zombie.
shrink_zombie( trap_id, owner )
{
	self endon( "death" );

	who = ent_desc( self );
	self.trap_shrunk = true;
	if ( !isDefined( level.zm_trap_shrink_count ) )
		level.zm_trap_shrink_count = 0;
	level.zm_trap_shrink_count++;
	self notify( "shrink" );

	shrink_time = ( 2.5 + randomFloatRange( 0.0, 0.5 ) ) * 2;
	health = self.health;

	playSoundAtPosition( "evt_shrink", self.origin );
	playFx( level._effect[ "zm_trap_shrink" ], self.origin + ( 0, 0, 30 ) );

	// T5 :340 setModel( level.shrink_models[ self.model ] ); no mini body
	// ships, so the same model is drawn at half size (see the header).
	self setModelScale( 0.5 );

	self.health = 1;
	codolprintln( "CoDOL zm trap: shrink #" + trap_id + " shrank zombie " + who + " into a dwarf for " + shrink_time + " s (health " + health + " -> 1)" );

	self thread shrink_ambient_vox();
	self thread shrink_watch_kicked( trap_id, who );

	wait( shrink_time );

	self notify( "unshrink_start" );
	playSoundAtPosition( "evt_unshrink", self.origin );
	playFx( level._effect[ "zm_trap_unshrink" ], self.origin + ( 0, 0, 30 ) );
	wait( 0.5 );

	// T5 :416 restores the model and the health it saved.
	self setModelScale( 1.0 );
	self.health = health;
	self.trap_shrunk = false;
	self notify( "unshrink" );
	codolprintln( "CoDOL zm trap: shrink #" + trap_id + " zombie " + who + " grew back" );
}

// T5 :450 play_ambient_vox.
shrink_ambient_vox()
{
	self endon( "death" );
	self endon( "unshrink" );
	self endon( "kicked" );

	wait( randomFloatRange( 0.2, 0.5 ) );
	for ( ;; )
	{
		self playSound( "zmb_mini_ambient" );
		wait( randomFloatRange( 1, 2.25 ) );
	}
}

// T5 :506 watch_for_kicked.  T5 links a 30-radius trigger_radius to the
// zombie; here the same volume is tested against each player (30 + the
// player's 15-unit hull).  A player must be moving; facing the dwarf and
// moving toward it kicks it, anything else steps on it.  Movement is the
// stick (getNormalizedMovement, T5's test) or, for a body with no stick
// input, its horizontal velocity.
shrink_watch_kicked( trap_id, who )
{
	self endon( "death" );
	self endon( "unshrink" );

	for ( ;; )
	{
		wait( 0.05 );

		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( !is_player_valid( p ) )
				continue;
			if ( abs( p.origin[ 2 ] - self.origin[ 2 ] ) > 48 )
				continue;
			if ( distanceSquared( ( p.origin[ 0 ], p.origin[ 1 ], 0 ), ( self.origin[ 0 ], self.origin[ 1 ], 0 ) ) > 45 * 45 )
				continue;

			movement = p getNormalizedMovement();
			velocity = p getVelocity();
			flat_velocity = ( velocity[ 0 ], velocity[ 1 ], 0 );
			stick = length( movement ) >= 0.1;
			if ( !stick && length( flat_velocity ) < 30 )
				continue;

			to_enemy = vectorNormalize( ( self.origin[ 0 ] - p.origin[ 0 ], self.origin[ 1 ] - p.origin[ 1 ], 0 ) );
			facing = vectorDot( anglesToForward( p.angles ), to_enemy );
			if ( stick )
				toward = facing > 0.5 && movement[ 0 ] > 0;
			else
				toward = vectorDot( vectorNormalize( flat_velocity ), to_enemy ) > 0.5;

			if ( toward )
			{
				self notify( "kicked" );
				self shrink_kicked_death( p, trap_id, who );
			}
			else
			{
				self notify( "stepped_on" );
				self shrink_stepped_death( p, trap_id, who );
			}
			return;
		}
	}
}

// T5 :582 kicked_death, minus launchragdoll (not registered here): the kill
// is directed away from the kicker so the death reaction goes that way.
shrink_kicked_death( killer, trap_id, who )
{
	playSoundAtPosition( "zmb_mini_kicked", self.origin );
	codolprintln( "CoDOL zm trap: shrink #" + trap_id + " dwarf " + who + " kicked away by " + killer.name );
	self doDamage( self.health + 666, killer.origin, killer );
}

// T5 :636 shrink_death.
shrink_stepped_death( killer, trap_id, who )
{
	playSoundAtPosition( "zmb_mini_squashed", self.origin );
	playFx( level._effect[ "zm_trap_squish" ], self.origin );
	codolprintln( "CoDOL zm trap: shrink #" + trap_id + " dwarf " + who + " stepped on by " + killer.name );
	self doDamage( self.health + 666, self.origin, killer );
}


// ==========================================================================
// Test hooks, idle unless set (probe use; a test client has no stick):
//   set scr_codol_traptest "throw <client> <weapon> [<near> [<dx> <dy>]]"
//       arms <weapon> in the equipment slot with a full clip and presses the
//       equipment button, so the engine throws it and raises grenade_fire.
//       <near> > 0 first waits (up to 400 s) for a zombie within <near>
//       units of the client; <dx> <dy> first steps the client that far.
//       (A trap weapon given at runtime that is not the character's own
//       reports 0 ammo after giveStartAmmo, hence the explicit clip.)
//   set scr_codol_traptest "grid <client0> <client1> <halfwidth>"
//       waits for a chasing zombie and stands a shock trap (owned by each
//       client) <halfwidth> units either side of its path 120 units ahead, so
//       the zombie must cross the live segment between the traps.
//   set scr_codol_traptest "place <client> <weapon> <ahead> [<within>]"
//       waits for a chasing zombie (within <within> of the client when
//       given) and stands a <weapon> trap owned by that
//       client <ahead> units in front of it.
//   set scr_codol_traptest "hit <client> <seconds> <nth>"
//       once the nth zombie has been frozen, that client lands one 10-point
//       hit on a frozen zombie, as a player shooting it would (shatter).
//   set scr_codol_traptest "push <client> <seconds> [<nth>]"
//       drives that client at the nearest dwarf by velocity, which is what a
//       player running into it does; the kick test above then decides.
// ==========================================================================

trap_test_watch()
{
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_codol_traptest" );
		if ( request == "" )
			continue;
		setDvar( "scr_codol_traptest", "" );
		tokens = strTok( request, " " );
		if ( tokens.size < 3 )
			continue;
		player = undefined;
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( players[ i ] getEntityNumber() == int( tokens[ 1 ] ) )
				player = players[ i ];
		}
		if ( !isDefined( player ) )
		{
			codolprintln( "CoDOL zm trap test: no client " + tokens[ 1 ] );
			continue;
		}
		if ( tokens[ 0 ] == "grid" && tokens.size > 3 )
		{
			other = undefined;
			for ( i = 0; i < players.size; i++ )
			{
				if ( players[ i ] getEntityNumber() == int( tokens[ 2 ] ) )
					other = players[ i ];
			}
			if ( isDefined( other ) )
				level thread trap_test_grid( player, other, float( tokens[ 3 ] ) );
			continue;
		}
		if ( tokens[ 0 ] == "place" && tokens.size > 3 )
		{
			within = 0;
			if ( tokens.size > 4 )
				within = float( tokens[ 4 ] );
			level thread trap_test_place( player, tokens[ 2 ], float( tokens[ 3 ] ), within );
			continue;
		}
		if ( tokens[ 0 ] == "push" )
		{
			nth = 1;
			if ( tokens.size > 3 )
				nth = int( tokens[ 3 ] );
			player thread trap_test_push( float( tokens[ 2 ] ), nth );
		}
		else if ( tokens[ 0 ] == "hit" && tokens.size > 3 )
			player thread trap_test_hit( float( tokens[ 2 ] ), int( tokens[ 3 ] ) );
		else if ( tokens[ 0 ] == "throw" )
		{
			near = 0;
			dx = 0;
			dy = 0;
			if ( tokens.size > 3 )
				near = float( tokens[ 3 ] );
			if ( tokens.size > 5 )
			{
				dx = float( tokens[ 4 ] );
				dy = float( tokens[ 5 ] );
			}
			player thread trap_test_throw( tokens[ 2 ], near, dx, dy );
		}
	}
}

trap_test_throw( weapon, near, dx, dy )
{
	self endon( "disconnect" );

	if ( getWeaponIndexFromName( weapon ) <= 0 )
	{
		codolprintln( "CoDOL zm trap test: " + weapon + " is not loaded" );
		return;
	}

	if ( near > 0 )
	{
		deadline = getTime() + 400000;
		found = false;
		while ( !found && getTime() < deadline )
		{
			zombies = live_zombies();
			for ( i = 0; i < zombies.size; i++ )
			{
				// Inside and chasing: ignoreall drops once the zombie is
				// through its window (zombie_setup_attack_properties).
				if ( !is_true( zombies[ i ].ignoreall ) && distanceSquared( zombies[ i ].origin, self.origin ) < near * near )
					found = true;
			}
			if ( !found )
				wait( 0.25 );
		}
		codolprintln( "CoDOL zm trap test: zombie within " + near + " of " + self.name + ": " + found );
	}

	if ( dx != 0 || dy != 0 )
	{
		self setOrigin( self.origin + ( dx, dy, 0 ) );
		wait( 0.5 );
	}

	slots = [];
	for ( i = 0; i < 3; i++ )
		slots[ i ] = self getOffhandEquipment( i );
	// All four traps (and the specials) share one ammo cap,
	// 'ZOMBIE_BLACK_HOLE_BOMB' (codol_checkweapon, zm_items_weapons.txt), so
	// the character's own trap is taken first or the new one gets none.
	if ( slots[ 2 ] != weapon && slots[ 2 ] != "none" && self hasWeapon( slots[ 2 ] ) )
		self takeWeapon( slots[ 2 ] );
	slots[ 2 ] = weapon;
	self giveWeapon( weapon );
	self giveStartAmmo( weapon );
	self setWeaponAmmoClip( weapon, 2 );
	self setOffhandEquipment( slots[ 0 ], slots[ 1 ], slots[ 2 ] );
	codolprintln( "CoDOL zm trap test: " + self.name + " at " + self.origin + " throws " + weapon +
		" (ammo " + self getAmmoCount( weapon ) + ")" );
	// Look at the floor so the trap lands at the client's feet.
	self setPlayerAngles( ( 85, self.angles[ 1 ], 0 ) );
	wait( 0.5 );
	self codol_botPressButtons( 65536, 0.4 );
}

// A zombie through its window and chasing (ignoreall drops at
// zombie_setup_attack_properties), or undefined after 400 s.
trap_test_chaser( origin, within )
{
	deadline = getTime() + 400000;
	while ( getTime() < deadline )
	{
		zombies = live_zombies();
		for ( i = 0; i < zombies.size; i++ )
		{
			if ( is_true( zombies[ i ].ignoreall ) || is_true( zombies[ i ].trap_shrunk ) || is_true( zombies[ i ].trap_frozen ) )
				continue;
			if ( within > 0 && distanceSquared( zombies[ i ].origin, origin ) > within * within )
				continue;
			return zombies[ i ];
		}
		wait( 0.25 );
	}
	return undefined;
}

trap_test_place( owner, weapon, ahead, within )
{
	target = trap_test_chaser( owner.origin, within );
	if ( !isDefined( target ) || !isDefined( owner ) )
		return;
	dir = vectorNormalize( ( owner.origin[ 0 ] - target.origin[ 0 ], owner.origin[ 1 ] - target.origin[ 1 ], 0 ) );
	spot = groundpos( target.origin + vector_scale( dir, ahead ) );
	codolprintln( "CoDOL zm trap test: " + weapon + " stood " + ahead + " units ahead of chasing zombie " + ent_desc( target ) );
	spawn_trap( spot, owner, weapon );
}

trap_test_grid( a, b, half )
{
	target = trap_test_chaser( a.origin, 0 );
	if ( !isDefined( target ) || !isDefined( a ) || !isDefined( b ) )
		return;

	dir = vectorNormalize( ( a.origin[ 0 ] - target.origin[ 0 ], a.origin[ 1 ] - target.origin[ 1 ], 0 ) );
	side = ( 0 - dir[ 1 ], dir[ 0 ], 0 );
	centre = target.origin + vector_scale( dir, 120 );
	codolprintln( "CoDOL zm trap test: grid across the path of zombie " + ent_desc( target ) + " at " + centre );

	// The throw itself is proven by the other tests; test clients do not
	// hold a setOrigin, so the two traps are stood directly on the line.
	level thread spawn_trap( groundpos( centre + vector_scale( side, half ) ), a, "trap_shock_zm" );
	wait( 0.05 );
	level thread spawn_trap( groundpos( centre - vector_scale( side, half ) ), b, "trap_shock_zm" );
}

trap_test_hit( seconds, nth )
{
	self endon( "disconnect" );
	deadline = getTime() + int( seconds * 1000 );
	while ( getTime() < deadline )
	{
		wait( 0.25 );
		if ( !isDefined( level.zm_trap_freeze_count ) || level.zm_trap_freeze_count < nth )
			continue;
		zombies = live_zombies();
		for ( i = 0; i < zombies.size; i++ )
		{
			if ( !is_true( zombies[ i ].trap_frozen ) )
				continue;
			codolprintln( "CoDOL zm trap test: " + self.name + " hits frozen zombie " + ent_desc( zombies[ i ] ) );
			zombies[ i ] doDamage( 10, self.origin, self );
			return;
		}
	}
}

trap_test_push( seconds, nth )
{
	self endon( "disconnect" );
	while ( !isDefined( level.zm_trap_shrink_count ) || level.zm_trap_shrink_count < nth )
		wait( 0.1 );
	deadline = getTime() + int( seconds * 1000 );
	reported = false;
	while ( getTime() < deadline )
	{
		target = undefined;
		best = 0;
		zombies = live_zombies();
		for ( i = 0; i < zombies.size; i++ )
		{
			if ( !is_true( zombies[ i ].trap_shrunk ) )
				continue;
			d = distanceSquared( zombies[ i ].origin, self.origin );
			if ( !isDefined( target ) || d < best )
			{
				target = zombies[ i ];
				best = d;
			}
		}
		if ( isDefined( target ) )
		{
			if ( !reported )
			{
				reported = true;
				// Start the run-up 90 units short of the dwarf, on the side
				// this client is on, so walls between them do not matter.
				back = vectorNormalize( ( self.origin[ 0 ] - target.origin[ 0 ], self.origin[ 1 ] - target.origin[ 1 ], 0 ) );
				self setOrigin( target.origin + vector_scale( back, 90 ) + ( 0, 0, 4 ) );
				codolprintln( "CoDOL zm trap test: pushing " + self.name + " at dwarf " + ent_desc( target ) + " from " + self.origin );
			}
			dir = vectorNormalize( ( target.origin[ 0 ] - self.origin[ 0 ], target.origin[ 1 ] - self.origin[ 1 ], 0 ) );
			self setVelocity( vector_scale( dir, 190 ) );
		}
		wait( 0.05 );
	}
}
