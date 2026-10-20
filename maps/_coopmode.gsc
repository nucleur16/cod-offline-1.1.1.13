// coop core: tuning, callbacks, spawn, loadout.
//
// The coop counterpart of the multiplayer _gamelogic/_playerlogic/_damage trio,
// reduced to what a Spec Ops match needs to boot and hold a player.  It is the
// sibling of maps/_zombiemode.gsc and follows the same three
// rules:
//
//   1. NOTHING IN THIS FILE MAY NAME A MULTIPLAYER MODULE.  A GSC
//      `file::function` reference is a compile-time dependency, so naming
//      maps\mp\_load or maps\mp\gametypes\_gamelogic would compile all 100
//      multiplayer scripts (measured with tools/codol_script_resolve.py) and
//      precache common_gametype_mp assets into a match that mounted
//      common_gametype_coop.  There are no #includes; every helper is local or
//      an engine builtin.
//   2. Tuning is READ, not hardcoded.  mp/cocoopclasstable.csv and
//      mp/cocoopmapsettings.csv ship in common_gametype_coop.ff (overridden by
//      patch_common_gametype_coop.ff) and are reachable with tableLookup.
//      CODOL_Scr_TableLookup returns "" for a missing table or row and never
//      errors (src/codol/script/codol_script_table.c), so an absent table
//      degrades to the defaults instead of failing the boot.
//   3. Every asset name is PROBED with precacheItem before it is used.
//      precacheItem is a silent no-op for a weapon in no mounted zone
//      (src/codol/script/codol_script_precache.c) and it is what registers the
//      definition, so getWeaponIndexFromName only answers non-zero afterwards
//      for a name the coop zones really carry.
//
// BEHAVIOUR REFERENCE.  D:\CoD\COD-GSC-Source\IW4-GSC\maps\_specialops_code.gsc
// (pick_starting_location_so :10-44, pick_starting_location_pmc :50-81,
// place_player_at_start_point :84-93) and maps\_coop.gsc (:11-62 init,
// :64-140 the downed/bleed-out loop).  Those are SP-namespace scripts that run
// on the singleplayer engine: they use flag_init/ent_flag, EnableDeathShield,
// level.gameskill, level.player/level.player2 and the actor system, none of
// which exist on this server.  What is ported here is their DATA CONTRACT --
// the spawn classnames and the order they are tried -- not their machinery.
//
// THE isSP() LANDMINE.  common_scripts\utility::isSP() infers singleplayer from
// !string_starts_with( getdvar("mapname"), "mp_" ), because IW4 shipped no
// multiplayer map that was not mp_*.  Under the PLAYLIST map spelling
// (coop_training_mp_killhouse_sh) that predicate is TRUE, and under the BASE
// spelling (mp_killhouse_sh) it is FALSE -- so the same coop match would behave
// differently depending on which of its two names the lobby sent.  A dedicated
// match server is never singleplayer either way.  isSP() caches into level.isSP
// and only computes when that is undefined, so assigning it is the mechanism
// the vendor function itself provides.  It is assigned at all three entry
// points this category can be entered through: init() below (gametype script
// load), levelMain() (the level script's main(), which the engine runs BEFORE
// CodeCallback_StartGameType) and Callback_StartGameType().

// --------------------------------------------------------------------------
// Tables
// --------------------------------------------------------------------------

// MEASURED, common_gametype_coop.ff: 16 columns x 43 rows, oriented rows =
// property names (column 0), columns 1..15 = the fifteen coop loadouts whose
// uniqueName row reads class0..class4, class0_up1..class4_up1,
// class0_up2..class4_up2.
//
// Decoded 2026-10-02 with tools/codol_stringtable_dump.py, the hash-only cells
// resolved against every weapon name in docs/zone_assets/ALL_weapon.txt with
// the table's own hash (h = tolower(c) + 31*h):
//
//   class            0                 1                 2      3    4
//   loadoutPrimary   mp5_silencer      cheytac_silencer  ump45  rpd  m16_basic
//   loadoutSecondary usp_silencer      tmp_silencer      aa12   rpg  barrett
//   loadoutLethal    hatchet_so        throwingknife_so  semtex_grenade_so
//                                                        fraggrenade_so ...
//   loadoutTactical  c4_so / claymore_so
//   loadoutSpecial   smoke_grenade_american_so / flash_grenade_so
//   loadoutMeleeWpn  coop_mel_karambit, _knife, _bonesaw, _bowie, _sog
//
// THE _so SUFFIX.  The primary and secondary cells are BASE names; the coop
// zones ship them only as <base>_so (mp5_silencer_so, usp_silencer_so,
// m16_basic_so, ump45_so, rpd_so, aa12_so, rpg_so, barrett_so ... all in
// common_gametype_coop), exactly as a multiplayer class cell is a base name the
// multiplayer chain completes with _mp.  No zone ships any of them without the
// suffix.  The three offhand rows already carry it.  coop_mel_* is in no zone
// as a weapon at all: it is a melee cosmetic, not something giveWeapon takes.
coop_class_table()
{
	return "mp/cocoopclasstable.csv";
}

// MEASURED, common_gametype_coop.ff: 2 columns x 11 rows, column 0 being the
// coop playlist map names coop_training_mp_killhouse, coop_take_mp_fav_tropical,
// coop_defense_mp_crash_tropical, coop_take_mp_estate_tropical,
// coop_evasion_mp_con_spring, coop_sniperfi_mp_con_spring,
// coop_suspension_mp_bridge, coop_chopper_mp_suburbs, coop_defense_mp_suburbs,
// coop_escape_mp_storm_spring, coop_ac_mp_hunted_spring.  Used only as a
// membership probe: what column 1 means is not established, so nothing reads it.
coop_map_table()
{
	return "mp/cocoopmapsettings.csv";
}

// Row-name lookup against the class table.  Column 1 is class0, the first
// loadout; level.coop_class_column selects which of the fifteen is in play.
class_value( row )
{
	// init() owns level.coop_class_column.  Callback_StartGameType re-runs
	// init() when it finds it missing, but a lookup must not fault on it either:
	// tableLookup takes an int column, and undefined is a script runtime error
	// that kills the whole StartGameType thread (VPS codol-match-17733,
	// 2026-10-02 15:49).
	column = level.coop_class_column;
	if ( !isDefined( column ) )
		column = 1;

	value = tableLookup( coop_class_table(), 0, row, column );
	if ( !isDefined( value ) || value == "none" )
		return "";

	return value;
}

