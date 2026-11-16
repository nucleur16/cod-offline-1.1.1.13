// PROTOTYPE ONLY: Kino risers on the frozen 818f script package / 4c0e native.
// Port/adaptation of Treyarch T5 maps/_zombiemode_spawner.gsc:3676-3933,
// _zombiemode.gsc rise tables, and _zombiemode_zone_manager rise ownership.
// Original installed BO1 spawner SHA256:
// 91cbd43817b378dc2fc122ba50ea12c2afcaba6ca99d7d2131b05eccb1605f9a
// C1: actual actor animScripted("noclip") consumes existing real XYZ root
// deltas; default "normal" does not. No actor/model proxy, fake clip or lift.
// Only Theater's authored riser structs qualify. Other spawners stay 818f.

#include maps\_zombiemode_utility;
#include animscripts\zombie_shared;

#using_animtree ("generic_human");

init()
{
	if ( getDvar( "mapname" ) != "zm_theater_sh" )
		return;
	level.kino_rise_anims = [];
	level.kino_rise_anims[ 1 ] = [];
	level.kino_rise_anims[ 1 ][ "walk" ] = %ai_zombie_traverse_ground_v1_walk;
	level.kino_rise_anims[ 1 ][ "run" ] = %ai_zombie_traverse_ground_v1_run;
	level.kino_rise_anims[ 1 ][ "sprint" ] = %ai_zombie_traverse_ground_climbout_fast;
	level.kino_rise_anims[ 2 ] = [];
	level.kino_rise_anims[ 2 ][ "walk" ] = %ai_zombie_traverse_ground_v2_walk_altA;
	level.kino_rise_deaths = [];
	level.kino_rise_deaths[ 1 ] = [];
	level.kino_rise_deaths[ 1 ][ "in" ] = [];
	level.kino_rise_deaths[ 1 ][ "in" ][ 0 ] = %ai_zombie_traverse_ground_v1_deathinside;
	level.kino_rise_deaths[ 1 ][ "in" ][ 1 ] = %ai_zombie_traverse_ground_v1_deathinside_alt;
	level.kino_rise_deaths[ 1 ][ "out" ] = [];
	level.kino_rise_deaths[ 1 ][ "out" ][ 0 ] = %ai_zombie_traverse_ground_v1_deathoutside;
	level.kino_rise_deaths[ 1 ][ "out" ][ 1 ] = %ai_zombie_traverse_ground_v1_deathoutside_alt;
	level.kino_rise_deaths[ 2 ] = [];
	level.kino_rise_deaths[ 2 ][ "in" ] = [];
	level.kino_rise_deaths[ 2 ][ "in" ][ 0 ] = %ai_zombie_traverse_ground_v2_death_low;
	level.kino_rise_deaths[ 2 ][ "in" ][ 1 ] = %ai_zombie_traverse_ground_v2_death_low_alt;
	level.kino_rise_deaths[ 2 ][ "out" ] = [];
	level.kino_rise_deaths[ 2 ][ "out" ][ 0 ] = %ai_zombie_traverse_ground_v2_death_high;
	level.kino_rise_deaths[ 2 ][ "out" ][ 1 ] = %ai_zombie_traverse_ground_v2_death_high_alt;
	level._effect[ "kino_rise_burst" ] = loadfx( "maps/zombie/fx_mp_zombie_hand_dirt_burst" );
	level._effect[ "kino_rise_billow" ] = loadfx( "maps/zombie/fx_mp_zombie_body_dirt_billowing" );
	level._effect[ "kino_rise_dust" ] = loadfx( "maps/zombie/fx_mp_zombie_body_dust_falling" );
}

is_riser( spawner )
{
	return getDvar( "mapname" ) == "zm_theater_sh" && isDefined( spawner ) &&
		isDefined( spawner.script_string ) && spawner.script_string == "riser";
}

// Before creating an actor or consuming the round budget: use only this
// spawner's enabled zone and its actual active enemy_spawns membership.
// This intentionally has no global fallback into a locked room.
prepare( spawner )
{
	if ( !is_riser( spawner ) || !isDefined( level.zm_spawn_zones ) ||
		!isDefined( level.enemy_spawns ) )
		return undefined;
	active = false;
	for ( i = 0; i < level.enemy_spawns.size; i++ )
	{
		if ( level.enemy_spawns[ i ] == spawner )
			active = true;
	}
	if ( !active )
		return undefined;
	for ( i = 0; i < level.zm_spawn_zones.size; i++ )
	{
		zone = level.zm_spawn_zones[ i ];
		if ( !zone.enabled || !isDefined( spawner.targetname ) ||
			zone.spawner_target != spawner.targetname )
			continue;
		owned = false;
		for ( j = 0; j < zone.spawners.size; j++ )
		{
			if ( zone.spawners[ j ] == spawner )
				owned = true;
		}
		if ( !owned )
			continue;
		spots = get_struct_array( zone.spawner_target + "_rise", "targetname" );
		valid = [];
		for ( j = 0; isDefined( spots ) && j < spots.size; j++ )
		{
			spot = spots[ j ];
			if ( !isDefined( spot.origin ) || ( isDefined( spot.enabled ) && !spot.enabled ) )
				continue;
			// Same real downward world trace used by the zone spawner gate.
			// Actual actor capsule placement is additionally checked in begin.
			floor = bulletTrace( spot.origin + ( 0, 0, 16 ), spot.origin - ( 0, 0, 256 ), false, undefined );
			if ( floor[ "fraction" ] >= 1 )
				continue;
			valid[ valid.size ] = spot;
		}
		if ( valid.size < 1 )
			return undefined;
		context = spawnStruct();
		context.zone = zone;
		context.locations = valid;
		context.spot = valid[ randomInt( valid.size ) ];
		return context;
	}
	return undefined;
}

