// zm_theater_sh level script: Kino der Toten, CoDOL's own edit (zm_theater_sh.ff,
// DLC255 of build 3.5.2.6 -- the install labelled "3.5.6.2" -- ported to
// 1.1.1.13 with the AssetExplorer's port-map).
//
// GScr_LoadScripts asks for exactly one level module per match, maps/<mapname>
// for a zm match (CODOL_ScriptEntryPoints), as zm_sumpf_sh.gsc explains.
//
// BEHAVIOUR REFERENCE: T5 maps\zombie_theater.gsc and its modules
// (D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps: zombie_theater_magic_box,
// _movie_screen, _quad, _teleporter).  Unlike Shi No Numa, CoDOL's Kino IS
// T5's entity layout: the same coordinates (T5's collision fixes at
// (-391 1194 16) and the "alley doubletap" wall at (-1746 -378 147) land on
// this map's geometry), T5's targetnames and T5's zone flags, with the
// spawners turned into script_structs as on CoDOL's other zm maps.  MEASURED
// from the ported MapEnts (2727 entities after the ammo crates left):
//
//   zones         24 player_volume info_volumes, 10 zones -> the zone manager.
//                 The adjacency graph is T5 theater_zone_init's, below.
//   spawners      <volume target> zombie_spawner structs; <target>_rise risers
//                 (theater 7); <target>_dog hound structs -> _zombiemode_ai_dogs.
//   doors         12 zombie_door trigger_use; two (script_noteworthy
//                 electric_door, the lobby->theater pair) open with the power,
//                 as T5's.  The script_flag fan-out sets T5's zone flags.
//   box           9 treasure_chest_use (start_exclude on the foyer one, as
//                 T5's level.random_pandora_box_start reads it) and 81
//                 magic_box_loc_light cage lights, 9 per spot
//                 (script_noteworthy <spot>_loc), T5 zombie_theater.csc's box
//                 indicator -> theater_box_lights below.
//   power         use_elec_switch / elec_switch / elec_switch_fx -> the
//                 shared _zombiemode_power (T5 zombie_theater.gsc is the
//                 module's own reference).
//   pack-a-punch  zombie_vending_upgrade in the projector room, reached
//                 only through the teleporter (below).
//   traps         4 zombie_trap "electric" (e1..e4) -> _zombiemode_traps;
//                 1 zombie_trap "fire" (f1, the crematorium) -> the fire
//                 branch of the same module.
//   teleporter    trigger_teleport_pad_0 (the stage pad, T5's "core"),
//                 pf8_auto1 (the lobby mainframe, T5's "pad"),
//                 teleport_room_0..3, projroom_teleport_player0..3,
//                 theater_teleport_player0..3 -> theater_teleporter below.
//   curtains      theater_curtains / theater_curtains_clip -> open with the
//                 power (T5 setupCurtains).
//   turrets       2 auto_turret_trigger (pf33 on the theater's centre
//                 platform, pf34 in the dining room) -> theater_auto_turrets.
//   perks         NOT IN THE ZONE: the edit has no zombie_vending and no
//                 machine models, so theater_perk_machines (below) places
//                 T5's four (zombie_theater.ents: Quick Revive in the lobby,
//                 Juggernog by the foyer stairs, Speed Cola in the dining
//                 room, Double Tap in the alley) at T5's origins and angles,
//                 before _zombiemode_perks sweeps "zombie_vending".  Mule Kick
//                 is left out (the user, 2026-10-25; T5 Kino places it from
//                 script too, place_additionalprimaryweapon_machine).
//
// NOT PORTED, and why:
//   * The ammo crates (7 ammo_cache prefabs: ammobox_collision brush,
//     ammobox_trigger, ammobox_screen, zm_ammo_crate_light struct, and the 14
//     mil_ammo_case_1_open static models beside them) are REMOVED FROM THE
//     ZONE by the port, as on zm_sumpf_sh: CodolSoundDumper port-map
//     --drop-model mil_ammo_case_1_open --drop-entity targetname=ammo_cache
//     --drop-entity script_noteworthy=zm_ammo_crate_light (the brush models
//     stay in the BSP with no entity, so nothing draws or collides with them).
//     The same port drops the two misc_turret entities
//     (--drop-entity script_noteworthy=sample_turret, see the turrets below).
//   * Perk machines and wall buys: CoDOL's edit carries none (no
//     zombie_vending, no weapon_upgrade, no machine models in the zone);
//     the perk machines are placed by script instead (perks, above).
//   * Quad zombies (quad_zombie_spawner structs on the roofs, T5
//     zombie_theater_quad roof breaks): C1 has no quad species or clips, and
//     zombies keep C1's cyborg animations; the roof spawners stay unused, as
//     the spawn module only takes script_noteworthy "zombie_spawner".
//   * The film reels / ee rooms / movie screen images (T5
//     zombie_theater_movie_screen reels, ee_teleport_player): the edit has
//     no ee_teleport_player spots and no reel pickups.
//   * Exploders and the CSC presentation (setTransported, extracam, the box
//     indicator client system, "ZPO" lights): no CSC VM and no exploder
//     channel to C1; the server plays the effects it can (playFx) instead.
//   * The CoDOL rescue helicopter (round 9 on Easy): the edit carries no
//     rescue_dest route, so _zombiemode_extraction extracts everyone standing
//     without a helicopter (T5 Kino has no rescue at all).
//   * T5's invisible collision fixes (collision_geo_64x64x128 /
//     collision_wall_128x128x10): neither model ships in any 1.1.1.13 zone.
//
// DELIBERATELY NOT HERE (as zm_sumpf_sh.gsc): maps\mp\_load, the
// maps/mp/zm_theater_sh_fx.gsc createfx module (C1 draws the zone's createfx
// itself), maps\_compass.