// --------------------------------------------------------------------------
// Gametype init.  Runs from coop.gsc::main(), i.e. at gametype-script load,
// before the level script and before CodeCallback_StartGameType.  Data only:
// no entity may be touched here.
// --------------------------------------------------------------------------

init()
{
	level.isSP = false;

	// mp_playlists.info's `gametype coop` block carries no `teambased` line.
	level.teamBased = false;
	level.gametype = "coop";
	level.players = [];

	// class0.  C1 sends the player's chosen coop class in his profile; until
	// that is read back here the first loadout is the honest default, and it is
	// the one the class table itself lists first.
	level.coop_class_column = 1;

	// Only coop_spec_mp_crash_tropical's addon MapEnts carry
	// info_player_start_pmcDefend, so this is derived from the entities in
	// Callback_StartGameType rather than declared here.
	level.coop_defend = false;

	level.coop_vision = undefined;
	level.coop_respawn_delay = 5;

	// The campaign mission layer (_coopmission.gsc).  Spec Ops has no respawn
	// (IW4 maps\_coop downs the player instead, which this server cannot do
	// yet -- see Callback_PlayerLastStand), so a player gets a small number of
	// respawns and the mission fails when every player has spent his.
	level.coop_lives = getDvarInt( "scr_coop_lives" );
	if ( level.coop_lives <= 0 )
		level.coop_lives = 3;

	// Downs and revives, IW4 Spec Ops (maps\_coop.gsc) through the shared
	// common_scripts/_laststand.gsc that zm runs too: a lethal hit downs the
	// player, a teammate revives him, a bleed-out is a death (which then
	// spends a life, below).  scr_coop_laststand 0 restores death on the
	// lethal hit.
	level.coop_laststand = ( getDvar( "scr_coop_laststand" ) == "" || getDvarInt( "scr_coop_laststand" ) != 0 );

	// What an AI round does to a player: the regular-difficulty model of
	// CoD4/IW4 maps\_gameskill.gsc (see Callback_PlayerDamage and
	// healthRegen).  The soldiers fire through codol_actorShoot, so their
	// rounds arrive with the soldier (an actor, never a player) as attacker;
	// anything else that is not a player (the world, a vehicle, a scripted
	// magicBullet) is told apart by its means of death.
	//
	// regular: player_damageMultiplier = 100 / playerDifficultyHealth(275).
	// The coop weapons' AI rounds arrive at 50..100 raw (MEASURED 2026-10-08:
	// m16_basic_so / ump45_so 97..100 a hit, aa12_so 50 a pellet), so the
	// scale that gives a full-health player SP's six-to-seven rifle hits is
	// lower than SP's 0.36.
	level.coop_ai_damage_scale = coop_dvar_float( "scr_coop_ai_damage_scale", 0.18 );
	// No single AI round takes more than this (sniper, close shotgun pellet).
	level.coop_ai_damage_cap = coop_dvar_float( "scr_coop_ai_damage_cap", 30 );
	// Damage from other non-player sources (explosions, falls, vehicles).
	level.coop_world_damage_scale = coop_dvar_float( "scr_coop_world_damage_scale", 0.5 );
	// _gameskill regular: invulTime_preShield 0.35 s, invulTime_onShield 0.5 s,
	// invulTime_postShield 0.3 s, worthyDamageRatio 0.1,
	// healthOverlayCutoff 0.2, player_deathInvulnerableTime 1700 ms,
	// playerHealth_RegularRegenDelay 2000 ms, longRegenTime 5000 ms.
	level.coop_invul_pre = coop_dvar_float( "scr_coop_invul_pre", 0.35 );
	level.coop_invul_on = coop_dvar_float( "scr_coop_invul_on", 0.5 );
	level.coop_invul_post = coop_dvar_float( "scr_coop_invul_post", 0.3 );
	level.coop_worthy_ratio = coop_dvar_float( "scr_coop_worthy_ratio", 0.1 );
	level.coop_red_cutoff = coop_dvar_float( "scr_coop_red_cutoff", 0.2 );
	level.coop_death_grace = coop_dvar_float( "scr_coop_death_grace", 1.7 );
	level.coop_regen_delay = coop_dvar_float( "scr_coop_regen_delay", 2.0 );
	level.coop_regen_delay_red = coop_dvar_float( "scr_coop_regen_delay_red", 5.0 );
	// Health per second once regeneration starts (a full bar in two seconds).
	level.coop_regen_rate = coop_dvar_float( "scr_coop_regen_rate", 50 );

	// IW4 player health is 100 with regeneration; Spec Ops does not change it
	// (maps\_utility::set_player_health is only used by scripted moments).
	level.coop_player_health = 100;

	level.gameEnded = false;
}

// --------------------------------------------------------------------------
// Level-script entry point.
//
// GScr_LoadScripts asks for exactly one level module per match: maps/<mapname>
// for coop (CODOL_ScriptEntryPoints, src/codol/script/codol_script_source.c).
// coop is the only category whose map name is NOT category-distinct: the
// lobby's two spellings for the same match are the playlist name
// (coop_training_mp_killhouse_sh) and the base level name (mp_killhouse_sh),
// and the coop zones ship no world assets at all -- only an addon_map_ents --
// so the level zone really is the shared mp_* one.  That is why maps/ carries
// BOTH spellings of all nine playlists; the base-map spellings
// (maps/mp_killhouse_sh.gsc) are distinct modules from the multiplayer level
// scripts (maps/mp/mp_killhouse_sh.gsc), as IW4 keeps SP and MP levels apart.
// Each of those files is a few lines and calls this.
// --------------------------------------------------------------------------

