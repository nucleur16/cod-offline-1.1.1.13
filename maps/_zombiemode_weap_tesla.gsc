// zm tesla gun: the chain lightning that IS the weapon's damage.
//
// Port of T5's maps\_zombiemode_weap_tesla.gsc (570 lines,
// D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\_zombiemode_weap_tesla.gsc).  Line
// numbers below are into that file unless another is named.
//
// WHY SCRIPT HAS TO DO THE DAMAGE.  MEASURED (codol_checkweapon tesla_gun_zm,
// docs/zm_evidence/zm_items_weapons.txt:1264-1282): tesla_gun_zm is a
// projectile weapon (type 2, class 6), clip 3 / max 18, fire time 1000 ms,
// base/min damage 1/0.  The projectile tickles; T5's chain is what kills.  T5
// starts the chain from _zombiemode_spawner.gsc:3548 (zombie_damage ->
// is_tesla_damage -> tesla_damage_init); this server's counterpart is the
// clearly delimited hook in _zombiemode_spawner::zombie_damage_watch, which
// reads the weapon out of the actor "damage" notify's tenth argument
// (src/asmsource/_g_combat_mp.asm G_Damage_230, CODOL_AddDamageWeaponArgument).
//
// TAKEN, by T5 function:
//   init                    :8    -- effects and the eight tuning vars.
//   tesla_damage_init       :46   -- one chain per player at a time.
//   tesla_arc_damage        :83   -- the recursive arc.
//   tesla_end_arc_damage    :119  -- max arcs / max kills / radius decay.
//   tesla_get_enemies_in_area :146 -- closest-first, sight-traced.
//   tesla_flag_hit          :202
//   tesla_do_damage         :218  -- the staggered kill.
//   tesla_play_death_fx     :316
//   tesla_play_arc_fx       :341  -- see ARC FX below.
//   is_tesla_damage         :405
//   tesla_network_choke     :553  -- the per-player death choke.
//
// C1 / THIS-SERVER INTERPRETATION, each a stated difference:
//   * The enemy list is level.zombies, not GetAiSpeciesArray("axis","all"):
//     that builtin is not registered here, and level.zombies is the live list
//     _zombiemode_spawner keeps (every zm enemy is in it).
//   * Positions come from the actor origin plus a fixed height, not
//     GetTagOrigin("j_head"/"J_SpineUpper"): the retained GScr_GetTagOrigin
//     raises a script error for a tag the model lacks, and a script error
//     shuts the server down.
//   * EFFECT LIFETIMES decide how each effect may be played.  MEASURED from
//     the FxEffectDef headers in common_gametype_zm (msecLoopingLife /
//     looping elements):
//         fx_zombie_tesla_bolt_secondary   0x7fffffff / 2  -- loops forever
//         fx_zombie_tesla_shock                  8700 / 12
//         fx_zombie_tesla_shock_secondary        3480 / 7
//     playFx spawns an effect at a world point that no event can reach
//     again (C1 0x444f10: spawn, then drop the only reference at 0x444fab),
//     so an effect that loops forever must never be played that way.  Until
//     2026-09-30 the arc was four playFx of the bolt along the source-target
//     line, which left every arc hanging in the air for the rest of the map
//     (the "zapped zombie FX gets stuck" report).  The bolt now rides a
//     tag_origin carrier exactly as T5 :341 does; the carrier gets an explicit
//     stopFxOnTag, one network frame, then delete, so both the stop event and
//     the entity's removal reach every client.
//   * Death FX: T5 :316 plays the shock on J_SpineUpper, so it follows the
//     falling body and ends with the corpse.  MEASURED (codol_checkmodel,
//     2026-09-30): every body in character_zm_zombie's rotation, and the
//     four full-skeleton gib bodies a wound swaps in, carries j_spineupper
//     as bone 10; any other model falls back to playFx at the chest point,
//     which is safe for these two finite effects.
//   * Death FX TIMING.  An entity carries four events per snapshot, and a
//     client that receives more between two snapshots it gets loses the
//     oldest.  MEASURED 2026-10-21 (zm_prototype_sh probe,
//     codol_traceActorEvents): T5's order (shock fx, impact sound, eye fx,
//     then the kill) put three events on the zombie in its kill frame and
//     the death animscript three more (stopsounds, the eye-glow stop, the
//     death vocal) one frame later -- six in 17 ms, so the shock effect,
//     queued first, was the one pushed out: the "tesla kills draw no shock"
//     report.  The kill now comes first; the shock and the eyes go on the
//     dying body 0.15 s later, after that burst, and the impact sound is a
//     positional sound that uses none of the zombie's slots.
//   * The arc's carrier is a new entity.  An entity event only reaches a
//     client that already had the entity in a snapshot when it was queued
//     (the rule _zombiemode_spawner::zombie_eye_glow_refresh works around),
//     so an effect played on the carrier's spawn frame can be lost.  The
//     bolt is put on the carrier 0.1 s after it is spawned, then it
//     travels.
//   * T5 :305 pays "death" points itself after the kill.  Not taken: this
//     port's _zombiemode_spawner::zombie_death_event already pays the player
//     named as the attacker of the kill, which doDamage below supplies, so
//     taking :305 would pay every tesla kill twice.
//   * Not taken: tesla_sound_thread / tesla_engine_sweets (idle loop sound
//     keyed off weapon_change), tesla_pvp_thread (weapon_pvp_attack is a T5
//     engine notify this server does not raise), tesla_killstreak_sound
//     (_zombiemode_audio is not ported), the head-gib func (no gib tags, see
//     above) and the dog/crawler death-anim selection (no tesla death clips
//     are registered for this port's zombies).

