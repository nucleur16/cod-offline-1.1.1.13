// coop core: tuning, callbacks, spawn, loadout.
//
// The coop counterpart of the multiplayer _gamelogic/_playerlogic/_damage trio,
// reduced to what a Spec Ops match needs to boot and hold a player.  It is the
// sibling of zm/maps/mp/gametypes/_zombiemode.gsc and follows the same three
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
// class0_up2..class4_up2.  Rows seen include loadoutPrimary (m16_basic,
// m16_grenadier_reddot), loadoutSecondary (barrett_zoom, usp_silencer_akimbo,
// tmp_silencer_reflex), loadoutMeleeWpn (coop_mel_karambit, coop_mel_knife,
// coop_mel_bonesaw, coop_mel_bowie, coop_mel_sog), loadoutPerk1..3
// (specialty_coop_*), loadoutKillstreak1..4, loadoutLethal, loadoutTactical.
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
	value = tableLookup( coop_class_table(), 0, row, level.coop_class_column );
	if ( !isDefined( value ) )
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

	// IW4 player health is 100 with regeneration; Spec Ops does not change it
	// (maps\_utility::set_player_health is only used by scripted moments).
	level.coop_player_health = 100;

	level.gameEnded = false;
}

// --------------------------------------------------------------------------
// Level-script entry point.
//
// GScr_LoadScripts asks for exactly one module, maps/mp/<mapname>, for every
// asset category (src/scr_vm_main.c).  coop is the only category whose map name
// is NOT category-distinct: the lobby's two spellings for the same match are
// the playlist name (coop_training_mp_killhouse_sh) and the base level name
// (mp_killhouse_sh), and the coop zones ship no world assets at all -- only an
// addon_map_ents -- so the level zone really is the shared mp_* one.  That is
// why coop/maps/mp/ carries BOTH spellings of all nine playlists, and why the
// base-map spellings there deliberately shadow the multiplayer level scripts of
// the same name.  Each of those files is a few lines and calls this.
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
	if ( isDefined( visionName ) && visionName != "" )
	{
		level.coop_vision = visionName;
		VisionSetNaked( visionName, 0 );
		VisionSetPain( visionName );
	}
}

// --------------------------------------------------------------------------
// Loadout
// --------------------------------------------------------------------------

// Probe a candidate list in order and return the first name the mounted coop
// zones actually carry.  Never returns a name the client cannot resolve.
resolveWeapon( candidates )
{
	for ( i = 0; i < candidates.size; i++ )
	{
		name = candidates[ i ];
		if ( !isDefined( name ) || name == "" )
			continue;

		precacheItem( name );
		if ( getWeaponIndexFromName( name ) > 0 )
			return name;
	}

	return undefined;
}

// One class table row -> a usable weapon name, with the row's own value first.
weaponFromRow( row, fallback1, fallback2 )
{
	candidates = [];
	candidates[ 0 ] = class_value( row );
	candidates[ 1 ] = fallback1;
	candidates[ 2 ] = fallback2;

	return resolveWeapon( candidates );
}

resolveLoadout()
{
	// MEASURED names out of mp/cocoopclasstable.csv.  The fallbacks are the
	// literal cells that decoded to text in that table; a cell this decode could
	// not resolve simply misses the probe and the next candidate is tried.
	level.coop_primary = weaponFromRow( "loadoutPrimary", "m16_basic", "m16_grenadier_reddot" );
	level.coop_secondary = weaponFromRow( "loadoutSecondary", "usp_silencer_akimbo", "tmp_silencer_reflex" );
	level.coop_melee = weaponFromRow( "loadoutMeleeWpn", "coop_mel_knife", "coop_mel_karambit" );

	if ( isDefined( level.coop_primary ) )
		println( "CoDOL coop: primary " + level.coop_primary );
	else
		println( "CoDOL coop: no primary weapon from " + coop_class_table() + " is present in the mounted coop zones" );

	if ( isDefined( level.coop_secondary ) )
		println( "CoDOL coop: secondary " + level.coop_secondary );

	if ( isDefined( level.coop_melee ) )
		println( "CoDOL coop: melee " + level.coop_melee );
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

	if ( isDefined( level.coop_melee ) )
		self giveWeapon( level.coop_melee );

	if ( isDefined( primary ) )
		self switchToWeapon( primary );

	// mp/basemaps.arena gives every coop_* entry allieschar "us_army" /
	// axischar "opforce_composite", and every ally actor in the coop addon
	// MapEnts is an actor_ally_codo_tf_* or actor_ally_tf_141_* model, so the
	// coop player is Task Force.  zone_faction() passes a non-zm category
	// through unchanged (src/codol/player/codol_player_appearance.c), so "tf"
	// is what composes.  INFERRED from those two data points; C1's own coop
	// character selector index has not been read out of the profile yet.
	if ( isDefined( primary ) )
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

// IW4's pick_starting_location_pmc spreads players with get_closest_exclude.
// getclosest is not a builtin on this server, so the same rule is expressed
// with distance(): take the candidate whose nearest already-taken point is the
// farthest away.
farthestFromTaken( points, taken )
{
	best = undefined;
	bestScore = -1;

	for ( i = 0; i < points.size; i++ )
	{
		score = 0;
		for ( j = 0; j < taken.size; j++ )
		{
			d = distance( points[ i ].origin, taken[ j ].origin );
			if ( j == 0 || d < score )
				score = d;
		}

		if ( !isDefined( best ) || score > bestScore )
		{
			best = points[ i ];
			bestScore = score;
		}
	}

	return best;
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

		if ( level.coop_taken_spawns.size <= 0 || points.size == 1 )
			point = points[ randomInt( points.size ) ];
		else
			point = farthestFromTaken( points, level.coop_taken_spawns );

		level.coop_taken_spawns[ level.coop_taken_spawns.size ] = point;
		return point;
	}

	return undefined;
}

spawnPlayer()
{
	self endon( "disconnect" );

	spawnpoint = pickStartingLocation();
	if ( !isDefined( spawnpoint ) )
	{
		println( "CoDOL coop: no usable spawn point on " + getDvar( "mapname" ) );
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
}

respawnPlayer()
{
	self endon( "disconnect" );

	wait( level.coop_respawn_delay );
	self spawnPlayer();
}

// --------------------------------------------------------------------------
// Callbacks
// --------------------------------------------------------------------------

Callback_StartGameType()
{
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

	// Membership probe only.  A coop map absent from mp/cocoopmapsettings.csv
	// is not an error -- the table lists eleven playlist names and only four of
	// them have a zone on disk -- but the line is worth having in the log when
	// a handoff misbehaves.
	mapname = getDvar( "mapname" );
	if ( tableLookup( coop_map_table(), 0, mapname, 0 ) == "" )
		println( "CoDOL coop: " + mapname + " is not a row of " + coop_map_table() );

	level notify( "prematch_over" );
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

	self thread spawnPlayer();
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

	self finishPlayerDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, timeOffset );
}

Callback_PlayerKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration )
{
	self.sessionstate = "dead";
	self.deaths++;

	self thread respawnPlayer();
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
	self Callback_PlayerKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration );
}
