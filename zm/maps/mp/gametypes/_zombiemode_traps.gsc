// zm map traps: the electric traps a level carries in its MapEnts.
//
// Port of T5 maps\_zombiemode_traps.gsc (D:\CoD\COD-GSC-Source\T5-STEAM-GSC),
// the generic trap module T5's _zombiemode.gsc:119 starts on every map.  Not
// to be confused with _zombiemode_traps_placeable.gsc, C1's own thrown
// equipment traps.
//
// MEASURED (zm_asylum_sh.ff MapEnts, 2026-10-02): Verruckt is authored for
// THIS module, not for T5 zombie_cod5_asylum.gsc's own gas_access triggers
// (there are none).  Each of its two zappers is a trigger_multiple
// targetname `zombie_trap`, script_noteworthy "electric", script_string
// "an" / "as", whose target names: one trigger_use (the handle prompt), a
// codo_zombie_zapper_handle script_model with script_noteworthy "switch",
// zombie_zapper_cagelight models, and script_structs with script_sound
// (the audio points).  The effect points are separate structs named
// `<script_string>_trap_fx` (5 north, 6 south), which T5 reads client-side
// (clientscripts\zombie_theater_fx.csc trap_fx_monitor :162).  Nothing on
// this server read any of it, so both zappers were dead props.
//
// TAKEN, by T5 function: init :17 (elec_trap_time 40, cooldown 60),
// trap_init :74 (component sort, NEED_POWER until flag "power_on", then
// BUY_TRAP and green lights), trap_use_think :272 (solo buy from the
// player's points), trap_lights_red / _green :382 / :408 (cage light models
// and their zapper_light_* effects), trap_move_switches :469,
// trap_activate_electric :499, trap_audio_fx :662, play_electrical_sound
// :700, trap_damage :716, player_elec_damage :809, zombie_trap_death :884
// (electric arm), play_elec_vocals :1000, electroctute_death_fx :1016.
//
// CHANGED, and why:
//   * The effects T5 plays from its client script (zapper at every
//     <id>_trap_fx struct; the powerbox and wall-control effects at
//     zapper_fx_<side> / zapper_switch_fx_<side> for 30 s; the spark run
//     along trap_wire_sparks_<side>, zombie_cod5_asylum_fx.csc :181 / :272)
//     are spawned here: CoDOL has no client script VM.  The side is the
//     zapper_fx_* struct nearest the trap's handle, so nothing hard-codes
//     north and south.
//   * T5 trap_damage waits on the damage trigger's "trigger" notify.  This
//     server's actors do not touch-fire trigger_multiples reliably, so the
//     live trap polls every 0.1 s with isTouching against the same brush,
//     for zombies and players alike.
//   * T5's 10% "burn instead" branch needs animscripts\zombie_death
//     flame_death_fx and gib refs, neither of which this zombie has; every
//     zombie takes the electrocution branch.
//   * T5 team-pool buying: this server has no team pool; the player pays,
//     as in T5 solo.
//   * The buyer gets `notify( "zm_trap_activated", "zombie_trap_" + type )`
//     so C1's CHALLENGE_ZM_TRAPS ("activate &&1 traps") counts map traps
//     as well as placed ones (_zombiemode_challenges watch_traps).
//
// NOT TAKEN: trap_dialog (its VO line is commented out in T5 itself),
// clientnotify (not a builtin here), Exploder for numeric script_strings (no
// CoDOL map uses one), the rotating / flipper / fire types (no CoDOL map
// carries one).  zm_zombie_training's `zombie_trap` (script_string "fk")
// names no trigger_use -- its target is a second trigger_multiple and two
// audio structs -- so it has no T5 buy handle and is left alone, logged.

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	level.zombie_trap_killed_count = 0;
	level.zm_map_traps = [];
	level.elec_trap_time = 40;
	level.elec_trap_cooldown_time = 60;

	// Developer-only test verbs (scr_codol_powertest); returns at once on
	// a production server.
	level thread maps\mp\gametypes\_zombiemode_powertest::init();

	ents = getEntArray( "zombie_trap", "targetname" );
	traps = [];
	for ( i = 0; i < ents.size; i++ )
	{
		if ( ents[ i ] trap_usable() )
			traps[ traps.size ] = ents[ i ];
	}

	if ( traps.size < 1 )
		return;

	// Every name MEASURED in zm_asylum_sh.ff (docs/zone_assets).  Loaded
	// only when the map has a usable trap: misc/fx_zombie_* ship in no other
	// zone.  T5 asylum zombie_cod5_asylum_fx.gsc :100-:137.
	level._effect[ "zapper" ] = loadfx( "misc/fx_zombie_electric_trap" );
	level._effect[ "zapper_fx" ] = loadfx( "misc/fx_zombie_zapper_powerbox_on" );
	level._effect[ "zapper_wall" ] = loadfx( "misc/fx_zombie_zapper_wall_control_on" );
	level._effect[ "zapper_light_ready" ] = loadfx( "misc/fx_zombie_zapper_light_green" );
	level._effect[ "zapper_light_notready" ] = loadfx( "misc/fx_zombie_zapper_light_red" );
	level._effect[ "elec_torso" ] = loadfx( "env/electrical/fx_elec_player_torso" );
	level._effect[ "elec_md" ] = loadfx( "env/electrical/fx_elec_player_md" );
	level._effect[ "elec_sm" ] = loadfx( "env/electrical/fx_elec_player_sm" );
	level._effect[ "electric_short_oneshot" ] = loadfx( "env/electrical/fx_elec_short_oneshot" );

	// T5 trap_model_type_init :1185 "default".
	precacheModel( "zombie_zapper_cagelight_green" );
	precacheModel( "zombie_zapper_cagelight_red" );

	// T5 asylum :32.  shock/electrocution.shock ships in common_gametype_zm.
	precacheShellShock( "electrocution" );

	add_sound( "purchase", "zmb_cha_ching" );

	for ( i = 0; i < traps.size; i++ )
	{
		level.zm_map_traps[ level.zm_map_traps.size ] = traps[ i ];
		traps[ i ] thread trap_init();
	}
}

