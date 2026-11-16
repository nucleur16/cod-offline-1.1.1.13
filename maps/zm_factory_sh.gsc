// zm_factory_sh level script: Der Riese, CoDOL's own edit (zm_factory_sh.ff,
// CoDOL 3.5.6.2 = build 3.5.2.6, ported to 1.1.1.13 with the AssetExplorer's
// port-map, 2026-10-22).
//
// GScr_LoadScripts asks for exactly one level module per match, maps/<mapname>
// for a zm match (CODOL_ScriptEntryPoints); see zm_sumpf_sh.gsc.  The stock
// client arena data already gives zm_factory_sh gametype "zm".
//
// BEHAVIOUR REFERENCE: T5 maps\zombie_cod5_factory.gsc and
// zombie_cod5_factory_teleporter.gsc (D:\CoD\COD-GSC-Source\T5-STEAM-GSC).
// Unlike Shi No Numa, CoDOL's factory edit keeps T5's entity layout almost
// name for name; it renames only what CoDOL's data-driven zm modules read.
// MEASURED from the 3.5.2.6 MapEnts (2147 entities):
//
//   perks         4 zombie_vending (jugg, revive, speed cola, double tap) and
//                 zombie_vending_upgrade (Pack-a-Punch behind pack_door) ->
//                 _zombiemode_perks, powered by the one master switch.
//   power         use_elec_switch / elec_switch / elec_switch_fx under the
//                 bridge -> _zombiemode_power (T5 power_electric_switch).
//   box           6 treasure_chest_use (start_chest, chest1..5) with their
//                 <noteworthy>_rubble piles -> _zombiemode_weapons.
//   doors/debris  10 zombie_door + 2 zombie_debris -> _zombiemode_blockers.
//                 The two receiver doors T5 opens with the power (T5
//                 script_noteworthy "electric_door") are authored
//                 "factory_electric_door" with a 10000 cost; they are made
//                 T5 electric doors below.
//   zones         12 player_volume zones -> the zone manager; the graph is T5
//                 factory_zone_init plus the connect_zones calls T5 makes
//                 from the power switch, the bridge and the bridge jump.
//   traps         3 zombie_trap "electric" (wh, wn, br) -> _zombiemode_traps;
//                 T5 electric_trap_think gates each on the power AND its
//                 area (enter_warehouse_building, enter_wnuen_building,
//                 bridge_down), see factory_trap_gates.
//   bridge        wnuen_bridge / warehouse_bridge with their coils and clips
//                 (T5 bridge_init) -- factory_bridge below.
//   teleporters   trigger_teleport_pad_0..2, trigger_teleport_core,
//                 teleport_room_0..3, origin_teleport_player_0..3,
//                 teleporter_powerup, pack_door / pack_door_clip ->
//                 maps\zm_factory_sh_teleporter (T5's teleporter module).
//   hounds        <zone>_spawners_dog structs -> _zombiemode_ai_dogs.
//   rescue        rescue_dest chains -> _zombiemode_extraction.
//   intel         12 intel_collectible -> _zombiemode_intel.
//
// REMOVED FROM THE ZONE (port-map --drop-entity / --drop-model, as for
// zm_sumpf_sh): the seven CoDOL ammo crates (ammo_cache script_model with its
// ammobox_collision / ammobox_trigger / ammobox_screen prefab and the
// zm_ammo_crate_light structs), the two teleporter_portal origins and their
// jump_target structs (the exfil jump; their animation
// zombie_exfil_heli_getin_03_short is in no 1.1.1.13 zone).
//
// NOT PORTED, and why:
//   * T5 radio/phonograph/meteor/teddy easter eggs, the fly trap and the
//     samantha/giant VO: their triggers (radio_one.., phono_one..,
//     meteor_one.., monk_scream_trig) are not in the edit.
//   * clientnotify / setClientSysState (CSC lights, the PA countdown, the
//     teleporter black-room images, the "tae" FOV aftereffect): no CSC VM.
//     The server plays what it can of them (exploder fx and sounds below).
//   * T5 include_weapons / include_powerups: CoDOL's zm weapon set and
//     power-ups are data-driven (mp/zombiemode.csv).
//   * The C1 cyborg production line (cyborg_ent / cyborg_start ..) and the
//     codo_face_identification_machine props: C1 features no zm module of
//     this server reads yet.
//
// DELIBERATELY NOT HERE (as zm_sumpf_sh.gsc): maps\mp\_load, the
// maps/mp/zm_factory_sh_fx.gsc createfx module (C1 draws the zone's createfx
// itself), maps\mp\_compass.

#include maps\_zombiemode_utility;

