// survival: the AI types, the wave table and the enemies.
//
// Ported from Infinity Ward's Modern Warfare 3 Spec Ops Survival,
// maps\_so_survival_ai.gsc (IW5; D:\CoD\COD-GSC-Source\IW5-GSC\scripts\1564.gsc
// in the decompiled dump), as CoDOL's own survival builds carry it: the tables
// are 2.6.9.7's common_gametype_survival (mp/survival_waves.csv, IW5's layout)
// and the map's mp/survival_waves_<map>.csv.  docs/CODOL_SURVIVAL_PLAN.md
// section 2.
//
// Survival plays on the multiplayer chain (maps\mp\gametypes\survival.gsc):
// the soldiers are the survival species' actors (aitype\enemy_so_*, the shared
// soldier contract) firing the multiplayer weapon the AI table names; the dogs
// are the multiplayer dog (maps\mp\_dogs, maps\mp\animscripts\dog); the actor
// damage and death go through the MP dog's callbacks, wrapped here for IW5's
// juggernaut damage factors.
//
// Tables.
//   AI types (level.survival_type_table, rows 100..199, column 0 the index):
//     1 ref  2 name  3 desc  4 classname  5 weapon (base|attachment|...)
//     6 alt weapon (grenade or boss secondary)  7 health  8 speed  9 xp (the credits a kill
//     pays)  10 boss  11 accuracy
//   Waves (level.survival_wave_table, IW5 get_* accessors, searched by column 2):
//     0 index  1 boss delay  2 wave  3 squad type  4 squad size  5 specials
//     6 special counts  7 AI bosses  8 non-AI bosses  9 repeating

#using_animtree ( "generic_human" );

// --------------------------------------------------------------------------
// Tables
// --------------------------------------------------------------------------

init()
{
	level.survival_ai = [];
	level.survival_ai_refs = [];
	if ( !isDefined( level.coop_weapon_models ) )
		level.coop_weapon_models = [];
	for ( i = 100; i < 200; i++ )
	{
		ref = tableLookup( level.survival_type_table, 0, "" + i, 1 );
		if ( !isDefined( ref ) || ref == "" )
			continue;
		t = spawnStruct();
		t.index = i;
		t.ref = ref;
		t.classname = tableLookup( level.survival_type_table, 0, "" + i, 4 );
		t.weapon_spec = tableLookup( level.survival_type_table, 0, "" + i, 5 );
		t.altweapon = tableLookup( level.survival_type_table, 0, "" + i, 6 );
		t.health = table_int( level.survival_type_table, i, 7, 150 );
		t.speed = table_float( level.survival_type_table, i, 8, 1 );
		t.xp = table_int( level.survival_type_table, i, 9, 100 );
		t.boss = ( tableLookup( level.survival_type_table, 0, "" + i, 10 ) == "1" );
		t.accuracy = table_float( level.survival_type_table, i, 11, 1 );
		t.weapon = "";
		t.grenade = "";
		level.survival_ai[ ref ] = t;
		level.survival_ai_refs[ level.survival_ai_refs.size ] = ref;
	}
	codolprintln( "CoDOL survival: " + level.survival_ai_refs.size + " AI types from " + level.survival_type_table );

	level.survival_waves = [];
	level.survival_repeat_waves = [];
	for ( i = 0; i <= 60; i++ )
	{
		num = int( tableLookup( level.survival_wave_table, 0, "" + i, 2 ) );
		if ( num <= 0 )
			continue;
		w = read_wave( num );
		level.survival_waves[ num ] = w;
		if ( w.repeating )
			level.survival_repeat_waves[ level.survival_repeat_waves.size ] = w;
	}
	level.survival_last_wave = 0;
	for ( i = 1; i <= 60; i++ )
	{
		if ( isDefined( level.survival_waves[ i ] ) )
			level.survival_last_wave = i;
	}
	codolprintln( "CoDOL survival: " + level.survival_last_wave + " waves from " + level.survival_wave_table +
		", " + level.survival_repeat_waves.size + " repeat after them" );

	// Only the types this level's waves name get their weapons composed and
	// precached (the server preloads only their aitypes too).
	used = [];
	for ( i = 1; i <= level.survival_last_wave; i++ )
	{
		w = level.survival_waves[ i ];
		if ( !isDefined( w ) )
			continue;
		used[ w.squad_type ] = true;
		for ( j = 0; j < w.specials.size; j++ )
			used[ w.specials[ j ] ] = true;
		for ( j = 0; j < w.bosses_ai.size; j++ )
			used[ w.bosses_ai[ j ] ] = true;
		for ( j = 0; j < w.bosses_nonai.size; j++ )
			used[ w.bosses_nonai[ j ] ] = true;
	}
	n = 0;
	for ( i = 0; i < level.survival_ai_refs.size; i++ )
	{
		t = level.survival_ai[ level.survival_ai_refs[ i ] ];
		if ( !isDefined( used[ t.ref ] ) )
			continue;
		// The type's multiplayer weapon, composed with its attachments.
		t.weapon = compose_weapon( t.weapon_spec );
		t.grenade = type_grenade( t.altweapon );
		n++;
	}
	codolprintln( "CoDOL survival: " + n + " AI types used by this level's waves" );

	level.survival_enemies = [];
	level.survival_bosses = [];
	loot_init();
	chase_init();

	// IW5 ai_preload: the martyrdom C4, the claymores, the gas.
	codescripts\character::precacheMounted( "weapon_c4" );
	codescripts\character::precacheMounted( "weapon_claymore" );
	level.survival_shield_model = "weapon_riot_shield_mp";
	codescripts\character::precacheMounted( level.survival_shield_model );
	precacheShellShock( "concussion_grenade_mp" );
	level._effect[ "martyrdom_c4_explosion" ] = loadFx( "explosions/grenadeexp_default" );
	// IW5 chemical tank smoke is not a C1 asset. Use C1's real MP tabun
	// effect and shock, already shipped by common_gametype_mp/code_post_gfx.
	level._effect[ "chemical_cloud" ] = loadFx( "weapon/grenade/fx_gas_poison_mp" );
	precacheShellShock( "tabun_gas_mp" );
}

table_int( table, row, column, fallback )
{
	v = tableLookup( table, 0, "" + row, column );
	if ( !isDefined( v ) || v == "" )
		return fallback;
	return int( v );
}

// The tables write some fractions without the leading zero (speed ".8" for
// the demolitionist, ".9" for noob and searcher), which float() reads as 0:
// a moveplaybackrate of 0 froze those soldiers in their run cycle for good.
table_float( table, row, column, fallback )
{
	v = tableLookup( table, 0, "" + row, column );
	if ( !isDefined( v ) || v == "" )
		return fallback;
	if ( v[ 0 ] == "." )
		v = "0" + v;
	else if ( v.size > 1 && v[ 0 ] == "-" && v[ 1 ] == "." )
		v = "-0" + getSubStr( v, 1, v.size );
	return float( v );
}

// Column 6 also contains juggernaut RPG secondaries. Only an actual native
// grenade definition may enter codol_actorThrowGrenade's offhand path.
type_grenade( weapon )
{
	if ( !isDefined( weapon ) || weapon == "" || !codol_isWeaponMounted( weapon ) )
		return "";
	precacheItem( weapon );
	if ( weaponType( weapon ) != "grenade" )
		return "";
	return weapon;
}

// --------------------------------------------------------------------------
// Multiplayer weapons from the tables
// --------------------------------------------------------------------------

// "base|attachment|..." (the later CoDOL builds' notation) -> the composed
// multiplayer weapon (codol_getPartyWeapon, the party modes' composer), its
// world model precached for the soldier's hand.  Attachments the 1.1.1.13
// marketplace lacks are read without their weapon suffix
// (mag_extclipmp5k -> mag_extclip), else dropped.  "" when the base is not
// in the mounted zones.
compose_weapon( spec )
{
	if ( !isDefined( spec ) || spec == "" )
		return "";
	parts = strTok( spec, "|" );
	base = parts[ 0 ];
	if ( !codol_isWeaponMounted( base ) )
	{
		codolprintln( "CoDOL survival: weapon " + base + " is in no mounted zone" );
		return "";
	}
	precacheItem( base );
	short = base;
	if ( isSubStr( short, "_mp" ) )
		short = getSubStr( short, 0, short.size - 3 );
	a = [];
	for ( i = 1; i < parts.size; i++ )
	{
		name = party_attachment( parts[ i ], short );
		if ( isDefined( name ) )
			a[ a.size ] = name;
	}
	switch ( a.size )
	{
	case 0:
		weapon = codol_getPartyWeapon( base );
		break;
	case 1:
		weapon = codol_getPartyWeapon( base, a[ 0 ] );
		break;
	case 2:
		weapon = codol_getPartyWeapon( base, a[ 0 ], a[ 1 ] );
		break;
	case 3:
		weapon = codol_getPartyWeapon( base, a[ 0 ], a[ 1 ], a[ 2 ] );
		break;
	case 4:
		weapon = codol_getPartyWeapon( base, a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ] );
		break;
	default:
		weapon = codol_getPartyWeapon( base, a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ] );
		break;
	}
	precacheItem( weapon );
	model = getWeaponModel( weapon );
	if ( isDefined( model ) && model != "" && !isDefined( level.coop_weapon_models[ model ] ) )
	{
		precacheModel( model );
		level.coop_weapon_models[ model ] = true;
	}
	return weapon;
}

