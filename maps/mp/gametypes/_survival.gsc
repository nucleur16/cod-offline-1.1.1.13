// survival: Spec Ops Survival, the mode, on the multiplayer chain.
//
// Ported from Infinity Ward's Modern Warfare 3 Spec Ops Survival,
// maps\_so_survival.gsc (IW5; D:\CoD\COD-GSC-Source\IW5-GSC\scripts\1571.gsc
// in the decompiled dump): survival_init / survival_logic, survival_wave
// (squads, specials, bosses, the aggressive phase at four left, the end of a
// wave), the intermission with its ready-up, repeating waves past the table,
// the credits and the armor.  CoDOL ran the same mode on its own data from 2.x
// on (common_gametype_survival); its tables are used as they are.
// docs/CODOL_SURVIVAL_PLAN.md says what is ported from where.
//
// User direction 2026-10-27: survival has the multiplayer weapons and the
// player's own create-a-class, so it is a multiplayer gametype
// (maps\mp\gametypes\survival.gsc) on the shared MP chain.  The MP layer
// spawns the player and gives his class (_playerlogic, _class), runs the
// killstreaks, the damage and the end of the match; this module adds the
// mode on top of it:
//   * the player damage wrapper: IW5 survival's regular-difficulty AI damage
//     (maps\_gameskill) and the body armor, before MP's own handler;
//   * the downs and revives of IW5 (maps\_coop through the shared
//     common_scripts\_laststand) in place of MP's Final Stand;
//   * the waves, the intermission (the dead come back, the class menu applies
//     at once), the credits and the HUD;
//   * AI kill credits. MP loadout streaks are disabled; purchased Survival
//     support remains available from the support kiosk.
//
// Differences from IW5, on purpose:
//   * the match ends when every player is down or dead, not at the first
//     death (four-player rooms); a player who died respawns at the next
//     intermission;
//   * the actor pool is 32 slots (level.survival_max_alive).

// The level's short name: mp_fav_tropical -> fav_tropical.
short_map()
{
	map = getDvar( "mapname" );
	if ( map.size > 3 && getSubStr( map, 0, 3 ) == "mp_" )
		return getSubStr( map, 3, map.size );
	return map;
}

dvar_float( name, value )
{
	if ( getDvar( name ) == "" )
		return value;
	return getDvarFloat( name );
}

dvar_bool( name, value )
{
	if ( getDvar( name ) == "" )
		return value;
	return getDvarInt( name ) != 0;
}

// The authored tables packaged in common_gametype_survival. IW5 Survival
// uses a level table when supplied, otherwise its shared difficulty tier.
// These two levels have authored placements but no level-specific table.
wave_table_for_map( mapname )
{
	switch ( mapname )
	{
	case "mp_abandon_sh":
		return "mp/survival_waves_abandon_sh.csv";
	case "mp_ambush_sh":
		return "mp/survival_waves_ambush_sh.csv";
	case "mp_crash_tropical":
		return "mp/survival_waves_crash_tropical.csv";
	case "mp_cross_fire":
		return "mp/survival_waves_cross_fire.csv";
	case "mp_fav_tropical":
		return "mp/survival_waves_fav_tropical.csv";
	case "mp_firingrange":
		return "mp/survival_waves_firingrange.csv";
	case "mp_hardhat_sh":
		return "mp/survival_waves_hardhat_sh.csv";
	case "mp_highrise":
		return "mp/survival_waves_highrise.csv";
	case "mp_seatown":
		return "mp/survival_waves_seatown.csv";
	case "mp_storm_spring":
		return "mp/survival_waves_storm_spring.csv";
	case "mp_subbase_sh":
		return "mp/survival_waves_subbase_sh.csv";
	case "mp_terminal_cls":
		return "mp/survival_waves_terminal_cls.csv";
	case "mp_qadeem_sh":
	case "mp_strike_sh":
		return "mp/survival_waves_tier_1.csv";
	}
	return "mp/survival_waves_tier_1.csv";
}

// --------------------------------------------------------------------------
// Precache: maps\mp\gametypes\survival::main, the gametype's precache window.
// --------------------------------------------------------------------------

