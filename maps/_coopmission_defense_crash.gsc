// coop_defense_mp_crash_tropical: "Crash Site", chapter 1 mission 2 of the
// CoDOL campaign (training -> coop_take_mp_fav_tropical -> this ->
// coop_take_mp_estate_tropical).
//
// The mission's level script does not ship (docs/CODOL_CAMPAIGN_MODE.md
// section 3.2); what ships is the addon MapEnts (the intro room's structs,
// Ghost's spawner and his flash / gun-throw points, the enemy groups, the
// office's flash window, claymore pickup and placement spots, the laptop,
// the crashed helicopter's objective struct and bomb site, the roof gathering
// trigger and the extraction helicopter's path), the zone's own localized
// objective strings (COOP_DEFENSE_MP_CRASH_TROPICAL_*), the common coop DSM
// download strings (PMC_DSM_*) and the voice aliases (crash_gst_*, Ghost;
// crash_pri_*, Price on the radio; crash_ortiz_*, crash_car1_*).
//
// The authority for the ORDER of events, who stands where and what is said
// when is a public gameplay recording of the retail CoDOL client playing
// this mission (user direction 2026-10-08), docs/CODOL_CAMPAIGN_MODE.md
// section M3.  The lines were placed by the subtitles C1 draws on screen,
// matched to the zone's localize entries and, through them, to the aliases
// (M3.3 says how the alias -> subtitle link was read from the zone).
// No audio was used.  Line durations are INFERRED from the subtitle length.
//
// Phases, in the recording's order:
//   0 intro: the title card; the rookie on the ground by the wreck with a
//     mercenary over him; blackout; the interrogation room (Ortiz on the
//     phone, four watchers); Ghost's flashbang; Ghost throws the rookie a
//     rifle ("catch -- in 141 nobody is left behind").
//   1 follow Ghost to Vazquez's office: out of the building and through the
//     town, fighting group after group.
//   2 the office: flashbangs from Ghost's pack, one through the door, clear
//     the ground floor, upstairs.
//   3 the download: pick up claymores, place two, start the DSM on the
//     laptop and hold the office while it copies the files.
//   4 back to the landing zone: the wreck is being searched; clear it, find
//     the explosives in the lamp store, plant them on the helicopter.
//   5 the roof: run to the roof, the flare, the wreck goes up, the
//     extraction helicopter arrives -- mission complete (_coopmission's
//     lobby return).
//
// Coop module (maps/, IW4 singleplayer namespace); names no multiplayer module.

#using_animtree ( "generic_human" );

SESSION()
{
	return "coop_defense_mp_crash_tropical";
}

is_mission()
{
	return getDvar( "codol_sessionMap" ) == SESSION();
}

// --------------------------------------------------------------------------
// Voice lines.  The alias is the zone's; the time is how long the subtitle
// stays up (INFERRED from its length: about five characters a second).
// --------------------------------------------------------------------------

line_time( alias )
{
	switch ( alias )
	{
	case "crash_car1_267":
	case "crash_car1_268":
	case "crash_car1_269":
		return 2.5;
	case "crash_ortiz_03":
		return 1.5;
	case "crash_ortiz_04":
		return 3.0;
	case "crash_ortiz_05":
		return 3.5;
	case "crash_gst_12":
		return 2.8;
	case "crash_gst_14":
		return 5.0;
	case "crash_gst_16":
	case "crash_gst_19":
	case "crash_gst_21":
	case "crash_gst_47":
	case "crash_gst_58":
	case "crash_gst_59":
	case "crash_gst_62":
	case "crash_gst_65":
	case "crash_gst_67":
	case "crash_gst_68":
	case "crash_gst_70":
	case "crash_gst_93":
	case "crash_gst_94":
	case "crash_gst_95":
	case "crash_gst_99":
	case "crash_gst_106":
		return 4.0;
	case "crash_pri_0293":
	case "crash_pri_0327":
	case "crash_pri_0339":
	case "crash_pri_0354":
	case "crash_pri_0391":
	case "crash_pri_0404":
	case "crash_pri_0408":
		return 5.0;
	}
	return 2.0;
}

// Ghost speaks from where he stands; Price is on the radio (his files are
// the _r radio takes), so he plays to every player.
say( speaker, alias, noWait )
{
	time = line_time( alias );
	if ( !soundExists( alias ) )
	{
		codolprintln( "CoDOL coop crash: no sound alias " + alias );
		if ( !isDefined( noWait ) || !noWait )
			wait( time + 0.3 );
		return;
	}
	if ( isSubStr( alias, "_pri_" ) )
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
			players[ i ] playLocalSound( alias );
	}
	else if ( isDefined( speaker ) )
		speaker playSound( alias );
	else
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
			players[ i ] playLocalSound( alias );
	}
	codolprintln( "CoDOL coop crash: line " + alias );
	if ( !isDefined( noWait ) || !noWait )
		wait( time + 0.3 );
}

// One-shot sounds, each checked first: a missing alias is a script runtime
// error, which ends the match.
sound_at( alias, origin )
{
	if ( soundExists( alias ) )
		playSoundAtPosition( alias, origin );
	else
		codolprintln( "CoDOL coop crash: no sound alias " + alias );
}

local_sound( alias )
{
	if ( soundExists( alias ) )
		self playLocalSound( alias );
}

ghost_say( alias, noWait )
{
	say( level.crash_ghost, alias, noWait );
}

// --------------------------------------------------------------------------
// Precache window (from _coopmode Callback_StartGameType).
// --------------------------------------------------------------------------

init()
{
	if ( !is_mission() )
		return;

	level.coop_mission_flow = "defense_crash";
	level.coop_mission_testcmd = ::test_command;
	// This script releases its own groups and draws its own markers.
	level.coop_mission_owns_groups = true;
	level.coop_mission_owns_markers = true;
	level.crash_phase = "intro";
	level.crash_objective_name = "";

	models = strTok( "body_hero_ghost_socom head_hero_ghost_soccom body_hero_ghost_socom_obj body_airport_assault_a_clean head_militia_a_wht " +
		"prop_suitcase_bomb prop_suitcase_bomb_obj prop_suitcase_bomb_bombsquad weapon_claymore_obj vehicle_blackhawk_minigun_hero tag_origin", " " );
	for ( i = 0; i < models.size; i++ )
		precacheModel( models[ i ] );

	precacheShader( "black" );
	precacheShader( "white" );
	precacheShader( "objpoint_default" );
	precacheShellShock( "flashbang_mp_scripted" );

	strings();

	level._effect[ "crash_flashbang" ] = loadfx( "explosions/flashbang" );
	level._effect[ "crash_flare" ] = loadfx( "misc/handflare_green" );
	level._effect[ "crash_flare_smoke" ] = loadfx( "smoke/signal_smoke_green" );
	level._effect[ "crash_heli_explosion" ] = loadfx( "explosions/helicopter_explosion" );
	level._effect[ "crash_heli_crash" ] = loadfx( "explosions/helicopter_crash" );

	// The rifle Ghost throws: the recording's M4A1 with the M203 and the
	// holographic sight (codo_weapon_m4a1tech_world is the authored throw
	// model, gun_to_throw).
	level.crash_rifle = maps\_coopmode::resolveWeapon( strTok( "m4m203_eotech_so m203_m4_eotech_so m4_grunt_so m16_basic_so", " " ) );
	level.crash_pistol = maps\_coopmode::resolveWeapon( strTok( "usp_silencer_so beretta_so", " " ) );
	level.crash_frag = maps\_coopmode::resolveWeapon( strTok( "fraggrenade_so", " " ) );
	level.crash_flash = maps\_coopmode::resolveWeapon( strTok( "flash_grenade_so", " " ) );
	level.crash_claymore = maps\_coopmode::resolveWeapon( strTok( "claymore_so", " " ) );
	codolprintln( "CoDOL coop crash: rifle " + name_of( level.crash_rifle ) + " pistol " + name_of( level.crash_pistol ) + " frag " +
		name_of( level.crash_frag ) + " flash " + name_of( level.crash_flash ) + " claymore " + name_of( level.crash_claymore ) );
}

name_of( v )
{
	if ( !isDefined( v ) )
		return "none";
	return v;
}

strings()
{
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_INTROSCREEN_LINE_0" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_INTROSCREEN_LINE_1" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_INTROSCREEN_LINE_2" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PAUSE_FOLLOW" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PICK_UP_FLASH" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_THROW_FLASH" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_GOTO_INTERIOR" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PICK_UP_CLAYMORE" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PLACE_CLAYMORES" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_START_DOWNLOAD" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_DEFEND_DOWNLOAD" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_RESUME_DOWNLOAD" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PAUSE_FOLLOW_LZ" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PAUSE_SCUTTLE" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_FIND_C4" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PLANT_C4" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PAUSE_ROOF" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_PICK_UP_FLASH" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_THROW_FLASHBANG" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_ADS_HINT" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_PICK_UP_CLAYMORE" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_PLACE_CLAYMORE" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_SIGNAL_STRENGTH" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_SIGNAL_LOST" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_DESTROY_HELI" );
	precacheString( &"COOP_DEFENSE_MP_CRASH_TROPICAL_SPRINT_HINT" );
	precacheString( &"PMC_DSM_FRAME" );
	precacheString( &"PMC_DSM_INIT" );
	precacheString( &"PMC_DSM_CONNECTING" );
	precacheString( &"PMC_DSM_LOGIN" );
	precacheString( &"PMC_DSM_LOCATE" );
	precacheString( &"PMC_DSM_PROGRESS" );
	precacheString( &"PMC_DSM_TOTALFILES" );
	precacheString( &"PMC_DOWNLOAD_COMPLETE" );
	precacheString( &"PMC_DOWNLOAD_INTERRUPTED" );
	precacheString( &"PMC_OBJECTIVE_ABORT_CODES" );
}

// --------------------------------------------------------------------------
// Entities.
// --------------------------------------------------------------------------

crash_struct( name )
{
	return maps\_coopvehicle::struct_by_targetname( name );
}

structs( name )
{
	list = [];
	if ( !isDefined( level.struct ) )
		return list;
	for ( i = 0; i < level.struct.size; i++ )
	{
		if ( isDefined( level.struct[ i ].targetname ) && level.struct[ i ].targetname == name )
			list[ list.size ] = level.struct[ i ];
	}
	return list;
}

// A point by targetname: a struct, else an entity.
point( name, fallback )
{
	s = crash_struct( name );
	if ( isDefined( s ) )
		return s.origin;
	e = getEnt( name, "targetname" );
	if ( isDefined( e ) )
		return e.origin;
	codolprintln( "CoDOL coop crash: no point " + name + ", using " + fallback );
	return fallback;
}

spawners( name )
{
	list = [];
	ents = getEntArray( name, "targetname" );
	for ( i = 0; i < ents.size; i++ )
	{
		if ( isDefined( ents[ i ].classname ) && getSubStr( ents[ i ].classname, 0, 12 ) == "actor_enemy_" &&
		     !isSubStr( ents[ i ].classname, "_dog" ) )
			list[ list.size ] = ents[ i ];
	}
	return list;
}

by_noteworthy( value )
{
	list = [];
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		if ( isDefined( ents[ i ].script_noteworthy ) && ents[ i ].script_noteworthy == value )
			list[ list.size ] = ents[ i ];
	}
	return list;
}

// --------------------------------------------------------------------------
// HUD: the objective line, the 3D markers, hints, the black screen.
// --------------------------------------------------------------------------

// The recording shows each new objective as one line of text at the top of
// the screen for a few seconds; the compass carries the marker.
objective( text, origin, ent )
{
	codolprintln( "CoDOL coop crash: objective " + level.crash_objective_name );
	if ( isDefined( level.crash_objective_hud ) )
		level.crash_objective_hud destroy();
	hud = newHudElem();
	hud.alignX = "center";
	hud.alignY = "middle";
	hud.horzAlign = "center";
	hud.vertAlign = "top";
	hud.x = 0;
	hud.y = 70;
	hud.font = "objective";
	hud.fontScale = 1.6;
	hud.color = ( 0.55, 0.8, 1 );
	hud.foreground = true;
	hud.hidewheninmenu = true;
	hud.alpha = 1;
	hud setText( text );
	level.crash_objective_hud = hud;
	hud thread objective_fade();

	objective_delete( 0 );
	if ( isDefined( ent ) )
	{
		objective_add( 0, "active", ent.origin );
		objective_onEntity( 0, ent );
	}
	else if ( isDefined( origin ) )
		objective_add( 0, "active", origin );
	marker( origin, ent );
}

objective_fade()
{
	self endon( "death" );
	wait( 5 );
	if ( !isDefined( self ) )
		return;
	self fadeOverTime( 1 );
	self.alpha = 0;
}

