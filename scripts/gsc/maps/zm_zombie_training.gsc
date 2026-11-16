// zm_zombie_training level script: the Zombies training mission.
//
// GScr_LoadScripts asks for exactly one level module per match, maps/<mapname>
// for a zm match (CODOL_ScriptEntryPoints, src/codol/script/codol_script_source.c).  No CoDOL zone ships
// a level script for any zm map (a sweep of all 90 client_shipRetail zones
// found only _fx pairs), so the server owns this file the same way it owns
// mp_killhouse_sh.gsc.  See docs/CODOL_ZOMBIES_STARTUP_20260915.md section 1.
//
// THE MAP IS A SCRIPTED TUTORIAL, NOT A ROUND MAP.  Its MapEnts are a guided
// walk from an elevator to a helicopter, and the map zone carries the
// tutorial's own localize keys (ZOMBIE_TUTORIAL_NEW_WELCOME_01..07,
// _DOOR1_* .. _DOOR6_*, _END_*, _ZAPPER_KILL, _TOKEN_INSTRUCTION; only in
// zm_zombie_training.ff), its one-shot sounds (cbg_tut_elevator_down,
// cbg_tut_energy_door_open, cbg_tut_final_gate_open, cbg_tut_museum_click,
// cbg_tut_claw_up, mus_cyborg_final_run) and the navigation_arrows brushes
// that point from one objective to the next (arrows_to_first .. _twelfth).
// docs/CODOL_ZM_TRAINING_FLOW.md reconstructs the flow from the entities;
// this file runs it.  In short:
//
//   0 lift       spawn in the elevator (mp_tdm_spawn_allies_start), the ride
//                down, the two lift doors slide into their pockets
//                (left/right_door_origin)
//   1 gate       the hallway to zm_main_gate (zombie_door door2_tutorial,
//                1000, points preloaded); first_door_blocker goes with it
//   2 gun        the spas12 wall buy; elec_door_01 sinks (elec_point_01)
//   3 display    one frozen cyborg on the display platform (the
//                start_zone_spawners struct); kill it; zm_door_01
//                (door3_tutorial) into the museum
//   4 museum     the exhibits (button_1..3 / male_vo_1..3), the mp5k wall
//                buy, a weapon-switch drill; elec_door_03 sinks in front of
//                zm_door_02 (door4_tutorial)
//   5 repair     the broken window (exterior_goal_tutorial): repair it, then
//                the window_spawners wave climbs in through it
//   6 power      the debris (north_upstairs_debris), the stairs, elec_door_04,
//                the power switch, Speed Cola; elec_door_02 sinks
//   7 final room find the three windows, three waves from window1..3_wave1..3,
//                then elec_door_fk and zm_door_03 (door5_tutorial)
//   8 exit       the corridor, elec_door_05, the zapper room, zapper_exit,
//                the final door (final_door_up), outside the dome, and the
//                rescue helicopter on the map's route (ending_heli_start ->
//                rescue_dest) through _zombiemode_extraction
//
// Nothing of the vanilla server's tutorial script survives; where the
// entities leave a step open the simplest playable reading is taken and the
// document says so.  The round loop is replaced (level.round_think_func, the
// hook T5 provides for exactly this) so no hound, boss or round spawning runs
// on this map: every zombie here is spawned by a stage, in that stage's own
// area, which is the only place the map's path nodes are (the repair room and
// the final area -- docs/CODOL_ZM_PATH_NODES_20260921.md section 5).
//
// DELIBERATELY NOT HERE:
//  * maps\mp\_load::main() -- the multiplayer level loader; naming it compiles
//    the multiplayer chain into a zm match.
//  * maps\mp\zm_zombie_training_fx::main() -- C1 draws the map's createfx
//    itself (C1.exe 0x42df70); docs/CODOL_CLIENT_AMBIENT_EMITTERS_20260915.md.
//  * setupMiniMap -- mp/basemaps.arena gives the map mapimage "blank".
//  * the instructor NPCs (dumb_talking / dumb_translator / dumb_officer ...)
//    -- script_origins naming an xanim; a script_model has no server animtree
//    here (codol_script_anim.c anim_receiver refuses non-actors).
//  * the instructor voice (cyborg_tutorial_vo_NNN) -- 41 lines whose order
//    against the 67 subtitle keys is not recoverable from the zone.  Only the
//    museum narration (cyborg_tutorial_m_vo_001..003 at male_vo_1..3) is
//    placed, because its three origins name its three lines.

#include maps\_zombiemode_utility;

main()
{
	// See _zombiemode::init() for why this is answered rather than inferred.
	// zm_zombie_training.ff ships no vision rawfile of its own, but C1's
	// startup zone common_mp does (vision/zm_zombie_training.vision).
	maps\_zombiemode::levelMain( "zm_zombie_training" );

	training_precache();

	// No round cap and no round count on this map: the mission is the match.
	level.zm_round_cap = 0;

	// The shared hooks: where a player (re)spawns, the round loop, and the
	// rescue's horde (_zombiemode::getSpawnPoint, round_start,
	// _zombiemode_extraction::extraction_horde).
	level.zm_spawn_point_func = ::training_spawn_point;
	level.round_think_func = ::training_mission;
	level.zm_extraction_horde_func = ::training_extraction_horde;

	level.zt_stage = 0;
	level.zt_stage_name = "lift";
	level.zt_skip = false;

	level thread training_setup();
	level thread training_test_init();
}

// Every localized string the mission sets on a hud element, and the assets
// it plays.  The precache window is the level script's first frame.
training_precache()
{
	precacheShader( "icon_objective_onscreen" );
	precacheShellShock( "electrocution" );

	level._effect[ "zt_zapper" ] = loadfx( "misc/fx_zombie_electric_trap" );
	level._effect[ "zt_energy_door" ] = loadfx( "env/electrical/fx_elec_wire_spark_burst_blue" );
	level._effect[ "zt_lz_light" ] = loadfx( "misc/tower_light_blue_steady" );

	precacheString( &"ZOMBIE_TUTORIAL_NEW_WELCOME_01" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_WELCOME_02" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_WELCOME_03" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_WELCOME_04" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_WELCOME_05" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_WELCOME_06" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_WELCOME_07" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR1_OPEN_IT" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR1_NOT_OPEN" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR1_TURN_BACK" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR1_OPENED" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR1_WALLGUN_INTRO_01" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR1_WALLGUN_INTRO_02" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR1_BOUGHT_GUN" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR1_REBUKE" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR2_1ST_CYBORG" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR2_LMB" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR2_NOT_KILLED_IN_TEN" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR2_KILLED" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR2_NOT_AT_DOOR_IN_TEN" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_A_01" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_A_02" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_A_03" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_B_01" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_B_02" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_C_01" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_C_02" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_GUIDE_01" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_GUIDE_02" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_GUIDE_03" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_WALLGUN" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_TO_DOOR" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_WAIT_LONG" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_BOUGHT_GUN" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_SWITCH_WEAPON" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_SWITCH_SUCCESS" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR3_OPENED" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR4_TIPS" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR4_TO_WINDOW" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR4_REPAIR_WINDOW" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR4_REMINDER" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR4_WARNING" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR4_LAST_KILLED" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR4_PROCEED" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR5_DEBRIS_FOUND" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR5_DEBRIS_NEED_CLEAR" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR5_DEBRIS_CLEARED" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR5_REMINDER" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR5_IN_STAIRS" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR5_NO_ACTION" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR5_LEVER_UP" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR5_SPEED_SODA" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR5_NOT_DRINKING" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR5_DRINKING" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR5_PROCEED" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR5_NOT_MOVING" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR6_ENTERING" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR6_INSTRUCTION" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR6_ENCOURAGE" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR6_KILLING_CYBORG" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR6_ONE_LEFT" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR6_CONGRATS" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_DOOR6_PROCEED" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_OPEN_MAGIC_BOX" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_ZAPPER_KILL" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_TOKEN_INSTRUCTION" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_END_IN_CORRIDOR" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_END_DISTANT" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_END_LEAVING_DOME" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_END_NOT_MOVING_1" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_END_NOT_MOVING_2" );
	precacheString( &"ZOMBIE_TUTORIAL_NEW_END_NOT_MOVING_3" );
}

