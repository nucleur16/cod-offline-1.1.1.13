// zm_factory_sh teleporters: Der Riese's three pads, the mainframe and the
// Pack-a-Punch door.
//
// Port of T5 maps\zombie_cod5_factory_teleporter.gsc
// (D:\CoD\COD-GSC-Source\T5-STEAM-GSC).  The CoDOL edit keeps T5's entity
// names (MEASURED, zm_factory_sh 3.5.2.6 MapEnts): trigger_teleport_pad_0..2,
// trigger_teleport_core, teleport_room_0..3 (the black rooms at y -320 east
// of the map), origin_teleport_player_0..3 (the mainframe landing spots),
// teleporter_powerup, pack_door, pack_door_clip, pad_<n>_wire structs.
//
// TAKEN, by T5 function: teleporter_init (cost 1500, delay 2 s, cooldown
// 5 s), init_pack_door (the door drops in three steps as the pads link,
// scaled to CoDOL's taller door model, see below; its clip connects paths
// and goes), pad_manager, teleport_pad_think,
// teleport_pad_countdown (30 s to reach the mainframe, with a HUD timer in
// place of T5's _zombiemode_timer), teleport_pad_active_think,
// player_teleporting, teleport_players (black room for 2 s, then the
// mainframe spots), teleport_core_hint_update, teleport_core_think,
// teleport_nuke (20 zombies / 300 units at the pad, any at the core),
// teleporter_wire_wait, teleport_aftereffects (explosion / electrocution
// shellshock, and the vision aftereffects on the zone's own flare and
// zombie_turned visions), packa_door_reminder, the powerup drop at
// teleporter_powerup, the exploders (played by zm_factory_sh's
// factory_exploder from the zone's createfx).
//
// CHANGED: SetInvisibleToPlayer is not a builtin here, so a "hidden" pad
// keeps an empty hint and ignores use (pad.factory_usable).  SetTransported
// (T5's first-person charge effect, an engine screen effect with no xanim
// behind it) is not a builtin and C1 has no equivalent; the charge plays
// C1's own player teleport effect, maps/zombie/fx_portable_teleporter_player
// (common_gametype_zm, the effect C1's teleporter_zm puts on a teleported
// player), on each player as the charge takes them and at their landing
// spot, and fades the view to the zone's flare vision over the last 0.3 s
// before the black room.  The black room takes T5's CSC black_box_start
// vision, "default" (common_mp, mounted by every match).  The aftereffects
// are T5's whole CSC set except the FOV one (cg_fov is the client's):
// explosion and electrocution shellshock, cheat_bw_invert_contrast,
// zombie_turned, the four-step contrast "flashy" sequence and flare, with
// T5's CSC timings (all the visions ship in common_mp or zm_factory_sh).
//
// STANCE (user, 2026-10-26: "when the player is in the teleporter enforce
// standing up").  T5 lets a player teleport crouched or prone (its black
// room takes stance offsets).  Here every player the charge takes is stood
// up (setStance "stand") and held standing (allowCrouch / allowProne false)
// while on the pad during the charge and until they land at the mainframe;
// a player who steps off the pad before the beam fires is released at once,
// and the hold is also lifted on death, last stand or disconnect.
//
// AUDIO.  T5 plays the teleporter's audio from CSC (zombie_cod5_factory_amb
// .csc) on the client notifies this script sends, and the players' lines
// from teleporter_vo.  C1 runs no CSC, so the server plays each at T5's
// moment; see the AUDIO section below for every T5 alias -> C1 alias.
//
// NOT TAKEN: the "teleporting too often after round 20" hound punishment
// (T5 special_dog_spawn; _zombiemode_ai_dogs has no out-of-round spawn) and
// its sam_nospawn line (in no C1 zone); the tele_help_<n> / packapunch_see
// trigger lines (the edit has neither trigger and C1 has no tele_help /
// perk_packa_see character line); the pole_fx_<n> spark poles (no pole_fx
// structs or createfx entries in the edit, although pole_spark ships in
// zm_factory_sh); the black room flashback images (CSC) and the FOV
// aftereffect.

#include maps\_zombiemode_utility;

// The PA's one-line-at-a-time hold (T5 pa_play_dialog waits for the line's
// sound to end; script cannot read a C1 line's length, and the ancr lines
// are short announcements).
PA_HOLD = 3;

// Level precache window: called from zm_factory_sh factory_precache.
precache()
{
	level._effect[ "factory_teleport_player" ] = loadfx( "maps/zombie/fx_portable_teleporter_player" );
}

init()
{
	level.teleport = [];
	level.active_links = 0;
	level.countdown = 0;

	level.teleport_delay = 2;
	level.teleport_cost = 1500;
	level.teleport_cooldown = 5;
	level.is_cooldown = false;
	level.active_timer = -1;
	level.teleport_time = 0;

	level.factory_pa_speaking = false;
	level.factory_sound_ok = [];

	flag_init( "teleporter_pad_link_1" );
	flag_init( "teleporter_pad_link_2" );
	flag_init( "teleporter_pad_link_3" );

	level.teleporter_pad_trig = [];
	for ( i = 0; i < 3; i++ )
	{
		trig = getEnt( "trigger_teleport_pad_" + i, "targetname" );
		if ( isDefined( trig ) )
			level.teleporter_pad_trig[ i ] = trig;
	}
	if ( level.teleporter_pad_trig.size < 3 || !isDefined( getEnt( "trigger_teleport_core", "targetname" ) ) )
	{
		codolprintln( "CoDOL zm factory teleporter: pads " + level.teleporter_pad_trig.size + ", no full teleporter set" );
		return;
	}
	for ( i = 0; i < 3; i++ )
		level.teleporter_pad_trig[ i ].factory_usable = true;

	flag_wait( "all_players_connected" );

	level thread teleport_pad_think( 0 );
	level thread teleport_pad_think( 1 );
	level thread teleport_pad_think( 2 );
	level thread teleport_core_think();
	level thread start_black_room_fx();
	level thread init_pack_door();

	codolprintln( "CoDOL zm factory teleporter: 3 pads, core, pack door ready" );
}

