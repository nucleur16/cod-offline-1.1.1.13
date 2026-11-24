#include maps\mp\_utility;

// survival audio: the announcer and the round music.
//
// The behaviour is IW5 Spec Ops Survival's (maps\_so_survival and its dialog
// module; D:\CoD\COD-GSC-Source\IW5-GSC\scripts\1571.gsc, 1576.gsc):
//   announcer  1.5 s after a wave starts, what it brings (the first wave: the
//              mission intro; a wave with specials: the special announced
//              least so far; else "enemies inbound"); the boss helicopters and
//              juggernauts as they are sent; the enemies' claymores once a
//              wave; 1.5 s after a wave ends, "wave over".
//   music      the Survival theme 1.5 s after the landing, 5 s of it and then
//              a 20 s fade; a sting as a wave starts and as it is cleared; no
//              music in an ordinary wave or in the intermission; a boss wave's
//              own track (one for helicopters, one for juggernauts), started
//              again 2 s after it ends, until the wave is cleared (3 s fade);
//              a beep a second in the last five of the intermission.
// Additions: the last enemy, a teammate down or revived, the last man
// standing, the reinforcement squad and the support a kiosk sends at once.
//
// The sounds are CoD Online's. Two announcers ship. The multiplayer one
// (UK_1mc_*) and the co-op partners speak English in 1.1.1.13 and are the
// default; Survival's own (2.6.9.7) says more, but only in Chinese. That
// announcer, the boss music and the stings come in an optional pack
// (tools/survival_audio: the zone patch_localized_common_gametype_survival and
// iw_sound_22.iwd); without it the nearest 1.1.1.13 sound plays, or none.
// pick() asks soundExists() for each, so an alias that no loaded zone carries
// is never sent to a client.
//
// One line at a time: each player has a queue with priorities, and it shares
// the multiplayer announcer's "busy" state (leaderDialogActive), so a
// killstreak line and a Survival line never overlap.  Lines wait out a
// player's slam zoom and go stale; nothing is queued after the end, whose
// line and music are the multiplayer ones (mission_success / mission_fail,
// victory / defeat).
//
// Music goes to each client on its own (codol_playmusic / codol_stopmusic: C1
// server commands 'l' and 'm', the music stream, its volume the client's), so
// a player who joins or lands later is brought in without restarting anyone.
//
//   scr_survival_announcer  1 (default) the 1.1.1.13 lines; 2 Survival's own
//                           where the pack has one; 0 no lines
//   scr_survival_music      1 (default) as IW5; 2 the Survival theme at the
//                           start of ordinary waves too (IW5 has none there);
//                           0 no music

// _survival::precache: before the threads of _music_and_dialog::init.
precache()
{
	// _music_and_dialog leaves its spawn and suspense music and its
	// objective line to this module.
	level.survival_audio = true;
	level.survival_audio_lines = dvar_on( "scr_survival_announcer", 1 );
	level.survival_audio_music = dvar_on( "scr_survival_music", 1 );
	level.survival_lines = [];
	level.survival_line_order = [];
	level.survival_music = undefined;
	level.survival_announced = [];
}

dvar_on( name, value )
{
	if ( getDvar( name ) == "" )
		return value;
	return getDvarInt( name );
}