precache()
{
	maps\mp\gametypes\_survival_utility::init();

	// tableLookup is not an asset-existence probe: a missing StringTable is
	// fatal before it can return an empty cell. Select a shipped wave table
	// before the first lookup. Qadeem and Strike use the real shared tier.
	level.survival_type_table = "mp/survival_waves.csv";
	level.survival_wave_table = wave_table_for_map( getDvar( "mapname" ) );

	precacheString( &"MPUI_ROUND_X" );
	precacheString( &"CGAME_SB_SCORE" );
	precacheString( &"SO_SURVIVAL_CREDITS" );
	precacheString( &"MPUI_TAR_MAP_SURVIVAL_GLOBAL_OBJ" );

	level.survival_wave_num = getDvarInt( "scr_survival_start_wave" );
	if ( level.survival_wave_num <= 0 )
		level.survival_wave_num = 1;
	level.survival_intermission = getDvarInt( "scr_survival_intermission" );
	if ( level.survival_intermission <= 0 )
		level.survival_intermission = 30;
	level.survival_in_intermission = false;

	// IW5 survival downs the player and a partner revives him.
	level.survival_laststand = dvar_bool( "scr_survival_laststand", true );
	// Survival rewards come from its economy, never MP loadout progression.
	level.survival_killstreaks = false;

	// IW4/IW5 maps\_gameskill regular: what an AI bullet does to a player.
	level.survival_ai_damage_scale = dvar_float( "scr_survival_ai_damage_scale", 0.18 );
	level.survival_ai_damage_cap = dvar_float( "scr_survival_ai_damage_cap", 30 );
	// Separate from MP dog weapon damage: three unarmored bites at 100 HP.
	level.survival_dog_damage_cap = dvar_float( "scr_survival_dog_damage_cap", 35 );
	level.survival_world_damage_scale = dvar_float( "scr_survival_world_damage_scale", 0.5 );
	level.survival_invul_pre = dvar_float( "scr_survival_invul_pre", 0.35 );
	level.survival_invul_on = dvar_float( "scr_survival_invul_on", 0.5 );
	level.survival_invul_post = dvar_float( "scr_survival_invul_post", 0.3 );
	level.survival_worthy_ratio = dvar_float( "scr_survival_worthy_ratio", 0.1 );
	level.survival_red_cutoff = dvar_float( "scr_survival_red_cutoff", 0.2 );
	level.survival_death_grace = dvar_float( "scr_survival_death_grace", 1.7 );

	maps\mp\gametypes\_survival_ai::init();
	maps\mp\gametypes\_survival_kiosk::init();
	maps\mp\gametypes\_survival_vehicles::precache();
	codolprintln( "CoDOL survival: " + getDvar( "mapname" ) + ", waves " + level.survival_wave_table );
}

// --------------------------------------------------------------------------
// Init: onStartGameType, once the MP layer has set its callbacks.
// --------------------------------------------------------------------------

init()
{
	// The player damage and the downs go through survival first.
	level.survival_mp_player_damage = level.callbackPlayerDamage;
	level.callbackPlayerDamage = ::player_damage;
	level.survival_mp_laststand = level.callbackPlayerLastStand;
	level.callbackPlayerLastStand = ::player_laststand;
	if ( level.survival_laststand )
		laststand_init();

	// IW5 ai_init: the threat bias groups (native, codol_threat_bias.c).
	createThreatBiasGroup( "sentry" );
	createThreatBiasGroup( "allies" );
	createThreatBiasGroup( "axis" );
	createThreatBiasGroup( "boss" );
	createThreatBiasGroup( "dogs" );
	setThreatBias( "sentry", "boss", 50 );
	setThreatBias( "sentry", "axis", 50 );
	setThreatBias( "boss", "allies", 2000 );
	setThreatBias( "dogs", "allies", 1000 );
	setThreatBias( "axis", "allies", 0 );
}

// The actor callbacks are the MP dog's (maps\mp\_dogs::init, threaded from
// the killstreaks' init); survival wraps them once they are installed.
wrap_actor_callbacks()
{
	for ( i = 0; i < 40 && !isDefined( level.callbackActorDamage ); i++ )
		wait( 0.05 );
	level.survival_mp_actor_damage = level.callbackActorDamage;
	level.callbackActorDamage = maps\mp\gametypes\_survival_ai::actor_damage;
	level.survival_mp_actor_killed = level.callbackActorKilled;
	level.callbackActorKilled = maps\mp\gametypes\_survival_ai::actor_killed;
}

