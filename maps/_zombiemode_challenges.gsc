// zm challenges, experience and the games log.
//
// WHAT C1 DEFINES.  mp/allChallengesTable.csv (611 x 47, patch_code_post_gfx_mp)
// carries two zombies categories, joined through mp/challengecategories.csv:
//
//   zm_weapon  ids 600-669   zm_marksman_<weapon>  "kill &&1 with this weapon"
//                            zm_expert_<weapon>    "&&1 headshot kills with it"
//                            <weapon> is the zm weapon name with its _zm
//                            suffix, e.g. zm_marksman_m1911_zm.
//   zm_unique  ids 700-719   zm_doors, zm_windows, zm_powerups, zm_perks,
//                            zm_wallweapons, zm_crateweapons, zm_traps,
//                            zm_revives, zm_packapunches, zm_tokens and the
//                            per-map, per-difficulty completions
//                            zm_<map>_<1..3> and zm_zombie_training_1.
//
// THE STORAGE IS THE MP ONE.  Progress is one cumulative float per challenge
// in the secure profile's challenges[1000], indexed by the numeric id in
// column 4 -- the same member maps\mp\gametypes\_missions.gsc ch_setProgress
// writes.  src/codol/stats/codol_match_report.c measures every id's session
// value against the attached profile and sends the rise to the lobby as
// progression.challenges, so nothing native is zm-specific: a write here is a
// write the lobby folds into players/<uid>.json.  The tier is derived, never
// stored (CODOL_ProfileChallengeState), so only the progress is written.
//
// THIS FILE NAMES NO MULTIPLAYER MODULE (see _zombiemode.gsc's header): the
// table is read directly rather than through _missions::buildChallegeInfo.
//
// EXPERIENCE.  Two C1 sources, both read from the mounted tables:
//   * mp/gametypesxptable.csv (common_gametype_zm.ff) row "zm": kill 5,
//     headshot 0, win/loss/tie 0, challenge 0.  Paid per zombie kill.
//   * a challenge tier's reward, columns 24..32, paid when the tier is
//     reached -- exactly what _missions::processChallenge does in MP through
//     giveRankXP( "challenge", reward ).
// Experience is the secure profile's rank/experience pair, written through
// setRankExperience exactly as _rank::codolSetTotalRankXP does, and the match
// report already measures it.
//
// WHO IS COUNTED.  A player with an attached secure profile.  Every human has
// one (the lobby attaches it at connect); a bot has none, and getPlayerData on
// a bot is a script error.  scr_zm_challenges_testclients 1 admits test
// clients, for a developer test that binds them to a profile first
// (codol_bindprofile).
//
// NOT COUNTED HERE, and why:
//   zm_packapunches    -- no Pack-a-Punch machine exists in this zm port.
//                         zm_challenge_inc( "zm_packapunches" ) is the hook.
//   zm_tokens          -- revive tokens are a C1 store item this server has
//                         no consumer for.
//   zm_<map>_<1..3>, zm_zombie_training_1
//                      -- "complete <map> on <difficulty>": the rescue round
//                         that ends a CoDOL zombies map, and the difficulty
//                         selection, are not ported; the game only ends when
//                         everyone is down.  zm_map_completed() is the hook.

#include maps\_zombiemode_utility;

