// zm_sumpf_sh level script: Shi No Numa, CoDOL's own edit (zm_sumpf_sh.ff,
// build 3.3.3.3, ported to 1.1.1.13).
//
// GScr_LoadScripts asks for exactly one level module per match, maps/<mapname>
// for a zm match (CODOL_ScriptEntryPoints, src/codol/script/codol_script_source.c),
// as zm_asylum_sh.gsc and zm_prototype_sh.gsc explain.  The client's arena
// data already gives zm_sumpf_sh gametype "zm"
// (docs/CODOL_ZOMBIES_STARTUP_20260915.md section 3.1).
//
// BEHAVIOUR REFERENCE: T5 maps\zombie_cod5_sumpf.gsc and its modules
// (D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps: zombie_cod5_sumpf_magic_box,
// _perks, _trap_pendulum, _trap_perk_electric, _zipline).  CoDOL's edit is
// NOT T5's entity layout: it is authored for the data-driven _zombiemode
// modules, like Verruckt and Prototype.  MEASURED from the 3.3.3.3 MapEnts
// (4269 entities):
//
//   perks         4 zombie_vending trigger_use, FIXED machines (jugg NW hut,
//                 revive NE, speed cola SE, double tap SW) -> _zombiemode_perks.
//                 T5's random_vending_start_location / randomize_vending_
//                 machines does not apply: the edit has no random spots.
//   pack-a-punch  zombie_vending_upgrade (specialty_weapupgrade) -> _perks.
//   box           6 treasure_chest_use (start_chest + nw/ne/se/sw/attic) ->
//                 lid magic_box_lid_N -> magic_box_weapon_spawn_N ->
//                 magic_box_base_N, the chain _zombiemode_weapons reads.
//   doors/debris  9 zombie_door + 2 zombie_debris -> _zombiemode_blockers,
//                 whose script_flag fan-out sets T5's sumpf flags
//                 (unlock_hospital_downstairs/_upstairs, nw/ne/se/sw_magic_box,
//                 <hut>_building_unlocked).  The wooden doors carry
//                 script_sound "wooden_door", registered below as T5
//                 init_sounds does.
//   zones         19 player_volume info_volumes -> the zone manager.  The
//                 adjacency graph is T5 sumpf_zone_init's, below.
//   hounds        <zone>_spawners_dog structs -> _zombiemode_ai_dogs (on by
//                 default; T5 sumpf sets level.dogs_enabled too).
//   power         4 use_elec_switch, one per hut, each powering its own hut
//                 (see sumpf_power_init).
//   traps         4 zombie_trap "electric" (ne/nw/se/sw) -> _zombiemode_traps,
//                 which waits on each hut's own power flag for them; 1 zombie_trap "rotating"
//                 (the log trap, script_flag_wait nw_magic_box), which that
//                 module does not run -> maps\zm_sumpf_sh_logtrap.
//   rescue        rescue_dest / blackhawk_minigun chain -> _zombiemode_extraction.
//   intel         12 intel_collectible (script_index 156..167) -> _zombiemode_intel.
//
// NOT PORTED, and why:
//   * T5 sumpf zipline (zombie_cod5_sumpf_zipline): the edit carries only the
//     zipline's ambient sound origins (zip_line_wheel / _rope / _switch); there
//     is no zipline trigger, no zipline_deactivated_hint_trigger, no rope or
//     platform entity to move, so there is nothing to drive.
//   * T5 random perks (zombie_cod5_sumpf_perks) -- fixed machines here.
//   * The radios and super egg (T5 radio_* / superegg_*): their triggers
//     (radio_one, battle_radio_trigger, ...) are not in the edit.  The toilet
//     egg's trigger is, but none of its aliases (phone_hook, dial_9, riiing,
//     toilet_flush) ships in the port or the zm common zones.
//   * The meteor and the KZMB radio (sumpf_kzmb): character VO and T5's
//     _waw_zombiemode_radio, neither of which this server has.
//   * Zombie risers (<zone>_spawners_rise, script_string in_water) and
//     southwest_initial_zombies: the spawner module has no rise-from-ground
//     path; the zones use their zombie_spawner structs only.
//   * teleporter_portal / jump_target / teleport_location, care_package /
//     zombie_care_package and the ammo_cache / ammobox_trigger prefabs are
//     C1 features no zm module of this server reads yet.
//   * setup_water_physics (phys_buoyancy client dvar), the
//     water_burst_overwrite rise effects, SetCullDist and the light-grid
//     dvars: client presentation T5 sets from its level script.  The
//     swamp's WADING (slow, no prone) is not script in T5 but engine water;
//     see sumpf_water below.
//   * clientnotify, the CSC radio/zipline/perk effects: no CSC VM here.
//
// DELIBERATELY NOT HERE (as zm_asylum_sh.gsc): maps\mp\_load, the
// maps/mp/zm_sumpf_sh_fx.gsc createfx module (C1 draws the zone's createfx
// itself), maps\mp\_compass.