// --------------------------------------------------------------------------
// Players
// --------------------------------------------------------------------------

// _playerlogic::maySpawn: a player who died in a wave waits for the
// intermission; nobody spawns after the end.
may_spawn()
{
	if ( isDefined( level.survival_over ) && level.survival_over )
		return false;
	return !( isDefined( self.survival_dead ) && self.survival_dead );
}

// _menus::menuClass: a class picked during the intermission (or at the
// armory's loadout change) is given at once instead of at the next spawn.
class_change_now()
{
	if ( level.survival_in_intermission || ( isDefined( self.survival_class_change ) && self.survival_class_change ) )
	{
		self.survival_class_change = false;
		self thread after_class_change();
		return true;
	}
	return false;
}

after_class_change()
{
	self endon( "disconnect" );
	waittillframeend;
	self survival_abilities();
}

// survival::onSpawnPlayer runs before the MP class is given; the survival
// state follows the loadout.
on_spawn()
{
	self.survival_dead = false;
	self.survival_last_damage = 0;
	self.survival_was_red = false;
	self.survival_grace_used = false;
	self.survival_invul_until = 0;
	self.laststand = false;
	if ( !isDefined( self.survival_credit ) )
		self.survival_credit = 0;
	if ( !isDefined( self.survival_armor ) )
		self.survival_armor = 0;
	self.score = self.survival_credit;
	self setClientDvar( "surHUD_credits", self.survival_credit );
	self thread after_spawn();
}

after_spawn()
{
	self endon( "disconnect" );
	self endon( "death" );
	self waittill( "spawned_player" );
	self survival_abilities();
	self player_hud();
	self setThreatBiasGroup( "allies" );
	self thread camping_think();
}

// What the class gave, plus a survival life's extras: the downs, the perks
// and the weapons bought at the armory.
survival_abilities()
{
	if ( level.survival_laststand )
		self common_scripts\_laststand::enable_player();
	if ( isDefined( self.survival_perks ) )
	{
		for ( i = 0; i < self.survival_perks.size; i++ )
			self maps\mp\perks\_perks::givePerk( self.survival_perks[ i ] );
	}
	if ( isDefined( self.survival_bought ) )
	{
		for ( i = 0; i < self.survival_bought.size; i++ )
			self give_weapon( self.survival_bought[ i ], false );
	}
}

// onPlayerKilled: no timed respawn; the next intermission brings him back.
player_killed()
{
	self.survival_dead = true;
	self.laststand = false;
	if ( isDefined( self.revivetrigger ) )
	{
		self.revivetrigger delete();
		self.revivetrigger = undefined;
	}
	level notify( "survival_player_died" );
	codolprintln( "CoDOL survival: " + self.name + " died in wave " + level.survival_wave_num );
}

// level.callbackPlayerDamage: IW5 survival's damage model, then MP's own.
player_damage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime )
{
	if ( !isAlive( self ) )
		return;
	// The team's own players: no friendly fire.
	if ( isDefined( eAttacker ) && isPlayer( eAttacker ) && eAttacker != self )
		return;

	if ( isDefined( eAttacker ) && !isPlayer( eAttacker ) && is_enemy( eAttacker ) )
	{
		if ( isDefined( sMeansOfDeath ) && isSubStr( sMeansOfDeath, "BULLET" ) )
		{
			iDamage = self ai_hit( iDamage, sHitLoc );
			if ( iDamage <= 0 )
				return;
		}
		else if ( sMeansOfDeath == "MOD_MELEE" && isDefined( eAttacker.isdog ) && eAttacker.isdog )
		{
			iDamage = dog_melee_damage( iDamage );
		}
		else if ( sMeansOfDeath != "MOD_MELEE" )
		{
			iDamage = int( iDamage * level.survival_world_damage_scale );
			if ( iDamage < 1 )
				iDamage = 1;
		}
	}

	// IW5 player_armor_shield: the body armor bought at a kiosk soaks first.
	iDamage = self armor_soak( iDamage, eAttacker, sMeansOfDeath );
	if ( iDamage <= 0 )
		return;

	self.survival_last_damage = getTime();
	if ( isDefined( self.survival_perf ) )
		self.survival_perf[ "damagetaken" ] += iDamage;
	if ( self.health - iDamage > self.maxhealth * level.survival_red_cutoff )
		self.survival_grace_used = false;
	self [[level.survival_mp_player_damage]]( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime );
}