init()
{
	level.zm_challenge_ids = [];
	level.zm_challenge_targets = [];
	level.zm_challenge_rewards = [];

	table = "mp/allChallengesTable.csv";

	// The zm rows are the table's last ones (ids 600+).  Stop on a run of
	// empty refs rather than the first one, so one blank row cannot hide
	// them, and never read past any plausible table size.
	empty = 0;
	count = 0;
	for ( row = 0; row < 2048 && empty < 8; row++ )
	{
		ref = tableLookupByRow( table, row, 0 );
		if ( ref == "" )
		{
			empty++;
			continue;
		}
		empty = 0;

		if ( ref.size < 4 || getSubStr( ref, 0, 3 ) != "zm_" )
			continue;

		id = tableLookupByRow( table, row, 4 );
		if ( id == "" )
			continue;
		id = int( id );
		if ( id < 0 || id >= 1000 )
			continue;

		// Mounted CoDOL rows only: challengeDeactivate (common_gametype_zm)
		// retires ids the lobby would refuse anyway.
		if ( tableLookup( "mp/challengeDeactivate.csv", 1, "" + id, 0 ) != "" )
			continue;

		targets = [];
		rewards = [];
		for ( tier = 0; tier < 9; tier++ )
		{
			target = tableLookupByRow( table, row, 6 + tier * 2 );
			if ( target == "" || int( target ) <= 0 )
				break;
			targets[ tier ] = int( target );
			rewards[ tier ] = int( tableLookupByRow( table, row, 24 + tier ) );
		}
		if ( targets.size == 0 )
			continue;

		level.zm_challenge_ids[ ref ] = id;
		level.zm_challenge_targets[ ref ] = targets;
		level.zm_challenge_rewards[ ref ] = rewards;
		count++;
	}

	// mp/gametypesxptable.csv: MODE, win, loss, tie, kill, headshot, ...
	level.zm_kill_xp = int( tableLookup( "mp/gametypesxptable.csv", 0, "zm", 4 ) );
	level.zm_headshot_xp = int( tableLookup( "mp/gametypesxptable.csv", 0, "zm", 5 ) );

	// The rank table caps experience; setRankExperience refuses a total past
	// the last rank's ceiling.  Column 7 is a rank's maximum XP.
	maxrank = tableLookup( "mp/rankTable.csv", 0, "maxrank", 1 );
	level.zm_max_xp = 0;
	if ( maxrank != "" )
		level.zm_max_xp = int( tableLookup( "mp/rankTable.csv", 0, maxrank, 7 ) );

	// Weapons whose challenge row is named after another weapon: the
	// mounted ref is the base the variant is built from.
	level.zm_challenge_weapon_alias = [];
	level.zm_challenge_weapon_alias[ "mk_scar_h_zm" ] = "scar_h_eotech_zm";
	level.zm_challenge_weapon_alias[ "gl_m16_zm" ] = "m16_zm";
	level.zm_challenge_weapon_alias[ "knife_ballistic_bowie_zm" ] = "knife_ballistic_zm";

	codolprintln( "CoDOL zm challenges: " + count + " zm challenge(s) from " + table +
		", kill XP " + level.zm_kill_xp + ", headshot XP " + level.zm_headshot_xp +
		", XP cap " + level.zm_max_xp );

	level thread on_player_connect();

	// Developer-only test verbs (scr_zm_chtest); returns at once on the live
	// server, which runs developer 0.
	level thread maps\_zombiemode_challengetest::init();
}

on_player_connect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread player_watch();
	}
}

player_watch()
{
	// The games log identifies a player the way MP's _playerlogic does.
	self.zm_log_guid = self getGuid();
	self.zm_log_num = self getEntityNumber();
	self.zm_log_name = self.name;
	logPrint( "J;" + self.zm_log_guid + ";" + self.zm_log_num + ";" + self.zm_log_name + "\n" );

	self thread watch_disconnect();
	self thread watch_kills();
	self thread watch_revived();
	self thread watch_perks();
	self thread watch_traps();
}

watch_disconnect()
{
	guid = self.zm_log_guid;
	num = self.zm_log_num;
	name = self.zm_log_name;
	self waittill( "disconnect" );
	logPrint( "Q;" + guid + ";" + num + ";" + name + "\n" );
}

// --------------------------------------------------------------------------
// The counting API.  self = the player.
// --------------------------------------------------------------------------

zm_challenges_enabled()
{
	if ( !isDefined( level.zm_challenge_ids ) || !isPlayer( self ) )
		return false;
	if ( self.isbot && !getDvarInt( "scr_zm_challenges_testclients" ) )
		return false;
	return true;
}