// _survival::init (onStartGameType: the factions are known).
init()
{
	mp = maps\mp\gametypes\_teams::getTeamVoicePrefix( "allies" ) + "1mc_";

	// key, the Survival announcer's alias and the seconds of its longest
	// variant, the 1.1.1.13 alias and its, priority, seconds it may wait,
	// seconds before it may be said again, dropped at the wave's end.
	add_line( "intro_coop", "so_hq_mission_intro", 6.4, mp + "readytomove", 1.5, 3, 30, 60, false );
	add_line( "intro_solo", "so_hq_mission_intro_sp", 5.0, mp + "readytomove", 1.5, 3, 30, 60, false );
	add_line( "generic", "so_hq_enemy_intel_generic", 3.0, "survival_vo_enemies", 2.4, 1, 8, 12, true );
	add_line( "martyrdom", "so_hq_enemy_intel_martyrdom", 3.1, "", 0, 2, 10, 12, true );
	add_line( "chemical", "so_hq_enemy_intel_chemical", 3.5, "", 0, 2, 10, 12, true );
	add_line( "hazmat", "so_hq_enemy_intel_chemical", 3.5, "", 0, 2, 10, 12, true );
	add_line( "dog_reg", "so_hq_enemy_intel_dog_reg", 4.1, mp + "enemy_dogs", 1.9, 2, 10, 12, true );
	add_line( "dog_splode", "so_hq_enemy_intel_dog_splode", 3.8, mp + "enemy_dogs", 1.9, 2, 10, 12, true );
	add_line( "claymore", "so_hq_enemy_update_claymore", 3.6, "", 0, 1, 8, 12, true );
	add_line( "chopper", "so_hq_boss_intel_chopper", 3.2, mp + "enemy_heli", 1.8, 3, 15, 5, true );
	add_line( "choppers", "so_hq_boss_intel_chopper_many", 4.5, mp + "enemy_heli", 1.8, 3, 15, 5, true );
	add_line( "missile_chopper", "so_hq_boss_intel_missile_chopper", 2.4, mp + "enemy_str_heli", 2.3, 3, 15, 5, true );
	// The juggernaut's carrier is the Mi-17 / Pave Low.
	add_line( "jug", "so_hq_enemy_intel_boss_transport", 4.4, mp + "enemy_heavy_heli", 2.3, 3, 15, 5, true );
	add_line( "jugs", "so_hq_enemy_intel_boss_transport_many", 3.8, mp + "enemy_heavy_heli", 2.3, 3, 15, 5, true );
	add_line( "wave_over", "so_hq_wave_over_flavor", 2.9, mp + "encourage_win", 2.9, 2, 10, 10, false );
	// IW5 says which armory a wave opened; here the kiosks are open from the
	// start and the first intermission points at them.
	add_line( "stations", "", 0, mp + "secure_supplies", 1.6, 1, 10, 600, false );
	add_line( "one_enemy", "", 0, mp + "one_enemy", 1.5, 1, 4, 20, true );
	add_line( "last_alive", "", 0, mp + "last_one", 1.4, 4, 4, 20, true );
	add_line( "player_down", "", 0, "survival_vo_player_down", 2.2, 4, 4, 3, true );
	add_line( "player_bleeding", "", 0, "survival_vo_player_bleeding", 2.9, 3, 4, 20, true );
	add_line( "player_revived", "", 0, "survival_vo_player_revived", 1.1, 1, 3, 3, true );
	add_line( "use_mortar", "", 0, mp + "use_mortar", 1.8, 2, 5, 5, false );
	add_line( "use_emp", "", 0, mp + "use_emp", 3.0, 2, 5, 5, false );

	// The music and the stings: alias and seconds.
	level.survival_track = [];
	add_track( "start", "survival_music_start", 121.9, "", 0 );
	add_track( "chopper", "so_survival_boss_music_01", 63.6, "survival_music_boss_chopper", 66.0 );
	add_track( "jug", "so_survival_boss_music_02", 61.9, "survival_music_boss_jug", 67.4 );
	level.survival_sting_start = pick( "survival_wave_start_splash", "mp_war_objective_taken" );
	level.survival_sting_end = pick( "survival_wave_end_splash", "mp_winning_short_stinger" );
	level.survival_beep = pick( "ui_mp_timer_countdown", "" );
	log_tables();

	level thread on_connect();
	level thread wave_start_lines();
	level thread wave_end_lines();
	level thread claymore_lines();
	level thread wave_music();
	level thread at_the_end();
}

// The shipped alias: `alias`, else `fallback`, else "" (nothing is played).
pick( alias, fallback )
{
	if ( alias != "" && soundExists( alias ) )
		return alias;
	if ( fallback != "" && soundExists( fallback ) )
		return fallback;
	return "";
}

add_line( key, own, ownLength, english, englishLength, priority, life, again, wave )
{
	s = spawnStruct();
	s.key = key;
	s.alias = pick( own, "" );
	s.length = ownLength;
	s.own = true;
	if ( s.alias == "" || level.survival_audio_lines != 2 )
	{
		s.alias = pick( english, "" );
		s.length = englishLength;
		s.own = false;
	}
	s.priority = priority;
	s.life = life * 1000;
	s.again = again * 1000;
	s.wave = wave;
	level.survival_lines[ key ] = s;
	level.survival_line_order[ level.survival_line_order.size ] = key;
}