// T5 teleport_trigger_invisible: no per-player visibility builtin here.
teleport_trigger_invisible( hide )
{
	self.factory_usable = !hide;
	if ( hide )
		self setHintString( "" );
}

// --------------------------------------------------------------------------
// T5 init_pack_door.
//
// CHANGED: the drop distances.  T5 lowers the door 35 + 25 + 60 = 120 units,
// one unit more than its model is tall: zombie_teleporter_mainframe_door's
// XModel bounds in zombie_cod5_factory.ff are z -120.1 .. -1.3 (119 units).
// CoDOL's door is a different model, codo_zombie_teleporter_mainframe_door,
// and it is 167.5 units tall (lod0 vertices z -110.4 .. 57.1, exported from
// zm_factory_sh.ff), filling the doorway from the PaP room floor (z 103.4)
// to the top of the opening (about z 270; both MEASURED with bulletTrace on
// a probe server).  T5's 120 units left the top 47.5 units of the door
// standing in the doorway, z 103 .. 150.6, while the clip was already gone
// (players walked through a door that was still visibly half shut; the
// model's own bullet collision still stopped shots up to z 150).  Each step
// is scaled by 167.5 / 119 so the door opens the same fraction of the
// doorway at every link as T5's, and the last step takes it the whole way:
// 49 + 35 + 85 = 169 units, the top ending at z 101.6, under the floor.
// --------------------------------------------------------------------------
init_pack_door()
{
	door = getEnt( "pack_door", "targetname" );
	if ( !isDefined( door ) )
		return;

	flag_wait( "teleporter_pad_link_1" );
	door moveZ( -49, 1.5, 1 );
	door playSound( "packa_door_2" );
	door thread packa_door_reminder();
	wait( 2 );

	flag_wait( "teleporter_pad_link_2" );
	door moveZ( -35, 1.5, 1 );
	door playSound( "packa_door_2" );
	wait( 2 );

	flag_wait( "teleporter_pad_link_3" );
	door moveZ( -85, 1.5, 1 );
	door playSound( "packa_door_2" );

	clip = getEntArray( "pack_door_clip", "targetname" );
	for ( i = 0; i < clip.size; i++ )
	{
		clip[ i ] connectpaths();
		clip[ i ] delete();
	}
	codolprintln( "CoDOL zm factory teleporter: Pack-a-Punch door open" );
}

packa_door_reminder()
{
	while ( !flag( "teleporter_pad_link_3" ) )
	{
		rand = randomIntRange( 4, 16 );
		self playSound( "packa_door_hitch" );
		wait( rand );
	}
}

// T5 start_black_room_fx: exploders 901..904, one a second.
start_black_room_fx()
{
	for ( i = 901; i <= 904; i++ )
	{
		wait( 1 );
		maps\zm_factory_sh::factory_exploder( i );
	}
}

// --------------------------------------------------------------------------
// T5 pad_manager: every pad cools down after a teleport.
// --------------------------------------------------------------------------
pad_manager()
{
	for ( i = 0; i < level.teleporter_pad_trig.size; i++ )
	{
		level.teleporter_pad_trig[ i ] setHintString( &"ZOMBIE_TELEPORT_COOLDOWN" );
		level.teleporter_pad_trig[ i ] teleport_trigger_invisible( false );
	}

	level.is_cooldown = true;
	wait( level.teleport_cooldown );
	level.is_cooldown = false;

	for ( i = 0; i < level.teleporter_pad_trig.size; i++ )
	{
		if ( is_true( level.teleporter_pad_trig[ i ].teleport_active ) )
			level.teleporter_pad_trig[ i ] setHintString( &"ZOMBIE_TELEPORT_TO_CORE", level.teleport_cost );
		else
			level.teleporter_pad_trig[ i ] setHintString( &"ZOMBIE_LINK_TPAD" );
	}
}

// A use of the pad trigger by a valid player while the pad is usable.
pad_wait_use()
{
	for ( ;; )
	{
		self waittill( "trigger", user );
		if ( !isDefined( user ) || !isPlayer( user ) || !is_player_valid( user ) )
			continue;
		if ( !is_true( self.factory_usable ) )
			continue;
		return user;
	}
}

