// zm audio: the playable characters' quotes, the round music and the level's
// ambient sound.
//
// Behaviour reference is T5's maps\_zombiemode_audio.gsc (create_and_play_dialog
// :649, do_player_playvox :746, player_killstreak_timer :885,
// player_zombie_kill_vox :931, get_mod_type :982, perks_a_cola_jingle_timer
// :1146, init_music_states :1247, change_zombie_music :1301), the ammo watcher
// in maps\_zombiemode.gsc:327 track_players_ammo_count, and the client halves
// T5 keeps in CSC: clientscripts\zombie_cod5_<map>_amb.csc (the "WAVE" music
// state and the amb_spooky_2d ambient package) and clientscripts\_audio.csc
// (the map's "random" / "looper" / "line_emitter" sound structs).  C1 runs no
// CSC, so the server drives all of it.
//
// EVERY ALIAS HERE IS C1's, MEASURED in the zones a zm match mounts
// (docs/zone_assets/, regenerated from client_shipRetail):
//
//  * Character quotes live in localized_common_gametype_zm, one alias per line
//    and character, named <code>_<line> with a <code>_<line>_2d twin.  <code>
//    is column 1 of mp/cyborgclasstable.csv (lb, danby, hh, lg), the same code
//    _zombiemode_laststand's revive line already uses.  Not every character
//    has every line (only danby has purchasebadweapon / shrinktrap / tesla /
//    trap, only hh has moneyball / monkeybomb, ...), so each alias is checked
//    with soundExists() before it is played and a missing one is silent.
//    T5's vox_plr_<n>_<category>_<variant> names do not exist in C1.
//  * Music: common_gametype_zm ships mus_zombie_start, mus_zombie_round_start,
//    mus_zombie_round_over, mus_zombie_game_over and mus_zombie_wave_loop (the
//    only looping one; all five are on C1's music channel 32), plus the perk
//    machines' mus_perks_<perk>_jingle / _sting.
//  * Ambience: amb_spooky_2d ships in all three zm map zones; zm_asylum_sh
//    also ships the aliases its own sound structs name.  The prototype and
//    training structs name amb_spooky / amb_zombies_*, which those zones do
//    NOT carry -- soundExists() filters them.
//
// THE SOUND BANK.  playSound / playLocalSound / playSoundAtPosition register
// the alias in the 255-slot CS_SOUNDS bank (G_SoundAliasIndex), and an
// overflow is a fatal map error.  Music goes through musicPlay, which sends
// the alias NAME (C1 command 'l', resolved client side) and takes no slot.
// A character only ever registers its own lines, so a full match adds at most
// four characters' worth.  `codol_limits` prints the bank's use.
//
// The quotes are 3D sounds on the speaking player (T5 do_player_playvox plays
// `self playsound( alias )`); the _2d twins are not used.  One player speaks
// at a time, level-wide, as in T5 (level.player_is_speaking).

#include maps\mp\gametypes\_zombiemode_utility;

// Measured speech length is not available to script; T5 waits for the
// sound_done notify.  The longest C1 quote is a few seconds; hold the
// level-wide speaker lock for this long.
VOX_HOLD = 3.5;

init()
{
	level.player_is_speaking = false;
	level.zmb_alias_exists = [];
	level.zmb_music_state = undefined;
	level.music_override = false;

	level thread on_player_connect();
	level thread ambient_package();
	level thread ambient_structs();
	level thread perk_jingles();

	// The match-wide music bed and stingers.
	level thread music_round_watch();
	level thread music_game_over_watch();

	if ( getDvarInt( "developer" ) > 0 )
		level thread audiotest_watch();
}

