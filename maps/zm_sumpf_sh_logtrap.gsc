// zm_sumpf_sh log trap: the spinning log under the north-west building.
//
// Port of T5 maps\zombie_cod5_sumpf_trap_pendulum.gsc (penThink :68,
// activatePen :151, trap_sounds :250, playerPenDamage :297,
// zombiePenDamage :317) onto the entities CoDOL's edit authors, which are
// T5's GENERIC trap layout (maps\_zombiemode_traps.gsc trap_init :73) rather
// than T5 sumpf's pendulum_buy_trigger chain.  MEASURED from the 3.3.3.3
// MapEnts:
//
//   zombie_trap  trigger_multiple (9558 1267 -511), spawnflags 5,
//                script_noteworthy "rotating", script_flag_wait "nw_magic_box",
//                target pf1904_auto1 -- the swept volume of the log.
//   pf1904_auto1 the log: script_model zombie_sumpf_logtrap_sh_d,
//                script_noteworthy "mover", script_string "pitch";
//                2 trigger_use (the buy handles, one each side);
//                2 zombie_zapper_handle (script_noteworthy "switch");
//                2 zombie_zapper_cagelight.
//   engine_loop_left/_right, wheel_loop_left/_right, blade_left/_right,
//   switch_left/_right, speaker_by_log -- T5 pendulum's sound origins, same
//   names, same places.
//
// _zombiemode_traps runs electric traps only and leaves this one alone (it
// logs "type rotating is not an electric trap"), so the level runs it.
//
// TAKEN: the buy (T5 penThink: 750 points unless the trigger carries
// zombie_cost), the lever throw with the "switch" sound at switch_left/right,
// the motor start/loop/stop and wheel/belt loops, the 30 s spin of
// 14040 degrees with 6 s ramp up and down (T5 rotatepitch( 14040, 30, 6, 6 );
// the edit's script_string "pitch" names the same axis), the blade whooshes,
// the 45 s cooldown and the "warning" announcement at speaker_by_log, the red
// and green cage lights (T5 turnLightRed / turnLightGreen).  Every alias
// (switch, motor_start_left/right, motor_loop_left/right, motor_stop_left/right,
// wheel_loop, belt_loop, blade_right, warning, zmb_death_gibs) ships in the
// ported map zone.
//
// CHANGED, and why:
//   * T5 waits on the damage trigger's "trigger" notify.  This server's actors
//     do not touch-fire trigger_multiples reliably (_zombiemode_traps header),
//     so the live trap polls isTouching every 0.1 s, as that module does.
//   * T5 flings a struck zombie with launchragdoll, which this VM does not
//     register; the zombie is killed in place with T5's own damage
//     (health + 600) and its gib sound.
//   * T5's player hit: solo 80 damage and a forced crouch, co-op a kill
//     unless already down.  Taken as is, with a one-second guard per player
//     so a solo player is not hit every poll.
//   * The hints are the generic trap strings (ZOMBIE_BUTTON_BUY_TRAP,
//     ZOMBIE_TRAP_ACTIVE, ZOMBIE_TRAP_COOLDOWN); T5's WAW_ZOMBIE_ACTIVATE_TRAP
//     is not a CoDOL localize key.
//
// NOT TAKEN: create_and_play_dialog (character VO), the "trap_log" fx on a
// struck zombie (no such effect in the zone), objectSolid's zipline helper
// (replaced by the wait-until-clear below).

#include maps\_zombiemode_utility;

LOG_TRAP_COST = 750;
LOG_TRAP_SPIN = 14040;
LOG_TRAP_TIME = 30;
LOG_TRAP_RAMP = 6;
LOG_TRAP_COOLDOWN = 45;

init()
{
	add_sound( "purchase", "zmb_cha_ching" );

	ents = getEntArray( "zombie_trap", "targetname" );
	count = 0;
	for ( i = 0; i < ents.size; i++ )
	{
		if ( !isDefined( ents[ i ].script_noteworthy ) || ents[ i ].script_noteworthy != "rotating" )
			continue;
		if ( !isDefined( ents[ i ].target ) )
			continue;
		ents[ i ] thread log_trap_init();
		count++;
	}
	codolprintln( "CoDOL zm sumpf: " + count + " log trap(s)" );
}