// The actual dog melee arrives through the AI weapon's native damage callback.
// Do not scale MP dogs or explosive-dog splash; armor still absorbs this bite.
dog_melee_damage( damage )
{
	cap = int( level.survival_dog_damage_cap );
	if ( cap < 1 )
		cap = 35;
	if ( damage > cap )
		return cap;
	return damage;
}

// An enemy of the players: an axis actor, an axis vehicle or turret, or what
// one of them fired.
is_enemy( e )
{
	if ( isDefined( e.survival_enemy ) && e.survival_enemy )
		return true;
	if ( isAI( e ) )
		return true;
	if ( isDefined( e.team ) && e.team == "axis" )
		return true;
	return false;
}

// IW4/IW5 maps\_gameskill regular (the coop core's numbers): an AI bullet
// does a fraction of its damage, headshots half again, and a big hit buys a
// short invulnerability; the one that would kill from above the red zone
// leaves one point once per red visit.
ai_hit( iDamage, sHitLoc )
{
	now = getTime();
	if ( isDefined( self.survival_invul_until ) && now < self.survival_invul_until )
		return 0;
	if ( isDefined( sHitLoc ) && ( sHitLoc == "head" || sHitLoc == "helmet" || sHitLoc == "neck" ) )
		iDamage = iDamage * 0.5;
	dealt = int( iDamage * level.survival_ai_damage_scale + 0.5 );
	if ( dealt < 1 )
		dealt = 1;
	if ( dealt > level.survival_ai_damage_cap )
		dealt = int( level.survival_ai_damage_cap );
	red = self.maxhealth * level.survival_red_cutoff;
	if ( dealt >= self.health )
	{
		if ( self.health > 1 && !( isDefined( self.survival_grace_used ) && self.survival_grace_used ) && level.survival_death_grace > 0 )
		{
			self.survival_grace_used = true;
			self.survival_invul_until = now + int( level.survival_death_grace * 1000 );
			self.survival_was_red = true;
			return self.health - 1;
		}
		return dealt;
	}
	if ( dealt >= self.maxhealth * level.survival_worthy_ratio )
	{
		after = self.health - dealt;
		invul = level.survival_invul_pre;
		if ( after <= red )
		{
			if ( isDefined( self.survival_was_red ) && self.survival_was_red )
				invul = level.survival_invul_post;
			else
				invul = level.survival_invul_on;
		}
		self.survival_invul_until = now + int( invul * 1000 );
	}
	if ( self.health - dealt <= red )
		self.survival_was_red = true;
	return dealt;
}

// IW5 player_armor_shield: the armor pool soaks the hit first.
armor_soak( damage, attacker, mod )
{
	if ( !isDefined( self.survival_armor ) || self.survival_armor <= 0 )
		return damage;
	if ( isDefined( mod ) && mod == "MOD_FALLING" )
		return damage;
	if ( self.survival_armor >= damage )
	{
		self.survival_armor -= damage;
		self update_armor_hud();
		return 0;
	}
	damage -= self.survival_armor;
	self.survival_armor = 0;
	self update_armor_hud();
	self iPrintLn( "Armor gone" );
	return damage;
}

// --------------------------------------------------------------------------
// Downs and revives (IW5 maps\_coop / _laststand through the shared
// common_scripts\_laststand: bleed-out, a teammate's 1.5 s revive within 128
// units, the pistol while down).  MP's own laststand is Final Stand, which
// survival does not use.
// --------------------------------------------------------------------------