party_attachment( name, short )
{
	if ( codol_isPartyAttachment( name ) )
		return name;
	if ( name.size > short.size && getSubStr( name, name.size - short.size, name.size ) == short )
	{
		stem = getSubStr( name, 0, name.size - short.size );
		if ( codol_isPartyAttachment( stem ) )
			return stem;
	}
	switch ( getSubStr( name, 0, 4 ) )
	{
	case "mag_":
		if ( isSubStr( name, "extclip" ) && codol_isPartyAttachment( "xmags" ) )
			return "xmags";
		break;
	case "eot_":
	case "red_":
		if ( codol_isPartyAttachment( "red_normal" ) )
			return "red_normal";
		break;
	}
	return undefined;
}

// IW5 wave_populate: one struct per wave row.
read_wave( num )
{
	t = level.survival_wave_table;
	key = "" + num;
	w = spawnStruct();
	w.num = num;
	w.boss_delay = int( tableLookup( t, 2, key, 1 ) );
	w.squad_type = tableLookup( t, 2, key, 3 );
	w.squad_size = int( tableLookup( t, 2, key, 4 ) );
	w.specials = tokens( tableLookup( t, 2, key, 5 ) );
	counts = tokens( tableLookup( t, 2, key, 6 ) );
	w.special_counts = [];
	for ( i = 0; i < counts.size; i++ )
		w.special_counts[ i ] = int( counts[ i ] );
	w.bosses_ai = tokens( tableLookup( t, 2, key, 7 ) );
	w.bosses_nonai = tokens( tableLookup( t, 2, key, 8 ) );
	w.repeating = ( tableLookup( t, 2, key, 9 ) == "1" );
	w.scale = 1.0;
	return w;
}

tokens( text )
{
	if ( !isDefined( text ) || text == "" )
		return [];
	return strTok( text, " " );
}

// IW5 update_wave: past the table the repeating rows cycle, harder each pass
// (IW5 grows health, speed and accuracy through independent repeat factors).
get_wave( num )
{
	if ( isDefined( level.survival_waves[ num ] ) )
		return level.survival_waves[ num ];
	if ( !level.survival_repeat_waves.size )
	{
		if ( level.survival_last_wave <= 0 )
			return undefined;
		base = level.survival_waves[ level.survival_last_wave ];
		pass = num - level.survival_last_wave;
	}
	else
	{
		past = num - level.survival_last_wave - 1;
		base = level.survival_repeat_waves[ past % level.survival_repeat_waves.size ];
		pass = num - level.survival_last_wave;
	}
	w = spawnStruct();
	w.num = num;
	w.boss_delay = base.boss_delay;
	w.squad_type = base.squad_type;
	w.squad_size = base.squad_size;
	w.specials = base.specials;
	w.special_counts = base.special_counts;
	w.bosses_ai = base.bosses_ai;
	w.bosses_nonai = base.bosses_nonai;
	w.repeating = true;
	// IW5 update_wave increments survival_waves_repeated on EACH generated
	// wave, not once per cycle: health +10%, speed +5%, accuracy +20%.
	w.scale = 1.0 + 0.1 * pass;
	level.survival_waves[ num ] = w;
	return w;
}

// IW5 get_squad_array: the wave's squad size in squads of three.
squad_array( size )
{
	a = [];
	if ( size <= 0 )
		return a;
	if ( size <= 3 )
	{
		a[ 0 ] = size;
		return a;
	}
	rest = size % 3;
	n = int( size / 3 );
	for ( i = 0; i < n; i++ )
		a[ i ] = 3;
	if ( rest == 1 )
	{
		a[ a.size - 1 ] = a[ a.size - 1 ] - 1;
		a[ a.size ] = 2;
	}
	else if ( rest == 2 )
		a[ a.size ] = 2;
	return a;
}

// --------------------------------------------------------------------------
// Spawning
// --------------------------------------------------------------------------

type_attributes( t, scale )
{
	if ( !isDefined( scale ) )
		scale = 1.0;
	repeated = ( scale - 1.0 ) * 10;
	a = spawnStruct();
	a.health = int( t.health * scale );
	a.speed = t.speed * ( 1.0 + 0.05 * repeated );
	if ( a.speed > 1.5 )
		a.speed = 1.5;
	else if ( a.speed < 0.5 )
		a.speed = 0.5;	// never a frozen (rate 0) or crawling cycle
	// Keep this MP soldier contract's base accuracy and safe ceiling; use
	// IW5's independent repeated-wave multiplier instead of health's scale.
	a.accuracy = t.accuracy * 0.3 * ( 1.0 + 0.2 * repeated );
	if ( a.accuracy > 0.6 )
		a.accuracy = 0.6;
	return a;
}

// A soldier of AI type `ref` at `origin` (IW5 spawn_ai on the type's
// classname spawner).  Returns the actor or undefined.
spawn_type( ref, origin, angles, scale )
{
	t = level.survival_ai[ ref ];
	if ( !isDefined( t ) )
	{
		codolprintln( "CoDOL survival: no AI type '" + ref + "'" );
		return undefined;
	}
	if ( is_dog_type( ref ) )
		return spawn_dog( ref, origin, angles, scale );
	if ( !isDefined( t.classname ) || t.classname == "" )
		return undefined;
	if ( !isDefined( scale ) )
		scale = 1.0;

	kind = spawnStruct();
	attributes = type_attributes( t, scale );
	kind.accuracy = attributes.accuracy;
	kind.health = attributes.health;
	if ( t.weapon != "" )
	{
		kind.weapon = t.weapon;
		// Authored later-build aitypes used generic SMG/AR roles. Keep their
		// actual launcher primary instead of inheriting automatic-gun pacing.
		if ( weaponType( t.weapon ) == "projectile" )
		{
			kind.role = "grenadelauncher";
			if ( weaponClass( t.weapon ) == "rocketlauncher" )
				kind.role = "rpg";
		}
	}
	ai = maps\mp\gametypes\_survival_utility::spawn_soldier( t.classname, origin, angles, kind );
	if ( !isDefined( ai ) )
		return undefined;
	ai maps\mp\gametypes\_survival_utility::arm_soldier();
	ai maps\mp\gametypes\_survival_utility::first_pose();

	ai.survival_type = t;
	ai.survival_ref = ref;
	ai.health = attributes.health;
	ai.maxhealth = ai.health;
	ai.accuracy = attributes.accuracy;
	// Empty means no grenades, including early shotgun/pistol infantry.
	// The shared soldier throw anim releases this actual native grenade.
	ai.survival_grenade = t.grenade;
	if ( t.grenade == "tabun_gas_mp" )
		ai.survival_grenade_on_explode = ::chemical_grenade;
	ai.coop_grenades = 0;
	if ( t.grenade != "" )
		ai.coop_grenades = 2;
	// IW5 moveplaybackrate from the speed column.
	ai.moveplaybackrate = attributes.speed;
	ai.ignoreall = false;
	// Survival squads pursue the players instead of settling into the MP
	// map's static cover network. Keep real native combat at weapon range.
	ai.coop_no_cover = true;
	ai codol_actorUseCover( false );
	// A hunter (soldier_move: the run, with IW4's sprints for variation),
	// never the bored patrol walk soldier_move gives a soldier with no enemy
	// and no hunt -- ~30 units a second, the "idling" stroll across the map.
	ai.coop_hunting = true;
	ai setThreatBiasGroup( "axis" );

	level.survival_enemies[ level.survival_enemies.size ] = ai;
	ai thread death_watch();
	ai thread chase_watch();
	ai special_behaviour( ref );
	if ( !t.boss )
		ai loot_roll( undefined );
	return ai;
}

