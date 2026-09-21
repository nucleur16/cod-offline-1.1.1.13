// zm weapons: the box pool, the mystery box, and wall buys.
//
// Port of the purchasable half of T5's maps\_zombiemode_weapons.gsc (3,106
// lines).
//
// TAKEN, by T5 function:
//   add_zombie_weapon              :28    -- the weapon record: upgrade name,
//                                            hint, cost, ammo cost, in_box.
//   include_zombie_weapon          :188
//   is_weapon_included             :177
//   get_weapon_hint/cost/ammo_cost :677,:684,:691
//   get_is_in_box                  :698
//   is_weapon_upgraded             :708
//   has_upgrade                    :734
//   has_weapon_or_upgrade          :755
//   treasure_chest_init            :793
//   get_chest_pieces               :945
//   treasure_chest_think           :1057  -- pay, roll, grab-or-timeout, close.
//   treasure_chest_lid_open/close  :1752,:1763
//   treasure_chest_timeout         :1742
//   treasure_chest_ChooseRandomWeapon         :1774
//   treasure_chest_ChooseWeightedRandomWeapon :1784
//   treasure_chest_weapon_spawn    :1951  -- the 40-cycle mario-kart roll and
//                                            the floating weapon model.
//   treasure_chest_give_weapon     :2365
//   weapon_spawn_think             :2649  -- the wall buy: first purchase,
//                                            then ammo at ammo_cost.
//   weapon_show                    :2798
//   init_weapon_upgrade            :356   -- wall-buy discovery.
//
// NOT TAKEN, and why:
//   The chest LID is a RotateRoll in T5, not an animation, so it ports; but
//   default_pandora_show_func / default_pandora_fx_func / treasure_chest_glowfx
//   (:1024,:1037,:2351) are playfx on level._effect entries that no zm effect
//   table on this server defines, and are left out.
//   treasure_chest_move / the teddy bear (:1443,:1489,:1673,:1700) -- the box
//   relocation needs the teddy bear model plus the fly-away, which is a
//   scripted move plus FX; the box stays where the map put it.
//   init_weapon_toggle / init_pay_turret / weapon cabinet / hacker / fire sale
//   / dual-wield left-hand models / limited tesla -- separate systems.
//   useweaponhidetags() (:2026) -- MEASURED, not a registered builtin here;
//   the floating box model is a plain setModel of getWeaponModel's answer.
//   setclientflag(level._ZOMBIE_SCRIPTMOVER_FLAG_BOX_RANDOM) (:1972) -- there
//   is no CSC VM and no zm client-flag table on this server.
//
// THE WEAPON TABLE IS CoDOL's, NOT T5's.  T5's init_weapons (:222) names
// ak74u_zm / mp40_zm / ray_gun_zm and so on.  MEASURED 2026-09-20 from
// docs/zone_assets/ALL_weapon.txt: CoDOL's zm zones carry a DIFFERENT set --
// 163 *_zm names, in common_gametype_zm / patch_common_gametype_zm, with the
// same "<base>_zm" / "<base>_upgraded_zm" pairing convention T5 uses.  So the
// contract (add_zombie_weapon, the cost tiers by class, the upgrade suffix) is
// T5's and the names are the ones the client can actually build.  Every entry
// is then validated at init: precacheItem is a silent no-op for a weapon in no
// mounted zone (src/codol/script/codol_script_precache.c:24-27) and
// getWeaponIndexFromName only answers non-zero afterwards for a name the zones
// really carry, so a weapon that is not present drops out of the pool instead
// of producing a box roll the client cannot render.

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	level.zombie_weapons = [];

	init_weapons();
	init_weapon_upgrade();
	treasure_chest_init();

	add_sound( "open_chest", "zmb_box_open" );
	add_sound( "close_chest", "zmb_box_close" );
	add_sound( "weapon_show", "zmb_cha_ching" );
}