// Developer test verbs, scr_codol_audiotest:
//   lines              each player: which of its character's lines exist
//   vox <line>         every player says <line> (speaker lock bypassed)
//   music <state>      game_start / round_start / round_end / game_over
audiotest_watch()
{
	setDvar( "scr_codol_audiotest", "" );
	all = strTok( "headshot multikill lowammo noammo perkcola purchase purchasegoodweapon purchasebadweapon nocashtopurchase matchstart matchend personality revivemanytimes whilerevivingteammate firetrap frost frostgrenade shrinktrap trap tesla monkeybomb moneyball teleporter reviveknife russiandolls", " " );
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_codol_audiotest" );
		if ( request == "" )
			continue;
		setDvar( "scr_codol_audiotest", "" );
		args = strTok( request, " " );
		players = getPlayers();

		if ( args[ 0 ] == "lines" )
		{
			for ( i = 0; i < players.size; i++ )
			{
				have = "";
				for ( j = 0; j < all.size; j++ )
				{
					if ( isDefined( players[ i ] vox_alias( all[ j ] ) ) )
						have = have + " " + all[ j ];
				}
				codolprintln( "CoDOL zm audio: " + players[ i ].name + " code " + players[ i ] vox_code() + ":" + have );
			}
		}
		else if ( args[ 0 ] == "vox" && args.size > 1 )
		{
			for ( i = 0; i < players.size; i++ )
			{
				level.player_is_speaking = false;
				ok = players[ i ] create_and_play_dialog( args[ 1 ], true );
				codolprintln( "CoDOL zm audio: " + players[ i ].name + " vox " + args[ 1 ] + " -> " + ok );
			}
		}
		else if ( args[ 0 ] == "music" && args.size > 1 )
		{
			level.zmb_music_state = undefined;
			change_zombie_music( args[ 1 ] );
			codolprintln( "CoDOL zm audio: music " + args[ 1 ] );
		}
	}
}

// --------------------------------------------------------------------------
// Alias bookkeeping
// --------------------------------------------------------------------------

alias_exists( alias )
{
	if ( !isDefined( level.zmb_alias_exists[ alias ] ) )
		level.zmb_alias_exists[ alias ] = soundExists( alias );
	return level.zmb_alias_exists[ alias ];
}

// --------------------------------------------------------------------------
// Music.  T5 change_zombie_music with C1's channels: the WAVE state's looping
// bed is musicPlay'd, the alias stingers are 2D local sounds on every player
// so they play over the bed, and the SILENCE state is musicStop.
// --------------------------------------------------------------------------

play_sound_2d_all( alias )
{
	if ( !alias_exists( alias ) )
		return;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] playLocalSound( alias );
}

change_zombie_music( state )
{
	if ( isDefined( level.zmb_music_state ) && level.zmb_music_state == "game_over" )
		return;

	switch ( state )
	{
		case "game_start":
			// C1's mus_zombie_start, the opening stinger T5's zm maps did not
			// have (WaW's splash screen music had its slot).  Played in place
			// of round 1's round-start stinger.
			play_sound_2d_all( "mus_zombie_start" );
			music_wave_start();
			break;

		case "round_start":
			// T5: round_start sets musicstate WAVE (musicAliasloop
			// mus_zombie_wave_loop) and plays mus_zombie_round_start 2D.
			play_sound_2d_all( "mus_zombie_round_start" );
			music_wave_start();
			break;

		case "round_end":
			// T5: musicstate SILENCE + mus_zombie_round_over.
			music_wave_stop();
			play_sound_2d_all( "mus_zombie_round_over" );
			break;

		case "game_over":
			// T5: musicstate SILENCE + mus_zombie_game_over, and nothing after.
			music_wave_stop();
			play_sound_2d_all( "mus_zombie_game_over" );
			break;
	}

	level.zmb_music_state = state;
}

music_wave_start()
{
	if ( is_true( level.zmb_wave_playing ) || !alias_exists( "mus_zombie_wave_loop" ) )
		return;
	level.zmb_wave_playing = true;
	musicPlay( "mus_zombie_wave_loop" );
}

music_wave_stop()
{
	if ( !is_true( level.zmb_wave_playing ) )
		return;
	level.zmb_wave_playing = false;
	// T5's WAVE state fades out over 4 s (musicAliasloop( .., 0, 4 )).
	musicStop( 4 );
}

music_round_watch()
{
	level endon( "end_game" );

	for ( ;; )
	{
		level waittill( "start_of_round" );
		if ( level.round_number <= 1 )
			level thread change_zombie_music( "game_start" );
		else
			level thread change_zombie_music( "round_start" );

		// T5 play_level_start_vox_delayed (_zombiemode.gsc:6357): five
		// seconds into the first round a random player says the intro line.
		if ( level.round_number <= 1 )
			level thread play_level_start_vox_delayed();

		level waittill( "end_of_round" );
		level thread change_zombie_music( "round_end" );
		level thread round_end_banter();
	}
}

music_game_over_watch()
{
	level waittill( "end_game" );
	change_zombie_music( "game_over" );

	// C1's <code>_matchend: one survivor (or anyone left) signs off.
	players = getPlayers();
	if ( players.size > 0 )
	{
		level.player_is_speaking = false;
		players[ randomInt( players.size ) ] thread create_and_play_dialog( "matchend", true );
	}
}

