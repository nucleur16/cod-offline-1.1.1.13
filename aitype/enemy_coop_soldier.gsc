// coop soldier aitype -- the coop species' own template, and the code every
// coop aitype shares.
//
// Contract (src/codol/player/codol_actor_scripts.cpp, CODOL_ActorScriptsLoadType):
// main(), precache() and spawner(), loaded as aitype/<classname without the
// actor_ prefix> for the "soldier" species row, whose category column is
// "coop".  Only the coop species preload names it, so no mp or zm match ever
// compiles it.
//
// Since 2026-10-13 every classname the coop addons author has its own IW4-style
// aitype (aitype\enemy_favela_ar_so, aitype\ally_hero_ghost_socom_so, ...),
// which the engine loads and precaches at level load and runs at spawnactor,
// and whose character\ scripts give the actor its body and head.  Each of them
// calls soldier_setup() below for the coop weapon, engagement and mission
// overrides.  The missions spawn through a template of the authored classname
// (maps\_coopmission::template_for); this generic template is their fallback
// for a classname whose aitype did not load, and then reads the faction and
// role the mission leaves in level.coop_soldier_next, as before.

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
	// Spec Ops Survival plays on the multiplayer chain and its constant window
	// (MP weapons, docs/CODOL_SURVIVAL_PLAN.md): its soldiers fire the
	// multiplayer weapon of the role when the AI table names none.
	if ( getDvar( "g_gametype" ) == "survival" )
		return mp_role_weapons( role );
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

// The multiplayer weapon of each role (IW5 survival's AI table weapons).
mp_role_weapons( role )
{
	names = [];
	switch ( role )
	{
	case "smg":
		names[ 0 ] = "mp5k_mp";
		names[ 1 ] = "ump45_mp";
		break;
	case "lmg":
		names[ 0 ] = "m240_mp";
		names[ 1 ] = "rpd_mp";
		break;
	case "shgn":
		names[ 0 ] = "aa12_mp";
		names[ 1 ] = "spas12_mp";
		break;
	case "snpr":
		names[ 0 ] = "cheytac_mp";
		names[ 1 ] = "barrett_mp";
		break;
	case "pistol":
		names[ 0 ] = "usp_mp";
		names[ 1 ] = "beretta_mp";
		break;
	default:
		names[ 0 ] = "scar_mp";
		names[ 1 ] = "m4_mp";
		break;
	}
	names[ names.size ] = "m4_mp";
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

// The fallback template: faction and role from level.coop_soldier_next (see
// the header), body and head from character\character_coop_soldier.
main()
{
	// animtrees/generic_human.atr, common_mp; declared by
	// animscripts/soldier_init.gsc's #using_animtree.
	self.animTree = "generic_human.atr";
	self.team = "axis";
	self.type = "soldier";
	self.accuracy = 0.3;
	self.health = 150;

	kind = level.coop_soldier_next;
	faction = "militia";
	role = "ar";
	if ( isDefined( kind ) )
	{
		faction = kind.faction;
		role = kind.role;
	}
	soldier_setup( role );

	if ( scripted_character() )
		return;
	character\character_coop_soldier::main( faction, role );
}

// What a mission may ask of the actor it is about to spawn, left in
// level.coop_soldier_next immediately before spawnActor() (which runs the
// aitype main() synchronously).  Every field is optional; the aitype's own
// values stand where one is absent.
//   team       "allies" for a friendly (a cast member, Vazquez, a civilian)
//   health     e.g. 1000 for an invulnerable-looking cast member
//   accuracy   the authored spawner's script_accuracy
//   role       the coop weapon role, overriding the aitype's
//   char_index the authored spawner's script_char_index (IW4's designer pick)
//   character  a character\ main() to wear instead of the aitype's own
//   body, head explicit models (the generic template's fallback)
//
// Then the coop weapon for the role, and the engagement bands.  The aitype has
// set team, type, accuracy and health before calling this.
soldier_setup( role )
{
	kind = level.coop_soldier_next;
	if ( isDefined( kind ) )
	{
		if ( isDefined( kind.role ) )
			role = kind.role;
		if ( isDefined( kind.health ) )
			self.health = kind.health;
		if ( isDefined( kind.accuracy ) )
			self.accuracy = kind.accuracy;
		if ( isDefined( kind.team ) )
			self.team = kind.team;
		if ( isDefined( kind.char_index ) )
			self.script_char_index = kind.char_index;
	}
	// The players' side: his rounds never hurt a player (_coopmode
	// Callback_PlayerDamage), as a friendly AI's do not in SP.
	self.coop_friendly = ( self.team == "allies" );
	self.coop_role = role;

	// Resolved on first use, not in precache(): codol_isWeaponPublished needs
	// the engine-to-wire map of the published window, which does not exist
	// until the first gamestate (MEASURED: every role resolved to nothing when
	// asked from precache), and soldiers only spawn once a player is in.
	if ( !isDefined( level.coop_role_weapon ) )
		level.coop_role_weapon = [];
	weapon = level.coop_role_weapon[ role ];
	// A spawner's own weapon (survival's AI table, column 5), when the
	// window carries it.
	if ( isDefined( kind ) && isDefined( kind.weapon ) && kind.weapon != "" &&
	     ( codol_isWeaponPublished( kind.weapon ) || codol_isWeaponMounted( kind.weapon ) ) )
		weapon = kind.weapon;
	else if ( !isDefined( weapon ) )
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
}

// True when the mission chose this actor's look itself (kind.character or
// kind.body); the aitype then skips its own character.
scripted_character()
{
	kind = level.coop_soldier_next;
	if ( !isDefined( kind ) )
		return false;
	if ( isDefined( kind.character ) )
	{
		self [[ kind.character ]]();
		return true;
	}
	if ( isDefined( kind.body ) )
	{
		character\character_coop_soldier::cast_model( kind.body, kind.head );
		return true;
	}
	return false;
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
	// (A mode may have recorded its own weapons' models before this runs.)
	if ( !isDefined( level.coop_weapon_models ) )
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
