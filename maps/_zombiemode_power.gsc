// zm power switch.
//
// Port of T5's per-level electric_switch() / wait_for_power() pair.  T5 has no
// shared power module: every level script carries its own copy, and they are
// line-for-line the same.  Reference taken from
// D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\zombie_theater.gsc:346-413; the
// copies in zombie_coast.gsc, zombie_cosmodrome.gsc, zombie_moon.gsc,
// zombie_pentagon.gsc and zombie_temple_power.gsc agree.
//
// The entities are authored on CoDOL's zm maps under T5's own names -- MEASURED
// from the zone MapEnts, docs/CODOL_ZOMBIES_STARTUP_20260915.md section 3.6:
// `use_elec_switch` (the trigger), `elec_switch` (the lever brush),
// `elec_switch_fx` / `power_switch_fx` / `power_off_fx` (effect structs),
// `elec_door_01`..`_05`, `elec_point_01`..`_03`, `arrows_to_power`.
//
// VERRUCKT (zm_asylum_sh) is the exception.  MEASURED from the
// zm_asylum_sh.ff MapEnts (2026-10-02): it carries NO use_elec_switch and NO
// elec_switch.  Its switch keeps T5 zombie_cod5_asylum.gsc's own names --
// trigger_use `use_master_switch` at (-302 -225 275), script_model
// `master_switch` (zombie_power_lever_handle_arm02), struct `switch_fx` --
// as T5 master_electric_switch() :1096 reads them.  Looking only for the
// theater names, this module found no switch on Verruckt and set the power
// on at round start: the lever never moved, the trigger never had a prompt,
// and the power room never came alive.  Both name sets are read now.
//
// Verruckt's power-on presentation, from T5 master_electric_switch() and
// the level's client script (clientscripts\zombie_cod5_asylum_amb.csc
// start_lights :74, run on the "start_lights" client system state):
// amb_sparks_l / amb_sparks_r at audio_swtch_left / audio_swtch_right, the
// circuit (zmb_circuit at each electrical_circuit struct, +3 s), the surges
// (evt_electrical_surge plus electric_short_oneshot at each electrical_surge
// struct, +5 s, then a random spark every 4-15 s), and the 2D zmb_turn_on at
// +12 s.  CoDOL has no client script VM, so the server plays them; every
// struct and alias is MEASURED in zm_asylum_sh.ff, and each step only runs
// when the map carries its entities, so the other maps are unchanged.  T5's
// light-model swap (init_lights :377, lights_indlight -> lights_indlight_on
// on every elect_light_model) is taken too.
//
// NOT TAKEN: clientnotify( "ZPO" ) -- there is no CSC VM on this server (zero
// .csc files in any zone) and clientnotify is not a registered builtin, so the
// client-side power-on effects channel does not exist.  Verruckt's
// exploder( 101 ) (the 20 createfx light glows that come on with the power)
// is the client's own createfx and stays unlit: the server has no exploder
// channel to C1 and no way to read the zone's createfx rawfile.  The
// announcer laugh (zmb_ann_vox_laugh_l) is not in any CoDOL zone.

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	flag_init( "power_on" );

	add_sound( "switch_flip", "zmb_switch_flip" );
	add_sound( "power_on", "zmb_turn_on" );

	// Both MEASURED in common_gametype_zm (docs/zone_assets).  T5 asylum
	// "switch_sparks" (zombie_cod5_asylum_fx.gsc:101) is
	// env/electrical/fx_elec_wire_spark_burst.
	level._effect[ "switch_sparks" ] = loadfx( "env/electrical/fx_elec_wire_spark_burst" );
	level._effect[ "electric_short_oneshot" ] = loadfx( "env/electrical/fx_elec_short_oneshot" );

	// T5 asylum init_lights :377: the lamp models swap with the power.
	level.zm_power_lights = [];
	lamps = getEntArray( "elect_light_model", "targetname" );
	for ( i = 0; i < lamps.size; i++ )
	{
		if ( isDefined( lamps[ i ].model ) && isSubStr( lamps[ i ].model, "indlight" ) )
			level.zm_power_lights[ level.zm_power_lights.size ] = lamps[ i ];
	}
	if ( level.zm_power_lights.size > 0 )
	{
		precacheModel( "lights_indlight" );
		precacheModel( "lights_indlight_on" );
	}

	level thread electric_switch();
	level thread wait_for_power();
	level thread power_lights();
}