// T5 :28.  cost is the wall/box price, ammo_cost the refill price.
add_zombie_weapon( weapon_name, upgrade_name, hint, cost, ammo_cost, in_box )
{
	if ( !isDefined( in_box ) )
		in_box = true;

	precacheItem( weapon_name );
	if ( getWeaponIndexFromName( weapon_name ) <= 0 )
		return;

	if ( isDefined( upgrade_name ) )
	{
		precacheItem( upgrade_name );
		if ( getWeaponIndexFromName( upgrade_name ) <= 0 )
			upgrade_name = undefined;
	}

	s = spawnStruct();
	s.weapon_name = weapon_name;
	s.upgrade_name = upgrade_name;
	s.hint = hint;
	s.cost = cost;
	s.ammo_cost = ammo_cost;
	s.is_in_box = in_box;
	s.weighting_func = ::default_weighting_func;

	level.zombie_weapons[ weapon_name ] = s;
}

// T5 :78.
default_weighting_func()
{
	return 1;
}

// T5 :222 init_weapons, with CoDOL's weapon names (see the header) and T5's
// cost tiers: pistol 50-2200, smg 1000-1200, shotgun 500-2000, rifle 500-1200,
// assault 100-1200, sniper 50-2500, mg 50-4000, launcher 2000, wonder 10000.
init_weapons()
{
	// Pistols
	add_zombie_weapon( "m1911_zm",					"m1911_upgraded_zm",				"M1911",			50,		250 );
	add_zombie_weapon( "coltanaconda_zm",			"coltanaconda_upgraded_zm",			"Anaconda",			2200,	1000 );
	add_zombie_weapon( "glock_zm",					"glock_upgraded_zm",				"Glock",			50,		250 );
	add_zombie_weapon( "glockdw_zm",				"glockdw_upgraded_zm",				"Dual Glock",		50,		250 );

	// SMGs
	add_zombie_weapon( "mp5k_zm",					"mp5k_upgraded_zm",					"MP5K",				1000,	500 );
	add_zombie_weapon( "uzi_zm",					"uzi_upgraded_zm",					"Uzi",				1200,	600 );
	add_zombie_weapon( "p90_zm",					"p90_upgraded_zm",					"P90",				1200,	600 );
	add_zombie_weapon( "tmp_zm",					"tmp_upgraded_zm",					"TMP",				1000,	500 );
	add_zombie_weapon( "pp2000_zm",					"pp2000_upgraded_zm",				"PP2000",			1000,	500 );

	// Shotguns
	add_zombie_weapon( "spas12_zm",					"spas12_upgraded_zm",				"SPAS-12",			2000,	1000 );
	add_zombie_weapon( "striker_zm",				"striker_upgraded_zm",				"Striker",			1500,	750 );
	add_zombie_weapon( "model1887_zm",				"model1887_upgraded_zm",			"Model 1887",		500,	250 );
	add_zombie_weapon( "ranger_zm",					"ranger_upgraded_zm",				"Ranger",			500,	250 );

	// Rifles
	add_zombie_weapon( "m14ebr_zm",					"m14ebr_upgraded_zm",				"M14 EBR",			500,	250 );
	add_zombie_weapon( "m16_zm",					"gl_m16_upgraded_zm",				"M16",				1200,	600 );
	add_zombie_weapon( "famas_zm",					"famas_upgraded_zm",				"FAMAS",			1200,	600 );
	add_zombie_weapon( "fal_zm",					"fal_upgraded_zm",					"FAL",				1200,	600 );
	add_zombie_weapon( "ak104_zm",					"ak104_upgraded_zm",				"AK-104",			1200,	600 );
	add_zombie_weapon( "aug_eotech_zm",				"aug_eotech_upgraded_zm",			"AUG",				1200,	600 );
	add_zombie_weapon( "tavor_zm",					"tavor_upgraded_zm",				"Tavor",			1200,	600 );
	add_zombie_weapon( "m4a1tech_zm",				"m4a1tech_upgraded_zm",				"M4A1",				1200,	600 );
	add_zombie_weapon( "scar_h_eotech_zm",			"mk_scar_h_upgraded_zm",			"SCAR-H",			1200,	600 );

	// Sniper rifles
	add_zombie_weapon( "cheytac_zm",				"cheytac_upgraded_zm",				"CheyTac",			2500,	1250 );
	add_zombie_weapon( "as50_zm",					"as50_upgraded_zm",					"AS50",				2500,	1250 );
	add_zombie_weapon( "wa2000_zm",					"wa2000_upgraded_zm",				"WA2000",			2500,	1250 );
	add_zombie_weapon( "sniper_explosive_zm",		"sniper_explosive_upgraded_zm",		"Explosive Sniper",	2500,	1250 );

	// Machineguns
	add_zombie_weapon( "l86_zm",					"l86_upgraded_zm",					"L86 LSW",			4000,	2000 );

	// Launchers
	add_zombie_weapon( "m79_zm",					"m79_upgraded_zm",					"M79",				2000,	1000 );
	add_zombie_weapon( "at4_zm",					"at4_upgraded_zm",					"AT4",				2000,	1000 );

	// Melee / special
	add_zombie_weapon( "knife_ballistic_zm",		"knife_ballistic_upgraded_zm",			"Ballistic Knife",		3000,	1500 );
	add_zombie_weapon( "knife_ballistic_bowie_zm",	"knife_ballistic_bowie_upgraded_zm",	"Bowie Ballistic Knife",3000,	1500 );
	add_zombie_weapon( "crossbow_explosive_zm",		"crossbow_explosive_upgraded_zm",		"Explosive Crossbow",	1200,	600 );

	// Wonder weapons
	add_zombie_weapon( "ray_gun_zm",				"ray_gun_upgraded_zm",				"Ray Gun",			10000,	4500 );
	add_zombie_weapon( "tesla_gun_zm",				"tesla_gun_upgraded_zm",			"Wunderwaffe",		10000,	4500 );
	add_zombie_weapon( "thundergun_zm",				"thundergun_upgraded_zm",			"Thundergun",		10000,	4500 );

	// Equipment -- wall buy only, never a box roll (T5 passes in_box false for
	// the same reason: a box that can hand you a claymore is a dead pull).
	add_zombie_weapon( "frag_grenade_zm",			undefined,	"Frag Grenade",		250,	250,	false );
	add_zombie_weapon( "semtex_grenade_zm",			undefined,	"Semtex",			250,	250,	false );
	add_zombie_weapon( "claymore_zm",				undefined,	"Claymore",			1000,	1000,	false );
	add_zombie_weapon( "bouncingbetty_zm",			undefined,	"Bouncing Betty",	1000,	1000,	false );
	add_zombie_weapon( "c4_zm",						undefined,	"C4",				1000,	1000,	false );

	keys = getArrayKeys( level.zombie_weapons );
	println( "CoDOL zm: " + keys.size + " zombie weapons resolved out of the mounted zones\n" );
}