// --------------------------------------------------------------------------
// Entity setup: everything the mission drives, in its starting state.
// --------------------------------------------------------------------------

training_setup()
{
	// The navigation arrows show one objective at a time.  They are thin
	// decal brushes; none of them should ever be stood on or block a shot.
	level.zt_arrow_names = [];
	names = strTok( "arrows_to_first arrows_to_second arrows_to_fourth arrows_to_fifth arrows_to_sixth arrows_to_seventh arrows_to_eighth arrows_to_power arrows_to_nineth arrows_to_tenth arrows_to_eleventh_1 arrows_to_eleventh_2 arrows_to_eleventh_3 arrows_to_twelfth", " " );
	for ( i = 0; i < names.size; i++ )
	{
		level.zt_arrow_names[ i ] = names[ i ];
		arrows = getEntArray( names[ i ], "targetname" );
		for ( j = 0; j < arrows.size; j++ )
		{
			arrows[ j ] notSolid();
			arrows[ j ] hide();
		}
	}

	// The zapper room's entrance door starts raised (open); its exit and the
	// final door start down (closed) -- as authored.
	training_hud_init();

	{}
}

// --------------------------------------------------------------------------
// Spawning.  _zombiemode::getSpawnPoint asks here first (self = the player).
// --------------------------------------------------------------------------

training_spawn_point()
{
	anchors = training_stage_anchors( level.zt_stage );
	for ( i = 0; i < anchors.size; i++ )
	{
		if ( !positionWouldTelefrag( anchors[ i ].origin ) )
			return anchors[ i ];
	}
	if ( anchors.size > 0 )
		return anchors[ 0 ];
	return undefined;
}

// The places a player may stand at a stage: the lift's two authored points
// (and two more inside the same car) at the start, then the stage's tele
// point dropped to its floor.
training_stage_anchors( stage )
{
	list = [];
	if ( stage <= 0 )
	{
		start = getEntArray( "mp_tdm_spawn_allies_start", "classname" );
		if ( start.size > 0 )
			list[ list.size ] = training_spot( start[ 0 ].origin, start[ 0 ].angles );
		structs = get_struct_array( "initial_spawn_points", "targetname" );
		for ( i = 0; i < structs.size; i++ )
		{
			// Only the one inside the lift; the other three are Prototype's.
			if ( structs[ i ].origin[ 0 ] > 1000 )
				list[ list.size ] = training_spot( structs[ i ].origin, structs[ i ].angles );
		}
		if ( list.size > 0 )
		{
			base = list[ 0 ].origin;
			list[ list.size ] = training_spot( base + ( -30, -10, 0 ), list[ 0 ].angles );
			list[ list.size ] = training_spot( base + ( 10, -40, 0 ), list[ 0 ].angles );
		}
		return list;
	}

	spot = training_stage_spot( stage );
	angles = ( 0, spot[ 3 ], 0 );
	floor = training_floor( ( spot[ 0 ], spot[ 1 ], spot[ 2 ] ) );
	list[ 0 ] = training_spot( floor, angles );
	list[ 1 ] = training_spot( floor + ( 40, 0, 0 ), angles );
	list[ 2 ] = training_spot( floor + ( -40, 0, 0 ), angles );
	list[ 3 ] = training_spot( floor + ( 0, 40, 0 ), angles );
	return list;
}

// A floor point inside each stage's area, near the map's own stage markers
// (script_origins second_tele .. zapper_room_tele, which float 100-300
// above their floors; last_room_tele drops onto a ledge at z 563, so the
// points are taken from playerPhysicsTrace walks of the route instead).
// x, y, a z just above the floor, and the yaw the player faces.
training_stage_spot( stage )
{
	switch ( stage )
	{
	case 1:  return training_vec4( 761, -2212, 260, 135 );
	case 2:  return training_vec4( 120, -1720, 280, 135 );
	case 3:  return training_vec4( 125, -1500, 280, 90 );
	case 4:  return training_vec4( 130, -760, 280, 90 );
	case 5:  return training_vec4( 133, 982, 280, 45 );
	case 6:  return training_vec4( -300, 1300, 280, 135 );
	case 7:  return training_vec4( -25, 2920, 470, 90 );
	case 8:  return training_vec4( -760, 3420, 470, 180 );
	}
	return training_vec4( 1482, -2746, 260, 225 );
}

training_vec4( x, y, z, yaw )
{
	v = [];
	v[ 0 ] = x;
	v[ 1 ] = y;
	v[ 2 ] = z;
	v[ 3 ] = yaw;
	return v;
}

training_spot( origin, angles )
{
	s = spawnStruct();
	s.origin = origin;
	s.angles = angles;
	if ( !isDefined( s.angles ) )
		s.angles = ( 0, 0, 0 );
	return s;
}

// The floor under a point.
training_floor( origin )
{
	trace = bulletTrace( origin, origin - ( 0, 0, 200 ), false, undefined );
	if ( trace[ "fraction" ] < 1 )
		return trace[ "position" ] + ( 0, 0, 2 );
	return origin;
}

// --------------------------------------------------------------------------
// The mission.  Runs as the round loop (round_start threads it once).
// --------------------------------------------------------------------------

training_mission()
{
	level endon( "end_game" );

	maps\_zombiemode::publishRoundHud();
	{}

	training_stage_lift();
	training_stage_gate();
	training_stage_gun();
	training_stage_display();
	training_stage_museum();
	training_stage_repair();
	training_stage_power();
	training_stage_final_room();
	training_stage_exit();
}

training_set_stage( stage, name )
{
	level.zt_stage = stage;
	level.zt_stage_name = name;
	level.zt_skip = false;
	level notify( "zt_stage", stage );
	{}
	training_respawn_dead();
}

// A player who died earlier rejoins at the stage he missed (T5's own rule is
// a respawn between rounds, and this map has no rounds).
training_respawn_dead()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ].sessionstate == "dead" )
			players[ i ] thread maps\_zombiemode::spawnPlayer();
	}
}

// ---- 0: the lift ----------------------------------------------------------

training_stage_lift()
{
	training_set_stage( 0, "lift" );

	lift = getEnt( "lift_sound", "targetname" );
	pos = ( 1430, -2780, 280 );
	if ( isDefined( lift ) )
		pos = lift.origin;

	wait( 1 );
	if ( isDefined( lift ) )
		lift playSound( "cbg_tut_elevator_down" );
	earthquake( 0.12, 6, pos, 500 );
	training_say( &"ZOMBIE_TUTORIAL_NEW_WELCOME_01", 5 );
	wait( 6 );

	training_open_lift();
	training_say( &"ZOMBIE_TUTORIAL_NEW_WELCOME_02", 3 );
}

// The two leaves slide into their pockets: each leaf's brushes move by the
// offset from the leaf's main brush to its pocket marker.
training_open_lift()
{
	training_slide_leaf( "right_lift_door", "right_door_origin" );
	training_slide_leaf( "left_lift_door", "left_door_origin" );

	lift = getEnt( "lift_sound", "targetname" );
	if ( isDefined( lift ) )
		lift playSound( "zmb_door_slide_open" );
	wait( 1.5 );
	{}
}

