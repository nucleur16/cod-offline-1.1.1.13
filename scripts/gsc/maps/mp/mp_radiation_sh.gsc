// PROVENANCE (2026-11-13): mp_radiation_sh is CODOL 3.19.26.22's Radiation, ported to 1.1.1.13 by
// CODOL-AssetExplorer port-map. Black Ops (T5) Radiation; this is T5's maps/mp/mp_radiation.gsc
// (COD-GSC-Source/T5-STEAM-GSC) translated to the 1.1.1.13 (IW4) script set. The map logic is kept:
// the catwalk switches that open/close the big tunnel doors (start open, cooldown, crush check),
// the panel exploders, the tunnel lights, the diggers and the conveyor belt.
// Adaptations (T5 -> 1.1.1.13):
//   * art: maps/createart/mp_radiation_sh_art.gsc, T5's setVolFog values as an IW4 setExpFog, plus its light grid dvars.
//   * precache: maps/mp/mp_radiation_sh_precache.gsc (IW4 convention; T5 precaches through its CSC).
//   * fx: maps/mp/mp_radiation_sh_fx.gsc is the zone's own (3.19.26.22); compass_map_mp_radiation_sh.
//   * left out: devgui (/# #/), mp_radiation_amb (every line of T5's is commented out, and it needs
//     maps\mp\_ambientpackage), _teamset_urbanspecops, _spawning::level_use_unified_spawning (T5 only),
//     spawncollision() of the collision_geo_* models (not in the zone, no such builtin here), and the
//     no-turret triggers (T5's level.noTurretPlacementTriggers has no 1.1.1.13 consumer).
//   * #"dvar" hashes -> plain names; set_dvar_int_if_unset, positive_value_func, trigger_thread,
//     isPregame and exploder_stop are defined here (T5 _utility functions this tree lacks).
//   * level.wagerMatch (T5) is false; playloopsound/stoploopsound take no fade times here.
//   * dropAllToGround (T5 physics) -> physicsExplosionSphere-free no-op wait; destroyEquipment detonates
//     the claymores/C4 touching a door through the players' claymoreArray/c4Array.
//   * conveyor belt: coveyer_trig's T5 spawnflags 16 (VEHICLE) is TOUCH_ONCE to this server, which freed the trigger
//     on the first touch; the trigger is parked out of reach and tested per frame instead (conveyer_belt_init).
//   * missing in the zone, so their calls are left out: evt_excavator_* sounds; the green_light /
//     blink_light effects are only played when level._effect defines them (the zone's fx file does not).
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	precachemodel( "tag_origin" );

	// 1.1.1.13: the destructible/animated-model anims the MapEnts' precache_script keys name (IW4 convention)
	maps\mp\mp_radiation_sh_precache::main();
	maps\createart\mp_radiation_sh_art::main();

	//needs to be first for create fx
	maps\mp\mp_radiation_sh_fx::main();

	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_radiation_sh" );

	//Increase sample size so as to resolve lighting issue
	setdvar( "sm_sunSampleSizeNear", ".5" );

	// Set up the default range of the compass
	setdvar( "compassmaxrange", "2100" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	if ( !isDefined( level.wagerMatch ) )
		level.wagerMatch = false;

	flag_init_func();

	level thread level_objects_init();
}

flag_init_func()
{
	flag_init( "kill_stuck_players" );
}

level_objects_init()
{
	level._digger_fx = loadfx( "maps/mp_maps/fx_mp_sand_digger_radiation" );
	waittillframeend;
	level._door_switch_trig1 = getent_and_assert( "switch_trigger1" );
	level._door_switch_trig2 = getent_and_assert( "switch_trigger2" );

	level._door1 = getent_and_assert( "big_door1_clip" );
	level._door2 = getent_and_assert( "big_door2_clip" );

	if ( !isDefined( level._door_switch_trig1 ) || !isDefined( level._door_switch_trig2 ) || !isDefined( level._door1 ) || !isDefined( level._door2 ) )
		return;

	level.const_fx_exploder_switch_green_light = 2001;
	level.const_fx_exploder_switch_red_light = 2002;

	//Gets some diggers digging
	level thread digger_dig_init();

	//Gets diggers in the background moving
	moving_diggers_init();

	//Sets up the switchs players can flip to open the double doors
	level thread door_switch_func();

	if ( !level.wagerMatch && !isPregame() )
	{
		//get my doors opening at the start of the match
		level thread double_doors_open_at_start();
	}

	//Get conveyer belt working
	level thread conveyer_belt_init();
}