play_level_start_vox_delayed()
{
	wait( 5 );
	players = getPlayers();
	if ( players.size > 0 )
		players[ randomInt( players.size ) ] create_and_play_dialog( "matchstart" );
}

// C1's <code>_personality has no T5 trigger; it is the character's idle
// banter, spoken now and then in the lull after a round.
round_end_banter()
{
	wait( 6 );
	if ( randomInt( 100 ) >= 35 )
		return;
	players = getPlayers();
	if ( players.size > 0 )
		players[ randomInt( players.size ) ] create_and_play_dialog( "personality" );
}

// --------------------------------------------------------------------------
// Character quotes
// --------------------------------------------------------------------------

// self = player.  The roster code that prefixes this player's lines.
vox_code()
{
	if ( isDefined( self.zmb_vox_code ) && isDefined( self.zombie_character ) &&
		isDefined( self.zmb_vox_code_key ) && self.zmb_vox_code_key == self.zombie_character )
		return self.zmb_vox_code;

	code = self maps\mp\gametypes\_zombiemode_laststand::revive_vox_code();
	self.zmb_vox_code = code;
	self.zmb_vox_code_key = self.zombie_character;
	return code;
}

// self = player.  The alias for this player's <line>, or undefined.
vox_alias( line )
{
	code = self vox_code();
	if ( !isDefined( code ) )
		return undefined;
	alias = code + "_" + line;
	if ( !alias_exists( alias ) )
		return undefined;
	return alias;
}

// self = player.  T5 create_and_play_dialog + do_player_playvox: no speech in
// last stand, one speaker at a time.
create_and_play_dialog( line, force )
{
	self endon( "disconnect" );

	if ( !isDefined( self ) || !isPlayer( self ) )
		return false;
	if ( self.sessionstate != "playing" && !is_true( force ) )
		return false;
	if ( is_true( self.laststand ) && !is_true( force ) )
		return false;

	alias = self vox_alias( line );
	if ( !isDefined( alias ) )
		return false;

	if ( level.player_is_speaking )
		return false;

	level.player_is_speaking = true;
	self playSound( alias );
	self thread release_speaker_lock();
	return true;
}

release_speaker_lock()
{
	wait( VOX_HOLD );
	level.player_is_speaking = false;
}

// self = player.  An item line: the user says it when the character has it,
// otherwise the nearest teammate within T5's response radius (500 units,
// setup_hero_rival) who has the line answers.  C1 hands several item lines to
// characters that do not carry the item (hh_monkeybomb, hh_moneyball,
// danby_shrinktrap), which only makes sense as a reaction.
play_item_dialog( line )
{
	if ( isDefined( self vox_alias( line ) ) )
	{
		self create_and_play_dialog( line );
		return;
	}

	best = undefined;
	best_dist = 500 * 500;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( p == self || p.sessionstate != "playing" || is_true( p.laststand ) )
			continue;
		if ( !isDefined( p vox_alias( line ) ) )
			continue;
		d = distanceSquared( p.origin, self.origin );
		if ( d < best_dist )
		{
			best = p;
			best_dist = d;
		}
	}

	if ( isDefined( best ) )
		best create_and_play_dialog( line );
}

on_player_connect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread player_vox_watchers();
	}
}

player_vox_watchers()
{
	self thread watch_kills();
	self thread watch_ammo();
	self thread watch_perks();
	self thread watch_items();
	self thread watch_traps();
	self thread watch_revived();
}

// T5 player_zombie_kill_vox / get_mod_type, reduced to the kill lines C1
// ships: headshot (a bullet to the head from beyond 400 units, not under
// instakill; T5's chance 99) and multikill (T5 player_killstreak_timer: more
// than 7 kills inside 5 seconds).
watch_kills()
{
	self endon( "disconnect" );

	kill_times = [];
	for ( ;; )
	{
		self waittill( "zom_kill", zombie );

		// Multikill.
		now = getTime();
		recent = [];
		for ( i = 0; i < kill_times.size; i++ )
		{
			if ( now - kill_times[ i ] < 5000 )
				recent[ recent.size ] = kill_times[ i ];
		}
		recent[ recent.size ] = now;
		kill_times = recent;
		if ( kill_times.size > 7 )
		{
			kill_times = [];
			self thread create_and_play_dialog( "multikill" );
			continue;
		}

		if ( !isDefined( zombie ) || is_true( self.zmb_kill_line_wait ) )
			continue;

		mod = zombie.damagemod;
		loc = zombie.damagelocation;
		if ( !isDefined( mod ) || !isDefined( loc ) )
			continue;
		if ( mod != "MOD_RIFLE_BULLET" && mod != "MOD_PISTOL_BULLET" )
			continue;
		if ( loc != "head" && loc != "helmet" )
			continue;
		if ( is_true( level.zombie_vars[ "zombie_insta_kill" ] ) )
			continue;
		if ( distanceSquared( self.origin, zombie.origin ) <= 400 * 400 )
			continue;

		self thread kill_line( "headshot" );
	}
}