#include maps\_zombiemode_utility;

main()
{
	// Before any wait: this is the level script's precache window.
	theater_precache();

	// Before levelMain: _zombiemode_perks::init sweeps "zombie_vending".
	theater_perk_machines();

	// T5 zombie_theater.gsc main: the box starts at a random spot.
	level.random_pandora_box_start = true;

	maps\_zombiemode::levelMain( "zm_theater_sh" );

	// T5 zombie_theater.gsc init_sounds.
	add_sound( "wooden_door", "zmb_door_wood_open" );
	add_sound( "fence_door", "zmb_door_fence_open" );

	theater_zone_init();

	// T5 places the players on initial_spawn_points (the lobby, beside the
	// mainframe), not on the single mp_* spawn entity.
	level.zm_spawn_point_func = ::theater_spawn_point;

	flag_init( "curtains_done" );
	flag_init( "teleporter_linked" );
	flag_init( "core_linked" );

	level thread theater_curtains();
	level thread theater_box_lights();
	level thread theater_teleporter();
	level thread theater_auto_turrets();
	level thread theater_fog();
	level thread theater_test_watch();
}

theater_precache()
{
	precacheModel( "zombie_zapper_cagelight" );
	precacheModel( "zombie_zapper_cagelight_red" );
	precacheModel( "zombie_zapper_cagelight_green" );
	precacheShellShock( "electrocution" );

	// T5 zombie_theater_fx.gsc, the names this script plays; every one ships
	// in the map zone.
	level._effect[ "boxlight_light_ready" ] = loadfx( "maps/zombie/fx_zombie_theater_lightboard_green" );
	level._effect[ "boxlight_light_notready" ] = loadfx( "maps/zombie/fx_zombie_theater_lightboard_red" );
	level._effect[ "zapper_light_ready" ] = loadfx( "misc/fx_zombie_zapper_light_green" );
	level._effect[ "zapper_light_notready" ] = loadfx( "misc/fx_zombie_zapper_light_red" );
	level._effect[ "fx_transporter_beam" ] = loadfx( "maps/zombie/fx_transporter_beam" );
	level._effect[ "fx_transporter_pad_start" ] = loadfx( "maps/zombie/fx_transporter_pad_start" );
	level._effect[ "fx_transporter_start" ] = loadfx( "maps/zombie/fx_transporter_start" );
	level._effect[ "fx_transporter_ambient" ] = loadfx( "maps/zombie/fx_transporter_ambient" );
	level._effect[ "fx_teleporter_pad_glow" ] = loadfx( "maps/zombie/fx_zombie_teleporter_pad_glow" );
}

// --------------------------------------------------------------------------
// Perk machines.  T5 zombie_theater.ents carries four zombie_vending
// trigger_use, each targeting its machine script_model (and a clip brush for
// Quick Revive); CoDOL's edit dropped all of them, but kept T5's coordinates,
// so the machines go back at T5's places.  A level script cannot spawn a
// trigger_use, so each machine's trigger is a usable script_origin at the
// trigger's origin carrying the fields _zombiemode_perks and
// _zombiemode_audio read (script_noteworthy = the perk, target = the model,
// script_sound / script_label = T5's jingle and sting).  The model is C1's
// powered machine, the one zm_factory_sh and zm_sumpf_sh place (C1 renames
// T5's zombie_vending_sleight to zombie_vending_speedcola_on); the unpowered
// models ship only in other map zones, so the machines stand in their
// powered skin and _zombiemode_perks lights them when the power comes on.
// MEASURED, docs/zone_assets/ALL_xmodel.txt: every model is defined in
// common_gametype_zm (and patch_common_gametype_zm), which zm_theater_sh
// loads.  No collision brush comes with them.
//                 perk                     trigger origin       model origin            yaw
//   Quick Revive  specialty_quickrevive    (527 -1261 106)      (531 -1261.5 79)        270
//   Speed Cola    specialty_fastreload     (1269 143 29)        (1268.5 141 -18)        180
//   Double Tap    specialty_rof            (-1730 -375 39)      (-1732 -378.5 0)         90
//   Juggernog     specialty_armorvest      (-328 -490 53)       (-328.5 -492 5.5)       180
// --------------------------------------------------------------------------

