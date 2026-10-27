// survival core: tuning, callbacks, spawn, the player damage model.
//
// The rules layer of Spec Ops Survival, its own match category (fs_game /
// codol_zoneType survival; common_gametype_survival on the unchanged
// multiplayer level; docs/CODOL_SURVIVAL_PLAN.md).  IW5 runs survival on its
// singleplayer engine (maps\_load, maps\_specialops, maps\_coop for the downs);
// this server has no such engine, so the parts of that contract survival
// needs live here: the player's spawn, the regular-difficulty AI damage model
// of IW4/IW5 maps\_gameskill, health regeneration, and the downs and revives
// through the shared common_scripts/_laststand.gsc.  The flow itself -- waves,
// enemies, kiosks -- is maps\_survival.
//
// Rules, as for the coop and zm cores: this file names no multiplayer module
// and no maps\_coop* module (a file::function reference is a compile
// dependency); tuning is read from dvars; every asset is probed before use.
// The damage model and the regeneration are the coop core's (_coopmode.gsc),
// copied rather than shared because a category may not compile another
// category's modules.

// --------------------------------------------------------------------------
// Gametype init: rules-script load, before the level script.  Data only.
// --------------------------------------------------------------------------

init()
{
	level.isSP = false;
	level.teamBased = false;
	level.gametype = "survival";
	level.players = [];
	level.gameEnded = false;
	level.survival_vision = undefined;

	// IW5 survival downs the player and a partner revives him; a bleed-out
	// is a death, which survival answers at the next intermission
	// (maps\_survival::player_killed).
	level.survival_laststand = ( getDvar( "scr_survival_laststand" ) == "" || getDvarInt( "scr_survival_laststand" ) != 0 );

	// IW4/IW5 maps\_gameskill regular (the coop core's tuning, see there).
	level.survival_ai_damage_scale = dvar_float( "scr_survival_ai_damage_scale", 0.18 );
	level.survival_ai_damage_cap = dvar_float( "scr_survival_ai_damage_cap", 30 );
	level.survival_world_damage_scale = dvar_float( "scr_survival_world_damage_scale", 0.5 );
	level.survival_invul_pre = dvar_float( "scr_survival_invul_pre", 0.35 );
	level.survival_invul_on = dvar_float( "scr_survival_invul_on", 0.5 );
	level.survival_invul_post = dvar_float( "scr_survival_invul_post", 0.3 );
	level.survival_worthy_ratio = dvar_float( "scr_survival_worthy_ratio", 0.1 );
	level.survival_red_cutoff = dvar_float( "scr_survival_red_cutoff", 0.2 );
	level.survival_death_grace = dvar_float( "scr_survival_death_grace", 1.7 );
	level.survival_regen_delay = dvar_float( "scr_survival_regen_delay", 2.0 );
	level.survival_regen_delay_red = dvar_float( "scr_survival_regen_delay_red", 5.0 );
	level.survival_regen_rate = dvar_float( "scr_survival_regen_rate", 50 );
	level.survival_player_health = 100;
}

dvar_float( name, value )
{
	if ( getDvar( name ) == "" )
		return value;
	return getDvarFloat( name );
}

// --------------------------------------------------------------------------
// Level-script entry: maps/<level>_survival.gsc's main() calls this, in the
// precache window before CodeCallback_StartGameType.
//   vision      the level's vision set (the multiplayer level script's)
//   minimap     the level's compass material (its maps\mp\_compass call)
// --------------------------------------------------------------------------

levelMain( vision, minimap )
{
	level.isSP = false;
	if ( isDefined( vision ) && vision != "" )
	{
		level.survival_vision = vision;
		VisionSetNaked( vision, 0 );
		VisionSetPain( vision );
	}
	// The level's own systems (destructibles, exploders, pipes, ...) through
	// the shared loader, as the coop and zm level scripts start them.
	common_scripts\_load_common::main();
	if ( isDefined( minimap ) )
		level thread setup_minimap( minimap );
	codolprintln( "CoDOL survival: level " + getDvar( "mapname" ) + ", vision " + vision );
}

// IW4 maps\mp\_compass::setupMiniMap over the level's minimap_corner pair,
// through the shared common_scripts/_compass.
setup_minimap( material )
{
	corners = getEntArray( "minimap_corner", "targetname" );
	if ( corners.size != 2 )
	{
		codolprintln( "CoDOL survival: minimap " + material + " not set: " + corners.size + " minimap_corner entities" );
		return;
	}
	if ( !isDefined( level.requiredMapAspectRatio ) )
		level.requiredMapAspectRatio = getDvarFloat( "scr_RequiredMapAspectratio", 1 );
	common_scripts\_compass::setupMiniMapCorners( material, corners );
}