// --------------------------------------------------------------------------
// Loot (IW5 maps\_so_survival_loot, scripts/1562.gsc): a soldier may carry a
// weapon on his back that he drops when he dies.  mp/survival_loot.csv:
//   0 index  1 ref  2 type  3 name  4 desc  5 chance  6 wave unlock
//   7 wave lock  8 rank
// IW5 rolls each spawn against the unlocked rows, at most one drop of a ref
// every two waves.  (CoDOL's table sets every chance to 0.)
// --------------------------------------------------------------------------

loot_init()
{
	level.survival_loot = [];
	t = "mp/survival_loot.csv";
	for ( i = 0; i <= 20; i++ )
	{
		ref = tableLookup( t, 0, "" + i, 1 );
		if ( !isDefined( ref ) || ref == "" || tableLookup( t, 0, "" + i, 2 ) != "weapon" )
			continue;
		if ( !codol_isWeaponMounted( ref ) )
			continue;
		precacheItem( ref );
		l = spawnStruct();
		l.ref = ref;
		l.chance = float( tableLookup( t, 0, "" + i, 5 ) );
		l.wave_unlock = int( tableLookup( t, 0, "" + i, 6 ) );
		l.wave_lock = int( tableLookup( t, 0, "" + i, 7 ) );
		l.wave_dropped = -999;
		level.survival_loot[ level.survival_loot.size ] = l;
	}
}

loot_roll( chance )
{
	wave = level.survival_wave_num;
	for ( i = 0; i < level.survival_loot.size; i++ )
	{
		l = level.survival_loot[ i ];
		if ( wave < l.wave_unlock || wave >= l.wave_lock || wave - l.wave_dropped < 2 )
			continue;
		c = l.chance;
		if ( isDefined( chance ) )
			c = chance;
		if ( c <= randomFloatRange( 0, 1 ) )
			continue;
		l.wave_dropped = wave;
		self thread loot_drop_on_death( l.ref );
		return true;
	}
	return false;
}

// IW5 loot_drop_on_death: the weapon rides on his back and falls where he
// dies. Keep the authored bonus clip; all enemy weapon drops have zero
// reserve (user rule), including this separately carried bonus weapon.
loot_drop_on_death( weapon )
{
	model = getWeaponModel( weapon );
	carried = undefined;
	if ( isDefined( model ) && model != "" && isDefined( level.coop_weapon_models[ model ] ) )
	{
		carried = spawn( "script_model", self getTagOrigin( "tag_stowed_back" ) );
		carried setModel( model );
		carried linkTo( self, "tag_stowed_back", ( 0, 0, 0 ), ( 0, 0, 0 ) );
	}
	self waittill( "death" );
	origin = self.origin + ( 0, 0, 24 );
	if ( isDefined( carried ) )
	{
		origin = carried.origin;
		carried delete();
	}
	item = spawn( "weapon_" + weapon, origin );
	clip = int( loot_max( 1, 0.4 * weaponClipSize( weapon ) ) );
	item itemWeaponSetAmmo( clip, 0 );
}

loot_max( a, b )
{
	if ( a > b )
		return a;
	return b;
}

// IW5 default_ai's attribute dispatch: the special enemies' abilities.
special_behaviour( ref )
{
	switch ( ref )
	{
	case "martyrdom":
		self thread martyrdom_ability( 1.5 );
		break;
	case "martyrdom_volatile":
		self thread martyrdom_ability( 0.05 );
		break;
	case "chemical":
	case "hazmat":
		self thread chemical_ability();
		break;
	case "claymore":
		self thread claymore_planter();
		break;
	case "riotshield":
		self carry_shield();
		break;
	}
	self engage_range( ref );
}

// How close a type comes before it stands and fights (the native attack
// distance) and how close the advance takes it after that.  The walking bombs
// carry their C4 to the player, firing as they run (the role's shotgun range had
// them stop 600 units out and hold at 220, beyond its blast) and the gas tank is worth
// something only near him (IW5 goal radius 512); the rest keep their weapon's.
engage_range( ref )
{
	switch ( ref )
	{
	case "martyrdom":
	case "martyrdom_volatile":
		self.meleeattackdist = 180;
		self.survival_engage_floor = 90;
		self.survival_rush = true;
		break;
	case "chemical":
	case "hazmat":
		self.meleeattackdist = 800;
		self.survival_engage_floor = 240;
		break;
	case "riotshield":
		self.meleeattackdist = 600;
		self.survival_engage_floor = 160;
		break;
	}
	self.survival_engage_base = self.meleeattackdist;
}

// IW5 martyrdom_ability / detonate_c4_when_dead: C4 strapped to the chest,
// and it goes off where he dies (IW5's radius 192; this port retains its
// 1.5 s warning fuse, with the volatile variant at once).
martyrdom_ability( fuse )
{
	if ( codescripts\character::modelMounted( "weapon_c4" ) )
		self attach( "weapon_c4", "j_spine4", true );
	self waittill( "death", attacker );
	origin = self.origin + ( 0, 0, 16 );
	// Capture the death position before native corpse cleanup. The fuse and
	// explosion belong to the level, not the entity that just died.
	level thread detonate_c4( origin, fuse );
}

detonate_c4( origin, fuse )
{
	// The charge lies on the ground where he fell; the blast stays at its own
	// height, where the ground does not shadow it.
	charge = spawn( "script_model", floor_under( origin ) );
	charge.survival_enemy = true;
	if ( codescripts\character::modelMounted( "weapon_c4" ) )
		charge setModel( "weapon_c4" );
	if ( fuse > 0.05 )
		playSoundAtPosition( "weap_c4detpack_trigger_plr", origin );
	wait( fuse );
	if ( level.survival_over )
	{
		charge delete();
		return;
	}
	if ( isDefined( level._effect[ "martyrdom_c4_explosion" ] ) )
		playFx( level._effect[ "martyrdom_c4_explosion" ], origin );
	playSoundAtPosition( "grenade_explode_metal", origin );
	// IW5 detonate_c4_when_dead: native radial damage, 192 units, 100/50.
	radiusDamage( origin, 192, 100, 50, charge );
	earthquake( 0.4, 0.6, origin, 600 );
	charge delete();
}

// IW5 chemical_ability / chemical_ability_gas_cloud: the gas tank bursts when
// he dies. Keep IW5's six-second lifetime with C1's real MP poison effect,
// damage/shock and a 160-unit exposure radius.
chemical_ability()
{
	self waittill( "death", attacker );
	origin = self.origin + ( 0, 0, 24 );
	level thread chemical_cloud( origin );
}

// The native actor throw returns the real missile. Like T5 _tabun, wait
// for explode without a death endon (explode and death occur together).
chemical_grenade()
{
	self waittill( "explode", origin );
	level thread chemical_cloud( origin );
}

chemical_cloud( origin )
{
	canister = spawn( "script_model", origin );
	canister.survival_enemy = true;
	fx = spawnFx( level._effect[ "chemical_cloud" ], origin );
	triggerFx( fx );
	for ( t = 0; t < 6 && !level.survival_over; t += 0.5 )
	{
		players = maps\mp\gametypes\_survival_utility::living_players();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( isDefined( p.laststand ) && p.laststand )
				continue;
			if ( distanceSquared( p.origin, origin ) > 160 * 160 )
				continue;
			if ( p hasAbilityModifier( "modGasMask" ) )
				continue;
			if ( !bulletTracePassed( origin, p.origin + ( 0, 0, 32 ), false, p ) )
				continue;
			p doDamage( 6, origin, canister, canister, 0, "MOD_GAS", 0, "tabun_gas_mp" );
			if ( !isDefined( p.survival_gassed ) || getTime() - p.survival_gassed > 2000 )
			{
				p.survival_gassed = getTime();
				p shellShock( "tabun_gas_mp", 1.5 );
			}
		}
		wait( 0.5 );
	}
	fx delete();
	canister delete();
}