// The glowing 3D marker of the objective (the recording's floating label and
// distance; the label text itself is the objective line above).
marker( origin, ent )
{
	if ( isDefined( level.crash_marker ) )
		level.crash_marker destroy();
	level.crash_marker = undefined;
	if ( !isDefined( origin ) && !isDefined( ent ) )
		return;
	wp = newHudElem();
	if ( isDefined( ent ) )
	{
		wp.x = ent.origin[ 0 ];
		wp.y = ent.origin[ 1 ];
		wp.z = ent.origin[ 2 ] + 80;
		wp setTargetEnt( ent );
	}
	else
	{
		wp.x = origin[ 0 ];
		wp.y = origin[ 1 ];
		wp.z = origin[ 2 ] + 24;
	}
	wp.alpha = 0.9;
	wp.hidewheninmenu = true;
	wp setShader( "objpoint_default", 8, 8 );
	wp setWaypoint( true, true );
	level.crash_marker = wp;
}

objective_clear()
{
	objective_delete( 0 );
	marker();
	level.crash_objective_name = "";
}

// A key hint at the bottom middle (the recording's "press ..." lines).
hint( text, time )
{
	if ( isDefined( level.crash_hint ) )
		level.crash_hint destroy();
	hud = newHudElem();
	hud.alignX = "center";
	hud.alignY = "middle";
	hud.horzAlign = "center";
	hud.vertAlign = "middle";
	hud.x = 0;
	hud.y = 60;
	hud.font = "default";
	hud.fontScale = 1.4;
	hud.foreground = true;
	hud.hidewheninmenu = true;
	hud setText( text );
	level.crash_hint = hud;
	if ( isDefined( time ) )
		hud thread hint_expire( time );
}

hint_expire( time )
{
	self endon( "death" );
	wait( time );
	if ( isDefined( self ) )
		self destroy();
}

hint_clear()
{
	if ( isDefined( level.crash_hint ) )
		level.crash_hint destroy();
	level.crash_hint = undefined;
}

black_screen( player, alpha, time )
{
	if ( !isDefined( player.crash_black ) )
	{
		b = newClientHudElem( player );
		b.x = 0;
		b.y = 0;
		b.horzAlign = "fullscreen";
		b.vertAlign = "fullscreen";
		b.foreground = false;
		b.sort = -10;
		b.alpha = 0;
		b setShader( "black", 640, 480 );
		player.crash_black = b;
	}
	if ( isDefined( time ) && time > 0 )
		player.crash_black fadeOverTime( time );
	player.crash_black.alpha = alpha;
}

black_all( alpha, time )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		black_screen( players[ i ], alpha, time );
}

// The opening card: the mission's name, the date, the place.
intro_card()
{
	lines = [];
	for ( i = 0; i < 3; i++ )
	{
		hud = newHudElem();
		hud.alignX = "left";
		hud.alignY = "middle";
		hud.horzAlign = "left";
		hud.vertAlign = "middle";
		hud.x = 60;
		hud.y = -40 + i * 18;
		hud.font = "objective";
		hud.fontScale = 1.4;
		hud.color = ( 0.55, 0.85, 1 );
		hud.foreground = true;
		hud.sort = 5;
		lines[ i ] = hud;
	}
	lines[ 0 ] setText( &"COOP_DEFENSE_MP_CRASH_TROPICAL_INTROSCREEN_LINE_0" );
	lines[ 1 ] setText( &"COOP_DEFENSE_MP_CRASH_TROPICAL_INTROSCREEN_LINE_1" );
	lines[ 2 ] setText( &"COOP_DEFENSE_MP_CRASH_TROPICAL_INTROSCREEN_LINE_2" );
	return lines;
}

// --------------------------------------------------------------------------
// The flow's published wait (for the log and the probe harness).
// --------------------------------------------------------------------------

step( kind, value, value2 )
{
	s = spawnStruct();
	s.kind = kind;
	s.value = value;
	s.value2 = value2;
	level.crash_step = s;
	text = kind;
	if ( kind == "near" || kind == "use" || kind == "flash" || kind == "download" || kind == "roof" )
		text += " " + value;
	else if ( kind == "kill" )
		text += " " + value;
	codolprintln( "CoDOL coop crash: waiting for " + text );
}

any_player_near( origin, radius )
{
	players = maps\_coopmission::living_players();
	for ( i = 0; i < players.size; i++ )
	{
		if ( distance( players[ i ].origin, origin ) <= radius )
			return players[ i ];
	}
	return undefined;
}

wait_player_near( origin, radius, timeout )
{
	step( "near", origin, radius );
	waited = 0;
	while ( waited < timeout )
	{
		p = any_player_near( origin, radius );
		if ( isDefined( p ) )
			return p;
		wait( 0.25 );
		waited += 0.25;
	}
	codolprintln( "CoDOL coop crash: nobody came near " + origin + " in " + timeout + " s" );
	return undefined;
}

// Hold the use key within `radius` of a point for `hold` seconds (the
// recording's "hold F" pickups), or the map's own trigger_use.
wait_use( origin, radius, hold, timeout, trigger, bar )
{
	step( "use", origin, radius );
	if ( isDefined( trigger ) )
		trigger thread use_trigger_relay();
	level.crash_force_use = undefined;
	waited = 0;
	held = [];
	while ( waited < timeout )
	{
		if ( isDefined( level.crash_force_use ) )
		{
			who = level.crash_force_use;
			level.crash_force_use = undefined;
			if ( isDefined( trigger ) )
				trigger notify( "crash_relay_stop" );
			return who;
		}
		if ( isDefined( trigger ) && isDefined( trigger.crash_used_by ) )
		{
			who = trigger.crash_used_by;
			trigger.crash_used_by = undefined;
			trigger notify( "crash_relay_stop" );
			return who;
		}
		players = maps\_coopmission::living_players();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			n = p getEntityNumber();
			if ( !isDefined( held[ n ] ) )
				held[ n ] = 0;
			if ( distance( p.origin, origin ) <= radius && p useButtonPressed() )
				held[ n ] += 0.05;
			else
				held[ n ] = 0;
			if ( isDefined( bar ) && bar )
				p progress_bar( held[ n ] / hold );
			if ( held[ n ] >= hold )
			{
				if ( isDefined( bar ) && bar )
					p progress_bar( -1 );
				if ( isDefined( trigger ) )
					trigger notify( "crash_relay_stop" );
				return p;
			}
		}
		wait( 0.05 );
		waited += 0.05;
	}
	if ( isDefined( trigger ) )
		trigger notify( "crash_relay_stop" );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] progress_bar( -1 );
	codolprintln( "CoDOL coop crash: nobody used " + origin + " in " + timeout + " s" );
	return undefined;
}

use_trigger_relay()
{
	self endon( "crash_relay_stop" );
	for ( ;; )
	{
		self waittill( "trigger", who );
		if ( isDefined( who ) && isPlayer( who ) )
		{
			self.crash_used_by = who;
			return;
		}
	}
}

// The planting bar (the recording's "planting..." bar over the suitcase).
progress_bar( fraction )
{
	if ( fraction < 0 || fraction == 0 )
	{
		if ( isDefined( self.crash_bar ) )
		{
			self.crash_bar destroy();
			self.crash_bar_bg destroy();
			self.crash_bar = undefined;
		}
		return;
	}
	if ( !isDefined( self.crash_bar ) )
	{
		bg = newClientHudElem( self );
		bg.alignX = "left";
		bg.alignY = "middle";
		bg.horzAlign = "center";
		bg.vertAlign = "middle";
		bg.x = -60;
		bg.y = -60;
		bg.alpha = 0.5;
		bg setShader( "black", 120, 6 );
		fg = newClientHudElem( self );
		fg.alignX = "left";
		fg.alignY = "middle";
		fg.horzAlign = "center";
		fg.vertAlign = "middle";
		fg.x = -60;
		fg.y = -60;
		fg.sort = 1;
		fg setShader( "white", 1, 6 );
		self.crash_bar = fg;
		self.crash_bar_bg = bg;
	}
	w = int( 120 * fraction );
	if ( w < 1 )
		w = 1;
	if ( w > 120 )
		w = 120;
	self.crash_bar setShader( "white", w, 6 );
}

// --------------------------------------------------------------------------
// Players: where they (re)spawn and what they carry, by phase.
// --------------------------------------------------------------------------

// Respawn points the addon authors for each stretch of the mission.
checkpoint_points()
{
	names = [];
	switch ( level.crash_phase )
	{
	case "intro":
	case "route":
		names[ 0 ] = "post_intro_checkpoint_1";
		names[ 1 ] = "post_intro_checkpoint_2";
		break;
	case "office":
		names[ 0 ] = "pre_download_checkpoint_1";
		names[ 1 ] = "pre_download_checkpoint_2";
		break;
	case "download":
		names[ 0 ] = "post_download_checkpoint_1";
		names[ 1 ] = "post_download_checkpoint_2";
		break;
	case "heli":
		names[ 0 ] = "player0_crash_end";
		names[ 1 ] = "player1_crash_end";
		break;
	case "roof":
		names[ 0 ] = "4_coop_start_01";
		names[ 1 ] = "4_coop_start_02";
		break;
	}
	pts = [];
	for ( i = 0; i < names.size; i++ )
	{
		s = crash_struct( names[ i ] );
		if ( !isDefined( s ) )
			s = getEnt( names[ i ], "targetname" );
		if ( isDefined( s ) )
			pts[ pts.size ] = s;
	}
	return pts;
}

players_watch()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread player_think();
	}
}

player_think()
{
	self endon( "disconnect" );
	wait( 0.05 );
	if ( isAlive( self ) && self.sessionstate == "playing" )
		self player_spawned();
	for ( ;; )
	{
		self waittill( "spawned_player" );
		self player_spawned();
	}
}

player_spawned()
{
	if ( !isDefined( self.crash_spawns ) )
		self.crash_spawns = 0;
	self.crash_spawns++;
	if ( level.crash_phase == "intro" && !isDefined( level.crash_intro_done ) )
	{
		// The captive: no weapons until Ghost throws the rifle.
		self takeAllWeapons();
		self setOffhandEquipment( "none", "none", "none" );
		self freezeControls( true );
		return;
	}
	pts = checkpoint_points();
	if ( pts.size > 0 && ( self.crash_spawns > 1 || level.crash_phase != "route" ) )
	{
		p = pts[ self getEntityNumber() % pts.size ];
		angles = ( 0, 0, 0 );
		if ( isDefined( p.angles ) )
			angles = p.angles;
		self setOrigin( p.origin );
		self setPlayerAngles( angles );
		codolprintln( "CoDOL coop crash: " + self.name + " respawns at " + p.origin + " (" + level.crash_phase + ")" );
	}
	self give_kit();
}

// The rookie's kit after the intro: Ghost's rifle, a pistol, frags; the
// flashbangs, claymores once they are handed out.
give_kit()
{
	self takeAllWeapons();
	if ( isDefined( level.crash_pistol ) )
	{
		self giveWeapon( level.crash_pistol );
		self giveMaxAmmo( level.crash_pistol );
	}
	if ( isDefined( level.crash_rifle ) )
	{
		self giveWeapon( level.crash_rifle );
		self giveMaxAmmo( level.crash_rifle );
		self setSpawnWeapon( level.crash_rifle );
		self switchToWeapon( level.crash_rifle );
	}
	self equip_offhands();
	self freezeControls( false );
	self enableWeapons();
}

equip_offhands()
{
	lethal = "none";
	special = "none";
	tactical = "none";
	if ( isDefined( level.crash_frag ) )
	{
		lethal = level.crash_frag;
		self giveWeapon( lethal );
		self setWeaponAmmoClip( lethal, 3 );
	}
	if ( isDefined( level.crash_have_flash ) && isDefined( level.crash_flash ) )
	{
		special = level.crash_flash;
		self giveWeapon( special );
		self giveMaxAmmo( special );
	}
	if ( isDefined( level.crash_have_claymore ) && isDefined( level.crash_claymore ) )
	{
		tactical = level.crash_claymore;
		self giveWeapon( tactical );
		self giveMaxAmmo( tactical );
	}
	self setOffhandEquipment( lethal, special, tactical );
}

freeze_all( on )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] freezeControls( on );
}

// --------------------------------------------------------------------------
// Ghost: a friendly soldier-species actor (the training cast's template),
// invulnerable, who leads and shoots.
// --------------------------------------------------------------------------