// T5: "if ( level.PrematchPeriod > 0 && level.inPrematchPeriod ) level waittill( "prematch_over" )"
wait_prematch_over()
{
	if ( !gameFlag( "prematch_done" ) )
		level waittill( "prematch_over" );
}

isPregame()
{
	return false;
}

exploder_stop( num )
{
	// T5 stops a client exploder; 1.1.1.13's exploders are one-shot createfx entities (server side only in
	// developer createfx), so there is nothing to stop here.
	level notify( "stop_exploder_" + num );
}

turnSwitchPanelRed()
{
	exploder_stop( level.const_fx_exploder_switch_green_light );
	exploder( level.const_fx_exploder_switch_red_light );
}

turnSwitchPanelGreen()
{
	exploder_stop( level.const_fx_exploder_switch_red_light );
	exploder( level.const_fx_exploder_switch_green_light );
}

set_dvar_int_if_unset( dvar, value )
{
	if ( getdvar( dvar ) == "" )
		setdvar( dvar, value );
	return getdvarint( dvar );
}

//Sets it up so a player can flip a switch and open large double doors
door_switch_func()
{
	//Set up the use trigger switches that activate my double doors
	level._door_switch_trig1 thread door_switch_setup();
	level._door_switch_trig2 thread door_switch_setup();

	//Cooldown switches inform the player that the door switch is on cooldown if they try to interact with it
	cooldown_switches = getentarray( "off_trigger", "targetname" );

	for ( i = 0; i < cooldown_switches.size; i++ )
	{
		cooldown_switches[ i ] usetriggerrequirelookat();
		cooldown_switches[ i ] setcursorhint( "HINT_NOICON" );
		cooldown_switches[ i ] sethintstring( &"MP_HOLD_DOOR_SWITCH_UNAVAILABLE" );
		cooldown_switches[ i ] trigger_off();
	}

	turnSwitchPanelRed();

	kill_trig1 = getent_and_assert( "edge_death_trig1" );
	kill_trig2 = getent_and_assert( "edge_death_trig2" );
	center_death_trig = getent_and_assert( "center_death_trig" );

	//Set up DVARS for tweaking door opening sequence
	d1_new_angle = set_dvar_int_if_unset( "scr_d1_new_angle", "123" );
	d2_new_angle = set_dvar_int_if_unset( "scr_d2_new_angle", "-123" );

	door_model1 = getent_and_assert( "big_door1" );
	door_model2 = getent_and_assert( "big_door2" );

	if ( isDefined( door_model1 ) )
		door_model1 linkto( level._door1 );
	if ( isDefined( door_model2 ) )
		door_model2 linkto( level._door2 );

	door_monster_clip = getent( "door_monster_clip", "targetname" );

	//snd_door_status tracks if the door is open or closed
	snd_door_status = 0;

	light_structs = getstructarray( "switch_struct", "targetname" );

	for ( i = 0; i < light_structs.size; i++ )
	{
		light_structs[ i ] thread switch_lights( i );
	}

	tunnel_structs = getstructarray( "tunnel_light_spot", "targetname" );

	for ( i = 0; i < tunnel_structs.size; i++ )
	{
		tunnel_structs[ i ] thread tunnel_lights( i );
	}

	while ( !level.wagerMatch )
	{
		//This is a switch in the level on the catwalk
		waittill_any_ents( level._door_switch_trig1, "trigger", level._door_switch_trig2, "trigger" );

		//Disables the switch temporarily
		level._door_switch_trig1 trigger_off();
		level._door_switch_trig2 trigger_off();

		for ( i = 0; i < cooldown_switches.size; i++ )
		{
			cooldown_switches[ i ] trigger_on();
		}

		door_time = set_dvar_int_if_unset( "scr_d1_time", "8" );

		if ( snd_door_status == 0 )
		{
			d1_accel = door_time * .6;
			d2_accel = door_time * .7;

			d1_decel = door_time * .4;
			d2_decel = door_time * .3;

			//Monster clip is placed and path nodes are severed to keep dogs from running on the doors
			if ( isDefined( door_monster_clip ) )
			{
				door_monster_clip trigger_on();
				door_monster_clip disconnectpaths();
			}
		}
		else
		{
			d1_accel = door_time * .7;
			d2_accel = door_time * .6;

			d1_decel = door_time * .3;
			d2_decel = door_time * .4;
		}

		door_cooldown = set_dvar_int_if_unset( "scr_door_cooldown", "20" );

		level._door1 rotateroll( d1_new_angle, door_time, d1_accel, d1_decel );
		level._door2 rotateroll( d2_new_angle, door_time, d2_accel, d2_decel );

		level thread destroyEquipment();

		// Play door opening sound
		level._door1 playloopsound( "evt_hydraulic_loop" );
		level._door1 playsound( "evt_hydraulic_start" );
		level._door1 thread door_snd_alarm( 5 );

		//This sets it so my doors move in reverse next time they're activated
		d1_new_angle = d1_new_angle * -1;
		d2_new_angle = d1_new_angle * -1;

		flag_set( "kill_stuck_players" );

		//Give time for doors to move a moderate amount before starts kill trigger
		wait 4;

		//Only want to kill players if doors are closing
		if ( snd_door_status == 1 )
		{
			if ( isDefined( kill_trig1 ) )
				kill_trig1 thread kill_edge_players_func();
			if ( isDefined( kill_trig2 ) )
				kill_trig2 thread kill_edge_players_func();
			if ( isDefined( center_death_trig ) )
				center_death_trig thread kill_edge_players_func();
		}
		level._door1 waittill( "rotatedone" );

		if ( snd_door_status == 0 )
		{
			level._door1 stoploopsound();
			if ( isDefined( center_death_trig ) )
				center_death_trig playsound( "evt_hydraulic_open" );
			snd_door_status = 1;
		}
		else
		{
			level._door1 stoploopsound();
			if ( isDefined( center_death_trig ) )
				center_death_trig playsound( "evt_hydraulic_close" );

			//Clip is removed so dogs can path
			if ( isDefined( door_monster_clip ) )
			{
				door_monster_clip connectpaths();
				door_monster_clip trigger_off();
			}
			snd_door_status = 0;
		}

		flag_clear( "kill_stuck_players" );

		//Special notify to end my check for players caught on collision
		level notify( "edge_check" );

		wait( door_cooldown );
		level notify( "no_cooldown" );

		for ( i = 0; i < cooldown_switches.size; i++ )
		{
			cooldown_switches[ i ] trigger_off();
		}

		level._door_switch_trig1 trigger_on();
		level._door_switch_trig2 trigger_on();
	}
}

