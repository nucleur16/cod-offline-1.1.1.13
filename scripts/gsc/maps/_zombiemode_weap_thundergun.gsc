// zm Thundergun: the air blast that knocks down, gibs and flings zombies.
//
// Port of T5's maps\_zombiemode_weap_thundergun.gsc (Treyarch, Call of Duty:
// Black Ops; D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\
// _zombiemode_weap_thundergun.gsc, 401 lines) and the view effects of
// clientscripts\_zombiemode_weap_thundergun.csc.  Line numbers below are into
// the .gsc unless the .csc is named.
//
// WHY SCRIPT HAS TO DO IT.  thundergun_zm / thundergun_upgraded_zm fire
// nothing that hurts: T5 makes the whole effect in script off the player's
// "weapon_fired" (:59), and until this file existed the gun did nothing here.
//
// TAKEN, by T5 function:
//   init                         :8   -- effects, the six tuning vars, the
//                                        gib refs; plus the knockdown/getup
//                                        tables T5 keeps in _zombiemode.gsc
//                                        init_anims (:1185-:1255)
//   wait_for_thundergun_fired    :56  -- per player, the smoke cloud
//   thundergun_fired             :88  -- PhysicsExplosionCylinder, then the
//                                        fling and knockdown passes
//   thundergun_network_choke     :76
//   thundergun_get_enemies_in_range :120 -- cone/cylinder selection, unchanged
//   thundergun_fling_zombie      :253 -- kill, StartRagdoll + LaunchRagdoll
//   thundergun_knockdown_zombie  :287 -- 15 damage, the gib, the knockdown
//   handle_thundergun_pain_notetracks :321
//   is_thundergun_damage / enemy_killed_by_thundergun :331 / :337
//   T5 _zombiemode_spawner.gsc:4940 zombie_knockdown (the zombie's
//   thundergun_knockdown_func) as the default knockdown
//   T5 animscripts\zombie_pain.gsc:374 playThundergunPainAnim, in
//   animscripts\zombie_pain.gsc
//
// BUILTINS this needed that the server lacked, now native:
//   getweaponmuzzlepoint / getweaponforwarddir, launchragdoll,
//   getaispeciesarray (src/codol/script/codol_script_ai_arrays.c,
//   src/codol/player/codol_actor_ragdoll.c), and physicsexplosioncylinder now
//   reaches C1 (event 96).  launchragdoll rides C1's own ragdoll impact
//   (StartRagdollFromImpact's state words); see codol_actor_ragdoll.c.
//
// C1 / THIS-SERVER INTERPRETATION, each a stated difference:
//   * T5 :65 puts the smoke cloud at GetTagOrigin("tag_flash") minus the view
//     height.  A player's world model here may lack tag_flash, and a missing
//     tag is a fatal script error, so the cloud starts at the weapon muzzle
//     point 32 units along the gun.
//   * T5 plays vox_thundergun_forcehit, wpn_thundergun_proj_impact and
//     fly_thundergun_forcehit.  None of them ships in a CoDOL zone
//     (docs/zone_assets/ALL_sound.txt), so each is played only if it exists.
//   * T5's damageYaw is the yaw of the damage DIRECTION; this server's actor
//     damageyaw is the yaw towards the attacker, 0..360
//     (codol_actor_lifecycle.cpp ActorRecordDamage).  The pain script turns
//     it back into T5's -180..180 before choosing the fall.
//   * Points: T5 pays a fling kill only through "thundergun_fling" (its
//     zombie_death_animscript returns early for a thundergun death, :3360).
//     Here _zombiemode_spawner::zombie_death_event pays "death" for every
//     player kill, so it skips a .thundergun_death kill and this file pays
//     the fling points, as T5 does.
//   * setup_thundergun_vox (:375): the kill dialog runs through
//     _zombiemode_audio::create_and_play_dialog if that port has one.
//   * The view effects (.csc): C1 has no client scripts; the power cells and
//     the steam run here through playViewModelFxOnTag, as the tesla bulbs do
//     (_zombiemode_weap_tesla.gsc).  Viewmodel notetracks never reach the
//     server, so the cells light whenever the gun is out with ammo.