// self = a zombie_trap.  True when it is a type this module runs and it
// names at least one trigger_use to buy it from.
trap_usable()
{
	type = "<none>";
	if ( isDefined( self.script_noteworthy ) )
		type = self.script_noteworthy;

	if ( type != "electric" )
	{
		codolprintln( "CoDOL zm trap: zombie_trap ent " + self getEntityNumber() + " type " + type + " is not an electric trap; left alone" );
		return false;
	}

	if ( !isDefined( self.target ) )
	{
		codolprintln( "CoDOL zm trap: zombie_trap ent " + self getEntityNumber() + " targets nothing; left alone" );
		return false;
	}

	parts = getEntArray( self.target, "targetname" );
	for ( i = 0; i < parts.size; i++ )
	{
		if ( parts[ i ].classname == "trigger_use" )
			return true;
	}

	codolprintln( "CoDOL zm trap: zombie_trap ent " + self getEntityNumber() + " (" + self.target + ") has no trigger_use to buy it from; left alone" );
	return false;
}

// T5 :74.  self = the zombie_trap damage trigger.
trap_init()
{
	self._trap_type = self.script_noteworthy;
	self._trap_id = "<none>";
	if ( isDefined( self.script_string ) )
		self._trap_id = self.script_string;

	self._trap_use_trigs = [];
	self._trap_lights = [];
	self._trap_switches = [];
	self._trap_light_fx = [];

	components = getEntArray( self.target, "targetname" );
	for ( i = 0; i < components.size; i++ )
	{
		c = components[ i ];
		if ( isDefined( c.script_noteworthy ) && c.script_noteworthy == "switch" )
		{
			self._trap_switches[ self._trap_switches.size ] = c;
			continue;
		}

		if ( c.classname == "trigger_use" )
			self._trap_use_trigs[ self._trap_use_trigs.size ] = c;
		else if ( c.classname == "script_model" && isDefined( c.model ) )
		{
			if ( c.model == "zombie_zapper_cagelight" )
				self._trap_lights[ self._trap_lights.size ] = c;
			else if ( c.model == "zombie_zapper_handle" || c.model == "codo_zombie_zapper_handle" )
				self._trap_switches[ self._trap_switches.size ] = c;
		}
	}

	// Audio points: T5 :256, structs that share the trap's target.
	self._trap_audio_structs = get_struct_array( self.target, "targetname" );

	// Effect points: the client script's <id>_trap_fx (structs with a model
	// are not effect points, T5 theater_fx.csc :170).
	self._trap_fx_structs = [];
	fx = get_struct_array( self._trap_id + "_trap_fx", "targetname" );
	for ( i = 0; i < fx.size; i++ )
	{
		if ( !isDefined( fx[ i ].model ) )
			self._trap_fx_structs[ self._trap_fx_structs.size ] = fx[ i ];
	}

	// The zapper box and wall-switch effect points nearest the handle.
	anchor = self._trap_use_trigs[ 0 ].origin;
	self._trap_box_fx = nearest_struct_prefixed( "zapper_fx_", anchor );
	self._trap_wall_fx = nearest_struct_prefixed( "zapper_switch_fx_", anchor );
	self._trap_sparks = nearest_struct_prefixed( "trap_wire_sparks_", anchor );

	if ( !isDefined( self.zombie_cost ) )
		self.zombie_cost = 1000;
	else
		self.zombie_cost = int( self.zombie_cost );
	self._trap_in_use = 0;
	self._trap_cooling_down = 0;
	self._trap_duration = level.elec_trap_time;
	self._trap_cooldown_time = level.elec_trap_cooldown_time;

	codolprintln( "CoDOL zm trap: " + self._trap_type + " trap " + self._trap_id + " ent " + self getEntityNumber() +
		": use triggers " + self._trap_use_trigs.size + ", switches " + self._trap_switches.size +
		", lights " + self._trap_lights.size + ", audio points " + self._trap_audio_structs.size +
		", fx points " + self._trap_fx_structs.size + ", box fx " + isDefined( self._trap_box_fx ) +
		", wall fx " + isDefined( self._trap_wall_fx ) + ", sparks " + isDefined( self._trap_sparks ) +
		", cost " + self.zombie_cost );

	flag_wait( "all_players_connected" );

	self trap_lights_red();
	for ( i = 0; i < self._trap_use_trigs.size; i++ )
		self._trap_use_trigs[ i ] setCursorHint( "HINT_NOICON" );

	if ( !isDefined( self.script_flag_wait ) )
	{
		self trap_set_string( &"ZOMBIE_NEED_POWER" );
		flag_wait( "power_on" );
	}
	else
	{
		if ( !flag_exist( self.script_flag_wait ) )
			flag_init( self.script_flag_wait );
		flag_wait( self.script_flag_wait );
	}

	self trap_set_string( &"ZOMBIE_BUTTON_BUY_TRAP", self.zombie_cost );
	self trap_lights_green();
	codolprintln( "CoDOL zm trap: " + self._trap_id + " is available for " + self.zombie_cost );

	for ( i = 0; i < self._trap_use_trigs.size; i++ )
		self._trap_use_trigs[ i ] thread trap_use_think( self );
}