theater_perk_machines()
{
	theater_perk_machine( "specialty_quickrevive", "vending_revive", "zombie_vending_revive_on",
		( 527, -1261, 106 ), ( 531, -1261.5, 79 ), 270, "mus_perks_revive_jingle", "mus_perks_revive_sting" );
	theater_perk_machine( "specialty_fastreload", "vending_sleight", "zombie_vending_speedcola_on",
		( 1269, 143, 29 ), ( 1268.5, 141, -18 ), 180, "mus_perks_speed_jingle", "mus_perks_speed_sting" );
	theater_perk_machine( "specialty_rof", "vending_doubletap", "zombie_vending_doubletap_on",
		( -1730, -375, 39 ), ( -1732, -378.5, 0 ), 90, "mus_perks_doubletap_jingle", "mus_perks_doubletap_sting" );
	theater_perk_machine( "specialty_armorvest", "vending_jugg", "zombie_vending_jugg_on",
		( -328, -490, 53 ), ( -328.5, -492, 5.5 ), 180, "mus_perks_jugganog_jingle", "mus_perks_jugganog_sting" );
}

theater_perk_machine( perk, name, model, use_origin, model_origin, yaw, jingle, sting )
{
	precacheModel( model );

	machine = spawn( "script_model", model_origin );
	machine.angles = ( 0, yaw, 0 );
	machine setModel( model );
	machine.targetname = name;

	use = spawn( "script_origin", use_origin );
	use.targetname = "zombie_vending";
	use.target = name;
	use.script_noteworthy = perk;
	use.script_sound = jingle;
	use.script_label = sting;
	use setCursorHint( "HINT_NOICON" );
	use makeUsable();

	codolprintln( "CoDOL zm theater: " + perk + " machine " + model + " at " + model_origin );
}

// --------------------------------------------------------------------------
// Zones.  T5 zombie_theater.gsc main: manage_zones( "foyer_zone",
// "foyer2_zone" ) and theater_zone_init, verbatim.  The volume names and the
// door flags are the edit's own (MEASURED: they are T5's).
// --------------------------------------------------------------------------

theater_zone_init()
{
	flag_init( "always_on" );
	flag_set( "always_on" );

	level.zm_zone_initial = [];
	level.zm_zone_initial[ 0 ] = "foyer_zone";
	level.zm_zone_initial[ 1 ] = "foyer2_zone";

	maps\_zombiemode_zone_manager::add_adjacent_zone( "foyer_zone", "foyer2_zone", "always_on" );

	maps\_zombiemode_zone_manager::add_adjacent_zone( "foyer_zone", "vip_zone", "magic_box_foyer1" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "foyer2_zone", "crematorium_zone", "magic_box_crematorium1" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "foyer_zone", "crematorium_zone", "magic_box_crematorium1" );

	maps\_zombiemode_zone_manager::add_adjacent_zone( "vip_zone", "dining_zone", "vip_to_dining" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "crematorium_zone", "alleyway_zone", "magic_box_alleyway1" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "dining_zone", "dressing_zone", "dining_to_dressing" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "dressing_zone", "stage_zone", "magic_box_dressing1" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "stage_zone", "west_balcony_zone", "magic_box_west_balcony2" );

	maps\_zombiemode_zone_manager::add_adjacent_zone( "theater_zone", "foyer2_zone", "power_on" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "theater_zone", "stage_zone", "power_on" );

	maps\_zombiemode_zone_manager::add_adjacent_zone( "west_balcony_zone", "alleyway_zone", "magic_box_west_balcony1" );
}

// --------------------------------------------------------------------------
// Spawning: the first free initial_spawn_points struct in script_int order
// (1..4).  self = the spawning player.
// --------------------------------------------------------------------------

theater_spawn_point()
{
	points = get_struct_array( "initial_spawn_points", "targetname" );
	if ( !isDefined( points ) || points.size < 1 )
		return undefined;

	for ( i = 1; i < points.size; i++ )
	{
		for ( j = i; j > 0 && theater_point_order( points[ j - 1 ] ) > theater_point_order( points[ j ] ); j-- )
		{
			swap = points[ j ];
			points[ j ] = points[ j - 1 ];
			points[ j - 1 ] = swap;
		}
	}

	for ( i = 0; i < points.size; i++ )
	{
		if ( !positionWouldTelefrag( points[ i ].origin ) )
			return theater_point( points[ i ] );
	}
	return theater_point( points[ randomInt( points.size ) ] );
}