#include maps\_zombiemode_utility;

#using_animtree( "generic_human" );

init()
{
	level.zm_thundergun_ok = false;

	// Developer-only test verbs (scr_weaptest); returns at once on a
	// non-developer server.
	maps\_zombiemode_weaptest::init();

	precacheItem( "thundergun_zm" );
	if ( !codol_isWeaponMounted( "thundergun_zm" ) )
		return;

	// T5 :17-27.  All ship in zm_prototype_sh (docs/zone_assets).
	level._effect[ "thundergun_viewmodel_power_cell1" ] = loadfx( "weapon/thunder_gun/fx_thundergun_power_cell_view1" );
	level._effect[ "thundergun_viewmodel_power_cell2" ] = loadfx( "weapon/thunder_gun/fx_thundergun_power_cell_view2" );
	level._effect[ "thundergun_viewmodel_power_cell3" ] = loadfx( "weapon/thunder_gun/fx_thundergun_power_cell_view3" );
	level._effect[ "thundergun_viewmodel_steam" ]       = loadfx( "weapon/thunder_gun/fx_thundergun_steam_view" );
	level._effect[ "thundergun_knockdown_ground" ]      = loadfx( "weapon/thunder_gun/fx_thundergun_knockback_ground" );
	level._effect[ "thundergun_smoke_cloud" ]           = loadfx( "weapon/thunder_gun/fx_thundergun_smoke_cloud" );

	// T5 :30.
	set_zombie_var( "thundergun_cylinder_radius",  180 );
	set_zombie_var( "thundergun_fling_range",      480 ); // 40 feet
	set_zombie_var( "thundergun_gib_range",        900 ); // 75 feet
	set_zombie_var( "thundergun_gib_damage",       75 );
	set_zombie_var( "thundergun_knockdown_range",  1200 ); // 100 feet
	set_zombie_var( "thundergun_knockdown_damage", 15 );

	level.thundergun_gib_refs = [];
	level.thundergun_gib_refs[ level.thundergun_gib_refs.size ] = "guts";
	level.thundergun_gib_refs[ level.thundergun_gib_refs.size ] = "right_arm";
	level.thundergun_gib_refs[ level.thundergun_gib_refs.size ] = "left_arm";

	init_knockdown_anims();

	level.zm_thundergun_ok = true;
	level thread thundergun_on_player_connect();
}