laststand_init()
{
	level.laststand_vars = [];
	level.laststand_vars[ "bleedout_time" ] = dvar_float( "scr_survival_bleedout_time", 120 );
	level.laststand_vars[ "revive_time" ] = dvar_float( "scr_survival_revive_time", 1.5 );
	level.laststand_vars[ "trigger_radius" ] = 128;
	level.laststand_vars[ "log" ] = "CoDOL survival laststand";
	level.laststand_funcs = [];
	level.laststand_funcs[ "pistol" ] = ::laststand_pistol;
	level.laststand_funcs[ "all_down" ] = ::all_down;
	common_scripts\_laststand::init();
}

player_laststand( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, psOffsetTime, deathAnimDuration )
{
	if ( level.survival_laststand && !level.survival_over )
	{
		self notify( "player_downed" );
		self common_scripts\_laststand::player_laststand();
		return;
	}
	self [[level.survival_mp_laststand]]( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, psOffsetTime, deathAnimDuration );
}

laststand_pistol()
{
	weapons = self getWeaponsListPrimaries();
	for ( i = 0; i < weapons.size; i++ )
	{
		if ( weaponClass( weapons[ i ] ) == "pistol" )
			return weapons[ i ];
	}
	return level.survival_laststand_pistol;
}

all_down()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] common_scripts\_laststand::player_is_in_laststand() )
			players[ i ] thread common_scripts\_laststand::laststand_die();
	}
}

// --------------------------------------------------------------------------
// Weapons the armory sells (kept through a respawn and a class change)
// --------------------------------------------------------------------------

give_weapon( weapon, switchTo )
{
	if ( !isDefined( weapon ) || weapon == "" || weapon == "none" )
		return false;
	if ( !isDefined( switchTo ) )
		switchTo = false;
	if ( self hasWeapon( weapon ) )
	{
		self giveMaxAmmo( weapon );
		if ( switchTo )
			self switchToWeapon( weapon );
		return true;
	}
	if ( weaponInventoryType( weapon ) == "primary" )
	{
		primaries = self getWeaponsListPrimaries();
		if ( primaries.size >= 2 )
		{
			replace = self getCurrentWeapon();
			found = false;
			for ( i = 0; i < primaries.size; i++ )
			{
				if ( primaries[ i ] == replace )
					found = true;
			}
			if ( !found )
				replace = primaries[ primaries.size - 1 ];
			self takeWeapon( replace );
			switchTo = true;
		}
	}
	self maps\mp\_utility::_giveWeapon( weapon );
	self giveMaxAmmo( weapon );
	if ( switchTo )
		self switchToWeapon( weapon );
	return true;
}

// --------------------------------------------------------------------------
// Credits and HUD
// --------------------------------------------------------------------------

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

hud_line( y, color )
{
	h = newClientHudElem( self );
	h.alignX = "right";
	h.alignY = "top";
	h.horzAlign = "right";
	h.vertAlign = "top";
	h.x = -20;
	h.y = y;
	h.font = "objective";
	h.fontScale = 1.4;
	h.color = color;
	h.foreground = true;
	h.hidewheninmenu = true;
	return h;
}

player_hud()
{
	if ( isDefined( self.survival_hud_wave ) )
		return;
	self.survival_hud_wave = self hud_line( 110, ( 1, 1, 1 ) );
	self.survival_hud_wave.label = &"MPUI_ROUND_X";
	self.survival_hud_wave setValue( level.survival_wave_num );
	self.survival_hud_credits = self hud_line( 128, ( 0.6, 1, 0.6 ) );
	self.survival_hud_credits.label = &"SO_SURVIVAL_CREDITS";
	// Spendable credits stay visible while an interactive kiosk is open.
	self.survival_hud_credits.hidewheninmenu = false;
	self.survival_hud_credits setValue( self.survival_credit );
	self.survival_hud_armor = self hud_line( 146, ( 0.6, 0.8, 1 ) );
	self update_armor_hud();
}

update_armor_hud()
{
	if ( !isDefined( self.survival_hud_armor ) )
		return;
	if ( isDefined( self.survival_armor ) && self.survival_armor > 0 )
		self.survival_hud_armor setText( "Armor " + self.survival_armor );
	else
		self.survival_hud_armor setText( "" );
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

// --------------------------------------------------------------------------
// The end
// --------------------------------------------------------------------------

// Every player down or dead with nobody standing to revive.
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
		if ( !isDefined( p.sessionstate ) || p.sessionstate != "playing" )
			continue;
		return true;
	}
	return false;
}