theater_point_order( s )
{
	if ( isDefined( s.script_int ) )
		return int( s.script_int );
	return 99;
}

theater_point( s )
{
	point = spawnStruct();
	point.origin = s.origin;
	point.angles = ( 0, 0, 0 );
	if ( isDefined( s.angles ) )
		point.angles = s.angles;
	return point;
}

// --------------------------------------------------------------------------
// Fog.  T5 clientscripts\zombie_theater_fx.csc level_fog_init /
// dog_start_monitor / dog_stop_monitor set height fog (setVolFog); C1 has
// exponential fog only, so each setting is T5's density at the floor of the
// lobby and the theater (z 100): halfway = half_dist * 2 ^ ( ( 100 -
// base_height ) / half_height ).
//   normal  start 155.625  half_dist 2332.79  half_height 677.241  base 23.2733  -> 2525, grey 0.64
//   hounds  start 0        half_dist 302.622  half_height 306.395  base 344.622  -> 174, opacity 0.65
// T5's hound fog is blue-grey (0.46 0.56 0.62); as on zm_sumpf_sh it is
// taken as a neutral grey, which C1 does not tint blue.
// --------------------------------------------------------------------------

theater_fog()
{
	level thread theater_fog_dvar();

	setExpFog( 155.625, 2525, 0.64, 0.64, 0.64, 1, 0 );

	// _zombiemode_ai_dogs owns the flag; it may be created after main().
	while ( !flag_exist( "dog_round" ) )
		wait( 0.5 );
	for ( ;; )
	{
		flag_wait( "dog_round" );
		setExpFog( 0, 174, 0.55, 0.57, 0.58, 0.65, 7 );
		flag_waitopen( "dog_round" );
		setExpFog( 155.625, 2525, 0.64, 0.64, 0.64, 1, 7 );
	}
}

// zm maps do not run maps\mp\_load, whose codolFogDvar sends C1's r_fog
// (dvar 1464); a previous map may have turned it off (zm_sumpf_sh.gsc).
theater_fog_dvar()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player setClientDvar( "1464", 1 );
	}
}

// --------------------------------------------------------------------------
// Curtains.  T5 zombie_theater_movie_screen.gsc setupCurtains: with the
// power the stage curtains open, their clip stops blocking and the paths
// through it connect; "curtains_done" then lets the theater's spawners in
// (T5 theater_ignore_spawner).  T5 plays %o_zombie_theatre_curtain on the
// curtain model; C1's client builds an animtree only from the animlists it
// loads, and no list names that clip, so the curtain is raised by script
// instead (the move T5's own moveCurtains uses, upward out of the arch).
// --------------------------------------------------------------------------

theater_curtains()
{
	curtains = getEnt( "theater_curtains", "targetname" );
	clip = getEnt( "theater_curtains_clip", "targetname" );

	flag_wait( "power_on" );

	if ( isDefined( clip ) )
	{
		clip notSolid();
		clip connectPaths();
	}
	if ( isDefined( curtains ) )
	{
		// T5 plays "curtain_open", an alias no CoDOL zone ships; the move is silent.
		curtains moveZ( 400, 3, 0.75, 0.75 );
		wait( 3 );
		curtains hide();
	}

	flag_set( "curtains_done" );
	codolprintln( "CoDOL zm theater: power on; stage curtains open" );
}

// --------------------------------------------------------------------------
// Box indicator.  T5 zombie_theater_magic_box.gsc magic_box_update and the
// client's box_indicator system (clientscripts\zombie_theater.csc): once the
// power is on, the nine cage lights of the box's spot glow green; while the
// box moves every light flashes; during a fire sale all of them are green.
// With no CSC VM the server swaps the models and plays the light effects.
// Every light is a magic_box_loc_light script_model whose script_noteworthy
// is "<treasure_chest_use script_noteworthy>_loc".
// --------------------------------------------------------------------------