// T5 _zombiemode.gsc init_anims :1185-:1255, "zombie" animname only (the one
// CoDOL's zombies use).  Every clip ships in common_gametype_zm or
// zm_prototype_sh (docs/zone_assets/ALL_xanim.txt).
init_knockdown_anims()
{
	k = [];
	k[ "front" ] = [];
	k[ "front" ][ "no_legs" ] = [];
	level.tg_building_crawler = true;
	k[ "front" ][ "no_legs" ][ 0 ] = tg_anim( "ai_zombie_thundergun_hit_armslegsforward" );
	k[ "front" ][ "no_legs" ][ 1 ] = tg_anim( "ai_zombie_thundergun_hit_doublebounce" );
	k[ "front" ][ "no_legs" ][ 2 ] = tg_anim( "ai_zombie_thundergun_hit_forwardtoface" );
	level.tg_building_crawler = undefined;
	if ( !isDefined( codol_getAnim( "generic_human", "ai_zombie_thundergun_hit_doublebounce" ) ) )
		k[ "front" ][ "no_legs" ][ 1 ] = codol_getAnim( "generic_human", "ai_zombie_walk_on_hands_shot_b" );

	f = [];
	f[ 0 ] = tg_anim( "ai_zombie_thundergun_hit_armslegsforward" );
	f[ 1 ] = tg_anim( "ai_zombie_thundergun_hit_doublebounce" );
	f[ 2 ] = tg_anim( "ai_zombie_thundergun_hit_upontoback" );
	f[ 3 ] = tg_anim( "ai_zombie_thundergun_hit_forwardtoface" );
	f[ 4 ] = tg_anim( "ai_zombie_thundergun_hit_armslegsforward" );
	f[ 5 ] = tg_anim( "ai_zombie_thundergun_hit_forwardtoface" );
	f[ 6 ] = tg_anim( "ai_zombie_thundergun_hit_stumblefall" );
	f[ 7 ] = tg_anim( "ai_zombie_thundergun_hit_armslegsforward" );
	f[ 8 ] = tg_anim( "ai_zombie_thundergun_hit_doublebounce" );
	f[ 9 ] = tg_anim( "ai_zombie_thundergun_hit_upontoback" );
	f[ 10 ] = tg_anim( "ai_zombie_thundergun_hit_forwardtoface" );
	f[ 11 ] = tg_anim( "ai_zombie_thundergun_hit_armslegsforward" );
	f[ 12 ] = tg_anim( "ai_zombie_thundergun_hit_forwardtoface" );
	f[ 13 ] = tg_anim( "ai_zombie_thundergun_hit_deadfallknee" );
	f[ 14 ] = tg_anim( "ai_zombie_thundergun_hit_armslegsforward" );
	f[ 15 ] = tg_anim( "ai_zombie_thundergun_hit_doublebounce" );
	f[ 16 ] = tg_anim( "ai_zombie_thundergun_hit_upontoback" );
	f[ 17 ] = tg_anim( "ai_zombie_thundergun_hit_forwardtoface" );
	f[ 18 ] = tg_anim( "ai_zombie_thundergun_hit_armslegsforward" );
	f[ 19 ] = tg_anim( "ai_zombie_thundergun_hit_forwardtoface" );
	f[ 20 ] = tg_anim( "ai_zombie_thundergun_hit_flatonback" );
	k[ "front" ][ "has_legs" ] = f;

	k[ "left" ] = [];
	k[ "left" ][ 0 ] = tg_anim( "ai_zombie_thundergun_hit_legsout_right" );
	k[ "right" ] = [];
	k[ "right" ][ 0 ] = tg_anim( "ai_zombie_thundergun_hit_legsout_left" );
	k[ "back" ] = [];
	k[ "back" ][ 0 ] = tg_anim( "ai_zombie_thundergun_hit_faceplant" );

	g = [];
	g[ "back" ] = [];
	g[ "back" ][ "early" ] = [];
	g[ "back" ][ "early" ][ 0 ] = tg_anim( "ai_zombie_thundergun_getup_b" );
	g[ "back" ][ "early" ][ 1 ] = tg_anim( "ai_zombie_thundergun_getup_c" );
	g[ "back" ][ "late" ] = [];
	g[ "back" ][ "late" ][ 0 ] = tg_anim( "ai_zombie_thundergun_getup_b" );
	g[ "back" ][ "late" ][ 1 ] = tg_anim( "ai_zombie_thundergun_getup_c" );
	g[ "back" ][ "late" ][ 2 ] = tg_anim( "ai_zombie_thundergun_getup_quick_b" );
	g[ "back" ][ "late" ][ 3 ] = tg_anim( "ai_zombie_thundergun_getup_quick_c" );
	g[ "belly" ] = [];
	g[ "belly" ][ "early" ] = [];
	g[ "belly" ][ "early" ][ 0 ] = tg_anim( "ai_zombie_thundergun_getup_a" );
	g[ "belly" ][ "late" ] = [];
	g[ "belly" ][ "late" ][ 0 ] = tg_anim( "ai_zombie_thundergun_getup_a" );
	g[ "belly" ][ "late" ][ 1 ] = tg_anim( "ai_zombie_thundergun_getup_quick_a" );

	if ( !isDefined( level._zombie_knockdowns ) )
		level._zombie_knockdowns = [];
	if ( !isDefined( level._zombie_getups ) )
		level._zombie_getups = [];
	level._zombie_knockdowns[ "zombie" ] = k;
	level._zombie_getups[ "zombie" ] = g;
}