// Advance a zm challenge by `amount` (default 1) and pay the reward of every
// tier the advance reaches.  An unknown or retired ref is a no-op.
zm_challenge_inc( ref, amount )
{
	if ( !self zm_challenges_enabled() )
		return;
	if ( !isDefined( level.zm_challenge_ids[ ref ] ) )
		return;
	if ( !isDefined( amount ) )
		amount = 1;
	if ( amount <= 0 )
		return;

	id = level.zm_challenge_ids[ ref ];
	before = self getPlayerData( "challenges", id );
	after = before + amount;
	self setPlayerData( "challenges", id, after );

	targets = level.zm_challenge_targets[ ref ];
	rewards = level.zm_challenge_rewards[ ref ];
	for ( tier = 0; tier < targets.size; tier++ )
	{
		if ( before >= targets[ tier ] || after < targets[ tier ] )
			continue;

		reward = 0;
		if ( isDefined( rewards[ tier ] ) )
			reward = rewards[ tier ];
		codolprintln( "CoDOL zm challenge award: " + self.name + " " + ref + " (id " + id +
			") tier " + ( tier + 1 ) + " at " + after + ", reward " + reward + " XP" );
		self zm_give_xp( reward );
	}
}

// Add experience to the secure profile's rank/experience pair.
zm_give_xp( amount )
{
	if ( !isDefined( amount ) || amount <= 0 )
		return;
	if ( !self zm_challenges_enabled() )
		return;

	total = self getRankExperience();
	if ( level.zm_max_xp > 0 && total >= level.zm_max_xp )
		return;
	total = total + amount;
	if ( level.zm_max_xp > 0 && total > level.zm_max_xp )
		total = level.zm_max_xp;
	self setRankExperience( total );
}

// The base weapon a zm weapon challenge is named after: the upgraded
// (Pack-a-Punch / box) variant counts toward its base weapon.
zm_challenge_weapon( weapon )
{
	if ( !isDefined( weapon ) || weapon == "" || weapon == "none" )
		return undefined;

	suffix = "_upgraded_zm";
	if ( weapon.size > suffix.size && getSubStr( weapon, weapon.size - suffix.size, weapon.size ) == suffix )
		weapon = getSubStr( weapon, 0, weapon.size - suffix.size ) + "_zm";

	if ( isDefined( level.zm_challenge_weapon_alias[ weapon ] ) )
		weapon = level.zm_challenge_weapon_alias[ weapon ];

	return weapon;
}

zm_is_bullet( mod )
{
	if ( !isDefined( mod ) )
		return false;
	return mod == "MOD_PISTOL_BULLET" || mod == "MOD_RIFLE_BULLET" || mod == "MOD_HEAD_SHOT";
}

zm_is_headshot( mod, hit_location )
{
	if ( isDefined( mod ) && mod == "MOD_HEAD_SHOT" )
		return true;
	if ( !isDefined( hit_location ) )
		return false;
	if ( hit_location != "head" && hit_location != "helmet" )
		return false;
	// A blast or a blade that happens to report the head is not a headshot.
	if ( isDefined( mod ) && ( mod == "MOD_MELEE" || mod == "MOD_GRENADE_SPLASH" ||
		mod == "MOD_EXPLOSIVE" || mod == "MOD_PROJECTILE_SPLASH" ) )
		return false;
	return true;
}

// Hook for a map's end: the rescue round reached on difficulty 1..3.
zm_map_completed( difficulty )
{
	self zm_challenge_inc( getDvar( "mapname" ) + "_" + difficulty );
}

// The killing hit, as the engine reports it to CodeCallback_ActorKilled
// (_callbacksetup.gsc).  self = the actor.  Recorded for watch_kills, which
// runs after zombie_death_event's waittillframeend, i.e. after this.
Callback_ActorKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset )
{
	self.zm_kill_mod = sMeansOfDeath;
	self.zm_kill_weapon = sWeapon;
	self.zm_kill_hitloc = sHitLoc;
}