// --------------------------------------------------------------------------
// T5 teleport_pad_think: power up the pad, then run to the mainframe.
// --------------------------------------------------------------------------
teleport_pad_think( index )
{
	active = false;
	level.teleport[ index ] = "waiting";

	trigger = level.teleporter_pad_trig[ index ];
	trigger setCursorHint( "HINT_NOICON" );
	trigger setHintString( &"ZOMBIE_FLAMES_UNAVAILABLE" );

	flag_wait( "power_on" );

	trigger setHintString( &"ZOMBIE_POWER_UP_TPAD" );
	trigger.teleport_active = false;

	while ( !active )
	{
		user = trigger pad_wait_use();

		if ( level.active_links < 3 )
		{
			core = getEnt( "trigger_teleport_core", "targetname" );
			core teleport_trigger_invisible( false );
		}

		// When one starts the others are disabled.
		for ( i = 0; i < level.teleporter_pad_trig.size; i++ )
			level.teleporter_pad_trig[ i ] teleport_trigger_invisible( true );
		level.teleport[ index ] = "timer_on";
		codolprintln( "CoDOL zm factory teleporter: pad " + index + " powered by " + user.name + "; 30 s to the mainframe" );

		trigger thread teleport_pad_countdown( index, 30 );
		// T5 teleporter_vo( "countdown", trigger ): the players at the pad.
		trigger thread teleporter_vo_near( "countdown", user );

		while ( level.teleport[ index ] == "timer_on" )
			wait( 0.05 );

		if ( level.teleport[ index ] == "active" )
		{
			active = true;
			// MM: auto teleport the first time.
			teleporter_wire_wait( index );
			trigger thread player_teleporting( index );
		}
		else
		{
			codolprintln( "CoDOL zm factory teleporter: pad " + index + " timed out" );
			for ( i = 0; i < level.teleporter_pad_trig.size; i++ )
			{
				level.teleporter_pad_trig[ i ] teleport_trigger_invisible( false );
				if ( !is_true( level.teleporter_pad_trig[ i ].teleport_active ) )
					level.teleporter_pad_trig[ i ] setHintString( &"ZOMBIE_POWER_UP_TPAD" );
			}
		}
		wait( 0.05 );
	}

	if ( level.is_cooldown )
	{
		trigger setHintString( &"ZOMBIE_TELEPORT_COOLDOWN" );
		trigger teleport_trigger_invisible( false );
		trigger.teleport_active = true;
	}
	else
		trigger thread teleport_pad_active_think( index );
}

// T5 teleport_pad_countdown, with a stopwatch HUD per player in place of
// T5 _zombiemode_timer::start_timer.
teleport_pad_countdown( index, time )
{
	self endon( "stop_countdown" );

	if ( level.active_timer < 0 )
		level.active_timer = index;
	level.countdown++;

	// T5 ClientNotify( "pac" + index ): the PA countdown.
	self thread pa_countdown( index );

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] thread pad_timer_hud( time + 1, self );

	wait( time + 1 );

	if ( level.active_timer == index )
		level.active_timer = -1;

	level.teleport[ index ] = "timer_off";
	level.countdown--;
}

pad_timer_hud( time, pad )
{
	hud = newClientHudElem( self );
	hud.alignX = "center";
	hud.alignY = "top";
	hud.horzAlign = "center";
	hud.vertAlign = "top";
	hud.x = 0;
	hud.y = 60;
	hud.fontScale = 1.6;
	hud.foreground = true;
	hud setTimer( time );

	pad waittill_any_timeout_factory( time, "stop_countdown" );
	if ( isDefined( hud ) )
		hud destroy();
}

waittill_any_timeout_factory( time, msg )
{
	self endon( msg );
	wait( time );
}

// --------------------------------------------------------------------------
// T5 teleport_pad_active_think: a linked pad sends players to the core.
// --------------------------------------------------------------------------
teleport_pad_active_think( index )
{
	self setCursorHint( "HINT_NOICON" );
	self.teleport_active = true;

	for ( ;; )
	{
		user = self pad_wait_use();

		if ( level.is_cooldown )
			continue;
		if ( user.score < level.teleport_cost )
		{
			play_sound_at_pos( "no_purchase", self.origin );
			continue;
		}

		for ( i = 0; i < level.teleporter_pad_trig.size; i++ )
			level.teleporter_pad_trig[ i ] teleport_trigger_invisible( true );

		user maps\_zombiemode_score::minus_to_player_score( level.teleport_cost );
		codolprintln( "CoDOL zm factory teleporter: " + user.name + " teleports from pad " + index );

		// Not threaded, so the trigger stays off until the cooldown.
		self player_teleporting( index );
	}
}

// T5 player_teleporting.
player_teleporting( index )
{
	time_since_last_teleport = getTime() - level.teleport_time;
	level.factory_charge_over = false;

	maps\zm_factory_sh::factory_exploder( 202 + index * 100 );
	maps\zm_factory_sh::factory_exploder( 105 );

	// T5 ClientNotify( "tpw" + index ): the pad's and the mainframe's warmup
	// and cooldown sounds.
	level thread teleport_warmup_audio( index );

	// T5 teleport_pad_player_fx (SetTransported), with the stance hold.
	self thread teleport_pad_player_fx( level.teleport_delay );
	self thread teleport_2d_audio();
	self thread teleport_nuke( 20, 300 );

	wait( level.teleport_delay );
	self notify( "fx_done" );

	maps\zm_factory_sh::factory_exploder( 201 + index * 100 );

	self teleport_players();

	// T5 ClientNotify( "tpc" + index ): the PA's "teleport finished".
	level thread pa_teleport_finish();

	if ( !level.is_cooldown )
		level thread pad_manager();

	// A goodie a couple of seconds later.
	wait( 2 );
	ss = get_struct( "teleporter_powerup", "targetname" );
	if ( isDefined( ss ) && isDefined( level.zombie_powerup_array ) && level.zombie_powerup_array.size > 0 )
	{
		name = maps\_zombiemode_powerups::get_next_powerup();
		if ( isDefined( name ) )
			maps\_zombiemode_powerups::specific_powerup_drop( name, ss.origin );
	}

	level.teleport_time = getTime();
}

// T5 player_is_near_pad: within 176 units (88 * 2) in 2D.
player_is_near_pad( player )
{
	return distance2d( player.origin, self.origin ) < 88 * 2;
}

// T5 teleport_2d_audio (the CSC "t2d" sounds, teleport_2d_fnt / _rear).
teleport_2d_audio()
{
	self endon( "fx_done" );
	for ( ;; )
	{
		players = getPlayers();
		wait( 1.7 );
		for ( i = 0; i < players.size; i++ )
		{
			if ( isDefined( players[ i ] ) && self player_is_near_pad( players[ i ] ) )
			{
				players[ i ] factory_local_sound( "teleport_2d_fnt" );
				players[ i ] factory_local_sound( "teleport_2d_rear" );
			}
		}
	}
}