#include maps\_zombiemode_utility;

#using_animtree ("generic_human");

init()
{
	level.zm_tesla_ok = false;

	// T5 _zombiemode.gsc init_anims: the tesla death sets.
	level._zombie_tesla_death = [];
	level._zombie_tesla_death[ 0 ] = %ai_zombie_tesla_death_a;
	level._zombie_tesla_death[ 1 ] = %ai_zombie_tesla_death_b;
	level._zombie_tesla_death[ 2 ] = %ai_zombie_tesla_death_c;
	level._zombie_tesla_death[ 3 ] = %ai_zombie_tesla_death_d;
	level._zombie_tesla_death[ 4 ] = %ai_zombie_tesla_death_e;
	level._zombie_tesla_crawl_death = [];
	level._zombie_tesla_crawl_death[ 0 ] = %ai_zombie_tesla_crawl_death_a;
	level._zombie_tesla_crawl_death[ 1 ] = %ai_zombie_tesla_crawl_death_b;
	level._effect[ "tesla_shock_eyes" ] = loadfx( "maps/zombie/fx_zombie_tesla_shock_eyes" );

	precacheItem( "tesla_gun_zm" );
	if ( !codol_isWeaponMounted( "tesla_gun_zm" ) )
		return;

	// The arc carrier (T5 :376).
	precacheModel( "tag_origin" );

	// T5 :15.  All three ship in common_gametype_zm
	// (docs/CODOL_ZM_EQUIPMENT_EVIDENCE_20260924.md section 4).
	level._effect[ "tesla_bolt" ]            = loadfx( "maps/zombie/fx_zombie_tesla_bolt_secondary" );
	level._effect[ "tesla_shock" ]           = loadfx( "maps/zombie/fx_zombie_tesla_shock" );
	level._effect[ "tesla_shock_secondary" ] = loadfx( "maps/zombie/fx_zombie_tesla_shock_secondary" );

	// T5 :32, the same eight vars and defaults.
	set_zombie_var( "tesla_max_arcs",            5 );
	set_zombie_var( "tesla_max_enemies_killed",  20 );
	set_zombie_var( "tesla_radius_start",        300 );
	set_zombie_var( "tesla_radius_decay",        20 );
	set_zombie_var( "tesla_head_gib_chance",     50 );
	set_zombie_var( "tesla_arc_travel_time",     0.5, true );
	set_zombie_var( "tesla_min_fx_distance",     128 );
	set_zombie_var( "tesla_network_death_choke", 4 );

	// T5 clientscripts\_zombiemode_weap_tesla.csc :17, the viewmodel effects
	// (bulbs and rail); all eight ship in common_gametype_zm.
	level._effect[ "tesla_viewmodel_rail" ]           = loadfx( "maps/zombie/fx_zombie_tesla_rail_view" );
	level._effect[ "tesla_viewmodel_tube" ]           = loadfx( "maps/zombie/fx_zombie_tesla_tube_view" );
	level._effect[ "tesla_viewmodel_tube2" ]          = loadfx( "maps/zombie/fx_zombie_tesla_tube_view2" );
	level._effect[ "tesla_viewmodel_tube3" ]          = loadfx( "maps/zombie/fx_zombie_tesla_tube_view3" );
	level._effect[ "tesla_viewmodel_rail_upgraded" ]  = loadfx( "maps/zombie/fx_zombie_tesla_rail_view_ug" );
	level._effect[ "tesla_viewmodel_tube_upgraded" ]  = loadfx( "maps/zombie/fx_zombie_tesla_tube_view_ug" );
	level._effect[ "tesla_viewmodel_tube2_upgraded" ] = loadfx( "maps/zombie/fx_zombie_tesla_tube_view2_ug" );
	level._effect[ "tesla_viewmodel_tube3_upgraded" ] = loadfx( "maps/zombie/fx_zombie_tesla_tube_view3_ug" );

	level.zm_tesla_ok = true;
	level thread on_player_connect();
}