// IW5 behavior_special_ai_claymore / mine_locs_attempt_plant: the planter
// heads for the claymore spot nearest the players and plants one there.
claymore_planter()
{
	self endon( "death" );
	if ( !isDefined( level.survival_claymore_spots ) || !level.survival_claymore_spots.size )
		return;
	planted = 0;
	while ( planted < 2 )
	{
		wait( 4 + randomFloat( 4 ) );
		target = closest_player( self.origin );
		if ( !isDefined( target ) )
			continue;
		spot = undefined;
		best = 0;
		for ( i = 0; i < level.survival_claymore_spots.size; i++ )
		{
			s = level.survival_claymore_spots[ i ];
			if ( isDefined( s.survival_claymore ) )
				continue;
			d = distanceSquared( s.origin, target.origin );
			if ( d < 300 * 300 )
				continue;
			if ( !isDefined( spot ) || d < best )
			{
				spot = s;
				best = d;
			}
		}
		if ( !isDefined( spot ) )
			continue;
		self.goalradius = 48;
		self setGoalPos( spot.origin );
		for ( t = 0; t < 20 && distanceSquared( self.origin, spot.origin ) > 72 * 72; t++ )
			wait( 0.5 );
		if ( distanceSquared( self.origin, spot.origin ) > 96 * 96 )
			continue;
		yaw = vectorToAngles( target.origin - spot.origin );
		spot.survival_claymore = spawn_claymore( self.origin, ( 0, yaw[ 1 ], 0 ) );
		spot.survival_claymore.survival_spot = spot;
		planted++;
		self thread hunt();
	}
}

spawn_claymore( origin, angles )
{
	c = spawn( "script_model", origin );
	c.angles = angles;
	if ( codescripts\character::modelMounted( "weapon_claymore" ) )
		c setModel( "weapon_claymore" );
	c.survival_enemy = true;
	if ( !isDefined( level.survival_claymores ) )
		level.survival_claymores = [];
	level.survival_claymores[ level.survival_claymores.size ] = c;
	c thread claymore_watch();
	return c;
}

// IW5 claymore_on_trigger: a player in front of it within 150 units, a beat,
// and it goes off (IW5 claymore_detonate: radius 192).  The survival EMP
// (maps\mp\gametypes\_survival_kiosk) disarms them.
claymore_watch()
{
	self endon( "survival_disarmed" );
	wait( 1 );
	forward = anglesToForward( self.angles );
	for ( ;; )
	{
		if ( level.survival_over )
			break;
		players = maps\mp\gametypes\_survival_utility::living_players();
		fired = false;
		for ( i = 0; i < players.size; i++ )
		{
			v = players[ i ].origin - self.origin;
			if ( lengthSquared( v ) > 150 * 150 )
				continue;
			if ( vectorDot( vectorNormalize( ( v[ 0 ], v[ 1 ], 0 ) ), forward ) < 0.5 )
				continue;
			fired = true;
		}
		if ( fired )
			break;
		wait( 0.1 );
	}
	if ( !level.survival_over )
	{
		playSoundAtPosition( "claymore_activated", self.origin );
		wait( 0.3 );
		if ( isDefined( level._effect[ "martyrdom_c4_explosion" ] ) )
			playFx( level._effect[ "martyrdom_c4_explosion" ], self.origin );
		radiusDamage( self.origin + ( 0, 0, 12 ), 192, 150, 30, self );
	}
	self remove_claymore();
}

remove_claymore()
{
	if ( isDefined( self.survival_spot ) )
		self.survival_spot.survival_claymore = undefined;
	self delete();
}

is_dog_type( ref )
{
	return isSubStr( ref, "dog" ) || isSubStr( ref, "wolf" );
}

// IW5 spawn_dogs / dog_setup on the multiplayer dog (maps\mp\_dogs: its
// template, model, bite and T5 state machine).  dog_splode carries C4 (IW5
// martyrdom): it explodes where it dies.
spawn_dog( ref, origin, angles, scale )
{
	t = level.survival_ai[ ref ];
	ai = maps\mp\gametypes\_survival_utility::spawn_dog( origin, angles );
	if ( !isDefined( ai ) )
		return undefined;

	ai.survival_type = t;
	ai.survival_ref = ref;
	attributes = type_attributes( t, scale );
	ai.health = attributes.health;
	ai.maxhealth = ai.health;
	ai.moveplaybackrate = attributes.speed;
	ai.favoriteenemy = closest_player( origin );
	ai setThreatBiasGroup( "dogs" );

	level.survival_enemies[ level.survival_enemies.size ] = ai;
	ai thread death_watch();
	ai thread chase_watch();
	if ( isSubStr( ref, "splode" ) )
		ai thread dog_c4();
	ai thread dog_hunt();
	return ai;
}

// IW5 dog_seek_player: the dog keeps after the nearest standing player.
dog_hunt()
{
	self endon( "death" );
	for ( ;; )
	{
		if ( level.survival_over )
			return;
		target = closest_player( self.origin );
		if ( isDefined( target ) )
		{
			self.favoriteenemy = target;
			if ( isDefined( self.survival_detour ) && getTime() < self.survival_detour_until &&
			     distanceSquared( self.origin, self.survival_detour ) > 96 * 96 )
			{
				self.goalradius = 64;
				self setGoalPos( self.survival_detour );
			}
			else
			{
				self.survival_detour = undefined;
				self.goalradius = 64;
				self setGoalPos( target.origin );
			}
		}
		wait( 1 );
	}
}

// IW5 dog_splode: C4 strapped on, it goes off where he dies.
dog_c4()
{
	if ( codescripts\character::modelMounted( "weapon_c4" ) )
		self attach( "weapon_c4", "j_spine4", true );
	self waittill( "death", attacker );
	origin = self.origin + ( 0, 0, 16 );
	level thread detonate_c4( origin, 0.5 );
}

// The kill: credits (IW5 givexp_kill, the type's xp as credits; CoDOL
// survivalbounties.csv: headshot +25%, melee +50), the boss assist share,
// without advancing the killer's ordinary MP class killstreaks.
death_watch()
{
	self waittill( "death", attacker );
	level notify( "survival_axis_died" );
	if ( isDefined( self.survival_boss ) )
		level notify( "survival_boss_died" );

	killer = credit_owner( attacker );
	if ( !isDefined( killer ) )
		return;
	value = 100;
	if ( isDefined( self.survival_type ) )
		value = self.survival_type.xp;
	if ( isDefined( self.survival_hitloc ) && self.survival_hitloc == "head" )
		value = int( value * 1.25 );
	if ( isDefined( self.survival_mod ) && self.survival_mod == "MOD_MELEE" )
		value += 50;
	killer maps\mp\gametypes\_survival::add_credits( value, "kill" );
	killer maps\mp\gametypes\_survival::perf_kill( isDefined( self.survival_hitloc ) && self.survival_hitloc == "head" );
	killer award_kill();
}

// A player, or the owner of what killed (a sentry, a killstreak vehicle).
credit_owner( attacker )
{
	if ( !isDefined( attacker ) )
		return undefined;
	if ( isPlayer( attacker ) )
		return attacker;
	if ( isDefined( attacker.owner ) && isPlayer( attacker.owner ) )
		return attacker.owner;
	return undefined;
}

// Scoreboard kills remain separate from purchased Survival support.
award_kill()
{
	self.kills = int( self.kills ) + 1;
	// Survival support is bought at kiosks. Normal MP class streaks do not
	// advance, even if a class or earlier match carried a streak counter.
}

living_enemies()
{
	out = [];
	for ( i = 0; i < level.survival_enemies.size; i++ )
	{
		e = level.survival_enemies[ i ];
		if ( isDefined( e ) && isAlive( e ) )
			out[ out.size ] = e;
	}
	level.survival_enemies = out;
	return out;
}

// --------------------------------------------------------------------------
// The actor callbacks (wrapping the MP dog's, maps\mp\_dogs)
// --------------------------------------------------------------------------

actor_damage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime )
{
	// The waves never hurt each other (IW5 AI friendly fire is off).
	if ( isDefined( eAttacker ) && eAttacker != self && isAI( eAttacker ) &&
	     isDefined( eAttacker.aiteam ) && eAttacker.aiteam == self.aiteam )
		return;
	if ( isDefined( eAttacker ) && isDefined( eAttacker.survival_enemy ) && eAttacker.survival_enemy && !isPlayer( eAttacker ) )
		return;
	if ( isDefined( self.dmg_factor ) )
		iDamage = self jug_damage( iDamage, eAttacker, sMeansOfDeath, sWeapon, sHitLoc, vDir );
	else if ( self shield_stops( sMeansOfDeath, vDir ) )
		iDamage = int( iDamage * 0.1 );
	if ( iDamage <= 0 )
		return;
	self [[level.survival_mp_actor_damage]]( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime );
	if ( isDefined( self.dmg_factor ) )
		self notify( "dmg_factored" );
}