#include maps\_zombiemode_utility;

main()
{
	// Before any wait: this is the level script's precache window.
	sumpf_precache();

	// T5 Shi No Numa purchase values (before levelMain, whose modules read them).
	sumpf_t5_values();

	maps\_zombiemode::levelMain( "zm_sumpf_sh" );

	// T5 zombie_cod5_sumpf.gsc init_sounds: the hut doors' script_sound.
	add_sound( "wooden_door", "zmb_door_wood_open" );

	sumpf_zone_init();

	// T5 sumpf_player_spawn_placement: the four initial_spawn_points
	// upstairs in the hospital, not the single mp_* spawn entity.
	level.zm_spawn_point_func = ::sumpf_spawn_point;

	sumpf_power_init();
	level thread maps\zm_sumpf_sh_logtrap::init();
	level thread sumpf_book_useage();
	level thread sumpf_fog();
	sumpf_water();
}

// --------------------------------------------------------------------------
// Swamp water.  In T5 the wading is engine behaviour, not script: pmove reads
// the water's height over the player's feet (CM_GetWaterHeight, PM_GetWaterLevel:
// level 1 from 3 units deep, 2 from 24, 3 from 45) and then scales movement by
// 1 - 0.4 * level / 3, refuses prone from level 1 (raising a prone player to
// crouch), refuses crouch and sprint from level 2.  The server does the same
// once the level declares its water (src/codol/player/codol_water.c); C1's
// prediction follows through the published speed scale and stance/sprint
// locks.  Zombies and hounds are not slowed: T5 has no actor water term.
//
// The height.  C1's zm_sumpf_sh has no water volume (MEASURED 2026-10-22:
// codol_isPointInWater is false at every riser and mud-bubble struct from the
// swamp floor to 120 units above it; the bullet trace lands on "mud").  T5's
// own createfx puts the swamp's low fog ON the water: fx_fog_low_floor_sm at
// z -685.875 / -686.875 / -687.875 over floors this map has at -716..-741
// (the same file's maggots sit within 3 units of this map's hut floors, so
// the two share their geometry).  The swamp floor is mostly -720..-730, so a
// player wades at level 2; the deepest pockets (-740) reach level 3.
// --------------------------------------------------------------------------

sumpf_water()
{
	codol_setLevelWaterHeight( -687 );
}

// --------------------------------------------------------------------------
// Fog.  T5 clientscripts\zombie_cod5_sumpf_fx.csc level_fog_init /
// dog_start_monitor / dog_stop_monitor set height fog (setVolFog); C1 has
// exponential fog only, so each setting is the T5 density at the swamp floor
// (z -500): halfway = half_dist * 2 ^ ( ( -500 - base_height ) / half_height ).
//   normal  start 452.915  half_dist 161.557  half_height 426.143  base -675.164  -> 215
//   hounds  start 1622.47  half_dist 2126.6   half_height 288.118  base  399.766  -> 244
// T5's blue-green colours read as a blue tint in C1, so the colours are dark
// grey instead (lighter for hound rounds, as T5's are).
// zm maps do not run maps\mp\_load, whose codolFogDvar sends C1's r_fog
// (dvar 1464); a previous map may have turned it off, so it is sent here.
// --------------------------------------------------------------------------

sumpf_fog()
{
	level thread sumpf_fog_dvar();

	setExpFog( 452.915, 215, 0.30, 0.31, 0.30, 1, 0 );

	// _zombiemode_ai_dogs owns the flag; it may be created after main().
	while ( !flag_exist( "dog_round" ) )
		wait( 0.5 );
	for ( ;; )
	{
		flag_wait( "dog_round" );
		setExpFog( 1622.47, 244, 0.38, 0.39, 0.38, 1, 7 );
		flag_waitopen( "dog_round" );
		setExpFog( 452.915, 215, 0.30, 0.31, 0.30, 1, 7 );
	}
}

sumpf_fog_dvar()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player setClientDvar( "1464", 1 );
	}
}