// T5 :405.  The spawner hook calls this with the damage notify's weapon.
is_tesla_damage( weapon, mod )
{
	if ( !is_true( level.zm_tesla_ok ) )
		return false;
	if ( !isDefined( weapon ) || !isDefined( mod ) )
		return false;
	if ( weapon != "tesla_gun_zm" && weapon != "tesla_gun_upgraded_zm" )
		return false;
	return ( mod == "MOD_PROJECTILE" || mod == "MOD_PROJECTILE_SPLASH" );
}

// T5 :46.  Threaded on level by the spawner hook, NOT called on the zombie:
// zombie_damage_watch ends on the zombie's own death, which is the first
// thing the chain causes, and T5 runs this synchronously inside
// zombie_damage where no such endon exists.
tesla_damage_init( zombie, hit_location, hit_origin, player )
{
	player endon( "disconnect" );

	if ( !isDefined( zombie ) || !isAlive( zombie ) )
		return;

	if ( isDefined( player.tesla_enemies_hit ) && player.tesla_enemies_hit > 0 )
		return;

	if ( is_true( zombie.zombie_tesla_hit ) )
		return;

	player.tesla_enemies = undefined;
	player.tesla_enemies_hit = 1;
	player.tesla_arc_count = 0;
	if ( !isDefined( player.tesla_network_death_choke ) )
		player.tesla_network_death_choke = 0;

	codolPrintLn( "CoDOL zm tesla: " + player.name + " hit zombie " + zombie getEntityNumber() + "; chain starts" );

	zombie tesla_arc_damage( zombie, player, 1 );

	codolPrintLn( "CoDOL zm tesla: chain by " + player.name + " reached " + player.tesla_enemies_hit + " zombies" );

	player.tesla_enemies_hit = 0;
}

// T5 :83.  self is the enemy being arced to.
tesla_arc_damage( source_enemy, player, arc_num )
{
	player endon( "disconnect" );

	self.zombie_tesla_hit = true;
	head = tesla_head_point( self );

	wait_network_frame();

	radius_decay = level.zombie_vars[ "tesla_radius_decay" ] * arc_num;
	enemies = tesla_get_enemies_in_area( head, level.zombie_vars[ "tesla_radius_start" ] - radius_decay, player );
	tesla_flag_hit( enemies, true );

	if ( isDefined( self ) )
		self thread tesla_do_damage( source_enemy, arc_num, player );

	for ( i = 0; i < enemies.size; i++ )
	{
		enemy = enemies[ i ];
		if ( !isDefined( enemy ) )
			continue;

		if ( isDefined( self ) && enemy == self )
			continue;

		if ( tesla_end_arc_damage( arc_num + 1, player.tesla_enemies_hit ) )
		{
			enemy.zombie_tesla_hit = false;
			continue;
		}

		player.tesla_enemies_hit++;
		enemy tesla_arc_damage( self, player, arc_num + 1 );
	}
}

// T5 :119.
tesla_end_arc_damage( arc_num, enemies_hit_num )
{
	if ( arc_num >= level.zombie_vars[ "tesla_max_arcs" ] )
		return true;

	if ( enemies_hit_num >= level.zombie_vars[ "tesla_max_enemies_killed" ] )
		return true;

	radius_decay = level.zombie_vars[ "tesla_radius_decay" ] * arc_num;
	if ( level.zombie_vars[ "tesla_radius_start" ] - radius_decay <= 0 )
		return true;

	return false;
}