anyone_joined()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isDefined( players[ i ].hasSpawned ) && players[ i ].hasSpawned )
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
		if ( !anyone_joined() )
			continue;
		if ( anyone_standing() )
			continue;
		// a revive may be completing
		wait( 2 );
		if ( anyone_standing() )
			continue;
		game_over();
		return;
	}
}

// onDeadEvent / watch_wipe: the run is over.
game_over()
{
	if ( level.survival_over )
		return;
	survived = level.survival_wave_num - 1;
	if ( survived < 0 )
		survived = 0;
	announce( "Survived " + survived + " waves" );
	// IW5 congrat_min_wave 5: five waves or more is a success for the
	// lobby's mission report.
	maps\mp\gametypes\_survival_utility::match_over( survived >= 5, "survived " + survived + " waves" );
}

// IW5 survival_wave_pickup_downed_players, and the dead come back.
end_of_wave_players()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( isAlive( p ) && isDefined( p.laststand ) && p.laststand && isDefined( level.laststand_funcs ) )
			p common_scripts\_laststand::revive_success( p );
		if ( isDefined( p.survival_dead ) && p.survival_dead )
			p thread respawn_dead();
		// IW5 player_performance_think: the personal wave reward.
		if ( isAlive( p ) )
			p add_credits( p wave_reward(), "wave" );
	}
}

respawn_dead()
{
	self endon( "disconnect" );
	self.survival_dead = false;
	if ( isAlive( self ) || !isDefined( self.pers[ "team" ] ) || self.pers[ "team" ] != "allies" )
		return;
	if ( !isDefined( self.class ) || self.class == "" )
		return;
	self notify( "end_respawn" );
	self.waitingToSpawn = false;
	self thread maps\mp\gametypes\_playerlogic::spawnPlayer();
}

// --------------------------------------------------------------------------
// The match
// --------------------------------------------------------------------------

main()
{
	level thread wrap_actor_callbacks();
	level thread maps\mp\gametypes\_survival_test::main();
	maps\mp\gametypes\_survival_squad::setup();
	maps\mp\gametypes\_survival_vehicles::init();

	while ( maps\mp\gametypes\_survival_utility::living_players().size <= 0 )
		wait( 0.5 );

	level thread watch_wipe();
	level thread maps\mp\gametypes\_survival_kiosk::main();

	// IW5 waittill_survival_start: a few seconds to look around.
	wait( 8 );
	level thread waves();
}