// T5 teleport_players: the players on the pad wait 2 s in the black room,
// then land on the mainframe spots.
teleport_players()
{
	player_radius = 16;
	players = getPlayers();

	core_pos = [];
	occupied = [];
	image_room = [];
	touching = [];

	prone_offset = ( 0, 0, 49 );
	crouch_offset = ( 0, 0, 20 );

	for ( i = 0; i < 4; i++ )
	{
		core_pos[ i ] = getEnt( "origin_teleport_player_" + i, "targetname" );
		occupied[ i ] = false;
		image_room[ i ] = getEnt( "teleport_room_" + i, "targetname" );

		if ( !isDefined( players[ i ] ) || !self player_is_near_pad( players[ i ] ) || !isAlive( players[ i ] ) )
			continue;
		touching[ touching.size ] = i;
		players[ i ].factory_teleporting = true;
		if ( !isDefined( image_room[ i ] ) )
			continue;

		p = players[ i ];
		p disableOffhandWeapons();
		p disableWeapons();
		desired = image_room[ i ].origin;
		if ( p getStance() == "prone" )
			desired = desired + prone_offset;
		else if ( p getStance() == "crouch" )
			desired = desired + crouch_offset;

		factory_teleport_fx( p.origin );
		p.teleport_origin = spawn( "script_origin", p.origin );
		p.teleport_origin.angles = p.angles;
		p linkTo( p.teleport_origin );
		p.teleport_origin.origin = desired;
		p freezeControls( true );
		wait_network_frame();
		if ( isDefined( p ) && isDefined( p.teleport_origin ) )
		{
			// T5 setClientSysState "black_box_start": the plain vision.
			p visionSetNakedForPlayer( "default", 0 );
			p.teleport_origin.angles = image_room[ i ].angles;
		}
	}
	// The charge is over: a held player the beam did not take is released.
	level.factory_charge_over = true;

	wait( 2 );

	core = getEnt( "trigger_teleport_core", "targetname" );
	core thread teleport_nuke( undefined, 300 );

	for ( i = 0; i < players.size; i++ )
	{
		if ( !isDefined( players[ i ] ) )
			continue;
		for ( j = 0; j < 4; j++ )
		{
			if ( !occupied[ j ] && isDefined( core_pos[ j ] ) && distance2d( core_pos[ j ].origin, players[ i ].origin ) < player_radius )
				occupied[ j ] = true;
		}
	}

	wait_network_frame();

	for ( i = 0; i < touching.size; i++ )
	{
		player = players[ touching[ i ] ];
		if ( !isDefined( player ) )
			continue;

		slot = i;
		start = 0;
		while ( occupied[ slot ] && start < 4 )
		{
			start++;
			slot++;
			if ( slot >= 4 )
				slot = 0;
		}
		occupied[ slot ] = true;

		player unlink();
		if ( isDefined( player.teleport_origin ) )
		{
			player.teleport_origin delete();
			player.teleport_origin = undefined;
		}
		player enableWeapons();
		player enableOffhandWeapons();
		if ( isDefined( core_pos[ slot ] ) )
		{
			player setOrigin( core_pos[ slot ].origin );
			player setPlayerAngles( core_pos[ slot ].angles );
			factory_teleport_fx( core_pos[ slot ].origin );
		}
		// T5 setClientSysState "black_box_end": back to the level's vision.
		player visionSetNakedForPlayer( factory_level_vision(), 0 );
		player freezeControls( false );
		player.factory_teleporting = undefined;
		player teleport_stance_release();
		player thread teleport_aftereffects();
	}

	maps\zm_factory_sh::factory_exploder( 106 );
}

// T5 teleport_core_hint_update.
teleport_core_hint_update()
{
	self setCursorHint( "HINT_NOICON" );
	last = "";
	for ( ;; )
	{
		state = "";
		if ( !flag( "power_on" ) )
			state = "nopower";
		else if ( teleport_pads_are_active() )
			state = "link";
		else if ( level.active_links == 0 )
			state = "inactive";

		if ( state != last )
		{
			last = state;
			switch ( state )
			{
			case "nopower":		self setHintString( &"ZOMBIE_FLAMES_UNAVAILABLE" ); break;
			case "link":		self setHintString( &"ZOMBIE_LINK_TPAD" ); break;
			case "inactive":	self setHintString( &"ZOMBIE_INACTIVE_TPAD" ); break;
			default:			self setHintString( "" ); break;
			}
		}
		wait( 0.05 );
	}
}

// T5 teleport_core_think: using the core while a pad counts down links it.
teleport_core_think()
{
	trigger = getEnt( "trigger_teleport_core", "targetname" );
	trigger thread teleport_core_hint_update();

	flag_wait( "power_on" );

	for ( ;; )
	{
		if ( teleport_pads_are_active() )
		{
			trigger waittill( "trigger", who );
			if ( !isDefined( who ) || !isPlayer( who ) || !is_player_valid( who ) )
				continue;

			for ( i = 0; i < level.teleport.size; i++ )
			{
				if ( !isDefined( level.teleport[ i ] ) || level.teleport[ i ] != "timer_on" )
					continue;

				level.teleport[ i ] = "active";
				level.active_links++;
				flag_set( "teleporter_pad_link_" + level.active_links );
				codolprintln( "CoDOL zm factory teleporter: pad " + i + " linked by " + who.name + " (" + level.active_links + " of 3)" );

				maps\zm_factory_sh::factory_exploder( 102 + i );

				pad = getEnt( "trigger_teleport_pad_" + i, "targetname" );
				pad stop_countdown();

				// T5 ClientNotify( "scd" + i ) (the PA's "pad <n> active")
				// and ClientNotify( "tp" + i ) (the pad's power loop).
				level thread pa_link_success( i, level.active_links == 3 );
				level thread telepad_power_loop( i );

				if ( level.active_links == 3 )
				{
					maps\zm_factory_sh::factory_exploder( 101 );
					// T5 ClientNotify( "pap1" ): linkall_2d and the homepad
					// loops; teleporter_vo( "linkall", trigger ).
					level thread linkall_audio();
					trigger thread teleporter_vo_near( "linkall", who );
					earthquake( 0.3, 2.0, trigger.origin, 3700 );
				}

				level.active_timer = -1;
			}
		}
		wait( 0.05 );
	}
}