levelMain( visionName )
{
	level.isSP = false;

	// Applied here rather than in Callback_StartGameType because that is where
	// IW4 applies it: the level script's main() calls maps\mp\_load::main(),
	// which does VisionSetNaked/VisionSetPain (the shared maps/mp/_load.gsc:93,
	// :97).  The NAME is passed in rather than taken from getDvar("mapname")
	// because two of the nine coop zones ship no vision rawfile of their own:
	// coop_spec_mp_fav_tropical ships none and coop_take_mp_estate_tropical
	// ships only vision/slomo_breach.vision.  Those two name their base map's
	// vision instead, which is mounted because CODOL_LoadMapArchives mounts the
	// base mp_<map> zone alongside the coop_<mode>_mp_<map> one.
	//
	// The level script that runs is the one named by the map the server
	// loaded, and for a coop match that is the BASE level (mp_killhouse_sh),
	// whose coop copy passes the base map's own multiplayer vision.  The
	// mission's vision is the session's, when its zone ships one (MEASURED,
	// docs/zone_assets: vision/<session>.vision in coop_training, _spec1,
	// _defense_crash, _prolog_crash_intro, _spec_crash, _spec_estate,
	// _take_fav), so the session wins: the training mission was drawn with
	// the multiplayer Killhouse vision (user report 2026-10-08, "the vision
	// seems different from the video").
	session = getDvar( "codol_sessionMap" );
	if ( coop_session_has_vision( session ) )
		visionName = session;
	if ( isDefined( visionName ) && visionName != "" )
	{
		level.coop_vision = visionName;
		VisionSetNaked( visionName, 0 );
		VisionSetPain( visionName );
	}
	{}

	// The mission's ambient bed: the Killhouse missions' zones ship
	// ambient_killhouse_exterior (the multiplayer level plays
	// ambient_scoutsniper_ext0 instead, from its own level script).
	if ( session == "coop_training_mp_killhouse_sh" || session == "coop_spec1_mp_killhouse_sh" )
		ambientPlay( "ambient_killhouse_exterior" );

	// The base map's own systems -- destructible vehicles and props,
	// exploders, pipes, the dynamic world, minefields -- through the shared
	// level loader (common_scripts/_load_common.gsc).  They were started only
	// by maps\mp\_load::main(), which this category must not compile.  The
	// level script's main() is the precache window they need.
	common_scripts\_load_common::main();

	coop_minimap();
}

coop_session_has_vision( session )
{
	switch ( session )
	{
	case "coop_training_mp_killhouse_sh":
	case "coop_spec1_mp_killhouse_sh":
	case "coop_defense_mp_crash_tropical":
	case "coop_prolog_mp_crash_intro":
	case "coop_spec_mp_crash_tropical":
	case "coop_spec_mp_estate_tropical":
	case "coop_take_mp_fav_tropical":
		return true;
	}
	return false;
}

// --------------------------------------------------------------------------
// Minimap
// --------------------------------------------------------------------------

// The compass map of the running mission.  The multiplayer level scripts set
// theirs with maps\mp\_compass::setupMiniMap, a multiplayer module, so coop
// never set one and C1 drew no map (configstring 823 stayed empty).  Each
// mission's own zone ships its material (MEASURED, docs/zone_assets:
// compass_map_killhouse in coop_training_mp_killhouse_sh,
// compass_map_cqbhanger in coop_spec1_mp_killhouse_sh, compass_map_mp_crash_sp
// in coop_defense_mp_crash_tropical, compass_map_mp_estate_sp in
// coop_take_mp_estate_tropical, compass_map_mp_fav_tropical_sp /
// _spec in coop_take / coop_spec_mp_fav_tropical); the other missions ship
// none and use their base map's (the multiplayer level script's material).
// The retail recording of the training mission shows the camp's own map in
// the corner, not the multiplayer killhouse.
coop_minimap_material( session )
{
	switch ( session )
	{
	case "coop_training_mp_killhouse_sh":
		return "compass_map_killhouse";
	case "coop_spec1_mp_killhouse_sh":
		return "compass_map_cqbhanger";
	case "coop_defense_mp_crash_tropical":
		return "compass_map_mp_crash_sp";
	case "coop_take_mp_estate_tropical":
		return "compass_map_mp_estate_sp";
	case "coop_take_mp_fav_tropical":
		return "compass_map_mp_fav_tropical_sp";
	case "coop_spec_mp_fav_tropical":
		return "compass_map_mp_fav_tropical_spec";
	}
	switch ( getDvar( "mapname" ) )
	{
	case "mp_killhouse_sh":
		return "compass_map_mp_killhouse_sh";
	case "mp_crash_tropical":
		return "compass_map_mp_crash_dlc";
	case "mp_crash_intro":
		return "compass_map_mp_crash_intro";
	case "mp_estate_tropical":
		return "compass_map_mp_estate_tropical";
	case "mp_fav_tropical":
		return "compass_map_mp_fav_tropical";
	}
	return undefined;
}

// The bounds: the map's minimap_corner script_origins.  The coop addon
// MapEnts overlay their own pair on the base map's for the killhouse
// missions (script_specialops 1, and script_specialopsname when it names
// one mission -- coop_training's pair spans -2368 1408 .. 4864 -5824, the
// whole camp; the base pair spans only the multiplayer killhouse), so the
// mission's pair wins and the base pair is the fallback.  The corner maths
// is maps\mp\_compass::setupMiniMap's.
coop_minimap()
{
	session = getDvar( "codol_sessionMap" );
	material = coop_minimap_material( session );
	if ( !isDefined( material ) )
	{
		{}
		return;
	}
	all = getEntArray( "minimap_corner", "targetname" );
	mission = [];
	base = [];
	for ( i = 0; i < all.size; i++ )
	{
		c = all[ i ];
		// script_specialops reads as defined on all four corners (MEASURED:
		// the isDefined test alone counted 4 mission corners), so its value
		// decides; only the addon's pair has 1.
		if ( isDefined( c.script_specialops ) && int( c.script_specialops ) == 1 )
		{
			if ( !isDefined( c.script_specialopsname ) || c.script_specialopsname == session )
				mission[ mission.size ] = c;
		}
		else
			base[ base.size ] = c;
	}
	corners = base;
	if ( mission.size == 2 )
		corners = mission;
	if ( corners.size != 2 )
	{
		{}
		for ( i = 0; i < all.size; i++ )
		{
			v = "undefined";
			if ( isDefined( all[ i ].script_specialops ) )
				v = "" + all[ i ].script_specialops;
			{}
		}
		return;
	}

	// IW4 _compass setupMiniMap's maths, shared (common_scripts/_compass.gsc).
	if ( !common_scripts\_compass::setupMiniMapCorners( material, corners ) )
		return;
	northwest = level.minimapNorthwest;
	southeast = level.minimapSoutheast;
	which = "base";
	if ( mission.size == 2 )
		which = "mission";
	{}
}