// The hit location and means of death the "death" notify does not carry.
actor_killed( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, psOffsetTime )
{
	self.survival_hitloc = sHitLoc;
	self.survival_mod = sMeansOfDeath;
	// IW5/T5 soldier death drops the held weapon before the corpse transition.
	// The native item path supplies pickup ammo, physics and drop-ring cleanup;
	// Dogs have no carried gun; their damage weapon is a bite definition.
	if ( !isDefined( self.isdog ) || !self.isdog )
		self codol_actorDropWeapon();
	if ( isDefined( level.survival_mp_actor_killed ) )
		self [[level.survival_mp_actor_killed]]( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, psOffsetTime );
}

// IW5 damage_factor: the share of a hit a juggernaut takes, by kind.
jug_damage( iDamage, eAttacker, sMeansOfDeath, sWeapon, sHitLoc, vDir )
{
	f = self.dmg_factor;
	if ( isDefined( eAttacker ) && isAI( eAttacker ) )
		factor = f[ "ai_damage" ];
	else if ( sMeansOfDeath == "MOD_MELEE" )
	{
		factor = f[ "melee" ];
		if ( isDefined( sWeapon ) && isSubStr( sWeapon, "riotshield" ) )
			factor = f[ "melee_riotshield" ];
	}
	else if ( sMeansOfDeath == "MOD_EXPLOSIVE" || sMeansOfDeath == "MOD_GRENADE" || sMeansOfDeath == "MOD_GRENADE_SPLASH" ||
	          sMeansOfDeath == "MOD_PROJECTILE" || sMeansOfDeath == "MOD_PROJECTILE_SPLASH" )
		factor = f[ "explosive" ];
	else if ( isDefined( sHitLoc ) && ( sHitLoc == "head" || sHitLoc == "helmet" ) )
		factor = f[ "headshot" ];
	else
		factor = f[ "bodyshot" ];

	if ( self shield_stops( sMeansOfDeath, vDir ) )
		factor = factor * 0.1;
	iDamage = int( iDamage * factor );
	if ( iDamage < 1 )
		iDamage = 1;
	return iDamage;
}

// The riot shield: a bullet into its face is stopped (IW5's shield has its
// own collision; here the hit's direction against his facing).
shield_stops( sMeansOfDeath, vDir )
{
	if ( !isDefined( self.survival_shield ) || !isDefined( vDir ) || !isSubStr( sMeansOfDeath, "BULLET" ) )
		return false;
	facing = anglesToForward( self.angles );
	return vectorDot( facing, vectorNormalize( ( vDir[ 0 ], vDir[ 1 ], 0 ) ) ) < -0.5;
}

carry_shield()
{
	if ( !codescripts\character::modelMounted( level.survival_shield_model ) )
		return;
	self attach( level.survival_shield_model, "tag_weapon_left", true );
	self.survival_shield = level.survival_shield_model;
}

// IW5 global_jug_behavior and the boss_jug_* factors.
jug_behaviour( ref )
{
	self.dmg_factor = [];
	self.dmg_factor[ "headshot" ] = 1;
	self.dmg_factor[ "bodyshot" ] = 1;
	self.dmg_factor[ "melee" ] = 1;
	self.dmg_factor[ "melee_riotshield" ] = 1;
	self.dmg_factor[ "explosive" ] = 1;
	self.dmg_factor[ "ai_damage" ] = 1;
	self.minpaindamage = 350;
	self.goalradius = 128;
	self setEngagementMinDist( 100, 60 );
	self setEngagementMaxDist( 512, 768 );
	// IW5's juggernaut walks in firing, to 128 units; the machine gun's band
	// had him stop 1400 units out.
	self.meleeattackdist = 700;
	self.survival_engage_base = 700;
	self.survival_engage_floor = 160;
	switch ( ref )
	{
	case "jug_riotshield":
		jug_factors( 0.75, 0.75, 0.33, 0.33, 1.0, 0.25 );
		self thread jug_riotshield();
		break;
	case "jug_explosive":
		jug_factors( 0.33, 0.25, 0.25, 0.25, 0.33, 0.25 );
		break;
	case "jug_headshot":
	case "jug_minigun":
		jug_factors( 1.0, 0.33, 0.25, 0.25, 1.0, 0.25 );
		break;
	default:
		jug_factors( 0.75, 0.33, 0.25, 0.25, 0.33, 0.33 );
		break;
	}
}

jug_factors( head, body, melee, meleeShield, explosive, ai )
{
	self.dmg_factor[ "headshot" ] = head;
	self.dmg_factor[ "bodyshot" ] = body;
	self.dmg_factor[ "melee" ] = melee;
	self.dmg_factor[ "melee_riotshield" ] = meleeShield;
	self.dmg_factor[ "explosive" ] = explosive;
	self.dmg_factor[ "ai_damage" ] = ai;
}

// IW5 boss_jug_riotshield / juggernaut_abandon_shield: he carries the shield
// until half his health is gone, drops it, and fights on as a regular
// juggernaut.
jug_riotshield()
{
	self endon( "death" );
	self carry_shield();
	full = self.health;
	for ( ;; )
	{
		self waittill( "damage" );
		if ( self.health / full <= 0.5 )
			break;
	}
	self drop_shield();
	jug_factors( 0.75, 0.33, 0.25, 0.25, 0.33, 0.33 );
}

drop_shield()
{
	if ( !isDefined( self.survival_shield ) )
		return;
	model = self.survival_shield;
	self.survival_shield = undefined;
	origin = self getTagOrigin( "tag_weapon_left" );
	angles = self getTagAngles( "tag_weapon_left" );
	self detach( model, "tag_weapon_left" );
	s = spawn( "script_model", origin );
	s.angles = angles;
	s setModel( model );
	// A script_model does not fall: it hung at his hand for its 30 s.
	ground = floor_under( origin ) + ( 0, 0, 2 );
	time = sqrt( ( origin[ 2 ] - ground[ 2 ] + 1 ) / 400 );
	if ( time < 0.1 )
		time = 0.1;
	s moveTo( ground, time, time, 0 );
	s rotateTo( ( 0, angles[ 1 ], 90 ), time );
	s thread delete_after( 30 );
}

// The ground under a point (the point itself over a drop).
floor_under( origin )
{
	trace = bulletTrace( origin + ( 0, 0, 8 ), origin - ( 0, 0, 400 ), false, undefined );
	if ( trace[ "fraction" ] >= 1 )
		return origin;
	return trace[ "position" ];
}

delete_after( time )
{
	wait( time );
	if ( isDefined( self ) )
		self delete();
}

// --------------------------------------------------------------------------
// Behaviour (IW5 default_ai / aggressive_ai)
// --------------------------------------------------------------------------

// A Survival hunter keeps approaching the nearest living player. The IW5
// 900-unit default radius assumed its own combat advance; our native soldier
// handles firing at actual weapon range, so an unseen player needs a close
// movement goal rather than stopping outside short-weapon engagement range.
//
// While the actor knows an enemy the native chase owns the goal (it re-sets
// it on the enemy every frame), so the script goal is only written while he
// has none: re-writing it then only reset the native route every second.
// The goal radius shrinks with the distance, so a far hunter is "there" at
// once and keeps walking as it is recomputed, and hunters far out approach
// along their own flank (a per-actor side offset, kept only while findpath
// can reach it) instead of all walking the same route in file.  A detour
// (chase_watch) replaces the player as the goal for a few seconds.
hunt()
{
	self endon( "death" );
	self notify( "survival_hunt" );
	self endon( "survival_hunt" );
	if ( !isAlive( self ) )
		return;
	self.survival_hunting = true;
	// Staggered: a squad spawned on one frame must not re-plan on one frame.
	wait( randomFloat( 1 ) );
	for ( ;; )
	{
		if ( level.survival_over || !isDefined( self ) )
			return;
		target = closest_player( self.origin );
		if ( isDefined( target ) )
			self chase_goal( target );
		wait( 0.8 + randomFloat( 0.4 ) );
	}
}

chase_goal( target )
{
	if ( isDefined( self.survival_dash ) )
		return;
	if ( !( isDefined( self.isdog ) && self.isdog ) )
		self.favoriteenemy = target;
	if ( isDefined( self.survival_detour ) )
	{
		if ( getTime() < self.survival_detour_until &&
		     distanceSquared( self.origin, self.survival_detour ) > 96 * 96 )
		{
			self.goalradius = 64;
			self setGoalPos( self.survival_detour );
			return;
		}
		self.survival_detour = undefined;
	}
	if ( isDefined( self.enemy ) && !( isDefined( self.isdog ) && self.isdog ) )
		return;
	d = distance( self.origin, target.origin );
	radius = d * 0.25;
	if ( radius < 64 )
		radius = 64;
	else if ( radius > 384 )
		radius = 384;
	goal = target.origin;
	if ( d > 1000 && !isDefined( self.survival_stalled ) )
	{
		goal = self approach_goal( target, d );
		if ( isDefined( goal ) )
			radius = 96;
		else
			goal = self flank_goal( target, d );
	}
	self.goalradius = radius;
	self setGoalPos( goal );
}

