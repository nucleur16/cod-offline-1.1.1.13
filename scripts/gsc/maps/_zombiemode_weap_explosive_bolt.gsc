// zm explosive bolts: the Explosive Crossbow and the Scavenger.
//
// Port of T5's bolt pair:
//   maps\_zombiemode_weap_crossbow.gsc          watch_for_monkey_bolt :28,
//                                               crossbow_monkey_bolt  :55
//   maps\_zombiemode_weap_sniper_explosive.gsc  watch_for_sniper_bolt :125,
//                                               ubersniper_bolt_audio :175,
//                                               sniper_explosive_death_response :61-:91
//   clientscripts\_explosive_bolt.csc           spawned / loop_local_sound
//   clientscripts\_zombiemode_weap_sniper_explosive.csc  the death mist
// and of the native half those scripts sit on, T5 g_missile.cpp MissileImpact
// case WEAPPROJEXP_BOLT: the bolt sticks, a grenade of the launcher's
// szSpawnedGrenadeWeaponName is fired there with that weapon's fuse, and its
// owner hears grenade_fire( grenade, name ).
//
// WHAT THE ENGINE DOES (src/codol/weapon/codol_bolt_attachment.c): the bolt
// of crossbow_explosive_zm / sniper_explosive_zm (and the upgraded pair)
// sticks where it hits, follows the zombie or player it hit, and raises
// grenade_fire on its owner with the spawned name -- explosive_bolt_zm,
// explosive_bolt_upgraded_zm, sniper_explosive_bolt_zm or
// sniper_explosive_bolt_upgraded_zm, read from the C1 definition (+b84).
// _zombiemode_equipment's grenade_fire registry hands it to this module.
//
// WHAT THIS MODULE DOES: what the spawned grenade weapon would have done.
// Those four weapons are named in C1's zm constant weapon window but no CoDOL
// zone ships their definitions (docs/zone_assets/ALL_weapon.txt), so their
// numbers are carried here.  MEASURED 2026-10-04 from Black Ops' own
// fastfiles (zone/Common/zombie_cod5_prototype.ff and zombie_coast.ff,
// decompressed, decoded with OpenBLOPS bg_weapons.h WeaponDef, sizeof 0x808;
// the decode reproduces crossbow_explosive_zm's radius 1, damage 150/100 and
// impact 750 exactly as C1 ships them):
//
//   spawned weapon                     fuse ms  radius  inner   outer
//   explosive_bolt_zm                   1500     256     400     75
//   explosive_bolt_upgraded_zm          4000     200     2500    225
//   sniper_explosive_bolt_zm            3000     360     10000   10000
//   sniper_explosive_bolt_upgraded_zm   3000     480     10000   10000
//
// The upgraded crossbow row is the second timed grenade every T5 crossbow
// level carries next to explosive_bolt_zm (prototype, asylum, theater, coast);
// its four-second fuse is the "monkey bolt" lure time.
//
// ADAPTED FOR C1:
//   * The blast is radiusDamage with MOD_GRENADE_SPLASH (T5's MOD for a
//     grenade's splash), attributed to the LAUNCHER, because the spawned
//     names have no weapon index on this server for G_Damage to carry.
//   * Presentation the T5 grenade drew client-side is played from here with
//     C1's own assets: the bolt's blink (weapon/crossbow/
//     fx_trail_crossbow_blink_grn_os / _red_os) and wpn_crossbow_alert every
//     0.3 s (T5 _explosive_bolt.csc), the Scavenger's wpn_ubersniper_rampup /
//     wpn_ubersniper_snapshot (T5 ubersniper_bolt_audio), and the blast:
//     weapon/jackal/fx_jackal_exp / _ug with wpn_ubersniper_bomb_explo /
//     _pap for the Scavenger, explosions/grenadeexp_default with
//     grenade_explode_default for the crossbow (common_mp; T5's bolt is a
//     plain grenade explosion).
//   * The Scavenger death mist (T5 setclientflag 13 -> csc playfx
//     fx_zmb_coast_jackal_death at J_SpineLower) is played by the server, and
//     the body is hidden as T5 hides it; the corpse itself stays with the
//     native actor lifecycle rather than being deleted from script.
//   * The upgraded crossbow's "monkey bolt" is a point of interest through
//     _zombiemode_poi, as T5 :88-:166 makes it; the holder's taunt animation
//     (T5 monkey_bolt_taunts) is not taken.

#include maps\_zombiemode_utility;