training_slide_leaf( leaf_name, pocket_name )
{
	pieces = getEntArray( leaf_name, "targetname" );
	pocket = getEnt( pocket_name, "targetname" );
	if ( pieces.size < 1 || !isDefined( pocket ) )
	{
		{}
		return;
	}

	// The main brush is the one with the biggest footprint.
	main = pieces[ 0 ];
	best = 0;
	for ( i = 0; i < pieces.size; i++ )
	{
		size = pieces[ i ] getAbsMaxs() - pieces[ i ] getAbsMins();
		area = size[ 0 ] * size[ 1 ];
		if ( area > best )
		{
			best = area;
			main = pieces[ i ];
		}
	}

	delta = ( pocket.origin[ 0 ] - main.origin[ 0 ], pocket.origin[ 1 ] - main.origin[ 1 ], 0 );
	for ( i = 0; i < pieces.size; i++ )
	{
		pieces[ i ] moveTo( pieces[ i ].origin + delta, 1.5, 0.3, 0.3 );
		pieces[ i ] connectpaths();
	}
}

// ---- 1: the hallway and the gate --------------------------------------------

training_stage_gate()
{
	training_set_stage( 1, "gate" );

	gate = training_door_trigger( "zm_main_gate" );
	training_arrows( "arrows_to_first" );
	if ( isDefined( gate ) )
		training_marker( gate.origin );

	// "This time I've preloaded you with enough."
	training_fund( training_trigger_cost( gate, 1000 ) );
	training_say( &"ZOMBIE_TUTORIAL_NEW_WELCOME_03", 5 );
	training_say( &"ZOMBIE_TUTORIAL_NEW_WELCOME_04", 5 );
	training_say( &"ZOMBIE_TUTORIAL_NEW_WELCOME_05", 6 );
	training_say( &"ZOMBIE_TUTORIAL_NEW_WELCOME_06", 5 );
	training_say( &"ZOMBIE_TUTORIAL_NEW_WELCOME_07", 5 );

	said_open_it = false;
	start = getTime();
	nagged = 0;
	while ( isDefined( gate ) )
	{
		if ( level.zt_skip )
		{
			level.zt_skip = false;
			training_skip_door( gate );
			break;
		}
		if ( !said_open_it && training_player_near( gate.origin, 220 ) )
		{
			said_open_it = true;
			training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR1_OPEN_IT", 4 );
		}
		if ( getTime() - start > 45000 + nagged * 30000 && nagged < 2 )
		{
			if ( nagged == 0 )
				training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR1_TURN_BACK", 4 );
			else
				training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR1_NOT_OPEN", 4 );
			nagged++;
		}
		wait( 0.25 );
	}

	// The gate's own leaves slide apart (_zombiemode_blockers door_activate);
	// the blocker brush standing in the same opening is the tutorial's.
	blocker = getEnt( "first_door_blocker", "targetname" );
	if ( isDefined( blocker ) )
	{
		blocker connectpaths();
		blocker delete();
	}
	{}
}

// ---- 2: the wall gun --------------------------------------------------------

training_stage_gun()
{
	training_set_stage( 2, "gun" );

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR1_OPENED", 4 );
	training_arrows( "arrows_to_second" );

	wall = training_wall_buy( "spas12_zm" );
	weapon = "spas12_zm";
	if ( isDefined( wall ) )
	{
		training_marker( wall.origin );
		training_fund( maps\_zombiemode_weapons::get_weapon_cost( weapon ) );
	}
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR1_WALLGUN_INTRO_01", 5 );

	start = getTime();
	nagged = false;
	while ( !training_any_has_weapon( weapon ) )
	{
		if ( level.zt_skip )
		{
			level.zt_skip = false;
			training_skip_give_weapon( weapon );
			break;
		}
		if ( !nagged && getTime() - start > 25000 )
		{
			nagged = true;
			training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR1_WALLGUN_INTRO_02", 4 );
		}
		wait( 0.25 );
	}

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR1_BOUGHT_GUN", 5 );
	training_energy_door( "elec_door_01", "elec_point_01", "elec_sound" );
}

// ---- 3: the display case ----------------------------------------------------

// The C1 pod is a static cylinder plus three hinged claws, not one skeletal
// animation. Original tutorial 7QjWY7SD8g8, 35.93--36.99, shows the cylinder
// sink and the claws fold outward before the live cyborg firing drill.
// The map's barrier_point lowers the cylinder85 units: its native model top
// goes from343.256 to258.256, level with the257.2 claw pivots. Each claw is
// rooted at its hinge and placed at local +Y around the cylinder; roll -90
// therefore folds it radially outward. Timing is footage-derived reconstruction
// (roughly.5-second sink/1-second fold), pending exact C1 visual comparison.
training_open_display()
{
	level endon( "end_game" );
	if ( is_true( level.zt_display_opened ) || is_true( level.gameEnded ) )
		return;

	base = getEnt( "zombie_barrier", "targetname" );
	point = getEnt( "barrier_point", "targetname" );
	claws = getEntArray( "zombie_clip", "targetname" );
	if ( !isDefined( base ) || !isDefined( point ) || claws.size != 3 )
	{
		{}
		return;
	}

	level.zt_display_opened = true;
	level.zt_display_moving = true;
	level thread training_display_end_cleanup( base, claws );
	playSoundAtPosition( "cbg_tut_claw_up", base.origin );
	base moveTo( point.origin, .5 );
	for ( i = 0; i < claws.size; i++ )
		claws[ i ] rotateRoll( -90, 1 );
	wait( 1 );
	level.zt_display_moving = false;
	level notify( "zt_display_motion_done" );
	{}
}

training_display_end_cleanup( base, claws )
{
	level endon( "zt_display_motion_done" );
	level waittill( "end_game" );
	// These are authored map entities. Freeze an in-flight transition at its
	// received transform; never delete the assembly or spawn replacement props.
	if ( isDefined( base ) )
		base moveTo( base.origin, .05 );
	for ( i = 0; i < claws.size; i++ )
	{
		if ( isDefined( claws[ i ] ) )
			claws[ i ] rotateTo( claws[ i ].angles, .05 );
	}
	level.zt_display_moving = false;
}

training_stage_display()
{
	training_set_stage( 3, "display" );

	training_arrows( undefined );
	sp = undefined;
	structs = get_struct_array( "start_zone_spawners", "targetname" );
	for ( i = 0; i < structs.size; i++ )
	{
		if ( isDefined( structs[ i ].script_noteworthy ) && structs[ i ].script_noteworthy == "zombie_spawner" )
			sp = structs[ i ];
	}

	ai = undefined;
	if ( isDefined( sp ) )
	{
		training_marker( sp.origin + ( 0, 0, 40 ) );
		level.zombie_health = level.zombie_vars[ "zombie_health_start" ];
		ai = maps\_zombiemode_spawner::spawn_zombie( sp );
	}

	if ( isDefined( ai ) )
	{
		// The exhibit stays on its platform: no entrance walk, no chase.
		ai notify( "zombie_delete" );
		ai.ignoreall = true;
		ai.zm_path_phase = "display";
		ai.goalradius = 4;
		ai setGoalPos( ai.origin );
		ai.ignoreme = false;
		{}
	}
	else
		{}

	level thread training_open_display();
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR2_1ST_CYBORG", 5 );

	start = getTime();
	nagged = 0;
	while ( isDefined( ai ) && isAlive( ai ) )
	{
		if ( level.zt_skip )
		{
			level.zt_skip = false;
			ai.no_powerup_drop = true;
			ai doDamage( ai.health + 10, ai.origin );
			break;
		}
		if ( nagged == 0 && getTime() - start > 10000 )
		{
			nagged = 1;
			training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR2_NOT_KILLED_IN_TEN", 4 );
		}
		else if ( nagged == 1 && getTime() - start > 30000 )
		{
			nagged = 2;
			training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR2_LMB", 4 );
		}
		wait( 0.25 );
	}

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR2_KILLED", 5 );

	door = training_door_trigger( "zm_door_01" );
	training_arrows( "arrows_to_fourth" );
	if ( isDefined( door ) )
		training_marker( door.origin );
	training_fund( training_trigger_cost( door, 1000 ) );

	start = getTime();
	nagged = 0;
	while ( isDefined( door ) )
	{
		if ( level.zt_skip )
		{
			level.zt_skip = false;
			training_skip_door( door );
			break;
		}
		if ( nagged == 0 && getTime() - start > 10000 )
		{
			nagged = 1;
			training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR2_NOT_AT_DOOR_IN_TEN", 4 );
		}
		wait( 0.25 );
	}
	{}
}