// The side he comes in from: a waypoint 500 to 950 units from the player, on
// a bearing the other hunters have not taken, within 110 degrees of his own.
// A wave's squads spawn on the far side of the map and arrived in a 50 degree
// arc.  Kept while the player stays near where it was chosen for; undefined
// when there is none he can get to, or once he has been there.
approach_goal( target, d )
{
	if ( d > 3500 || isDefined( self.survival_rush ) || isDefined( self.survival_boss ) || ( isDefined( self.isdog ) && self.isdog ) )
		return undefined;
	if ( isDefined( self.survival_approach ) && distanceSquared( target.origin, self.survival_approach_for ) < 500 * 500 )
	{
		if ( distanceSquared( self.origin, self.survival_approach ) < 200 * 200 )
			self.survival_approach_done = true;
		if ( isDefined( self.survival_approach_done ) )
			return undefined;
		return self.survival_approach;
	}
	now = getTime();
	if ( now < level.survival_next_approach || ( isDefined( self.survival_approach_wait ) && now < self.survival_approach_wait ) )
		return self.survival_approach;
	level.survival_next_approach = now + 100;
	self.survival_approach_wait = now + 4000;
	self.survival_approach = undefined;
	self.survival_approach_done = undefined;
	own = vectorToYaw( self.origin - target.origin );
	taken = [];
	enemies = living_enemies();
	for ( i = 0; i < enemies.size; i++ )
	{
		e = enemies[ i ];
		if ( e == self )
			continue;
		if ( isDefined( e.survival_approach ) && distanceSquared( e.survival_approach_for, target.origin ) < 900 * 900 )
			taken[ taken.size ] = vectorToYaw( e.survival_approach - target.origin );
		else if ( distanceSquared( e.origin, target.origin ) < 1200 * 1200 )
			taken[ taken.size ] = vectorToYaw( e.origin - target.origin );
	}
	points = waypoints_around( target.origin, 2 );
	best = [];
	bestScore = [];
	for ( i = 0; i < points.size; i++ )
	{
		dt = distance2d( points[ i ], target.origin );
		if ( dt < 500 || dt > 950 )
			continue;
		yaw = vectorToYaw( points[ i ] - target.origin );
		off = abs( angleClamp180( yaw - own ) );
		if ( off > 110 )
			continue;
		apart = 120;
		for ( k = 0; k < taken.size; k++ )
		{
			a = abs( angleClamp180( taken[ k ] - yaw ) );
			if ( a < apart )
				apart = a;
		}
		score = apart * 2 - off * 0.5 + randomInt( 30 );
		slot = best.size;
		if ( slot >= 3 )
		{
			if ( score <= bestScore[ 2 ] )
				continue;
			slot = 2;
		}
		while ( slot > 0 && bestScore[ slot - 1 ] < score )
		{
			best[ slot ] = best[ slot - 1 ];
			bestScore[ slot ] = bestScore[ slot - 1 ];
			slot--;
		}
		best[ slot ] = points[ i ];
		bestScore[ slot ] = score;
	}
	for ( i = 0; i < best.size; i++ )
	{
		if ( !findPath( self.origin, best[ i ] ) )
			continue;
		self.survival_approach = best[ i ];
		self.survival_approach_for = target.origin;
		return best[ i ];
	}
	return undefined;
}

// A side offset of up to 300 units, perpendicular to the line to the target
// and fixed per actor, re-validated every few seconds; the plain target when
// findpath cannot reach the offset point.
flank_goal( target, d )
{
	if ( !isDefined( self.survival_flank_side ) )
	{
		self.survival_flank_side = randomFloatRange( -1, 1 );
		self.survival_flank_check = 0;
	}
	v = vectorNormalize( ( target.origin[ 0 ] - self.origin[ 0 ], target.origin[ 1 ] - self.origin[ 1 ], 0 ) );
	side = ( 0 - v[ 1 ], v[ 0 ], 0 );
	reach = d * 0.25;
	if ( reach > 300 )
		reach = 300;
	goal = target.origin + side * ( reach * self.survival_flank_side );
	if ( getTime() >= self.survival_flank_check )
	{
		self.survival_flank_check = getTime() + 3000 + randomInt( 1000 );
		self.survival_flank_ok = findPath( self.origin, goal );
	}
	if ( !self.survival_flank_ok )
		return target.origin;
	return goal;
}

// --------------------------------------------------------------------------
// The chase watchdog (MW3 Survival: the enemies always find you after a
// while).  One per enemy, staggered.  It measures progress -- the distance to
// the nearest standing player shrinking by 64 units, or the actor inside
// close range -- and escalates an actor that has made none:
//   6 s   a soldier fighting from long range advances (a juggernaut or a
//         walking bomb after 3 s): his native attack range (meleeattackdist, the distance at which
//         the frame hands him soldier_combat) is cut below his current
//         distance, down to a floor per weapon or type, so he breaks off,
//         closes 250-450 units and fights again -- a bounding advance, not a
//         beeline.
//   a soldier already that close who has stood on one spot for a while, or
//         has had no shot from it for some seconds (a squadmate or a wall in
//         the line), moves to another that has one (combat_tick).
//   8 s   a follower leaves his (stuck) leader and hunts himself; a hunter
//         with no enemy whose route fails is sent to the reachable waypoint
//         nearest the player for a few seconds (the goal it could not reach
//         is replaced, not re-issued).
//   20 s  (12 s once the planner reported bad_path) an actor no player can
//         see is moved to a spot no player can see, 600-1400 units from the
//         player, from which findpath reaches him: the off-graph, wedged and
//         unroutable ones.
//   10 s  on one spot far from the player, unseen and without a shot: the same.
// The player moving away rebases the measure without counting as a stall.
// --------------------------------------------------------------------------

chase_init()
{
	level.survival_waypoints = codol_getWalkableWaypoints();
	if ( !isDefined( level.survival_waypoints ) )
		level.survival_waypoints = [];
	level.survival_wp_grid = [];
	for ( i = 0; i < level.survival_waypoints.size; i++ )
	{
		key = wp_cell( level.survival_waypoints[ i ], 0, 0 );
		if ( !isDefined( level.survival_wp_grid[ key ] ) )
			level.survival_wp_grid[ key ] = [];
		n = level.survival_wp_grid[ key ].size;
		level.survival_wp_grid[ key ][ n ] = level.survival_waypoints[ i ];
	}
	level.survival_next_dash = 0;
	level.survival_next_approach = 0;
	codolprintln( "CoDOL survival: chase watchdog, " + level.survival_waypoints.size + " walkable waypoints" );
}

// The waypoints by 512-unit cell, so a look around one actor reads a few
// dozen of them and not the level's thousand.
wp_cell( p, dx, dy )
{
	return int( floor( p[ 0 ] / 512 ) + dx ) + "_" + int( floor( p[ 1 ] / 512 ) + dy );
}

waypoints_around( origin, cells )
{
	out = [];
	for ( x = 0 - cells; x <= cells; x++ )
	{
		for ( y = 0 - cells; y <= cells; y++ )
		{
			cell = level.survival_wp_grid[ wp_cell( origin, x, y ) ];
			if ( !isDefined( cell ) )
				continue;
			for ( i = 0; i < cell.size; i++ )
				out[ out.size ] = cell[ i ];
		}
	}
	return out;
}