add_track( key, alias, length, fallback, fallbackLength )
{
	s = spawnStruct();
	s.key = key;
	s.alias = pick( alias, fallback );
	s.length = length;
	if ( s.alias != alias )
		s.length = fallbackLength;
	level.survival_track[ key ] = s;
}

log_tables()
{
	own = 0;
	english = "";
	none = "";
	for ( i = 0; i < level.survival_line_order.size; i++ )
	{
		s = level.survival_lines[ level.survival_line_order[ i ] ];
		if ( s.alias == "" )
			none += " " + s.key;
		else if ( s.own )
			own++;
		else
			english += " " + s.key + "=" + s.alias;
	}
	codolprintln( "CoDOL survival audio: announcer " + level.survival_audio_lines + ", " + level.survival_line_order.size +
		" moments, " + own + " with Survival's own line; 1.1.1.13 lines:" + english + "; none:" + none );
	codolprintln( "CoDOL survival audio: music " + level.survival_audio_music + ", start " + level.survival_track[ "start" ].alias +
		", chopper " + level.survival_track[ "chopper" ].alias + ", jug " + level.survival_track[ "jug" ].alias +
		", stings " + level.survival_sting_start + " / " + level.survival_sting_end + ", beep " + level.survival_beep );
}

// --------------------------------------------------------------------------
// Players
// --------------------------------------------------------------------------

on_connect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player.survival_vo = [];
		player.survival_vo_said = [];
		player thread watch_down();
		player thread watch_revived();
		if ( player is_a_bot() )
			continue;
		player thread announcer();
		player thread landing();
	}
}

is_a_bot()
{
	return isDefined( self.isbot ) && self.isbot;
}

// In the match and on the ground: a player picking his class, or still in
// the slam zoom, hears no line and no sting.
landed()
{
	if ( !isDefined( self.hasSpawned ) || !self.hasSpawned )
		return false;
	return !( isDefined( self.survival_in_intro ) && self.survival_in_intro );
}

// The first landing: the theme at the start of the match, else whatever
// plays.
landing()
{
	self endon( "disconnect" );
	level endon( "survival_over" );
	self waittill( "spawned_player" );
	wait( 0.5 );
	while ( !self landed() )
		wait( 0.1 );
	if ( isDefined( level.survival_music ) )
	{
		self thread music_play( level.survival_music );
		return;
	}
	if ( isDefined( level.survival_first_wave_over ) )
		return;
	wait( 1.5 );
	self thread music_theme( 5, 20 );
}

// --------------------------------------------------------------------------
// The announcer
// --------------------------------------------------------------------------

// Queue line `key` for `who` (every player when undefined). Each player's
// announcer says it when it is free.
speak( key, who )
{
	if ( !level.survival_audio_lines || level.survival_over )
		return;
	s = level.survival_lines[ key ];
	if ( !isDefined( s ) || s.alias == "" )
		return;
	if ( !isDefined( who ) )
		who = getPlayers();
	for ( i = 0; i < who.size; i++ )
	{
		if ( isDefined( who[ i ] ) && isDefined( who[ i ].survival_vo ) && !( who[ i ] is_a_bot() ) )
			who[ i ] queue_line( s );
	}
}

queue_line( s )
{
	now = getTime();
	said = self.survival_vo_said[ s.alias ];
	if ( isDefined( said ) && now - said < s.again )
		return;
	for ( i = 0; i < self.survival_vo.size; i++ )
	{
		if ( self.survival_vo[ i ].alias == s.alias )
			return;
	}
	e = spawnStruct();
	e.key = s.key;
	e.alias = s.alias;
	e.length = s.length;
	e.priority = s.priority;
	e.wave = s.wave;
	e.until = now + s.life;

	// In front of the first line of a lower priority; four wait at most.
	queue = [];
	placed = false;
	for ( i = 0; i < self.survival_vo.size; i++ )
	{
		if ( !placed && self.survival_vo[ i ].priority < e.priority )
		{
			queue[ queue.size ] = e;
			placed = true;
		}
		queue[ queue.size ] = self.survival_vo[ i ];
	}
	if ( !placed )
		queue[ queue.size ] = e;
	self.survival_vo = [];
	for ( i = 0; i < queue.size && i < 4; i++ )
		self.survival_vo[ i ] = queue[ i ];
}