// T5 :177,:188,:677-:708.
is_weapon_included( weapon_name )
{
	return isDefined( level.zombie_weapons[ weapon_name ] );
}

get_weapon_hint( weapon_name )
{
	if ( !is_weapon_included( weapon_name ) )
		return weapon_name;
	return level.zombie_weapons[ weapon_name ].hint;
}

get_weapon_cost( weapon_name )
{
	if ( !is_weapon_included( weapon_name ) )
		return 1000;
	return level.zombie_weapons[ weapon_name ].cost;
}

get_ammo_cost( weapon_name )
{
	if ( !is_weapon_included( weapon_name ) )
		return 500;
	return level.zombie_weapons[ weapon_name ].ammo_cost;
}

get_is_in_box( weapon_name )
{
	if ( !is_weapon_included( weapon_name ) )
		return false;
	return level.zombie_weapons[ weapon_name ].is_in_box;
}

get_upgrade_name( weapon_name )
{
	if ( !is_weapon_included( weapon_name ) )
		return undefined;
	return level.zombie_weapons[ weapon_name ].upgrade_name;
}

// T5 :708.  An upgraded weapon is the one named as some base weapon's upgrade.
is_weapon_upgraded( weapon_name )
{
	keys = getArrayKeys( level.zombie_weapons );
	for ( i = 0; i < keys.size; i++ )
	{
		up = level.zombie_weapons[ keys[ i ] ].upgrade_name;
		if ( isDefined( up ) && up == weapon_name )
			return true;
	}
	return false;
}

// T5 :734,:755.  self is a player.
has_upgrade( weapon_name )
{
	up = get_upgrade_name( weapon_name );
	if ( !isDefined( up ) )
		return false;
	return self hasWeapon( up );
}

has_weapon_or_upgrade( weapon_name )
{
	if ( self hasWeapon( weapon_name ) )
		return true;
	return self has_upgrade( weapon_name );
}