// Called before the actor is admitted to level.zombies and before any wait.
// The same real actor is placed at the authored surface, then C1's existing
// noclip alignment starts it at T5's buried origin. Never teleport after rise.
begin( context )
{
	self hide();
	spot = context.spot;
	angles = ( 0, 0, 0 );
	if ( isDefined( spot.angles ) )
		angles = spot.angles;
	self forceTeleport( spot.origin, angles );
	self.zombie_rise_version = randomInt( 2 ) + 1;
	if ( self.zombie_move_speed != "walk" )
		self.zombie_rise_version = 1;
	depth = 45;
	if ( self.zombie_rise_version == 2 )
		depth = 14;
	start = spot.origin - ( 0, 0, depth );
	delta = self.origin - start;
	// Exact native alignment contract: <=64 XY and <=64 Z. Reject physics
	// relocation beyond it rather than animating at the old spawner/floor.
	if ( delta[ 0 ] * delta[ 0 ] + delta[ 1 ] * delta[ 1 ] > 64 * 64 ||
		delta[ 2 ] > 64 || delta[ 2 ] < -64 )
		return false;
	clip = level.kino_rise_anims[ self.zombie_rise_version ][ self.zombie_move_speed ];
	if ( !isDefined( clip ) || getAnimLength( clip ) <= 0 )
		return false;
	self.kino_rise_context = context;
	self.kino_rise_clip = clip;
	self.kino_rise_completed = false;
	self.kino_rise_end_note = undefined;
	self.zombie_rise_death_out = false;
	self.in_the_ground = true;
	self.zm_path_phase = "rise";
	self.kino_rise_previous_death_function = self.deathFunction;
	self.deathFunction = ::rise_death;
	self AnimScripted( "rise", start, angles, clip, "noclip" );
	// Native mode5 alignment is immediate. Enforce that actual start, not a
	// timer that pretends to rise if the actor failed to enter scripted state.
	if ( distanceSquared( self.origin, start ) > 0.01 )
	{
		self StopAnimScripted();
		return false;
	}
	self thread hide_pop();
	self thread rise_fx();
	self thread cleanup_death();
	self thread cleanup_delete();
	level thread cleanup_match( self, "end_game" );
	level thread cleanup_match( self, "intermission" );
	self thread completion_guard( getAnimLength( clip ) + 1 );
	return true;
}

// T5's wait-for-risen is synchronous here: the thinker itself owns this
// genuine notetrack pump, and receives the selected struct's actual value.
run()
{
	self endon( "death" );
	self endon( "zombie_delete" );
	level endon( "end_game" );
	level endon( "intermission" );
	note = self DoNoteTracks( "rise", ::rise_note, self.kino_rise_context.spot );
	if ( note != "end" && note != "finish" )
	{
		self fail_rise( "rise ended without a genuine completion note" );
		return undefined;
	}
	self.kino_rise_end_note = note;
	// G_RunFrame runs entity movement before the DObj update that delivers
	// this end note. Two consecutive VM yields guarantee an intervening entity
	// phase even at default 20 Hz when the first resume precedes actor movement.
	// Keep mode5 for that phase's final XYZ delta. These waits synchronize
	// AFTER genuine completion; neither chooses or repeats the rise duration.
	wait_network_frame();
	wait_network_frame();
	self.kino_rise_completed = true;
	self notify( "rise_anim_finished" );
	self cleanup();
	self StopAnimScripted();
	self.deathFunction = self.kino_rise_previous_death_function;
	self.in_the_ground = false;
	result = self.kino_rise_context.spot.script_noteworthy;
	self notify( "risen", result );
	return result;
}

rise_note( note, spot )
{
	if ( note == "deathout" || note == "deathhigh" )
	{
		self.zombie_rise_death_out = true;
		self.kino_rise_phase_note = note;
		self notify( "zombie_rise_death_out" );
		// T5 stops the effect two seconds later. Keep the pump listening so
		// a native completion note cannot be lost during that delay.
		self thread stop_fx_after_phase();
	}
}

stop_fx_after_phase()
{
	self endon( "death" );
	self endon( "zombie_delete" );
	self endon( "rise_anim_finished" );
	wait 2;
	self notify( "kino_rise_stop_fx" );
}

