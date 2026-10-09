// coop enemy soldier aitype -- the template every coop map's authored
// actor_enemy_<faction>_<role>_so spawner is spawned through.
//
// Contract (src/codol/player/codol_actor_scripts.cpp, CODOL_ActorScriptsLoadType):
// main(), precache() and spawner(), loaded as aitype/<classname without the
// actor_ prefix> for the "soldier" species row, whose category column is
// "coop".  Only the coop species preload names it, so no mp or zm match ever
// compiles it.
//
// IW4 ships one aitype per authored classname (aitype\enemy_favela_ar_so ...)
// and none of them is in a CoDOL zone (no zone ships an aitype or character
// asset; docs/CODOL_CAMPAIGN_MODE.md section 4).  The coop mission script
// (maps\_coopmission) therefore reads the authored spawner's
// classname, turns it into a faction and a role, and leaves that choice in
// level.coop_soldier_next immediately before spawnActor() -- which runs this
// main() synchronously -- the same way IW4's aitype files hard-code theirs.

/*QUAKED actor_enemy_coop_soldier (1.0 0.25 0.0) (-16 -16 0) (16 16 72) SPAWNER MAKEROOM UNDELETABLE ENEMYINFO SCRIPT_FORCESPAWN
defaultmdl="body_complete_sp_juggernaut"
"count" -- max AI to ever spawn from this spawner
SPAWNER -- makes this a spawner instead of a guy
*/

// Weapon per role.  A soldier's shot (codol_actorShoot) publishes
// EV_FIRE_WEAPON on him with the weapon's index, and C1 dereferences that
// index's definition with no NULL check, so an
// enemy may only fire a weapon the client has REGISTERED: a slot of C1's coop
// constant window (mp/constconfigstrings_coop.csv,
// docs/CODOL_CATEGORY_WEAPON_WINDOW_20260917.md section 1.1) that this
// server actually published.  MEASURED 2026-10-07 on a coop_spec_mp_estate
// probe: the server publishes slots 1..31 and stops at slot 32
// (mp5k_codo_so is not buildable from the mounted archives), so every name
// below is one of slots 2..30 -- ak47_so (52), saw_so (44), m1014_so (55) and
// deserteagle_so (63) are deliberately NOT used.  codol_isWeaponPublished is
// the gate (a live definition with a wire index), so a shorter window simply
// drops a candidate.
role_weapons( role )
{
	names = [];
	switch ( role )
	{
	case "smg":
		names[ 0 ] = "ump45_so";
		names[ 1 ] = "mp5_silencer_so";
		names[ 2 ] = "tmp_silencer_so";
		break;
	case "lmg":
		names[ 0 ] = "rpd_so";
		break;
	case "shgn":
		names[ 0 ] = "aa12_so";
		break;
	case "snpr":
		names[ 0 ] = "cheytac_silencer_so";
		names[ 1 ] = "barrett_so";
		break;
	case "pistol":
		names[ 0 ] = "beretta_so";
		names[ 1 ] = "usp_silencer_so";
		break;
	default:
		// ar, ak47, rpg (the RPG role keeps a rifle; see the doc's "left" list)
		names[ 0 ] = "m16_basic_so";
		names[ 1 ] = "m16_reflex_so";
		break;
	}
	names[ names.size ] = "m16_basic_so";
	return names;
}

resolve_role_weapon( role )
{
	// Published first.  But MEASURED 2026-10-07 on coop_prolog_mp_crash_intro
	// after an in-process map change: codol_isWeaponPublished answered 0 even
	// for the player's own mp5_silencer_so while the server logged the window
	// "published 32 of 64" -- the engine-to-wire reverse map it reads is not
	// trustworthy in coop yet (docs/CODOL_CAMPAIGN_MODE.md, "left").  Every
	// candidate here is one of window slots 2..30, which that publish covers,
	// so a mounted candidate is the fallback.
	names = role_weapons( role );
	for ( i = 0; i < names.size; i++ )
	{
		if ( codol_isWeaponPublished( names[ i ] ) )
			return names[ i ];
	}
	for ( i = 0; i < names.size; i++ )
	{
		if ( codol_isWeaponMounted( names[ i ] ) )
			return names[ i ];
	}
	return undefined;
}