// --------------------------------------------------------------------------
// Callbacks
// --------------------------------------------------------------------------

Callback_StartGameType()
{
	if ( !isDefined( level.survival_player_health ) )
		init();
	level.isSP = false;
	if ( !isDefined( level.players ) )
		level.players = [];
	level.gameEnded = false;
	level.survival_taken_spawns = [];

	common_scripts\_perks::init();
	if ( level.survival_laststand )
		laststand_init();

	// The native world gun every armed player carries needs tag_origin.
	precacheModel( "tag_origin" );

	level notify( "prematch_over" );

	maps\_survival::init();
	level thread maps\_survival::main();
}

Callback_CodeEndGame()
{
	level.gameEnded = true;
}

Callback_PlayerConnect()
{
	self.statusicon = "";
	self waittill( "begin" );

	self.connected = true;
	level.players[ level.players.size ] = self;
	self.score = 0;
	self.deaths = 0;
	self.kills = 0;

	level notify( "connected", self );
	self thread onMenuResponse();
	self thread spawnPlayer();
}

// C1's pause screen answers the closing Escape with "mr <id> <slot> back";
// two closeInGameMenu passes run its two-step teardown (zm and coop do the
// same).  A kiosk menu's own responses are maps\_survival_kiosk's.
onMenuResponse()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "menuresponse", menu, response );
		if ( response != "back" )
			continue;
		if ( isDefined( self.survival_in_kiosk ) && self.survival_in_kiosk )
			continue;
		self closeInGameMenu();
		self closeInGameMenu();
	}
}

Callback_PlayerDisconnect()
{
	players = [];
	for ( i = 0; i < level.players.size; i++ )
	{
		if ( level.players[ i ] == self )
			continue;
		players[ players.size ] = level.players[ i ];
	}
	level.players = players;
	level notify( "survival_player_left" );
}

Callback_PlayerDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, timeOffset )
{
	if ( level.gameEnded )
		return;
	// A dead player has no die handler (see _coopmode); IW4 returns first.
	if ( !isAlive( self ) )
		return;
	// No friendly fire between players (IW5 survival g_friendlyfireDamageScale
	// 0.5 applies to the team's own explosives; none of those reach here).
	if ( isDefined( eAttacker ) && isPlayer( eAttacker ) && eAttacker != self )
		return;

	if ( !isDefined( eAttacker ) || !isPlayer( eAttacker ) )
	{
		if ( isDefined( sMeansOfDeath ) && isSubStr( sMeansOfDeath, "BULLET" ) )
		{
			iDamage = ai_hit( iDamage, sHitLoc );
			if ( iDamage <= 0 )
				return;
		}
		else
		{
			iDamage = int( iDamage * level.survival_world_damage_scale );
			if ( iDamage < 1 )
				iDamage = 1;
			if ( iDamage > 40 )
				iDamage = 40;
		}
	}

	// IW5 player_armor_shield: the body armor bought at a kiosk soaks first.
	iDamage = self maps\_survival::armor_soak( iDamage, eAttacker, sMeansOfDeath );
	if ( iDamage <= 0 )
		return;

	self.survival_last_damage = getTime();
	if ( self.health - iDamage > self.maxhealth * level.survival_red_cutoff )
		self.survival_grace_used = false;
	self finishPlayerDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, timeOffset );
}

Callback_PlayerKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration )
{
	self.laststand = false;
	if ( isDefined( self.revivetrigger ) )
	{
		self.revivetrigger delete();
		self.revivetrigger = undefined;
	}
	self.sessionstate = "dead";
	self.deaths++;
	if ( level.gameEnded )
		return;
	self maps\_survival::player_killed();
}

Callback_ActorKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset )
{
	self maps\_survival_ai::on_actor_killed( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset );
}

Callback_PlayerLastStand( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration )
{
	if ( level.survival_laststand )
	{
		self common_scripts\_laststand::player_laststand();
		return;
	}
	self Callback_PlayerKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration );
}

