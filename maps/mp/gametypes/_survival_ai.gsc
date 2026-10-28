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
//     6 alt weapon (grenade)  7 health  8 speed  9 xp (the credits a kill
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
		n++;
	}
	codolprintln( "CoDOL survival: " + n + " AI types used by this level's waves" );

	level.survival_enemies = [];
	level.survival_bosses = [];
	loot_init();

	// IW5 ai_preload: the martyrdom C4, the claymores, the gas.
	codescripts\character::precacheMounted( "weapon_c4" );
	codescripts\character::precacheMounted( "weapon_claymore" );
	level.survival_shield_model = "weapon_riot_shield_mp";
	codescripts\character::precacheMounted( level.survival_shield_model );
	precacheShellShock( "concussion_grenade_mp" );
	level._effect[ "martyrdom_c4_explosion" ] = loadFx( "explosions/grenadeexp_default" );
}

table_int( table, row, column, fallback )
{
	v = tableLookup( table, 0, "" + row, column );
	if ( !isDefined( v ) || v == "" )
		return fallback;
	return int( v );
}

table_float( table, row, column, fallback )
{
	v = tableLookup( table, 0, "" + row, column );
	if ( !isDefined( v ) || v == "" )
		return fallback;
	return float( v );
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
// (IW5 grows the AI through the repeat count; here health and accuracy scale).
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
		pass = int( past / level.survival_repeat_waves.size ) + 1;
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
	w.scale = 1.0 + 0.15 * pass;
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
	kind.accuracy = t.accuracy * 0.3;
	if ( t.weapon != "" )
		kind.weapon = t.weapon;
	ai = maps\mp\gametypes\_survival_utility::spawn_soldier( t.classname, origin, angles, kind );
	if ( !isDefined( ai ) )
		return undefined;
	ai maps\mp\gametypes\_survival_utility::arm_soldier();
	ai maps\mp\gametypes\_survival_utility::first_pose();

	ai.survival_type = t;
	ai.survival_ref = ref;
	ai.health = int( t.health * scale );
	ai.maxhealth = ai.health;
	ai.accuracy = t.accuracy * 0.3 * scale;
	if ( ai.accuracy > 0.6 )
		ai.accuracy = 0.6;
	// IW5 moveplaybackrate from the speed column.
	ai.moveplaybackrate = t.speed;
	ai.ignoreall = false;
	ai setThreatBiasGroup( "axis" );

	level.survival_enemies[ level.survival_enemies.size ] = ai;
	ai thread death_watch();
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
// dies, with 40% of a clip and half the reserve.
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
	stock = int( loot_max( 1, 0.5 * weaponMaxAmmo( weapon ) ) );
	item itemWeaponSetAmmo( clip, stock );
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
	}
}

// IW5 martyrdom_ability / detonate_c4_when_dead: C4 strapped to the chest,
// and it goes off where he dies (IW5: 1.5 s fuse, radius 192; the volatile
// variant at once).
martyrdom_ability( fuse )
{
	if ( codescripts\character::modelMounted( "weapon_c4" ) )
		self attach( "weapon_c4", "j_spine4", true );
	self waittill( "death", attacker );
	origin = self.origin + ( 0, 0, 16 );
	if ( fuse > 0.05 )
		playSoundAtPosition( "weap_c4detpack_trigger_plr", origin );
	wait( fuse );
	if ( isDefined( level._effect[ "martyrdom_c4_explosion" ] ) )
		playFx( level._effect[ "martyrdom_c4_explosion" ], origin );
	playSoundAtPosition( "grenade_explode_metal", origin );
	radiusDamage( origin, 192, 150, 50 );
	earthquake( 0.4, 0.6, origin, 600 );
}

