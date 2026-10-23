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
// 5 s), init_pack_door (the door drops 35 / 25 / 60 units as the pads link,
// its clip connects paths and goes), pad_manager, teleport_pad_think,
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
// (T5's first-person charge effect) is not a builtin either; the charge plays
// T5's own 2D teleport sounds instead.
//
// NOT TAKEN: the "teleporting too often after round 20" hound punishment
// (T5 special_dog_spawn; _zombiemode_ai_dogs has no out-of-round spawn), the
// teleporter VO (create_and_play_dialog level lines are not in C1), the
// black room flashback images and the FOV aftereffect (CSC).

#include maps\_zombiemode_utility;

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
// --------------------------------------------------------------------------
init_pack_door()
{
	door = getEnt( "pack_door", "targetname" );
	if ( !isDefined( door ) )
		return;

	flag_wait( "teleporter_pad_link_1" );
	door moveZ( -35, 1.5, 1 );
	door playSound( "packa_door_2" );
	door thread packa_door_reminder();
	wait( 2 );

	flag_wait( "teleporter_pad_link_2" );
	door moveZ( -25, 1.5, 1 );
	door playSound( "packa_door_2" );
	wait( 2 );

	flag_wait( "teleporter_pad_link_3" );
	door moveZ( -60, 1.5, 1 );
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

	maps\zm_factory_sh::factory_exploder( 202 + index * 100 );
	maps\zm_factory_sh::factory_exploder( 105 );

	self thread teleport_2d_audio();
	self thread teleport_nuke( 20, 300 );

	wait( level.teleport_delay );
	self notify( "fx_done" );

	maps\zm_factory_sh::factory_exploder( 201 + index * 100 );

	self teleport_players();

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
				players[ i ] playLocalSound( "teleport_2d_fnt" );
				players[ i ] playLocalSound( "teleport_2d_rear" );
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

		p.teleport_origin = spawn( "script_origin", p.origin );
		p.teleport_origin.angles = p.angles;
		p linkTo( p.teleport_origin );
		p.teleport_origin.origin = desired;
		p freezeControls( true );
		wait_network_frame();
		if ( isDefined( p ) && isDefined( p.teleport_origin ) )
			p.teleport_origin.angles = image_room[ i ].angles;
	}

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
		}
		player freezeControls( false );
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

				if ( level.active_links == 3 )
				{
					maps\zm_factory_sh::factory_exploder( 101 );
					earthquake( 0.3, 2.0, trigger.origin, 3700 );
				}

				pad = getEnt( "trigger_teleport_pad_" + i, "targetname" );
				pad stop_countdown();
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

// T5 teleport_aftereffects: one at random.  T5's vision aftereffects run in
// CSC; the server plays them with the zone's own flare / zombie_turned
// visions for T5's 4 s.
teleport_aftereffects()
{
	switch ( randomInt( 4 ) )
	{
	case 0:
		self shellShock( "explosion", 4 );
		break;
	case 1:
		self shellShock( "electrocution", 4 );
		break;
	case 2:
		self thread teleport_aftereffect_vision( "flare" );
		break;
	default:
		self thread teleport_aftereffect_vision( "zombie_turned" );
		break;
	}
}

teleport_aftereffect_vision( vision )
{
	self endon( "disconnect" );
	self visionSetNakedForPlayer( vision, 0.5 );
	wait( 4 );
	restore = "zm_factory_sh";
	if ( isDefined( level.zm_vision ) )
		restore = level.zm_vision;
	self visionSetNakedForPlayer( restore, 1 );
}