sumpf_precache()
{
	// The log trap's cage lights (zombie_zapper_cagelight_* ship in the map
	// zone).  _zombiemode_traps loads the same names for the electric traps;
	// the log trap must not depend on that module having found one.
	precacheModel( "zombie_zapper_cagelight_red" );
	precacheModel( "zombie_zapper_cagelight_green" );
	level._effect[ "zapper_light_ready" ] = loadfx( "misc/fx_zombie_zapper_light_green" );
	level._effect[ "zapper_light_notready" ] = loadfx( "misc/fx_zombie_zapper_light_red" );
}

// --------------------------------------------------------------------------
// Zones.  T5 zombie_cod5_sumpf.gsc main: manage_zones( "center_building_
// upstairs" ) and sumpf_zone_init, verbatim.  The volume names and flag
// names are the edit's own (MEASURED).
// --------------------------------------------------------------------------

sumpf_zone_init()
{
	level.zm_zone_initial = [];
	level.zm_zone_initial[ 0 ] = "center_building_upstairs";

	maps\_zombiemode_zone_manager::add_adjacent_zone( "center_building_upstairs", "center_building_upstairs_buy", "unlock_hospital_upstairs" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "center_building_upstairs", "center_building_combined", "unlock_hospital_downstairs" );

	maps\_zombiemode_zone_manager::add_adjacent_zone( "center_building_upstairs_buy", "center_building_combined", "unlock_hospital_upstairs" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "center_building_upstairs_buy", "center_building_combined", "unlock_hospital_downstairs" );

	maps\_zombiemode_zone_manager::add_adjacent_zone( "center_building_combined", "northeast_outside", "ne_magic_box" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "center_building_combined", "northwest_outside", "nw_magic_box" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "center_building_combined", "southeast_outside", "se_magic_box" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "center_building_combined", "southwest_outside", "sw_magic_box" );

	maps\_zombiemode_zone_manager::add_adjacent_zone( "northeast_outside", "northeast_building", "northeast_building_unlocked" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "northwest_outside", "northwest_building", "northwest_building_unlocked" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "southeast_outside", "southeast_building", "southeast_building_unlocked" );
	maps\_zombiemode_zone_manager::add_adjacent_zone( "southwest_outside", "southwest_building", "southwest_building_unlocked" );
}

// --------------------------------------------------------------------------
// Spawning.  T5 sumpf_player_spawn_placement: player i on initial_spawn_points
// i.  Here the first free point in script_int order (1..4), so a respawn
// between rounds lands upstairs too.  self = the spawning player.
// --------------------------------------------------------------------------

sumpf_spawn_point()
{
	points = get_struct_array( "initial_spawn_points", "targetname" );
	if ( !isDefined( points ) || points.size < 1 )
		return undefined;

	// Insertion sort by script_int.
	for ( i = 1; i < points.size; i++ )
	{
		for ( j = i; j > 0 && sumpf_point_order( points[ j - 1 ] ) > sumpf_point_order( points[ j ] ); j-- )
		{
			swap = points[ j ];
			points[ j ] = points[ j - 1 ];
			points[ j - 1 ] = swap;
		}
	}

	for ( i = 0; i < points.size; i++ )
	{
		if ( !positionWouldTelefrag( points[ i ].origin ) )
			return sumpf_point( points[ i ] );
	}
	return sumpf_point( points[ randomInt( points.size ) ] );
}

sumpf_point_order( s )
{
	if ( isDefined( s.script_int ) )
		return int( s.script_int );
	return 99;
}

// A struct may carry no angles; spawn() needs both.
sumpf_point( s )
{
	point = spawnStruct();
	point.origin = s.origin;
	point.angles = ( 0, 0, 0 );
	if ( isDefined( s.angles ) )
		point.angles = s.angles;
	return point;
}