main()
{
	// Before any wait: this is the level script's precache window.
	factory_precache();

	// Entity fields the trigger modules read (before levelMain).
	factory_t5_values();

	// Declare the initial zone and purchased-door graph before any level
	// initialization can yield or start the zone manager. Overlapping closed
	// room volumes must never select the legacy occupancy-unlocks fallback.
	factory_zone_init();

	maps\_zombiemode::levelMain( "zm_factory_sh" );

	// T5 zombie_cod5_factory.gsc init_sounds.
	add_sound( "door_slide_open", "door_slide_open" );


	// T5 places the players on initial_spawn_points in the receiver.
	level.zm_spawn_point_func = ::factory_spawn_point;

	level thread factory_power_watch();
	level thread factory_bridge();
	level thread factory_jump_from_bridge();
	level thread factory_trap_gates();
	level thread maps\zm_factory_sh_teleporter::init();
	level thread factory_test_watch();
}

factory_precache()
{
	precacheShellShock( "electrocution" );
	precacheShellShock( "explosion" );
	maps\zm_factory_sh_teleporter::precache();

	// Exploder effects: T5 plays them from the createfx exploders; the
	// server plays them itself (factory_exploder).  The paths are the zone's
	// own maps/mp/zm_factory_sh_fx.gsc ones.
	level._effect[ "mp_light_lamp_no_eo" ] = loadfx( "maps/mp_maps/fx_mp_light_lamp_no_eo" );
	level._effect[ "transporter_ambient" ] = loadfx( "maps/zombie/fx_transporter_ambient" );
	level._effect[ "transporter_beam" ] = loadfx( "maps/zombie/fx_transporter_beam" );
	level._effect[ "transporter_pad_start" ] = loadfx( "maps/zombie/fx_transporter_pad_start" );
	level._effect[ "transporter_start" ] = loadfx( "maps/zombie/fx_transporter_start" );
	level._effect[ "zombie_difference" ] = loadfx( "maps/zombie/fx_zombie_difference" );
	level._effect[ "zombie_elec_gen_idle" ] = loadfx( "misc/fx_zombie_elec_gen_idle" );
	level._effect[ "zombie_flashback_american" ] = loadfx( "maps/zombie/fx_zombie_flashback_american" );
	level._effect[ "zombie_heat_sink" ] = loadfx( "maps/zombie/fx_zombie_heat_sink" );
	level._effect[ "zombie_mainframe_beam" ] = loadfx( "maps/zombie/fx_zombie_mainframe_beam" );
	level._effect[ "zombie_mainframe_beam_start" ] = loadfx( "maps/zombie/fx_zombie_mainframe_beam_start" );
	level._effect[ "zombie_mainframe_flat" ] = loadfx( "maps/zombie/fx_zombie_mainframe_flat" );
	level._effect[ "zombie_mainframe_flat_start" ] = loadfx( "maps/zombie/fx_zombie_mainframe_flat_start" );
	level._effect[ "zombie_mainframe_link_all" ] = loadfx( "maps/zombie/fx_zombie_mainframe_link_all" );
	level._effect[ "zombie_mainframe_link_single" ] = loadfx( "maps/zombie/fx_zombie_mainframe_link_single" );
	level._effect[ "zombie_mainframe_link_single_01" ] = loadfx( "maps/zombie/fx_zombie_mainframe_link_single_01" );
	level._effect[ "zombie_mainframe_linked" ] = loadfx( "maps/zombie/fx_zombie_mainframe_linked" );
	level._effect[ "zombie_mainframe_steam" ] = loadfx( "maps/zombie/fx_zombie_mainframe_steam" );

	factory_exploder_init();
}

// --------------------------------------------------------------------------
// T5 values and entity fields.
//
// Box: T5's treasure chests cost 950 (the edit carries 1000; 950 also gives
// the ZOMBIE_RANDOM_WEAPON_950 hint, as on zm_sumpf_sh).
// Electric doors: T5 zombie_cod5_factory's two receiver doors
// (outside_south_east_door / outside_south_west_door) open with the power;
// the edit authors them script_noteworthy "factory_electric_door", cost
// 10000, which _zombiemode_blockers would sell.  T5's "electric_door" makes
// them power doors with the ZOMBIE_NEED_POWER hint.
// Traps: see factory_trap_gates.
// --------------------------------------------------------------------------
factory_t5_values()
{
	chests = getEntArray( "treasure_chest_use", "targetname" );
	for ( i = 0; i < chests.size; i++ )
		chests[ i ].zombie_cost = 950;

	doors = getEntArray( "zombie_door", "targetname" );
	electric = 0;
	for ( i = 0; i < doors.size; i++ )
	{
		if ( isDefined( doors[ i ].script_noteworthy ) && doors[ i ].script_noteworthy == "factory_electric_door" )
		{
			doors[ i ].script_noteworthy = "electric_door";
			electric++;
		}
	}

	// T5 electric_trap_think: 1000 points, re-armed after the cooldown;
	// T5 factory keeps the module's 40 s / 60 s defaults.
	traps = getEntArray( "zombie_trap", "targetname" );
	level.factory_traps = [];
	for ( i = 0; i < traps.size; i++ )
	{
		t = traps[ i ];
		if ( !isDefined( t.script_noteworthy ) || t.script_noteworthy != "electric" || !isDefined( t.script_string ) )
			continue;
		enable = undefined;
		switch ( t.script_string )
		{
		case "wh":	enable = "enter_warehouse_building"; break;
		case "wn":	enable = "enter_wnuen_building"; break;
		case "br":	enable = "bridge_down"; break;
		}
		if ( !isDefined( enable ) )
			continue;
		t.factory_enable_flag = enable;
		t.script_flag_wait = "factory_trap_" + t.script_string;
		flag_init( t.script_flag_wait );
		level.factory_traps[ level.factory_traps.size ] = t;
	}

	codolprintln( "CoDOL zm factory: T5 values: " + chests.size + " box spot(s) at 950, " + electric +
		" electric door(s), " + level.factory_traps.size + " gated trap(s)" );
}