// T5 :146.  Closest first, cached per chain on the player exactly as T5
// caches player.tesla_enemies.
tesla_get_enemies_in_area( origin, distance, player )
{
	distance_squared = distance * distance;
	enemies = [];

	if ( !isDefined( player.tesla_enemies ) )
		player.tesla_enemies = tesla_sorted_zombies( origin );

	zombies = player.tesla_enemies;

	for ( i = 0; i < zombies.size; i++ )
	{
		zombie = zombies[ i ];
		if ( !isDefined( zombie ) || !isAlive( zombie ) )
			continue;

		if ( is_true( zombie.zombie_tesla_hit ) )
			continue;

		test_origin = tesla_head_point( zombie );

		if ( distanceSquared( origin, test_origin ) > distance_squared )
			continue;

		if ( !bulletTracePassed( origin, test_origin, false, undefined ) )
			continue;

		enemies[ enemies.size ] = zombie;
	}

	return enemies;
}

// T5 get_array_of_closest( origin, GetAiSpeciesArray( "axis", "all" ) ),
// over level.zombies.  Insertion sort: the list is capped at
// level.zombie_ai_limit (24).
tesla_sorted_zombies( origin )
{
	sorted = [];
	dists = [];

	if ( !isDefined( level.zombies ) )
		return sorted;

	for ( i = 0; i < level.zombies.size; i++ )
	{
		zombie = level.zombies[ i ];
		if ( !isDefined( zombie ) || !isAlive( zombie ) )
			continue;

		d = distanceSquared( origin, zombie.origin );
		j = sorted.size;
		while ( j > 0 && dists[ j - 1 ] > d )
		{
			sorted[ j ] = sorted[ j - 1 ];
			dists[ j ] = dists[ j - 1 ];
			j--;
		}
		sorted[ j ] = zombie;
		dists[ j ] = d;
	}

	return sorted;
}

// T5 :202.
tesla_flag_hit( enemies, hit )
{
	for ( i = 0; i < enemies.size; i++ )
	{
		if ( isDefined( enemies[ i ] ) )
			enemies[ i ].zombie_tesla_hit = hit;
	}
}

// T5 :218.  self is the enemy.
tesla_do_damage( source_enemy, arc_num, player )
{
	player endon( "disconnect" );

	if ( arc_num > 1 )
		wait( randomFloatRange( 0.2, 0.6 ) * arc_num );

	if ( !isDefined( self ) || !isAlive( self ) )
		return;

	if ( isDefined( source_enemy ) && source_enemy != self )
	{
		if ( player.tesla_arc_count > 3 )
		{
			wait_network_frame();
			player.tesla_arc_count = 0;
		}

		player.tesla_arc_count++;
		tesla_play_arc_fx( tesla_chest_point( source_enemy ), self );
	}

	while ( player.tesla_network_death_choke > level.zombie_vars[ "tesla_network_death_choke" ] )
		wait( 0.05 );

	if ( !isDefined( self ) || !isAlive( self ) )
		return;

	player.tesla_network_death_choke++;

	self.tesla_death = true;

	// T5 :234-:243: a tesla kill plays one of the shock deaths
	// (ai_zombie_tesla_death_a-e, crawlers _crawl_death_a/b; all ship in
	// common_gametype_zm).
	if ( !is_true( self.isdog ) && isDefined( level._zombie_tesla_death ) )
	{
		if ( is_true( self.has_legs ) )
			self.deathanim = level._zombie_tesla_death[ randomInt( level._zombie_tesla_death.size ) ];
		else
			self.deathanim = level._zombie_tesla_crawl_death[ randomInt( level._zombie_tesla_crawl_death.size ) ];
	}

	// T5 :284: the arc's originator picks the death direction.
	origin = player.origin;
	if ( isDefined( source_enemy ) && source_enemy != self )
		origin = source_enemy.origin;

	// The kill, then the death fx (T5 :316) once the death animscript's own
	// events are out -- see DEATH FX TIMING in the header.
	level thread tesla_play_death_fx( self, arc_num );

	if ( isDefined( self.tesla_damage_func ) )
	{
		self [[ self.tesla_damage_func ]]( origin, player );
		return;
	}

	self doDamage( self.health + 666, origin, player );
}