chase_watch()
{
	self endon( "death" );
	self notify( "survival_chase_watch" );
	self endon( "survival_chase_watch" );
	self.survival_badpath = 0;
	self thread chase_badpath();
	self.survival_engage_base = self.meleeattackdist;
	best = undefined;
	anchor = undefined;
	since = getTime();
	badpathSeen = 0;
	pushed = 0;
	nextRelocate = 0;
	stillAt = undefined;
	stillSince = since;
	wait( 1 + randomFloat( 1 ) );
	for ( ;; )
	{
		wait( 1 );
		if ( level.survival_over )
			return;
		target = closest_player( self.origin );
		if ( !isDefined( target ) )
			continue;
		now = getTime();
		d = distance( self.origin, target.origin );
		close = self chase_close_range();
		self combat_tick( target, d, now );

		// 10 s on one spot, far from the player, unseen and without a shot:
		// stuck, however the player moves about meanwhile (his moving rebases
		// the stall below, so against one who kept moving it never came).
		if ( !isDefined( stillAt ) || distanceSquared( self.origin, stillAt ) > 64 * 64 || self shot_since( stillSince ) )
		{
			stillAt = self.origin;
			stillSince = now;
		}
		else if ( now - stillSince >= 10000 && d > 700 && now >= nextRelocate && !self seen_by_players() )
		{
			nextRelocate = now + 5000;
			if ( self chase_relocate( target ) )
			{
				stillAt = undefined;
				best = undefined;
				since = now;
				self.survival_stalled = undefined;
				self.survival_detour = undefined;
				continue;
			}
		}
		if ( isDefined( anchor ) && distanceSquared( target.origin, anchor ) > 256 * 256 &&
		     self is_soldier() && self.meleeattackdist < self.survival_engage_base )
			self.meleeattackdist = ( self.meleeattackdist + self.survival_engage_base ) * 0.5;	// the player moved: give some range back
		if ( !isDefined( best ) || d < best - 64 || d <= close ||
		     distanceSquared( target.origin, anchor ) > 256 * 256 )
		{
			if ( isDefined( best ) && d < best - 64 && isDefined( self.survival_stalled ) )
				self.survival_stalled = undefined;
			best = d;
			anchor = target.origin;
			since = now;
			pushed = 0;
			if ( d <= close )
				self.survival_stalled = undefined;
			continue;
		}
		stalled = ( now - since ) / 1000;
		if ( self.survival_badpath > badpathSeen )
		{
			badpathSeen = self.survival_badpath;
			if ( stalled < 4 )
				continue;
		}

		// 6 s: a soldier in his combat script at long range bounds forward.
		patience = 6;
		if ( isDefined( self.survival_boss ) || isDefined( self.survival_rush ) )
			patience = 3;
		if ( stalled >= patience && now >= pushed && self is_soldier() && isDefined( self.enemy ) )
		{
			engageFloor = self chase_engage_floor();
			if ( d > engageFloor + 64 )
			{
				range = d - 250 - randomInt( 200 );
				if ( range < engageFloor )
					range = engageFloor;
				if ( range < self.meleeattackdist )
					self.meleeattackdist = range;
				pushed = now + 5000 + randomInt( 3000 );
			}
		}

		// 8 s: no more following, and no more re-issuing a goal that fails.
		if ( stalled >= 8 && !isDefined( self.survival_stalled ) )
		{
			self.survival_stalled = true;
			self.survival_leader = undefined;
			if ( !( isDefined( self.isdog ) && self.isdog ) && !( isDefined( self.survival_hunting ) && self.survival_hunting ) )
				self thread hunt();
			if ( !isDefined( self.enemy ) || ( isDefined( self.isdog ) && self.isdog ) )
				self chase_detour( target );
		}
		else if ( stalled >= 14 && isDefined( self.survival_stalled ) && !isDefined( self.survival_detour ) &&
		          !isDefined( self.enemy ) )
			self chase_detour( target );

		// 20 s (12 after a bad_path): out of sight, out of the hole.
		limit = 20;
		if ( badpathSeen > 0 )
			limit = 12;
		if ( stalled >= limit && d > 700 && now >= nextRelocate && !self seen_by_players() )
		{
			nextRelocate = now + 5000;
			if ( self chase_relocate( target ) )
			{
				best = undefined;
				since = getTime();
				self.survival_stalled = undefined;
				self.survival_detour = undefined;
			}
		}
	}
}

shot_since( time )
{
	return isDefined( self.a ) && isDefined( self.a.lastShootTime ) && self.a.lastShootTime > time;
}

chase_badpath()
{
	self endon( "death" );
	for ( ;; )
	{
		self waittill( "bad_path" );
		self.survival_badpath++;
	}
}

is_soldier()
{
	return !( isDefined( self.isdog ) && self.isdog ) && isDefined( self.survival_engage_base ) && self.survival_engage_base > 0;
}

// Inside this distance an actor is engaging, not chasing.
chase_close_range()
{
	if ( isDefined( self.isdog ) && self.isdog )
		return 160;
	return self chase_engage_floor() + 96;
}

// How close the bounding advance takes a soldier before it lets him hold.
chase_engage_floor()
{
	if ( isDefined( self.survival_engage_floor ) )
		return self.survival_engage_floor;
	base = self.survival_engage_base;
	if ( !isDefined( base ) || base <= 0 )
		return 160;
	if ( base >= 2000 )
		return 1000;	// snipers keep their distance
	if ( base <= 600 )
		return 220;	// shotguns, pistols
	return 400;
}

// The walkable waypoint nearest the target that findpath reaches from here,
// as the goal for a few seconds.
chase_detour( target )
{
	wps = level.survival_waypoints;
	if ( !isDefined( wps ) || !wps.size )
		return;
	near = nearest_points( wps, target.origin, 6, 0, 900 );
	for ( i = 0; i < near.size; i++ )
	{
		if ( distanceSquared( near[ i ], self.origin ) < 128 * 128 )
			continue;
		if ( !findPath( self.origin, near[ i ] ) )
			continue;
		self.survival_detour = near[ i ];
		self.survival_detour_until = getTime() + 6000;
		self.goalradius = 64;
		self setGoalPos( near[ i ] );
		return;
	}
}

// Up to `count` of `points` nearest `origin`, between `minDist` and `maxDist`.
nearest_points( points, origin, count, minDist, maxDist )
{
	out = [];
	dist = [];
	for ( i = 0; i < points.size; i++ )
	{
		d = distanceSquared( points[ i ], origin );
		if ( d < minDist * minDist || d > maxDist * maxDist )
			continue;
		if ( out.size >= count && d >= dist[ out.size - 1 ] )
			continue;
		slot = out.size;
		if ( slot >= count )
			slot = count - 1;
		while ( slot > 0 && dist[ slot - 1 ] > d )
		{
			out[ slot ] = out[ slot - 1 ];
			dist[ slot ] = dist[ slot - 1 ];
			slot--;
		}
		out[ slot ] = points[ i ];
		dist[ slot ] = d;
	}
	return out;
}

seen_by_players()
{
	return point_seen( self.origin + ( 0, 0, 60 ), self );
}

point_seen( point, ignore )
{
	players = maps\mp\gametypes\_survival_utility::living_players();
	for ( i = 0; i < players.size; i++ )
	{
		if ( sightTracePassed( players[ i ] getEye(), point, false, ignore ) )
			return true;
	}
	return false;
}

// A hidden spot 600-1400 units from the target from which findpath reaches
// him: the walkable waypoints, or the squad spawn locations on a map without
// a waypoint graph.  The actor keeps his health, wave and squad.
chase_relocate( target )
{
	points = level.survival_waypoints;
	if ( !isDefined( points ) || !points.size )
	{
		points = [];
		for ( i = 0; i < level.survival_squad_locs.size; i++ )
			points[ points.size ] = level.survival_squad_locs[ i ].origin;
	}
	near = nearest_points( points, target.origin, 24, 600, 1400 );
	tried = 0;
	for ( i = randomInt( 4 ); i < near.size && tried < 8; i++ )
	{
		p = near[ i ];
		if ( point_seen( p + ( 0, 0, 60 ), self ) )
			continue;
		tried++;
		if ( !findPath( p, target.origin ) )
			continue;
		from = self.origin;
		placed = self forceTeleport( p + ( 0, 0, 4 ), ( 0, vectorToYaw( target.origin - p ), 0 ) );
		if ( isDefined( placed ) && !placed )
			continue;
		codolprintln( "CoDOL survival: " + self.survival_ref + " made no progress toward the player from " + from +
			"; moved out of sight to " + p );
		return true;
	}
	return false;
}

// --------------------------------------------------------------------------
// Fighting from a new place.  Once a soldier stood inside his weapon's floor
// he stayed on that spot for the rest of the fight, three or four of them in
// a clump, some without a shot because a squadmate stood in their line (the
// animscript holds fire for a friendly).  Now he moves on after a while, or
// when he has had no shot for some seconds: to a waypoint near him that sees
// the player, away from his squadmates and off their bearing, with the native
// enemy dropped (ignoreall) for the run, since the chase otherwise owns the
// goal.  One search every 0.2 s on the level.
// --------------------------------------------------------------------------