kill_line( line )
{
	self endon( "disconnect" );
	self.zmb_kill_line_wait = true;
	self create_and_play_dialog( line );
	wait( 2 );
	self.zmb_kill_line_wait = false;
}

// T5 track_players_ammo_count: low ammo below 5 rounds left in the held
// weapon, out of ammo when it reads 0 for two seconds; each line then rests
// for 20 seconds.  Only primaries count (T5 excluded every bottle, knuckle
// crack, equipment and launcher-attachment pseudo-weapon by name; C1's
// weaponInventoryType answers the same question).
watch_ammo()
{
	self endon( "disconnect" );

	wait( 5 );
	low_ready = 0;
	out_ready = 0;

	for ( ;; )
	{
		wait( 0.5 );

		if ( self.sessionstate != "playing" || is_true( self.laststand ) || is_true( self.is_drinking ) )
			continue;

		weapon = self getCurrentWeapon();
		if ( !isDefined( weapon ) || weapon == "none" )
			continue;
		if ( weaponInventoryType( weapon ) != "primary" )
			continue;

		ammo = self getWeaponAmmoClip( weapon ) + self getWeaponAmmoStock( weapon );
		if ( ammo > 5 )
			continue;

		now = getTime();
		if ( ammo > 0 && ammo < 5 )
		{
			if ( now >= low_ready && self create_and_play_dialog( "lowammo" ) )
				low_ready = now + 20000;
			continue;
		}

		if ( ammo == 0 && now >= out_ready )
		{
			wait( 2 );
			if ( !isDefined( self ) || self getCurrentWeapon() != weapon )
				continue;
			if ( self getWeaponAmmoClip( weapon ) + self getWeaponAmmoStock( weapon ) != 0 )
				continue;
			if ( self create_and_play_dialog( "noammo" ) )
				out_ready = getTime() + 20000;
		}
	}
}

// T5 perk_vox: 1.5 s after the bottle, the buyer names the drink.  C1 has a
// single <code>_perkcola line for every machine.  The machine's sting (T5
// vending_trigger_think :1383 play_jingle_or_stinger( script_label )) plays
// at the machine the perk came from.
watch_perks()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "perk_bought", perk );
		level thread perk_sting( perk, self.origin );
		self thread perk_vox();
	}
}

perk_vox()
{
	self endon( "disconnect" );
	wait( 1.5 );
	self create_and_play_dialog( "perkcola" );
}

perk_sting( perk, origin )
{
	machine = perk_machine( perk, origin );
	if ( !isDefined( machine ) || !isDefined( machine.script_label ) )
		return;
	machine thread play_jingle_or_stinger( machine.script_label );
}

perk_machine( perk, origin )
{
	machines = getEntArray( "zombie_vending", "targetname" );
	best = undefined;
	best_dist = undefined;
	for ( i = 0; i < machines.size; i++ )
	{
		if ( !isDefined( machines[ i ].script_noteworthy ) || machines[ i ].script_noteworthy != perk )
			continue;
		d = distanceSquared( machines[ i ].origin, origin );
		if ( !isDefined( best_dist ) || d < best_dist )
		{
			best = machines[ i ];
			best_dist = d;
		}
	}
	return best;
}

// Item lines.  The thrown offhands arrive on the engine's grenade_fire; the
// placeable traps and the level's electric/fire traps on zm_trap_activated
// (_zombiemode_traps_placeable / _zombiemode_traps).
watch_items()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "grenade_fire", grenade, weapon );
		if ( !isDefined( weapon ) )
			continue;

		line = undefined;
		switch ( weapon )
		{
			case "zombie_cymbal_monkey":	line = "monkeybomb"; break;
			case "money_ball_zm":			line = "moneyball"; break;
			case "teleporter_zm":			line = "teleporter"; break;
			case "revive_knife_zm":			line = "reviveknife"; break;
			case "zombie_nesting_dolls":	line = "russiandolls"; break;
		}

		if ( isDefined( line ) )
			self thread play_item_dialog( line );
	}
}