// T5 :2365, plus the ammo top-up branch of :2649.
weapon_give( weapon_name )
{
	self giveWeapon( weapon_name );
	self giveMaxAmmo( weapon_name );
	self switchToWeapon( weapon_name );
}

ammo_give( weapon_name )
{
	if ( !self hasWeapon( weapon_name ) )
		return false;

	if ( self getFractionMaxAmmo( weapon_name ) >= 1 )
		return false;

	self giveMaxAmmo( weapon_name );
	return true;
}

// ==========================================================================
// WALL BUYS -- T5 :356 init_weapon_upgrade, :2649 weapon_spawn_think.
// ==========================================================================

init_weapon_upgrade()
{
	weapon_spawns = getEntArray( "weapon_upgrade", "targetname" );

	for ( i = 0; i < weapon_spawns.size; i++ )
	{
		trig = weapon_spawns[ i ];

		if ( !isDefined( trig.zombie_weapon_upgrade ) )
			continue;

		if ( !is_weapon_included( trig.zombie_weapon_upgrade ) )
		{
			// The wall names a weapon that is in no mounted zone.  T5 would
			// AssertEx; here the wall is left inert rather than ending the
			// match, and it is reported by name.
			println( "CoDOL zm: wall buy names unknown weapon " + trig.zombie_weapon_upgrade + "\n" );
			continue;
		}

		trig thread weapon_spawn_think();
	}

	println( "CoDOL zm: " + weapon_spawns.size + " wall buys\n" );
}

// T5 :2649, solo purse, minus the hacker/fire-sale/collector branches.
weapon_spawn_think()
{
	weapon_name = self.zombie_weapon_upgrade;
	cost = get_weapon_cost( weapon_name );
	ammo_cost = get_ammo_cost( weapon_name );

	self setCursorHint( "HINT_NOICON" );
	self setHintString( "Hold ^3[{+activate}]^7 to Buy " + get_weapon_hint( weapon_name ) + " [Cost: " + cost + "]" );

	self.first_time_triggered = false;

	for ( ;; )
	{
		self waittill( "trigger", player );

		if ( !isDefined( player ) || !isPlayer( player ) )
			continue;

		if ( !is_player_valid( player ) )
		{
			wait( 0.1 );
			continue;
		}

		if ( !player useButtonPressed() )
			continue;

		if ( player in_revive_trigger() )
			continue;

		if ( !player has_weapon_or_upgrade( weapon_name ) )
		{
			if ( player.score < cost )
			{
				self play_sound_on_ent( "no_purchase" );
				wait( 0.5 );
				continue;
			}

			if ( self.first_time_triggered == false )
			{
				model = getEnt( self.target, "targetname" );
				if ( isDefined( model ) )
					model thread weapon_show();
				self.first_time_triggered = true;
				self setHintString( "Hold ^3[{+activate}]^7 to Buy " + get_weapon_hint( weapon_name ) +
					" Ammo [Cost: " + ammo_cost + "]" );
			}

			player maps\mp\gametypes\_zombiemode_score::minus_to_player_score( cost );
			player weapon_give( weapon_name );
			player play_sound_on_ent( "purchase" );
			wait( 0.5 );
			continue;
		}

		// Owns it: sell ammo.  T5's rule -- an upgraded weapon's refill is
		// 4500, the base weapon's is its own ammo_cost.
		refill_cost = ammo_cost;
		refill_name = weapon_name;
		if ( player has_upgrade( weapon_name ) )
		{
			refill_cost = 4500;
			refill_name = get_upgrade_name( weapon_name );
		}

		if ( player.score < refill_cost )
		{
			self play_sound_on_ent( "no_purchase" );
			wait( 0.5 );
			continue;
		}

		if ( player ammo_give( refill_name ) )
		{
			player maps\mp\gametypes\_zombiemode_score::minus_to_player_score( refill_cost );
			player play_sound_on_ent( "purchase" );
		}

		wait( 0.5 );
	}
}

// T5 :2798.  The wall weapon model appears once, the first time the wall is
// bought.  T5 also rotates it to face the buyer; the map already authored the
// angles, so only the reveal is kept.
weapon_show()
{
	self show();
	self play_sound_on_ent( "weapon_show" );
}

// ==========================================================================
// MYSTERY BOX -- T5 :793, :945, :1057, :1742-:1784, :1951, :2365.
// ==========================================================================