theater_box_lights()
{
	all = getEntArray( "magic_box_loc_light", "targetname" );
	if ( all.size < 1 )
		return;

	level.theater_box_lights = [];
	for ( i = 0; i < all.size; i++ )
	{
		if ( !isDefined( all[ i ].script_noteworthy ) )
			continue;
		name = all[ i ].script_noteworthy;
		if ( !isDefined( level.theater_box_lights[ name ] ) )
			level.theater_box_lights[ name ] = [];
		list = level.theater_box_lights[ name ];
		list[ list.size ] = all[ i ];
		level.theater_box_lights[ name ] = list;
	}

	flag_wait( "power_on" );
	wait( 2 );

	shown = "";
	for ( ;; )
	{
		if ( flag_exist( "moving_chest_now" ) && flag( "moving_chest_now" ) )
		{
			theater_box_lights_flash();
			shown = "";
			continue;
		}

		want = theater_box_location();
		if ( is_true( level.zombie_vars[ "zombie_powerup_fire_sale_on" ] ) )
			want = "*";

		if ( want != shown )
		{
			theater_box_lights_set( want );
			shown = want;
		}
		wait( 0.5 );
	}
}

// The live spot's "<noteworthy>_loc" name.
theater_box_location()
{
	if ( !isDefined( level.chests ) || !isDefined( level.chest_index ) || level.chest_index < 0 ||
		level.chest_index >= level.chests.size )
		return "";
	chest = level.chests[ level.chest_index ];
	if ( !isDefined( chest ) || !isDefined( chest.script_noteworthy ) )
		return "";
	return chest.script_noteworthy + "_loc";
}

// "*" lights every spot.
theater_box_lights_set( want )
{
	keys = getArrayKeys( level.theater_box_lights );
	for ( k = 0; k < keys.size; k++ )
	{
		lights = level.theater_box_lights[ keys[ k ] ];
		on = ( want == "*" || keys[ k ] == want );
		for ( i = 0; i < lights.size; i++ )
			lights[ i ] theater_light( on, "zombie_zapper_cagelight_green", "boxlight_light_ready" );
	}
}

theater_box_lights_flash()
{
	keys = getArrayKeys( level.theater_box_lights );
	on = true;
	while ( flag( "moving_chest_now" ) )
	{
		for ( k = 0; k < keys.size; k++ )
		{
			lights = level.theater_box_lights[ keys[ k ] ];
			for ( i = 0; i < lights.size; i++ )
				lights[ i ] theater_light( on, "zombie_zapper_cagelight_green", undefined );
		}
		on = !on;
		wait( 0.5 );
	}
}

// self = a cage light.  T5 turnLightGreen / turnLightRed: the coloured
// model, and its effect on a tag_origin 10 units below.
theater_light( on, model, fx_name )
{
	if ( isDefined( self.theater_fx ) )
	{
		self.theater_fx delete();
		self.theater_fx = undefined;
	}

	if ( !on )
	{
		self setModel( "zombie_zapper_cagelight" );
		return;
	}

	self setModel( model );
	if ( isDefined( fx_name ) && isDefined( level._effect[ fx_name ] ) )
	{
		self.theater_fx = spawn( "script_model", self.origin - ( 0, 0, 10 ) );
		self.theater_fx setModel( "tag_origin" );
		self.theater_fx.angles = self.angles;
		playFxOnTag( level._effect[ fx_name ], self.theater_fx, "tag_origin" );
	}
}

// --------------------------------------------------------------------------
// Teleporter.  T5 zombie_theater_teleporter.gsc: with the power on, the
// stage pad ("core", trigger_teleport_pad_0) is started, then linked at the
// lobby mainframe (its target, pf8_auto1); the core then sends everyone
// standing on it to the projector room (projroom_teleport_player0..3, where
// Pack-a-Punch is) by way of the black room (teleport_room_0..3), and after
// 30 s back out to the mainframe (theater_teleport_player0..3).  90 s
// cooldown, free (level.teleport_cost 0).  T5's "eerooms" leg needs the
// ee_teleport_player spots, which the edit does not carry, so the return
// always goes to the mainframe (T5's 25% branch).
// --------------------------------------------------------------------------

theater_teleporter()
{
	core = getEnt( "trigger_teleport_pad_0", "targetname" );
	if ( !isDefined( core ) || !isDefined( core.target ) )
	{
		codolprintln( "CoDOL zm theater: no teleporter core on this map" );
		return;
	}
	pad = getEnt( core.target, "targetname" );
	if ( !isDefined( pad ) )
	{
		codolprintln( "CoDOL zm theater: the teleporter core links to nothing" );
		return;
	}

	level.teleport_delay = 1.8;
	level.teleport_cost = 0;
	level.theater_tp_core = core;
	level.theater_tp_pad = pad;
	level.link_cable_off = getEnt( "teleporter_link_cable_off", "targetname" );
	level.link_cable_on = getEnt( "teleporter_link_cable_on", "targetname" );
	if ( isDefined( level.link_cable_on ) )
		level.link_cable_on hide();

	// T5 teleport_pad_hide_use.
	core setCursorHint( "HINT_NOICON" );
	pad setCursorHint( "HINT_NOICON" );
	core setHintString( "" );
	pad setHintString( &"ZOMBIE_NEED_POWER" );

	theater_indicator_lights( false );

	flag_wait( "power_on" );
	pad setHintString( &"ZOMBIE_THEATER_START_CORE" );

	// T5 teleporter_init: the clock and the two points of interest are not
	// taken (the clock is client presentation, the pois lure zombies to the
	// mainframe while a team is away, which this server's poi module would
	// do for good: they are never turned off in T5 either).
	codolprintln( "CoDOL zm theater: teleporter ready; core " + core.origin + ", mainframe " + pad.origin );

	level thread theater_tp_link_think( core, pad );
	level thread theater_tp_pad_think( core, pad );
	level thread theater_tp_core_think( core, pad );
}