stop_countdown()
{
	self notify( "stop_countdown" );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] notify( "stop_countdown" );
}

teleport_pads_are_active()
{
	for ( i = 0; i < level.teleport.size; i++ )
	{
		if ( isDefined( level.teleport[ i ] ) && level.teleport[ i ] == "timer_on" )
			return true;
	}
	return false;
}

// T5 teleport_nuke: kill the zombies nearest the pad (all within range when
// max_zombies is undefined).
teleport_nuke( max_zombies, range )
{
	// The live zombies and hounds, as _zombiemode_powerups nuke_powerup
	// reads them (T5: getaispeciesarray( "axis" )).
	zombies = [];
	if ( isDefined( level.zombies ) )
		zombies = level.zombies;
	near = [];
	for ( i = 0; i < zombies.size; i++ )
	{
		if ( isDefined( zombies[ i ] ) && isAlive( zombies[ i ] ) && distance( zombies[ i ].origin, self.origin ) <= range )
			near[ near.size ] = zombies[ i ];
	}

	// Closest first.
	for ( i = 1; i < near.size; i++ )
	{
		for ( j = i; j > 0 && distanceSquared( near[ j - 1 ].origin, self.origin ) > distanceSquared( near[ j ].origin, self.origin ); j-- )
		{
			swap = near[ j ];
			near[ j ] = near[ j - 1 ];
			near[ j - 1 ] = swap;
		}
	}

	count = near.size;
	if ( isDefined( max_zombies ) && count > max_zombies )
		count = max_zombies;

	for ( i = 0; i < count; i++ )
	{
		wait( randomFloatRange( 0.2, 0.3 ) );
		z = near[ i ];
		if ( !isDefined( z ) || !isAlive( z ) )
			continue;
		if ( !is_true( z.isdog ) && !is_true( z.no_gib ) )
			z maps\_zombiemode_gib::zombie_head_gib();
		z doDamage( z.health + 10000, z.origin );
		if ( soundExists( "nuked" ) )
			playSoundAtPosition( "nuked", z.origin );
	}
}

// T5 teleporter_wire_wait: the spark travels along the pad's wire, 0.1 s a
// struct.
teleporter_wire_wait( index )
{
	targ = get_struct( "pad_" + index + "_wire", "targetname" );
	while ( isDefined( targ ) && isDefined( targ.target ) )
	{
		wait( 0.1 );
		targ = get_struct( targ.target, "targetname" );
	}
}

// T5 teleport_aftereffects: one at random from T5's set (minus the FOV one,
// see the header).  T5's vision aftereffects run in CSC with these visions
// and timings (zombie_cod5_factory_teleporter.csc).
teleport_aftereffects()
{
	switch ( randomInt( 6 ) )
	{
	case 0:
		self shellShock( "explosion", 4 );
		break;
	case 1:
		self shellShock( "electrocution", 4 );
		break;
	case 2:
		self thread teleport_aftereffect_vision( "cheat_bw_invert_contrast" );
		break;
	case 3:
		self thread teleport_aftereffect_vision( "zombie_turned" );
		break;
	case 4:
		self thread teleport_aftereffect_flashy();
		break;
	default:
		self thread teleport_aftereffect_vision( "flare" );
		break;
	}
}

factory_level_vision()
{
	if ( isDefined( level.zm_vision ) )
		return level.zm_vision;
	return "zm_factory_sh";
}

// T5 CSC teleport_aftereffect_bw / red / flare_vision.
teleport_aftereffect_vision( vision )
{
	self endon( "disconnect" );
	self visionSetNakedForPlayer( vision, 0.4 );
	wait( 1.25 );
	self visionSetNakedForPlayer( factory_level_vision(), 1 );
}

// T5 CSC teleport_aftereffect_flashy_vision.
teleport_aftereffect_flashy()
{
	self endon( "disconnect" );
	self visionSetNakedForPlayer( "cheat_bw_invert_contrast", 0.1 );
	wait( 0.4 );
	self visionSetNakedForPlayer( "cheat_bw_contrast", 0.1 );
	wait( 0.4 );
	self visionSetNakedForPlayer( "cheat_invert_contrast", 0.1 );
	wait( 0.4 );
	self visionSetNakedForPlayer( "cheat_contrast", 0.1 );
	wait( 0.4 );
	self visionSetNakedForPlayer( factory_level_vision(), 5 );
}