// --------------------------------------------------------------------------
// Loadout
// --------------------------------------------------------------------------

// Probe a candidate list in order and return the first name the mounted coop
// zones actually carry.  Never returns a name the client cannot resolve.
//
// codol_isWeaponMounted is the find-only probe; getWeaponIndexFromName is NOT
// one.  For a name in no mounted zone the database hands back defaultweapon_mp
// and getWeaponIndexFromName answers a non-zero index for that clone
// (src/codol/script/codol_script_weapon_queries.c), so the old gate always
// accepted the first candidate, mounted or not.  Only the winner is
// precached, which is what registers its definition.
resolveWeapon( candidates )
{
	for ( i = 0; i < candidates.size; i++ )
	{
		name = candidates[ i ];
		if ( !isDefined( name ) || name == "" || name == "none" )
			continue;

		if ( !codol_isWeaponMounted( name ) )
			continue;

		precacheItem( name );
		return name;
	}

	return undefined;
}

// One class table row -> a usable weapon name.  The row's own value is tried
// with the coop _so suffix first (see the table comment above), then as
// written, then the fallbacks.
weaponFromRow( row, fallback1, fallback2 )
{
	value = class_value( row );

	candidates = [];
	if ( value != "" && !isSubStr( value, "_so" ) )
		candidates[ candidates.size ] = value + "_so";
	candidates[ candidates.size ] = value;
	candidates[ candidates.size ] = fallback1;
	candidates[ candidates.size ] = fallback2;

	return resolveWeapon( candidates );
}

resolveLoadout()
{
	// The class's own cells first (decoded above), then class4's primary and
	// class0's secondary as fallbacks.  Every fallback is a common_gametype_coop
	// weapon, so it is mounted for every coop map, whichever coop_<mode>_<map>
	// session zone rides along.
	level.coop_primary = weaponFromRow( "loadoutPrimary", "m16_basic_so", "mp5_silencer_so" );
	level.coop_secondary = weaponFromRow( "loadoutSecondary", "usp_silencer_so", "usp_silencer_akimbo_so" );

	// The three offhands, in C1's button order (+frag lethal, +smoke special,
	// +tact tactical) -- the order setOffhandEquipment takes, as the
	// multiplayer _class::codol_giveEquipment passes it.
	level.coop_lethal = weaponFromRow( "loadoutLethal", "fraggrenade_so", undefined );
	level.coop_special = weaponFromRow( "loadoutSpecial", "flash_grenade_so", undefined );
	level.coop_tactical = weaponFromRow( "loadoutTactical", undefined, undefined );

	// Not a weapon asset in any zone (see the table comment); recorded for the
	// melee cosmetic, never given.
	level.coop_melee_item = class_value( "loadoutMeleeWpn" );

	// The class's three perk slots (specialty_coop_* rows of
	// mp/coPerkStatsTable.csv, applied by the shared perk core at spawn).
	level.coop_perks = [];
	for ( slot = 1; slot <= 3; slot++ )
	{
		perk = class_value( "loadoutPerk" + slot );
		if ( perk != "" )
			level.coop_perks[ level.coop_perks.size ] = perk;
	}

	// IW4 maps\_coop.gsc:995: a downed player holds his own pistol, else the
	// forced "Beretta" -- beretta_so in common_gametype_coop.
	pistols = [];
	pistols[ 0 ] = "beretta_so";
	pistols[ 1 ] = "usp_silencer_so";
	level.coop_laststand_pistol = resolveWeapon( pistols );

	// codolprintln, not println: println is a developer builtin and production
	// runs developer 0.
	summary = "CoDOL coop: class column " + level.coop_class_column;
	summary += " primary " + coop_name( level.coop_primary );
	summary += " secondary " + coop_name( level.coop_secondary );
	summary += " lethal " + coop_name( level.coop_lethal );
	summary += " special " + coop_name( level.coop_special );
	summary += " tactical " + coop_name( level.coop_tactical );
	summary += " perks " + common_scripts\_perks::perkList( level.coop_perks );
	{}

	if ( !isDefined( level.coop_primary ) )
		{}
}

coop_name( weapon )
{
	if ( !isDefined( weapon ) )
		return "none";
	return weapon;
}

// The one way a coop script gives a player a weapon (the training stations,
// the map's weapon pickups, mission rewards): a player carries at most two
// primary weapons, as in the CoD4 / MW2 campaigns, plus his offhands and
// equipment.  A new primary when two are held replaces one: a held primary
// of the same weaponClass (a pistol replaces the pistol, a rifle the rifle),
// else the weapon in hand (the CoD4 SP pickup swap).  The replaced weapon is
// taken, not dropped: CoD4 killhouse's scripted gives drop nothing.  Offhands
// and items are only added.  User report 2026-10-07: "when the scripts give
// the player a new weapon it adds onto their total weapons instead of just
// keeping only two weapons".
coop_give_weapon( weapon, switchTo )
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
			replace = undefined;
			newClass = weaponClass( weapon );
			for ( i = 0; i < primaries.size; i++ )
			{
				if ( weaponClass( primaries[ i ] ) == newClass )
					replace = primaries[ i ];
			}
			if ( !isDefined( replace ) )
			{
				current = self getCurrentWeapon();
				for ( i = 0; i < primaries.size; i++ )
				{
					if ( primaries[ i ] == current )
						replace = current;
				}
			}
			if ( !isDefined( replace ) )
				replace = primaries[ primaries.size - 1 ];
			self takeWeapon( replace );
			switchTo = true;
			{}
		}
	}
	self giveWeapon( weapon );
	self giveMaxAmmo( weapon );
	if ( switchTo )
		self switchToWeapon( weapon );
	return true;
}