// ---- 4: the museum ----------------------------------------------------------

training_stage_museum()
{
	training_set_stage( 4, "museum" );

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM", 5 );
	level thread training_museum_exhibits();

	training_arrows( "arrows_to_sixth" );
	training_arrows_add( "arrows_to_fifth" );

	weapon = "mp5k_zm";
	wall = training_wall_buy( weapon );
	if ( isDefined( wall ) )
	{
		training_marker( wall.origin );
		training_fund( maps\_zombiemode_weapons::get_weapon_cost( weapon ) );
	}
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_WALLGUN", 5 );

	door = training_door_trigger( "zm_door_02" );
	start = getTime();
	nagged = false;
	warned_door = false;
	while ( !training_any_has_weapon( weapon ) )
	{
		if ( level.zt_skip )
		{
			level.zt_skip = false;
			training_skip_give_weapon( weapon );
			break;
		}
		if ( !warned_door && isDefined( door ) && training_player_near( door.origin, 120 ) )
		{
			warned_door = true;
			training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_TO_DOOR", 4 );
		}
		if ( !nagged && getTime() - start > 40000 )
		{
			nagged = true;
			training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_WAIT_LONG", 4 );
		}
		wait( 0.25 );
	}

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_BOUGHT_GUN", 4 );

	// The switch drill: two weapon changes, or 20 s.
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_SWITCH_WEAPON", 4 );
	training_wait_weapon_switch( 20 );
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_SWITCH_SUCCESS", 4 );
	training_wait_weapon_switch( 15 );
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_OPENED", 3 );

	// The energy door in front of the museum's far door drops; then the door
	// itself is a purchase (door4_tutorial).
	training_energy_door( "elec_door_03", undefined, "door3_back" );
	training_arrows( "arrows_to_fifth" );
	if ( isDefined( door ) )
		training_marker( door.origin );
	training_fund( training_trigger_cost( door, 1000 ) );

	while ( isDefined( door ) )
	{
		if ( level.zt_skip )
		{
			level.zt_skip = false;
			training_skip_door( door );
			break;
		}
		wait( 0.25 );
	}
	level notify( "zt_museum_done" );
	{}
}

// The three exhibits: walking up to a museum button plays its click, the
// narrator line placed at its male_vo point, and the instructor's notes.
training_museum_exhibits()
{
	level endon( "end_game" );
	level endon( "zt_museum_done" );

	for ( n = 1; n <= 3; n++ )
		level thread training_museum_exhibit( n );
}

training_museum_exhibit( n )
{
	level endon( "end_game" );
	level endon( "zt_museum_done" );

	button = getEnt( "button_" + n, "targetname" );
	voice = getEnt( "male_vo_" + n, "targetname" );
	if ( !isDefined( button ) )
		return;

	while ( !training_player_near( button.origin, 110 ) )
		wait( 0.25 );

	button playSound( "cbg_tut_museum_click" );
	if ( isDefined( voice ) )
		voice playSound( "cyborg_tutorial_m_vo_00" + n );
	{}

	switch ( n )
	{
	case 1:
		training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_GUIDE_01", 6 );
		training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_A_01", 5 );
		training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_A_02", 5 );
		training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_A_03", 5 );
		break;
	case 2:
		training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_GUIDE_02", 6 );
		training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_B_01", 5 );
		training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_B_02", 5 );
		break;
	default:
		training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_GUIDE_03", 7 );
		training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_C_01", 5 );
		training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR3_MUSEUM_C_02", 5 );
		break;
	}
}

training_wait_weapon_switch( seconds )
{
	weapons = [];
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		weapons[ i ] = players[ i ] getCurrentWeapon();

	deadline = getTime() + int( seconds * 1000 );
	while ( getTime() < deadline && !level.zt_skip )
	{
		players = getPlayers();
		for ( i = 0; i < players.size && i < weapons.size; i++ )
		{
			w = players[ i ] getCurrentWeapon();
			if ( isDefined( w ) && w != "none" && isDefined( weapons[ i ] ) && w != weapons[ i ] )
				return true;
		}
		wait( 0.25 );
	}
	level.zt_skip = false;
	return false;
}

// ---- 5: the repair room -----------------------------------------------------

training_stage_repair()
{
	training_set_stage( 5, "repair" );

	window = training_tutorial_window();
	if ( isDefined( window ) && isDefined( window.barrier_chunks ) )
	{
		// The window is broken when the room is reached.
		for ( i = 0; i < window.barrier_chunks.size; i++ )
			maps\_zombiemode_blockers::remove_chunk( window.barrier_chunks[ i ], window, true );
	}

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR4_TIPS", 5 );
	training_arrows( "arrows_to_seventh" );
	near = getEnt( "broken_window", "targetname" );
	spot = ( 483, 1246, 271 );
	if ( isDefined( near ) )
		spot = near.origin;
	training_marker( spot + ( 0, 0, 40 ) );
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR4_TO_WINDOW", 4 );

	while ( !training_player_near( spot, 200 ) && !level.zt_skip )
		wait( 0.25 );
	level.zt_skip = false;

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR4_REPAIR_WINDOW", 6 );
	start = getTime();
	nagged = false;
	while ( isDefined( window ) && isDefined( window.barrier_chunks ) && !all_chunks_intact( window.barrier_chunks ) )
	{
		if ( level.zt_skip )
			break;
		if ( !nagged && getTime() - start > 30000 )
		{
			nagged = true;
			training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR4_REMINDER", 4 );
		}
		wait( 0.25 );
	}
	level.zt_skip = false;

	// The wave: the two window_spawners, two zombies each, through this window.
	training_marker( undefined );
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR4_WARNING", 4 );
	spawners = get_struct_array( "window_spawners", "targetname" );
	zombies = [];
	level.zombie_health = level.zombie_vars[ "zombie_health_start" ];
	for ( pass = 0; pass < 2; pass++ )
	{
		for ( i = 0; i < spawners.size; i++ )
		{
			ai = training_spawn( spawners[ i ] );
			if ( isDefined( ai ) )
				zombies[ zombies.size ] = ai;
			wait( 1 );
		}
		wait( 3 );
	}
	{}

	training_wait_dead( zombies, undefined );
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR4_LAST_KILLED", 4 );
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR4_PROCEED", 4 );
}

// The barrier whose struct carries script_string exterior_goal_tutorial,
// else the one nearest the broken_window marker.
training_tutorial_window()
{
	if ( !isDefined( level.exterior_goals ) )
		return undefined;
	for ( i = 0; i < level.exterior_goals.size; i++ )
	{
		g = level.exterior_goals[ i ];
		if ( isDefined( g.script_string ) && g.script_string == "exterior_goal_tutorial" )
			return g;
	}
	return get_closest( ( 565, 1335, 272 ), level.exterior_goals );
}

// ---- 6: debris, power and the drink -----------------------------------------

