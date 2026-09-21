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
// NOT TAKEN: clientnotify( "ZPO" ) -- there is no CSC VM on this server (zero
// .csc files in any zone) and clientnotify is not a registered builtin, so the
// client-side power-on effects channel does not exist.  playfx on
// level._effect[...] is also left out: no zm effect table is loaded, and
// loadfx on a name that is in no mounted zone is the kind of thing worth
// proving before shipping.  The lever still moves, the sound still plays and
// every gameplay consumer of flag "power_on" is wired.

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	flag_init( "power_on" );

	add_sound( "switch_flip", "zmb_switch_flip" );
	add_sound( "power_on", "zmb_turn_on" );

	level thread electric_switch();
	level thread wait_for_power();
}

// T5 zombie_theater.gsc:347.
electric_switch()
{
	trigs = getEntArray( "use_elec_switch", "targetname" );

	if ( trigs.size < 1 )
	{
		// A map with no switch has no power gate; opening it immediately is
		// the honest reading, because otherwise every electric door and every
		// perk machine on that map would be unreachable forever.
		println( "CoDOL zm: no use_elec_switch on this map; power is on from the start\n" );
		flag_set( "power_on" );
		return;
	}

	for ( i = 0; i < trigs.size; i++ )
	{
		trigs[ i ] setHintString( &"ZOMBIE_ELECTRIC_SWITCH" );
		trigs[ i ] setCursorHint( "HINT_NOICON" );
	}

	trig = trigs[ 0 ];

	for ( ;; )
	{
		trig waittill( "trigger", user );

		if ( !isDefined( user ) || !is_player_valid( user ) )
			continue;

		if ( !user useButtonPressed() )
			continue;

		break;
	}

	for ( i = 0; i < trigs.size; i++ )
		trigs[ i ] delete();

	flag_set( "power_on" );
}

// T5 zombie_theater.gsc:368.  The lever animation is a RotateRoll, not a
// scripted animation, so it ports as-is.
wait_for_power()
{
	master_switch = undefined;
	switches = getEntArray( "elec_switch", "targetname" );
	if ( switches.size > 0 )
	{
		master_switch = switches[ 0 ];
		master_switch notSolid();
	}

	flag_wait( "power_on" );

	if ( isDefined( master_switch ) )
	{
		master_switch rotateRoll( -90, 0.3 );
		master_switch play_sound_on_ent( "switch_flip" );
		wait( 0.3 );
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

	println( "CoDOL zm: power on\n" );
}