giveStartingLoadout()
{
	self takeAllWeapons();

	primary = level.coop_primary;
	if ( isDefined( primary ) )
	{
		self giveWeapon( primary );
		self giveMaxAmmo( primary );
	}

	if ( isDefined( level.coop_secondary ) )
	{
		self giveWeapon( level.coop_secondary );
		self giveMaxAmmo( level.coop_secondary );
	}

	// Offhands as the multiplayer _class::codol_giveEquipment gives them:
	// giveWeapon + giveStartAmmo per slot, then one setOffhandEquipment in
	// button order.
	offhands = [];
	offhands[ 0 ] = level.coop_lethal;
	offhands[ 1 ] = level.coop_special;
	offhands[ 2 ] = level.coop_tactical;
	for ( slot = 0; slot < 3; slot++ )
	{
		if ( !isDefined( offhands[ slot ] ) )
		{
			offhands[ slot ] = "none";
			continue;
		}

		self giveWeapon( offhands[ slot ] );
		self giveStartAmmo( offhands[ slot ] );
	}
	self setOffhandEquipment( offhands[ 0 ], offhands[ 1 ], offhands[ 2 ] );

	// The class's perks through the shared perk core (setPerk on the
	// specialty_coop_* rows; a Pro grants its base too).
	given = self common_scripts\_perks::giveLoadoutPerks( level.coop_perks );
	{}

	if ( isDefined( primary ) )
	{
		self setSpawnWeapon( primary );
		self switchToWeapon( primary );
	}

	// mp/basemaps.arena gives every coop_* entry allieschar "us_army" /
	// axischar "opforce_composite", and every ally actor in the coop addon
	// MapEnts is an actor_ally_codo_tf_* or actor_ally_tf_141_* model, so the
	// coop player is Task Force.  zone_faction() passes a non-zm category
	// through unchanged (src/codol/player/codol_player_appearance.c), so "tf"
	// is what composes.  INFERRED from those two data points; C1's own coop
	// character selector index has not been read out of the profile yet.
	//
	// A test client has no uploaded profile to compose, and the builtin raises
	// for one -- a runtime error that ends the match (MEASURED 2026-10-02 with
	// an addTestClient probe).  Same guard as zm's _zombiemode.
	if ( isDefined( primary ) && !( isDefined( self.isbot ) && self.isbot ) )
		self codol_applyProfileCharacter( "tf", primary );
}

// --------------------------------------------------------------------------
// Spawning
//
// Ported from IW4 maps\_specialops_code.gsc:10-81.  Its order is
// info_player_start_so, then info_player_start; PMC modes use
// info_player_start_pmcDefend when the mission is a defend mission and
// info_player_start_pmc otherwise, spread by picking the farthest-from-taken
// point for each successive player.
//
// MEASURED across the seven coop addon MapEnts this pass decoded:
//   info_player_start_so         coop_training_mp_killhouse_sh (2),
//                                coop_defense_mp_crash_tropical (6),
//                                coop_spec_mp_crash_tropical (2),
//                                coop_take_mp_estate_tropical (2)
//   info_player_start_pmc        coop_spec_mp_estate_tropical (2),
//                                coop_spec_mp_fav_tropical (2),
//                                coop_take_mp_fav_tropical (4)
//   info_player_start_pmcDefend  coop_spec_mp_crash_tropical (2)
//
// THE CATCH, and why the multiplayer classnames are in the list too: all of
// those live in the zone's addon_map_ents asset (IW4 asset type 42).  This
// server's asset table stops at type 32 (src/asmsource/_db_assetnames.asm,
// g_assetNames) and has no addon_map_ents loader at all, so until one exists
// the entities the server actually sees are the BASE mp_<map> zone's -- the
// ordinary multiplayer spawn points.  Falling through to those is what makes a
// coop match spawn a player today, and the coop classnames being first is what
// makes it spawn him in the right place the moment the loader lands.
// --------------------------------------------------------------------------

spawnClassnames()
{
	names = [];
	if ( level.coop_defend )
		names[ names.size ] = "info_player_start_pmcDefend";

	names[ names.size ] = "info_player_start_so";
	names[ names.size ] = "info_player_start_pmc";
	names[ names.size ] = "info_player_start";
	// Base-zone multiplayer fallbacks, in the order a one-per-classname map
	// makes useful.
	names[ names.size ] = "mp_tdm_spawn_allies_start";
	names[ names.size ] = "mp_dm_spawn";
	names[ names.size ] = "mp_tdm_spawn";

	return names;
}

// A start a player can actually stand on: solid ground within 256 units
// below it.  MEASURED 2026-10-07 every addon start on all nine coop maps
// passes (checkstarts in _cooptest), so this only guards a future map.
isUsableStart( point )
{
	if ( !isDefined( point ) || !isDefined( point.origin ) )
		return false;
	trace = bulletTrace( point.origin + ( 0, 0, 32 ), point.origin - ( 0, 0, 256 ), false, undefined );
	return trace[ "fraction" ] < 1;
}

// THE FIRST PLAYER'S START, deterministically.
//
// The bug this fixes (user report 2026-10-07, coop_training_mp_killhouse_sh:
// "I spawned outside of the map", then "restarted the map and I ended up in
// the right starting spot"): that addon authors TWO info_player_start_so,
//   non_CQB_spawn_point  (5560 -1866 16)  on the road in front of the
//                        humvee_player jeep -- the mission's opening
//   CQB_only_spawn_point (-133 -1129 8)   by the hangar, the "skip to the
//                        CQB course" start, nowhere near the opening
// and the old pick was points[ randomInt( points.size ) ] -- IW4
// _specialops_code's random( p1_starts ), which is only right because an IW4
// so_* level script deletes the starts it does not want.  No such script
// ships, so every load was a coin toss; the VPS log of the bad load shows the
// player touching the hangar's at_ssdd_hanger trigger, i.e. the CQB start.
//
// The authored names are the only signal left, so they rank the starts:
//   * "only" -- a special-case start (CQB_only_...) -- last;
//   * a numbered or "_2nd" spelling (player_start_point02, 4_coop_start_02,
//     estate_player_start_2nd) after the plain one;
// ties keep map order, so the same start wins on every load.
startRank( point )
{
	name = "";
	if ( isDefined( point.targetname ) )
		name = toLower( point.targetname );

	rank = 0;
	if ( isSubStr( name, "only" ) )
		rank += 100;
	if ( isSubStr( name, "2nd" ) || isSubStr( name, "02" ) || isSubStr( name, "03" ) || isSubStr( name, "04" ) )
		rank += 10;
	return rank;
}

primaryStart( points )
{
	best = undefined;
	bestRank = 0;
	for ( i = 0; i < points.size; i++ )
	{
		if ( !isUsableStart( points[ i ] ) )
			continue;
		rank = startRank( points[ i ] );
		if ( !isDefined( best ) || rank < bestRank )
		{
			best = points[ i ];
			bestRank = rank;
		}
	}
	return best;
}