door_snd_alarm( alarmTimes )
{
	for ( i = 0; i < alarmTimes; i++ )
	{
		wait( .5 );

		playsoundatpos( ( -664, 110, 436 ), "amb_alarm_buzz" );
		playsoundatpos( ( -664, -72, 436 ), "amb_alarm_buzz" );
		playsoundatpos( ( -666, -602, 436 ), "amb_alarm_buzz" );
		playsoundatpos( ( -666, 660, 444 ), "amb_alarm_buzz" );

		wait( 1.5 );
	}
}

//Self is the use triggers that activate the double doors
door_switch_setup()
{
	self usetriggerrequirelookat();
	if ( level.wagerMatch )
	{
		self sethintstring( &"MP_HOLD_DOOR_SWITCH_UNAVAILABLE" );
		turnSwitchPanelRed();
	}
	else
	{
		self sethintstring( &"MP_HOLD_TO_OPERATE_DOORS" );
		turnSwitchPanelGreen();
	}

	while ( !level.wagerMatch )
	{
		self waittill( "trigger", who );

		if ( isDefined( who ) && isPlayer( who ) )
		{
			who playsound( "evt_hydraulic_switch" );
		}

		wait( 1 );
	}
}

//Self is a trigger. The triggers are placed in places i'm afraid of players getting caught
kill_edge_players_func()
{
	level endon( "edge_check" );

	while ( 1 )
	{
		self waittill( "trigger", player );

		if ( isPlayer( player ) && ( ( player istouching( level._door1 ) ) || ( player istouching( level._door2 ) ) ) )
		{
			player dodamage( player.health * 2, self.origin );
		}
		else
		{
			wait .05;
		}
	}
}