// --------------------------------------------------------------------------
// Power.  T5 Shi No Numa has no power switch; CoDOL's edit authors FOUR, one
// in each hut: a use_elec_switch trigger_use, an elec_switch lever
// (zombie_power_lever_handle_arm03_off), an elec_body, an elec_switch_fx
// struct and two power_off_fx origins.  MEASURED positions put each switch
// beside one perk machine and one electric trap (NW 8557 2980: Juggernog and
// trap "nw"; NE 11487 3356: Quick Revive and "ne"; SE-hut 12598 -1157: Speed
// Cola and "sw"; SW-hut 7313 -1135: Double Tap and "se" -- the edit's trap
// letters are swapped against the huts, the distances are not).
//
// So each switch powers its own hut only: the nearest zombie_vending trigger
// and the nearest electric zombie_trap get their own flag
// ("sumpf_power_<n>") through the zm_power_flag field _zombiemode_perks,
// _zombiemode_traps and _zombiemode_audio read, and the switch throws only
// its own lever.  _zombiemode_power runs sumpf_power_run (level.zm_power_func)
// instead of its single master switch.
//
// PACK-A-PUNCH (zombie_vending_upgrade in the centre building, no hut of its
// own) comes on once ALL FOUR huts are powered (the user's rule for this map;
// nothing in the edit ties it to a switch).  The first hut sets the map-wide "power_on" flag,
// which on this map gates nothing per-hut (every machine and trap carries its
// own flag); it keeps "the power is on somewhere" meaning what it means on
// the other maps.
// --------------------------------------------------------------------------

// Runs from main(): data only, before the trigger modules read the fields.
sumpf_power_init()
{
	trigs = getEntArray( "use_elec_switch", "targetname" );
	if ( trigs.size < 1 )
		return;

	levers = getEntArray( "elec_switch", "targetname" );
	sparks = get_struct_array( "elec_switch_fx", "targetname" );
	machines = getEntArray( "zombie_vending", "targetname" );

	traps = [];
	all_traps = getEntArray( "zombie_trap", "targetname" );
	for ( i = 0; i < all_traps.size; i++ )
	{
		if ( isDefined( all_traps[ i ].script_noteworthy ) && all_traps[ i ].script_noteworthy == "electric" )
			traps[ traps.size ] = all_traps[ i ];
	}

	level.sumpf_huts = [];
	for ( i = 0; i < trigs.size; i++ )
	{
		hut = spawnStruct();
		hut.trig = trigs[ i ];
		hut.flag_name = "sumpf_power_" + i;
		hut.lever = sumpf_nearest( trigs[ i ].origin, levers );
		hut.sparks = sumpf_nearest( trigs[ i ].origin, sparks );
		hut.machine = sumpf_nearest( trigs[ i ].origin, machines );
		hut.trap = sumpf_nearest( trigs[ i ].origin, traps );
		flag_init( hut.flag_name );

		text = "CoDOL zm sumpf: hut switch " + i + " at " + trigs[ i ].origin + " -> flag " + hut.flag_name;
		if ( isDefined( hut.machine ) )
		{
			hut.machine.zm_power_flag = hut.flag_name;
			text = text + ", machine " + hut.machine.script_noteworthy + " at " + hut.machine.origin +
				" (" + int( distance( hut.machine.origin, trigs[ i ].origin ) ) + ")";
		}
		else
			text = text + ", no machine";
		if ( isDefined( hut.trap ) )
		{
			hut.trap.zm_power_flag = hut.flag_name;
			trap_id = "?";
			if ( isDefined( hut.trap.script_string ) )
				trap_id = hut.trap.script_string;
			text = text + ", trap " + trap_id + " at " + hut.trap.origin +
				" (" + int( distance( hut.trap.origin, trigs[ i ].origin ) ) + ")";
		}
		else
			text = text + ", no trap";
		if ( isDefined( hut.lever ) )
			text = text + ", lever at " + hut.lever.origin;
		codolprintln( text );

		level.sumpf_huts[ level.sumpf_huts.size ] = hut;
	}

	level.zm_power_func = ::sumpf_power_run;
}

sumpf_nearest( origin, list )
{
	if ( !isDefined( list ) || list.size < 1 )
		return undefined;
	return get_closest( origin, list );
}

// _zombiemode_power::init runs this in place of its master switch.
sumpf_power_run()
{
	for ( i = 0; i < level.sumpf_huts.size; i++ )
	{
		hut = level.sumpf_huts[ i ];
		if ( isDefined( hut.lever ) )
			hut.lever notSolid();
		hut.trig setHintString( &"ZOMBIE_ELECTRIC_SWITCH" );
		hut.trig setCursorHint( "HINT_NOICON" );
		hut thread sumpf_hut_switch_think();
	}
}