main()
{
	// animtrees/generic_human.atr, common_mp; declared by
	// animscripts/soldier_init.gsc's #using_animtree.
	self.animTree = "generic_human.atr";
	self.team = "axis";
	self.type = "soldier";

	kind = level.coop_soldier_next;
	faction = "militia";
	role = "ar";
	health = 150;
	accuracy = 0.3;
	if ( isDefined( kind ) )
	{
		faction = kind.faction;
		role = kind.role;
		if ( isDefined( kind.health ) )
			health = kind.health;
		if ( isDefined( kind.accuracy ) )
			accuracy = kind.accuracy;
		// A cast member (_cooptraining.gsc): a friendly soldier on the allies
		// team with the body and head the level authors for him.
		if ( isDefined( kind.team ) )
			self.team = kind.team;
	}
	// The players' side: his rounds never hurt a player (_coopmode
	// Callback_PlayerDamage), as a friendly AI's do not in SP.
	self.coop_friendly = false;
	if ( isDefined( kind ) && isDefined( kind.team ) )
		self.coop_friendly = ( kind.team == "allies" );

	self.accuracy = accuracy;
	self.health = health;
	self.coop_role = role;

	// Resolved on first use, not in precache(): codol_isWeaponPublished needs
	// the engine-to-wire map of the published window, which does not exist
	// until the first gamestate (MEASURED: every role resolved to nothing when
	// asked from precache), and soldiers only spawn once a player is in.
	if ( !isDefined( level.coop_role_weapon ) )
		level.coop_role_weapon = [];
	weapon = level.coop_role_weapon[ role ];
	if ( !isDefined( weapon ) )
	{
		weapon = resolve_role_weapon( role );
		if ( !isDefined( weapon ) )
			weapon = "";
		level.coop_role_weapon[ role ] = weapon;
		codolprintln( "CoDOL coop soldier: role " + role + " fires '" + weapon + "'" );
	}
	// coop_weapon, not .weapon: the engine owns an entity field of that name
	// and an actor reads it back empty (MEASURED on the first probe), so the
	// animscripts and the mission read this script field instead.
	self.coop_weapon = weapon;
	self.secondaryweapon = "";
	self.sidearm = "";
	self.grenadeWeapon = "";
	self.grenadeAmmo = 0;
	self.csvInclude = "";

	// Engagement distances by role, IW4 animscripts\combat_utility's bands.
	// meleeAttackDist is the native "in attack range" distance: the frame
	// switches to soldier_combat once the enemy is this close AND the knee and
	// head lines to him are clear (CODOL_ActorMeleeRangeGoal), which is the
	// soldier's line-of-fire test.
	switch ( role )
	{
	case "shgn":
	case "pistol":
		self setEngagementMinDist( 0, 0 );
		self setEngagementMaxDist( 400, 600 );
		self.meleeattackdist = 600;
		break;
	case "snpr":
		self setEngagementMinDist( 512, 256 );
		self setEngagementMaxDist( 3000, 4000 );
		self.meleeattackdist = 3000;
		break;
	default:
		self setEngagementMinDist( 128, 0 );
		self setEngagementMaxDist( 1200, 1600 );
		self.meleeattackdist = 1400;
		break;
	}

	if ( isDefined( kind ) && isDefined( kind.body ) )
	{
		character\character_coop_soldier::cast_model( kind.body, kind.head );
		return;
	}
	character\character_coop_soldier::main( faction, role );
}

spawner()
{
	self setspawnerteam( "axis" );
}

precache()
{
	character\character_coop_soldier::precache();

	// The world model of every candidate the mounted zones carry is precached
	// now, in the only window a model can be; which candidate a role actually
	// fires is decided at the first spawn (see main()).  Every candidate is one
	// of the coop window's slots 2..30, which the server registers while it
	// builds the window at map load, so getWeaponModel registers nothing new.
	level.coop_role_weapon = [];
	// Only a model recorded here may be attached later: attaching one that was
	// not precached is a fatal script error.  (MEASURED on
	// coop_spec_mp_crash_tropical: "model 'weapon_cheytac' not precached".
	// The cause was native: G_ModelIndex gave weapon_m82 the slot IW3's
	// constant table names for it, overwriting weapon_cheytac, which a
	// precache moments earlier had put there -- fixed in _g_utils_mp.asm.)
	level.coop_weapon_models = [];
	roles = [];
	roles[ 0 ] = "ar";
	roles[ 1 ] = "smg";
	roles[ 2 ] = "lmg";
	roles[ 3 ] = "shgn";
	roles[ 4 ] = "snpr";
	roles[ 5 ] = "pistol";
	for ( r = 0; r < roles.size; r++ )
	{
		names = role_weapons( roles[ r ] );
		for ( i = 0; i < names.size; i++ )
		{
			if ( !codol_isWeaponMounted( names[ i ] ) )
				continue;
			model = getWeaponModel( names[ i ] );
			if ( isDefined( model ) && model != "" )
			{
				precacheModel( model );
				level.coop_weapon_models[ model ] = true;
				codolprintln( "CoDOL coop soldier: precached " + model + " for " + names[ i ] );
			}
		}
	}
}