// IW5 chemical_ability / chemical_ability_gas_cloud: the gas tank bursts when
// he dies, and the cloud hurts whoever stands in it (IW5: radius 160, about
// 10 s).
chemical_ability()
{
	self waittill( "death", attacker );
	origin = self.origin + ( 0, 0, 24 );
	if ( isDefined( level._effect[ "chemical_cloud" ] ) )
		playFx( level._effect[ "chemical_cloud" ], origin );
	for ( t = 0; t < 10; t += 0.5 )
	{
		players = maps\mp\gametypes\_survival_utility::living_players();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( distanceSquared( p.origin, origin ) > 160 * 160 )
				continue;
			p doDamage( 6, origin );
			if ( !isDefined( p.survival_gassed ) || getTime() - p.survival_gassed > 2000 )
			{
				p.survival_gassed = getTime();
				p shellShock( "concussion_grenade_mp", 2 );
			}
		}
		wait( 0.5 );
	}
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
	health = 200;
	if ( isDefined( t ) )
		health = t.health * 4;
	if ( !isDefined( scale ) )
		scale = 1.0;
	ai.health = int( health * scale );
	ai.maxhealth = ai.health;
	ai.favoriteenemy = closest_player( origin );
	ai setThreatBiasGroup( "dogs" );

	level.survival_enemies[ level.survival_enemies.size ] = ai;
	ai thread death_watch();
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
			self.goalradius = 64;
			self setGoalPos( target.origin );
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
	wait( 0.5 );
	if ( isDefined( level._effect[ "martyrdom_c4_explosion" ] ) )
		playFx( level._effect[ "martyrdom_c4_explosion" ], origin );
	radiusDamage( origin, 192, 150, 50 );
}

// The kill: credits (IW5 givexp_kill, the type's xp as credits; CoDOL
// survivalbounties.csv: headshot +25%, melee +50), the boss assist share,
// and the killer's class killstreaks advance.
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

// The MP kill streak on an AI kill (as _damage's player kill counts it).
award_kill()
{
	self.kills = int( self.kills ) + 1;
	if ( !level.survival_killstreaks || !isAlive( self ) )
		return;
	if ( !isDefined( self.pers[ "cur_kill_streak" ] ) )
		self.pers[ "cur_kill_streak" ] = 0;
	self.pers[ "cur_kill_streak" ]++;
	self thread maps\mp\killstreaks\_killstreaks::checkKillstreakReward( self.pers[ "cur_kill_streak" ] );
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

	// The riot shield: a bullet into its face is stopped (IW5's shield has
	// its own collision; here the hit's direction against his facing).
	if ( isDefined( self.survival_shield ) && isSubStr( sMeansOfDeath, "BULLET" ) && isDefined( vDir ) )
	{
		facing = anglesToForward( self.angles );
		if ( vectorDot( facing, vectorNormalize( ( vDir[ 0 ], vDir[ 1 ], 0 ) ) ) < -0.5 )
			factor = factor * 0.1;
	}
	iDamage = int( iDamage * factor );
	if ( iDamage < 1 )
		iDamage = 1;
	return iDamage;
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
	if ( codescripts\character::modelMounted( level.survival_shield_model ) )
	{
		self attach( level.survival_shield_model, "tag_weapon_left", true );
		self.survival_shield = level.survival_shield_model;
	}
	full = self.health;
	for ( ;; )
	{
		self waittill( "damage" );
		if ( self.health / full <= 0.5 )
			break;
	}
	self drop_shield();
	jug_factors( 0.75, 0.33, 0.25, 0.25, 0.33, 0.33 );
	self thread jug_shield_on_death();
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
	s thread delete_after( 30 );
}

jug_shield_on_death()
{
	self waittill( "death" );
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

// IW5 default_squad_leader / aggressive_squad_leader: the leader heads for
// the closest player, goal radius 900, or 384 in the aggressive phase.
hunt()
{
	self endon( "death" );
	self notify( "survival_hunt" );
	self endon( "survival_hunt" );
	for ( ;; )
	{
		if ( level.survival_over )
			return;
		aggressive = isDefined( level.survival_aggressive ) && level.survival_aggressive;
		self.goalradius = 900;
		if ( aggressive )
			self.goalradius = 384;
		if ( isDefined( self.dmg_factor ) )
			self.goalradius = 128;
		target = closest_player( self.origin );
		if ( isDefined( target ) )
			self setGoalPos( target.origin );
		wait( 2 + randomFloat( 2 ) );
	}
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

// IW5 spawn_special_ai: at the spawn location farthest from the players.
spawn_specials( ref, count, scale )
{
	spawned = 0;
	for ( i = 0; i < count; i++ )
	{
		loc = maps\mp\gametypes\_survival_squad::farthest_location();
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