// The knockdown clips by name, never %anim.  C1's client builds its
// generic_human tree only from the animlists it loads (common,
// common_actor_shared, common_zombie, the map), and none of them names the
// ai_zombie_thundergun_* clips.  A %anim reference added those 16 nodes to the
// SERVER tree only, which shifted every later entry index on the actor
// animation channel: C1 then played the wrong clip, or nothing (a T-pose), for
// every zombie animation past entry 231 -- the walk cycles included
// (tools/codol_coop_anim_check.py: "TREES DIFFER: server 263, client 247").
// codol_getAnim answers only what the client's tree has, so on C1 this is
// undefined and the knockdown is not played.
tg_anim( name )
{
	a = codol_getAnim( "generic_human", name );
	if ( isDefined( a ) )
		return a;
	// zm_sumpf_sh's own animlist ships most of T5's knockdown set (the
	// server logs it: "CoDOL animlists: zm_sumpf_sh.animlist ... 48 entries")
	// but not these three; a sibling T5 clip comes before the C1 stand-in.
	sibling = undefined;
	switch ( name )
	{
	case "ai_zombie_thundergun_hit_forwardtoface":
		sibling = "ai_zombie_thundergun_hit_faceplant";
		break;
	case "ai_zombie_thundergun_hit_deadfallknee":
		sibling = "ai_zombie_thundergun_hit_stumblefall";
		break;
	case "ai_zombie_thundergun_getup_quick_a":
		sibling = "ai_zombie_thundergun_getup_a";
		break;
	}
	if ( isDefined( sibling ) )
	{
		a = codol_getAnim( "generic_human", sibling );
		if ( isDefined( a ) )
			return a;
	}
	return codol_getAnim( "generic_human", tg_c1_standin( name ) );
}

// C1's stand-ins for T5's thundergun knockdown clips where the map ships
// none of them: common_zombie and the Prototype / Asylum / Training lists
// have no fall or getup at all (2026-10-21); only zm_sumpf_sh carries T5's:
//   the falls    -> ai_zombie_humangun_react, C1's stagger-back hit reaction
//                   (1.33 s; T5's falls run 1.3-2.4 s);
//   a crawler    -> ai_zombie_walk_on_hands_shot_a/b, the crawler's own hit
//                   reaction;
//   the getups   -> ai_zombie_flash_react_01, the dazed recover (4.53 s,
//                   played faster by tg_getup_rate to T5's ~3 s getup).
tg_c1_standin( name )
{
	switch ( name )
	{
	case "ai_zombie_thundergun_hit_armslegsforward":
	case "ai_zombie_thundergun_hit_doublebounce":
	case "ai_zombie_thundergun_hit_forwardtoface":
		if ( isDefined( level.tg_building_crawler ) )
			return "ai_zombie_walk_on_hands_shot_a";
		return "ai_zombie_humangun_react";
	case "ai_zombie_thundergun_getup_a":
	case "ai_zombie_thundergun_getup_b":
	case "ai_zombie_thundergun_getup_c":
	case "ai_zombie_thundergun_getup_quick_a":
	case "ai_zombie_thundergun_getup_quick_b":
	case "ai_zombie_thundergun_getup_quick_c":
		return "ai_zombie_flash_react_01";
	default:
		return "ai_zombie_humangun_react";
	}
}

// Playback rate for a getup: 1 for T5's own clip, faster for the C1 daze so
// the zombie is up in T5's time.
tg_getup_rate( getupAnim )
{
	daze = codol_getAnim( "generic_human", "ai_zombie_flash_react_01" );
	if ( isDefined( daze ) && isDefined( getupAnim ) && getupAnim == daze )
		return getAnimLength( daze ) / 3.0;
	return 1;
}