// T5 teleport_link_think: the first press, on the core (the stage pad),
// starts it; the mainframe pad then offers the link (theater_tp_pad_think).
theater_tp_link_think( core, pad )
{
	for ( ;; )
	{
		if ( !flag( "core_linked" ) )
		{
			core setHintString( &"ZOMBIE_THEATER_LINK_CORE" );
			core theater_wait_use();
			core playSound( "evt_teleporter_activate_start" );
			flag_set( "core_linked" );
			core setHintString( "" );
			pad setHintString( &"ZOMBIE_THEATER_LINK_PAD" );
			codolprintln( "CoDOL zm theater: teleporter core started" );
		}
		wait( 0.1 );
	}
}

theater_tp_pad_think( core, pad )
{
	for ( ;; )
	{
		if ( !flag( "teleporter_linked" ) && flag( "core_linked" ) )
		{
			pad theater_wait_use();
			pad setHintString( "" );
			pad playSound( "evt_teleporter_activate_finish" );
			flag_set( "teleporter_linked" );
			if ( isDefined( level.link_cable_on ) && isDefined( level.link_cable_off ) )
			{
				level.link_cable_off hide();
				level.link_cable_on show();
			}
			codolprintln( "CoDOL zm theater: teleporter linked at the mainframe" );
		}
		wait( 0.1 );
	}
}

// T5 teleport_core_think( 0 ).
theater_tp_core_think( core, pad )
{
	for ( ;; )
	{
		flag_wait( "teleporter_linked" );

		theater_indicator_lights( true );
		core setHintString( &"ZOMBIE_THEATER_USE_TELEPORTER" );

		user = core theater_wait_use();
		if ( user.score < level.teleport_cost )
			continue;

		theater_indicator_lights( false );
		core setHintString( "" );
		if ( level.teleport_cost > 0 )
			user maps\_zombiemode_score::minus_to_player_score( level.teleport_cost );

		codolprintln( "CoDOL zm theater: teleporter used by " + user.name );
		core theater_player_teleporting();

		if ( isDefined( level.link_cable_on ) && isDefined( level.link_cable_off ) )
		{
			level.link_cable_on hide();
			level.link_cable_off show();
		}

		core setHintString( &"ZOMBIE_TELEPORT_COOLDOWN" );
		wait( 90 );

		flag_clear( "teleporter_linked" );
		flag_clear( "core_linked" );
		pad setHintString( &"ZOMBIE_THEATER_START_CORE" );
		core setHintString( "" );
	}
}

// self = a trigger_use.  Returns the valid player who used it.
theater_wait_use()
{
	for ( ;; )
	{
		self waittill( "trigger", user );
		if ( isDefined( user ) && is_player_valid( user ) )
			return user;
	}
}

// T5 turnLightGreen / turnLightRed( "teleporter_indicator_light1" ).
theater_indicator_lights( green )
{
	lights = getEntArray( "teleporter_indicator_lights", "targetname" );
	for ( i = 0; i < lights.size; i++ )
	{
		if ( green )
			lights[ i ] theater_light( true, "zombie_zapper_cagelight_green", "zapper_light_ready" );
		else
			lights[ i ] theater_light( true, "zombie_zapper_cagelight_red", "zapper_light_notready" );
	}
}

// self = the core.  T5 player_teleporting.
theater_player_teleporting()
{
	if ( isDefined( level._effect[ "fx_transporter_pad_start" ] ) )
		playFx( level._effect[ "fx_transporter_pad_start" ], self.origin );
	self playSound( "evt_beam_fx" );

	// T5 teleport_nuke( undefined, 300 ).
	self thread theater_teleport_nuke( 300 );

	wait( level.teleport_delay );

	if ( isDefined( level._effect[ "fx_transporter_beam" ] ) )
		playFx( level._effect[ "fx_transporter_beam" ], self.origin );

	travellers = [];
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isDefined( players[ i ] ) && is_player_valid( players[ i ] ) && players[ i ] isTouching( self ) )
			travellers[ travellers.size ] = players[ i ];
	}
	if ( travellers.size < 1 )
	{
		codolprintln( "CoDOL zm theater: nobody stayed on the pad" );
		return;
	}

	theater_teleport_players( travellers, "projroom_teleport_player" );
	codolprintln( "CoDOL zm theater: " + travellers.size + " player(s) in the projector room" );

	wait( 30 );

	// Back out to the mainframe; T5 nukes around the destination (20).
	theater_teleport_players( travellers, "theater_teleport_player" );
	if ( isDefined( level.theater_tp_pad ) )
		level.theater_tp_pad thread theater_teleport_nuke( 120 );
	codolprintln( "CoDOL zm theater: projector room visit over" );
}