spawn_ghost( origin, angles )
{
	// The addon's own Ghost spawner classname: aitype\ally_hero_ghost_socom_so,
	// character\character_hero_seal_soccom_ghost.
	kind = spawnStruct();
	kind.role = "ar";
	kind.health = 1000;
	kind.accuracy = 0.45;
	ai = maps\_coopmission::spawn_class_actor( "actor_ally_hero_ghost_socom_so", "allies", kind );
	if ( !isDefined( ai ) )
	{
		codolprintln( "CoDOL coop crash: no actor for Ghost" );
		return undefined;
	}
	ai forceTeleport( origin, angles );
	// His rifle in his hands, so his fire has a flash and a sound.
	ai maps\_coopmission::arm_soldier();
	ai setCanDamage( false );
	ai.ignoreall = true;
	ai.crash_moving = false;
	ai.goalradius = 48;
	ai.coop_name = "Ghost";
	ai.targetname = "crash_ghost";
	// He runs when he moves (soldier_move's run, not the patrol walk) and
	// stands rifle up (soldier_stop's cast pose).  exposed_aim_5 is a whole
	// pose; exposed_idle_alert_v1 is built additive in common_gametype_coop
	// (its xanim carries the additive flag, like the exposed_shoot_* set), so
	// as his only knob it drew him T-posed in C1 (user's playthrough, at the
	// office corner, the download room and the roof).
	ai.coop_hunting = true;
	ai.coop_cast = "ghost";
	ai.coop_idle_anim = %exposed_aim_5;
	ai setAnimKnobAll( %exposed_aim_5, %body, 1, 0.1, 1 );
	ai notify( "coop_cast_pose" );
	if ( !isDefined( level.coop_cast_list ) )
		level.coop_cast_list = [];
	level.coop_cast_list[ level.coop_cast_list.size ] = ai;
	level.crash_ghost = ai;
	ai thread ghost_fire_support();
	codolprintln( "CoDOL coop crash: Ghost (actor ent " + ai getEntityNumber() + ") at " + origin );
	return ai;
}

// Ghost walks to a point over the bot waypoint graph (setGoalPos, the native
// planner).  When the planner makes no progress for a while he is placed
// there instead, so the squad is never left without its lead.  Returns once
// he is within `radius`.
ghost_move( dest, what, radius, timeout )
{
	ai = level.crash_ghost;
	if ( !isDefined( ai ) )
		return;
	if ( !isDefined( radius ) )
		radius = 64;
	if ( !isDefined( timeout ) )
		timeout = 40;
	ai notify( "crash_ghost_move" );
	ai endon( "crash_ghost_move" );
	ai.crash_moving = true;
	ai orientMode( "face default" );
	ai setGoalPos( dest );
	start = ai.origin;
	last = ai.origin;
	still = 0;
	waited = 0;
	while ( waited < timeout )
	{
		wait( 0.5 );
		waited += 0.5;
		if ( !isDefined( ai ) )
			return;
		d = ai.origin - dest;
		if ( length( ( d[ 0 ], d[ 1 ], 0 ) ) <= radius && crash_abs( d[ 2 ] ) < 90 )
		{
			ai.crash_moving = false;
			codolprintln( "CoDOL coop crash: Ghost reached " + what + " " + dest + " from " + start + " in " + waited + " s" );
			return;
		}
		if ( distance( ai.origin, last ) < 12 )
			still += 0.5;
		else
			still = 0;
		last = ai.origin;
		if ( still >= 5 )
			break;
	}
	codolprintln( "CoDOL coop crash: Ghost could not walk to " + what + " " + dest + " (at " + ai.origin + " after " + waited +
		" s); placing him there" );
	ai forceTeleport( dest, ai.angles );
	ai setGoalPos( dest );
	ai.crash_moving = false;
}

crash_abs( v )
{
	if ( v < 0 )
		return 0 - v;
	return v;
}

// A scripted walk for Ghost where the bot waypoint graph has no nodes (the
// intro building's rooms and stairs): he rides an invisible mover along
// hand-placed points (codol_actorRide; actors cannot linkTo), playing the
// run.  The points were measured with _cooptest colmap / floors.
ghost_walk( points, what, speed )
{
	ai = level.crash_ghost;
	if ( !isDefined( ai ) )
		return;
	if ( !isDefined( speed ) )
		speed = 150;
	ai notify( "crash_ghost_move" );
	ai.crash_moving = true;
	ai orientMode( "face default" );
	mover = spawn( "script_model", ai.origin );
	mover setModel( "tag_origin" );
	mover.angles = ( 0, ai.angles[ 1 ], 0 );
	ai animMode( "nophysics" );
	ai codol_actorRide( mover, "tag_origin" );
	ai.coop_pose_anim = %run_lowready_f;
	ai notify( "coop_cast_pose" );
	for ( i = 0; i < points.size; i++ )
	{
		d = distance( mover.origin, points[ i ] );
		if ( d < 1 )
			continue;
		yaw = vectorToAngles( points[ i ] - mover.origin );
		mover rotateTo( ( 0, yaw[ 1 ], 0 ), 0.2 );
		t = d / speed;
		mover moveTo( points[ i ], t, 0, 0 );
		wait( t );
	}
	ai codol_actorRide();
	ai animMode( "none" );
	ai forceTeleport( mover.origin, mover.angles );
	ai setGoalPos( mover.origin );
	ai.coop_pose_anim = undefined;
	ai notify( "coop_cast_pose" );
	mover delete();
	ai.crash_moving = false;
	codolprintln( "CoDOL coop crash: Ghost walked " + what + " to " + ai.origin );
}

// The double door between the building's two ground-floor rooms (base map
// script_brushmodels price_door, hinged at their outer ends: MEASURED bounds
// x 1635..1671 and 1669..1705 at y -1784): Ghost opens it northward.
open_price_door()
{
	doors = getEntArray( "price_door", "targetname" );
	for ( i = 0; i < doors.size; i++ )
	{
		d = doors[ i ];
		lo = d getPointInBounds( -1, 0, 0 );
		hi = d getPointInBounds( 1, 0, 0 );
		// The hinge is the end nearer the origin; the door swings north.
		if ( abs_( d.origin[ 0 ] - lo[ 0 ] ) < abs_( d.origin[ 0 ] - hi[ 0 ] ) )
			d rotateYaw( 90, 0.6, 0.1, 0.2 );
		else
			d rotateYaw( -90, 0.6, 0.1, 0.2 );
		d notSolid();
	}
	if ( doors.size > 0 )
		sound_at( "door_wood_slow_open", doors[ 0 ].origin );
	codolprintln( "CoDOL coop crash: price_door opened (" + doors.size + " halves)" );
}

// Ghost's standing (or crouched) pose while he waits; both are whole poses
// from common_human, never the additive alert idles.
ghost_pose( pose )
{
	ai = level.crash_ghost;
	if ( !isDefined( ai ) )
		return;
	ai.coop_idle_anim = pose;
	ai notify( "coop_cast_pose" );
}

ghost_wait_player( radius, timeout )
{
	waited = 0;
	while ( !isDefined( any_player_near( level.crash_ghost.origin, radius ) ) && waited < timeout )
	{
		wait( 0.5 );
		waited += 0.5;
	}
}

// Where the squad is: Ghost, else the first living player.  The route's
// groups are sent there, so they come out of the houses and engage (IW4
// sends them to their goal volumes; those do not ship with a script here).
ghost_spot()
{
	if ( isDefined( level.crash_ghost ) )
		return level.crash_ghost.origin;
	players = maps\_coopmission::living_players();
	if ( players.size > 0 )
		return players[ 0 ].origin;
	return undefined;
}

abs_( v )
{
	if ( v < 0 )
		return 0 - v;
	return v;
}

// Ghost moves on once a player is with him: the recording's "follow" lead,
// walking a few points at a time and waiting at each for the squad.
ghost_lead( points, what, waitRadius )
{
	if ( !isDefined( waitRadius ) )
		waitRadius = 450;
	for ( i = 0; i < points.size; i++ )
	{
		ghost_move( points[ i ], what + " " + i );
		waited = 0;
		while ( !isDefined( any_player_near( level.crash_ghost.origin, waitRadius ) ) && waited < 45 )
		{
			wait( 0.5 );
			waited += 0.5;
		}
	}
}

// Ghost's rifle: he takes the nearest enemy he can see inside 1400 units
// and fires short bursts at him (the same codol_actorShoot round the enemy
// soldiers fire, soldier_combat).  He holds fire with a player in the line.
ghost_fire_support()
{
	self endon( "death" );
	level endon( "coop_mission_over" );
	for ( ;; )
	{
		wait( 0.6 + randomFloat( 0.8 ) );
		if ( isDefined( level.crash_ghost_hold_fire ) && level.crash_ghost_hold_fire )
			continue;
		target = ghost_target();
		if ( !isDefined( target ) )
			continue;
		self.crash_target = target;
		if ( !self.crash_moving )
		{
			yaw = vectorToAngles( target.origin - self.origin );
			self orientMode( "face angle", yaw[ 1 ] );
		}
		self setAnim( %add_fire, 1, 0.1, 1 );
		shots = 3 + randomInt( 3 );
		for ( i = 0; i < shots; i++ )
		{
			if ( !isDefined( target ) || !isAlive( target ) )
				break;
			ghost_shot( target );
			wait( 0.1 );
		}
		self clearAnim( %add_fire, 0.2 );
	}
}

ghost_target()
{
	if ( !isDefined( level.coop_enemies ) )
		return undefined;
	eye = self.origin + ( 0, 0, 60 );
	best = undefined;
	bestDist = 1400;
	for ( i = 0; i < level.coop_enemies.size; i++ )
	{
		e = level.coop_enemies[ i ];
		if ( !isDefined( e ) || !isAlive( e ) )
			continue;
		if ( isDefined( e.crash_unaware ) && e.crash_unaware && !isDefined( level.crash_ghost_free_fire ) )
			continue;
		d = distance( self.origin, e.origin );
		if ( d >= bestDist )
			continue;
		if ( !can_see( eye, e, self ) )
			continue;
		best = e;
		bestDist = d;
	}
	return best;
}

// A clear line to an actor: nothing but the actor himself in the way.
can_see( eye, e, ignore )
{
	t = bulletTrace( eye, e.origin + ( 0, 0, 48 ), true, ignore );
	if ( t[ "fraction" ] >= 0.99 )
		return true;
	return isDefined( t[ "entity" ] ) && t[ "entity" ] == e;
}

ghost_shot( target )
{
	if ( !isDefined( self.crash_shots ) )
		self.crash_shots = 0;
	self.crash_shots++;
	if ( self.crash_shots == 1 || self.crash_shots % 50 == 0 )
		codolprintln( "CoDOL coop crash: Ghost has fired " + self.crash_shots + " rounds (target ent " + target getEntityNumber() + " at " +
			int( distance( self.origin, target.origin ) ) + ")" );
	aim = target.origin + ( 0, 0, 44 );
	chest = self.origin + ( 0, 0, 52 );
	dir = vectorNormalize( aim - chest );
	start = chest + dir * 16;
	if ( isDefined( self.coop_weapon_model ) )
		start = self getTagOrigin( "tag_flash" );
	trace = bulletTrace( start, aim, true, self );
	if ( isDefined( trace[ "entity" ] ) && trace[ "entity" ] != target && ( isPlayer( trace[ "entity" ] ) || isAI( trace[ "entity" ] ) ) )
		return;
	if ( randomFloat( 1 ) > 0.3 )
	{
		side = vectorNormalize( ( 0 - dir[ 1 ], dir[ 0 ], 0 ) );
		miss = 18 + randomFloat( 30 );
		if ( randomInt( 2 ) )
			miss = 0 - miss;
		aim = aim + side * miss;
	}
	end = start + vectorNormalize( aim - start ) * ( distance( start, aim ) + 256 );
	weapon = self.coop_weapon;
	if ( !isDefined( weapon ) || weapon == "" )
		weapon = "m16_basic_so";
	self maps\_coopmission::actor_fire_round( weapon, start, end );
}

// --------------------------------------------------------------------------
// Enemies: the addon's groups through _coopmission's soldier spawn.
// --------------------------------------------------------------------------

// Spawns every soldier of a group (each spawner's count, at most `limit`
// in all).  `hunt` sends them at the players / a point.
spawn_group( name, limit, goal, noHunt )
{
	list = spawners( name );
	ais = [];
	for ( i = 0; i < list.size; i++ )
	{
		if ( isDefined( limit ) && ais.size >= limit )
			break;
		// _coopmission's soldier_think leaves a coop_no_hunt spawner's men to
		// this script (the unaware groups hold their idles).
		if ( isDefined( noHunt ) && noHunt )
			list[ i ].coop_no_hunt = true;
		ai = maps\_coopmission::spawn_soldier( list[ i ] );
		// The actor budget is full: wait a little for a slot rather than
		// lose the soldier (an earlier group's men may still be dying).
		for ( tries = 0; !isDefined( ai ) && tries < 8 && level.coop_alive >= level.coop_max_alive && !level.coop_mission_over; tries++ )
		{
			wait( 1 );
			ai = maps\_coopmission::spawn_soldier( list[ i ] );
		}
		if ( !isDefined( ai ) )
			continue;
		ai.crash_group = name;
		// Unaware from the first frame: a soldier that acquires an enemy
		// (or inherits one from his squad) before ignoreall is set keeps
		// chasing him (MEASURED: the wreck's searchers ran for Ghost).
		if ( isDefined( noHunt ) && noHunt )
		{
			ai.ignoreall = true;
			ai.crash_unaware = true;
		}
		if ( isDefined( goal ) )
		{
			ai.goalradius = 300;
			ai setGoalPos( goal );
		}
		ais[ ais.size ] = ai;
		wait( 0.1 );
	}
	codolprintln( "CoDOL coop crash: group " + name + " spawned " + ais.size + " of " + list.size );
	return ais;
}

