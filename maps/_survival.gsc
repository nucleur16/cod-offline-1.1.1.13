// survival: Spec Ops Survival, the mode (its own match category).
//
// Ported from Infinity Ward's Modern Warfare 3 Spec Ops Survival,
// maps\_so_survival.gsc (IW5; D:\CoD\COD-GSC-Source\IW5-GSC\scriptso1.gsc
// in the decompiled dump): survival_init / survival_logic, the loadout from the
// wave table's 1000.. rows, survival_wave (squads, specials, bosses, the
// aggressive phase at four left, the end of a wave), the intermission with its
// ready-up, repeating waves past the table, and the credits.  CoDOL ran the
// same mode on its own data from 2.x on (common_gametype_survival); its tables
// are used as they are.  docs/CODOL_SURVIVAL_PLAN.md says what is ported from
// where and what differs.
//
// The category (fs_game / codol_zoneType survival) selects this layer:
// maps/_survival_gametype -> _survival_mode (rules) -> here.  The level is the
// unchanged multiplayer zone; the mode's per-map data (player starts, squad
// locations, kiosks, dog and chopper points) is maps/<level>_survival.gsc's.
// Names no multiplayer and no coop module.
//
// Differences from IW5, on purpose:
//   * the match ends when every player is down or dead, not at the first
//     death (four-player rooms); a player who died respawns at the next
//     intermission, and the downed are picked up at the end of a wave, as in
//     IW5 survival_wave_pickup_downed_players;
//   * the actor pool is 32 slots (level.survival_max_alive), so reinforcement
//     waits for room as IW5's "32 - squad" guard does;
//   * threat bias groups, the chopper bosses and the air support wait for the
//     engine work listed in the plan (phase 5).

// The level's short name: mp_fav_tropical -> fav_tropical.
short_map()
{
	map = getDvar( "mapname" );
	if ( map.size > 3 && getSubStr( map, 0, 3 ) == "mp_" )
		return getSubStr( map, 3, map.size );
	return map;
}

// --------------------------------------------------------------------------
// Init: _survival_mode::Callback_StartGameType, the precache window.
// --------------------------------------------------------------------------

init()
{
	maps\_survival_utility::init();

	// IW5 level.wave_table: the map's own table (common_gametype_survival
	// ships mp/survival_waves_<map>.csv for every survival map), else the
	// mode's tier table.
	level.survival_type_table = "mp/survival_waves.csv";
	level.survival_wave_table = "mp/survival_waves_" + short_map() + ".csv";
	if ( tableLookup( level.survival_wave_table, 2, "1", 3 ) == "" )
		level.survival_wave_table = "mp/survival_waves_tier_1.csv";
	if ( tableLookup( level.survival_wave_table, 2, "1", 3 ) == "" )
		level.survival_wave_table = level.survival_type_table;
	level.survival_loadout_table = level.survival_type_table;

	precacheString( &"MPUI_ROUND_X" );
	precacheString( &"CGAME_SB_SCORE" );
	precacheString( &"MPUI_TAR_MAP_SURVIVAL_GLOBAL_OBJ" );

	// The loadout's weapons (IW5 precache_loadout_item).
	level.survival_loadout = [];
	for ( i = 1000; i <= 1014; i++ )
	{
		key = tableLookup( level.survival_loadout_table, 0, "" + i, 1 );
		item = survival_item( tableLookup( level.survival_loadout_table, 0, "" + i, 2 ) );
		ammo = tableLookup( level.survival_loadout_table, 0, "" + i, 3 );
		if ( !isDefined( key ) || key == "" )
			continue;
		e = spawnStruct();
		e.key = key;
		e.item = item;
		e.ammo = ammo;
		level.survival_loadout[ level.survival_loadout.size ] = e;
		if ( item != "" && item != "armor" && item != "laststand" )
			precacheItem( item );
	}

	level.survival_wave_num = getDvarInt( "scr_survival_start_wave" );
	if ( level.survival_wave_num <= 0 )
		level.survival_wave_num = 1;
	level.survival_intermission = getDvarInt( "scr_survival_intermission" );
	if ( level.survival_intermission <= 0 )
		level.survival_intermission = 30;

	maps\_survival_ai::init();
	maps\_survival_kiosk::init();
	codolprintln( "CoDOL survival: " + getDvar( "mapname" ) + ", waves " + level.survival_wave_table );
}

// The later CoDOL tables name multiplayer weapons; survival carries the _so
// arsenal (plan section 3.3).
survival_item( name )
{
	if ( !isDefined( name ) || name == "" )
		return "";
	switch ( name )
	{
	case "beretta_mp":
	case "m9_mp":
		return "beretta_so";
	case "usp_mp":
		return "usp_silencer_so";
	case "frag_grenade_mp":
		return "fraggrenade_so";
	case "flash_grenade_mp":
		return "flash_grenade_so";
	case "semtex_mp":
		return "semtex_grenade_so";
	case "throwingknife_mp":
		return "throwingknife_so";
	case "c4_mp":
		return "c4_so";
	case "claymore_mp":
		return "claymore_so";
	case "smoke_grenade_mp":
		return "smoke_grenade_american_so";
	}
	return name;
}