// --------------------------------------------------------------------------
// The listeners.  Each waits on a notify the subsystem already sends, so the
// subsystems need no knowledge of challenges.
// --------------------------------------------------------------------------

// _zombiemode_spawner::zombie_death_event: `attacker notify( "zom_kill",
// zombie )` after it has waited for the killing hit's damage notify, so the
// zombie's damagemod / damagelocation / damageweapon describe the kill.
watch_kills()
{
	self endon( "disconnect" );

	for ( ;; )
	{
		self waittill( "zom_kill", zombie );

		mod = undefined;
		hit_location = undefined;
		weapon = undefined;
		if ( isDefined( zombie ) )
		{
			// The engine's own account of the killing hit first; the
			// damage notify's fields (a bone name, not a hit location) after.
			mod = zombie.zm_kill_mod;
			hit_location = zombie.zm_kill_hitloc;
			weapon = zombie.zm_kill_weapon;
			if ( !isDefined( mod ) )
				mod = zombie.damagemod;
			if ( !isDefined( weapon ) || weapon == "none" )
				weapon = zombie.damageweapon;
		}

		// A bullet with no weapon on record came out of the gun in hand.
		if ( ( !isDefined( weapon ) || weapon == "none" ) && zm_is_bullet( mod ) )
			weapon = self getCurrentWeapon();

		self log_zombie_kill( zombie, weapon, mod, hit_location );

		if ( !self zm_challenges_enabled() )
			continue;

		headshot = zm_is_headshot( mod, hit_location );

		self zm_give_xp( level.zm_kill_xp );
		if ( headshot )
			self zm_give_xp( level.zm_headshot_xp );

		// A melee kill is the knife's, whatever is in the player's hands.
		if ( isDefined( mod ) && mod == "MOD_MELEE" )
			continue;

		base = zm_challenge_weapon( weapon );
		if ( !isDefined( base ) )
			continue;

		self zm_challenge_inc( "zm_marksman_" + base );
		if ( headshot )
			self zm_challenge_inc( "zm_expert_" + base );
	}
}

// MP's K; line (maps\mp\gametypes\_damage.gsc logPrintPlayerDeath), with the
// zombie as the victim: no guid, number -1, team axis, name "zombie".
log_zombie_kill( zombie, weapon, mod, hit_location )
{
	if ( !isDefined( weapon ) )
		weapon = "none";
	if ( !isDefined( mod ) )
		mod = "MOD_UNKNOWN";
	if ( !isDefined( hit_location ) )
		hit_location = "none";
	damage = 0;
	if ( isDefined( zombie ) && isDefined( zombie.maxhealth ) )
		damage = zombie.maxhealth;

	logPrint( "K;;-1;axis;zombie;" + self getGuid() + ";" + self getEntityNumber() + ";allies;" +
		self.name + ";" + weapon + ";" + damage + ";" + mod + ";" + hit_location + "\n" );
}

// _zombiemode_laststand::revive_success: `self notify( "player_revived",
// reviver )` on the player who got up.  A self revive (solo Quick Revive)
// names no other reviver and is not a revive of a teammate.
watch_revived()
{
	self endon( "disconnect" );

	for ( ;; )
	{
		self waittill( "player_revived", reviver );

		if ( !isDefined( reviver ) || !isPlayer( reviver ) || reviver == self )
			continue;

		reviver zm_challenge_inc( "zm_revives" );
	}
}

// _zombiemode_perks::give_perk: `self notify( "perk_bought", perk )` once the
// bottle is finished -- a machine soda, or the free-perk power-up's.
watch_perks()
{
	self endon( "disconnect" );

	for ( ;; )
	{
		self waittill( "perk_bought", perk );
		self zm_challenge_inc( "zm_perks" );
	}
}

// _zombiemode_traps_placeable: `player notify( "zm_trap_activated", weapon )`
// when the owner's trap goes live.
watch_traps()
{
	self endon( "disconnect" );

	for ( ;; )
	{
		self waittill( "zm_trap_activated", weapon );
		self zm_challenge_inc( "zm_traps" );
	}
}