watch_traps()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "zm_trap_activated", weapon );
		if ( !isDefined( weapon ) )
			continue;

		line = undefined;
		if ( weapon == "trap_fire_zm" || isSubStr( weapon, "fire" ) )
			line = "firetrap";
		else if ( weapon == "trap_freeze_zm" )
			line = "frost";
		else if ( weapon == "trap_shrink_zm" )
			line = "shrinktrap";
		else
			line = "trap";

		// hh's freeze line is named frostgrenade.
		if ( line == "frost" && !isDefined( self vox_alias( "frost" ) ) && isDefined( self vox_alias( "frostgrenade" ) ) )
			line = "frostgrenade";

		self thread play_item_dialog( line );
	}
}

// C1's <code>_revivemanytimes: the reviver, once the same teammate has gone
// down three times or more.
watch_revived()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "player_revived", reviver );
		if ( !isDefined( reviver ) || !isPlayer( reviver ) || reviver == self )
			continue;
		if ( !isDefined( self.downs ) || self.downs < 3 )
			continue;
		reviver thread revive_many_line();
	}
}

revive_many_line()
{
	self endon( "disconnect" );
	wait( 1 );
	self create_and_play_dialog( "revivemanytimes" );
}

// self = player.  Called by the purchase sites: no money (T5 "no_money" /
// "perk_deny" / "door_deny", all "nomoney").
no_cash_vox()
{
	if ( isDefined( self.zmb_nocash_time ) && getTime() - self.zmb_nocash_time < 5000 )
		return;
	self.zmb_nocash_time = getTime();
	self thread create_and_play_dialog( "nocashtopurchase" );
}

// self = player.  A wall weapon bought.
purchase_vox()
{
	self thread create_and_play_dialog( "purchase" );
}

// self = player.  A mystery box weapon taken.  T5 weapon_pickup picks a line
// by weapon class ("wpck_crappy" for a pistol, the wonder weapons their own);
// C1 has a good / bad pair (bad is danby's only) and danby's Wunderwaffe line.
box_weapon_vox( weapon )
{
	if ( !isDefined( weapon ) )
		return;

	if ( isSubStr( weapon, "tesla_gun" ) && isDefined( self vox_alias( "tesla" ) ) )
	{
		self thread create_and_play_dialog( "tesla" );
		return;
	}

	if ( is_good_box_weapon( weapon ) )
	{
		self thread create_and_play_dialog( "purchasegoodweapon" );
		return;
	}

	if ( weaponClass( weapon ) == "pistol" && !isSubStr( weapon, "dw" ) )
		self thread create_and_play_dialog( "purchasebadweapon" );
}

is_good_box_weapon( weapon )
{
	if ( isSubStr( weapon, "ray_gun" ) || isSubStr( weapon, "tesla_gun" ) || isSubStr( weapon, "thundergun" ) )
		return true;
	if ( isSubStr( weapon, "crossbow" ) || isSubStr( weapon, "sniper_explosive" ) || isSubStr( weapon, "knife_ballistic" ) )
		return true;
	if ( isSubStr( weapon, "upgraded" ) )
		return true;
	wclass = weaponClass( weapon );
	return wclass == "mg";
}

// --------------------------------------------------------------------------
// Perk machine jingles.  T5 perks_a_cola_jingle_timer: once the machine has
// power, every 31-45 s a 15% chance to play its jingle (the trigger's
// script_sound, e.g. mus_perks_speed_jingle), never over another jingle.
// play_random_broken_sounds' surges stay out: evt_electrical_surge is the
// power module's.
// --------------------------------------------------------------------------

perk_jingles()
{
	machines = getEntArray( "zombie_vending", "targetname" );
	for ( i = 0; i < machines.size; i++ )
		machines[ i ] thread perks_a_cola_jingle_timer();
}

perks_a_cola_jingle_timer()
{
	self endon( "death" );

	if ( !isDefined( self.script_noteworthy ) || !isDefined( self.script_sound ) )
		return;
	if ( !alias_exists( self.script_sound ) )
		return;

	// The power module fans <perk>_power_on out after flag "power_on".
	if ( flag_exist( "power_on" ) )
		flag_wait( "power_on" );

	for ( ;; )
	{
		wait( randomFloatRange( 31, 45 ) );
		if ( randomInt( 100 ) < 15 )
			self thread play_jingle_or_stinger( self.script_sound );
	}
}