// The switch trigger: T5 theater's name, else T5 asylum's.
power_switch_triggers()
{
	trigs = getEntArray( "use_elec_switch", "targetname" );
	if ( trigs.size < 1 )
		trigs = getEntArray( "use_master_switch", "targetname" );
	return trigs;
}

// The lever model: T5 theater's name, else T5 asylum's.
power_switch_lever()
{
	switches = getEntArray( "elec_switch", "targetname" );
	if ( switches.size < 1 )
		switches = getEntArray( "master_switch", "targetname" );
	if ( switches.size < 1 )
		return undefined;
	return switches[ 0 ];
}

// T5 zombie_theater.gsc:347.
electric_switch()
{
	trigs = power_switch_triggers();

	if ( trigs.size < 1 )
	{
		// A map with no switch has no power gate; opening it immediately is
		// the honest reading, because otherwise every electric door and every
		// perk machine on that map would be unreachable forever.
		codolprintln( "CoDOL zm power: no power switch on this map; power is on from the start" );
		flag_set( "power_on" );
		return;
	}

	for ( i = 0; i < trigs.size; i++ )
	{
		trigs[ i ] setHintString( &"ZOMBIE_ELECTRIC_SWITCH" );
		trigs[ i ] setCursorHint( "HINT_NOICON" );
	}

	trig = trigs[ 0 ];
	codolprintln( "CoDOL zm power: switch " + trig.targetname + " ent " + trig getEntityNumber() + " at " + trig.origin + " waiting for a player" );

	user = undefined;
	for ( ;; )
	{
		trig waittill( "trigger", user );

		if ( !isDefined( user ) || !is_player_valid( user ) )
			continue;

		if ( !user useButtonPressed() )
			continue;

		break;
	}

	codolprintln( "CoDOL zm power: switch thrown by " + user.name );

	for ( i = 0; i < trigs.size; i++ )
		trigs[ i ] delete();

	flag_set( "power_on" );
}

// T5 zombie_theater.gsc:368.  The lever animation is a RotateRoll, not a
// scripted animation, so it ports as-is.
wait_for_power()
{
	master_switch = power_switch_lever();
	if ( isDefined( master_switch ) )
		master_switch notSolid();

	flag_wait( "power_on" );

	// T5 asylum :1158 "switch_flipped".
	level notify( "switch_flipped" );

	asylum = get_struct_array( "electrical_surge", "targetname" ).size > 0;
	if ( asylum )
		level thread asylum_power_on_presentation();

	if ( isDefined( master_switch ) )
	{
		master_switch rotateRoll( -90, 0.3 );
		master_switch play_sound_on_ent( "switch_flip" );
		wait( 0.3 );

		// T5 theater :382 / asylum :1198: sparks at the switch once it is down.
		sparks = get_struct( "elec_switch_fx", "targetname" );
		if ( !isDefined( sparks ) )
			sparks = get_struct( "switch_fx", "targetname" );
		if ( isDefined( sparks ) )
			playFx( level._effect[ "switch_sparks" ], sparks.origin );

		// Verruckt plays this 2D at +12 s instead (asylum_power_on_presentation).
		if ( !asylum )
			master_switch play_sound_on_ent( "power_on" );
	}

	wait_network_frame();

	// T5 fans the power out to the perk machines by notify, one network frame
	// apart so the machines' own state does not all land in one snapshot.
	// maps\mp\gametypes\_zombiemode_perks::vending_trigger_think waits on
	// <perk>_power_on by exactly these names.
	names = [];
	names[ 0 ] = "specialty_quickrevive_power_on";
	names[ 1 ] = "specialty_armorvest_power_on";
	names[ 2 ] = "specialty_fastreload_power_on";
	names[ 3 ] = "specialty_rof_power_on";
	names[ 4 ] = "specialty_longersprint_power_on";
	names[ 5 ] = "specialty_deadshot_power_on";
	names[ 6 ] = "Pack_A_Punch_on";

	for ( i = 0; i < names.size; i++ )
	{
		level notify( names[ i ] );
		wait_network_frame();
	}

	// T5 asylum :1219 "master_switch_activated".
	level notify( "master_switch_activated" );

	codolprintln( "CoDOL zm power: power on; perk machines notified" );
}