// self = a hut.  T5 zombie_theater.gsc electric_switch :347 and
// wait_for_power :368, for one hut.
sumpf_hut_switch_think()
{
	for ( ;; )
	{
		self.trig waittill( "trigger", user );
		if ( !isDefined( user ) || !is_player_valid( user ) )
			continue;
		if ( !user useButtonPressed() )
			continue;
		break;
	}

	codolprintln( "CoDOL zm sumpf: hut switch at " + self.trig.origin + " thrown by " + user.name + "; " + self.flag_name + " on" );
	self.trig delete();

	flag_set( self.flag_name );

	first = !flag( "power_on" );
	if ( first )
		flag_set( "power_on" );

	if ( isDefined( self.lever ) )
	{
		self.lever rotateRoll( -90, 0.3 );
		self.lever play_sound_on_ent( "switch_flip" );
		wait( 0.3 );
		if ( isDefined( self.sparks ) && isDefined( level._effect[ "switch_sparks" ] ) )
			playFx( level._effect[ "switch_sparks" ], self.sparks.origin );
		self.lever play_sound_on_ent( "power_on" );
	}

	if ( first )
	{
		// _zombiemode_power::wait_for_power's notifies for the first power.
		level notify( "switch_flipped" );
		wait_network_frame();
		level notify( "master_switch_activated" );
		codolprintln( "CoDOL zm sumpf: first hut powered" );
	}

	// Pack-a-Punch waits for every hut (the user's rule for this map).
	powered = 0;
	for ( i = 0; i < level.sumpf_huts.size; i++ )
	{
		if ( flag( level.sumpf_huts[ i ].flag_name ) )
			powered++;
	}
	if ( powered == level.sumpf_huts.size )
	{
		level notify( "Pack_A_Punch_on" );
		codolprintln( "CoDOL zm sumpf: all " + powered + " huts powered; Pack-a-Punch on" );
	}
	else
		codolprintln( "CoDOL zm sumpf: " + powered + " of " + level.sumpf_huts.size + " huts powered" );
}

// --------------------------------------------------------------------------
// T5 zombie_cod5_sumpf.gsc book_useage: the book in the NE hut sets off the
// maniac laugh.  book_trig, maniac_l and maniac_r are in the edit, and both
// aliases ship in the port.
// --------------------------------------------------------------------------

sumpf_book_useage()
{
	book_trig = getEnt( "book_trig", "targetname" );
	if ( !isDefined( book_trig ) )
		return;

	book_trig setCursorHint( "HINT_NOICON" );
	book_trig setHintString( "" );
	book_trig useTriggerRequireLookAt();

	maniac_l = getEnt( "maniac_l", "targetname" );
	maniac_r = getEnt( "maniac_r", "targetname" );

	book_trig waittill( "trigger", player );

	if ( isDefined( maniac_l ) )
		maniac_l playSound( "maniac_l" );
	if ( isDefined( maniac_r ) )
		maniac_r playSound( "maniac_r" );
}

// --------------------------------------------------------------------------
// T5 Shi No Numa values.  The ported map entities carry CODOL 3.3.3.3's
// costs (mystery box 1000, hut doors 1000), which differ from T5's own
// zombie_cod5_sumpf entities (MEASURED from Black Ops zombie_cod5_sumpf.ff's
// entity string): every treasure_chest_use 950, the four hut doors
// (script_flag *_building_unlocked) 750.  T5 zombie_cod5_sumpf_trap_perk_electric
// runs the hut electric traps for 25 s (electric_trap_fx) and re-arms them
// after 90 s (cost 1000, unchanged here).  950 also gives the box its
// ZOMBIE_RANDOM_WEAPON_950 hint; 1000 had no box hint and fell back to
// the generic "buy weapon" text.
// --------------------------------------------------------------------------
sumpf_t5_values()
{
	chests = getEntArray( "treasure_chest_use", "targetname" );
	for ( i = 0; i < chests.size; i++ )
		chests[ i ].zombie_cost = 950;

	doors = getEntArray( "zombie_door", "targetname" );
	hut_doors = 0;
	for ( i = 0; i < doors.size; i++ )
	{
		flag = doors[ i ].script_flag;
		if ( isDefined( flag ) && ( flag == "northwest_building_unlocked" || flag == "northeast_building_unlocked" ||
			flag == "southwest_building_unlocked" || flag == "southeast_building_unlocked" ) )
		{
			doors[ i ].zombie_cost = 750;
			hut_doors++;
		}
	}

	level.elec_trap_time = 25;
	level.elec_trap_cooldown_time = 90;

	codolprintln( "CoDOL zm sumpf: T5 values: " + chests.size + " mystery box spot(s) at 950, " + hut_doors +
		" hut door(s) at 750, electric traps 25 s / 90 s cooldown" );
}