// self = the zombie_trap damage volume.
log_trap_init()
{
	self.log_movers = [];
	self.log_use_trigs = [];
	self.log_switches = [];
	self.log_lights = [];

	parts = getEntArray( self.target, "targetname" );
	for ( i = 0; i < parts.size; i++ )
	{
		p = parts[ i ];
		if ( isDefined( p.script_noteworthy ) && p.script_noteworthy == "mover" )
			self.log_movers[ self.log_movers.size ] = p;
		else if ( isDefined( p.script_noteworthy ) && p.script_noteworthy == "switch" )
			self.log_switches[ self.log_switches.size ] = p;
		else if ( p.classname == "trigger_use" )
			self.log_use_trigs[ self.log_use_trigs.size ] = p;
		else if ( p.classname == "script_model" && isDefined( p.model ) && p.model == "zombie_zapper_cagelight" )
			self.log_lights[ self.log_lights.size ] = p;
	}

	if ( self.log_movers.size < 1 || self.log_use_trigs.size < 1 )
	{
		codolprintln( "CoDOL zm sumpf: log trap " + self.target + " has " + self.log_movers.size + " movers and " +
			self.log_use_trigs.size + " use triggers; not run" );
		return;
	}

	if ( isDefined( self.zombie_cost ) )
		self.zombie_cost = int( self.zombie_cost );
	else
		self.zombie_cost = LOG_TRAP_COST;

	self.log_active = false;
	self.log_in_use = false;
	self.log_kills = 0;

	codolprintln( "CoDOL zm sumpf: log trap " + self.target + ": movers " + self.log_movers.size + ", use triggers " +
		self.log_use_trigs.size + ", switches " + self.log_switches.size + ", lights " + self.log_lights.size +
		", cost " + self.zombie_cost );

	flag_wait( "all_players_connected" );

	self log_lights( "zombie_zapper_cagelight_red", "zapper_light_notready" );
	self log_set_hint( "" );

	// T5 waitfor_flag_open_chest_location( "nw_magic_box" ): the log trap is
	// usable once the north-west door is bought.
	if ( isDefined( self.script_flag_wait ) )
	{
		flag_init( self.script_flag_wait );
		flag_wait( self.script_flag_wait );
	}

	self log_lights( "zombie_zapper_cagelight_green", "zapper_light_ready" );
	self log_set_hint( &"ZOMBIE_BUTTON_BUY_TRAP", self.zombie_cost );
	codolprintln( "CoDOL zm sumpf: log trap available for " + self.zombie_cost );

	for ( i = 0; i < self.log_use_trigs.size; i++ )
		self.log_use_trigs[ i ] thread log_use_think( self );
}

log_set_hint( string, param )
{
	for ( i = 0; i < self.log_use_trigs.size; i++ )
	{
		self.log_use_trigs[ i ] setCursorHint( "HINT_NOICON" );
		if ( isDefined( param ) )
			self.log_use_trigs[ i ] setHintString( string, param );
		else
			self.log_use_trigs[ i ] setHintString( string );
	}
}

// T5 turnLightRed / turnLightGreen.
log_lights( model, fx_ref )
{
	for ( i = 0; i < self.log_lights.size; i++ )
	{
		light = self.log_lights[ i ];
		light setModel( model );

		if ( isDefined( light.log_fx ) )
			light.log_fx delete();

		if ( !isDefined( level._effect[ fx_ref ] ) )
			continue;
		angles = ( 0, 0, 0 );
		if ( isDefined( light.angles ) )
			angles = light.angles;
		light.log_fx = spawnFx( level._effect[ fx_ref ], light.origin, anglesToForward( angles ), anglesToUp( angles ) );
		triggerFx( light.log_fx );
	}
}