// T5 electric_trap_think( enable_flag ): flag_wait( "power_on" ), then the
// trap stays off until its area is open.  _zombiemode_traps waits on one
// flag (script_flag_wait), so that flag is set here once both hold.
factory_trap_gates()
{
	for ( i = 0; i < level.factory_traps.size; i++ )
		level.factory_traps[ i ] thread factory_trap_gate();
}

factory_trap_gate()
{
	flag_wait( "power_on" );
	while ( !flag_exist( self.factory_enable_flag ) )
		wait( 0.5 );
	flag_wait( self.factory_enable_flag );
	flag_set( self.script_flag_wait );
	codolprintln( "CoDOL zm factory: trap " + self.script_string + " enabled (" + self.factory_enable_flag + ")" );
}

// --------------------------------------------------------------------------
// Zones.  T5 zombie_cod5_factory.gsc main: manage_zones( "receiver_zone" )
// and factory_zone_init, verbatim; the volume and flag names are the edit's.
// T5's runtime connect_zones calls become adjacencies on flags of our own,
// set where T5 calls connect_zones.
// --------------------------------------------------------------------------
factory_zone_init()
{
	flag_init( "factory_power_zones" );
	flag_init( "factory_bridge_zones" );
	flag_init( "factory_bridge_jump" );
	flag_init( "bridge_down" );

	level.zm_zone_initial = [];
	level.zm_zone_initial[ 0 ] = "receiver_zone";

	maps\_zombiemode_zone_manager::add_adjacent_zone( "receiver_zone", "outside_east_zone", "enter_outside_east" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "receiver_zone", "outside_west_zone", "enter_outside_west" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "wnuen_zone", "outside_east_zone", "enter_wnuen_building" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "wnuen_zone", "wnuen_bridge_zone", "enter_wnuen_loading_dock" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "warehouse_bottom_zone", "outside_west_zone", "enter_warehouse_building" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "warehouse_bottom_zone", "warehouse_top_zone", "enter_warehouse_second_floor" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "warehouse_top_zone", "bridge_zone", "enter_warehouse_second_floor" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "tp_east_zone", "wnuen_zone", "enter_tp_east" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "tp_east_zone", "outside_east_zone", "enter_tp_east", true );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "tp_south_zone", "outside_south_zone", "enter_tp_south" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "tp_west_zone", "warehouse_top_zone", "enter_tp_west" );

	// T5 power_electric_switch: connect_zones( east, south ) and
	// connect_zones( west, south, one_way ).
	maps\_zombiemode_zone_manager::add_adjacent_zone( "outside_east_zone", "outside_south_zone", "factory_power_zones" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "outside_west_zone", "outside_south_zone", "factory_power_zones", true );

	// T5 bridge_init once the bridges are down.
	maps\_zombiemode_zone_manager::add_adjacent_zone( "wnuen_bridge_zone", "bridge_zone", "factory_bridge_zones" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "warehouse_top_zone", "bridge_zone", "factory_bridge_zones" );

	// T5 jump_from_bridge, both one way.
	maps\_zombiemode_zone_manager::add_adjacent_zone( "outside_south_zone", "bridge_zone", "factory_bridge_jump", true );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "outside_south_zone", "wnuen_bridge_zone", "factory_bridge_jump", true );

	// T5 add_zone_flags( "enter_tp_east", "enter_wnuen_building" ).
	level thread factory_zone_flag_link( "enter_tp_east", "enter_wnuen_building" );
}

factory_zone_flag_link( from, to )
{
	while ( !flag_exist( from ) || !flag_exist( to ) )
		wait( 0.5 );
	flag_wait( from );
	flag_set( to );
}

