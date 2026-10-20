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
//     2026-09-30): every body in character_zm_zombie's rotation carries
//     j_spineupper as bone 10; any other model falls back to playFx at the
//     chest point, which is safe for these two finite effects.
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

init()
{
	level.zm_tesla_ok = false;

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
	self tesla_play_death_fx( arc_num );

	// T5 :284: the arc's originator picks the death direction.
	origin = player.origin;
	if ( isDefined( source_enemy ) && source_enemy != self )
		origin = source_enemy.origin;

	if ( !isDefined( self ) || !isAlive( self ) )
		return;

	if ( isDefined( self.tesla_damage_func ) )
	{
		self [[ self.tesla_damage_func ]]( origin, player );
		return;
	}

	self doDamage( self.health + 666, origin, player );
}

// T5 :316.  On the body's J_SpineUpper, see the header.
tesla_play_death_fx( arc_num )
{
	fx = "tesla_shock";
	if ( arc_num > 1 )
		fx = "tesla_shock_secondary";

	if ( tesla_has_spine_tag( self ) )
		playFxOnTag( level._effect[ fx ], self, "j_spineupper" );
	else
		playFx( level._effect[ fx ], tesla_chest_point( self ) );
	self playSound( "wpn_imp_tesla" );
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
	return ent.origin + ( 0, 0, 48 );
}

// T5 :417 / :553.  The choke counter every tesla_do_damage waits on.
on_player_connect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread tesla_network_choke();
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