init()
{
	level.zm_explosive_bolts = [];

	// Both launchers have to be in this map's box roster (T5 :9 / :15).
	crossbow = maps\_zombiemode_weapons::is_weapon_included( "crossbow_explosive_zm" );
	sniper = maps\_zombiemode_weapons::is_weapon_included( "sniper_explosive_zm" );
	if ( !crossbow && !sniper )
	{
		{}
		return;
	}

	// The armed bolt's grenade danger indicator: C1 only draws it for a timed
	// frag definition, so the engine marks each armed bolt with an unseen
	// stielhandgranate (C1 zm weapon window 101; no trail or beacon effect),
	// falling back to frag_grenade_zm (codol_bolt_attachment.c).  Registered
	// here, before any wait, as precacheItem requires.
	precacheItem( "stielhandgranate" );

	if ( crossbow )
	{
		level._effect[ "crossbow_friendly_light" ] = loadfx( "weapon/crossbow/fx_trail_crossbow_blink_grn_os" );
		level._effect[ "crossbow_enemy_light" ] = loadfx( "weapon/crossbow/fx_trail_crossbow_blink_red_os" );
		level._effect[ "explosive_bolt_explode" ] = loadfx( "explosions/grenadeexp_default" );

		add_bolt( "explosive_bolt_zm", "crossbow_explosive_zm", 1.5, 256, 400, 75,
			"explosive_bolt_explode", "grenade_explode_default", false );
		add_bolt( "explosive_bolt_upgraded_zm", "crossbow_explosive_upgraded_zm", 4.0, 200, 2500, 225,
			"explosive_bolt_explode", "grenade_explode_default", true );
	}

	if ( sniper )
	{
		level._effect[ "sniper_explosive_explode" ] = loadfx( "weapon/jackal/fx_jackal_exp" );
		level._effect[ "sniper_explosive_explode_ug" ] = loadfx( "weapon/jackal/fx_jackal_exp_ug" );
		level._effect[ "sniper_explosive_death_mist" ] = loadfx( "maps/zombie/fx_zmb_coast_jackal_death" );

		add_bolt( "sniper_explosive_bolt_zm", "sniper_explosive_zm", 3.0, 360, 10000, 10000,
			"sniper_explosive_explode", "wpn_ubersniper_bomb_explo", false );
		add_bolt( "sniper_explosive_bolt_upgraded_zm", "sniper_explosive_upgraded_zm", 3.0, 480, 10000, 10000,
			"sniper_explosive_explode_ug", "wpn_ubersniper_bomb_explo_pap", false );

		// T5 sniper_explosive_death_response: a Scavenger blast kill is a mist.
		if ( !isDefined( level.zombie_kill_weapon_callbacks ) )
			level.zombie_kill_weapon_callbacks = [];
		level.zombie_kill_weapon_callbacks[ "sniper_explosive_zm" ] = ::sniper_explosive_kill;
		level.zombie_kill_weapon_callbacks[ "sniper_explosive_upgraded_zm" ] = ::sniper_explosive_kill;
	}

	keys = getArrayKeys( level.zm_explosive_bolts );
	for ( i = 0; i < keys.size; i++ )
		maps\_zombiemode_equipment::register_equipment( keys[ i ], ::explosive_bolt_armed );

	{}
}

add_bolt( spawned, launcher, fuse, radius, inner, outer, fx, sound, monkey )
{
	s = spawnStruct();
	s.launcher = launcher;
	s.fuse = fuse;
	s.radius = radius;
	s.inner = inner;
	s.outer = outer;
	s.fx = fx;
	s.sound = sound;
	s.monkey = monkey;
	s.sniper = isSubStr( spawned, "sniper" );
	level.zm_explosive_bolts[ spawned ] = s;
}