// The struct whose targetname starts with prefix nearest origin.
nearest_struct_prefixed( prefix, origin )
{
	best = undefined;
	best_d = 0;
	if ( !isDefined( level.struct ) )
		return undefined;
	for ( i = 0; i < level.struct.size; i++ )
	{
		s = level.struct[ i ];
		if ( !isDefined( s ) || !isDefined( s.targetname ) || !isDefined( s.origin ) )
			continue;
		if ( s.targetname.size < prefix.size || getSubStr( s.targetname, 0, prefix.size ) != prefix )
			continue;
		d = distanceSquared( s.origin, origin );
		if ( !isDefined( best ) || d < best_d )
		{
			best = s;
			best_d = d;
		}
	}
	return best;
}

// T5 :272.  self = a trigger_use, trap = the zombie_trap.
trap_use_think( trap )
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

		if ( !is_player_valid( who ) || trap._trap_in_use )
			continue;

		if ( who.score < trap.zombie_cost )
		{
			who maps\mp\gametypes\_zombiemode_audio::no_cash_vox();
			codolprintln( "CoDOL zm trap: " + trap._trap_id + " refused to " + who.name + " (" + who.score + " points, costs " + trap.zombie_cost + ")" );
			continue;
		}

		who maps\mp\gametypes\_zombiemode_score::minus_to_player_score( trap.zombie_cost );

		trap._trap_in_use = 1;
		trap trap_set_string( &"ZOMBIE_TRAP_ACTIVE" );
		play_sound_at_pos( "purchase", who.origin );

		// C1 CHALLENGE_ZM_TRAPS.
		if ( !isDefined( who.zm_traps_activated ) )
			who.zm_traps_activated = 0;
		who.zm_traps_activated++;
		who notify( "zm_trap_activated", "zombie_trap_" + trap._trap_type );

		codolprintln( "CoDOL zm trap: " + trap._trap_id + " bought by " + who.name + " for " + trap.zombie_cost + "; " + who.score + " points left" );

		if ( trap._trap_switches.size )
		{
			trap thread trap_move_switches();
			trap waittill( "switch_activated" );
		}

		trap thread trap_activate_electric();
		trap waittill( "trap_done" );

		trap._trap_cooling_down = 1;
		trap trap_set_string( &"ZOMBIE_TRAP_COOLDOWN" );
		codolprintln( "CoDOL zm trap: " + trap._trap_id + " done; cooling down " + trap._trap_cooldown_time + " s" );

		wait( trap._trap_cooldown_time );
		trap._trap_cooling_down = 0;

		trap notify( "available" );
		trap._trap_in_use = 0;
		trap trap_set_string( &"ZOMBIE_BUTTON_BUY_TRAP", trap.zombie_cost );
		codolprintln( "CoDOL zm trap: " + trap._trap_id + " is available again" );
	}
}