waves()
{
	level endon( "survival_over" );
	for ( ;; )
	{
		w = maps\mp\gametypes\_survival_ai::get_wave( level.survival_wave_num );
		if ( !isDefined( w ) )
		{
			codolprintln( "CoDOL survival: no wave table; nothing to fight" );
			return;
		}
		update_wave_hud();
		announce( "Wave " + level.survival_wave_num );
		level notify( "wave_started", level.survival_wave_num );
		performance_reset();
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
	squads = maps\mp\gametypes\_survival_ai::squad_array( w.squad_size );
	for ( i = 0; i < squads.size; i++ )
		maps\mp\gametypes\_survival_squad::spawn_squad( w.squad_type, squads[ i ], scale );

	for ( i = 0; i < w.specials.size; i++ )
	{
		ref = w.specials[ i ];
		count = 0;
		if ( i < w.special_counts.size )
			count = w.special_counts[ i ];
		if ( count <= 0 )
			continue;
		level thread maps\mp\gametypes\_survival_ai::spawn_specials( ref, count, scale );
	}

	// IW5 reenforcement_squad_spawn( 1, squad[0] ): one more squad of the
	// first size when a leader falls, while the wave lasts.
	if ( squads.size )
		level thread reinforce( w, squads[ 0 ], scale );

	level.survival_boss_pending = 0;
	if ( w.bosses_ai.size )
		level thread boss_spawn( w, scale );
	if ( w.bosses_nonai.size )
		level thread chopper_spawn( w, scale );

	// Let the spawns land before counting.
	wait( 3 );

	while ( maps\mp\gametypes\_survival_ai::living_enemies().size > 4 )
		level waittill_timeout( "survival_axis_died", 1 );
	maps\mp\gametypes\_survival_ai::go_aggressive();

	for ( ;; )
	{
		if ( level.survival_boss_pending <= 0 &&
		     maps\mp\gametypes\_survival_ai::living_enemies().size == 0 &&
		     maps\mp\gametypes\_survival_vehicles::living_choppers() == 0 )
			break;
		level waittill_timeout( "survival_axis_died", 1 );
		prune_stragglers();
	}
	level notify( "survival_wave_cleared" );
}

waittill_timeout( note, time )
{
	if ( time <= 0 )
		return;
	self endon( note );
	wait( time );
}

reinforce( w, size, scale )
{
	level endon( "survival_over" );
	level endon( "survival_wave_cleared" );
	start = maps\mp\gametypes\_survival_squad::living_leaders().size;
	wait( 5 );
	for ( ;; )
	{
		if ( level.survival_aggressive )
			return;
		if ( maps\mp\gametypes\_survival_squad::living_leaders().size < start &&
		     level.survival_alive + size <= level.survival_max_alive )
		{
			maps\mp\gametypes\_survival_squad::spawn_squad( w.squad_type, size, scale );
			return;
		}
		wait( 0.5 );
	}
}

// IW5 spawn_boss: the juggernauts come in by Mi-17 (spawn_juggernaut ->
// drop_jug_by_chopper); a boss_delay wave waits for the aggressive phase.
boss_spawn( w, scale )
{
	level.survival_boss_pending++;
	if ( w.boss_delay )
	{
		while ( !level.survival_aggressive && !level.survival_over )
			wait( 0.5 );
	}
	maps\mp\gametypes\_survival_ai::spawn_bosses( w.bosses_ai, scale );
	level.survival_boss_pending--;
}

// IW5 spawn_boss's non-AI bosses: the attack choppers.
chopper_spawn( w, scale )
{
	level.survival_boss_pending++;
	if ( w.boss_delay )
	{
		while ( !level.survival_aggressive && !level.survival_over )
			wait( 0.5 );
	}
	for ( i = 0; i < w.bosses_nonai.size; i++ )
	{
		maps\mp\gametypes\_survival_vehicles::spawn_chopper_boss( w.bosses_nonai[ i ], scale );
		wait( 1 );
	}
	level.survival_boss_pending--;
}

prune_stragglers()
{
	enemies = maps\mp\gametypes\_survival_ai::living_enemies();
	if ( enemies.size > 2 )
		return;
	now = getTime();
	for ( i = 0; i < enemies.size; i++ )
	{
		e = enemies[ i ];
		if ( isDefined( e.survival_boss ) )
			continue;
		d = maps\mp\gametypes\_survival_utility::nearest_player_distance( e.origin );
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
			maps\mp\gametypes\_survival_utility::remove_actor( e );
		}
	}
}

// IW5 survival_wave_intermission: 30 s, 5 of them before the ready-up, with
// a 5 s count; every player pressing ready ends it early.  The class menu
// applies at once while it lasts.
intermission()
{
	level endon( "survival_over" );
	level.survival_in_intermission = true;
	level notify( "survival_intermission" );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] iPrintLn( "Intermission: a class chosen now is given at once" );
	total = level.survival_intermission;
	wait( 5 );
	total -= 5;
	if ( total < 0 )
		total = 0;
	level.survival_ready = 0;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] thread ready_up( total );
	announce( "Next wave in " + total + " s (press [{+actionslot 2}] when ready)" );
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
	// IW5 PC readies with "skip"; C1 binds none, so action slot 2 too.
	self notifyOnPlayerCommand( "survival_player_ready", "skip" );
	self notifyOnPlayerCommand( "survival_player_ready", "+actionslot 2" );
	self waittill( "survival_player_ready" );
	level.survival_ready++;
	self iPrintLn( "Ready" );
	if ( level.survival_ready >= getPlayers().size )
		level notify( "survival_all_ready" );
}

// --------------------------------------------------------------------------
// The wave reward (IW5 player_performance_init / reward_calculation): per
// wave, headshots, kills, the damage not taken, the time under 90 s and the
// wave number, at IW5's coop rates when more than one player is in.
// Accuracy is left out: this server's MP layer keeps no per-wave shot count.
// --------------------------------------------------------------------------