// T5 teleport_players: every traveller waits 2 s frozen in the black room
// (teleport_room_<slot>), then lands on a free <dest_prefix><slot> spot.
theater_teleport_players( travellers, dest_prefix )
{
	black = theater_spots( "teleport_room_" );
	dest = theater_spots( dest_prefix );

	for ( i = 0; i < travellers.size; i++ )
	{
		p = travellers[ i ];
		if ( !isDefined( p ) )
			continue;
		p.inteleportation = true;
		p disableWeapons();
		p freezeControls( true );
		if ( black.size > 0 )
		{
			spot = black[ i % black.size ];
			p setOrigin( spot.origin );
			if ( isDefined( spot.angles ) )
				p setPlayerAngles( spot.angles );
		}
	}

	wait( 2 );

	for ( i = 0; i < travellers.size; i++ )
	{
		p = travellers[ i ];
		if ( !isDefined( p ) )
			continue;
		if ( dest.size > 0 )
		{
			spot = dest[ i % dest.size ];
			p setOrigin( spot.origin );
			if ( isDefined( spot.angles ) )
				p setPlayerAngles( spot.angles );
		}
		p freezeControls( false );
		p enableWeapons();
		p.inteleportation = false;
		// T5 teleport_aftereffects (one of its seven; the shellshock one).
		p shellShock( "electrocution", 1.5 );
	}
}

theater_spots( prefix )
{
	spots = [];
	for ( i = 0; i < 4; i++ )
	{
		e = getEnt( prefix + i, "targetname" );
		if ( isDefined( e ) )
			spots[ spots.size ] = e;
	}
	return spots;
}

// T5 teleport_nuke( undefined, range ): the zombies near the pad die.
theater_teleport_nuke( range )
{
	if ( !isDefined( level.zombies ) )
		return;
	victims = [];
	for ( i = 0; i < level.zombies.size; i++ )
	{
		z = level.zombies[ i ];
		if ( isDefined( z ) && isAlive( z ) && distance( z.origin, self.origin ) <= range )
			victims[ victims.size ] = z;
	}
	for ( i = 0; i < victims.size; i++ )
	{
		wait( randomFloatRange( 0.2, 0.3 ) );
		z = victims[ i ];
		if ( !isDefined( z ) || !isAlive( z ) )
			continue;
		if ( !is_true( z.isdog ) )
			z maps\_zombiemode_gib::zombie_head_gib();
		z doDamage( z.health + 100, z.origin );
	}
}

// --------------------------------------------------------------------------
// Auto turrets.  T5 _zombiemode_auto_turret.gsc init / auto_turret_think: a
// map turret is bought at its auto_turret_trigger (1500, once the power is
// on) and fires on its own for 30 s, at most two at a time.  The edit's two
// misc_turret entities name weaponinfo zombie_bullet_crouch_mp, which no
// 1.1.1.13 zone defines (the server refuses a turret with an unknown weapon),
// so the port leaves them out (--drop-entity script_noteworthy=sample_turret)
// and each purchase plants the zm sentry (_zombiemode_auto_turret: C1's own
// sentry_minigun_mp kit) at the removed turret's place and facing, MEASURED
// from the 3.5.2.6 MapEnts:
//   pf33_auto1  origin (0 952 74)   angles (0 270 0)
//   pf34_auto1  origin (1520 424 16) angles (0 0 0)
// --------------------------------------------------------------------------

theater_auto_turrets()
{
	spots = [];
	spots[ "pf33_auto_turret_trigger" ] = theater_turret_spot( ( 0, 952, 74 ), 270 );
	spots[ "pf34_auto_turret_trigger" ] = theater_turret_spot( ( 1520, 424, 16 ), 0 );

	if ( !isDefined( level.auto_turret_cost ) )
		level.auto_turret_cost = 1500;

	names = getArrayKeys( spots );
	for ( i = 0; i < names.size; i++ )
	{
		trig = getEnt( names[ i ], "targetname" );
		if ( !isDefined( trig ) )
			continue;
		trig setCursorHint( "HINT_NOICON" );
		trig setHintString( &"ZOMBIE_NEED_POWER" );
		trig thread theater_auto_turret_think( spots[ names[ i ] ] );
	}
}