training_stage_power()
{
	training_set_stage( 6, "power" );

	debris = undefined;
	triggers = getEntArray( "zombie_debris", "targetname" );
	for ( i = 0; i < triggers.size; i++ )
	{
		if ( isDefined( triggers[ i ].script_flag ) && triggers[ i ].script_flag == "north_upstairs_blocker" )
			debris = triggers[ i ];
	}

	training_arrows( "arrows_to_eighth" );
	if ( isDefined( debris ) )
	{
		training_marker( debris.origin );
		training_fund( training_trigger_cost( debris, 1000 ) );
		while ( !training_player_near( debris.origin, 260 ) && !level.zt_skip )
			wait( 0.25 );
		level.zt_skip = false;
	}
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR5_DEBRIS_FOUND", 4 );
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR5_DEBRIS_NEED_CLEAR", 4 );

	while ( isDefined( debris ) && !flag( "north_upstairs_blocker" ) )
	{
		if ( level.zt_skip )
		{
			level.zt_skip = false;
			training_skip_debris( debris );
			break;
		}
		wait( 0.25 );
	}
	level.zt_skip = false;

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR5_DEBRIS_CLEARED", 5 );
	training_arrows( "arrows_to_power" );
	stairs = getEnt( "stairs_point", "targetname" );
	if ( isDefined( stairs ) )
	{
		training_marker( stairs.origin );
		while ( !training_player_near( stairs.origin, 140 ) && !level.zt_skip )
			wait( 0.25 );
		level.zt_skip = false;
	}

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR5_IN_STAIRS", 4 );
	training_energy_door( "elec_door_04", undefined, undefined );
	training_arrows( "arrows_to_nineth" );
	switch_trig = getEnt( "use_elec_switch", "targetname" );
	if ( isDefined( switch_trig ) )
		training_marker( switch_trig.origin );

	start = getTime();
	nagged = 0;
	while ( !flag( "power_on" ) )
	{
		if ( level.zt_skip )
		{
			level.zt_skip = false;
			flag_set( "power_on" );
			break;
		}
		if ( nagged == 0 && getTime() - start > 20000 )
		{
			nagged = 1;
			training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR5_NO_ACTION", 4 );
		}
		else if ( nagged == 1 && getTime() - start > 45000 )
		{
			nagged = 2;
			training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR5_REMINDER", 4 );
		}
		wait( 0.25 );
	}
	level.zt_skip = false;
	wait( 1 );

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR5_LEVER_UP", 4 );
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR5_SPEED_SODA", 4 );
	training_arrows( "arrows_to_tenth" );
	perk = "specialty_fastreload";
	machines = getEntArray( "zombie_vending", "targetname" );
	for ( i = 0; i < machines.size; i++ )
	{
		if ( isDefined( machines[ i ].script_noteworthy ) && machines[ i ].script_noteworthy == perk )
			training_marker( machines[ i ].origin + ( 0, 0, 20 ) );
	}
	training_fund( maps\_zombiemode_perks::perk_cost( perk, getPlayers().size == 1 ) );

	start = getTime();
	nagged = false;
	while ( !training_any_has_perk( perk ) )
	{
		if ( level.zt_skip )
		{
			level.zt_skip = false;
			players = getPlayers();
			if ( players.size > 0 )
				players[ 0 ] maps\_zombiemode_perks::give_perk( perk );
			break;
		}
		if ( !nagged && getTime() - start > 25000 )
		{
			nagged = true;
			training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR5_NOT_DRINKING", 5 );
		}
		wait( 0.25 );
	}
	level.zt_skip = false;

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR5_DRINKING", 6 );
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR5_PROCEED", 5 );
	training_energy_door( "elec_door_02", "elec_point_02", "elec_point_03_sound" );
}

// ---- 7: the final room ------------------------------------------------------

training_stage_final_room()
{
	training_set_stage( 7, "final room" );

	training_arrows( undefined );
	training_marker( ( -25, 2900, 470 ) );

	start = getTime();
	nagged = false;
	while ( !training_player_in_final_room() && !level.zt_skip )
	{
		if ( !nagged && getTime() - start > 20000 )
		{
			nagged = true;
			training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR5_NOT_MOVING", 4 );
		}
		wait( 0.25 );
	}
	level.zt_skip = false;

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR6_ENTERING", 4 );
	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR6_INSTRUCTION", 5 );

	// The three windows: each arrow group lights while its window is waited on.
	windows = [];
	windows[ 0 ] = "window1";
	windows[ 1 ] = "window4";
	windows[ 2 ] = "window3";
	arrows = [];
	arrows[ 0 ] = "arrows_to_eleventh_1";
	arrows[ 1 ] = "arrows_to_eleventh_2";
	arrows[ 2 ] = "arrows_to_eleventh_3";
	training_arrows( undefined );
	for ( i = 0; i < windows.size; i++ )
	{
		t = getEnt( windows[ i ], "targetname" );
		training_arrows_add( arrows[ i ] );
		if ( !isDefined( t ) )
			continue;
		training_marker( t.origin + ( 0, 0, 40 ) );
		while ( !training_player_near( t.origin, 110 ) && !level.zt_skip )
			wait( 0.25 );
		level.zt_skip = false;
		{}
	}
	training_marker( undefined );

	// Three waves, each from every window's group for that wave.
	for ( wave = 1; wave <= 3; wave++ )
	{
		training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR6_ENCOURAGE", 3 );
		level.zombie_health = level.zombie_vars[ "zombie_health_start" ] + ( wave - 1 ) * level.zombie_vars[ "zombie_health_increase" ];
		level.zombie_move_speed = ( wave - 1 ) * level.zombie_vars[ "zombie_move_speed_multiplier" ] + 1;
		zombies = [];
		for ( w = 1; w <= 3; w++ )
		{
			group = get_struct_array( "window" + w + "_wave" + wave, "targetname" );
			for ( i = 0; i < group.size; i++ )
			{
				ai = training_spawn( group[ i ] );
				if ( isDefined( ai ) )
					zombies[ zombies.size ] = ai;
				wait( 0.5 );
			}
		}
		{}
		training_wait_dead( zombies, wave == 3 );
		training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR6_KILLING_CYBORG", 3 );
		wait( 3 );
	}

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR6_CONGRATS", 5 );

	// The box is there to be tried; it is not a gate.
	chest = getEnt( "treasure_chest_use", "targetname" );
	if ( isDefined( chest ) )
	{
		training_fund( chest maps\_zombiemode_weapons::treasure_chest_cost() );
		training_say( &"ZOMBIE_TUTORIAL_NEW_OPEN_MAGIC_BOX", 5 );
	}

	training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR6_PROCEED", 5 );
	training_energy_door( "elec_door_fk", undefined, "final_door_hint" );
	door = training_door_trigger( "zm_door_03" );
	training_arrows( "arrows_to_twelfth" );
	if ( isDefined( door ) )
		training_marker( door.origin );
	training_fund( training_trigger_cost( door, 1000 ) );

	start = getTime();
	nagged = 0;
	while ( isDefined( door ) )
	{
		if ( level.zt_skip )
		{
			level.zt_skip = false;
			training_skip_door( door );
			break;
		}
		if ( getTime() - start > 30000 + nagged * 30000 && nagged < 2 )
		{
			nagged++;
			training_say( &"ZOMBIE_TUTORIAL_NEW_END_NOT_MOVING_3", 5 );
		}
		wait( 0.25 );
	}
	{}
}

training_player_in_final_room()
{
	// The final_zone info_volume's bounds (-538 2820 415) .. (514 3859 564).
	mins = ( -538, 2820, 415 );
	maxs = ( 514, 3859, 564 );
	volume = getEnt( "final_zone", "targetname" );
	if ( isDefined( volume ) )
	{
		mins = volume getAbsMins();
		maxs = volume getAbsMaxs();
	}
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( !is_player_valid( players[ i ] ) )
			continue;
		o = players[ i ].origin;
		if ( o[ 0 ] >= mins[ 0 ] && o[ 0 ] <= maxs[ 0 ] && o[ 1 ] >= mins[ 1 ] && o[ 1 ] <= maxs[ 1 ] &&
			o[ 2 ] >= mins[ 2 ] - 16 && o[ 2 ] <= maxs[ 2 ] )
			return true;
	}
	return false;
}

// ---- 8: the way out ---------------------------------------------------------