// IW4 places every later player NEXT TO the first: _specialops_code's player 2
// takes the closest info_player_start_soPlayer2 (or is placed near player 1),
// and pick_starting_location_pmc's get_closest_exclude is the CLOSEST unused
// start to the previous one.  (The old code took the FARTHEST, which on the
// training map would have sent player 2 to the CQB start.)  Excluding "only"
// starts keeps a squad together; when every start is taken the closest one is
// shared.
closestFreeStart( points, anchor, taken )
{
	best = undefined;
	bestDist = 0;
	fallback = undefined;
	fallbackDist = 0;
	for ( i = 0; i < points.size; i++ )
	{
		p = points[ i ];
		if ( !isUsableStart( p ) || startRank( p ) >= 100 )
			continue;
		d = distance( p.origin, anchor.origin );
		if ( !isDefined( fallback ) || d < fallbackDist )
		{
			fallback = p;
			fallbackDist = d;
		}
		used = false;
		for ( j = 0; j < taken.size; j++ )
		{
			if ( taken[ j ] == p )
				used = true;
		}
		if ( used )
			continue;
		if ( !isDefined( best ) || d < bestDist )
		{
			best = p;
			bestDist = d;
		}
	}
	if ( isDefined( best ) )
		return best;
	if ( isDefined( fallback ) )
		return fallback;
	return anchor;
}

pickStartingLocation()
{
	names = spawnClassnames();

	if ( !isDefined( level.coop_taken_spawns ) )
		level.coop_taken_spawns = [];

	for ( i = 0; i < names.size; i++ )
	{
		points = getEntArray( names[ i ], "classname" );
		if ( points.size <= 0 )
			continue;

		if ( !isDefined( level.coop_primary_start ) || level.coop_primary_start.classname != names[ i ] )
		{
			point = primaryStart( points );
			if ( !isDefined( point ) )
				continue;
			level.coop_primary_start = point;
			{}
		}
		else
			point = closestFreeStart( points, level.coop_primary_start, level.coop_taken_spawns );

		if ( level.coop_taken_spawns.size <= 0 )
			point = level.coop_primary_start;

		level.coop_taken_spawns[ level.coop_taken_spawns.size ] = point;
		return point;
	}

	return undefined;
}

startName( point )
{
	if ( isDefined( point.targetname ) )
		return point.targetname;
	return "(unnamed)";
}

spawnPlayer()
{
	self endon( "disconnect" );

	spawnpoint = pickStartingLocation();
	if ( !isDefined( spawnpoint ) )
	{
		{}
		return;
	}

	// Non-team-based, but the engine still needs a session team; every coop
	// arena entry names us_army as allieschar.
	self.sessionteam = "allies";
	self.sessionstate = "playing";

	// self.statusicon is deliberately only ever cleared, never named: the
	// setter resolves the string against the precached status-icon
	// configstrings and Scr_Errors fatally on a name that was not precached,
	// and that precaching is done by the multiplayer chain this script set
	// exists not to load.  The empty string is the documented zero index.
	self.statusicon = "";

	self.maxhealth = level.coop_player_health;
	self.health = self.maxhealth;

	self spawn( spawnpoint.origin, spawnpoint.angles );

	self giveStartingLoadout();

	// modMaxHealth (specialty_coop_armorvest +50) is a script quantity.
	self common_scripts\_perks::applyHealthPerks( level.coop_player_health );

	self.laststand = false;
	if ( level.coop_laststand )
		self common_scripts\_laststand::enable_player();

	self thread healthRegen();

	// The opening ride: a player who spawns before it is over (the first
	// player, and any teammate who joins while it waits or drives) starts
	// seated in the vehicle.
	self maps\_coopintro::board();

	// IW4's own notify for a finished spawn (maps\mp\gametypes\_playerlogic).
	self notify( "spawned_player" );
}

coop_dvar_float( name, value )
{
	if ( getDvar( name ) == "" )
		return value;
	return getDvarFloat( name );
}

// IW4 SP and MP both regenerate player health a few seconds out of damage;
// the multiplayer _healthoverlay that does it there is an mp module, so the
// coop player gets this local copy of CoD4/IW4 _gameskill's regular rule:
// regeneration starts playerHealth_RegularRegenDelay (2 s) after the last
// hit, or longRegenTime (5 s) after a hit that put him in the red (under
// healthOverlayCutoff), and then refills the bar in about two seconds.
healthRegen()
{
	self endon( "disconnect" );
	self endon( "death" );
	self notify( "coop_regen" );
	self endon( "coop_regen" );

	self.coop_last_damage = 0;
	self.coop_was_red = false;
	self.coop_grace_used = false;
	self.coop_invul_until = 0;
	carry = 0;
	for ( ;; )
	{
		wait( 0.1 );
		if ( self.health <= 0 || self.health >= self.maxhealth || ( isDefined( self.laststand ) && self.laststand ) )
		{
			self.coop_was_red = false;
			self.coop_grace_used = false;
			carry = 0;
			continue;
		}
		delay = level.coop_regen_delay;
		if ( self.coop_was_red )
			delay = level.coop_regen_delay_red;
		if ( getTime() - self.coop_last_damage < delay * 1000 )
		{
			carry = 0;
			continue;
		}
		carry += level.coop_regen_rate * 0.1;
		step = int( carry );
		if ( step < 1 )
			continue;
		carry -= step;
		self.health = self.health + step;
		if ( self.health > self.maxhealth )
			self.health = self.maxhealth;
	}
}

coop_is_bullet( sMeansOfDeath )
{
	return isDefined( sMeansOfDeath ) && isSubStr( sMeansOfDeath, "BULLET" );
}