// self = the stuck bolt, from _zombiemode_equipment::watch_equipment_fire on
// the engine's grenade_fire( bolt, spawned name ).  T5's spawned grenade from
// here on: arm, blink, lure, explode.
explosive_bolt_armed( player, weapon )
{
	bolt = self;
	s = level.zm_explosive_bolts[ weapon ];
	if ( !isDefined( s ) )
		return;

	info = spawnStruct();
	info.exploded = false;
	info.origin = bolt.origin;

	if ( s.sniper )
		bolt thread ubersniper_bolt_audio( info );
	else
		bolt thread crossbow_bolt_blink( info, s.monkey );

	if ( s.monkey )
		bolt thread crossbow_monkey_bolt( info );

	// The fuse.  The bolt follows whatever it is stuck in, so the blast is
	// wherever the bolt is when it runs out.
	waited = 0;
	while ( waited < s.fuse )
	{
		wait( 0.05 );
		waited += 0.05;
		if ( !isDefined( bolt ) )
		{
			// T5's fuse belongs to its grenade entity. Removing that entity
			// cancels the blast; do not explode at a cached old position after
			// pickup, native cleanup, or deletion of an attached carrier.
			info.exploded = true;
			return;
		}
		info.origin = bolt.origin;
	}

	info.exploded = true;
	origin = info.origin;

	if ( isDefined( bolt ) )
	{
		if ( is_true( bolt.is_zombie_poi ) )
			bolt maps\_zombiemode_poi::remove_zombie_point_of_interest();
		bolt notify( "explode", origin );
	}

	// The bolt is the blast's inflictor, as T5's spawned grenade is: the
	// engine then measures each target from its origin, T5's rule, rather than
	// from the edge of its box, C1's rule for its own explosions, which made
	// the T5 radius reach about a body's width further and ignored the height
	// of a bolt stuck in a chest (codol_bolt_attachment.c
	// CODOL_RadiusDamageFromOrigin).
	if ( isDefined( bolt ) )
	{
		if ( isDefined( player ) && isPlayer( player ) )
			bolt radiusDamage( origin, s.radius, s.inner, s.outer, player, "MOD_GRENADE_SPLASH", s.launcher );
		else
			bolt radiusDamage( origin, s.radius, s.inner, s.outer );
	}
	else if ( isDefined( player ) && isPlayer( player ) )
		radiusDamage( origin, s.radius, s.inner, s.outer, player, "MOD_GRENADE_SPLASH", s.launcher );
	else
		radiusDamage( origin, s.radius, s.inner, s.outer );

	if ( isDefined( level._effect[ s.fx ] ) )
		playFx( level._effect[ s.fx ], origin );
	playSoundAtPosition( s.sound, origin );

	{}

	if ( isDefined( bolt ) )
		bolt delete();
}

// T5 _explosive_bolt.csc spawned/loop_local_sound: the alert beep and the
// blinking light every 0.3 s until the bolt is gone.  Green for the shooter's
// side; the upgraded (monkey) bolt shows red, as T5 shows it to everyone.
crossbow_bolt_blink( info, monkey )
{
	bolt = self;
	fx = level._effect[ "crossbow_friendly_light" ];
	if ( monkey )
		fx = level._effect[ "crossbow_enemy_light" ];

	while ( isDefined( bolt ) && !info.exploded )
	{
		bolt playSound( "wpn_crossbow_alert" );
		if ( isDefined( fx ) )
			playFx( fx, bolt.origin );
		wait( 0.3 );
	}
}

// T5 ubersniper_bolt_audio :175.
ubersniper_bolt_audio( info )
{
	bolt = self;
	bolt playSound( "wpn_ubersniper_rampup" );
	wait( 2.1 );
	if ( isDefined( bolt ) && !info.exploded )
		bolt playSound( "wpn_ubersniper_snapshot" );
}

// T5 crossbow_monkey_bolt :55: the upgraded bolt lures every zombie within
// 1536 units until it blows.
crossbow_monkey_bolt( info )
{
	bolt = self;

	attract_dist_diff = level.monkey_attract_dist_diff;
	if ( !isDefined( attract_dist_diff ) )
		attract_dist_diff = 45;
	num_attractors = level.num_monkey_attractors;
	if ( !isDefined( num_attractors ) )
		num_attractors = 96;
	max_attract_dist = level.monkey_attract_dist;
	if ( !isDefined( max_attract_dist ) )
		max_attract_dist = 1536;

	bolt maps\_zombiemode_poi::create_zombie_point_of_interest( max_attract_dist, num_attractors, 10000, true );
	bolt maps\_zombiemode_poi::create_zombie_point_of_interest_attractor_positions( 4, attract_dist_diff );
}

// T5 sniper_explosive_death_response / the csc mist.  Run on the attacker by
// _zombiemode_spawner::zombie_death_event for a Scavenger kill.
sniper_explosive_kill( zombie, weapon )
{
	if ( !isDefined( zombie ) )
		return;
	if ( !isDefined( zombie.damagemod ) || ( zombie.damagemod != "MOD_GRENADE_SPLASH" && zombie.damagemod != "MOD_GRENADE" ) )
		return;

	origin = zombie.origin + ( 0, 0, 40 );
	if ( isDefined( level._effect[ "sniper_explosive_death_mist" ] ) )
		playFx( level._effect[ "sniper_explosive_death_mist" ], origin );
	zombie hide();
}