training_stage_exit()
{
	training_set_stage( 8, "exit" );

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] playLocalSound( "mus_cyborg_final_run" );

	training_arrows( undefined );
	guide = getEnt( "guide_point", "targetname" );
	if ( isDefined( guide ) )
	{
		training_marker( guide.origin + ( 0, 0, 40 ) );
		training_wait_near_or_nag( guide.origin, 140 );
	}

	// The zapper room: its energy door drops, the room shocks whoever walks
	// in (the "see what going down feels like" lesson, without the down),
	// and the far door rises.
	training_energy_door( "elec_door_05", undefined, "zapper_door_sound" );
	room = getEnt( "last_stand_point", "targetname" );
	if ( isDefined( room ) )
	{
		training_marker( room.origin + ( 0, 0, 40 ) );
		training_wait_near_or_nag( room.origin, 160 );
	}
	training_say( &"ZOMBIE_TUTORIAL_NEW_ZAPPER_KILL", 6 );
	training_zapper();
	training_say( &"ZOMBIE_TUTORIAL_NEW_TOKEN_INSTRUCTION", 7 );

	training_raise( "zapper_exit", "zapper_up_point", "zapper_door_sound" );

	corridor = getEnt( "inside_vision", "targetname" );
	door_spot = getEnt( "heli_spawn_point", "targetname" );
	if ( isDefined( door_spot ) )
		training_marker( door_spot.origin + ( 0, 0, 40 ) );
	said = false;
	while ( isDefined( door_spot ) && !training_player_near( door_spot.origin, 120 ) && !level.zt_skip )
	{
		if ( !said && isDefined( corridor ) && training_player_touching( corridor ) )
		{
			said = true;
			training_say( &"ZOMBIE_TUTORIAL_NEW_END_IN_CORRIDOR", 4 );
		}
		wait( 0.25 );
	}
	level.zt_skip = false;

	// The final door rises and the dome is left behind.
	// cbg_tut_amb_final_gate_rumble and _final_wind are looping aliases
	// (zone alias flags bit 0); playSound on one is a fatal script error.
	rumble = getEnt( "final_door_trigger", "targetname" );
	if ( isDefined( rumble ) )
	{
		rumble playSound( "cbg_tut_final_gate_open" );
		rumble playLoopSound( "cbg_tut_amb_final_gate_rumble" );
	}
	training_raise( "final_door", "final_door_up", undefined );
	if ( isDefined( rumble ) )
		rumble stopLoopSound();
	{}

	level thread training_leaving_dome();
	level thread training_lz_lights();
	level thread training_award_on_liftoff();

	training_arrows( undefined );
	dest = get_struct( "rescue_dest", "targetname" );
	if ( isDefined( dest ) )
		training_marker( dest.origin + ( 0, 0, 60 ) );

	// The chopper is "waiting up ahead": a short call, not the minute-long
	// one of a survival map.  scr_zm_extract_arrive still wins when set.
	if ( getDvar( "scr_zm_extract_arrive" ) == "" )
		level.zm_extract_arrive = 15;
	level thread training_marker_clear_on_landing();
	maps\_zombiemode_extraction::extraction_start();
}

training_marker_clear_on_landing()
{
	level endon( "end_game" );
	while ( !isDefined( level.zm_lz_waypoint ) )
		wait( 0.25 );
	training_marker( undefined );
}

// No horde follows the trainee out of the dome.
training_extraction_horde()
{
	{}
}

training_leaving_dome()
{
	level endon( "end_game" );
	outside = getEnt( "outside_vision", "targetname" );
	if ( !isDefined( outside ) )
		return;
	while ( !training_player_touching( outside ) )
		wait( 0.25 );
	wind = getEnt( "wind_blow", "targetname" );
	if ( isDefined( wind ) )
		wind playLoopSound( "cbg_tut_amb_final_wind" );
	training_say( &"ZOMBIE_TUTORIAL_NEW_END_LEAVING_DOME", 5 );
	training_say( &"ZOMBIE_TUTORIAL_NEW_END_DISTANT", 4 );
}

// The four blue lights around the landing zone.
training_lz_lights()
{
	structs = get_struct_array( "light_blue_steady_FX_origin", "targetname" );
	for ( i = 0; i < structs.size; i++ )
	{
		fx = spawnFx( level._effect[ "zt_lz_light" ], structs[ i ].origin );
		triggerFx( fx );
	}
}

// CHALLENGE_ZM_ZOMBIE_TRAINING ("complete the zombie training mission") is
// zm_zombie_training_1 in mp/allChallengesTable.csv.
training_award_on_liftoff()
{
	level waittill( "zm_extraction_liftoff" );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( is_true( players[ i ].zm_extracted ) )
			players[ i ] maps\_zombiemode_challenges::zm_map_completed( 1 );
	}
	{}
}

// The zapper room's trap points fire once.
training_zapper()
{
	points = get_struct_array( "fk_trap_fx", "targetname" );
	more = get_struct_array( "pf0_auto407", "targetname" );
	for ( i = 0; i < more.size; i++ )
		points[ points.size ] = more[ i ];

	for ( i = 0; i < points.size; i++ )
		playFx( level._effect[ "zt_zapper" ], points[ i ].origin );
	if ( points.size > 0 )
		playSoundAtPosition( "zmb_elec_start", points[ 0 ].origin );

	trap = getEnt( "zombie_trap", "targetname" );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( !is_player_valid( players[ i ] ) )
			continue;
		if ( isDefined( trap ) && !players[ i ] isTouching( trap ) )
			continue;
		players[ i ] shellShock( "electrocution", 2.5 );
	}
	earthquake( 0.3, 2, ( -965, 3730, 470 ), 400 );
	wait( 3 );
}

// --------------------------------------------------------------------------
// Stage helpers.
// --------------------------------------------------------------------------

// The zombie_door trigger whose target is this door group.
training_door_trigger( door_name )
{
	triggers = getEntArray( "zombie_door", "targetname" );
	for ( i = 0; i < triggers.size; i++ )
	{
		if ( isDefined( triggers[ i ].target ) && triggers[ i ].target == door_name )
			return triggers[ i ];
	}
	{}
	return undefined;
}

training_trigger_cost( trig, def )
{
	if ( isDefined( trig ) && isDefined( trig.zombie_cost ) )
		return int( trig.zombie_cost );
	return def;
}

training_wall_buy( weapon )
{
	walls = getEntArray( "weapon_upgrade", "targetname" );
	best = undefined;
	for ( i = 0; i < walls.size; i++ )
	{
		if ( !isDefined( walls[ i ].zombie_weapon_upgrade ) || walls[ i ].zombie_weapon_upgrade != weapon )
			continue;
		// The tutorial's own wall, on the training floor (Prototype's leftover
		// spas12 sits at z 202 in the yard).
		if ( !isDefined( best ) || walls[ i ].origin[ 2 ] > best.origin[ 2 ] )
			best = walls[ i ];
	}
	return best;
}

// Everyone can afford the step: "this time I've preloaded you with enough".
training_fund( cost )
{
	if ( !isDefined( cost ) || cost <= 0 )
		return;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ].score < cost )
			players[ i ] maps\_zombiemode_score::add_to_player_score( cost - players[ i ].score );
	}
}

training_any_has_weapon( weapon )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] maps\_zombiemode_weapons::has_weapon_or_upgrade( weapon ) )
			return true;
	}
	return false;
}

training_any_has_perk( perk )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] hasPerk( perk ) )
			return true;
	}
	return false;
}

training_player_near( origin, radius )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( !is_player_valid( players[ i ] ) )
			continue;
		if ( distance( players[ i ].origin, origin ) <= radius )
			return true;
	}
	return false;
}

training_player_touching( ent )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( is_player_valid( players[ i ] ) && players[ i ] isTouching( ent ) )
			return true;
	}
	return false;
}