// T5 :793.
treasure_chest_init()
{
	flag_init( "chest_has_been_used" );

	level.chests = getEntArray( "treasure_chest_use", "targetname" );

	if ( level.chests.size < 1 )
	{
		println( "CoDOL zm: no treasure_chest_use on this map; no mystery box\n" );
		return;
	}

	for ( i = 0; i < level.chests.size; i++ )
	{
		level.chests[ i ] get_chest_pieces();

		if ( !isDefined( level.chests[ i ].zombie_cost ) )
			level.chests[ i ].zombie_cost = 950;
	}

	// T5 randomizes which of several chests is live and moves it with the
	// teddy bear.  The relocation is not ported (see the header), so every
	// authored chest is live -- which is what a single-chest map does anyway,
	// and is the only honest behaviour without the move.
	level.chest_index = 0;
	level.chest_accessed = 0;

	array_thread_1( level.chests, ::treasure_chest_think );

	println( "CoDOL zm: " + level.chests.size + " mystery box location(s)\n" );
}

// T5 :945.  use trigger -> chest_lid -> chest_origin -> chest_box.
get_chest_pieces()
{
	if ( isDefined( self.target ) )
		self.chest_lid = getEnt( self.target, "targetname" );

	if ( isDefined( self.chest_lid ) && isDefined( self.chest_lid.target ) )
		self.chest_origin = getEnt( self.chest_lid.target, "targetname" );

	if ( isDefined( self.chest_origin ) && isDefined( self.chest_origin.target ) )
		self.chest_box = getEnt( self.chest_origin.target, "targetname" );
}

// T5 :1752,:1763.  RotateRoll, not an animation.
treasure_chest_lid_open()
{
	self rotateRoll( 105, 0.5, 0.25 );
	play_sound_at_pos( "open_chest", self.origin );
}

treasure_chest_lid_close()
{
	self rotateRoll( -105, 0.5, 0.25 );
	play_sound_at_pos( "close_chest", self.origin );
}

// T5 :1742.
treasure_chest_timeout()
{
	self endon( "user_grabbed_weapon" );
	wait( 12 );
	self notify( "trigger", level );
}

// T5 :1774,:1784.
treasure_chest_ChooseRandomWeapon()
{
	keys = getArrayKeys( level.zombie_weapons );
	return keys[ randomInt( keys.size ) ];
}

treasure_chest_ChooseWeightedRandomWeapon( player )
{
	keys = getArrayKeys( level.zombie_weapons );

	filtered = [];
	for ( i = 0; i < keys.size; i++ )
	{
		if ( !get_is_in_box( keys[ i ] ) )
			continue;

		if ( isDefined( player ) && is_player_valid( player ) && player has_weapon_or_upgrade( keys[ i ] ) )
			continue;

		filtered[ filtered.size ] = keys[ i ];
	}

	// T5 falls back to the unfiltered set when the player owns everything the
	// box can give.
	if ( filtered.size < 1 )
	{
		for ( i = 0; i < keys.size; i++ )
		{
			if ( get_is_in_box( keys[ i ] ) )
				filtered[ filtered.size ] = keys[ i ];
		}
	}

	if ( filtered.size < 1 )
		return undefined;

	return filtered[ randomInt( filtered.size ) ];
}