// --------------------------------------------------------------------------
// T5 power_electric_switch, the level part: _zombiemode_power throws the
// switch and powers the machines; T5 then lights the factory (exploder 600)
// and opens the courtyard to both outside zones.
// --------------------------------------------------------------------------
factory_power_watch()
{
	flag_wait( "power_on" );
	wait_network_frame();
	factory_exploder( 600 );
	flag_set( "factory_power_zones" );
	codolprintln( "CoDOL zm factory: power on; courtyard zones connected" );
}

// --------------------------------------------------------------------------
// T5 bridge_init: both bridges stand raised until the power is on, then
// lower in 4 s, connect their paths and drop their clips.
// --------------------------------------------------------------------------
factory_bridge()
{
	wnuen_bridge = getEnt( "wnuen_bridge", "targetname" );
	warehouse_bridge = getEnt( "warehouse_bridge", "targetname" );
	if ( !isDefined( wnuen_bridge ) || !isDefined( warehouse_bridge ) )
	{
		codolprintln( "CoDOL zm factory: no bridge entities" );
		return;
	}

	coils = getEntArray( "wnuen_bridge_coils", "targetname" );
	for ( i = 0; i < coils.size; i++ )
		coils[ i ] linkTo( wnuen_bridge );
	coils = getEntArray( "warehouse_bridge_coils", "targetname" );
	for ( i = 0; i < coils.size; i++ )
		coils[ i ] linkTo( warehouse_bridge );

	// The clips close the gap while the bridges are up: T5 compiles their
	// links disconnected, this server cuts them at spawn.
	clips = [];
	clips[ 0 ] = getEnt( "wnuen_bridge_clip", "targetname" );
	clips[ 1 ] = getEnt( "warehouse_bridge_clip", "targetname" );
	for ( i = 0; i < clips.size; i++ )
	{
		if ( isDefined( clips[ i ] ) )
			clips[ i ] disconnectpaths();
	}

	wnuen_bridge rotatePitch( 90, 1, 0.5, 0.5 );
	warehouse_bridge rotatePitch( -90, 1, 0.5, 0.5 );
	wait( 1 );
	wnuen_bridge disconnectpaths();
	warehouse_bridge disconnectpaths();

	flag_wait( "power_on" );

	wnuen_bridge rotatePitch( -90, 4, 0.5, 1.5 );
	warehouse_bridge rotatePitch( 90, 4, 0.5, 1.5 );
	playSoundAtPosition( "bridge_lower", ( 304, -1252, 200 ) );

	factory_exploder( 500 );

	wnuen_bridge waittill( "rotatedone" );
	wnuen_bridge connectpaths();
	warehouse_bridge connectpaths();

	flag_set( "bridge_down" );
	playSoundAtPosition( "bridge_hit", ( 304, -1252, 200 ) );

	for ( i = 0; i < clips.size; i++ )
	{
		if ( isDefined( clips[ i ] ) )
		{
			clips[ i ] connectpaths();
			clips[ i ] delete();
		}
	}

	flag_set( "factory_bridge_zones" );
	codolprintln( "CoDOL zm factory: bridges down" );
}

// T5 jump_from_bridge.
factory_jump_from_bridge()
{
	trig = getEnt( "trig_outside_south_zone", "targetname" );
	if ( !isDefined( trig ) )
		return;
	for ( ;; )
	{
		trig waittill( "trigger", who );
		if ( isDefined( who ) && isPlayer( who ) )
			break;
	}
	flag_set( "factory_bridge_jump" );
}

// --------------------------------------------------------------------------
// Spawning: the four initial_spawn_points in the receiver, by script_int.
// --------------------------------------------------------------------------
factory_spawn_point()
{
	points = get_struct_array( "initial_spawn_points", "targetname" );
	if ( !isDefined( points ) || points.size < 1 )
		return undefined;

	for ( i = 1; i < points.size; i++ )
	{
		for ( j = i; j > 0 && factory_point_order( points[ j - 1 ] ) > factory_point_order( points[ j ] ); j-- )
		{
			swap = points[ j ];
			points[ j ] = points[ j - 1 ];
			points[ j - 1 ] = swap;
		}
	}

	for ( i = 0; i < points.size; i++ )
	{
		if ( !positionWouldTelefrag( points[ i ].origin ) )
			return factory_point( points[ i ] );
	}
	return factory_point( points[ randomInt( points.size ) ] );
}

factory_point_order( s )
{
	if ( isDefined( s.script_int ) )
		return int( s.script_int );
	return 99;
}

factory_point( s )
{
	point = spawnStruct();
	point.origin = s.origin;
	point.angles = ( 0, 0, 0 );
	if ( isDefined( s.angles ) )
		point.angles = s.angles;
	return point;
}