// T5 :425.  self = the trap.
trap_set_string( string, param1 )
{
	for ( i = 0; i < self._trap_use_trigs.size; i++ )
	{
		if ( !isDefined( param1 ) )
			self._trap_use_trigs[ i ] setHintString( string );
		else
			self._trap_use_trigs[ i ] setHintString( string, param1 );
	}
}

// T5 :382 / :408.  self = the trap.
trap_lights_red()
{
	self trap_lights( "zombie_zapper_cagelight_red", "zapper_light_notready" );
}

trap_lights_green()
{
	self trap_lights( "zombie_zapper_cagelight_green", "zapper_light_ready" );
}

trap_lights( model, fx_ref )
{
	for ( i = 0; i < self._trap_lights.size; i++ )
	{
		light = self._trap_lights[ i ];
		light setModel( model );

		if ( isDefined( light.zm_trap_fx ) )
			light.zm_trap_fx delete();

		angles = ( 0, 0, 0 );
		if ( isDefined( light.angles ) )
			angles = light.angles;
		light.zm_trap_fx = spawnFx( level._effect[ fx_ref ], light.origin, anglesToForward( angles ), anglesToUp( angles ) );
		triggerFx( light.zm_trap_fx );
	}
}

// T5 :469.  self = the trap.
trap_move_switches()
{
	self trap_lights_red();
	for ( i = 0; i < self._trap_switches.size; i++ )
	{
		self._trap_switches[ i ] rotatePitch( 180, 0.5 );
		self._trap_switches[ i ] playSound( "amb_sparks_l_b" );
	}
	wait( 0.5 );

	self notify( "switch_activated" );

	self waittill( "available" );
	for ( i = 0; i < self._trap_switches.size; i++ )
		self._trap_switches[ i ] rotatePitch( -180, 0.5 );
	wait( 0.5 );
	self trap_lights_green();
}