is_thundergun_weapon( weapon )
{
	return isDefined( weapon ) && ( weapon == "thundergun_zm" || weapon == "thundergun_upgraded_zm" );
}

// T5 :45.
thundergun_on_player_connect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread wait_for_thundergun_fired();
		player thread thundergun_viewmodel_fx_power_cell();
	}
}

// T5 :56.  self is the player.  weapon_fired carries the weapon here
// (_g_weapon.asm FireWeapon), which is the gun that actually fired.
wait_for_thundergun_fired()
{
	self endon( "disconnect" );

	for ( ;; )
	{
		self waittill( "weapon_fired", weapon );
		if ( !isDefined( weapon ) )
			weapon = self getCurrentWeapon();
		if ( !is_thundergun_weapon( weapon ) )
			continue;

		self thread thundergun_fired( weapon );
		self thread thundergun_viewmodel_fx_fire( weapon );

		forward = self getWeaponForwardDir();
		view_pos = self getWeaponMuzzlePoint() + vector_scale( forward, 32 );
		playFx( level._effect[ "thundergun_smoke_cloud" ], view_pos, forward, anglesToUp( vectorToAngles( forward ) ) );
	}
}

// T5 :76.
thundergun_network_choke()
{
	level.thundergun_network_choke_count++;

	if ( !( level.thundergun_network_choke_count % 10 ) )
	{
		wait_network_frame();
		wait_network_frame();
		wait_network_frame();
	}
}

// T5 :88.  self is the player.
thundergun_fired( weapon )
{
	// Carry the actual weapon_fired name through throttled child threads.
	if ( !is_thundergun_weapon( weapon ) )
		return;
	source = self getEye();
	// ww: physics hit when firing
	physicsExplosionCylinder( self.origin, 600, 240, 1 );

	// T5 :93-:118: the selection fills four level lists.
	self thundergun_get_enemies_in_range();

	fling_enemies = level.thundergun_sel_fling;
	fling_vecs = level.thundergun_sel_fling_vecs;
	knockdown_enemies = level.thundergun_sel_knockdown;
	knockdown_gib = level.thundergun_sel_gib;

	codolPrintLn( "CoDOL zm thundergun: " + self.name + " fired " + self getCurrentWeapon() + ": " +
		fling_enemies.size + " fling, " + knockdown_enemies.size + " knockdown" );

	level.thundergun_network_choke_count = 0;
	for ( i = 0; i < fling_enemies.size; i++ )
	{
		thundergun_network_choke();
		fling_enemies[ i ] thread thundergun_fling_zombie( self, fling_vecs[ i ], i, weapon, source );
	}

	for ( i = 0; i < knockdown_enemies.size; i++ )
	{
		thundergun_network_choke();
		knockdown_enemies[ i ] thread thundergun_knockdown_zombie( self, knockdown_gib[ i ], weapon, source );
	}
}