// T5 penThink.  self = a buy trigger_use, trap = the zombie_trap.
log_use_think( trap )
{
	for ( ;; )
	{
		self waittill( "trigger", who );

		if ( !isDefined( who ) || !isPlayer( who ) )
			continue;
		if ( !who useButtonPressed() )
			continue;
		if ( who in_revive_trigger() )
			continue;
		if ( !is_player_valid( who ) || trap.log_in_use )
			continue;

		if ( who.score < trap.zombie_cost )
		{
			who maps\_zombiemode_audio::no_cash_vox();
			continue;
		}

		trap.log_in_use = true;
		who maps\_zombiemode_score::minus_to_player_score( trap.zombie_cost );
		play_sound_at_pos( "purchase", who.origin );

		// C1 CHALLENGE_ZM_TRAPS, as _zombiemode_traps::trap_use_think.
		if ( !isDefined( who.zm_traps_activated ) )
			who.zm_traps_activated = 0;
		who.zm_traps_activated++;
		who notify( "zm_trap_activated", "zombie_trap_rotating" );

		codolprintln( "CoDOL zm sumpf: log trap bought by " + who.name + " for " + trap.zombie_cost );

		trap log_lights( "zombie_zapper_cagelight_red", "zapper_light_notready" );
		trap log_set_hint( &"ZOMBIE_TRAP_ACTIVE" );

		trap log_move_levers( 180 );
		trap log_run( who );

		trap log_set_hint( &"ZOMBIE_TRAP_COOLDOWN" );
		trap log_move_levers( -180 );

		wait( LOG_TRAP_COOLDOWN );

		speaker = getEnt( "speaker_by_log", "targetname" );
		if ( isDefined( speaker ) )
			speaker playSound( "warning" );

		trap log_lights( "zombie_zapper_cagelight_green", "zapper_light_ready" );
		trap log_set_hint( &"ZOMBIE_BUTTON_BUY_TRAP", trap.zombie_cost );
		trap.log_in_use = false;
		codolprintln( "CoDOL zm sumpf: log trap available again" );
	}
}

// T5 moveLeverDown / moveLeverUp.  self = the trap.
log_move_levers( pitch )
{
	for ( i = 0; i < self.log_switches.size; i++ )
		self.log_switches[ i ] rotatePitch( pitch, 0.5 );

	sound_left = getEnt( "switch_left", "targetname" );
	sound_right = getEnt( "switch_right", "targetname" );
	if ( isDefined( sound_left ) )
		sound_left playSound( "switch" );
	if ( isDefined( sound_right ) )
		sound_right playSound( "switch" );

	wait( 0.5 );
}

// T5 activatePen.  self = the trap.
log_run( who )
{
	motor_left = getEnt( "engine_loop_left", "targetname" );
	motor_right = getEnt( "engine_loop_right", "targetname" );
	wheel_left = getEnt( "wheel_loop_left", "targetname" );
	wheel_right = getEnt( "wheel_loop_right", "targetname" );

	if ( isDefined( motor_left ) )
		playSoundAtPosition( "motor_start_left", motor_left.origin );
	if ( isDefined( motor_right ) )
		playSoundAtPosition( "motor_start_right", motor_right.origin );

	// T5's ramp-up pause.
	wait( 0.5 );

	if ( isDefined( motor_left ) )
		motor_left playLoopSound( "motor_loop_left" );
	if ( isDefined( motor_right ) )
		motor_right playLoopSound( "motor_loop_right" );
	wait_network_frame();
	if ( isDefined( wheel_left ) )
		wheel_left playLoopSound( "wheel_loop" );
	if ( isDefined( wheel_right ) )
		wheel_right playLoopSound( "belt_loop" );

	old_angles = [];
	for ( i = 0; i < self.log_movers.size; i++ )
	{
		mover = self.log_movers[ i ];
		old_angles[ i ] = mover.angles;
		mover notify( "log_stop_solid_watch" );
		mover notSolid();
		mover log_rotate( LOG_TRAP_SPIN, LOG_TRAP_TIME, LOG_TRAP_RAMP );
	}

	self.log_active = true;
	self.log_kills = 0;
	self thread log_damage( who );
	self thread log_blade_sounds();
	level thread log_trap_sounds( motor_left, motor_right, wheel_left, wheel_right );

	wait( LOG_TRAP_TIME );

	self.log_active = false;
	self notify( "log_done" );

	// The spin is whole turns; set the authored angles back exactly and give
	// the log its collision once nobody stands in it.
	for ( i = 0; i < self.log_movers.size; i++ )
	{
		self.log_movers[ i ].angles = old_angles[ i ];
		self.log_movers[ i ] thread log_solid_when_clear();
	}

	codolprintln( "CoDOL zm sumpf: log trap finished; zombies killed " + self.log_kills );
}

// The edit's script_string names the axis (T5 sumpf spins the log in pitch).
// self = a mover.
log_rotate( degrees, time, ramp )
{
	axis = "pitch";
	if ( isDefined( self.script_string ) )
		axis = self.script_string;

	if ( axis == "yaw" )
		self rotateYaw( degrees, time, ramp, ramp );
	else if ( axis == "roll" )
		self rotateRoll( degrees, time, ramp, ramp );
	else
		self rotatePitch( degrees, time, ramp, ramp );
}