announcer()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		wait( 0.1 );
		if ( level.survival_over )
		{
			self.survival_vo = [];
			return;
		}
		if ( !self.survival_vo.size || !self landed() )
			continue;
		// A multiplayer line (a killstreak's) is on the air.
		if ( isDefined( self.leaderDialogActive ) && self.leaderDialogActive != "" )
			continue;
		e = self.survival_vo[ 0 ];
		queue = [];
		for ( i = 1; i < self.survival_vo.size; i++ )
			queue[ queue.size ] = self.survival_vo[ i ];
		self.survival_vo = queue;
		if ( getTime() > e.until )
			continue;
		self say_line( e );
	}
}

// Marked as the multiplayer announcer marks its own (_utility::
// playLeaderDialogOnPlayer), whose lines queue behind it and follow it.
say_line( e )
{
	self endon( "disconnect" );
	self.survival_vo_said[ e.alias ] = getTime();
	self.leaderDialogActive = e.alias;
	self playLocalSound( e.alias );
	self maps\mp\gametypes\_hud_message::codolAnnouncerLine( e.alias );
	codolprintln( "CoDOL survival audio: " + self.name + " hears " + e.key + " (" + e.alias + ")" );
	wait( e.length + 0.2 );
	if ( self.leaderDialogActive != e.alias )
		return;
	self.leaderDialogActive = "";
	if ( !isDefined( self.leaderDialogQueue ) || !self.leaderDialogQueue.size || !isDefined( self.pers[ "team" ] ) )
		return;
	next = self.leaderDialogQueue[ 0 ];
	rest = [];
	for ( i = 1; i < self.leaderDialogQueue.size; i++ )
		rest[ rest.size ] = self.leaderDialogQueue[ i ];
	self.leaderDialogQueue = rest;
	self thread maps\mp\_utility::playLeaderDialogOnPlayer( next, self.pers[ "team" ] );
}

// A wave's lines are not news after it.
drop_wave_lines()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( !isDefined( p.survival_vo ) )
			continue;
		keep = [];
		for ( j = 0; j < p.survival_vo.size; j++ )
		{
			if ( !p.survival_vo[ j ].wave )
				keep[ keep.size ] = p.survival_vo[ j ];
		}
		p.survival_vo = keep;
	}
}

wave_start_lines()
{
	level endon( "survival_over" );
	first = true;
	for ( ;; )
	{
		level waittill( "wave_started", num );
		level thread wave_start_line( num, first );
		first = false;
	}
}

wave_start_line( num, first )
{
	level endon( "survival_over" );
	level endon( "wave_ended" );
	wait( 1.5 );
	if ( first )
	{
		if ( getPlayers().size > 1 )
			speak( "intro_coop" );
		else
			speak( "intro_solo" );
	}
	else
		speak( wave_key( num ) );
	level thread last_enemy_line();
}

// The special announced least so far, as IW5 picks it; a wave whose specials
// have no line of their own is announced as any other.
wave_key( num )
{
	key = "generic";
	w = maps\mp\gametypes\_survival_ai::get_wave( num );
	if ( !isDefined( w ) )
		return key;
	least = 0;
	for ( i = 0; i < w.specials.size; i++ )
	{
		ref = w.specials[ i ];
		if ( !isDefined( level.survival_lines[ ref ] ) || level.survival_lines[ ref ].alias == "" )
			continue;
		count = 0;
		if ( isDefined( level.survival_announced[ ref ] ) )
			count = level.survival_announced[ ref ];
		if ( key == "generic" || count < least )
		{
			key = ref;
			least = count;
		}
	}
	if ( key != "generic" )
		level.survival_announced[ key ] = least + 1;
	return key;
}

// _survival::reinforce: the squad that replaces a fallen leader.
reinforced()
{
	speak( "generic" );
}