play_jingle_or_stinger( alias )
{
	if ( !isDefined( alias ) || !alias_exists( alias ) )
		return;
	if ( is_true( self.jingle_is_playing ) || is_true( level.music_override ) )
		return;

	self.jingle_is_playing = true;
	self playSound( alias );
	wait( 12 );
	self.jingle_is_playing = false;
}

// --------------------------------------------------------------------------
// Ambience
// --------------------------------------------------------------------------

// T5 zombie_cod5_<map>_amb.csc: every zm room carries the ambient package
// element addAmbientElement( "amb_spooky_2d", 5, 8, ... ) -- a 2D whisper
// every 5-8 seconds.  Per player, as the client ran it.
ambient_package()
{
	if ( !alias_exists( "amb_spooky_2d" ) )
		return;

	for ( ;; )
	{
		level waittill( "connected", player );
		player thread ambient_package_player();
	}
}

ambient_package_player()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		wait( randomFloatRange( 5, 8 ) );
		if ( self.sessionstate == "playing" )
			self playLocalSound( "amb_spooky_2d" );
	}
}

// T5 clientscripts\_audio.csc startSoundRandoms / startSoundLoops /
// startLineEmitters over the map's own script_structs.  A "random" struct
// plays its script_sound at its origin every script_wait_min..max seconds
// (default 1..3); a "looper" loops it at its origin; a "line_emitter" loops it
// along a segment toward its target, which a server can only approximate --
// the loop sits at the segment's midpoint.  A random emitter only fires with a
// player within 1500 units, so the far side of the map sends nothing.
ambient_structs()
{
	if ( !isDefined( level.struct ) )
		return;

	wait( 1 );

	for ( i = 0; i < level.struct.size; i++ )
	{
		s = level.struct[ i ];
		if ( !isDefined( s ) || !isDefined( s.script_label ) || !isDefined( s.script_sound ) || !isDefined( s.origin ) )
			continue;
		if ( !alias_exists( s.script_sound ) )
			continue;

		if ( s.script_label == "random" )
			level thread sound_random_thread( s );
		else if ( s.script_label == "looper" && is_true_string( s.script_looping ) && is_amb( s.script_sound ) )
			level thread sound_loop_at( s.script_sound, s.origin );
		else if ( s.script_label == "line_emitter" && isDefined( s.target ) )
			level thread sound_line_emitter( s );
	}
}

is_true_string( value )
{
	// Struct spawn vars arrive typed: script_looping "1" is the int 1.
	if ( !isDefined( value ) )
		return false;
	text = "" + value;
	return text != "" && text != "0";
}

// Loopers that are not amb_* are fire and light hums the map's createfx
// already places, and C1 plays createfx's loop sounds itself
// (docs/CODOL_CLIENT_AMBIENT_EMITTERS_20260915.md).
is_amb( alias )
{
	return getSubStr( alias, 0, 4 ) == "amb_";
}

sound_random_thread( s )
{
	level endon( "end_game" );

	wait_min = 1;
	wait_max = 3;
	if ( isDefined( s.script_wait_min ) )
		wait_min = float( s.script_wait_min );
	if ( isDefined( s.script_wait_max ) )
		wait_max = float( s.script_wait_max );
	if ( wait_max < wait_min )
		wait_max = wait_min;

	for ( ;; )
	{
		wait( randomFloatRange( wait_min, wait_max + 0.01 ) );
		if ( !player_near( s.origin, 1500 ) )
			continue;
		playSoundAtPosition( s.script_sound, s.origin );
	}
}

player_near( origin, radius )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( distanceSquared( players[ i ].origin, origin ) < radius * radius )
			return true;
	}
	return false;
}

sound_loop_at( alias, origin )
{
	ent = spawn( "script_origin", origin );
	ent playLoopSound( alias );
}

sound_line_emitter( s )
{
	target = undefined;
	for ( i = 0; i < level.struct.size; i++ )
	{
		t = level.struct[ i ];
		if ( isDefined( t ) && isDefined( t.targetname ) && t.targetname == s.target )
		{
			target = t;
			break;
		}
	}
	if ( !isDefined( target ) || !isDefined( target.origin ) )
		return;

	mid = ( s.origin + target.origin ) * 0.5;
	if ( is_true_string( s.script_looping ) )
		sound_loop_at( s.script_sound, mid );
	else
		playSoundAtPosition( s.script_sound, mid );
}