// --------------------------------------------------------------------------
// The player during the charge: T5 teleport_pad_player_fx, which calls
// SetTransported( delay ) every frame on each player near the pad (and
// SetTransported( 0 ) on the others).  Here a player near the pad is stood
// up and held standing, gets C1's player teleport effect once, and in the
// last 0.3 s of the charge fades to flare (the white-out into the black
// room); one who walks off is released and their view restored.  On
// fx_done the players still near the pad are the ones teleport_players
// takes; it releases them as they land.
// --------------------------------------------------------------------------
teleport_pad_player_fx( delay )
{
	self endon( "fx_done" );

	start = getTime();
	flashed = [];
	for ( ;; )
	{
		late = ( getTime() - start ) >= ( delay - 0.3 ) * 1000;
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( !isDefined( p ) )
				continue;
			num = p getEntityNumber();
			near = self player_is_near_pad( p ) && isAlive( p ) && !is_true( p.laststand );
			if ( near )
			{
				if ( !is_true( p.factory_stance_held ) )
				{
					p teleport_stance_hold();
					factory_teleport_fx( p.origin );
				}
				if ( late && !isDefined( flashed[ num ] ) )
				{
					flashed[ num ] = true;
					p visionSetNakedForPlayer( "flare", 0.3 );
				}
			}
			else if ( is_true( p.factory_stance_held ) && !is_true( p.factory_teleporting ) )
			{
				p teleport_stance_release();
				if ( isDefined( flashed[ num ] ) )
				{
					flashed[ num ] = undefined;
					p visionSetNakedForPlayer( factory_level_vision(), 0.3 );
				}
			}
		}
		wait( 0.05 );
	}
}

// self = player.  Stand, and stay standing until teleport_stance_release.
teleport_stance_hold()
{
	if ( is_true( self.factory_stance_held ) )
		return;
	self.factory_stance_held = true;
	if ( self getStance() != "stand" )
		self setStance( "stand" );
	self allowCrouch( false );
	self allowProne( false );
	self thread teleport_stance_watch();
	factory_sound_log( "stance hold " + self.name + " (was " + self getStance() + ")" );
}

teleport_stance_release()
{
	if ( !is_true( self.factory_stance_held ) )
		return;
	self.factory_stance_held = undefined;
	self notify( "factory_stance_release" );
	self allowCrouch( true );
	self allowProne( true );
	factory_sound_log( "stance release " + self.name );
}

// Lifts the hold if the player dies or goes down while it is on, and, once
// the charge is over (teleport_players has marked the players it takes),
// from a player the beam did not take.
teleport_stance_watch()
{
	self endon( "disconnect" );
	self endon( "factory_stance_release" );

	for ( ;; )
	{
		wait( 0.05 );
		if ( !isAlive( self ) || is_true( self.laststand ) || self.sessionstate != "playing" )
		{
			self.factory_teleporting = undefined;
			self teleport_stance_release();
			return;
		}
		if ( is_true( level.factory_charge_over ) && !is_true( self.factory_teleporting ) )
		{
			self teleport_stance_release();
			self visionSetNakedForPlayer( factory_level_vision(), 0.3 );
			return;
		}
	}
}

factory_teleport_fx( origin )
{
	if ( isDefined( level._effect[ "factory_teleport_player" ] ) )
		playFx( level._effect[ "factory_teleport_player" ], origin );
}

// --------------------------------------------------------------------------
// AUDIO.  T5 alias (zombie_cod5_factory_amb.csc / teleporter.gsc) -> C1
// alias, MEASURED in the zones a zm_factory_sh match mounts (CodolSoundDumper
// zone-aliases on client_shipRetail\chinese, 2026-10-26):
//
//   PA countdown ("pac"), pa_countdown:
//     pa_buzz                     pa_buzz              zm_factory_sh
//     pa_audio_link_start         link_count_start_00  localized_common_gametype_zm
//     pa_audio_link_20/15/10..1   link_count_<n>_00    localized_common_gametype_zm
//     clock_tick_1sec             clock_tick_1sec      zm_factory_sh
//     pa_audio_link_fail          link_count_no_00     localized_common_gametype_zm
//   Link ("scd"), pa_countdown_success:
//     pa_audio_act_pad_0/1/2      pad_active_1/2/3_00  localized_common_gametype_zm
//     (third link)                pad_active_all_00    localized_common_gametype_zm
//   Pad power loop ("tp"), telepad_loop: power_loop (zm_factory_sh)
//   All linked ("pap1"): linkall_2d, homepad_power_loop, break_arc
//   Teleport ("tpw"), teleportation_audio: <script_sound>_warmup /
//     _cooldown = pad_ / pad2l_ / pad2r_warmup / _cooldown, the homepad's
//     top_spark1..4_warmup and lever_pull_warmup (zm_factory_sh)
//   "t2d": teleport_2d_fnt / teleport_2d_rear (zm_factory_sh)
//   After the teleport ("tpc"), pa_teleport:
//     pa_teleport_finish          teleport_end_00      localized_common_gametype_zm
//   teleporter_vo "countdown" (tele_count) and "linkall" (tele_linkall):
//     the speaker's <code>_teleporter line (hh_teleporter / lb_teleporter,
//     localized_common_gametype_zm; the only teleporter lines C1 has),
//     through _zombiemode_audio create_and_play_dialog.
//
// HOW EACH PLAYS.  Every PA line in C1 is a 2D alias (flags 0x4700, as the
// characters' *_2d twins), so where T5 plays them at each pa_system speaker
// struct (the edit has none) the server plays them as a local sound on every
// player; pa_buzz and clock_tick_1sec go the same way (T5's clock tick is a
// CSC playsound at the world origin, i.e. everywhere).  linkall_2d and the
// teleport_2d pair are 2D in both games.  The pad and mainframe sounds are
// positional (playSoundAtPosition, or playLoopSound on a script_origin) where
// C1's own createfx puts them: the zone's maps/createfx/zm_factory_sh_fx.gsc
// carries T5's audio exploders "tp<n>", "tpw<n>" and "801" (the homepad
// loops), but in the original level's frame: each sits (1050, 700, 1080)
// from where the CoDOL edit has the same object (MEASURED: the tpw top-spark
// rings centre on the three pads within 8 units, tp1's tower loops land on
// exploder 103's power_loop origins, 801's break_arc on the power station's
// sparks and homepad_power_loop on the mainframe), so they are translated
// back.  Pad cooldowns play at the pad's own warmup origins (exploders
// 202 / 302 / 402), as T5's telepad structs play warmup and cooldown from
// one struct.
// --------------------------------------------------------------------------