// T5 :120.  The enemy list is T5's: every live axis actor, dogs included.
thundergun_get_enemies_in_range()
{
	level.thundergun_sel_fling = [];
	level.thundergun_sel_fling_vecs = [];
	level.thundergun_sel_knockdown = [];
	level.thundergun_sel_gib = [];

	view_pos = self getWeaponMuzzlePoint();
	zombies = thundergun_closest( view_pos, getAiSpeciesArray( "axis", "all" ), level.zombie_vars[ "thundergun_knockdown_range" ] );
	if ( !isDefined( zombies ) )
		return;

	knockdown_range_squared = level.zombie_vars[ "thundergun_knockdown_range" ] * level.zombie_vars[ "thundergun_knockdown_range" ];
	gib_range_squared = level.zombie_vars[ "thundergun_gib_range" ] * level.zombie_vars[ "thundergun_gib_range" ];
	fling_range_squared = level.zombie_vars[ "thundergun_fling_range" ] * level.zombie_vars[ "thundergun_fling_range" ];
	cylinder_radius_squared = level.zombie_vars[ "thundergun_cylinder_radius" ] * level.zombie_vars[ "thundergun_cylinder_radius" ];

	forward_view_angles = self getWeaponForwardDir();
	end_pos = view_pos + vector_scale( forward_view_angles, level.zombie_vars[ "thundergun_knockdown_range" ] );

	for ( i = 0; i < zombies.size; i++ )
	{
		if ( !isDefined( zombies[ i ] ) || !isAlive( zombies[ i ] ) )
			continue; // guy died on us

		test_origin = zombies[ i ] thundergun_centroid();
		test_range_squared = distanceSquared( view_pos, test_origin );
		if ( test_range_squared > knockdown_range_squared )
			return; // everything else in the list will be out of range

		normal = vectorNormalize( test_origin - view_pos );
		dot = vectorDot( forward_view_angles, normal );
		if ( 0 > dot )
			continue; // guy's behind us

		radial_origin = pointOnSegmentNearestToPoint( view_pos, end_pos, test_origin );
		if ( distanceSquared( test_origin, radial_origin ) > cylinder_radius_squared )
			continue; // outside the cylinder of effect

		if ( 0 == zombies[ i ] damageConeTrace( view_pos, self ) )
			continue; // can't actually be hit from where we are

		if ( test_range_squared < fling_range_squared )
		{
			level.thundergun_sel_fling[ level.thundergun_sel_fling.size ] = zombies[ i ];

			// the closer they are, the harder they get flung
			dist_mult = ( fling_range_squared - test_range_squared ) / fling_range_squared;
			fling_vec = vectorNormalize( test_origin - view_pos );

			// within 6 feet, just push them straight away from the player, ignoring radial motion
			if ( 5000 < test_range_squared )
				fling_vec = fling_vec + vectorNormalize( test_origin - radial_origin );
			fling_vec = ( fling_vec[ 0 ], fling_vec[ 1 ], abs( fling_vec[ 2 ] ) );
			fling_vec = vector_scale( fling_vec, 100 + 100 * dist_mult );
			level.thundergun_sel_fling_vecs[ level.thundergun_sel_fling_vecs.size ] = fling_vec;

			zombies[ i ] thread setup_thundergun_vox( self, true, false, false );
		}
		else if ( test_range_squared < gib_range_squared )
		{
			level.thundergun_sel_knockdown[ level.thundergun_sel_knockdown.size ] = zombies[ i ];
			level.thundergun_sel_gib[ level.thundergun_sel_gib.size ] = true;

			zombies[ i ] thread setup_thundergun_vox( self, false, true, false );
		}
		else
		{
			level.thundergun_sel_knockdown[ level.thundergun_sel_knockdown.size ] = zombies[ i ];
			level.thundergun_sel_gib[ level.thundergun_sel_gib.size ] = false;

			zombies[ i ] thread setup_thundergun_vox( self, false, false, true );
		}
	}
}

// maps\_utility::get_array_of_closest( org, array, undefined, undefined,
// maxdist ), which zm does not load: closest first, within maxdist.
thundergun_closest( org, array, maxdist )
{
	sorted = [];
	dists = [];
	max_sq = maxdist * maxdist;
	for ( i = 0; i < array.size; i++ )
	{
		d = distanceSquared( org, array[ i ].origin );
		if ( d > max_sq )
			continue;
		j = sorted.size;
		while ( j > 0 && dists[ j - 1 ] > d )
		{
			sorted[ j ] = sorted[ j - 1 ];
			dists[ j ] = dists[ j - 1 ];
			j--;
		}
		sorted[ j ] = array[ i ];
		dists[ j ] = d;
	}
	return sorted;
}