// --------------------------------------------------------------------------
// Downs and revives (IW5 maps\_coop / _laststand: bleed-out, a teammate's
// 1.5 s revive within 128 units, the pistol while down).  Everyone down: IW5
// ends the mission; every downed player bleeds out now, and
// maps\_survival::watch_wipe ends the match.
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
// Spawning: the map's survival player starts (IW5 info_player_start_so, from
// maps/<level>_survival.gsc's data), spread farthest-from-taken as IW4
// _specialops_code pick_starting_location_so does; the level's multiplayer
// spawns when the map has no survival data.
// --------------------------------------------------------------------------

spawn_points()
{
	if ( isDefined( level.survival_player_starts ) && level.survival_player_starts.size )
		return level.survival_player_starts;
	names = [];
	names[ 0 ] = "mp_tdm_spawn_allies_start";
	names[ 1 ] = "mp_dm_spawn";
	names[ 2 ] = "mp_tdm_spawn";
	for ( i = 0; i < names.size; i++ )
	{
		points = getEntArray( names[ i ], "classname" );
		if ( points.size )
			return points;
	}
	return [];
}

pick_spawn()
{
	points = spawn_points();
	if ( !points.size )
		return undefined;
	if ( !isDefined( level.survival_taken_spawns ) )
		level.survival_taken_spawns = [];
	best = undefined;
	bestScore = -1;
	for ( i = 0; i < points.size; i++ )
	{
		p = points[ i ];
		score = 0;
		players = getPlayers();
		near = 999999;
		for ( j = 0; j < players.size; j++ )
		{
			if ( players[ j ] == self || !isAlive( players[ j ] ) )
				continue;
			d = distance( players[ j ].origin, p.origin );
			if ( d < near )
				near = d;
		}
		// Close to the team (a respawn joins them) but not on top of anyone.
		if ( near < 64 )
			score = 0;
		else if ( near == 999999 )
			score = 1000 - i;
		else
			score = 100000 - near;
		if ( score > bestScore )
		{
			bestScore = score;
			best = p;
		}
	}
	return best;
}

spawnPlayer()
{
	self endon( "disconnect" );
	spawnpoint = pick_spawn();
	if ( !isDefined( spawnpoint ) )
	{
		println( "CoDOL survival: no usable spawn point on " + getDvar( "mapname" ) );
		return;
	}
	angles = spawnpoint.angles;
	if ( !isDefined( angles ) )
		angles = ( 0, 0, 0 );

	self.sessionteam = "allies";
	self.sessionstate = "playing";
	self.statusicon = "";
	self.maxhealth = level.survival_player_health;
	self.health = self.maxhealth;

	self spawn( spawnpoint.origin, angles );

	primary = self maps\_survival::give_loadout();
	if ( isDefined( primary ) && !( isDefined( self.isbot ) && self.isbot ) )
		self codol_applyProfileCharacter( "tf", primary );

	self.laststand = false;
	if ( level.survival_laststand )
		self common_scripts\_laststand::enable_player();
	self thread healthRegen();
	self notify( "spawned_player" );
}

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
	self giveWeapon( weapon );
	self giveMaxAmmo( weapon );
	if ( switchTo )
		self switchToWeapon( weapon );
	return true;
}

// --------------------------------------------------------------------------
// The damage model and regeneration (IW4/IW5 maps\_gameskill regular; the
// coop core's, see _coopmode.gsc for the measurements).
// --------------------------------------------------------------------------

healthRegen()
{
	self endon( "disconnect" );
	self endon( "death" );
	self notify( "survival_regen" );
	self endon( "survival_regen" );
	self.survival_last_damage = 0;
	self.survival_was_red = false;
	self.survival_grace_used = false;
	self.survival_invul_until = 0;
	carry = 0;
	for ( ;; )
	{
		wait( 0.1 );
		if ( self.health <= 0 || self.health >= self.maxhealth || ( isDefined( self.laststand ) && self.laststand ) )
		{
			self.survival_was_red = false;
			self.survival_grace_used = false;
			carry = 0;
			continue;
		}
		delay = level.survival_regen_delay;
		if ( self.survival_was_red )
			delay = level.survival_regen_delay_red;
		if ( getTime() - self.survival_last_damage < delay * 1000 )
		{
			carry = 0;
			continue;
		}
		carry += level.survival_regen_rate * 0.1;
		step = int( carry );
		if ( step < 1 )
			continue;
		carry -= step;
		self.health = self.health + step;
		if ( self.health > self.maxhealth )
			self.health = self.maxhealth;
	}
}

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
			if ( self.survival_was_red )
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