//'Self' is the struct within the panel that operates the double doors
switch_lights( element_number )
{
	wait_prematch_over();

	while ( 1 )
	{
		effect_ent = spawn( "script_model", self.origin );
		effect_ent setmodel( "tag_origin" );

		wait .1;

		//Green light signals the switch is not on cooldown
		turnSwitchPanelGreen();

		waittill_any_ents( level._door_switch_trig1, "trigger", level._door_switch_trig2, "trigger" );

		for ( i = 0; i < element_number; i++ )
		{
			wait .1;
		}

		effect_ent delete();

		blinky_effect_ent = spawn( "script_model", self.origin );
		blinky_effect_ent setmodel( "tag_origin" );

		wait .1;

		//Blink red light signals the double doors are moving
		turnSwitchPanelRed();

		level waittill( "edge_check" );

		for ( i = 0; i < element_number; i++ )
		{
			wait .1;
		}

		blinky_effect_ent delete();

		final_effect_ent = spawn( "script_model", self.origin );
		final_effect_ent setmodel( "tag_origin" );

		wait .1;

		level waittill( "no_cooldown" );

		for ( i = 0; i < element_number; i++ )
		{
			wait .1;
		}

		final_effect_ent delete();
	}
}

//'Self' is the struct within the light bulbs in the Radiation tunnel. This function manages FX in the tunnel
tunnel_lights( element_number )
{
	door_closed = true;

	while ( 1 )
	{
		effect_ent = undefined;

		if ( door_closed )
		{
			effect_ent = spawn( "script_model", self.origin );
			effect_ent setmodel( "tag_origin" );

			wait .1;

			if ( isDefined( level._effect[ "green_light" ] ) )
				playfxontag( level._effect[ "green_light" ], effect_ent, "tag_origin" );
		}

		waittill_any_ents( level._door_switch_trig1, "trigger", level._door_switch_trig2, "trigger" );

		for ( i = 0; i < element_number; i++ )
		{
			wait .1;
		}

		new_effect_ent = spawn( "script_model", self.origin );
		new_effect_ent setmodel( "tag_origin" );

		if ( isDefined( effect_ent ) )
		{
			effect_ent delete();
		}

		wait .1;

		if ( isDefined( level._effect[ "blink_light" ] ) )
			playfxontag( level._effect[ "blink_light" ], new_effect_ent, "tag_origin" );

		level waittill( "edge_check" );

		for ( i = 0; i < element_number; i++ )
		{
			wait .1;
		}

		new_effect_ent delete();

		door_closed = !door_closed;
	}
}

//Open doors at start
double_doors_open_at_start()
{
	wait_prematch_over();

	//Slow down my door for other functions to hit waittil's
	wait .3;

	level._door_switch_trig1 notify( "trigger" );
}

//Sets it up so I can handle multiple diggers doing a digging action
digger_dig_init()
{
	diggers = getentarray( "digger_body", "targetname" );
	array_thread( diggers, ::digger_dig_think );
}

//Self is the digger's body entity. Function creates a series of movements for the diggers to create a digging behavior
digger_dig_think()
{
	body = self;
	if ( !isDefined( self.target ) )
		return;
	arm = getent( self.target, "targetname" );
	if ( !isDefined( arm ) || !isDefined( arm.target ) )
		return;
	blade_center = getent( arm.target, "targetname" );
	if ( !isDefined( blade_center ) )
		return;

	blade_pieces = getentarray( "digger_blade", "targetname" );

	for ( i = 0; i < blade_pieces.size; i++ )
	{
		blade_pieces[ i ] linkto( blade_center );
	}

	blade_center linkto( arm );
	arm linkto( body );

	if ( isDefined( self.script_float ) )
	{
		set_dvar_int_if_unset( "scr_dig_delay", self.script_float );
	}
	else
	{
		//'20' is the default value for the cooldown on the digger's dig action
		set_dvar_int_if_unset( "scr_dig_delay", 20 );
	}

	while ( 1 )
	{
		arm_move_speed = set_dvar_int_if_unset( "scr_arm_move_speed", 11 );
		blade_spin_speed = set_dvar_int_if_unset( "scr_blade_spin_speed", 80 );
		blade_spin_up_time = set_dvar_int_if_unset( "scr_blade_spin_up_time", 3 );

		body_turn = randomintrange( -15, 15 );
		body_turn_speed = ( positive_value_func( body_turn ) ) * .3;

		body rotateyaw( body_turn, body_turn_speed, body_turn_speed / 4, body_turn_speed / 4 );

		body waittill( "rotatedone" );

		arm unlink();
		arm rotatepitch( -45, arm_move_speed, arm_move_speed / 4, arm_move_speed / 4 );
		arm waittill( "rotatedone" );

		blade_center unlink();
		blade_center rotatepitch( 1800, blade_spin_speed, blade_spin_up_time, blade_spin_up_time );
		smokeAngles = ( 0, arm.angles[ 1 ] + 180, arm.angles[ 2 ] );

		forward = anglestoforward( smokeAngles );

		playfx( level._digger_fx, ( blade_center.origin[ 0 ], blade_center.origin[ 1 ], blade_center.origin[ 2 ] - 560 ), forward );

		blade_center waittill( "rotatedone" );

		blade_center linkto( arm );

		arm rotatepitch( 45, arm_move_speed, arm_move_speed / 4, arm_move_speed / 4 );

		arm waittill( "rotatedone" );
		arm linkto( body );

		body rotateyaw( ( body_turn * -1 ), body_turn_speed, body_turn_speed / 4, body_turn_speed / 4 );

		body waittill( "rotatedone" );

		wait( getdvarfloat( "scr_dig_delay" ) );
	}
}