// T5 getcentroid(): the middle of the actor's bounds.  A standing zombie's
// hull is 72 tall, a hound's about 40 (tesla_chest_point's numbers).
thundergun_centroid()
{
	if ( is_true( self.isdog ) )
		return self.origin + ( 0, 0, 20 );
	if ( !is_true( self.has_legs ) )
		return self.origin + ( 0, 0, 12 );
	return self.origin + ( 0, 0, 36 );
}

// Native C1 adaptation: CODOL_Scr_DoDamage uses the MP 2..8 argument
// contract. Its short3 form defaults headshot/MOD/weapon/inflictor; retain
// accepted4dba full attribution while keeping upstream selection/FX/ragdoll.
// T5 :253.  self is the zombie.
thundergun_fling_zombie( player, fling_vec, index, weapon, source )
{
	if ( !isDefined( player ) || !isPlayer( player ) || !is_thundergun_weapon( weapon ) )
		return;
	if ( !isDefined( self ) || !isAlive( self ) )
		return; // guy died on us

	if ( isDefined( self.thundergun_fling_func ) )
	{
		self [[ self.thundergun_fling_func ]]( player );
		return;
	}

	dist = distance( player.origin, self.origin );

	// Marked before the kill: zombie_death_event reads it at frame end and
	// leaves the payment to "thundergun_fling" below.
	self.thundergun_death = true;
	self doDamage( self.health + 666, source, player, player, 0, "MOD_PROJECTILE", 0, weapon );

	if ( self.health <= 0 )
	{
		points = 10;
		if ( !index )
			points = maps\_zombiemode_score::get_zombie_death_player_points();
		else if ( 1 == index )
			points = 30;
		player maps\_zombiemode_score::player_add_points( "thundergun_fling", points );

		self startRagdoll();
		self launchRagdoll( fling_vec );

		codolPrintLn( "CoDOL zm thundergun: flung zombie " + self getEntityNumber() + " at " + int( dist ) +
			" units, force " + fling_vec + " (" + int( length( fling_vec ) ) + "), ragdoll " + self isRagdoll() +
			", " + points + " points" );
	}
	else
		self.thundergun_death = undefined;
}

// T5 :287.  self is the zombie.
thundergun_knockdown_zombie( player, gib, weapon, source )
{
	if ( !isDefined( player ) || !isPlayer( player ) || !is_thundergun_weapon( weapon ) )
		return;
	self endon( "death" );

	play_sound_if_exists( "vox_thundergun_forcehit", self.origin );
	play_sound_if_exists( "wpn_thundergun_proj_impact", self.origin );

	if ( !isDefined( self ) || !isAlive( self ) )
		return; // guy died on us

	dist = distance( player.origin, self.origin );
	health = self.health;

	if ( isDefined( self.thundergun_knockdown_func ) )
		self [[ self.thundergun_knockdown_func ]]( player, gib, weapon, source );
	else
		self zombie_knockdown( player, gib, weapon, source );

	// T5 :307-:318 then gibs and hurts once more after the func, whichever
	// func ran (both hits land in this frame, so the pain script starts
	// once).
	if ( !isAlive( self ) )
		return;
	if ( gib && !is_true( self.isdog ) )
	{
		self.a.gib_ref = common_scripts\utility::random( level.thundergun_gib_refs );
		self thread maps\_zombiemode_gib::do_gib();
	}
	self.thundergun_handle_pain_notetracks = ::handle_thundergun_pain_notetracks;
	if ( !is_true( self.isdog ) )
		self.thundergun_knockdown = true;
	self doDamage( level.zombie_vars[ "thundergun_knockdown_damage" ], source, player, player, 0, "MOD_PROJECTILE", 0, weapon );
	play_sound_if_exists( "fly_thundergun_forcehit", self.origin );

	codolPrintLn( "CoDOL zm thundergun: knocked down zombie " + self getEntityNumber() + " at " + int( dist ) +
		" units, gib " + gib + ", health " + health + " -> " + self.health );
}