performance_reset()
{
	coop = getPlayers().size > 1;
	level.survival_bonus = [];
	level.survival_bonus[ "damagetaken" ] = 2;
	level.survival_bonus[ "time" ] = 2;
	level.survival_bonus[ "wavebonus" ] = 25;
	level.survival_bonus[ "headshot" ] = 20;
	level.survival_bonus[ "kill" ] = 10;
	if ( coop )
	{
		level.survival_bonus[ "wavebonus" ] = 50;
		level.survival_bonus[ "headshot" ] = 50;
		level.survival_bonus[ "kill" ] = 50;
	}
	level.survival_wave_start = getTime();
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		p.survival_perf = [];
		p.survival_perf[ "headshot" ] = 0;
		p.survival_perf[ "kill" ] = 0;
		p.survival_perf[ "damagetaken" ] = 0;
	}
}

// From maps\mp\gametypes\_survival_ai::death_watch.
perf_kill( headshot )
{
	if ( !isDefined( self.survival_perf ) )
		return;
	self.survival_perf[ "kill" ]++;
	if ( headshot )
		self.survival_perf[ "headshot" ]++;
}

wave_reward()
{
	if ( !isDefined( self.survival_perf ) || !isDefined( level.survival_bonus ) )
		return 0;
	b = level.survival_bonus;
	total = self.survival_perf[ "headshot" ] * b[ "headshot" ];
	notTaken = 400 - self.survival_perf[ "damagetaken" ] * b[ "damagetaken" ];
	if ( notTaken > 0 )
		total += notTaken;
	total += self.survival_perf[ "kill" ] * b[ "kill" ];
	seconds = 90 - int( ( getTime() - level.survival_wave_start ) / 1000 );
	if ( seconds > 0 )
		total += b[ "time" ] * seconds;
	total += level.survival_wave_num * b[ "wavebonus" ];
	return int( total );
}

// --------------------------------------------------------------------------
// Camping (IW5 camping_think / camp_response / level_ai_respond): a player
// who holds one spot (220 units) for about 20 s while not under pressure is
// camping; every 8 s while he camps the soldiers are handed grenades for him
// (IW5 throw_grenade_at_player: two each, forced for 8 s).
// --------------------------------------------------------------------------

camping_think()
{
	self endon( "death" );
	self endon( "disconnect" );
	self notify( "survival_camping" );
	self endon( "survival_camping" );
	self.camping = false;
	self thread camp_response();
	for ( ;; )
	{
		self.camping = false;
		count = 0;
		anchor = self.origin;
		kills = int( self.kills );
		while ( count <= 20 )
		{
			if ( distance( anchor, self.origin ) < 220 )
				count++;
			else
				count -= 2;
			if ( self.health < 40 )
				count--;
			if ( int( self.kills ) - kills > 0 )
				count += int( self.kills ) - kills;
			if ( count <= 0 || level.survival_in_intermission || ( isDefined( self.laststand ) && self.laststand ) )
			{
				count = 0;
				anchor = self.origin;
			}
			kills = int( self.kills );
			wait( 1 );
		}
		self.camping = true;
		self.camping_loc = self.origin;
		self notify( "camping" );
		while ( distance( anchor, self.origin ) < 260 )
			wait( 1 );
		self.camping = false;
		self notify( "stopped camping" );
	}
}

camp_response()
{
	self endon( "death" );
	self endon( "disconnect" );
	self endon( "survival_camping" );
	for ( ;; )
	{
		wait( 0.05 );
		if ( !self.camping )
			continue;
		enemies = maps\mp\gametypes\_survival_ai::living_enemies();
		for ( i = 0; i < enemies.size; i++ )
		{
			e = enemies[ i ];
			if ( isDefined( e.isdog ) && e.isdog )
				continue;
			e.coop_grenades = 2;
		}
		// the shared AI grenade timer is waived for the response
		level.coop_next_ai_grenade = 0;
		if ( !isDefined( self.camping_logged ) || self.camping_logged != self.camping_loc )
			codolprintln( "CoDOL survival: " + self.name + " is camping at " + self.camping_loc + "; the enemies get grenades" );
		self.camping_logged = self.camping_loc;
		wait( 8 );
	}
}