// --------------------------------------------------------------------------
// Players
// --------------------------------------------------------------------------

// _survival_mode::spawnPlayer: IW5 give_loadout from rows 1000..1014,
// in place of the coop class.  Returns the primary for the character.
give_loadout()
{
	self takeAllWeapons();
	primary = undefined;
	offhands = [];
	offhands[ 0 ] = "none";
	offhands[ 1 ] = "none";
	offhands[ 2 ] = "none";
	for ( i = 0; i < level.survival_loadout.size; i++ )
	{
		e = level.survival_loadout[ i ];
		if ( e.item == "" )
			continue;
		if ( getSubStr( e.key, 0, 6 ) == "weapon" )
		{
			self giveWeapon( e.item );
			if ( e.ammo == "max" )
				self giveMaxAmmo( e.item );
			else
				self setWeaponAmmoStock( e.item, int( e.ammo ) );
			if ( !isDefined( primary ) )
				primary = e.item;
		}
		else if ( e.key == "grenade_1" )
		{
			self giveWeapon( e.item );
			self setWeaponAmmoClip( e.item, int( e.ammo ) );
			offhands[ 0 ] = e.item;
		}
		else if ( e.key == "grenade_2" )
		{
			self giveWeapon( e.item );
			self setWeaponAmmoClip( e.item, int( e.ammo ) );
			offhands[ 2 ] = e.item;
		}
		else if ( e.key == "armor_1" )
			self.survival_armor = int( e.ammo );
	}
	self setOffhandEquipment( offhands[ 0 ], offhands[ 1 ], offhands[ 2 ] );
	// What the player bought survives a respawn (IW5 keeps his weapons
	// through last stand; a death in coop is a respawn here).
	if ( isDefined( self.survival_bought ) )
	{
		for ( i = 0; i < self.survival_bought.size; i++ )
			self maps\_survival_mode::give_weapon( self.survival_bought[ i ], false );
	}
	if ( isDefined( primary ) )
	{
		self setSpawnWeapon( primary );
		self switchToWeapon( primary );
	}
	return primary;
}

// _survival_mode::Callback_PlayerKilled: no timed respawn; the next
// intermission brings him back (respawn_dead).  Returns true when handled.
player_killed()
{
	self.survival_dead = true;
	level notify( "survival_player_died" );
	codolprintln( "CoDOL survival: " + self.name + " died in wave " + level.survival_wave_num );
	return true;
}

// _survival_mode::Callback_PlayerDamage: IW5 player_armor_shield, the
// body armor bought at the support kiosk soaks the hit first.
armor_soak( damage, attacker, mod )
{
	if ( !isDefined( self.survival_armor ) || self.survival_armor <= 0 )
		return damage;
	if ( self.survival_armor >= damage )
	{
		self.survival_armor -= damage;
		return 0;
	}
	damage -= self.survival_armor;
	self.survival_armor = 0;
	self iPrintLn( "Armor gone" );
	return damage;
}

add_credits( value, why )
{
	if ( !isDefined( self.survival_credit ) )
		self.survival_credit = 0;
	self.survival_credit = int( self.survival_credit ) + int( value );
	// The scoreboard column is the credits, as IW5's summary shows them.
	self.score = self.survival_credit;
	self notify( "credit_updated" );
	self setClientDvar( "surHUD_credits", self.survival_credit );
	if ( isDefined( self.survival_hud_credits ) )
		self.survival_hud_credits setValue( self.survival_credit );
}

spend_credits( value )
{
	if ( !isDefined( self.survival_credit ) || self.survival_credit < value )
		return false;
	self add_credits( 0 - value, "spend" );
	return true;
}

player_hud()
{
	self endon( "disconnect" );
	if ( isDefined( self.survival_hud_wave ) )
		return;
	w = newClientHudElem( self );
	w.alignX = "right";
	w.alignY = "top";
	w.horzAlign = "right";
	w.vertAlign = "top";
	w.x = -20;
	w.y = 110;
	w.font = "objective";
	w.fontScale = 1.4;
	w.foreground = true;
	w.hidewheninmenu = true;
	w.label = &"MPUI_ROUND_X";
	w setValue( level.survival_wave_num );
	self.survival_hud_wave = w;

	c = newClientHudElem( self );
	c.alignX = "right";
	c.alignY = "top";
	c.horzAlign = "right";
	c.vertAlign = "top";
	c.x = -20;
	c.y = 128;
	c.font = "objective";
	c.fontScale = 1.4;
	c.color = ( 0.6, 1, 0.6 );
	c.foreground = true;
	c.hidewheninmenu = true;
	c.label = &"CGAME_SB_SCORE";
	if ( !isDefined( self.survival_credit ) )
		self.survival_credit = 0;
	c setValue( self.survival_credit );
	self.survival_hud_credits = c;
}