// The men an earlier stretch left behind (on roofs, upper floors, stranded
// by the waypoint graph) are out of the fight once the squad has moved on:
// they are taken out of the scene so the actor budget serves the next
// stretch.  `except` keeps one group.
clear_leftovers( dist, except )
{
	n = 0;
	for ( i = 0; i < level.coop_enemies.size; i++ )
	{
		e = level.coop_enemies[ i ];
		if ( !isDefined( e ) || !isAlive( e ) )
			continue;
		if ( isDefined( except ) && isDefined( e.crash_group ) && isSubStr( e.crash_group, except ) )
			continue;
		d = maps\_coopmission::nearest_player_distance( e.origin );
		if ( isDefined( d ) && d < dist )
			continue;
		remove_actor( e );
		n++;
	}
	if ( n > 0 )
		codolprintln( "CoDOL coop crash: " + n + " leftover enemies taken out of the scene (" + dist + " from the players)" );
}

append( a, b )
{
	for ( i = 0; i < b.size; i++ )
		a[ a.size ] = b[ i ];
	return a;
}

alive_count( ais )
{
	n = 0;
	for ( i = 0; i < ais.size; i++ )
	{
		if ( isDefined( ais[ i ] ) && isAlive( ais[ i ] ) )
			n++;
	}
	return n;
}

// Until the group is dead; after `timeout` the last ones are marked on the
// compass and they come for the players.
wait_group_dead( ais, what, timeout, allowLeft, moveOn )
{
	// allowLeft: the squad moves on with this many still up (the men on the
	// upper floors and roofs, who keep firing from there, as in the
	// recording, where Ghost leads on through the town under fire).
	if ( !isDefined( allowLeft ) )
		allowLeft = 0;
	step( "kill", what );
	level.crash_kill_list = ais;
	waited = 0;
	marked = false;
	pruned = false;
	while ( alive_count( ais ) > allowLeft )
	{
		wait( 0.5 );
		waited += 0.5;
		// moveOn: Ghost leads on after this long whoever is still up (the
		// rooftop and upper-floor shooters, who stay and keep firing).
		if ( isDefined( moveOn ) && waited >= moveOn )
		{
			codolprintln( "CoDOL coop crash: " + what + ": moving on with " + alive_count( ais ) + " still up after " + waited + " s" );
			break;
		}
		if ( !marked && waited >= timeout )
		{
			// The last few come for the players and are marked on the
			// compass (IW4 _pmc's last enemies, _coopmission's markers).
			marked = true;
			players = maps\_coopmission::living_players();
			slot = 0;
			for ( i = 0; i < ais.size; i++ )
			{
				if ( !isDefined( ais[ i ] ) || !isAlive( ais[ i ] ) || players.size <= 0 )
					continue;
				ais[ i ] wake();
				ais[ i ].ignoreall = false;
				ais[ i ].goalradius = 200;
				ais[ i ] setGoalPos( players[ randomInt( players.size ) ].origin );
				if ( slot < 4 )
				{
					objective_add( 4 + slot, "active", ais[ i ].origin );
					objective_onEntity( 4 + slot, ais[ i ] );
					slot++;
				}
				codolprintln( "CoDOL coop crash: " + what + ": one left at " + ais[ i ].origin );
			}
			codolprintln( "CoDOL coop crash: " + what + ": " + alive_count( ais ) + " left after " + timeout + " s, sent at the players" );
		}
		// One stuck far from everyone (the waypoint graph strands a soldier
		// now and then) has fled: he is taken out of the scene.
		if ( !pruned && waited >= timeout * 1.6 )
		{
			pruned = true;
			for ( i = 0; i < ais.size; i++ )
			{
				if ( !isDefined( ais[ i ] ) || !isAlive( ais[ i ] ) )
					continue;
				d = maps\_coopmission::nearest_player_distance( ais[ i ].origin );
				if ( isDefined( d ) && d > 900 )
				{
					codolprintln( "CoDOL coop crash: " + what + ": a straggler at " + ais[ i ].origin + " (" + int( d ) + " from the players) is removed" );
					remove_actor( ais[ i ] );
				}
			}
		}
		if ( waited >= timeout * 2.2 )
		{
			codolprintln( "CoDOL coop crash: " + what + " not cleared after " + waited + " s; the rest are removed" );
			for ( i = 0; i < ais.size; i++ )
				remove_actor( ais[ i ] );
			break;
		}
	}
	for ( i = 0; i < 4; i++ )
		objective_delete( 4 + i );
	codolprintln( "CoDOL coop crash: " + what + " cleared in " + waited + " s" );
}

// An unaware soldier: holds an idle from the common animlists every coop
// client loads, ignores everyone, until a player comes close or shoots him
// (the office's smokers and phone man, the wreck's searchers).
unaware( idle, wakeRadius )
{
	self endon( "death" );
	self.ignoreall = true;
	self.crash_unaware = true;
	self.coop_cast = "unaware";
	self.coop_idle_anim = idle;
	self setAnimKnobAll( idle, %body, 1, 0.2, 1 );
	self notify( "coop_cast_pose" );
	health = self.health;
	for ( ;; )
	{
		wait( 0.25 );
		if ( isDefined( level.crash_alert ) && isDefined( self.crash_group ) && isDefined( level.crash_alert[ self.crash_group ] ) )
			break;
		if ( self.health < health )
			break;
		if ( isDefined( any_player_near( self.origin, wakeRadius ) ) )
			break;
	}
	self wake();
}

wake()
{
	if ( !isDefined( self.crash_unaware ) )
		return;
	self.crash_unaware = undefined;
	self.coop_cast = undefined;
	self.coop_idle_anim = undefined;
	self.ignoreall = false;
	self setAnimKnobAll( %exposed_aim_5, %body, 1, 0.2, 1 );
	self notify( "coop_cast_pose" );
	if ( isDefined( self.crash_group ) )
	{
		if ( !isDefined( level.crash_alert ) )
			level.crash_alert = [];
		level.crash_alert[ self.crash_group ] = true;
	}
}

// --------------------------------------------------------------------------
// Entry point (threaded from _coopmode Callback_StartGameType).
// --------------------------------------------------------------------------

main()
{
	if ( !is_mission() )
		return;
	level thread players_watch();
	level thread maps\_cooptraining::show_nametags();
	prepare_props();

	while ( maps\_coopmission::living_players().size <= 0 )
		wait( 0.25 );
	// _coopmission's own main() has run its init() by now.
	level.coop_max_alive = 14;
	wait( 1 );
	level.crash_start_time = getTime();

	if ( getDvarInt( "scr_coop_crash_skip" ) > 0 )
		skip_to( getDvarInt( "scr_coop_crash_skip" ) );
	else
	{
		phase_intro();
		phase_route();
	}
	if ( !level.coop_mission_over && level.crash_skip <= 2 )
		phase_office();
	if ( !level.coop_mission_over && level.crash_skip <= 3 )
		phase_download();
	if ( !level.coop_mission_over && level.crash_skip <= 4 )
		phase_heli();
	if ( !level.coop_mission_over )
		phase_roof();
}

// Developer start points (+set scr_coop_crash_skip 2..5), the way IW4
// levels have start points.
skip_to( n )
{
	level.crash_skip = n;
	level.crash_intro_done = true;
	ghostSpot = point( "pre_download_checkpoint_price", ( -404, 2120, 268 ) );
	switch ( n )
	{
	case 3:
		level.crash_have_flash = true;
		ghostSpot = point( "post_download_checkpoint_price", ( -668, 2156, 400 ) );
		level.crash_phase = "download";
		break;
	case 4:
		level.crash_have_flash = true;
		level.crash_have_claymore = true;
		ghostSpot = point( "post_download_checkpoint_price", ( -668, 2156, 400 ) );
		level.crash_phase = "download";
		break;
	case 5:
		ghostSpot = ( 1488, 664, 608 );
		level.crash_phase = "heli";
		break;
	default:
		level.crash_phase = "office";
		break;
	}
	spawn_ghost( ghostSpot, ( 0, 0, 0 ) );
	pts = checkpoint_points();
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( pts.size > 0 )
			players[ i ] setOrigin( pts[ i % pts.size ].origin );
		players[ i ] give_kit();
	}
	codolprintln( "CoDOL coop crash: skipped to phase " + n );
}

// The office's props start as the recording shows them.
prepare_props()
{
	level.crash_skip = 0;
	glow = getEnt( "the_laptop_obj", "targetname" );
	if ( isDefined( glow ) )
		glow hide();
	place = getEntArray( "me0_auto3367", "targetname" );
	place = append( place, getEntArray( "me0_auto3368", "targetname" ) );
	for ( i = 0; i < place.size; i++ )
		place[ i ] hide();
	site = getEnt( "bomb_plant_site", "targetname" );
	if ( isDefined( site ) )
		site hide();
	// The helicopter at the end flies in; its authored entity stays hidden.
	chopper = getEnt( "chopper_end", "targetname" );
	if ( isDefined( chopper ) )
		chopper hide();
}

// --------------------------------------------------------------------------
// 0. The intro.
// --------------------------------------------------------------------------

phase_intro()
{
	level.crash_phase = "intro";
	freeze_all( true );
	black_all( 1, 0 );
	card = intro_card();
	wait( 4.5 );
	for ( i = 0; i < card.size; i++ )
	{
		card[ i ] fadeOverTime( 0.8 );
		card[ i ].alpha = 0;
	}

	// The rookie on the ground by the wreck, a mercenary over him (the
	// addon's intro_dragger at drag_players).
	drag = crash_struct( "drag_players" );
	dragOrigin = ( 605, 336, 143 );
	dragYaw = 210;
	if ( isDefined( drag ) )
	{
		dragOrigin = drag.origin;
		dragYaw = drag.angles[ 1 ];
	}
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		players[ i ] setOrigin( dragOrigin + ( i * 40, 0, 0 ) );
		players[ i ] setPlayerAngles( ( -55, dragYaw, 0 ) );
		players[ i ] setStance( "prone" );
	}
	dragger = undefined;
	list = spawners( "intro_dragger" );
	if ( list.size > 0 )
	{
		list[ 0 ].coop_no_hunt = true;
		dragger = maps\_coopmission::spawn_soldier( list[ 0 ] );
		if ( isDefined( dragger ) )
		{
			fwd = anglesToForward( ( 0, dragYaw, 0 ) );
			dragger forceTeleport( dragOrigin + fwd * 44, ( 0, dragYaw + 180, 0 ) );
			dragger.ignoreall = true;
			dragger.coop_cast = "intro";
			dragger.coop_idle_anim = %casual_stand_idle;
			dragger notify( "coop_cast_pose" );
		}
	}
	black_all( 0, 1.0 );
	wait( 1 );
	say( dragger, "crash_car1_269" );
	wait( 1 );
	black_all( 1, 0.6 );
	wait( 1 );
	remove_actor( dragger );
	for ( i = 0; i < card.size; i++ )
		card[ i ] destroy();

	// The interrogation room (player_interrogation_start, the upper floor
	// of the building south-east of the wreck): Ortiz on the phone, his
	// four watchers.
	room = crash_struct( "player_interrogation_start" );
	roomOrigin = ( 1401.5, -1407.7, 226 );
	roomYaw = 338;
	if ( isDefined( room ) )
	{
		roomOrigin = room.origin;
		roomYaw = room.angles[ 1 ];
	}
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		spot = roomOrigin;
		if ( i == 1 )
			spot = point( "player1_3rd_person_idle", roomOrigin + ( 0, 40, 0 ) );
		players[ i ] setOrigin( spot );
		players[ i ] setStance( "stand" );
		players[ i ] setPlayerAngles( ( 0, roomYaw, 0 ) );
	}
	level.crash_intro_enemies = [];
	ortiz = intro_actor( "intro_ai_puncher_spawner", "loc_punch_scene_start", %patrol_bored_idle_cellphone );
	if ( isDefined( ortiz ) )
	{
		ortiz setModel( "body_airport_assault_a_clean" );
		ortiz.crash_ortiz = true;
	}
	for ( n = 1; n <= 4; n++ )
		intro_actor( "intro_ai_watcher0" + n + "_spawner", "loc_ai_watcher0" + n, %casual_stand_idle );
	black_all( 0, 1.5 );
	wait( 1 );
	say( ortiz, "crash_ortiz_03" );
	wait( 1 );
	say( ortiz, "crash_ortiz_04" );
	if ( isDefined( ortiz ) )
	{
		yaw = vectorToAngles( roomOrigin - ortiz.origin );
		ortiz orientMode( "face angle", yaw[ 1 ] );
		ortiz.coop_idle_anim = %casual_stand_v2_twitch_talk;
		ortiz notify( "coop_cast_pose" );
	}
	say( ortiz, "crash_ortiz_05" );

	// Ghost's flashbang (magic_flashbang) through the doorway, then Ghost
	// (price_flash_point / price_peek_point) with the rifle for the rookie.
	flashAt = point( "magic_flashbang", ( 1611, -1810, 282 ) );
	center = ( roomOrigin + ortiz_or( ortiz, roomOrigin ) ) * 0.5;
	playFx( level._effect[ "crash_flashbang" ], center + ( 0, 0, 40 ) );
	sound_at( "flashbang_explode_default", center );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] shellShock( "flashbang_mp_scripted", 3 );
	for ( i = 0; i < level.crash_intro_enemies.size; i++ )
	{
		e = level.crash_intro_enemies[ i ];
		if ( !isDefined( e ) || !isAlive( e ) )
			continue;
		e.coop_idle_anim = %casual_crouch_idle;
		e notify( "coop_cast_pose" );
	}
	ghostSpot = point( "price_flash_point", ( 1689.5, -1778, 226 ) );
	spawn_ghost( ghostSpot, ( 0, 60, 0 ) );
	wait( 1.5 );
	// The gun: the recording's M4A1 with the M203 and the holo sight.
	level.crash_intro_done = true;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		players[ i ] local_sound( "scn_crash_player_catch_gun" );
		players[ i ] give_kit();
	}
	ghost_say( "crash_gst_12", true );
	level.crash_phase = "route";
	// Ortiz gets away; the watchers recover and fight.
	if ( isDefined( ortiz ) && isAlive( ortiz ) )
		ortiz thread ortiz_escape();
	for ( i = 0; i < level.crash_intro_enemies.size; i++ )
	{
		e = level.crash_intro_enemies[ i ];
		if ( isDefined( e ) && isAlive( e ) && !isDefined( e.crash_ortiz ) )
			e thread recover_after( 2.5 );
	}
	watchers = [];
	for ( i = 0; i < level.crash_intro_enemies.size; i++ )
	{
		e = level.crash_intro_enemies[ i ];
		if ( isDefined( e ) && !isDefined( e.crash_ortiz ) )
			watchers[ watchers.size ] = e;
	}
	level.crash_ghost_free_fire = true;
	wait_group_dead( watchers, "the watchers", 25 );
	level.crash_ghost_free_fire = undefined;
}