//Sets it up so the script can easily handle multiple moving diggers
moving_diggers_init()
{
	diggers = getentarray( "moving_digger", "targetname" );
	array_thread( diggers, ::moving_diggers_think );
}

//Sets up diggers that follow structs. Note the script_int K/V pair sets up the move time.
moving_diggers_think()
{
	if ( !isDefined( self.target ) )
		return;
	digger_struct = getstruct( self.target, "targetname" );

	while ( isDefined( digger_struct ) && isDefined( digger_struct.script_int ) )
	{
		self moveto( digger_struct.origin, digger_struct.script_int );

		self waittill( "movedone" );

		if ( isDefined( digger_struct.target ) )
		{
			digger_struct = getstruct( digger_struct.target, "targetname" );
		}
		else
		{
			break;
		}
	}
}

//Function needed to get the absolute value of an integer and be sure its a positive number
positive_value_func( num )
{
	if ( num < 0 )
		num = num * -1;
	if ( num < 1 )
		num = 1;
	return num;
}

//Alters the velocity of every player standing on the belt, as long as he is in the trigger.
// 1.1.1.13: coveyer_trig carries T5's spawnflags 16 (VEHICLE: vehicles may trigger it). The IW3-derived match
// server reads trigger_multiple spawnflag 16 as TOUCH_ONCE (Touch_Multi -> G_FreeEntityDelay), so the first player
// who stepped on the belt deleted the trigger: T5's trigger_thread then ran on a removed entity and nobody was
// carried. spawnflags is read-only to script, so the trigger is parked out of reach (no player can touch it, so the
// engine never frees it) and moved back onto the belt only for the istouching() tests of one script frame.
conveyer_belt_init()
{
	set_dvar_int_if_unset( "scr_coveyer_speed", 45 );

	conveyer_trigger = getent_and_assert( "coveyer_trig" );
	if ( !isDefined( conveyer_trigger ) || !isDefined( conveyer_trigger.target ) )
		return;

	trigger_struct = getstruct( conveyer_trigger.target, "targetname" );
	if ( !isDefined( trigger_struct ) || !isDefined( trigger_struct.angles ) )
		return;

	trigger_angles = anglestoforward( trigger_struct.angles );

	conveyer_trigger._conveyer_vector = trigger_angles * getdvarint( "scr_coveyer_speed" );

	belt_origin = conveyer_trigger.origin;
	parked_origin = belt_origin - ( 0, 0, 10000 );
	conveyer_trigger.origin = parked_origin;

	while ( 1 )
	{
		conveyer_trigger.origin = belt_origin;
		foreach ( player in level.players )
		{
			if ( !isAlive( player ) || !player istouching( conveyer_trigger ) )
				continue;
			if ( player isonground() )
				player setvelocity( player getvelocity() + conveyer_trigger._conveyer_vector );
		}
		conveyer_trigger.origin = parked_origin;

		wait .05;
	}
}

getent_and_assert( ent_name )
{
	return getent( ent_name, "targetname" );
}

destroyEquipment()
{
	level endon( "edge_check" );

	for ( ;; )
	{
		wait( 2 );

		foreach ( player in level.players )
		{
			lists = [];
			if ( isDefined( player.claymoreArray ) )
				lists[ lists.size ] = player.claymoreArray;
			if ( isDefined( player.c4Array ) )
				lists[ lists.size ] = player.c4Array;

			foreach ( list in lists )
			{
				foreach ( item in list )
				{
					if ( !isDefined( item ) )
						continue;
					if ( !item istouching( level._door1 ) && !item istouching( level._door2 ) )
						continue;
					item detonate();
				}
			}
		}
	}
}