update_wave_hud()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isDefined( players[ i ].survival_hud_wave ) )
			players[ i ].survival_hud_wave setValue( level.survival_wave_num );
		players[ i ] setClientDvar( "surHUD_wave", level.survival_wave_num );
	}
}

announce( text )
{
	codolprintln( "CoDOL survival: " + text );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] iPrintLnBold( text );
}

// Every player down or dead with nobody standing to revive: the end.
anyone_standing()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( !isAlive( p ) )
			continue;
		if ( isDefined( p.laststand ) && p.laststand )
			continue;
		if ( isDefined( p.sessionstate ) && p.sessionstate != "playing" )
			continue;
		return true;
	}
	return false;
}

watch_wipe()
{
	level endon( "survival_over" );
	for ( ;; )
	{
		wait( 1 );
		if ( !getPlayers().size )
			continue;
		if ( anyone_standing() )
			continue;
		// a frame of grace: a revive may be completing
		wait( 2 );
		if ( anyone_standing() )
			continue;
		game_over();
		return;
	}
}

game_over()
{
	survived = level.survival_wave_num - 1;
	announce( "Survived " + survived + " waves" );
	codolprintln( "CoDOL survival: over at wave " + level.survival_wave_num + ", " + survived + " survived" );
	// IW5 congrat_min_wave 5: a run of five waves or more is a success for the
	// lobby's mission report.
	if ( survived >= 5 )
		maps\_survival_utility::match_over( true, "survived " + survived + " waves" );
	else
		maps\_survival_utility::match_over( false, "survived " + survived + " waves" );
}

// IW5 survival_wave_pickup_downed_players, and the dead come back.
end_of_wave_players()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( isAlive( p ) && isDefined( p.laststand ) && p.laststand )
		{
			if ( isDefined( level.laststand_funcs ) )
				p common_scripts\_laststand::revive_success( p );
		}
		if ( isDefined( p.survival_dead ) && p.survival_dead )
		{
			p.survival_dead = false;
			p thread maps\_survival_mode::spawnPlayer();
		}
		// IW5 survival_credits: the wave bonus (IW5 pays it through the
		// performance summary; CoDOL's hardline_round_reward is 500).
		if ( isAlive( p ) )
			p add_credits( 100 + 25 * level.survival_wave_num, "wave" );
	}
}

// --------------------------------------------------------------------------
// The match
// --------------------------------------------------------------------------

main()
{
	level thread maps\_survival_test::main();
	maps\_survival_squad::setup();

	while ( maps\_survival_utility::living_players().size <= 0 )
		wait( 0.5 );

	level thread watch_players();
	level thread watch_wipe();
	level thread maps\_survival_kiosk::main();

	// IW5 survival_objective: the objective line ("survive"); objective_add
	// on this server takes a position, so the text is a hud line.
	level thread objective_text( &"MPUI_TAR_MAP_SURVIVAL_GLOBAL_OBJ" );

	// IW5 waittill_survival_start: a few seconds to look around.
	wait( 8 );
	level thread waves();
}

objective_text( text )
{
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
	hud setText( text );
	wait( 8 );
	hud fadeOverTime( 1 );
	hud.alpha = 0;
	wait( 1 );
	hud destroy();
}

watch_players()
{
	level endon( "survival_over" );
	for ( ;; )
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( !isDefined( players[ i ].survival_hud_wave ) )
				players[ i ] thread player_hud();
		}
		wait( 1 );
	}
}

waves()
{
	level endon( "survival_over" );
	for ( ;; )
	{
		w = maps\_survival_ai::get_wave( level.survival_wave_num );
		if ( !isDefined( w ) )
		{
			codolprintln( "CoDOL survival: no wave table; nothing to fight" );
			return;
		}
		update_wave_hud();
		announce( "Wave " + level.survival_wave_num );
		level notify( "wave_started", level.survival_wave_num );
		run_wave( w );
		level notify( "wave_ended", level.survival_wave_num );
		announce( "Wave " + level.survival_wave_num + " complete" );
		end_of_wave_players();
		intermission();
		level.survival_wave_num++;
	}
}