// --------------------------------------------------------------------------
// Exploders.  T5 calls exploder( n ) and the createfx exploders play.  On
// C1 the zone's createfx runs on the client with no exploder channel from
// this server, so the exploders T5's factory scripts fire are played here,
// from the zone's own maps/createfx/zm_factory_sh_fx.gsc entries (MEASURED,
// copied verbatim: effect, origin, angles, delay, sound, loop sound).
// --------------------------------------------------------------------------
factory_exploder_init()
{
	level.factory_exploders = [];
	factory_add_exploder( 101, "zombie_mainframe_link_all", ( -60.9077, 545.063, 649.894 ), ( 270, 0, 0 ), 0, "", "" );
	factory_add_exploder( 101, "zombie_mainframe_linked", ( -59.7638, 543.148, 635.402 ), ( 272, 90, -90 ), 0, "", "" );
	factory_add_exploder( 101, "zombie_elec_gen_idle", ( -493.222, 255.954, 88.551 ), ( 0, 90, 0 ), 0, "", "" );
	factory_add_exploder( 101, "zombie_elec_gen_idle", ( -497.541, 399.109, 28.583 ), ( 0, 90, 0 ), 0, "", "" );
	factory_add_exploder( 101, "zombie_elec_gen_idle", ( -486.577, 282.212, 31.273 ), ( 0, 90, 0 ), 0, "", "" );
	factory_add_exploder( 101, "zombie_elec_gen_idle", ( -490.619, 418.622, 93.305 ), ( 0, 90, 0 ), 0, "", "" );
	factory_add_exploder( 101, "", ( -503.337, 311.013, 108.118 ), ( 0, 90, 0 ), 0, "", "zm_sh_sparks_power_station" );
	factory_add_exploder( 102, "zombie_mainframe_link_single", ( -1.26648, 566.41, 545.173 ), ( 340, 30, 0 ), 0, "", "" );
	factory_add_exploder( 102, "zombie_difference", ( 1499.96, 1141.3, 268.41 ), ( 0, 244, 0 ), 0, "", "power_loop" );
	factory_add_exploder( 102, "zombie_difference", ( 1042.73, 1146.5, 272.971 ), ( 0, 288, 0 ), 0, "", "power_loop" );
	factory_add_exploder( 102, "transporter_ambient", ( 1257.84, 1275.66, 416.591 ), ( 270, 359.484, 0.516024 ), 0, "", "" );
	factory_add_exploder( 102, "", ( 1264.07, 1265.49, 306.074 ), ( 270, 0, 0 ), 0, "", "zm_sh_power_hum" );
	factory_add_exploder( 102, "zombie_mainframe_link_single_01", ( -58.2454, 510.333, 313.647 ), ( 340, 30, 0 ), 0, "", "" );
	factory_add_exploder( 103, "zombie_mainframe_link_single", ( -57.1328, 482.405, 548.721 ), ( 334, 270, 0 ), 0, "", "" );
	factory_add_exploder( 103, "zombie_difference", ( 500.757, -3128.68, 258.287 ), ( 0, 92, 0 ), 0, "", "power_loop" );
	factory_add_exploder( 103, "zombie_difference", ( 80.5138, -3126.37, 258.767 ), ( 0, 88, 0 ), 0, "", "power_loop" );
	factory_add_exploder( 103, "transporter_ambient", ( 305.166, -3190.55, 405.3 ), ( 270, 359.484, 0.516024 ), 0, "", "" );
	factory_add_exploder( 103, "", ( 311.576, -3197, 305.223 ), ( 270, 0, 0 ), 0, "", "zm_sh_power_hum" );
	factory_add_exploder( 103, "zombie_mainframe_link_single_01", ( -55.0748, 508.751, 366.559 ), ( 334, 270, 0 ), 0, "", "" );
	factory_add_exploder( 104, "zombie_mainframe_link_single", ( -115.997, 566.44, 541.636 ), ( 340, 150, 0 ), 0, "", "" );
	factory_add_exploder( 104, "zombie_difference", ( -1655.34, -1323.81, 303.465 ), ( 0, 38, 0 ), 0, "", "power_loop" );
	factory_add_exploder( 104, "zombie_difference", ( -1646.82, -901.83, 300.38 ), ( 0, 322, 0 ), 0, "", "power_loop" );
	factory_add_exploder( 104, "transporter_ambient", ( -1782.32, -1111.53, 448.898 ), ( 270, 359.484, 0.516024 ), 0, "", "" );
	factory_add_exploder( 104, "", ( -1776.42, -1109.78, 331.355 ), ( 270, 0, 0 ), 0, "", "zm_sh_power_hum" );
	factory_add_exploder( 104, "zombie_mainframe_link_single_01", ( -56.1562, 510.765, 412.394 ), ( 340, 150, 0 ), 0, "", "" );
	factory_add_exploder( 105, "zombie_mainframe_beam_start", ( -56.7883, 296.402, 104 ), ( 270, 0, 0 ), 0, "", "" );
	factory_add_exploder( 105, "zombie_mainframe_flat_start", ( -56.8396, 296.767, 104 ), ( 270, 0, 0 ), 0, "", "" );
	factory_add_exploder( 106, "zombie_mainframe_beam", ( -59.2913, 295.86, 105 ), ( 270, 0, 0 ), 0, "", "" );
	factory_add_exploder( 106, "zombie_mainframe_flat", ( -57.8089, 297.147, 104 ), ( 270, 180, 180 ), 0, "", "" );
	factory_add_exploder( 106, "zombie_mainframe_steam", ( 0.308329, 523.282, 314.562 ), ( 282.64, 341.411, 6.16853 ), 1.5, "", "" );
	factory_add_exploder( 106, "zombie_heat_sink", ( 21.1592, 540.42, 162.688 ), ( 0, 0, 0 ), 2.5, "", "" );
	factory_add_exploder( 106, "zombie_heat_sink", ( -139.955, 547.623, 162.567 ), ( 0, 182, 0 ), 2.5, "", "" );
	factory_add_exploder( 106, "zombie_mainframe_steam", ( -54.7512, 481.627, 310.079 ), ( 282, 270, 90 ), 1.5, "", "" );
	factory_add_exploder( 106, "zombie_mainframe_steam", ( -112.923, 522.773, 317.338 ), ( 279.995, 207.512, 158.442 ), 1.5, "", "" );
	factory_add_exploder( 106, "zombie_mainframe_steam", ( -89.1748, 588.628, 315.139 ), ( 275.192, 124.271, -89.7115 ), 1.5, "", "" );
	factory_add_exploder( 106, "zombie_mainframe_steam", ( -20.997, 590.789, 313.061 ), ( 278.067, 28.8485, 5.7661 ), 1.5, "", "" );
	factory_add_exploder( 201, "transporter_beam", ( 1261.84, 1275.33, 199 ), ( 270, 0, 0 ), 0, "", "" );
	factory_add_exploder( 201, "zombie_heat_sink", ( 1339.91, 1270.03, 237.368 ), ( 0, 0, 0 ), 2.5, "", "" );
	factory_add_exploder( 201, "zombie_heat_sink", ( 1185.24, 1273.5, 237.48 ), ( 359.999, 174.015, -2.0145 ), 2.5, "", "" );
	factory_add_exploder( 202, "transporter_pad_start", ( 1261.96, 1275.22, 199 ), ( 270, 180, 180 ), 0, "pad_warmup", "" );
	factory_add_exploder( 202, "transporter_start", ( 1316.86, 1140.98, 410.125 ), ( 330, 292, 0 ), 0, "", "" );
	factory_add_exploder( 202, "transporter_start", ( 1210.07, 1141.36, 411.49 ), ( 334.017, 250.225, -0.975173 ), 0, "", "" );
	factory_add_exploder( 202, "transporter_start", ( 1135.1, 1216.45, 410.802 ), ( 332, 204, 0 ), 0, "", "" );
	factory_add_exploder( 202, "transporter_start", ( 1135.33, 1323.12, 411.347 ), ( 0, 0, 0 ), 0, "", "" );
	factory_add_exploder( 202, "transporter_start", ( 1392.36, 1215.54, 410.804 ), ( 338, 336, 0 ), 0, "", "" );
	factory_add_exploder( 202, "transporter_start", ( 1393.56, 1321.76, 410.526 ), ( 336, 20, 0 ), 0, "", "" );
	factory_add_exploder( 202, "", ( 1341.91, 1272.5, 230.268 ), ( 270, 0, 0 ), 0, "pad2l_warmup", "" );
	factory_add_exploder( 202, "", ( 1187.61, 1276.14, 228.89 ), ( 270, 0, 0 ), 0, "pad2r_warmup", "" );
	factory_add_exploder( 301, "transporter_beam", ( 301.042, -3194.96, 188.777 ), ( 270, 84.7976, -84.7976 ), 0, "", "" );
	factory_add_exploder( 301, "zombie_heat_sink", ( 380.683, -3209.81, 225.959 ), ( 359.583, 6.00742, 1.97082 ), 2.5, "", "" );
	factory_add_exploder( 301, "zombie_heat_sink", ( 219.515, -3216.04, 228.043 ), ( 0.621098, 192.004, -1.9164 ), 2.5, "", "" );
	factory_add_exploder( 302, "transporter_pad_start", ( 301.049, -3195, 188.777 ), ( 270, 84.7976, -84.7976 ), 0, "pad_warmup", "" );
	factory_add_exploder( 302, "transporter_start", ( 168.208, -3243.43, 396.253 ), ( 332, 204, 0 ), 0, "", "" );
	factory_add_exploder( 302, "transporter_start", ( 168.414, -3134.5, 396.798 ), ( 332, 158, 0 ), 0, "", "" );
	factory_add_exploder( 302, "transporter_start", ( 246.136, -3057.42, 396.836 ), ( 336, 114, 0 ), 0, "", "" );
	factory_add_exploder( 302, "transporter_start", ( 355.679, -3056.93, 396.678 ), ( 334, 68, 0 ), 0, "", "" );
	factory_add_exploder( 302, "transporter_start", ( 434.371, -3134.25, 396.977 ), ( 336, 20, 0 ), 0, "", "" );
	factory_add_exploder( 302, "transporter_start", ( 430.107, -3242.56, 397.255 ), ( 338, 336, 0 ), 0, "", "" );
	factory_add_exploder( 302, "", ( 383.125, -3209.61, 221.102 ), ( 270, 0, 0 ), 0, "pad2r_warmup", "" );
	factory_add_exploder( 302, "", ( 217.579, -3215.59, 222.48 ), ( 270, 0, 0 ), 0, "pad2l_warmup", "" );
	factory_add_exploder( 401, "transporter_beam", ( -1780.58, -1115.1, 230.734 ), ( 270, 0, 0 ), 0, "", "" );
	factory_add_exploder( 401, "zombie_heat_sink", ( -1780.11, -1025.11, 275.545 ), ( 358.029, 96.0215, -0.417649 ), 2.5, "", "" );
	factory_add_exploder( 401, "zombie_heat_sink", ( -1786.26, -1201.6, 274.702 ), ( 2.01335, 266.015, 0 ), 2.5, "", "" );
	factory_add_exploder( 402, "transporter_pad_start", ( -1782.5, -1114.83, 229.734 ), ( 270, 180, 180 ), 0, "pad_warmup", "" );
	factory_add_exploder( 402, "transporter_start", ( -1725.46, -1238.68, 442.886 ), ( 330, 292, 0 ), 0, "", "" );
	factory_add_exploder( 402, "transporter_start", ( -1832.39, -1236.9, 442.251 ), ( 334, 248, 0 ), 0, "", "" );
	factory_add_exploder( 402, "transporter_start", ( -1831.05, -979.918, 442.146 ), ( 336, 114, 0 ), 0, "", "" );
	factory_add_exploder( 402, "transporter_start", ( -1724.55, -981.168, 441.988 ), ( 334, 68, 0 ), 0, "", "" );
	factory_add_exploder( 402, "transporter_start", ( -1649.21, -1055.98, 442.287 ), ( 336, 20, 0 ), 0, "", "" );
	factory_add_exploder( 402, "transporter_start", ( -1650.8, -1161.65, 441.565 ), ( 338, 336, 0 ), 0, "", "" );
	factory_add_exploder( 402, "", ( -1783.9, -1195.21, 270.891 ), ( 270, 0, 0 ), 0, "pad2r_warmup", "" );
	factory_add_exploder( 402, "", ( -1782.64, -1022.24, 272.269 ), ( 270, 0, 0 ), 0, "pad2l_warmup", "" );
	factory_add_exploder( 500, "zombie_mainframe_steam", ( 458.257, -1183.51, 221.009 ), ( 360, 130, 0 ), 0, "", "" );
	factory_add_exploder( 500, "zombie_mainframe_steam", ( 152.499, -1184.86, 183.69 ), ( 356, 46, 0 ), 0, "", "" );
	factory_add_exploder( 500, "zombie_mainframe_steam", ( 152.272, -1322.3, 183.965 ), ( 348, 316, 0 ), 0, "", "" );
	factory_add_exploder( 500, "zombie_mainframe_steam", ( 458.461, -1319.7, 222.285 ), ( 0, 224, 0 ), 0, "", "" );
	factory_add_exploder( 600, "mp_light_lamp_no_eo", ( -1234.91, -1521.39, 342.882 ), ( 90, 0, 0 ), 0, "light_start", "light" );
	factory_add_exploder( 600, "mp_light_lamp_no_eo", ( -1503.49, -1517.29, 346.273 ), ( 90, 0, 0 ), 0, "light_start", "light" );
	factory_add_exploder( 600, "mp_light_lamp_no_eo", ( -87.5603, -979.828, 310.919 ), ( 16.8245, 204.51, -170.869 ), 0, "light_start", "light" );
	factory_add_exploder( 600, "mp_light_lamp_no_eo", ( -1002.06, -1430.08, 342.427 ), ( 90, 0, 0 ), 0, "light_start", "light" );
	factory_add_exploder( 600, "mp_light_lamp_no_eo", ( 14.8, -2643.72, 441.815 ), ( 90, 341.078, -18.9216 ), 0, "light_start", "light" );
	factory_add_exploder( 600, "mp_light_lamp_no_eo", ( 647.858, -2239.82, 413.975 ), ( 17.9318, 6.2729, 1.30888 ), 0, "light_start", "light" );
	factory_add_exploder( 600, "mp_light_lamp_no_eo", ( 442.475, -1144.06, 210.389 ), ( 90, 0, 0 ), 0, "light_start", "light" );
	factory_add_exploder( 600, "mp_light_lamp_no_eo", ( 611.79, -1432.51, 208.71 ), ( 90, 0, 0 ), 0, "light_start", "light" );
	factory_add_exploder( 600, "mp_light_lamp_no_eo", ( 447.645, -980.478, 397.002 ), ( 36.2142, 210.037, -164.364 ), 0, "light_start", "light" );
	factory_add_exploder( 600, "mp_light_lamp_no_eo", ( -361.198, -525.006, 173.158 ), ( 17.7609, 303.726, -141.551 ), 0, "light_start", "light" );
	factory_add_exploder( 901, "zombie_flashback_american", ( 2047.92, -279.977, 115.73 ), ( 0, 270, 0 ), 0, "", "" );
	factory_add_exploder( 902, "zombie_flashback_american", ( 2367.92, -279.977, 115.73 ), ( 0, 270, 0 ), 0, "", "" );
	factory_add_exploder( 903, "zombie_flashback_american", ( 2687.92, -279.977, 115.73 ), ( 0, 270, 0 ), 0, "", "" );
	factory_add_exploder( 904, "zombie_flashback_american", ( 3007.92, -279.977, 115.73 ), ( 0, 270, 0 ), 0, "", "" );
}