// C1 can deliver lethal damage after death; choose before zombie_death's
// deathanim read through its existing hook rather than racing a damage waiter.
rise_death()
{
	if ( !self.kino_rise_completed )
	{
		phase = "in";
		if ( self.zombie_rise_death_out )
			phase = "out";
		clips = level.kino_rise_deaths[ self.zombie_rise_version ][ phase ];
		self.deathanim = clips[ randomInt( clips.size ) ];
	}
	self cleanup();
	self.in_the_ground = false;
	if ( isDefined( self.kino_rise_previous_death_function ) )
		return self [[ self.kino_rise_previous_death_function ]]();
	// Default path only. C1 clears mode on entry to zombie_death; its
	// stationary mode1 does not consume this genuine death clip's XYZ delta.
	// animMode is an existing setter valid on a dying native actor. Subsequent
	// same-function death ticks preserve this override. A previous death hook
	// owns its result/clip/mode/tail even when false: no getter can establish
	// whether it deliberately changed mode, so that path above is untouched.
	if ( !self.kino_rise_completed && !is_true( self.a.nodeath ) )
	{
		self.kino_rise_death_motion_clip = self.deathanim;
		self.kino_rise_previous_death_tail = self.deathanimscript;
		self.deathanimscript = ::rise_death_tail;
		self animMode( "noclip" );
	}
	return false;
}

// Called by the existing zombie_death only after its real notetrack pump
// returns. Two consecutive VM yields keep this death thread alive across an
// intervening entity phase even at 20 Hz, so the final XYZ is consumed before
// native corpse conversion. Then restore its original tail callback.
// No death clip, note, ragdoll event or duration is synthesized here.
rise_death_tail()
{
	if ( self.deathanim == self.kino_rise_death_motion_clip )
	{
		wait_network_frame();
		wait_network_frame();
		self animMode( "none" );
	}
	self.deathanimscript = self.kino_rise_previous_death_tail;
	if ( isDefined( self.kino_rise_previous_death_tail ) )
		self [[ self.kino_rise_previous_death_tail ]]();
}

hide_pop()
{
	self endon( "death" );
	self endon( "zombie_delete" );
	level endon( "end_game" );
	level endon( "intermission" );
	wait 0.5;
	self show();
}

rise_fx()
{
	self endon( "death" );
	self endon( "zombie_delete" );
	self endon( "kino_rise_stop_fx" );
	level endon( "end_game" );
	level endon( "intermission" );
	self thread rise_dust();
	spot = self.kino_rise_context.spot;
	playSoundAtPosition( "zmb_zombie_spawn", spot.origin );
	playfx( level._effect[ "kino_rise_burst" ], spot.origin + ( 0, 0, randomIntRange( 5, 10 ) ) );
	wait 0.25;
	playfx( level._effect[ "kino_rise_billow" ], spot.origin +
		( randomIntRange( -10, 10 ), randomIntRange( -10, 10 ), randomIntRange( 5, 10 ) ) );
}

rise_dust()
{
	self endon( "death" );
	self endon( "zombie_delete" );
	self endon( "kino_rise_stop_fx" );
	level endon( "end_game" );
	level endon( "intermission" );
	for ( t = 0; t < 7.5; t += 0.1 )
	{
		playFxOnTag( level._effect[ "kino_rise_dust" ], self, "J_SpineUpper" );
		wait 0.1;
	}
}

cleanup()
{
	self notify( "kino_rise_stop_fx" );
	stopFxOnTag( level._effect[ "kino_rise_dust" ], self, "J_SpineUpper" );
	self show();
}

cleanup_death()
{
	self endon( "zombie_delete" );
	self endon( "rise_anim_finished" );
	self waittill( "death" );
	self cleanup();
	// Native's first dead frame clears scripted state before entering the
	// death animscript. Do not race it by stopping animation from this waiter.
}

cleanup_delete()
{
	self endon( "death" );
	self endon( "rise_anim_finished" );
	self waittill( "zombie_delete" );
	self cleanup();
	// G_FreeEntity owns native actor/script termination on actual deletion.
}

cleanup_match( zombie, event )
{
	zombie endon( "death" );
	zombie endon( "zombie_delete" );
	zombie endon( "rise_anim_finished" );
	zombie endon( "kino_rise_terminal" );
	level waittill( event );
	if ( isDefined( zombie ) )
	{
		zombie cleanup();
		if ( isAlive( zombie ) )
			zombie StopAnimScripted();
		zombie notify( "kino_rise_terminal" );
	}
}

completion_guard( maximum )
{
	self endon( "death" );
	self endon( "zombie_delete" );
	self endon( "rise_anim_finished" );
	level endon( "end_game" );
	level endon( "intermission" );
	wait maximum;
	self fail_rise( "genuine rise completion note timed out" );
}

fail_rise( reason )
{
	if ( !isAlive( self ) )
		return;
	self.kino_rise_failure = reason;
	self.zm_path_phase = "rise_failed";
	println( "Kino riser rejected: " + reason );
	self cleanup();
	self StopAnimScripted();
	self.no_powerup_drop = true;
	self doDamage( self.health + 10, self.origin );
}
