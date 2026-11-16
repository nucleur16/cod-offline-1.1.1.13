// PROVENANCE (2026-10-22): CoDOL mp_abandon_sh (2.6.9.7 port). The zone ships the carnival
// game entities but not their script, so this recreates it. Behaviour follows IW5
// maps/mp/mp_boardwalk.gsc (COD-GSC-Source IW5-GSC: animal_race / baseball_pitch, which use the
// same "accelerator", "animal_marquee_lights" and "catcher" entities), adapted to this map's
// MapEnts and to the assets the mp_abandon_sh zone ships:
//   * animal race: trigger_damage "accelerator" (bullets only, accumulate_trigger 50) targets
//     animal1..5 (abdn_zodiac_animals_*); each animal targets pf62_autoN, which names both its
//     finish struct and its lane lamp (abdn_light_game). The race runs diagonally here, so the
//     animal moves along start->finish instead of boardwalk's +y.
//   * catcher (pitching game): trigger_damage "catcher" 1..3 between the four light posts
//     catcher_lights_row1..4.
//   * lights: marquee_lights_white_flashing_slow <-> marquee_lights_yellow_flashing_fast and
//     abdn_light_game <-> abdn_light_game_on (all four models ship in the zone).
//   * sounds: the zone's anim_game_start / _move / _move_final / _reset_lp / _return, and
//     target_metal_hit (C1's coop killhouse alias, carried into the zone) for the catcher.
// Left out on purpose: the winner fireworks (createfx exploder 1, firework_winner) and every
// other firework launcher, effect and sound. No %anim is used.
#include common_scripts\utility;

precache()
{
	precacheModel( "marquee_lights_white_flashing_slow" );
	precacheModel( "marquee_lights_yellow_flashing_fast" );
	precacheModel( "abdn_light_game" );
	precacheModel( "abdn_light_game_on" );
}

main()
{
	level thread animal_race();
	level thread catcher_game();
}

// --- animal race -------------------------------------------------------------------------------

animal_race()
{
	level.abandon_animals = [];
	level.abandon_animal_marquee = getentarray( "animal_marquee_lights", "targetname" );

	accelerators = getentarray( "accelerator", "targetname" );
	foreach ( trig in accelerators )
	{
		if ( !isdefined( trig.target ) )
			continue;
		animal = getent( trig.target, "targetname" );
		if ( !isdefined( animal ) || !isdefined( animal.target ) )
			continue;
		goal = getstruct( animal.target, "targetname" );
		if ( !isdefined( goal ) )
			continue;

		animal.race_start = animal.origin;
		animal.race_goal = ( goal.origin[ 0 ], goal.origin[ 1 ], animal.origin[ 2 ] );
		animal.race_lamp = undefined;
		foreach ( ent in getentarray( animal.target, "targetname" ) )
		{
			if ( isdefined( ent.model ) && ent.model == "abdn_light_game" )
				animal.race_lamp = ent;
		}
		// boardwalk: 80 damage notifies to the line; here one step per accumulate_trigger (50) damage
		animal.race_steps = 20;
		trig.race_animal = animal;
		trig.race_accumulate = 50;
		if ( isdefined( trig.accumulate_trigger ) && int( trig.accumulate_trigger ) > 0 )
			trig.race_accumulate = int( trig.accumulate_trigger );
		level.abandon_animals[ level.abandon_animals.size ] = animal;
	}

	codolPrintLn( "CoDOL mp_abandon_sh carnival: animal race with " + level.abandon_animals.size + " animals" );
	if ( !level.abandon_animals.size )
		return;

	level endon( "game_ended" );
	for ( ;; )
	{
		level.abandon_race_started = false;
		foreach ( trig in accelerators )
		{
			if ( isdefined( trig.race_animal ) )
				trig thread accelerator_monitor();
		}
		level waittill( "abandon_race_over", winner );
		animal_race_victory( winner );
	}
}