// An actor taken out of the scene: hidden, then killed, so _coopmission's
// bookkeeping (it counts on the "death" notify) stays right.
remove_actor( ai )
{
	if ( !isDefined( ai ) || !isAlive( ai ) )
		return;
	ai hide();
	ai doDamage( ai.health + 1000, ai.origin );
}

ortiz_or( ortiz, fallback )
{
	if ( isDefined( ortiz ) )
		return ortiz.origin;
	return fallback;
}

intro_actor( spawnerNoteworthy, locNoteworthy, idle )
{
	list = by_noteworthy( spawnerNoteworthy );
	if ( list.size <= 0 )
		return undefined;
	list[ 0 ].coop_no_hunt = true;
	ai = maps\_coopmission::spawn_soldier( list[ 0 ] );
	if ( !isDefined( ai ) )
		return undefined;
	locs = by_noteworthy( locNoteworthy );
	if ( locs.size > 0 )
		ai forceTeleport( locs[ 0 ].origin, locs[ 0 ].angles );
	ai.ignoreall = true;
	ai.crash_unaware = true;
	ai.coop_cast = "intro";
	ai.coop_idle_anim = idle;
	ai setAnimKnobAll( idle, %body, 1, 0.2, 1 );
	ai notify( "coop_cast_pose" );
	level.crash_intro_enemies[ level.crash_intro_enemies.size ] = ai;
	return ai;
}

recover_after( time )
{
	self endon( "death" );
	wait( time );
	self.crash_unaware = true;
	self wake();
}

// Ortiz runs for the stairs and is gone (the recording never shows him
// again in this mission).
ortiz_escape()
{
	self endon( "death" );
	self.coop_cast = undefined;
	self.coop_idle_anim = undefined;
	self.crash_unaware = undefined;
	self notify( "coop_cast_pose" );
	self.goalradius = 64;
	self setGoalPos( point( "price_open_door", ( 1607, -1801, 66 ) ) );
	wait( 6 );
	codolprintln( "CoDOL coop crash: Ortiz escaped" );
	remove_actor( self );
}

// --------------------------------------------------------------------------
// 1. Follow Ghost to Vazquez's office.
// --------------------------------------------------------------------------

phase_route()
{
	level.crash_phase = "route";
	level.crash_objective_name = "follow Ghost to Vazquez's office";
	objective( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PAUSE_FOLLOW", undefined, level.crash_ghost );
	say( undefined, "crash_pri_0293" );
	ghost_say( "crash_gst_14", true );

	// Out of the room, down the stairs and out of the building the way the
	// recording goes: the upper landing, the stairs down to the east, the
	// south room, Ghost's door (price_door) into the north room, and out of
	// its north door into the street.
	upstairs = [];
	upstairs[ 0 ] = ( 1670, -1850, 202 );
	upstairs[ 1 ] = ( 1410, -1880, 202 );
	upstairs[ 2 ] = ( 1405, -1985, 202 );
	ghost_walk( upstairs, "to the stairs" );
	ghost_wait_player( 500, 30 );
	ghost_say( "crash_gst_15", true );
	downstairs = [];
	downstairs[ 0 ] = ( 1440, -1985, 196 );
	downstairs[ 1 ] = ( 1585, -1985, 66 );
	downstairs[ 2 ] = ( 1650, -1900, 66 );
	downstairs[ 3 ] = ( 1655, -1815, 66 );
	ghost_walk( downstairs, "down the stairs" );
	open_price_door();
	wait( 0.6 );
	ghost_wait_player( 450, 30 );
	outside = [];
	outside[ 0 ] = ( 1655, -1700, 66 );
	outside[ 1 ] = ( 1600, -1450, 66 );
	outside[ 2 ] = ( 1600, -1330, 66 );
	ghost_walk( outside, "out to the street" );

	// Outside: the first group comes at them (group_a).
	ais = spawn_group( "group_a", undefined, ghost_spot() );
	ghost_say( "crash_gst_16", true );
	wait_group_dead( ais, "group_a", 40, 1 );
	ghost_say( "crash_gst_17", true );

	// West through the town on the bot waypoint graph (the points are its
	// nodes 7, 21, 23, 18, 305, 19), to the yard where the recording's dogs
	// come.  There is no coop dog species yet: the yard's old_group_b
	// soldiers stand in for the three dogs (group_b).
	route = [];
	route[ 0 ] = ( 1255, -1438, 64 );
	route[ 1 ] = ( 959, -1520, 68 );
	route[ 2 ] = ( 712, -1518, 63 );
	route[ 3 ] = ( 540, -1363, 64 );
	route[ 4 ] = ( 89, -1325, 68 );
	route[ 5 ] = ( -105, -1250, 68 );
	ghost_lead( route, "to the yard" );
	ais = spawn_group( "old_group_b", undefined, ghost_spot() );
	wait_group_dead( ais, "old_group_b (the dogs' yard)", 45 );
	ghost_say( "crash_gst_45" );

	// North up the alley (nodes 28, 37): another wave.
	route = [];
	route[ 0 ] = ( -249, -1158, 77 );
	route[ 1 ] = ( -168, -876, 103 );
	ghost_lead( route, "up the alley" );
	ais = spawn_group( "group_c", undefined, ghost_spot() );
	ghost_say( "crash_gst_18", true );
	wait_group_dead( ais, "group_c", 40, 1 );
	ghost_say( "crash_gst_20", true );

	// Further up (nodes 38, 39): men on the ground and on the rooftops
	// (group_g's roof pair at z 397, group_j in the street).
	route = [];
	route[ 0 ] = ( -157, -595, 104 );
	route[ 1 ] = ( -128, -257, 116 );
	ghost_lead( route, "to the rooftops" );
	ais = spawn_group( "group_g", undefined, ghost_spot() );
	ais = append( ais, spawn_group( "group_j", undefined, ghost_spot() ) );
	wait_group_dead( ais, "the rooftops (group_g, group_j)", 40, 2, 25 );
	ghost_say( "crash_gst_21", true );

	// North to the office's street (nodes 98, 97, 96, 95, 78, 77): the
	// office's guard (group_h, two of them upstairs).
	route = [];
	route[ 0 ] = ( -72, -61, 134 );
	route[ 1 ] = ( -3, 86, 136 );
	route[ 2 ] = ( 64, 350, 136 );
	route[ 3 ] = ( 134, 589, 136 );
	route[ 4 ] = ( 161, 975, 148 );
	route[ 5 ] = ( -151, 1174, 214 );
	ghost_lead( route, "to the office" );
	ais = spawn_group( "group_h", undefined, ghost_spot() );
	ghost_say( "crash_gst_47", true );
	wait( 2 );
	say( undefined, "crash_pri_0327", true );
	wait_group_dead( ais, "group_h (the office's guard)", 40, 2, 30 );
	route = [];
	route[ 0 ] = ( -496, 1210, 249 );
	route[ 1 ] = ( -498, 1712, 252 );
	ghost_lead( route, "up the office's street" );
}

// --------------------------------------------------------------------------
// 2. The office: flashbang through the door, clear the ground floor.
// --------------------------------------------------------------------------

phase_office()
{
	level.crash_phase = "office";
	clear_leftovers( 1500 );
	// The office's occupants as the recording shows them before the flash:
	// a smoker leaning on the wall, a man on the phone, men upstairs.
	ais = spawn_group( "group_d", undefined, undefined, true );
	for ( i = 0; i < ais.size; i++ )
	{
		sp = ais[ i ].coop_spawner;
		idle = %patrol_bored_idle;
		if ( isDefined( sp.script_animation ) && sp.script_animation == "lean_smoke" )
			idle = %parabolic_leaning_guy_smoking_idle;
		else if ( isDefined( sp.target ) )
		{
			s = crash_struct( sp.target );
			if ( isDefined( s ) )
			{
				a = ais[ i ].angles;
				if ( isDefined( s.angles ) )
					a = s.angles;
				ais[ i ] forceTeleport( s.origin, a );
				if ( isDefined( s.script_animation ) && s.script_animation == "phone" )
					idle = %patrol_bored_idle_cellphone;
				else if ( isDefined( s.script_animation ) && s.script_animation == "smoke" )
					idle = %patrol_bored_idle_smoke;
			}
		}
		// They wake only to a shot, or to a player right beside them: in
		// the recording the room stays oblivious while the rookie stands in
		// the doorway with the flashbang (at 160 units the user's own
		// playthrough had them up and firing at the doorway before the
		// throw).
		ais[ i ] thread unaware( idle, 72 );
	}

	// Ghost at the office's corner: flashbangs from his pack.
	packSpot = point( "pre_download_checkpoint_price", ( -404, 2120, 268 ) );
	ghost_move( packSpot, "the office corner" );
	// He crouches at the corner with his pack to the rookie (the
	// recording, 3:08), rifle up, until the room is flashed.
	ghost_pose( %exposed_crouch_aim_5 );
	level.crash_objective_name = "get flashbangs from Ghost";
	objective( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PICK_UP_FLASH", undefined, level.crash_ghost );
	ghost_say( "crash_gst_48", true );
	hint( &"COOP_DEFENSE_MP_CRASH_TROPICAL_PICK_UP_FLASH" );
	trig = getEnt( "get_flash_bangs", "targetname" );
	who = wait_use( level.crash_ghost.origin, 110, 0.6, 60, trig );
	hint_clear();
	level.crash_have_flash = true;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isAlive( players[ i ] ) )
			players[ i ] equip_offhands();
	}
	codolprintln( "CoDOL coop crash: flashbangs handed out" );

	// Through the door (throw_flash_window_obj).
	window = point( "throw_flash_window_obj", ( -521.5, 2148, 307 ) );
	level.crash_objective_name = "throw a flashbang through the door";
	objective( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_THROW_FLASH", window );
	ghost_say( "crash_gst_50", true );
	hint( &"COOP_DEFENSE_MP_CRASH_TROPICAL_THROW_FLASHBANG" );
	thrown = wait_flash( window, 30 );
	hint_clear();
	if ( !thrown )
		ghost_say( "crash_gst_51", true );
	// The room is flashed: everyone in it staggers, then wakes.
	for ( i = 0; i < ais.size; i++ )
	{
		if ( isDefined( ais[ i ] ) && isAlive( ais[ i ] ) )
			ais[ i ] thread flashed_then_wake( 2 );
	}
	ghost_say( "crash_gst_54", true );
	ghost_pose( %exposed_aim_5 );
	objective_clear();
	hint( &"COOP_DEFENSE_MP_CRASH_TROPICAL_ADS_HINT", 5 );
	level.crash_ghost_free_fire = true;
	ghost_move( point( "player_entry_room", window ), "into the room", 96, 20 );
	wait_group_dead( ais, "group_d (the office)", 30, 2 );
	level.crash_ghost_free_fire = undefined;
	ghost_say( "crash_gst_57", true );
	level.crash_objective_name = "go to the second floor";
	objective( &"COOP_DEFENSE_MP_CRASH_TROPICAL_GOTO_INTERIOR", point( "claymore_pick_up_objective", ( -573, 1955, 411 ) ) );
	ghost_move( point( "post_download_checkpoint_price", ( -668, 2156, 400 ) ), "upstairs" );
}

wait_flash( window, timeout )
{
	step( "flash", window );
	level.crash_flash_seen = undefined;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] thread watch_flash_throw();
	waited = 0;
	while ( waited < timeout )
	{
		if ( isDefined( level.crash_flash_seen ) )
			return true;
		wait( 0.25 );
		waited += 0.25;
	}
	level notify( "crash_flash_watch_stop" );
	codolprintln( "CoDOL coop crash: no flashbang thrown in " + timeout + " s; the room is taken anyway" );
	return false;
}