factory_old_frame( origin )
{
	return origin + ( -1050, -700, -1080 );
}

factory_sound_ok( alias )
{
	if ( !isDefined( level.factory_sound_ok ) )
		level.factory_sound_ok = [];
	if ( !isDefined( level.factory_sound_ok[ alias ] ) )
	{
		level.factory_sound_ok[ alias ] = soundExists( alias );
		if ( !level.factory_sound_ok[ alias ] )
			codolprintln( "CoDOL zm factory sound: alias " + alias + " is in no mounted zone" );
	}
	return level.factory_sound_ok[ alias ];
}

factory_sound_log( text )
{
	if ( getDvarInt( "scr_codol_factory_soundlog" ) > 0 )
		codolprintln( "CoDOL zm factory sound: " + text );
}

// self = player.
factory_local_sound( alias )
{
	if ( !factory_sound_ok( alias ) )
		return;
	self playLocalSound( alias );
	factory_sound_log( "local " + alias + " -> " + self.name );
}

// A 2D sound to every player (T5's CSC playsound on each client).
factory_sound_all( alias )
{
	if ( !factory_sound_ok( alias ) )
		return;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isDefined( players[ i ] ) )
			players[ i ] playLocalSound( alias );
	}
	factory_sound_log( "all " + alias + " (" + players.size + " players)" );
}

factory_sound_at( alias, origin )
{
	if ( !factory_sound_ok( alias ) )
		return;
	playSoundAtPosition( alias, origin );
	factory_sound_log( "at " + alias + " " + origin );
}

factory_sound_at_delayed( alias, origin, delay )
{
	if ( delay > 0 )
		wait( delay );
	factory_sound_at( alias, origin );
}

factory_loop_at( alias, origin )
{
	if ( !factory_sound_ok( alias ) )
		return;
	ent = spawn( "script_origin", origin );
	ent playLoopSound( alias );
	factory_sound_log( "loop " + alias + " " + origin );
}

// T5 pa_play_dialog: one PA line at a time; a line asked for while one
// plays is dropped.
pa_play_dialog( alias )
{
	if ( level.factory_pa_speaking )
	{
		factory_sound_log( "pa busy, dropped " + alias );
		return;
	}
	level.factory_pa_speaking = true;
	factory_sound_all( alias );
	wait( PA_HOLD );
	level.factory_pa_speaking = false;
}

// T5 pa_countdown, self = the pad trigger.  The link's stop_countdown ends
// it, so the count and the failure line only play while the pad waits.
pa_countdown( index )
{
	self endon( "stop_countdown" );

	factory_sound_all( "pa_buzz" );
	level thread pa_play_dialog( "link_count_start_00" );

	for ( count = 30; count > 0; count-- )
	{
		if ( count == 20 || count == 15 || count <= 10 )
			factory_sound_all( "link_count_" + count + "_00" );
		factory_sound_all( "clock_tick_1sec" );
		wait( 1 );
	}
	factory_sound_all( "pa_buzz" );
	wait( 1.2 );
	codolprintln( "CoDOL zm factory teleporter: PA link failed, pad " + index );
	level thread pa_play_dialog( "link_count_no_00" );
}

// T5 pa_countdown_success.  On the third link the PA follows with
// pad_active_all_00, C1's "all pads linked" line (T5 has none to play; its
// pa_audio_link_yes is commented out).
pa_link_success( index, all )
{
	factory_sound_all( "pa_buzz" );
	wait( 1.2 );
	// The link line wins over a countdown line still holding the PA.
	level.factory_pa_speaking = false;
	pa_play_dialog( "pad_active_" + ( index + 1 ) + "_00" );
	if ( all )
		pa_play_dialog( "pad_active_all_00" );
}

// T5 pa_teleport: a second after the teleport.
pa_teleport_finish()
{
	wait( 1 );
	factory_sound_all( "pa_buzz" );
	wait( 1.2 );
	pa_play_dialog( "teleport_end_00" );
}

// T5 teleport_pad_init / telepad_loop: the pad's loopers start at its link
// (createfx exploder "tp<n>").  tp1 / tp2's tower loops are exploders 103 /
// 104's power_loop already; these are the remaining ones (tp0's one looper
// is where C1's createfx puts it, at the mainframe).
telepad_power_loop( index )
{
	switch ( index )
	{
	case 0:
		factory_loop_at( "power_loop", factory_old_frame( ( 989.5, 1203, 1389.5 ) ) );
		break;
	case 1:
		factory_loop_at( "power_loop", factory_old_frame( ( 1346.5, -2492, 1400 ) ) );
		break;
	default:
		factory_loop_at( "power_loop", factory_old_frame( ( -733.2, -413, 1432.5 ) ) );
		break;
	}
}

// T5 linkall_2d and homepad_loop (createfx exploder "801").
linkall_audio()
{
	factory_sound_all( "linkall_2d" );
	factory_loop_at( "homepad_power_loop", factory_old_frame( ( 989.5, 1203, 1389.5 ) ) );
	factory_loop_at( "break_arc", factory_old_frame( ( 545.5, 1038.5, 1261 ) ) );
}