// _survival::boss_spawn / chopper_spawn, as they are sent: the line and the
// boss music.
boss_spawning( kind, refs )
{
	if ( kind == "chopper" )
	{
		// A level without helicopter paths sends none.
		if ( !isDefined( level.survival_boss_nodes ) || !level.survival_boss_nodes.size )
			return;
		count = 0;
		missile = false;
		for ( i = 0; i < refs.size; i++ )
		{
			if ( refs[ i ] == "jug_drop_chopper" )
				continue;
			count++;
			if ( refs[ i ] == "missile_chopper" )
				missile = true;
		}
		if ( !count )
			return;
		if ( missile )
			speak( "missile_chopper" );
		else if ( count > 1 )
			speak( "choppers" );
		else
			speak( "chopper" );
		// The juggernauts' track wins in a wave that has both.
		if ( !isDefined( level.survival_music ) || level.survival_music.key != "jug" )
			music_start( "chopper" );
		return;
	}
	count = 0;
	for ( i = 0; i < refs.size; i++ )
	{
		if ( isSubStr( refs[ i ], "jug_" ) )
			count++;
	}
	if ( !count )
		return;
	if ( count > 1 || !isDefined( level.survival_drop_starts ) || !level.survival_drop_starts.size )
		speak( "jugs" );
	else
		speak( "jug" );
	music_start( "jug" );
}

// One enemy left, once a wave.
last_enemy_line()
{
	level endon( "survival_over" );
	level endon( "wave_ended" );
	wait( 5 );
	for ( ;; )
	{
		wait( 0.5 );
		if ( !level.survival_aggressive || level.survival_boss_pending > 0 )
			continue;
		if ( maps\mp\gametypes\_survival::hud_enemy_count() != 1 )
			continue;
		speak( "one_enemy" );
		return;
	}
}

claymore_lines()
{
	level endon( "survival_over" );
	for ( ;; )
	{
		level waittill( "survival_claymore_planted" );
		speak( "claymore" );
		level waittill( "wave_ended" );
	}
}

wave_end_lines()
{
	level endon( "survival_over" );
	for ( ;; )
	{
		level waittill( "wave_ended" );
		first = !isDefined( level.survival_first_wave_over );
		level.survival_first_wave_over = true;
		drop_wave_lines();
		level thread wave_over_line( first );
	}
}

wave_over_line( first )
{
	level endon( "survival_over" );
	level endon( "wave_started" );
	wait( 1.5 );
	speak( "wave_over" );
	if ( first )
		speak( "stations" );
}

// A downed player calls his teammates; the last one standing is told so.
watch_down()
{
	self endon( "disconnect" );
	level endon( "survival_over" );
	for ( ;; )
	{
		self waittill( "player_downed" );
		// _survival and _laststand both raise it.
		wait( 0.3 );
		if ( !isDefined( self.laststand ) || !self.laststand )
			continue;
		others = [];
		up = [];
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( p == self )
				continue;
			others[ others.size ] = p;
			if ( isAlive( p ) && !( isDefined( p.laststand ) && p.laststand ) && p landed() )
				up[ up.size ] = p;
		}
		speak( "player_down", others );
		if ( up.size == 1 && others.size > 1 )
			speak( "last_alive", up );
		self thread bleeding_line( others );
		wait( 2 );
	}
}

// Still down with half the bleed-out time gone.
bleeding_line( others )
{
	self endon( "disconnect" );
	self endon( "player_revived" );
	self endon( "laststand_over" );
	self endon( "death" );
	level endon( "survival_over" );
	time = 120;
	if ( isDefined( level.laststand_vars ) && isDefined( level.laststand_vars[ "bleedout_time" ] ) )
		time = level.laststand_vars[ "bleedout_time" ];
	wait( time * 0.5 );
	if ( isDefined( self.laststand ) && self.laststand )
		speak( "player_bleeding", others );
}

// Whoever brought him back tells him so ("come on, get up").
watch_revived()
{
	self endon( "disconnect" );
	level endon( "survival_over" );
	for ( ;; )
	{
		self waittill( "player_revived", reviver );
		if ( !isDefined( reviver ) || !isPlayer( reviver ) || reviver == self )
			continue;
		who = [];
		who[ 0 ] = self;
		speak( "player_revived", who );
	}
}