training_wait_near_or_nag( origin, radius )
{
	start = getTime();
	nagged = 0;
	while ( !training_player_near( origin, radius ) && !level.zt_skip )
	{
		if ( getTime() - start > 25000 + nagged * 25000 && nagged < 2 )
		{
			nagged++;
			if ( nagged == 1 )
				training_say( &"ZOMBIE_TUTORIAL_NEW_END_NOT_MOVING_1", 4 );
			else
				training_say( &"ZOMBIE_TUTORIAL_NEW_END_NOT_MOVING_2", 4 );
		}
		wait( 0.25 );
	}
	level.zt_skip = false;
}

// A stage zombie from one of the map's spawner structs; it runs the normal
// zombie_think (entrance walk to its own window, tear, climb, hunt).
training_spawn( sp )
{
	if ( !isDefined( sp ) )
		return undefined;
	ai = maps\_zombiemode_spawner::spawn_zombie( sp );
	if ( !isDefined( ai ) )
	{
		{}
		return undefined;
	}
	return ai;
}

// Until every zombie of the wave is dead.  The skip verb kills them.
training_wait_dead( zombies, announce_last )
{
	said_last = false;
	for ( ;; )
	{
		alive = 0;
		for ( i = 0; i < zombies.size; i++ )
		{
			if ( isDefined( zombies[ i ] ) && isAlive( zombies[ i ] ) )
				alive++;
		}
		if ( alive == 0 )
			break;
		if ( is_true( announce_last ) && !said_last && alive == 1 )
		{
			said_last = true;
			training_say( &"ZOMBIE_TUTORIAL_NEW_DOOR6_ONE_LEFT", 4 );
		}
		if ( level.zt_skip )
		{
			level.zt_skip = false;
			for ( i = 0; i < zombies.size; i++ )
			{
				if ( isDefined( zombies[ i ] ) && isAlive( zombies[ i ] ) )
				{
					zombies[ i ].no_powerup_drop = true;
					zombies[ i ] doDamage( zombies[ i ].health + 10, zombies[ i ].origin );
				}
			}
			level.zt_skip = false;
		}
		wait( 0.5 );
	}
}

// An energy door sinks into the floor: to its authored point when it has one
// (elec_point_01 / _02), else by its own height.
training_energy_door( name, point_name, sound_name )
{
	pieces = getEntArray( name, "targetname" );
	if ( pieces.size < 1 )
	{
		{}
		return;
	}

	top = -100000;
	bottom = 100000;
	for ( i = 0; i < pieces.size; i++ )
	{
		mins = pieces[ i ] getAbsMins();
		maxs = pieces[ i ] getAbsMaxs();
		if ( maxs[ 2 ] > top )
			top = maxs[ 2 ];
		if ( mins[ 2 ] < bottom )
			bottom = mins[ 2 ];
	}

	drop = top - bottom + 4;
	if ( isDefined( point_name ) )
	{
		point = getEnt( point_name, "targetname" );
		if ( isDefined( point ) )
			drop = pieces[ 0 ].origin[ 2 ] - point.origin[ 2 ];
	}

	sound_at = pieces[ 0 ].origin;
	if ( isDefined( sound_name ) )
	{
		s = getEnt( sound_name, "targetname" );
		if ( isDefined( s ) )
			sound_at = s.origin;
	}
	playSoundAtPosition( "cbg_tut_energy_door_open", sound_at );
	playFx( level._effect[ "zt_energy_door" ], sound_at );

	for ( i = 0; i < pieces.size; i++ )
	{
		pieces[ i ] notSolid();
		pieces[ i ] connectpaths();
		pieces[ i ] moveZ( 0 - drop, 1.5, 0.2, 0.2 );
	}
	wait( 1.5 );
	for ( i = 0; i < pieces.size; i++ )
		pieces[ i ] hide();
	{}
}

// A door that rises to its authored "up" point: every entity of the name
// (the doorframe model and its brush) by the same amount.
training_raise( name, up_name, sound_name )
{
	pieces = getEntArray( name, "targetname" );
	up = getEnt( up_name, "targetname" );
	if ( pieces.size < 1 || !isDefined( up ) )
	{
		{}
		return;
	}

	// The model sits at the matching "down" point; its offset to "up" is the
	// travel for every piece.
	model = pieces[ 0 ];
	for ( i = 0; i < pieces.size; i++ )
	{
		if ( pieces[ i ].classname == "script_model" )
			model = pieces[ i ];
	}
	rise = up.origin[ 2 ] - model.origin[ 2 ];

	at = model.origin;
	if ( isDefined( sound_name ) )
	{
		s = getEnt( sound_name, "targetname" );
		if ( isDefined( s ) )
			at = s.origin;
	}
	playSoundAtPosition( "cbg_tut_energy_door_open", at );

	for ( i = 0; i < pieces.size; i++ )
	{
		pieces[ i ] moveZ( rise, 2, 0.4, 0.4 );
		if ( pieces[ i ].classname == "script_brushmodel" )
			pieces[ i ] connectpaths();
	}
	wait( 2 );
	{}
}

// --------------------------------------------------------------------------
// Presentation: the arrows, the objective marker and the instructor's lines.
// --------------------------------------------------------------------------

training_arrows( name )
{
	for ( i = 0; i < level.zt_arrow_names.size; i++ )
	{
		arrows = getEntArray( level.zt_arrow_names[ i ], "targetname" );
		for ( j = 0; j < arrows.size; j++ )
			arrows[ j ] hide();
	}
	if ( isDefined( level.zt_arrow_sound ) )
	{
		level.zt_arrow_sound stopLoopSound();
		level.zt_arrow_sound delete();
		level.zt_arrow_sound = undefined;
	}
	if ( isDefined( name ) )
		training_arrows_add( name );
}

training_arrows_add( name )
{
	arrows = getEntArray( name, "targetname" );
	for ( j = 0; j < arrows.size; j++ )
		arrows[ j ] show();
	if ( arrows.size > 0 && !isDefined( level.zt_arrow_sound ) )
	{
		level.zt_arrow_sound = spawn( "script_origin", arrows[ 0 ].origin );
		level.zt_arrow_sound playLoopSound( "cbg_tut_direct_arrow_lp" );
	}
}

// The blue objective marker ("follow the blue marker").
training_marker( origin )
{
	if ( !isDefined( origin ) )
	{
		if ( isDefined( level.zt_marker ) )
			level.zt_marker.alpha = 0;
		return;
	}
	if ( !isDefined( level.zt_marker ) )
	{
		level.zt_marker = newHudElem();
		level.zt_marker.hidewheninmenu = true;
		level.zt_marker setShader( "icon_objective_onscreen", 8, 8 );
		level.zt_marker setWaypoint( true, true );
	}
	level.zt_marker.x = origin[ 0 ];
	level.zt_marker.y = origin[ 1 ];
	level.zt_marker.z = origin[ 2 ];
	level.zt_marker.alpha = 1;
}

training_hud_init()
{
	level.zt_lines = [];
	level.zt_holds = [];
	level.zt_speaking = false;

	sub = newHudElem();
	sub.alignX = "center";
	sub.alignY = "middle";
	sub.horzAlign = "center";
	sub.vertAlign = "bottom";
	sub.x = 0;
	sub.y = -110;
	sub.fontScale = 1.4;
	sub.foreground = true;
	sub.hidewheninmenu = true;
	sub.alpha = 0;
	level.zt_sub = sub;

	level thread training_subtitle_think();
}

// Lines queue; the mission never waits on them, so a slow reader cannot stall
// a stage and a fast player does not lose a line.
training_say( text, hold )
{
	if ( !isDefined( hold ) )
		hold = 4;
	level.zt_lines[ level.zt_lines.size ] = text;
	level.zt_holds[ level.zt_holds.size ] = hold;
}