// T5 _zombiemode_spawner.gsc:4940 zombie_knockdown, the regular zombie's
// thundergun_knockdown_func.  The pain animscript reads
// .thundergun_knockdown to play the fall instead of returning.
zombie_knockdown( player, gib, weapon, source )
{
	if ( gib && !is_true( self.gibbed ) )
	{
		self.a.gib_ref = common_scripts\utility::random( level.thundergun_gib_refs );
		self thread maps\_zombiemode_gib::do_gib();
	}

	self.thundergun_handle_pain_notetracks = ::handle_thundergun_pain_notetracks;
	self.thundergun_knockdown = true;
	self doDamage( level.zombie_vars[ "thundergun_knockdown_damage" ], source, player, player, 0, "MOD_PROJECTILE", 0, weapon );
	play_sound_if_exists( "fly_thundergun_forcehit", self.origin );
}

// T5 :321.
handle_thundergun_pain_notetracks( note )
{
	if ( note == "zombie_knockdown_ground_impact" )
	{
		playFx( level._effect[ "thundergun_knockdown_ground" ], self.origin, anglesToForward( self.angles ), anglesToUp( self.angles ) );
		play_sound_if_exists( "fly_thundergun_forcehit", self.origin );
	}
}

// T5 :331.
is_thundergun_damage( weapon, mod )
{
	return is_thundergun_weapon( weapon ) && isDefined( mod ) && mod != "MOD_GRENADE" && mod != "MOD_GRENADE_SPLASH";
}

// T5 :337.
enemy_killed_by_thundergun()
{
	return is_true( self.thundergun_death );
}

// T5 :375.  self is the zombie being hit.
setup_thundergun_vox( player, fling, gib, knockdown )
{
	if ( !isDefined( self ) || !isAlive( self ) )
		return;

	if ( fling && 30 > randomIntRange( 1, 100 ) && isDefined( level.zombie_thundergun_kill_vox_func ) )
		player thread [[ level.zombie_thundergun_kill_vox_func ]]();
}

play_sound_if_exists( alias, origin )
{
	if ( soundExists( alias ) )
		playSoundAtPosition( alias, origin );
}

// .csc thundergun_fx_power_cell / thundergun_play_power_cell_fx (:56-:146):
// the three power cells on tag_bulb1..3, as many lit as there are rounds in
// the clip (at most three), refreshed every 3 s while the gun is out.  The
// cell effects loop, so a cell is stopped when its round is spent and all of
// them when the gun goes away.
thundergun_viewmodel_fx_power_cell()
{
	self endon( "disconnect" );

	lit = 0;
	last = 0;
	for ( ;; )
	{
		wait( 0.1 );

		weapon = self getCurrentWeapon();
		count = 0;
		if ( is_thundergun_weapon( weapon ) && isAlive( self ) )
		{
			count = self getWeaponAmmoClip( weapon );
			if ( count > 3 )
				count = 3;
		}

		if ( count == lit && ( !count || getTime() - last < 3000 ) )
			continue;

		for ( i = lit; i > count; i-- )
			self stopViewModelFxOnTag( level._effect[ "thundergun_viewmodel_power_cell" + i ], "tag_bulb" + i );
		for ( i = count; i > 0; i-- )
			self playViewModelFxOnTag( level._effect[ "thundergun_viewmodel_power_cell" + i ], "tag_bulb" + i );
		lit = count;
		last = getTime();
	}
}

// .csc thundergun_fx_fire (:148): steam from the three vents and the breath.
thundergun_viewmodel_fx_fire( weapon )
{
	for ( i = 3; i > 0; i-- )
		self playViewModelFxOnTag( level._effect[ "thundergun_viewmodel_steam" ], "tag_steam" + i );
	if ( soundExists( "wpn_thunder_breath" ) )
		self playLocalSound( "wpn_thunder_breath" );
}