// T5 :499.  self = the trap.
trap_activate_electric()
{
	self._trap_duration = level.elec_trap_time;
	self._trap_cooldown_time = level.elec_trap_cooldown_time;
	self._trap_kills = 0;

	self notify( "trap_activate" );
	codolprintln( "CoDOL zm trap: " + self._trap_id + " live for " + self._trap_duration + " s" );

	// The client script's effects (see header).
	fx = [];
	for ( i = 0; i < self._trap_fx_structs.size; i++ )
	{
		s = self._trap_fx_structs[ i ];
		angles = ( 0, 0, 0 );
		if ( isDefined( s.angles ) )
			angles = s.angles;
		e = spawnFx( level._effect[ "zapper" ], s.origin, anglesToForward( angles ), anglesToUp( angles ) );
		triggerFx( e );
		fx[ fx.size ] = e;
	}
	self thread trap_machine_fx();
	if ( isDefined( self._trap_sparks ) )
		self thread trap_wire_sparks( self._trap_sparks );

	for ( i = 0; i < self._trap_audio_structs.size; i++ )
	{
		wait_network_frame();
		self._trap_audio_structs[ i ] thread trap_audio_fx( self );
	}

	self thread trap_damage();
	wait( self._trap_duration );

	self notify( "trap_done" );

	for ( i = 0; i < fx.size; i++ )
	{
		if ( isDefined( fx[ i ] ) )
			fx[ i ] delete();
	}

	codolprintln( "CoDOL zm trap: " + self._trap_id + " finished; zombies killed " + self._trap_kills );
}

// zombie_cod5_asylum_fx.csc zapper_switch_fx :181: the powerbox and the
// wall control glow for 30 s.
trap_machine_fx()
{
	made = [];
	pts = [];
	refs = [];
	if ( isDefined( self._trap_box_fx ) )
	{
		pts[ pts.size ] = self._trap_box_fx;
		refs[ refs.size ] = "zapper_fx";
	}
	if ( isDefined( self._trap_wall_fx ) )
	{
		pts[ pts.size ] = self._trap_wall_fx;
		refs[ refs.size ] = "zapper_wall";
	}
	for ( i = 0; i < pts.size; i++ )
	{
		angles = ( 0, 0, 0 );
		if ( isDefined( pts[ i ].angles ) )
			angles = pts[ i ].angles;
		e = spawnFx( level._effect[ refs[ i ] ], pts[ i ].origin, anglesToForward( angles ), anglesToUp( angles ) );
		triggerFx( e );
		made[ made.size ] = e;
	}

	wait( 30 );

	for ( i = 0; i < made.size; i++ )
	{
		if ( isDefined( made[ i ] ) )
			made[ i ] delete();
	}
}

// zombie_cod5_asylum_fx.csc electric_trap_wire_sparks :272: sparks run
// down the wire chain, again every 10-15 s, for 30 s.
trap_wire_sparks( start )
{
	self endon( "trap_done" );
	stop = getTime() + 30000;

	while ( getTime() < stop )
	{
		if ( isDefined( start.target ) )
			targ = get_struct( start.target, "targetname" );
		else
			targ = undefined;

		for ( n = 0; isDefined( targ ) && n < 32; n++ )
		{
			if ( randomInt( 100 ) > 50 )
				playFx( level._effect[ "electric_short_oneshot" ], targ.origin );
			wait( 0.075 );
			if ( isDefined( targ.target ) )
				targ = get_struct( targ.target, "targetname" );
			else
				targ = undefined;
		}
		wait( randomIntRange( 10, 15 ) );
	}
}

// T5 :662.  self = an audio struct.
trap_audio_fx( trap )
{
	sound_origin = spawn( "script_origin", self.origin );
	sound_origin playSound( "zmb_elec_start" );
	sound_origin playLoopSound( "zmb_elec_loop" );
	self thread play_electrical_sound( trap );

	trap waittill( "trap_done" );

	if ( isDefined( sound_origin ) )
	{
		sound_origin stopLoopSound();
		wait( 0.05 );
		sound_origin delete();
	}
}

// T5 :700.
play_electrical_sound( trap )
{
	trap endon( "trap_done" );
	for ( ;; )
	{
		wait( randomFloatRange( 0.1, 0.5 ) );
		playSoundAtPosition( "zmb_elec_arc", self.origin );
	}
}