// The regular-difficulty AI hit (CoD4/IW4 _gameskill): the round's damage
// scaled and capped; a "worthy" hit (at least worthyDamageRatio of max
// health) makes the player briefly invulnerable to the next rounds -- 0.35 s,
// 0.5 s on the hit that first puts him in the red, 0.3 s after that -- and a
// hit that would kill a player who was not already at death's door leaves
// him at 1 health and untouchable for player_deathInvulnerableTime.  This is
// what lets a full-health player take several hits and run for cover.
// Returns the damage to apply (0: none).
coop_ai_hit( iDamage, sHitLoc )
{
	now = getTime();
	if ( isDefined( self.coop_invul_until ) && now < self.coop_invul_until )
		return 0;

	// An AI does not pick headshots: the round lands where the scatter puts
	// it, so the weapon's head multiplier (raw 200 against 100 to the body,
	// MEASURED for m16_basic_so) is taken back out.
	if ( isDefined( sHitLoc ) && ( sHitLoc == "head" || sHitLoc == "helmet" || sHitLoc == "neck" ) )
		iDamage = iDamage * 0.5;
	dealt = int( iDamage * level.coop_ai_damage_scale + 0.5 );
	if ( dealt < 1 )
		dealt = 1;
	if ( dealt > level.coop_ai_damage_cap )
		dealt = int( level.coop_ai_damage_cap );

	red = self.maxhealth * level.coop_red_cutoff;
	if ( dealt >= self.health )
	{
		// The death shield: once per trip into the red.
		if ( self.health > 1 && !( isDefined( self.coop_grace_used ) && self.coop_grace_used ) && level.coop_death_grace > 0 )
		{
			self.coop_grace_used = true;
			self.coop_invul_until = now + int( level.coop_death_grace * 1000 );
			self.coop_was_red = true;
			return self.health - 1;
		}
		return dealt;
	}

	if ( dealt >= self.maxhealth * level.coop_worthy_ratio )
	{
		after = self.health - dealt;
		invul = level.coop_invul_pre;
		if ( after <= red )
		{
			if ( self.coop_was_red )
				invul = level.coop_invul_post;
			else
				invul = level.coop_invul_on;
		}
		self.coop_invul_until = now + int( invul * 1000 );
	}
	if ( self.health - dealt <= red )
		self.coop_was_red = true;
	return dealt;
}

respawnPlayer()
{
	self endon( "disconnect" );

	wait( level.coop_respawn_delay );
	if ( isDefined( level.coop_mission_over ) && level.coop_mission_over )
		return;
	self spawnPlayer();
}

// --------------------------------------------------------------------------
// Callbacks
// --------------------------------------------------------------------------

Callback_StartGameType()
{
	// _coop_gametype::main() runs init() at rules-script load.  If this
	// callback is reached without it -- the engine pins the coop category to
	// maps/_coop_gametype now (CODOL_ScriptEntryPoints), but on 2026-10-02 a coop
	// load under g_gametype dm ran the multiplayer dm.gsc main() against this
	// _callbacksetup instead -- run it here, before anything reads its state.
	// No player has connected yet, so init()'s level.players = [] loses nothing.
	if ( !isDefined( level.coop_class_column ) )
	{
		{}
		init();
	}

	level.isSP = false;

	if ( !isDefined( level.players ) )
		level.players = [];

	level.gameEnded = false;
	level.coop_taken_spawns = [];

	// coop_spec_mp_crash_tropical is the only decoded coop zone that ships
	// info_player_start_pmcDefend, so the mission type is derived from the
	// entities rather than declared.  Harmless when the addon MapEnts are not
	// loaded: the array is empty and this stays false.
	defendStarts = getEntArray( "info_player_start_pmcDefend", "classname" );
	level.coop_defend = ( defendStarts.size > 0 );

	// Resolved here rather than in init() because Callback_StartGameType is the
	// precache window this server is demonstrably in: it is where the
	// multiplayer chain's maps\mp\gametypes\_class::init() precaches every
	// loadout weapon today.
	resolveLoadout();

	common_scripts\_perks::init();
	if ( level.coop_laststand )
		coop_laststand_init();

	// The native world gun every armed player carries is a script_model with
	// the tag_origin model linked to his hand (src/codol/player/
	// codol_player_worldweapon.c spawn_worldweapon).  mp precaches tag_origin
	// in _globallogic and zm in its weapon modules; coop compiled neither, so
	// the first player whose character gave him a server DObj killed the
	// server with "Sys_Error: model 'tag_origin' not precached" (MEASURED
	// 2026-10-07 on a coop_spec_mp_estate_tropical probe).
	precacheModel( "tag_origin" );

	// Membership probe only.  A coop map absent from mp/cocoopmapsettings.csv
	// is not an error -- the table lists eleven playlist names and only four of
	// them have a zone on disk -- but the line is worth having in the log when
	// a handoff misbehaves.
	mapname = getDvar( "mapname" );
	if ( tableLookup( coop_map_table(), 0, mapname, 0 ) == "" )
		{}

	level notify( "prematch_over" );

	// Scripted events read from the map entities (_coopevents.gsc): cleanup of
	// MP game objects first, and the vehicle-group precache, which needs this
	// precache window.  Then the training course's pop-up targets.
	maps\_coopevents::init();
	// The training mission's own flow (coop_training_mp_killhouse_sh only):
	// before the course, which leaves the ship mock-up to it, and before the
	// intro ride, which takes Price's lines from it.
	maps\_cooptraining::init();
	// coop_take_mp_fav_tropical's own flow (the meet and the chase).
	maps\_coopmission_take_fav::init();
	// coop_take_mp_estate_tropical's own flow (stealth, house, breach, chase).
	maps\_coopmission_estate::init();
	// coop_defense_mp_crash_tropical's own flow (the crash site).
	maps\_coopmission_defense_crash::init();
	maps\_coopcourse::init();

	// The campaign / Spec Ops mission, driven by the map's own entities (the
	// coop addon MapEnts this server overlays onto the base level).
	level thread maps\_coopmission::main();
	level thread maps\_coopevents::main();

	// The mission's opening vehicle ride, when the map authors one (the
	// training map's humvee_player, the prologue's seaknightPlayer).  init()
	// precaches, so it runs here, in the precache window.
	maps\_coopintro::init();
	level thread maps\_coopintro::main();
	level thread maps\_cooptraining::main();
	level thread maps\_coopmission_take_fav::main();
	level thread maps\_coopmission_estate::main();
	level thread maps\_coopmission_defense_crash::main();

	// Probe harness; inert unless +set scr_coop_test 1.
	level thread maps\_cooptest::main();
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
	self.coop_lives_left = level.coop_lives;
	self.coop_out = false;

	level notify( "connected", self );

	self thread onMenuResponse();
	self thread spawnPlayer();
}