// IW5 survival_wave's body for one wave.
run_wave( w )
{
	level.survival_aggressive = false;
	scale = w.scale;
	squads = maps\_survival_ai::squad_array( w.squad_size );
	for ( i = 0; i < squads.size; i++ )
		maps\_survival_squad::spawn_squad( w.squad_type, squads[ i ], scale );

	for ( i = 0; i < w.specials.size; i++ )
	{
		ref = w.specials[ i ];
		count = 0;
		if ( i < w.special_counts.size )
			count = w.special_counts[ i ];
		if ( count <= 0 )
			continue;
		level thread maps\_survival_ai::spawn_specials( ref, count, scale );
	}

	// IW5 reenforcement_squad_spawn( 1, squad[0] ): one more squad of the
	// first size when a leader falls, while the wave lasts.
	if ( squads.size )
		level thread reinforce( w, squads[ 0 ], scale );

	if ( w.bosses_ai.size )
		level thread boss_spawn( w, scale );
	if ( w.bosses_nonai.size )
		codolprintln( "CoDOL survival: wave " + w.num + " non-AI bosses (" + w.bosses_nonai.size +
			") wait for the vehicle layer (plan phase 5)" );

	// Let the spawns land before counting.
	wait( 3 );

	while ( maps\_survival_ai::living_enemies().size > 4 )
		level waittill_timeout( "survival_axis_died", 1 );
	maps\_survival_ai::go_aggressive();

	for ( ;; )
	{
		pending = isDefined( level.survival_boss_pending ) && level.survival_boss_pending;
		if ( !pending && maps\_survival_ai::living_enemies().size == 0 )
			break;
		level waittill_timeout( "survival_axis_died", 1 );
		// a straggler the graph stranded far from everyone is removed, as
		// _coopmission_defense_crash prunes its groups
		prune_stragglers();
	}
	level notify( "survival_wave_cleared" );
}

waittill_timeout( note, time )
{
	self endon( note );
	wait( time );
}

reinforce( w, size, scale )
{
	level endon( "survival_over" );
	level endon( "survival_wave_cleared" );
	start = maps\_survival_squad::living_leaders().size;
	wait( 5 );
	for ( ;; )
	{
		if ( level.survival_aggressive )
			return;
		if ( maps\_survival_squad::living_leaders().size < start &&
		     level.survival_alive + size <= level.survival_max_alive )
		{
			maps\_survival_squad::spawn_squad( w.squad_type, size, scale );
			return;
		}
		wait( 0.5 );
	}
}

boss_spawn( w, scale )
{
	level.survival_boss_pending = true;
	// IW5 spawn_boss: a boss_delay wave waits for the aggressive phase.
	if ( w.boss_delay )
	{
		while ( !level.survival_aggressive && !level.survival_over )
			wait( 0.5 );
	}
	maps\_survival_ai::spawn_bosses( w.bosses_ai, scale );
	level.survival_boss_pending = false;
}

prune_stragglers()
{
	enemies = maps\_survival_ai::living_enemies();
	if ( enemies.size > 2 )
		return;
	now = getTime();
	for ( i = 0; i < enemies.size; i++ )
	{
		e = enemies[ i ];
		d = maps\_survival_utility::nearest_player_distance( e.origin );
		if ( !isDefined( d ) || d < 1500 )
		{
			e.survival_far_since = undefined;
			continue;
		}
		if ( !isDefined( e.survival_far_since ) )
			e.survival_far_since = now;
		else if ( now - e.survival_far_since > 45000 )
		{
			codolprintln( "CoDOL survival: a straggler at " + e.origin + " (" + int( d ) + " away) is removed" );
			maps\_survival_utility::remove_actor( e );
		}
	}
}

// IW5 survival_wave_intermission: 30 s, 5 of them before the ready-up, with
// a 5 s count; every player pressing ready ends it early (IW5 PC binds
// "skip"; the notify is the same).
intermission()
{
	level endon( "survival_over" );
	level.survival_in_intermission = true;
	level notify( "survival_intermission" );
	total = level.survival_intermission;
	wait( 5 );
	total -= 5;
	level.survival_ready = 0;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] thread ready_up( total );
	announce( "Next wave in " + total + " s (press [Actionslot 2] when ready)" );
	waittill_timeout( "survival_all_ready", total );
	level notify( "survival_ready_done" );
	for ( t = 5; t > 0; t-- )
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
			players[ i ] iPrintLnBold( "" + t );
		wait( 1 );
	}
	level.survival_in_intermission = false;
}

ready_up( total )
{
	self endon( "disconnect" );
	level endon( "survival_ready_done" );
	// IW5 PC readies with the "skip" command (consoles +stance); C1 binds no
	// "skip", so action slot 2 readies too.
	self notifyOnPlayerCommand( "survival_player_ready", "skip" );
	self notifyOnPlayerCommand( "survival_player_ready", "+actionslot 2" );
	self waittill( "survival_player_ready" );
	level.survival_ready++;
	self iPrintLn( "Ready" );
	if ( level.survival_ready >= getPlayers().size )
		level notify( "survival_all_ready" );
}