// T5 :316.  On the body's J_SpineUpper, see the header.  A level thread:
// the zombie is dead (or dying) by the time it plays.
tesla_play_death_fx( zombie, arc_num )
{
	fx = "tesla_shock";
	if ( arc_num > 1 )
		fx = "tesla_shock_secondary";

	origin = tesla_chest_point( zombie );
	playSoundAtPosition( "wpn_imp_tesla", origin );

	// T5 :331 tesla_head_gib_func = zombie_tesla_head_gib (spawner :4963).
	// Its head gib is behind endon( "death" ) and the kill lands in the same
	// frame, so in T5 it practically never fires (docs/
	// CODOL_ZOMBIE_T5_PARITY.md 6.7); the eyes are what a tesla kill shows.
	eyes = !is_true( zombie.isdog ) && !is_true( zombie.head_gibbed ) &&
		randomInt( 100 ) >= level.zombie_vars[ "tesla_head_gib_chance" ];

	wait( 0.15 );

	if ( !isDefined( zombie ) )
	{
		playFx( level._effect[ fx ], origin );
		return;
	}

	if ( tesla_has_spine_tag( zombie ) )
		playFxOnTag( level._effect[ fx ], zombie, "j_spineupper" );
	else
		playFx( level._effect[ fx ], tesla_chest_point( zombie ) );

	if ( eyes && isDefined( level._effect[ "tesla_shock_eyes" ] ) && !is_true( zombie.head_gibbed ) )
		playFxOnTag( level._effect[ "tesla_shock_eyes" ], zombie, "j_eyeball_le" );

	codolPrintLn( "CoDOL zm tesla: death fx " + fx + " on zombie " + zombie getEntityNumber() + " (" + zombie.model +
		", spine tag " + tesla_has_spine_tag( zombie ) + ", eyes " + eyes + ")" );
}

// playFxOnTag raises a fatal script error for a tag the model lacks, so the
// tag is used only on the measured zombie bodies.
tesla_has_spine_tag( ent )
{
	if ( !isDefined( ent.model ) )
		return false;

	bodies = character\character_zm_zombie::zombie_bodies();
	for ( i = 0; i < bodies.size; i++ )
	{
		if ( ent.model == bodies[ i ] )
			return true;
	}

	// The full-skeleton gib bodies (character_zm_zombie: _upclean,
	// _rarmoff_1/2, _larmoff_1/2, _torso_1, 73 bones each).
	prefixes = character\character_zm_zombie::zombie_gib_prefixes();
	upper = [];
	upper[ 0 ] = "upclean";
	upper[ 1 ] = "rarmoff_1";
	upper[ 2 ] = "rarmoff_2";
	upper[ 3 ] = "larmoff_1";
	upper[ 4 ] = "larmoff_2";
	upper[ 5 ] = "torso_1";
	for ( i = 0; i < prefixes.size; i++ )
	{
		for ( j = 0; j < upper.size; j++ )
		{
			if ( ent.model == prefixes[ i ] + upper[ j ] )
				return true;
		}
	}
	return false;
}

// T5 :341.  The bolt travels from the source to the target in
// tesla_arc_travel_time; T5 skips it for enemies closer than
// tesla_min_fx_distance.  The caller waits the travel time, as T5 waits for
// "movedone".
tesla_play_arc_fx( origin, target )
{
	if ( !isDefined( target ) )
	{
		wait( level.zombie_vars[ "tesla_arc_travel_time" ] );
		return;
	}

	target_origin = tesla_chest_point( target );
	min_dist = level.zombie_vars[ "tesla_min_fx_distance" ];
	if ( distanceSquared( origin, target_origin ) < min_dist * min_dist )
		return;

	playSoundAtPosition( "wpn_tesla_bounce", origin );
	level thread tesla_arc_bolt( origin, target_origin );
	wait( level.zombie_vars[ "tesla_arc_travel_time" ] );
}