training_subtitle_think()
{
	level endon( "end_game" );
	for ( ;; )
	{
		if ( level.zt_lines.size < 1 )
		{
			wait( 0.1 );
			continue;
		}

		text = level.zt_lines[ 0 ];
		hold = level.zt_holds[ 0 ];
		lines = [];
		holds = [];
		for ( i = 1; i < level.zt_lines.size; i++ )
		{
			lines[ lines.size ] = level.zt_lines[ i ];
			holds[ holds.size ] = level.zt_holds[ i ];
		}
		level.zt_lines = lines;
		level.zt_holds = holds;

		level.zt_speaking = true;
		level.zt_sub setText( text );
		level.zt_sub fadeOverTime( 0.3 );
		level.zt_sub.alpha = 1;
		wait( hold );
		level.zt_sub fadeOverTime( 0.3 );
		level.zt_sub.alpha = 0;
		wait( 0.35 );
		level.zt_speaking = false;
	}
}

// --------------------------------------------------------------------------
// Test hooks: the real purchase paths with the payment and the use button
// taken out, so a test client can walk the mission.
// --------------------------------------------------------------------------

training_skip_door( trig )
{
	{}
	trig maps\_zombiemode_blockers::door_opened();
}

training_skip_give_weapon( weapon )
{
	players = getPlayers();
	if ( players.size > 0 )
		players[ 0 ] maps\_zombiemode_weapons::weapon_give( weapon );
	{}
}

// _zombiemode_blockers debris_think's purchase branch, unpaid.
training_skip_debris( trig )
{
	flag_set( trig.script_flag );
	junk = getEntArray( trig.target, "targetname" );
	for ( i = 0; i < junk.size; i++ )
	{
		if ( isDefined( junk[ i ].script_noteworthy ) && junk[ i ].script_noteworthy == "clip" )
		{
			junk[ i ] connectpaths();
			junk[ i ] delete();
			continue;
		}
		s = undefined;
		if ( isDefined( junk[ i ].script_linkTo ) )
			s = get_struct( junk[ i ].script_linkTo, "script_linkname" );
		if ( isDefined( s ) )
			junk[ i ] thread maps\_zombiemode_blockers::debris_move( s );
		else
			junk[ i ] delete();
	}
	trig maps\_zombiemode_blockers::delete_blocker_triggers();
	{}
}

// Developer only (the live server runs developer 0).  scr_zt_test:
//   status                    stage, players, live zombies and their distance
//   skip                      complete the current objective
//   goto <client> [stage]     put the client at a stage's spawn point
//   trace <x> <y> <z> <x> <y> <z>   playerPhysicsTrace between two points
//   walk <x> <y> <z> <x> <y>         a stepping player-hull walk over the floor
training_test_init()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;

	setDvar( "scr_zt_test", "" );
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_zt_test" );
		if ( request == "" )
			continue;
		setDvar( "scr_zt_test", "" );

		tokens = strTok( request, " " );
		switch ( tokens[ 0 ] )
		{
		case "status":
			training_test_status();
			break;

		case "skip":
			level.zt_skip = true;
			{}
			break;

		case "goto":
			p = training_test_player( tokens, 1 );
			stage = level.zt_stage;
			if ( tokens.size > 2 )
				stage = int( tokens[ 2 ] );
			anchors = training_stage_anchors( stage );
			if ( isDefined( p ) && anchors.size > 0 )
			{
				p setOrigin( anchors[ 0 ].origin );
				{}
			}
			break;

		case "walk":
			if ( tokens.size >= 6 )
				training_test_walk( ( int( tokens[ 1 ] ), int( tokens[ 2 ] ), int( tokens[ 3 ] ) ), ( int( tokens[ 4 ] ), int( tokens[ 5 ] ), 0 ) );
			break;

		case "profile":
			if ( tokens.size >= 6 )
				training_test_profile( ( int( tokens[ 1 ] ), int( tokens[ 2 ] ), int( tokens[ 3 ] ) ), ( int( tokens[ 4 ] ), int( tokens[ 5 ] ), 0 ) );
			break;

		case "trace":
			if ( tokens.size >= 7 )
			{
				a = ( int( tokens[ 1 ] ), int( tokens[ 2 ] ), int( tokens[ 3 ] ) );
				b = ( int( tokens[ 4 ] ), int( tokens[ 5 ] ), int( tokens[ 6 ] ) );
				end = playerPhysicsTrace( a, b );
				{}
			}
			break;
		}
	}
}

// A player-hull walk over the floor from a to b (x, y only): 16-unit steps,
// an 18-unit step-up, any drop.  Reports where it stops and why.
training_test_walk( a, b )
{
	cur = training_floor_from( a, a[ 2 ] + 40 );
	dir = vectorNormalize( ( b[ 0 ] - a[ 0 ], b[ 1 ] - a[ 1 ], 0 ) );
	total = distance( ( a[ 0 ], a[ 1 ], 0 ), ( b[ 0 ], b[ 1 ], 0 ) );
	steps = int( total / 16 );
	for ( i = 0; i < steps; i++ )
	{
		// Flat first (a low ceiling would stop the stepped hull), then the
		// 18-unit step.
		next = cur + ( dir[ 0 ] * 16, dir[ 1 ] * 16, 1 );
		end = playerPhysicsTrace( cur + ( 0, 0, 1 ), next );
		if ( distance( end, next ) > 1 )
		{
			next = cur + ( dir[ 0 ] * 16, dir[ 1 ] * 16, 18 );
			end = playerPhysicsTrace( cur + ( 0, 0, 18 ), next );
		}
		if ( distance( end, next ) > 1 )
		{
			{}
			return;
		}
		trace = bulletTrace( next, next - ( 0, 0, 300 ), false, undefined );
		if ( trace[ "fraction" ] >= 1 )
		{
			{}
			return;
		}
		cur = trace[ "position" ] + ( 0, 0, 1 );
	}
	{}
}

// Floor heights every 16 units from a to b, searched from a's z + 60 down.
training_test_profile( a, b )
{
	dir = vectorNormalize( ( b[ 0 ] - a[ 0 ], b[ 1 ] - a[ 1 ], 0 ) );
	total = distance( ( a[ 0 ], a[ 1 ], 0 ), ( b[ 0 ], b[ 1 ], 0 ) );
	line = "CoDOL zt test: profile " + a + " -> " + b + ":";
	for ( d = 0; d <= total; d += 16 )
	{
		p = ( a[ 0 ] + dir[ 0 ] * d, a[ 1 ] + dir[ 1 ] * d, a[ 2 ] + 60 );
		trace = bulletTrace( p, p - ( 0, 0, 300 ), false, undefined );
		if ( trace[ "fraction" ] >= 1 )
			line += " -";
		else
			line += " " + int( trace[ "position" ][ 2 ] );
	}
	{}
}

// The highest floor under (x, y) below z.
training_floor_from( p, z )
{
	trace = bulletTrace( ( p[ 0 ], p[ 1 ], z ), ( p[ 0 ], p[ 1 ], z - 1200 ), false, undefined );
	return trace[ "position" ] + ( 0, 0, 1 );
}

training_test_player( tokens, index )
{
	if ( tokens.size <= index )
		return undefined;
	num = int( tokens[ index ] );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] getEntityNumber() == num )
			return players[ i ];
	}
	return undefined;
}

training_test_status()
{
	players = getPlayers();
	line = "CoDOL zt test: stage " + level.zt_stage + " " + level.zt_stage_name + ", " + players.size + " player(s)";
	for ( i = 0; i < players.size; i++ )
		line += "; " + players[ i ].name + " " + players[ i ].sessionstate + " at " + players[ i ].origin + " hp " + players[ i ].health + " score " + players[ i ].score;
	{}

	if ( !isDefined( level.zombies ) )
		return;
	for ( i = 0; i < level.zombies.size; i++ )
	{
		z = level.zombies[ i ];
		if ( !isDefined( z ) || !isAlive( z ) )
			continue;
		d = -1;
		if ( players.size > 0 )
			d = int( distance( z.origin, players[ 0 ].origin ) );
		phase = "";
		if ( isDefined( z.zm_path_phase ) )
			phase = z.zm_path_phase;
		{}
	}
}