// One Escape resumes from the pause menu, as in zm (04aab03c): C1's pause
// screen answers the closing Escape with "mr <id> <slot> back" and only hides
// its movie, leaving the legacy sf_pausemenu_minimap panel holding
// KEYCATCH_UI.  Two closeInGameMenu ('H', C1 0x005F5160) passes run C1's
// two-step teardown; see zm _zombiemode.gsc onMenuResponse.
onMenuResponse()
{
	self endon( "disconnect" );

	for ( ;; )
	{
		self waittill( "menuresponse", menu, response );

		if ( response != "back" )
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
}

Callback_PlayerDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, timeOffset )
{
	if ( level.gameEnded )
		return;

	// A dead player, between Callback_PlayerKilled and the respawn, has the
	// CLIENT_DEAD entity handler, which has no die function: finishPlayerDamage
	// on him is the script error "No die handler for player entity type 12",
	// which ends the match (MEASURED on a probe: a second hit on a killed coop
	// player; any explosion reaching a body takes the same path).  IW4
	// maps\mp\gametypes\_damage.gsc Callback_PlayerDamage_internal returns
	// first thing for a victim that is not isReallyAlive.
	if ( !isAlive( self ) )
		return;

	// Every coop player is on the same side: no damage from another player.
	if ( isDefined( eAttacker ) && isPlayer( eAttacker ) && eAttacker != self )
		return;
	// Nor from a friendly AI (the cast: Ghost, Price, the allies).
	if ( isDefined( eAttacker ) && isDefined( eAttacker.coop_friendly ) && eAttacker.coop_friendly )
		return;

	iRawDamage = iDamage;
	if ( !isDefined( eAttacker ) || !isPlayer( eAttacker ) )
	{
		if ( coop_is_bullet( sMeansOfDeath ) )
		{
			iDamage = coop_ai_hit( iDamage, sHitLoc );
			if ( iDamage <= 0 )
				return;
		}
		else
		{
			iDamage = int( iDamage * level.coop_world_damage_scale );
			if ( iDamage < 1 )
				iDamage = 1;
			// No single non-player hit takes more than 40 (the rule this
			// mode had for every non-player source before).
			if ( iDamage > 40 )
				iDamage = 40;
		}
	}

	self.coop_last_damage = getTime();
	if ( self.health - iDamage > self.maxhealth * level.coop_red_cutoff )
		self.coop_grace_used = false;
	// The probe harness's measurement hook (_cooptest duel); unset in play.
	if ( isDefined( level.coop_damage_watch ) )
		[[ level.coop_damage_watch ]]( self, iRawDamage, iDamage, eAttacker, sWeapon, sHitLoc );
	self finishPlayerDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, timeOffset );
}

Callback_PlayerKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration )
{
	// A downed player finished off (or bled out) is no longer down.
	self.laststand = false;
	if ( isDefined( self.revivetrigger ) )
	{
		self.revivetrigger delete();
		self.revivetrigger = undefined;
	}

	self.sessionstate = "dead";
	self.deaths++;

	if ( isDefined( level.coop_mission_over ) && level.coop_mission_over )
		return;

	if ( !isDefined( self.coop_lives_left ) )
		self.coop_lives_left = level.coop_lives;
	self.coop_lives_left--;
	if ( self.coop_lives_left < 0 )
	{
		self.coop_out = true;
		self iPrintLnBold( "Out of lives" );
		level notify( "coop_player_out" );
		return;
	}

	self thread respawnPlayer();
}

// CodeCallback_ActorKilled (coop _callbacksetup).  The mission's own
// bookkeeping runs off the actor's "death" notify; this is the per-kill hook.
Callback_ActorKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset )
{
	self maps\_coopmission::onActorKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset );
}

// Reached only when the engine decides a player is eligible for last stand.
// Nothing in this script set grants that eligibility, so it is unreachable
// today; it is registered because GScr_LoadGameTypeScript binds the label with
// errorIfMissing = 1 and the server would not start without it.
//
// The real Spec Ops behaviour is IW4 maps\_coop.gsc:64-201: EnableDeathShield
// converts a lethal hit into a "deathshield" notify, the player goes down and
// bleeds out over 120/90/60 s by difficulty, and a buddy inside the revive
// trigger brings him back.  EnableDeathShield is not a builtin on this server
// and neither is the revive trigger, so implementing that here would mean
// writing the state machine against nothing.  Forwarding to the killed path is
// the honest behaviour until the engine side exists.
Callback_PlayerLastStand( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration )
{
	if ( level.coop_laststand )
	{
		self common_scripts\_laststand::player_laststand();
		return;
	}
	self Callback_PlayerKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration );
}

// --------------------------------------------------------------------------
// Downs and revives: the coop configuration of common_scripts/_laststand.gsc.
// IW4 maps\_coop.gsc player_coop_proc (:64-140): bleed-out 120 s at regular
// difficulty, a 1.5 s revive hold (:771 totalTime), the revive reach
// player_useradius 128 (:868); the downed player holds his pistol (:995).
// Everyone down: IW4 fails the mission at once; here every downed player
// bleeds out now, which is a death and spends a life (Callback_PlayerKilled),
// so the existing lives rule decides whether the mission fails.
// --------------------------------------------------------------------------

coop_laststand_init()
{
	level.laststand_vars = [];
	level.laststand_vars[ "bleedout_time" ] = coop_dvar_float( "scr_coop_bleedout_time", 120 );
	level.laststand_vars[ "revive_time" ] = coop_dvar_float( "scr_coop_revive_time", 1.5 );
	level.laststand_vars[ "trigger_radius" ] = 128;
	level.laststand_vars[ "log" ] = "CoDOL coop laststand";
	level.laststand_funcs = [];
	level.laststand_funcs[ "pistol" ] = ::coop_laststand_pistol;
	level.laststand_funcs[ "all_down" ] = ::coop_all_down;
	common_scripts\_laststand::init();
}

coop_laststand_pistol()
{
	weapons = self getWeaponsListPrimaries();
	for ( i = 0; i < weapons.size; i++ )
	{
		if ( weaponClass( weapons[ i ] ) == "pistol" )
			return weapons[ i ];
	}
	return level.coop_laststand_pistol;
}

coop_all_down()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] common_scripts\_laststand::player_is_in_laststand() )
			players[ i ] thread common_scripts\_laststand::laststand_die();
	}
}