accelerator_monitor()
{
	level endon( "game_ended" );
	level endon( "abandon_race_over" );

	animal = self.race_animal;
	animal.race_step = 0;
	pending = 0;

	for ( ;; )
	{
		self waittill( "damage", amount, attacker );

		if ( isdefined( attacker ) && isplayer( attacker ) && attacker maps\mp\_utility::isEMPed() )
			continue;

		if ( !isdefined( amount ) )
			amount = self.race_accumulate;
		pending += amount;
		if ( pending < self.race_accumulate )
			continue;
		pending = 0;

		if ( !level.abandon_race_started )
		{
			level.abandon_race_started = true;
			animal playsound( "anim_game_start" );
		}

		animal.race_step++;
		frac = animal.race_step / animal.race_steps;
		if ( frac >= 1 )
		{
			animal moveto( animal.race_goal, 0.25 );
			animal playsound( "anim_game_move_final" );
			animal waittill( "movedone" );
			level notify( "abandon_race_over", animal );
			return;
		}
		animal moveto( animal.race_start + ( animal.race_goal - animal.race_start ) * frac, 0.25 );
		animal playsound( "anim_game_move" );
	}
}

animal_race_victory( winner )
{
	level endon( "game_ended" );

	codolPrintLn( "CoDOL mp_abandon_sh carnival: " + winner.model + " wins the animal race" );
	if ( isdefined( winner.race_lamp ) )
		winner.race_lamp setmodel( "abdn_light_game_on" );
	foreach ( light in level.abandon_animal_marquee )
		light setmodel( "marquee_lights_yellow_flashing_fast" );

	wait 4;

	winner playloopsound( "anim_game_reset_lp" );
	foreach ( animal in level.abandon_animals )
		animal moveto( animal.race_start, 5 );

	wait 5;

	winner stoploopsound();
	winner playsound( "anim_game_return" );
	if ( isdefined( winner.race_lamp ) )
		winner.race_lamp setmodel( "abdn_light_game" );
	foreach ( light in level.abandon_animal_marquee )
		light setmodel( "marquee_lights_white_flashing_slow" );
}

// --- catcher (pitching game) -------------------------------------------------------------------

catcher_game()
{
	rows = [];
	foreach ( light in getentarray( "catcher_lights", "targetname" ) )
	{
		if ( !isdefined( light.script_noteworthy ) )
			continue;
		if ( !isdefined( rows[ light.script_noteworthy ] ) )
			rows[ light.script_noteworthy ] = [];
		rows[ light.script_noteworthy ][ rows[ light.script_noteworthy ].size ] = light;
	}

	catchers = getentarray( "catcher", "targetname" );
	codolPrintLn( "CoDOL mp_abandon_sh carnival: " + catchers.size + " catcher targets" );
	foreach ( trig in catchers )
	{
		n = 1;
		if ( isdefined( trig.script_noteworthy ) )
			n = int( trig.script_noteworthy );
		// target n sits between light posts n and n+1
		trig.catcher_lights = [];
		names = [];
		names[ 0 ] = "catcher_lights_row" + n;
		names[ 1 ] = "catcher_lights_row" + ( n + 1 );
		foreach ( name in names )
		{
			if ( !isdefined( rows[ name ] ) )
				continue;
			foreach ( light in rows[ name ] )
				trig.catcher_lights[ trig.catcher_lights.size ] = light;
		}
		trig thread catcher_monitor();
	}
}

catcher_monitor()
{
	level endon( "game_ended" );
	for ( ;; )
	{
		self waittill( "damage", amount, attacker );

		if ( isdefined( attacker ) && isplayer( attacker ) && attacker maps\mp\_utility::isEMPed() )
			continue;

		playsoundatpos( self.origin, "target_metal_hit" );
		foreach ( light in self.catcher_lights )
			light setmodel( "marquee_lights_yellow_flashing_fast" );

		wait 3;

		foreach ( light in self.catcher_lights )
			light setmodel( "marquee_lights_white_flashing_slow" );
	}
}