// T5 :373-:382, the bolt on its carrier.  A level thread with no endon:
// the caller's thread dies with the player's disconnect, and a carrier left
// undeleted would keep the forever-looping bolt alive on every client.
tesla_arc_bolt( origin, target_origin )
{
	travel = level.zombie_vars[ "tesla_arc_travel_time" ];

	carrier = spawn( "script_model", origin );
	carrier setModel( "tag_origin" );
	// Not on the spawn frame: see the header.
	wait( 0.1 );
	playFxOnTag( level._effect[ "tesla_bolt" ], carrier, "tag_origin" );
	carrier moveTo( target_origin, travel );
	wait( travel );

	// Stop first, and give the stop one frame to go out before the entity
	// it rides on is freed: a stop queued on a freed entity is never sent.
	stopFxOnTag( level._effect[ "tesla_bolt" ], carrier, "tag_origin" );
	wait_network_frame();
	carrier delete();
}

// Standing actor heights (the zombie aitype's 72-unit hull): the head test
// point and the chest point T5 takes from j_head and J_SpineUpper.
tesla_head_point( ent )
{
	return ent.origin + ( 0, 0, 60 );
}

tesla_chest_point( ent )
{
	// T5 :187 plays a hound's shock on J_Spine1, a dog's back, not a standing
	// zombie's chest.
	if ( is_true( ent.isdog ) )
		return ent.origin + ( 0, 0, 20 );
	return ent.origin + ( 0, 0, 48 );
}

// T5 :417 / :553.  The choke counter every tesla_do_damage waits on.
on_player_connect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread tesla_network_choke();
		player thread tesla_viewmodel_fx_tube();
		player thread tesla_viewmodel_fx_tube_on_fire();
		player thread tesla_viewmodel_fx_tube_on_raise();
		player thread tesla_viewmodel_fx_rail();
		player thread tesla_viewmodel_fx_grenade_watch();
		player thread tesla_viewmodel_trace_events();
	}
}

// VIEWMODEL EFFECTS.  T5 plays the bulb glow and the rail spark from its
// client script (clientscripts\_zombiemode_weap_tesla.csc tesla_fx_tube :92,
// tesla_fx_rail :46) with PlayViewmodelFx.  C1 has no client scripts; its
// counterpart is the server player method PlayViewModelFXOnTag (C1 544ff0,
// event 92 to that player only), which this server registers as
// playViewModelFxOnTag.  The loops below are T5's, run per player here:
//   * tube: every 0.1 s, the bulb effect on tag_brass, three/two/one lit by
//     the clip (T5 :123-153); the effect lives 300 ms, so it stays lit.
//   * rail: every 8-12 s, the rail spark on tag_flash and wpn_tesla_effects,
//     not while aiming down the sights (T5 :46-89).
//
// TIMING.  T5 gates both on level.tesla_play_fx (tesla_notetrack_think
// :160), which the viewmodel notetracks switch: off at the reload's
// sndnt#wpn_tesla_switch_flip_off and at sndnt#wpn_tesla_first_raise_start,
// on at sndnt#wpn_tesla_switch_flip_on, tesla_pullout_start (the first frame
// of every raise) and tesla_idle_start (idle after anything else, a cancelled
// reload included).  C1 moved that gate into the animations themselves.
// MEASURED 2026-10-22 (codol_checkxanim on the C1 assets): the reload
// carries maskfxbytag#tag_brass and #tag_flash with flip_off at 0.1218 of
// the animation and unmaskfxbytag with flip_on at 0.7563; the first raise
// masks both at 0 and unmasks with its flip_on at 0.4625.  The weapon plays
// its reload over 6000 ms (upgraded 4699 ms) and its first raise over
// 2500 ms, so the bulbs go out 731 ms into a reload (572 upgraded) and come
// back at 4538 ms (3554), and a first raise lights them at 1156 ms.  In C1
// the notetrack handler (C1 7342c0) flags the tag's viewmodel bone and kills
// what plays there, PlayViewModelFX's event handler (44fc00, 44fc9d) drops
// an effect on a flagged bone, and starting any other viewmodel animation
// (47c550, 47cc16) clears every flag -- T5's idle_start/pullout_start.
// So the server's part is T5's: keep the bulbs playing while the gun is held
// with rounds in the clip, reload or not, and the client masks them on the
// animation's own clock (the reload's speed modifiers included).
//
// Before this, a 0.1 s check took the bulbs off for the whole reload:
// out at its first frame instead of 731 ms in, and back only when it
// finished (6000 ms, plus the poll) instead of at 4538 ms.  An empty clip
// also showed one bulb from the last shot until the reload's flip_off.
//
// Per shot: T5's client reads its predicted clip every 0.1 s.  The server
// sees the shot as "weapon_fired" in the frame it is fired, so the count is
// replayed then (tesla_viewmodel_fx_tube_on_fire) rather than at the next
// tick, which keeps the network delay from adding a poll's worth on top.
// A raise ("weapon_change" to the gun) is lit at once the same way.
//
// Differences, each forced by what this server can see:
//   * IsThrowingGrenade / IsMeleeing are client queries with no server
//     builtin here; a grenade throw is tracked from grenade_pullback /
//     grenade_fire instead, and melee is not checked.  IsOnTurret is
//     isUsingTurret.
//   * tesla_happy's "TGH" pause of the rail is not taken: its notify comes
//     from tesla_killstreak_sound, which this port does not carry.
//   * A reload cancelled with an empty magazine (sprint, melee, a switch,
//     going down) leaves the clip at 0, where T5 lights nothing; the user
//     asked for the bulbs to come back after any cancelled reload, so after
//     a reload that ended without filling the clip, an empty clip with
//     rounds in reserve shows the one-round glow until the next shot or
//     reload.  An empty clip after the last shot stays dark, as in T5.
//
// scr_zm_tesla_vmtrace 1 logs every bulb play and weapon event with its
// time (gettime) for timing checks.
tesla_viewmodel_weapon()
{
	if ( !isAlive( self ) )
		return undefined;
	weapon = self getCurrentWeapon();
	if ( !isDefined( weapon ) || ( weapon != "tesla_gun_zm" && weapon != "tesla_gun_upgraded_zm" ) )
		return undefined;
	if ( is_true( self.zm_tesla_throwing ) || self isUsingTurret() )
		return undefined;
	return weapon;
}