// T5 asylum init_lights :377.  The indicator lamps read off until the power
// comes on.
power_lights()
{
	lamps = level.zm_power_lights;
	for ( i = 0; i < lamps.size; i++ )
	{
		if ( isDefined( lamps[ i ] ) )
			lamps[ i ] setModel( "lights_indlight" );
	}

	flag_wait( "power_on" );

	for ( i = 0; i < lamps.size; i++ )
	{
		wait_network_frame();
		if ( isDefined( lamps[ i ] ) )
			lamps[ i ] setModel( "lights_indlight_on" );
	}
}

// T5 asylum master_electric_switch :1159 and
// clientscripts\zombie_cod5_asylum_amb.csc start_lights :74.
asylum_power_on_presentation()
{
	left = getEnt( "audio_swtch_left", "targetname" );
	right = getEnt( "audio_swtch_right", "targetname" );

	if ( isDefined( left ) )
		left playSound( "amb_sparks_l" );
	if ( isDefined( right ) )
		right playSound( "amb_sparks_r" );

	// start_lights: 2 s, then the circuit.
	wait( 2 );
	circuits = get_struct_array( "electrical_circuit", "targetname" );
	for ( i = 0; i < circuits.size; i++ )
		level thread asylum_circuit_sound( circuits[ i ].origin );

	// 3 s more, then the surges along the light runs.
	wait( 3 );
	surges = get_struct_array( "electrical_surge", "targetname" );
	for ( i = 0; i < surges.size; i++ )
		level thread asylum_surge( surges[ i ].origin );

	// 7 s more: the 2D "power on" sting for everyone.
	wait( 7 );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] playLocalSound( "zmb_turn_on" );
	codolprintln( "CoDOL zm power: Verruckt power-on sting played to " + players.size + " players" );

	// T5 asylum :1227: the switch sparks wind down 14 s after the flip.
	wait( 2 );
	if ( isDefined( left ) )
		playSoundAtPosition( "amb_sparks_l_end", left.origin );
	if ( isDefined( right ) )
		playSoundAtPosition( "amb_sparks_r_end", right.origin );
}

// zombie_cod5_asylum_amb.csc circuit_sound :127.
asylum_circuit_sound( origin )
{
	wait( 1 );
	playSoundAtPosition( "zmb_circuit", origin );
}

// zombie_cod5_asylum_amb.csc light_sound :97 / run_sparks_loop :112: one
// surge as the lights catch, then a random spark for the rest of the match.
asylum_surge( origin )
{
	wait( randomFloatRange( 1, 4 ) );
	playSoundAtPosition( "evt_electrical_surge", origin );
	playFx( level._effect[ "electric_short_oneshot" ], origin );

	for ( ;; )
	{
		wait( randomFloatRange( 4, 15 ) );
		if ( is_true( level.intermission ) )
			return;
		if ( randomFloatRange( 0, 1 ) < 0.5 )
		{
			playFx( level._effect[ "electric_short_oneshot" ], origin );
			playSoundAtPosition( "evt_electrical_surge", origin );
		}
		wait( randomIntRange( 1, 4 ) );
	}
}