// T5 teleportation_audio ("tpw" + index): the telepad's structs play warmup
// and, 2 s later, cooldown; the homepad's play both 2 s in, as the players
// arrive.  The pad warmups at the pad itself are exploders 202 / 302 / 402
// (zm_factory_sh factory_exploder); the rest are here.
teleport_warmup_audio( index )
{
	switch ( index )
	{
	case 0:
		factory_sound_at( "top_spark1_warmup", factory_old_frame( ( 2378, 1901.5, 1469 ) ) );
		factory_sound_at( "top_spark2_warmup", factory_old_frame( ( 2378.5, 2033.5, 1469 ) ) );
		factory_sound_at( "top_spark3_warmup", factory_old_frame( ( 2242, 2030, 1469 ) ) );
		factory_sound_at( "top_spark4_warmup", factory_old_frame( ( 2246, 1903, 1469 ) ) );
		factory_sound_at( "lever_pull_warmup", factory_old_frame( ( 2199, 1891.5, 1325 ) ) );
		level thread factory_sound_at_delayed( "pad_cooldown", ( 1261.96, 1275.22, 199 ), 2 );
		level thread factory_sound_at_delayed( "pad2l_cooldown", ( 1341.91, 1272.5, 230.268 ), 2 );
		level thread factory_sound_at_delayed( "pad2r_cooldown", ( 1187.61, 1276.14, 228.89 ), 2 );
		break;
	case 1:
		factory_sound_at( "top_spark1_warmup", factory_old_frame( ( 1277.5, -2423, 1429.5 ) ) );
		factory_sound_at( "top_spark2_warmup", factory_old_frame( ( 1274, -2555.5, 1429.5 ) ) );
		factory_sound_at( "top_spark3_warmup", factory_old_frame( ( 1420, -2550, 1429.5 ) ) );
		factory_sound_at( "top_spark4_warmup", factory_old_frame( ( 1415.5, -2424, 1429.5 ) ) );
		factory_sound_at( "lever_pull_warmup", factory_old_frame( ( 1428, -2416.5, 1338.5 ) ) );
		level thread factory_sound_at_delayed( "pad_cooldown", ( 301.049, -3195, 188.777 ), 2 );
		level thread factory_sound_at_delayed( "pad2r_cooldown", ( 383.125, -3209.61, 221.102 ), 2 );
		level thread factory_sound_at_delayed( "pad2l_cooldown", ( 217.579, -3215.59, 222.48 ), 2 );
		break;
	default:
		factory_sound_at( "top_spark1_warmup", factory_old_frame( ( -662.4, -346.8, 1467.5 ) ) );
		factory_sound_at( "top_spark2_warmup", factory_old_frame( ( -788.5, -332.7, 1467.5 ) ) );
		factory_sound_at( "top_spark3_warmup", factory_old_frame( ( -788.5, -494.2, 1467.5 ) ) );
		factory_sound_at( "top_spark4_warmup", factory_old_frame( ( -663.5, -481.3, 1467.5 ) ) );
		factory_sound_at( "lever_pull_warmup", factory_old_frame( ( -663.1, -469.2, 1376.5 ) ) );
		level thread factory_sound_at_delayed( "pad_cooldown", ( -1782.5, -1114.83, 229.734 ), 2 );
		level thread factory_sound_at_delayed( "pad2r_cooldown", ( -1783.9, -1195.21, 270.891 ), 2 );
		level thread factory_sound_at_delayed( "pad2l_cooldown", ( -1782.64, -1022.24, 272.269 ), 2 );
		break;
	}

	// The homepad (the mainframe), the same for every pad.
	wait( 2 );
	home_spark = factory_old_frame( ( 1063, 1176.5, 1375.5 ) );
	factory_sound_at( "top_spark1_warmup", home_spark );
	factory_sound_at( "top_spark2_warmup", home_spark );
	factory_sound_at( "top_spark3_warmup", home_spark );
	factory_sound_at( "top_spark4_warmup", home_spark );
	factory_sound_at( "pad2r_warmup", factory_old_frame( ( 891.5, 1081, 1166.5 ) ) );
	factory_sound_at( "pad2r_cooldown", factory_old_frame( ( 891.5, 1081, 1166.5 ) ) );
	factory_sound_at( "pad2l_warmup", factory_old_frame( ( 1092, 1083.5, 1166.5 ) ) );
	factory_sound_at( "pad2l_cooldown", factory_old_frame( ( 1092, 1083.5, 1166.5 ) ) );
	factory_sound_at( "pad_warmup", factory_old_frame( ( 992, 1237.5, 1521.5 ) ) );
	factory_sound_at( "pad_cooldown", factory_old_frame( ( 992, 1237.5, 1521.5 ) ) );
}

// T5 teleporter_vo: the players within 64 units of the trigger say the
// line (tele_count 3 s after the countdown starts, tele_linkall at once);
// C1's one teleporter line per character stands for both.  The trigger's
// user is always a candidate (C1's pad and core triggers are big brushes
// whose centre is more than 64 units from much of the floor they cover).
// _zombiemode_audio lets one player speak at a time.
teleporter_vo_near( type, user )
{
	pre_wait = 0;
	if ( type == "countdown" )
		pre_wait = 3;

	speakers = [];
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( !isDefined( p ) )
			continue;
		if ( p == user || distance( p.origin, self.origin ) < 64 )
			speakers[ speakers.size ] = p;
	}
	if ( pre_wait > 0 )
		wait( pre_wait );
	for ( i = 0; i < speakers.size; i++ )
	{
		if ( isDefined( speakers[ i ] ) && speakers[ i ] maps\_zombiemode_audio::create_and_play_dialog( "teleporter" ) )
		{
			factory_sound_log( "vox teleporter (" + type + ") by " + speakers[ i ].name );
			return;
		}
	}
	factory_sound_log( "vox teleporter (" + type + "): no speaker said the line" );
}