// Rounds the bulbs show: the clip, or one for an empty clip with rounds in
// reserve after a cancelled reload (see above).
tesla_viewmodel_ammo( weapon )
{
	ammo = self getWeaponAmmoClip( weapon );
	if ( ammo > 0 )
		self.zm_tesla_reload_cancelled = false;
	else if ( is_true( self.zm_tesla_reload_cancelled ) && self getWeaponAmmoStock( weapon ) > 0 )
		ammo = 1;
	return ammo;
}

tesla_viewmodel_tube_fx( weapon, ammo )
{
	if ( weapon == "tesla_gun_upgraded_zm" )
	{
		if ( ammo == 3 || ammo == 4 )
			return level._effect[ "tesla_viewmodel_tube2_upgraded" ];
		if ( ammo == 1 || ammo == 2 )
			return level._effect[ "tesla_viewmodel_tube3_upgraded" ];
		return level._effect[ "tesla_viewmodel_tube_upgraded" ];
	}
	if ( ammo == 1 )
		return level._effect[ "tesla_viewmodel_tube3" ];
	if ( ammo == 2 )
		return level._effect[ "tesla_viewmodel_tube2" ];
	return level._effect[ "tesla_viewmodel_tube" ];
}

// One T5 tesla_fx_tube pass (:96-153).
tesla_viewmodel_tube_play( why )
{
	weapon = self tesla_viewmodel_weapon();
	if ( !isDefined( weapon ) )
		return;
	ammo = self tesla_viewmodel_ammo( weapon );
	if ( ammo <= 0 )
	{
		if ( why != "tick" )
			self tesla_viewmodel_trace( why + " dark", weapon );
		return;
	}
	self playViewModelFxOnTag( tesla_viewmodel_tube_fx( weapon, ammo ), "tag_brass" );
	self tesla_viewmodel_trace( why + " bulbs " + ammo, weapon );
}

tesla_viewmodel_fx_tube()
{
	self endon( "disconnect" );

	self.zm_tesla_reload_cancelled = false;
	reload_weapon = undefined;

	for ( ;; )
	{
		wait( 0.1 );

		// A reload of the gun that ends with the clip still empty was
		// cancelled before its rounds went in.
		weapon = self getCurrentWeapon();
		if ( isDefined( reload_weapon ) )
		{
			if ( !( isDefined( weapon ) && weapon == reload_weapon && self isReloading() ) )
			{
				if ( self getWeaponAmmoClip( reload_weapon ) <= 0 )
					self.zm_tesla_reload_cancelled = true;
				self tesla_viewmodel_trace( "reload over", reload_weapon );
				reload_weapon = undefined;
			}
		}
		else if ( isDefined( weapon ) && ( weapon == "tesla_gun_zm" || weapon == "tesla_gun_upgraded_zm" ) && self isReloading() )
		{
			reload_weapon = weapon;
			self.zm_tesla_reload_cancelled = false;
			self tesla_viewmodel_trace( "reload seen", weapon );
		}

		self tesla_viewmodel_tube_play( "tick" );
	}
}