watch_flash_throw()
{
	self endon( "disconnect" );
	level endon( "crash_flash_watch_stop" );
	for ( ;; )
	{
		self waittill( "grenade_fire", grenade, weapon );
		if ( !isDefined( weapon ) || !isSubStr( weapon, "flash" ) )
			continue;
		codolprintln( "CoDOL coop crash: " + self.name + " threw " + weapon );
		wait( 1.5 );
		level.crash_flash_seen = true;
		level notify( "crash_flash_watch_stop" );
		return;
	}
}

flashed_then_wake( time )
{
	self endon( "death" );
	self.coop_idle_anim = %casual_crouch_idle;
	self notify( "coop_cast_pose" );
	wait( time );
	self wake();
}

// --------------------------------------------------------------------------
// 3. The download.
// --------------------------------------------------------------------------

phase_download()
{
	level.crash_phase = "download";

	// Claymores (claymore_pick_up_objective, claymores_trigger; the five
	// pick_up_claymores models on the crate).
	pickSpot = point( "claymore_pick_up_objective", ( -573, 1955, 411 ) );
	ghost_say( "crash_gst_58", true );
	wait( 1 );
	say( undefined, "crash_pri_0339", true );
	level.crash_objective_name = "pick up the claymores";
	objective( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PICK_UP_CLAYMORE", pickSpot );
	hint( &"COOP_DEFENSE_MP_CRASH_TROPICAL_PICK_UP_CLAYMORE" );
	trig = getEnt( "claymores_trigger", "targetname" );
	who = wait_use( pickSpot, 96, 0.6, 60, trig );
	hint_clear();
	models = getEntArray( "pick_up_claymores", "targetname" );
	for ( i = 0; i < models.size; i++ )
		models[ i ] hide();
	level.crash_have_claymore = true;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isAlive( players[ i ] ) )
			players[ i ] equip_offhands();
	}

	// Two of them at the doors (claymore_place_objective, each with its
	// glowing bombsquad claymore).
	spots = structs( "claymore_place_objective" );
	level.crash_objective_name = "place the claymores";
	objective( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PLACE_CLAYMORES", undefined );
	for ( i = 0; i < spots.size; i++ )
	{
		objective_add( 1 + i, "active", spots[ i ].origin );
		if ( isDefined( spots[ i ].target ) )
		{
			glow = getEnt( spots[ i ].target, "targetname" );
			if ( isDefined( glow ) )
			{
				glow show();
				spots[ i ].crash_glow = glow;
			}
		}
	}
	hint( &"COOP_DEFENSE_MP_CRASH_TROPICAL_PLACE_CLAYMORE" );
	ghost_say( "crash_gst_59", true );
	placed = wait_claymores( spots, 45 );
	hint_clear();
	for ( i = 0; i < spots.size; i++ )
	{
		objective_delete( 1 + i );
		if ( isDefined( spots[ i ].crash_glow ) )
			spots[ i ].crash_glow hide();
	}

	// The laptop (the_laptop, its glowing copy the_laptop_obj, the use
	// trigger me0_auto3366) and the DSM.
	laptop = getEnt( "the_laptop", "targetname" );
	laptopOrigin = ( -550, 2152, 419.5 );
	if ( isDefined( laptop ) )
		laptopOrigin = laptop.origin;
	radius = 800;
	if ( isDefined( laptop ) && isDefined( laptop.radius ) )
		radius = float( laptop.radius );
	level.crash_laptop_radius = radius;
	glow = getEnt( "the_laptop_obj", "targetname" );
	use = getEnt( "me0_auto3366", "targetname" );

	ghost_say( "crash_gst_62", true );
	totalFiles = 30 + randomInt( 4 );
	level.crash_files_done = 0;
	level.crash_download_elapsed = 0;
	downloadTime = getDvarFloat( "scr_coop_crash_download_time" );
	if ( downloadTime <= 0 )
		downloadTime = 70;
	first = true;
	level.crash_waves = [];
	waves_started = false;
	while ( level.crash_download_elapsed < downloadTime )
	{
		if ( first )
		{
			level.crash_objective_name = "start the download";
			// The recording: "obtain the enemy's intel" at the top (the common
			// coop string), "start download" over the laptop.
			objective( &"PMC_OBJECTIVE_ABORT_CODES", laptopOrigin );
		}
		else
		{
			level.crash_objective_name = "resume the download";
			objective( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_RESUME_DOWNLOAD", laptopOrigin );
		}
		if ( isDefined( glow ) )
			glow show();
		hint( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_START_DOWNLOAD" );
		wait_use( laptopOrigin, 80, 0.6, 9999, use );
		hint_clear();
		if ( isDefined( glow ) )
			glow hide();
		level.crash_objective_name = "defend the download";
		objective( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_DEFEND_DOWNLOAD", laptopOrigin );
		if ( !waves_started )
		{
			waves_started = true;
			clear_leftovers( 700 );
			level thread download_waves( laptopOrigin );
			level thread download_lines( downloadTime );
			level thread download_callouts();
		}
		else
			say( undefined, "crash_pri_0358", true );
		ok = download_run( laptopOrigin, radius, downloadTime, totalFiles, first );
		first = false;
		if ( !ok )
		{
			level thread dsm_status( &"PMC_DOWNLOAD_INTERRUPTED", ( 1, 0.25, 0.25 ), 6 );
			say( undefined, "crash_pri_0355", true );
		}
	}
	level notify( "crash_download_done" );
	level thread dsm_status( &"PMC_DOWNLOAD_COMPLETE", ( 0.55, 0.8, 1 ), 6 );
	objective_clear();
	ghost_say( "crash_gst_93", true );
	wait( 1.5 );
	say( undefined, "crash_pri_0364" );
	codolprintln( "CoDOL coop crash: download complete, " + level.crash_files_done + " files" );
}

wait_claymores( spots, timeout )
{
	step( "claymore", spots );
	level.crash_claymores_placed = 0;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] thread watch_claymores( spots );
	waited = 0;
	said = false;
	while ( waited < timeout && level.crash_claymores_placed < spots.size )
	{
		if ( !said && level.crash_claymores_placed == 1 )
		{
			said = true;
			ghost_say( "crash_gst_61", true );
		}
		wait( 0.25 );
		waited += 0.25;
	}
	level notify( "crash_claymore_watch_stop" );
	codolprintln( "CoDOL coop crash: " + level.crash_claymores_placed + " claymore spot(s) covered in " + waited + " s" );
	return level.crash_claymores_placed;
}

watch_claymores( spots )
{
	self endon( "disconnect" );
	level endon( "crash_claymore_watch_stop" );
	for ( ;; )
	{
		self waittill( "grenade_fire", grenade, weapon );
		if ( !isDefined( weapon ) || !isSubStr( weapon, "claymore" ) || !isDefined( grenade ) )
			continue;
		wait( 1 );
		if ( !isDefined( grenade ) )
			continue;
		for ( i = 0; i < spots.size; i++ )
		{
			if ( isDefined( spots[ i ].crash_done ) )
				continue;
			r = 110;
			if ( isDefined( spots[ i ].radius ) )
				r = float( spots[ i ].radius ) + 40;
			if ( distance( grenade.origin, spots[ i ].origin ) <= r )
			{
				spots[ i ].crash_done = true;
				objective_delete( 1 + i );
				if ( isDefined( spots[ i ].crash_glow ) )
					spots[ i ].crash_glow hide();
				level.crash_claymores_placed++;
				codolprintln( "CoDOL coop crash: " + self.name + " covered claymore spot " + i + " at " + grenade.origin );
				break;
			}
		}
	}
}

// One run of the DSM until it finishes or the signal is lost.  The HUD is
// IW4 so_download_arcadia's (PMC_DSM_* strings, which the coop zone ships):
// the start-up lines on the first run, then "files copied N / total" and
// the signal strength.
download_run( laptopOrigin, radius, downloadTime, totalFiles, first )
{
	step( "download", laptopOrigin );
	frame = dsm_hud( 0 );
	frame.label = &"PMC_DSM_FRAME";
	frame.alignX = "right";
	if ( first )
	{
		labels = [];
		labels[ 0 ] = &"PMC_DSM_INIT";
		labels[ 1 ] = &"PMC_DSM_CONNECTING";
		labels[ 2 ] = &"PMC_DSM_LOGIN";
		labels[ 3 ] = &"PMC_DSM_LOCATE";
		times = [];
		times[ 0 ] = 2.5;
		times[ 1 ] = 0.9;
		times[ 2 ] = 3.75;
		times[ 3 ] = 1.5;
		for ( k = 0; k < labels.size; k++ )
		{
			status = dsm_hud( 0 );
			status.label = labels[ k ];
			dsm_count( status, times[ k ] );
			status destroy();
			level.crash_download_elapsed += times[ k ] + 1.5;
		}
	}
	frame destroy();

	progress = dsm_hud( 0 );
	progress.label = &"PMC_DSM_PROGRESS";
	count = dsm_hud( 170 );
	count.alignX = "right";
	// Every element gets its value as it is made: a labelled element with
	// no value yet draws its raw placeholder (the user's playthrough showed
	// "&&1%" for the signal and a blank count for the first frame).
	count setValue( int( level.crash_files_done ) );
	total = dsm_hud( 170 );
	total.label = &"PMC_DSM_TOTALFILES";
	total setValue( totalFiles );
	signal = dsm_hud( 0 );
	signal.y = 70;
	signal.label = &"COOP_DEFENSE_MP_CRASH_TROPICAL_SIGNAL_STRENGTH";
	signal setValue( dsm_signal( laptopOrigin, radius ) );

	lost = 0;
	ok = true;
	while ( level.crash_download_elapsed < downloadTime )
	{
		wait( 0.25 );
		d = dsm_nearest( laptopOrigin );
		strength = dsm_signal( laptopOrigin, radius );
		signal setValue( strength );
		if ( strength < 30 )
			signal.color = ( 1, 0.25, 0.25 );
		else
			signal.color = ( 1, 1, 1 );
		if ( d > radius )
		{
			lost += 0.25;
			if ( lost == 0.25 )
				ghost_say( "crash_gst_67", true );
			if ( lost >= 6 )
			{
				ok = false;
				break;
			}
			continue;
		}
		lost = 0;
		level.crash_download_elapsed += 0.25;
		level.crash_files_done = totalFiles * level.crash_download_elapsed / downloadTime;
		if ( level.crash_files_done > totalFiles )
			level.crash_files_done = totalFiles;
		count setValue( int( level.crash_files_done ) );
	}
	progress destroy();
	count destroy();
	total destroy();
	signal destroy();
	codolprintln( "CoDOL coop crash: download run ends, ok " + ok + ", " + int( level.crash_files_done ) + " of " + totalFiles + " files, " +
		level.crash_download_elapsed + " s" );
	return ok;
}

// The nearest living player's distance to the laptop.
dsm_nearest( laptopOrigin )
{
	d = 99999;
	players = maps\_coopmission::living_players();
	for ( i = 0; i < players.size; i++ )
	{
		dd = distance( players[ i ].origin, laptopOrigin );
		if ( dd < d )
			d = dd;
	}
	return d;
}

// Signal strength from that distance (99 at the laptop, 16 at the edge of
// the radius, as the recording's red 16% when the rookie strays).
dsm_signal( laptopOrigin, radius )
{
	d = dsm_nearest( laptopOrigin );
	strength = int( 100 - 84 * d / radius );
	if ( strength > 99 )
		strength = 99;
	if ( strength < 0 )
		strength = 0;
	strength = strength - randomInt( 3 );
	if ( strength < 0 )
		strength = 0;
	return strength;
}

dsm_hud( xOffset )
{
	hud = newHudElem();
	hud.alignX = "left";
	hud.alignY = "middle";
	hud.horzAlign = "right";
	hud.vertAlign = "top";
	hud.x = -250 + xOffset;
	hud.y = 50;
	hud.font = "objective";
	hud.fontScale = 1.4;
	hud.color = ( 0.55, 0.8, 1 );
	hud.foreground = true;
	hud.hidewheninmenu = true;
	return hud;
}

dsm_count( hud, time )
{
	hud setValue( 0 );
	wait( 1 );
	steps = int( time / 0.1 );
	if ( steps < 1 )
		steps = 1;
	for ( i = 0; i < steps; i++ )
	{
		hud setValue( int( i * 100 / steps ) );
		wait( 0.1 );
	}
	hud setValue( 100 );
	wait( 0.5 );
}

dsm_status( label, color, time )
{
	hud = dsm_hud( 0 );
	hud.label = label;
	hud.color = color;
	for ( t = 0; t < time; t += 1 )
	{
		hud.alpha = 1;
		wait( 0.5 );
		hud.alpha = 0.2;
		wait( 0.5 );
	}
	hud destroy();
}

// The attack on the office: the three authored waves from the town around
// it (group_wave_1..3: the garage across the street, the alley, the ladder
// on the east side, the stairs), each sent at the laptop.
download_waves( laptopOrigin )
{
	level endon( "crash_download_done" );
	wait( 3 );
	ghost_say( "crash_gst_102", true );
	names = strTok( "group_wave_1 group_wave_2 group_wave_3", " " );
	for ( w = 0; w < names.size; w++ )
	{
		ais = spawn_group( names[ w ], undefined, laptopOrigin );
		level.crash_waves = append( level.crash_waves, ais );
		waited = 0;
		while ( alive_count( ais ) > 3 && waited < 22 )
		{
			wait( 0.5 );
			waited += 0.5;
		}
		if ( w == 0 )
			ghost_say( "crash_gst_38", true );
	}
}

download_lines( downloadTime )
{
	level endon( "crash_download_done" );
	wait( 3.5 );
	ghost_say( "crash_gst_65", true );
	wait( 4 );
	say( undefined, "crash_pri_0343", true );
	said = [];
	marks = strTok( "0.3 0.5 0.65 0.85 0.93", " " );
	lines = strTok( "crash_pri_0353 crash_pri_0350 crash_pri_0354 crash_pri_0351 crash_pri_0352", " " );
	for ( ;; )
	{
		wait( 0.5 );
		f = level.crash_download_elapsed / downloadTime;
		for ( i = 0; i < marks.size; i++ )
		{
			if ( !isDefined( said[ i ] ) && f >= float( marks[ i ] ) )
			{
				said[ i ] = true;
				say( undefined, lines[ i ], true );
				break;
			}
		}
	}
}

// Ghost calls out where they come from, by the addon's callout volumes
// (trigger_multiple, AI-only spawnflags).
download_callouts()
{
	level endon( "crash_download_done" );
	names = strTok( "callout_garage callout_on_ladder callout_on_stairs callout_east callout_north callout_first_floor callout_second_floor", " " );
	aliases = strTok( "crash_gst_68 crash_gst_70 crash_gst_89 crash_gst_73 crash_gst_69 crash_gst_88 crash_gst_87", " " );
	vols = [];
	for ( i = 0; i < names.size; i++ )
		vols[ i ] = getEnt( names[ i ], "targetname" );
	last = [];
	for ( ;; )
	{
		wait( 0.5 );
		for ( i = 0; i < vols.size; i++ )
		{
			if ( !isDefined( vols[ i ] ) )
				continue;
			if ( isDefined( last[ i ] ) && getTime() - last[ i ] < 20000 )
				continue;
			for ( j = 0; j < level.crash_waves.size; j++ )
			{
				e = level.crash_waves[ j ];
				if ( !isDefined( e ) || !isAlive( e ) )
					continue;
				if ( e isTouching( vols[ i ] ) )
				{
					last[ i ] = getTime();
					ghost_say( aliases[ i ], true );
					wait( 4 );
					break;
				}
			}
		}
	}
}

// --------------------------------------------------------------------------
// 4. Back to the landing zone: the wreck.
// --------------------------------------------------------------------------

phase_heli()
{
	level.crash_phase = "heli";
	clear_leftovers( 1200 );
	level.crash_objective_name = "follow Ghost to the extraction point";
	objective( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PAUSE_FOLLOW_LZ", undefined, level.crash_ghost );

	// The searchers at the wreck (group_investigate_heli on their
	// crouch_search / stand_search structs).
	searchers = spawn_searchers();

	// Down and out of the office, back south to the landing zone on the
	// waypoint graph (nodes 55, 51, 314, 66, 77, 78, 79).
	ghost_move( ( -481, 2025, 250 ), "out of the office", 96, 30 );
	route = [];
	route[ 0 ] = ( -309, 1770, 232 );
	route[ 1 ] = ( -228, 1669, 230 );
	route[ 2 ] = ( -180, 1466, 229 );
	route[ 3 ] = ( -151, 1174, 214 );
	ghost_lead( route, "back to the landing zone" );
	// With the download's survivors still on the office stairs the actor
	// budget can be full when the stretch starts (MEASURED on the probe:
	// 0 of 5 spawned, the wreck empty).  Half way there they are far
	// enough behind to be taken out, and the searchers come now.
	if ( alive_count( searchers ) == 0 )
	{
		clear_leftovers( 900 );
		searchers = spawn_searchers();
	}
	route = [];
	route[ 0 ] = ( 161, 975, 148 );
	route[ 1 ] = ( 318, 796, 129 );
	ghost_lead( route, "back to the landing zone (2)" );
	vol = getEnt( "investigate_heli_volume", "targetname" );
	volOrigin = ( 499, 740, 177 );
	if ( isDefined( vol ) )
		volOrigin = vol.origin;
	wait_player_near( volOrigin, 1300, 60 );
	ghost_say( "crash_gst_94", true );
	heliObj = point( "destroy_heli_obj", ( 458, 529, 245 ) );
	level.crash_objective_name = "destroy the helicopter";
	objective( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PAUSE_SCUTTLE", heliObj );
	say( undefined, "crash_pri_0391", true );
	ghost_move( ( 250, 760, 140 ), "the wreck's edge", 96, 25 );
	wait_group_dead( searchers, "the wreck's searchers", 45 );

	// The explosives: the lamp store the enemy uses as a supply depot
	// (bomb_site01).
	ghost_say( "crash_gst_99" );
	site = point( "bomb_site01", ( -688, 1336, 266 ) );
	level.crash_objective_name = "find the explosives";
	objective( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_FIND_C4", site );
	ghost_say( "crash_gst_95", true );
	guards = spawn_group( "group_f", 6 );
	case_model = spawn( "script_model", ground_or( site ) + ( 0, 0, 2 ) );
	case_model setModel( "prop_suitcase_bomb_obj" );
	route = [];
	route[ 0 ] = ( 161, 975, 148 );
	route[ 1 ] = ( -151, 1174, 214 );
	route[ 2 ] = ( -496, 1210, 249 );
	ghost_lead( route, "to the lamp store", 600 );
	wait_player_near( site, 260, 90 );
	ghost_say( "crash_gst_106", true );
	hint( &"COOP_DEFENSE_MP_CRASH_TROPICAL_DESTROY_HELI" );
	who = wait_use( site, 90, 1.0, 60, undefined, true );
	hint_clear();
	case_model delete();
	codolprintln( "CoDOL coop crash: explosives picked up" );
	say( undefined, "crash_pri_0404", true );

	// Back to the wreck with them; plant.
	plant = getEnt( "bomb_plant_site", "targetname" );
	plantOrigin = ( 499, 427, 148 );
	if ( isDefined( plant ) )
	{
		plantOrigin = plant.origin;
		plant show();
	}
	level.crash_objective_name = "plant the C4 on the helicopter";
	objective( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PLANT_C4", plantOrigin );
	counter = spawn_group( "group_e", 6 );
	ghost_say( "crash_gst_103", true );
	route = [];
	route[ 0 ] = ( -151, 1174, 214 );
	route[ 1 ] = ( 161, 975, 148 );
	route[ 2 ] = ( 611, 492, 139 );
	ghost_lead( route, "back to the wreck", 700 );
	who = wait_use( plantOrigin, 90, 3, 120, undefined, true );
	if ( isDefined( plant ) )
		plant hide();
	bomb = spawn( "script_model", plantOrigin );
	bomb setModel( "prop_suitcase_bomb" );
	sound_at( "mp_bomb_plant", plantOrigin );
	if ( soundExists( "bomb_tick" ) )
		bomb playLoopSound( "bomb_tick" );
	level.crash_bomb = bomb;
	codolprintln( "CoDOL coop crash: C4 planted at " + plantOrigin );
	level thread bomb_timer( 40 );
	objective_clear();
}

ground_or( p )
{
	return maps\_coopvehicle::ground_under( p );
}

spawn_searchers()
{
	searchers = spawn_group( "group_investigate_heli", undefined, undefined, true );
	for ( i = 0; i < searchers.size; i++ )
	{
		sp = searchers[ i ].coop_spawner;
		idle = %casual_crouch_idle;
		if ( isDefined( sp.target ) )
		{
			s = crash_struct( sp.target );
			if ( isDefined( s ) )
			{
				searchers[ i ] forceTeleport( s.origin, searchers[ i ].angles );
				if ( isDefined( s.script_animation ) && s.script_animation == "stand_search" )
					idle = %patrol_bored_idle;
			}
		}
		searchers[ i ] thread unaware( idle, 700 );
	}
	return searchers;
}

bomb_timer( time )
{
	level endon( "crash_bomb_blown" );
	wait( time );
	blow_wreck();
}

// The wreck goes up (the recording: a column of smoke behind the roof as
// the helicopter comes in).
blow_wreck()
{
	if ( isDefined( level.crash_wreck_blown ) )
		return;
	level.crash_wreck_blown = true;
	level notify( "crash_bomb_blown" );
	o = point( "destroy_heli_obj", ( 458, 529, 245 ) );
	if ( isDefined( level.crash_bomb ) )
	{
		o = level.crash_bomb.origin;
		level.crash_bomb stopLoopSound();
		level.crash_bomb delete();
	}
	playFx( level._effect[ "crash_heli_explosion" ], o + ( 0, 0, 40 ) );
	playFx( level._effect[ "crash_heli_crash" ], o );
	sound_at( "exp_suitcase_bomb_main", o );
	earthquake( 0.4, 1.5, o, 2500 );
	radiusDamage( o + ( 0, 0, 40 ), 300, 200, 20 );
	codolprintln( "CoDOL coop crash: the wreck is destroyed at " + o );
}

// --------------------------------------------------------------------------
// 5. The roof: flare, the wreck goes up, the extraction helicopter.
// --------------------------------------------------------------------------

phase_roof()
{
	level.crash_phase = "heli";
	roof = getEnt( "trigger_gathering_onRoof", "targetname" );
	roofOrigin = ( 1540, 682, 608 );
	if ( isDefined( roof ) )
		roofOrigin = roof.origin;
	ghost_say( "crash_gst_107", true );
	wait( 1 );
	ghost_say( "crash_gst_108", true );
	level.crash_objective_name = "get to the roof";
	objective( &"COOP_DEFENSE_MP_CRASH_TROPICAL_OBJ_PAUSE_ROOF", roofOrigin );
	hint( &"COOP_DEFENSE_MP_CRASH_TROPICAL_SPRINT_HINT", 4 );
	clear_leftovers( 1500 );
	ais = spawn_group( "group_f" );
	ais = append( ais, spawn_group( "group_i" ) );
	level thread ghost_to_roof();

	// On the roof (the authored trigger, or the gathering volume).
	step( "roof", roofOrigin );
	who = undefined;
	waited = 0;
	while ( waited < 240 )
	{
		players = maps\_coopmission::living_players();
		for ( i = 0; i < players.size; i++ )
		{
			if ( isDefined( roof ) && players[ i ] isTouching( roof ) )
				who = players[ i ];
			else if ( distance( players[ i ].origin, roofOrigin ) < 140 )
				who = players[ i ];
		}
		if ( isDefined( who ) )
			break;
		wait( 0.25 );
		waited += 0.25;
	}
	level.crash_phase = "roof";
	objective_clear();
	if ( isDefined( who ) )
		codolprintln( "CoDOL coop crash: " + who.name + " is on the roof" );

	// The flare (flare_wave): green smoke, the extraction helicopter
	// (chopper_end on so_chopper_end_path), the wreck goes up.
	flare = point( "flare_wave", ( 1514, 596, 590 ) );
	freeze_all( true );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		players[ i ] local_sound( "scn_crash_tropical_grab_flare" );
		players[ i ] disableWeapons();
	}
	wait( 0.8 );
	playFx( level._effect[ "crash_flare" ], flare + ( 0, 0, 4 ) );
	playFx( level._effect[ "crash_flare_smoke" ], flare );
	sound_at( "scn_crash_tropical_flare_start", flare );
	// The recording: the flare, the wreck goes up behind, and the Black Hawk
	// comes in over the roof before "mission complete" (7:13 -> 7:25, about
	// ten seconds).  The mission used to end 4.5 s after the flare, with the
	// helicopter still kilometres out and nobody facing it, so it was never
	// seen.  Now the squad looks toward it and the end waits for it.
	level.crash_chopper_arrived = undefined;
	level thread extraction_chopper();
	level thread watch_chopper();
	start = getTime();
	wait( 3 );
	blow_wreck();
	wait( 1.5 );
	say( undefined, "crash_pri_0408" );
	while ( !isDefined( level.crash_chopper_arrived ) && getTime() - start < 14000 )
		wait( 0.1 );
	wait( 1.5 );
	level notify( "crash_chopper_view_stop" );
	players = getPlayers();
	kills = 0;
	for ( i = 0; i < players.size; i++ )
		kills += int( players[ i ].kills );
	shots = 0;
	if ( isDefined( level.crash_ghost ) && isDefined( level.crash_ghost.crash_shots ) )
		shots = level.crash_ghost.crash_shots;
	start = getTime();
	if ( isDefined( level.crash_start_time ) )
		start = level.crash_start_time;
	codolprintln( "CoDOL coop crash: mission played in " + int( ( getTime() - start ) / 1000 ) + " s; the players killed " + kills +
		", Ghost fired " + shots + " rounds" );
	maps\_coopmission::mission_complete( "the squad reached the roof and was extracted" );
}

// Ghost to the roof: across to the building (nodes 102, 103, 104) and up
// its stairs on the graph (node 170 is on the roof, z 580).
ghost_to_roof()
{
	ghost_move( ( 618, 335, 143 ), "the wreck's east side", 96, 30 );
	ghost_move( ( 972, 449, 135 ), "the road east", 96, 30 );
	ghost_move( ( 1331, 662, 132 ), "the roof building", 96, 30 );
	ghost_move( ( 1484, 683, 580 ), "the roof", 96, 60 );
}

extraction_chopper()
{
	chopper = getEnt( "chopper_end", "targetname" );
	path = [];
	if ( isDefined( chopper ) && isDefined( chopper.target ) )
		path = maps\_coopvehicle::get_path( chopper.target );
	else
		path = maps\_coopvehicle::get_path( "so_chopper_end_path" );
	start = ( -4047, 426, 1140 );
	model = "vehicle_blackhawk_minigun_hero";
	if ( isDefined( chopper ) )
	{
		start = chopper.origin;
		if ( isDefined( chopper.model ) && chopper.model != "" )
			model = chopper.model;
	}
	mover = spawn( "script_model", start );
	mover setModel( model );
	mover.angles = ( 0, 0, 0 );
	level.crash_chopper = mover;
	codolprintln( "CoDOL coop crash: extraction helicopter from " + start + " over " + path.size + " nodes" );
	flyStart = getTime();
	for ( i = 0; i < path.size; i++ )
	{
		mph = 60;
		if ( isDefined( path[ i ].speed ) )
			mph = float( path[ i ].speed );
		if ( mph < 10 )
			mph = 10;
		d = distance( mover.origin, path[ i ].origin );
		if ( d < 1 )
			continue;
		yaw = vectorToAngles( path[ i ].origin - mover.origin );
		mover rotateTo( ( 0, yaw[ 1 ], 0 ), 0.5 );
		time = d / ( mph * 17.6 );
		mover moveTo( path[ i ].origin, time, 0, 0 );
		wait( time );
	}
	// Over the roof, low.
	roof = point( "smoke_point", ( 1696, 688, 698 ) );
	dest = roof + ( -200, 0, 300 );
	d = distance( mover.origin, dest );
	time = d / 1056;
	if ( time < 2.5 )
		time = 2.5;
	yaw = vectorToAngles( dest - mover.origin );
	mover rotateTo( ( 0, yaw[ 1 ], 0 ), 0.5 );
	mover moveTo( dest, time, 0, time * 0.4 );
	wait( time * 0.7 );
	level.crash_chopper_arrived = true;
	codolprintln( "CoDOL coop crash: extraction helicopter over the roof at " + mover.origin + " after " + ( getTime() - flyStart ) / 1000 + " s" );
}

// The squad's view follows the incoming helicopter (the recording ends on
// the rookie looking up at it).  Their controls are frozen; the view is
// eased toward it a little each frame rather than snapped.
watch_chopper()
{
	level endon( "crash_chopper_view_stop" );
	wait( 0.5 );
	for ( ;; )
	{
		wait( 0.05 );
		if ( !isDefined( level.crash_chopper ) )
			continue;
		players = maps\_coopmission::living_players();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			eye = p.origin + ( 0, 0, 60 );
			want = vectorToAngles( level.crash_chopper.origin - eye );
			have = p getPlayerAngles();
			p setPlayerAngles( ( ease_angle( have[ 0 ], want[ 0 ], 0.12 ), ease_angle( have[ 1 ], want[ 1 ], 0.12 ), 0 ) );
		}
	}
}

ease_angle( from, to, f )
{
	d = to - from;
	while ( d > 180 )
		d -= 360;
	while ( d < -180 )
		d += 360;
	return from + d * f;
}

// --------------------------------------------------------------------------
// Probe harness verbs (scr_coop_test 1; _cooptest's level.coop_mission_testcmd).
// --------------------------------------------------------------------------

test_command( tokens )
{
	switch ( tokens[ 0 ] )
	{
	case "floors":
		// floors <x> <y>: every surface under (x, y) from z 1400 down.
		x = float( tokens[ 1 ] );
		y = float( tokens[ 2 ] );
		z = 1400;
		text = "CoDOL crashtest: floors " + x + " " + y + ":";
		for ( n = 0; n < 8 && z > -600; n++ )
		{
			t = bulletTrace( ( x, y, z ), ( x, y, -800 ), false, undefined );
			if ( t[ "fraction" ] >= 1 )
				break;
			p = t[ "position" ];
			text += " " + int( p[ 2 ] );
			z = p[ 2 ] - 40;
		}
		codolprintln( text );
		break;
	case "crashstatus":
		s = level.crash_step;
		text = "CoDOL crashtest: phase " + level.crash_phase + " objective '" + level.crash_objective_name + "'";
		if ( isDefined( s ) )
			text += " step " + s.kind;
		if ( isDefined( level.crash_ghost ) )
			text += " ghost " + level.crash_ghost.origin;
		codolprintln( text );
		break;
	case "crashroute":
		level thread crash_route();
		break;
	}
}

// crashroute: player 0 plays the mission: walks where each step waits
// (botMoveTo), shoots the enemies it waits on with real bullets (botLookAt +
// the fire button), holds use, throws the flashbang and places the
// claymores for real where it can.  What it cannot do is logged SIMULATED.
crash_route()
{
	p = getPlayers()[ 0 ];
	if ( !isDefined( p ) )
		return;
	p endon( "disconnect" );
	codolprintln( "CoDOL crashtest: crashroute starts" );
	last = undefined;
	for ( ;; )
	{
		wait( 0.5 );
		if ( level.coop_mission_over )
		{
			codolprintln( "CoDOL crashtest: crashroute sees the mission over" );
			return;
		}
		s = level.crash_step;
		if ( !isDefined( s ) || !isAlive( p ) )
			continue;
		// Keep up with Ghost, as a player following him would.
		if ( isDefined( level.crash_ghost ) && distance( p.origin, level.crash_ghost.origin ) > 700 &&
		     ( s.kind == "kill" || s.kind == "near" ) && level.crash_phase != "download" )
			route_walk( p, level.crash_ghost.origin + ( 50, 50, 0 ), "Ghost" );
		if ( isDefined( last ) && last == s )
		{
			if ( s.kind == "kill" || s.kind == "download" || s.kind == "roof" || s.kind == "near" )
				route_act( p, s );
			continue;
		}
		last = s;
		route_act( p, s );
	}
}

route_act( p, s )
{
	switch ( s.kind )
	{
	case "near":
	case "roof":
		route_walk( p, s.value, s.kind );
		break;
	case "use":
		route_walk( p, s.value, "use" );
		p setPlayerAngles( vectorToAngles( s.value - p getEye() ) );
		route_use( p, s );
		break;
	case "kill":
		route_fight( p );
		break;
	case "flash":
		route_walk( p, s.value + ( 120, -60, 0 ), "flash spot" );
		p botLookAt( s.value, 2 );
		p setPlayerAngles( vectorToAngles( s.value - p getEye() ) );
		wait( 0.5 );
		// The special offhand button (C1 usercmd 0x8000), as a player throws.
		p codol_botPressButtons( 32768, 0.4 );
		codolprintln( "CoDOL crashtest: flashbang button pressed, carrying " + p getCurrentOffhand_text() );
		wait( 3 );
		if ( !isDefined( level.crash_flash_seen ) )
		{
			codolprintln( "CoDOL crashtest: flashbang throw SIMULATED (no grenade_fire from the test client)" );
			level.crash_flash_seen = true;
		}
		break;
	case "claymore":
		for ( i = 0; i < s.value.size; i++ )
		{
			if ( isDefined( s.value[ i ].crash_done ) )
				continue;
			route_walk( p, s.value[ i ].origin, "claymore spot " + i );
			// Look down at the spot and press the equipment button (0x10000).
			p setPlayerAngles( ( 45, p.angles[ 1 ], 0 ) );
			wait( 0.2 );
			p codol_botPressButtons( 65536, 0.4 );
			codolprintln( "CoDOL crashtest: claymore " + i + " button pressed" );
			wait( 2.5 );
			if ( !isDefined( s.value[ i ].crash_done ) )
			{
				codolprintln( "CoDOL crashtest: claymore " + i + " SIMULATED" );
				s.value[ i ].crash_done = true;
				level.crash_claymores_placed++;
			}
		}
		break;
	case "download":
		// Stay by the laptop and fight.
		if ( distance( p.origin, s.value ) > 300 )
			route_walk( p, s.value + ( 60, -120, 0 ), "laptop" );
		route_fight( p );
		break;
	}
}

route_use( p, s )
{
	// The use button for 1.2 s (usercmd button 40 is +activate on this
	// server's bot path: codol_botPressButtons), else the harness relays it.
	held = 0;
	while ( held < 3.5 && level.crash_step == s )
	{
		p codol_botPressButtons( 40, 0.3 );
		wait( 0.3 );
		held += 0.3;
	}
	if ( level.crash_step == s )
	{
		codolprintln( "CoDOL crashtest: use at " + s.value + " SIMULATED" );
		trig = undefined;
		ents = getEntArray( "trigger_use", "classname" );
		for ( i = 0; i < ents.size; i++ )
		{
			if ( distance( ents[ i ].origin, s.value ) < 200 )
				ents[ i ].crash_used_by = p;
		}
		level.crash_force_use = p;
	}
}

getCurrentOffhand_text()
{
	text = "";
	list = self getWeaponsList();
	for ( i = 0; i < list.size; i++ )
		text += list[ i ] + " ";
	return text;
}

route_walk( p, dest, what )
{
	p botMoveTo( dest );
	last = p.origin;
	still = 0;
	for ( t = 0; t < 40; t += 0.5 )
	{
		wait( 0.5 );
		if ( !isAlive( p ) )
			return;
		d = p.origin - dest;
		if ( length( ( d[ 0 ], d[ 1 ], 0 ) ) < 60 && crash_abs( d[ 2 ] ) < 90 )
			break;
		if ( distance( p.origin, last ) < 10 )
			still += 0.5;
		else
			still = 0;
		last = p.origin;
		if ( still >= 3 )
		{
			codolprintln( "CoDOL crashtest: walk to " + what + " " + dest + " STUCK at " + p.origin + "; teleporting" );
			p setOrigin( dest );
			break;
		}
	}
	p botStop();
}

// Shoot the nearest living enemy the player can see (real bullets); after a
// while without a shot at anyone, walk toward the nearest one.
route_fight( p )
{
	best = undefined;
	bestDist = 3000;
	for ( i = 0; i < level.coop_enemies.size; i++ )
	{
		e = level.coop_enemies[ i ];
		if ( !isDefined( e ) || !isAlive( e ) )
			continue;
		d = distance( p.origin, e.origin );
		if ( d < bestDist )
		{
			best = e;
			bestDist = d;
		}
	}
	if ( !isDefined( best ) )
		return;
	aim = best.origin + ( 0, 0, 46 );
	if ( bulletTracePassed( p getEye(), aim, false, p ) )
	{
		p botStop();
		p setPlayerAngles( vectorToAngles( aim - p getEye() ) );
		p botLookAt( aim, 1 );
		p codol_botPressButtons( 1, 0.6 );
		wait( 0.6 );
		return;
	}
	if ( bestDist > 200 )
	{
		p botMoveTo( best.origin );
		wait( 1.5 );
		p botStop();
	}
}