// T5 objectSolid (zombie_cod5_sumpf_zipline.gsc).  self = a mover.
log_solid_when_clear()
{
	self endon( "log_stop_solid_watch" );

	for ( ;; )
	{
		clear = true;
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( isAlive( players[ i ] ) && players[ i ] isTouching( self ) )
			{
				clear = false;
				break;
			}
		}
		if ( clear )
			break;
		wait( 0.1 );
	}
	self solid();
}

// T5 trap_sounds.
log_trap_sounds( motor_left, motor_right, wheel_left, wheel_right )
{
	wait( 13 );
	if ( isDefined( motor_left ) )
	{
		motor_left stopLoopSound( 2 );
		motor_left playSound( "motor_stop_left" );
	}
	if ( isDefined( motor_right ) )
	{
		motor_right stopLoopSound( 2 );
		motor_right playSound( "motor_stop_right" );
	}
	wait( 8 );
	if ( isDefined( wheel_left ) )
		wheel_left stopLoopSound( 8 );
	if ( isDefined( wheel_right ) )
		wheel_right stopLoopSound( 8 );
}

// T5 blade_sounds plays "blade_right" at both blade origins each half turn of
// the log.  Timed here from the spin profile instead of polling the angle:
// 14040 degrees over 30 s with 6 s ramps is a 585 deg/s cruise, a half turn
// every 0.31 s; the ramps are approximated with a slower beat.
log_blade_sounds()
{
	self endon( "log_done" );

	blade_left = getEnt( "blade_left", "targetname" );
	blade_right = getEnt( "blade_right", "targetname" );
	cruise = 180.0 / ( LOG_TRAP_SPIN / ( LOG_TRAP_TIME - LOG_TRAP_RAMP ) );

	start = getTime();
	for ( ;; )
	{
		t = ( getTime() - start ) / 1000.0;
		beat = cruise;
		if ( t < LOG_TRAP_RAMP || t > LOG_TRAP_TIME - LOG_TRAP_RAMP )
			beat = cruise * 2;

		if ( t > 1 && t < LOG_TRAP_TIME - 1 )
		{
			if ( isDefined( blade_left ) )
				blade_left playSound( "blade_right" );
			if ( isDefined( blade_right ) )
				blade_right playSound( "blade_right" );
		}
		wait( beat );
	}
}

// T5 penDamage, polled (see header).  self = the trap.
log_damage( who )
{
	self endon( "log_done" );

	for ( ;; )
	{
		if ( isDefined( level.zombies ) )
		{
			zombies = [];
			for ( i = 0; i < level.zombies.size; i++ )
				zombies[ i ] = level.zombies[ i ];

			for ( i = 0; i < zombies.size; i++ )
			{
				z = zombies[ i ];
				if ( !isDefined( z ) || !isAlive( z ) || isDefined( z.marked_for_death ) )
					continue;
				if ( z isTouching( self ) )
					z thread log_zombie_death( self );
			}
		}

		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( is_player_valid( p ) && p isTouching( self ) )
				p thread log_player_damage();
		}

		wait( 0.1 );
	}
}

// T5 zombiePenDamage / play_imp_sound, without the ragdoll launch.
log_zombie_death( trap )
{
	self endon( "death" );
	self.marked_for_death = true;

	self playSound( "zmb_death_gibs" );

	level notify( "trap_kill", self, trap );
	if ( isDefined( level.zombie_trap_killed_count ) )
		level.zombie_trap_killed_count++;
	trap.log_kills++;

	self doDamage( self.health + 600, self.origin );
}

// T5 playerPenDamage.  self = a player in the spinning log.
log_player_damage()
{
	self endon( "death" );
	self endon( "disconnect" );

	if ( isDefined( self.log_hit_time ) && getTime() - self.log_hit_time < 1000 )
		return;
	self.log_hit_time = getTime();

	if ( self maps\_zombiemode_laststand::player_is_in_laststand() )
		return;

	if ( getPlayers().size == 1 )
	{
		self doDamage( 80, self.origin + ( 0, 0, 20 ) );
		self setStance( "crouch" );
	}
	else
		self doDamage( self.health + 100, self.origin );
}