factory_add_exploder( id, fx, origin, angles, delay, sound, loop )
{
	e = spawnStruct();
	e.id = id;
	e.fx = fx;
	e.origin = origin;
	e.angles = angles;
	e.delay = delay;
	e.sound = sound;
	e.loop = loop;
	level.factory_exploders[ level.factory_exploders.size ] = e;
}

factory_exploder( id )
{
	level notify( "exploding_" + id );
	for ( i = 0; i < level.factory_exploders.size; i++ )
	{
		e = level.factory_exploders[ i ];
		if ( e.id == id )
			e thread factory_exploder_play();
	}
}

factory_exploder_play()
{
	if ( self.delay > 0 )
		wait( self.delay );

	if ( self.fx != "" && isDefined( level._effect[ self.fx ] ) )
	{
		fwd = anglesToForward( self.angles );
		up = anglesToUp( self.angles );
		fx = spawnFx( level._effect[ self.fx ], self.origin, fwd, up );
		triggerFx( fx );
	}
	if ( self.sound != "" && soundExists( self.sound ) )
		playSoundAtPosition( self.sound, self.origin );
	if ( self.loop != "" && soundExists( self.loop ) && !isDefined( self.loop_ent ) )
	{
		self.loop_ent = spawn( "script_origin", self.origin );
		self.loop_ent playLoopSound( self.loop );
	}
}