combat_tick( target, d, now )
{
	if ( !self is_soldier() || isDefined( self.survival_boss ) || isDefined( self.survival_rush ) || isDefined( self.survival_dash ) )
		return;
	if ( !isDefined( self.survival_held_at ) || !isDefined( self.enemy ) ||
	     distanceSquared( self.origin, self.survival_held_at ) > 48 * 48 )
	{
		self.survival_held_at = self.origin;
		self.survival_held_since = now;
		self.survival_hold = 6000 + randomInt( 5000 );
		return;
	}
	held = now - self.survival_held_since;
	shot = self.survival_held_since;
	if ( isDefined( self.a ) && isDefined( self.a.lastShootTime ) && self.a.lastShootTime > shot )
		shot = self.a.lastShootTime;
	// 2 s without a shot when squadmates stand in the line, else 3.5 s
	patience = 3500;
	if ( isDefined( self.survival_blocked ) && now - self.survival_blocked < 1500 )
		patience = 2000;
	if ( held < self.survival_hold && now - shot < patience )
		return;
	if ( now < level.survival_next_dash )
		return;
	level.survival_next_dash = now + 200;
	spot = self combat_spot( target, d );
	if ( isDefined( spot ) )
		self thread combat_dash( spot );
	else
	{
		// nowhere else to shoot from: come closer instead
		self.survival_held_since = now;
		range = d - 150;
		least = self combat_nearest();
		if ( range < least )
			range = least;
		if ( range < self.meleeattackdist )
			self.meleeattackdist = range;
	}
}

// A new firing spot may be this near the player.
combat_nearest()
{
	near = self chase_engage_floor() * 0.5;
	if ( near < 120 )
		near = 120;
	return near;
}

combat_spot( target, d )
{
	far = d + 150;
	if ( far > self.survival_engage_base )
		far = self.survival_engage_base;
	near = self combat_nearest();
	enemies = living_enemies();
	points = waypoints_around( self.origin, 1 );
	best = [];
	bestScore = [];
	for ( i = 0; i < points.size; i++ )
	{
		p = points[ i ];
		step = distance( p, self.origin );
		if ( step < 140 || step > 520 || abs( p[ 2 ] - self.origin[ 2 ] ) > 120 )
			continue;
		dt = distance( p, target.origin );
		if ( dt < near || dt > far )
			continue;
		// the bearing farthest from the others', and nobody standing there
		yaw = vectorToYaw( p - target.origin );
		apart = 90;
		taken = false;
		for ( k = 0; k < enemies.size; k++ )
		{
			e = enemies[ k ];
			if ( e == self || distanceSquared( e.origin, target.origin ) > 1200 * 1200 )
				continue;
			if ( distanceSquared( e.origin, p ) < 96 * 96 || ( isDefined( e.survival_dash ) && distanceSquared( e.survival_dash, p ) < 96 * 96 ) )
			{
				taken = true;
				break;
			}
			a = abs( angleClamp180( vectorToYaw( e.origin - target.origin ) - yaw ) );
			if ( a < apart )
				apart = a;
		}
		if ( taken )
			continue;
		score = apart * 3 - step * 0.2 + randomInt( 60 );
		slot = best.size;
		if ( slot >= 5 )
		{
			if ( score <= bestScore[ 4 ] )
				continue;
			slot = 4;
		}
		while ( slot > 0 && bestScore[ slot - 1 ] < score )
		{
			best[ slot ] = best[ slot - 1 ];
			bestScore[ slot ] = bestScore[ slot - 1 ];
			slot--;
		}
		best[ slot ] = p;
		bestScore[ slot ] = score;
	}
	for ( i = 0; i < best.size; i++ )
	{
		if ( !bulletTracePassed( best[ i ] + ( 0, 0, 56 ), target getEye(), false, self ) )
			continue;
		if ( findPath( self.origin, best[ i ] ) )
			return best[ i ];
	}
	return undefined;
}

combat_dash( spot )
{
	self endon( "death" );
	self.survival_dash = spot;
	if ( !isDefined( self.survival_dashes ) )
		self.survival_dashes = 0;
	self.survival_dashes++;
	self.ignoreall = true;
	self.goalradius = 40;
	self setGoalPos( spot );
	from = self.origin;
	still = 0;
	for ( t = 0; t < 4.5; t += 0.25 )
	{
		wait( 0.25 );
		if ( distanceSquared( self.origin, spot ) < 64 * 64 )
			break;
		if ( distanceSquared( self.origin, from ) < 8 * 8 )
			still++;
		else
			still = 0;
		// a burst or a reload may hold the start of the run
		if ( still >= 8 )
			break;
		from = self.origin;
		target = closest_player( self.origin );
		if ( isDefined( target ) && distanceSquared( target.origin, self.origin ) < 140 * 140 )
			break;
	}
	self.ignoreall = false;
	self.survival_dash = undefined;
	self.survival_held_at = undefined;
	target = closest_player( self.origin );
	if ( isDefined( target ) )
		self.favoriteenemy = target;
}

closest_player( origin )
{
	players = maps\mp\gametypes\_survival_utility::living_players();
	best = undefined;
	bestDist = 0;
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( isDefined( p.laststand ) && p.laststand )
			continue;
		d = distanceSquared( p.origin, origin );
		if ( !isDefined( best ) || d < bestDist )
		{
			best = p;
			bestDist = d;
		}
	}
	if ( !isDefined( best ) && players.size )
		best = players[ 0 ];
	return best;
}

// IW5 survival_wave: once four or fewer are left, every enemy goes
// aggressive and hunts.
go_aggressive()
{
	level.survival_aggressive = true;
	enemies = living_enemies();
	for ( i = 0; i < enemies.size; i++ )
	{
		e = enemies[ i ];
		e.survival_leader = undefined;
		if ( isDefined( e.isdog ) && e.isdog )
			continue;
		e thread hunt();
	}
	codolprintln( "CoDOL survival: wave " + level.survival_wave_num + " aggressive, " + enemies.size + " left" );
}

// --------------------------------------------------------------------------
// Specials and bosses
// --------------------------------------------------------------------------

// IW5 spawn_special_ai / spawn_dogs: each at one of the four spawn locations
// farthest from the players.
spawn_specials( ref, count, scale )
{
	spawned = 0;
	for ( i = 0; i < count; i++ )
	{
		loc = maps\mp\gametypes\_survival_squad::far_location( 4 );
		if ( !isDefined( loc ) )
			break;
		ai = undefined;
		for ( tries = 0; !isDefined( ai ) && tries < 10; tries++ )
		{
			ai = spawn_type( ref, loc.origin, loc.angles, scale );
			if ( !isDefined( ai ) )
				wait( 1 );
		}
		if ( !isDefined( ai ) )
			continue;
		if ( !( isDefined( ai.isdog ) && ai.isdog ) )
			ai thread hunt();
		spawned++;
		wait( 0.1 );
	}
	return spawned;
}

// IW5 spawn_juggernaut: each one comes in on an Mi-17 along a drop path
// (maps\mp\gametypes\_survival_vehicles::drop_juggernaut); a map without
// drop paths, or a drop that fails, has him walk in from the farthest
// location.
spawn_bosses( refs, scale )
{
	level.survival_bosses = [];
	for ( i = 0; i < refs.size; i++ )
	{
		ref = refs[ i ];
		if ( !isSubStr( ref, "jug_" ) )
			continue;
		if ( maps\mp\gametypes\_survival_vehicles::drop_juggernaut( ref, scale ) )
		{
			codolprintln( "CoDOL survival: boss " + ref + " inbound by chopper" );
			wait( 1 );
			continue;
		}
		ai = undefined;
		{
			loc = maps\mp\gametypes\_survival_squad::farthest_location();
			if ( !isDefined( loc ) )
				continue;
			for ( tries = 0; !isDefined( ai ) && tries < 15; tries++ )
			{
				ai = spawn_boss( ref, loc.origin, loc.angles, scale );
				if ( !isDefined( ai ) )
					wait( 1 );
			}
		}
		if ( !isDefined( ai ) )
			continue;
		codolprintln( "CoDOL survival: boss " + ref + " spawned" );
		wait( 0.5 );
	}
}

// A juggernaut with IW5's boss behaviour.
spawn_boss( ref, origin, angles, scale )
{
	ai = spawn_type( ref, origin, angles, scale );
	if ( !isDefined( ai ) )
		return undefined;
	ai.survival_boss = true;
	ai setThreatBiasGroup( "boss" );
	ai jug_behaviour( ref );
	level.survival_bosses[ level.survival_bosses.size ] = ai;
	ai thread hunt();
	return ai;
}

bosses_alive()
{
	n = 0;
	for ( i = 0; i < level.survival_bosses.size; i++ )
	{
		if ( isDefined( level.survival_bosses[ i ] ) && isAlive( level.survival_bosses[ i ] ) )
			n++;
	}
	return n;
}