// T5 :1057.  Pay, roll, grab or time out, close.  self is the use trigger.
treasure_chest_think()
{
	self endon( "kill_chest_think" );

	self setCursorHint( "HINT_NOICON" );
	self setHintString( "Hold ^3[{+activate}]^7 for Mystery Box [Cost: " + self.zombie_cost + "]" );

	for ( ;; )
	{
		user = undefined;

		for ( ;; )
		{
			self waittill( "trigger", who );

			if ( !isDefined( who ) || !isPlayer( who ) )
				continue;

			if ( !is_player_valid( who ) )
			{
				wait( 0.1 );
				continue;
			}

			if ( !who useButtonPressed() )
				continue;

			if ( who in_revive_trigger() )
			{
				wait( 0.1 );
				continue;
			}

			if ( who getCurrentWeapon() == "none" )
			{
				wait( 0.1 );
				continue;
			}

			if ( who.score < self.zombie_cost )
			{
				self play_sound_on_ent( "no_purchase" );
				wait( 0.5 );
				continue;
			}

			who maps\mp\gametypes\_zombiemode_score::minus_to_player_score( self.zombie_cost );
			user = who;
			break;
		}

		flag_set( "chest_has_been_used" );

		if ( isDefined( self.chest_lid ) )
			self.chest_lid thread treasure_chest_lid_open();

		self disable_trigger();

		weapon = self treasure_chest_weapon_spawn( user );

		if ( !isDefined( weapon ) )
		{
			// Nothing to give.  Refund, exactly as T5 refunds a teddy-bear
			// pull (:1188 add_to_player_score( user_cost, false )): the spend
			// comes back without crediting the lifetime total.
			user maps\mp\gametypes\_zombiemode_score::add_to_player_score( self.zombie_cost, false );
			if ( isDefined( self.chest_lid ) )
				self.chest_lid thread treasure_chest_lid_close();
			self enable_trigger();
			continue;
		}

		self.grabbed_weapon = weapon;
		self setHintString( "Hold ^3[{+activate}]^7 to Swap for " + get_weapon_hint( weapon ) );
		self enable_trigger();
		self thread treasure_chest_timeout();

		for ( ;; )
		{
			self waittill( "trigger", grabber );

			if ( grabber == level )
			{
				// timed out
				break;
			}

			if ( !isDefined( grabber ) || grabber != user )
				continue;

			if ( !is_player_valid( user ) )
				break;

			if ( !user useButtonPressed() )
				continue;

			self notify( "user_grabbed_weapon" );
			user treasure_chest_give_weapon( weapon );
			level.chest_accessed++;
			break;
		}

		self disable_trigger();

		if ( isDefined( self.weapon_model ) )
		{
			self.weapon_model delete();
			self.weapon_model = undefined;
		}

		if ( isDefined( self.chest_lid ) )
			self.chest_lid thread treasure_chest_lid_close();

		self setHintString( "Hold ^3[{+activate}]^7 for Mystery Box [Cost: " + self.zombie_cost + "]" );

		wait( 3 );
		self enable_trigger();
	}
}

// T5 :1951.  The 40-cycle roll, slowing down as it goes, then the weapon
// floating above the box.  Returns the weapon it landed on.
treasure_chest_weapon_spawn( player )
{
	if ( !isDefined( self.chest_origin ) )
		return treasure_chest_ChooseWeightedRandomWeapon( player );

	origin = self.chest_origin.origin;
	angles = self.chest_origin.angles;
	if ( !isDefined( angles ) )
		angles = ( 0, 0, 0 );

	if ( !isDefined( self.weapon_model ) )
	{
		self.weapon_model = spawn( "script_model", origin + ( 0, 0, 40 ) );
		self.weapon_model.angles = angles + ( 0, 90, 0 );
	}

	rand = undefined;
	number_cycles = 40;

	for ( i = 0; i < number_cycles; i++ )
	{
		if ( i < 20 )
			wait( 0.05 );
		else if ( i < 30 )
			wait( 0.1 );
		else if ( i < 35 )
			wait( 0.2 );
		else if ( i < 38 )
			wait( 0.3 );
		else
			wait( 0.05 );

		if ( i + 1 < number_cycles )
			rand = treasure_chest_ChooseRandomWeapon();
		else
			rand = treasure_chest_ChooseWeightedRandomWeapon( player );

		if ( !isDefined( rand ) )
			continue;

		modelname = getWeaponModel( rand );
		if ( isDefined( modelname ) && modelname != "" )
			self.weapon_model setModel( modelname );
	}

	return rand;
}

// T5 :2365.  The box replaces the held primary rather than stacking.
treasure_chest_give_weapon( weapon_name )
{
	current = self getCurrentWeapon();

	if ( isDefined( current ) && current != "none" && current != weapon_name )
	{
		// Never take the starting pistol away: T5 keeps it as the last-stand
		// weapon (level.laststandpistol, _zombiemode.gsc:653).
		if ( !isDefined( level.zombie_start_weapon ) || current != level.zombie_start_weapon )
			self takeWeapon( current );
	}

	self giveWeapon( weapon_name );
	self giveMaxAmmo( weapon_name );
	self switchToWeapon( weapon_name );
}