// --------------------------------------------------------------------------
// The music
// --------------------------------------------------------------------------

// Every player who is on the ground.
sting( alias )
{
	if ( alias == "" || level.survival_over )
		return;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( !( p is_a_bot() ) && p landed() )
			p playLocalSound( alias );
	}
}

wave_music()
{
	level endon( "survival_over" );
	for ( ;; )
	{
		level waittill( "wave_started", num );
		if ( level.survival_audio_music )
		{
			sting( level.survival_sting_start );
			codolprintln( "CoDOL survival audio: wave " + num + " sting " + level.survival_sting_start );
		}
		if ( level.survival_audio_music > 1 && isDefined( level.survival_first_wave_over ) && !isDefined( level.survival_music ) )
			level thread ordinary_wave_music();
		level waittill( "wave_ended" );
		if ( level.survival_audio_music )
		{
			sting( level.survival_sting_end );
			codolprintln( "CoDOL survival audio: wave " + num + " over, sting " + level.survival_sting_end );
		}
		music_stop( 3 );
	}
}

// scr_survival_music 2: the theme at the start of a wave without a boss.
ordinary_wave_music()
{
	level endon( "survival_over" );
	level endon( "wave_ended" );
	wait( 1 );
	if ( isDefined( level.survival_music ) )
		return;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( !( players[ i ] is_a_bot() ) && players[ i ] landed() )
			players[ i ] thread music_theme( 15, 20 );
	}
}

// The intermission's last five seconds (_survival::intermission).
countdown_beep()
{
	if ( level.survival_audio_music )
		sting( level.survival_beep );
}

// A track for everyone, over and over, until music_stop.
music_start( key )
{
	t = level.survival_track[ key ];
	if ( !level.survival_audio_music || level.survival_over || !isDefined( t ) || t.alias == "" )
		return;
	if ( isDefined( level.survival_music ) && level.survival_music.key == key )
		return;
	level.survival_music = t;
	codolprintln( "CoDOL survival audio: music " + key + " (" + t.alias + ")" );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		// A player who has not landed gets it then (landing()).
		if ( !( players[ i ] is_a_bot() ) && players[ i ] landed() )
			players[ i ] thread music_play( t );
	}
}

music_stop( fade )
{
	level.survival_music = undefined;
	players = getPlayers();
	stopped = 0;
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		p notify( "survival_music" );
		if ( !isDefined( p.survival_music_on ) )
			continue;
		p.survival_music_on = undefined;
		p codol_stopmusic( fade );
		stopped++;
	}
	if ( stopped )
		codolprintln( "CoDOL survival audio: music off for " + stopped + " in " + fade + " s" );
}

// One player's copy of track `t`. The music stream plays one track: what it
// plays fades first.
music_play( t )
{
	self endon( "disconnect" );
	self notify( "survival_music" );
	self endon( "survival_music" );
	if ( isDefined( self.survival_music_on ) )
	{
		self codol_stopmusic( 1.5 );
		wait( 1.6 );
	}
	self.survival_music_on = true;
	for ( ;; )
	{
		self codol_playmusic( t.alias );
		codolprintln( "CoDOL survival audio: " + self.name + " music " + t.alias );
		wait( t.length + 2 );
	}
}

// The theme once: `full` seconds, then a fade.
music_theme( full, fade )
{
	self endon( "disconnect" );
	self notify( "survival_music" );
	self endon( "survival_music" );
	t = level.survival_track[ "start" ];
	if ( !level.survival_audio_music || level.survival_over || t.alias == "" )
		return;
	self.survival_music_on = true;
	self codol_playmusic( t.alias );
	codolprintln( "CoDOL survival audio: " + self.name + " music " + t.alias + ", " + full + " s and a " + fade + " s fade" );
	wait( full );
	self codol_stopmusic( fade );
	wait( fade );
	self.survival_music_on = undefined;
}

// The end: the multiplayer victory / defeat track takes over (game_win).
at_the_end()
{
	level waittill( "survival_over" );
	music_stop( 2 );
}