// --------------------------------------------------------------------------
// Developer-only test verbs (developer servers only), scr_factorytest:
//   pad <client> <0..2>   stand the client on the pad and use it
//   core <client>         stand the client at the mainframe and use it
//   state                 print the teleporter, bridge and trap state
// --------------------------------------------------------------------------
factory_test_watch()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;
	setDvar( "scr_factorytest", "" );
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_factorytest" );
		if ( request == "" )
			continue;
		setDvar( "scr_factorytest", "" );
		tokens = strTok( request, " " );
		players = getPlayers();
		switch ( tokens[ 0 ] )
		{
		case "pad":
		case "core":
			if ( tokens.size < 2 || int( tokens[ 1 ] ) >= players.size )
				break;
			p = players[ int( tokens[ 1 ] ) ];
			name = "trigger_teleport_core";
			if ( tokens[ 0 ] == "pad" )
				name = "trigger_teleport_pad_" + tokens[ 2 ];
			trig = getEnt( name, "targetname" );
			p setOrigin( trig.origin );
			wait( 0.2 );
			trig notify( "trigger", p );
			codolprintln( "CoDOL factorytest: " + p.name + " uses " + name + " at " + trig.origin );
			break;
		case "state":
			text = "CoDOL factorytest: power " + flag( "power_on" ) + " bridge " + flag( "bridge_down" ) + " links " + level.active_links;
			for ( i = 0; i < level.teleport.size; i++ )
				text = text + " pad" + i + "=" + level.teleport[ i ];
			for ( i = 0; i < level.factory_traps.size; i++ )
				text = text + " trap_" + level.factory_traps[ i ].script_string + "=" + flag( level.factory_traps[ i ].script_flag_wait );
			text = text + " pack_door_clip " + getEntArray( "pack_door_clip", "targetname" ).size;
			if ( players.size > 0 )
				text = text + " p0 at " + players[ 0 ].origin + " score " + players[ 0 ].score;
			codolprintln( text );
			break;
		}
	}
}