// T5 :716, polled (see header).  self = the trap.
trap_damage()
{
	self endon( "trap_done" );

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
					z thread zombie_trap_death( self, randomInt( 100 ) );
			}
		}

		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( is_player_valid( p ) && p isTouching( self ) )
				p thread player_elec_damage();
		}

		wait( 0.1 );
	}
}

// T5 :809.  self = a player standing in a live trap.
player_elec_damage()
{
	self endon( "death" );
	self endon( "disconnect" );

	if ( isDefined( self.is_burning ) || self maps\mp\gametypes\_zombiemode_laststand::player_is_in_laststand() )
		return;

	// Under zm the native perk lookup resolves the T5 name to C1's no_slot
	// row specialty_zombie_armorvest (codol_script_perks.c read_definition).
	jugg = self hasPerk( "specialty_armorvest" );

	self.is_burning = 1;
	self shellShock( "electrocution", 2.5 );
	self playSound( "zmb_zombie_arc" );

	if ( !isDefined( self.zm_trap_shock_logged ) || getTime() - self.zm_trap_shock_logged > 5000 )
	{
		self.zm_trap_shock_logged = getTime();
		codolprintln( "CoDOL zm trap: " + self.name + " walked into a live trap (jugg " + jugg + ", health " + self.health + ")" );
	}

	if ( !jugg || self.health - 100 < 1 )
	{
		// T5 radiusdamage( origin, 10, health + 100, health + 100 ).
		self doDamage( self.health + 100, self.origin );
		self.is_burning = undefined;
	}
	else
	{
		self doDamage( 50, self.origin );
		wait( 0.1 );
		self.is_burning = undefined;
	}
}

// T5 :884, the electric arm.  self = a zombie.
zombie_trap_death( trap, param )
{
	self endon( "death" );
	self.marked_for_death = true;

	if ( !isDefined( self.animname ) || self.animname != "zombie_dog" )
	{
		playSoundAtPosition( "zmb_zombie_arc", self.origin );
		if ( randomInt( 100 ) > 50 )
		{
			self thread electroctute_death_fx();
			self thread play_elec_vocals();
		}
		wait( randomFloat( 1.25 ) );
		self playSound( "zmb_zombie_arc" );
	}

	level notify( "trap_kill", self, trap );
	level.zombie_trap_killed_count++;
	if ( isDefined( trap ) )
		trap._trap_kills++;
	self doDamage( self.health + 666, self.origin );
}

// T5 :1000.
play_elec_vocals()
{
	if ( !isDefined( self ) )
		return;
	org = self.origin;
	wait( 0.15 );
	playSoundAtPosition( "zmb_elec_vocals", org );
	playSoundAtPosition( "zmb_zombie_arc", org );
	playSoundAtPosition( "zmb_exp_jib_zombie", org );
}

// T5 :1016.  The tags are the T5 zombie skeleton's, which the CoDOL zombie
// models keep.
electroctute_death_fx()
{
	self endon( "death" );

	if ( is_true( self.is_electrocuted ) )
		return;
	self.is_electrocuted = true;

	playFxOnTag( level._effect[ "elec_torso" ], self, "J_SpineLower" );
	self playSound( "zmb_elec_jib_zombie" );
	wait( 1 );

	tags = [];
	tags[ 0 ] = "J_Elbow_LE";
	tags[ 1 ] = "J_Elbow_RI";
	tags[ 2 ] = "J_Knee_RI";
	tags[ 3 ] = "J_Knee_LE";
	tags = array_randomize( tags );
	playFxOnTag( level._effect[ "elec_md" ], self, tags[ 0 ] );
	self playSound( "zmb_elec_jib_zombie" );

	wait( 1 );
	self playSound( "zmb_elec_jib_zombie" );

	tags = [];
	tags[ 0 ] = "J_Wrist_RI";
	tags[ 1 ] = "J_Wrist_LE";
	tags[ 2 ] = "J_Ankle_RI";
	tags[ 3 ] = "J_Ankle_LE";
	tags = array_randomize( tags );
	playFxOnTag( level._effect[ "elec_sm" ], self, tags[ 0 ] );
	playFxOnTag( level._effect[ "elec_sm" ], self, tags[ 1 ] );
}