theater_turret_spot( origin, yaw )
{
	s = spawnStruct();
	s.origin = origin;
	s.yaw = yaw;
	return s;
}

// self = an auto_turret_trigger.  T5 auto_turret_think.
theater_auto_turret_think( spot )
{
	flag_wait( "power_on" );

	if ( !is_true( level.zm_sentry_ok ) )
	{
		codolprintln( "CoDOL zm theater: the zm sentry kit is not mounted; turret at " + spot.origin + " stays off" );
		self setHintString( "" );
		return;
	}

	for ( ;; )
	{
		self setHintString( &"ZOMBIE_AUTO_TURRET", level.auto_turret_cost );
		self waittill( "trigger", player );

		if ( !isDefined( player ) || !is_player_valid( player ) || player in_revive_trigger() )
			continue;
		if ( player.score < level.auto_turret_cost )
		{
			player maps\_zombiemode_audio::no_cash_vox();
			continue;
		}

		player maps\_zombiemode_score::minus_to_player_score( level.auto_turret_cost );
		self setHintString( "" );

		turret = spawnTurret( "misc_turret", spot.origin, level.zm_sentry_weapon );
		if ( !isDefined( turret ) )
		{
			codolprintln( "CoDOL zm theater: spawnturret returned nothing at " + spot.origin );
			continue;
		}
		turret.angles = ( 0, spot.yaw, 0 );
		turret setModel( level.zm_sentry_model );
		turret makeUnusable();
		turret makeTurretInoperable();
		turret.owner = player;
		turret.zm_kills = 0;
		turret setConvergenceTime( 0.3 );
		turret setTurretTeam( "allies" );

		codolprintln( "CoDOL zm theater: turret at " + spot.origin + " bought by " + player.name );
		turret thread maps\_zombiemode_auto_turret::auto_turret_activate( player );
		turret waittill( "turret_deactivated" );
	}
}

// --------------------------------------------------------------------------
// Developer-only test verbs (developer 1; the live server runs developer 0),
// through the dvar scr_theater_test, as _zombiemode_powertest's:
//   press <client> <trigger targetname>   stand the test client in that
//                                          trigger and hold C1 USE (0x28)
//   where <client>                         print the client's origin
// The teleporter is driven with them: press 0 trigger_teleport_pad_0 (core),
// press 0 pf8_auto1 (mainframe), press 0 trigger_teleport_pad_0 (teleport).
// --------------------------------------------------------------------------

theater_test_watch()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;
	setDvar( "scr_theater_test", "" );
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_theater_test" );
		if ( request == "" )
			continue;
		setDvar( "scr_theater_test", "" );
		tokens = strTok( request, " " );
		if ( tokens.size < 2 )
			continue;
		players = getPlayers();
		index = int( tokens[ 1 ] );
		if ( index < 0 || index >= players.size )
			continue;
		p = players[ index ];
		if ( tokens[ 0 ] == "press" && tokens.size >= 3 )
		{
			trig = getEnt( tokens[ 2 ], "targetname" );
			if ( !isDefined( trig ) )
			{
				codolprintln( "CoDOL theater test: no entity " + tokens[ 2 ] );
				continue;
			}
			p setOrigin( trig.origin - ( 0, 0, 40 ) );
			wait( 0.2 );
			codolprintln( "CoDOL theater test: client " + index + " at " + p.origin + " pressing USE in " + tokens[ 2 ] );
			p codol_botPressButtons( 40, 1 );
		}
		else if ( tokens[ 0 ] == "notify" && tokens.size >= 3 )
		{
			// A look-at trigger (Pack-a-Punch) a test client cannot aim at.
			trig = getEnt( tokens[ 2 ], "targetname" );
			if ( !isDefined( trig ) )
				continue;
			p setOrigin( trig.origin - ( 0, 0, 40 ) );
			wait( 0.2 );
			codolprintln( "CoDOL theater test: client " + index + " holding " + p getCurrentWeapon() + " triggers " + tokens[ 2 ] );
			trig notify( "trigger", p );
		}
		else if ( tokens[ 0 ] == "weapons" )
		{
			list = p getWeaponsListPrimaries();
			text = "";
			for ( i = 0; isDefined( list ) && i < list.size; i++ )
				text = text + list[ i ] + " ";
			codolprintln( "CoDOL theater test: client " + index + " holds " + p getCurrentWeapon() + "; primaries " + text );
		}
		else if ( tokens[ 0 ] == "where" )
			codolprintln( "CoDOL theater test: client " + index + " at " + p.origin );
	}
}