tesla_viewmodel_fx_tube_on_fire()
{
	self endon( "disconnect" );

	for ( ;; )
	{
		self waittill( "weapon_fired" );
		self.zm_tesla_reload_cancelled = false;
		self tesla_viewmodel_tube_play( "fired" );
	}
}

tesla_viewmodel_fx_tube_on_raise()
{
	self endon( "disconnect" );

	for ( ;; )
	{
		self waittill( "weapon_change" );
		self tesla_viewmodel_tube_play( "raise" );
	}
}

tesla_viewmodel_fx_rail()
{
	self endon( "disconnect" );

	for ( ;; )
	{
		wait( randomFloatRange( 8, 12 ) );

		weapon = self tesla_viewmodel_weapon();
		if ( !isDefined( weapon ) || self playerADS() > 0 )
			continue;
		if ( self getWeaponAmmoClip( weapon ) <= 0 )
			continue;

		fx = level._effect[ "tesla_viewmodel_rail" ];
		if ( weapon == "tesla_gun_upgraded_zm" )
			fx = level._effect[ "tesla_viewmodel_rail_upgraded" ];

		self playViewModelFxOnTag( fx, "tag_flash" );
		self playLocalSound( "wpn_tesla_effects" );
		self tesla_viewmodel_trace( "rail", weapon );
	}
}

tesla_viewmodel_fx_grenade_watch()
{
	self endon( "disconnect" );

	for ( ;; )
	{
		self waittill( "grenade_pullback" );
		self.zm_tesla_throwing = true;
		self common_scripts\utility::waittill_any( "grenade_fire", "death", "weapon_change" );
		self.zm_tesla_throwing = false;
	}
}

tesla_viewmodel_trace( what, weapon )
{
	if ( getDvarInt( "scr_zm_tesla_vmtrace" ) <= 0 )
		return;
	held = self getCurrentWeapon();
	if ( !isDefined( held ) )
		held = "none";
	codolPrintLn( "CoDOL zm tesla vm: t=" + getTime() + " " + self getEntityNumber() + " " + what + " (" + weapon +
		" clip " + self getWeaponAmmoClip( weapon ) + " stock " + self getWeaponAmmoStock( weapon ) +
		" reloading " + self isReloading() + " held " + held + ")" );
}

// The trace's weapon events (scr_zm_tesla_vmtrace only).
tesla_viewmodel_trace_events()
{
	self thread tesla_viewmodel_trace_event( "weapon_fired" );
	self thread tesla_viewmodel_trace_event( "reload_start" );
	self thread tesla_viewmodel_trace_event( "reload" );
	self thread tesla_viewmodel_trace_event( "weapon_change" );
	self thread tesla_viewmodel_trace_event( "weapon_switch_started" );
	self thread tesla_viewmodel_trace_event( "grenade_pullback" );
	self thread tesla_viewmodel_trace_event( "grenade_fire" );
}

tesla_viewmodel_trace_event( name )
{
	self endon( "disconnect" );

	for ( ;; )
	{
		self waittill( name );
		if ( getDvarInt( "scr_zm_tesla_vmtrace" ) <= 0 )
			continue;
		weapon = self getCurrentWeapon();
		if ( !isDefined( weapon ) )
			weapon = "none";
		if ( weapon == "tesla_gun_zm" || weapon == "tesla_gun_upgraded_zm" )
			self tesla_viewmodel_trace( "event " + name, weapon );
		else
			codolPrintLn( "CoDOL zm tesla vm: t=" + getTime() + " " + self getEntityNumber() + " event " + name + " (held " + weapon + ")" );
	}
}

tesla_network_choke()
{
	self endon( "disconnect" );

	self.tesla_network_death_choke = 0;
	self.tesla_enemies_hit = 0;

	for ( ;; )
	{
		wait_network_frame();
		wait_network_frame();
		self.tesla_network_death_choke = 0;
	}
}
