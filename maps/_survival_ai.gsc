// coop survival: the AI types, the wave table and the enemies.
//
// Ported from Infinity Ward's Modern Warfare 3 Spec Ops Survival,
// maps\_so_survival_ai.gsc (IW5; D:\CoD\COD-GSC-Source\IW5-GSC\scripts\1564.gsc
// in the decompiled dump), as CoDOL's own survival builds carry it: the tables
// are 2.6.9.7's common_gametype_survival (mp/survival_waves.csv, IW5's layout)
// and the session zone's mp/survival_waves_<map>.csv.  docs/CODOL_SURVIVAL_PLAN.md
// section 2.
//
// Coop module (maps/, the singleplayer namespace); it names no multiplayer
// module.  Enemies are coop soldiers spawned through maps\_coopmission's
// spawn_soldier, so the shared actor budget, death accounting, nametags and
// weapons serve them.
//
// Tables.
//   AI types (level.survival_type_table, rows 100..199, column 0 the index):
//     1 ref  2 name  3 desc  4 classname  5 weapon  6 alt weapon (grenade)
//     7 health  8 speed  9 xp (the credits a kill pays)  10 boss  11 accuracy
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
	for ( i = 100; i < 200; i++ )
	{
		ref = tableLookup( level.survival_type_table, 0, "" + i, 1 );
		if ( !isDefined( ref ) || ref == "" )
			continue;
		t = spawnStruct();
		t.index = i;
		t.ref = ref;
		t.classname = tableLookup( level.survival_type_table, 0, "" + i, 4 );
		t.weapon = tableLookup( level.survival_type_table, 0, "" + i, 5 );
		t.altweapon = tableLookup( level.survival_type_table, 0, "" + i, 6 );
		t.health = table_int( level.survival_type_table, i, 7, 150 );
		t.speed = table_float( level.survival_type_table, i, 8, 1 );
		t.xp = table_int( level.survival_type_table, i, 9, 100 );
		t.boss = ( tableLookup( level.survival_type_table, 0, "" + i, 10 ) == "1" );
		t.accuracy = table_float( level.survival_type_table, i, 11, 1 );
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

	// The juggernaut boss classes and the dogs are spawned through the same
	// soldier path; the dog has no coop species yet (plan section 3.2).
	level.survival_enemies = [];
	level.survival_bosses = [];

	// IW5 ai_preload: the martyrdom C4 (the dog_splode and martyrdom types).
	codescripts\character::precacheMounted( "weapon_c4" );
	level._effect[ "martyrdom_c4_explosion" ] = loadFx( "explosions/grenadeexp_default" );

	// IW5 squad_drop_weapon_rate 1, ai_dropAkimboChance 0: nothing this
	// server's coop soldier drops needs a rate.
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

// IW5 get_squad_array: the wave's squad size in squads of three (a remainder
// of one joins the last squad when IW5's merge_squad_member_max is 4, else it
// takes one from it so no squad is a lone man).
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
	{
		// The choppers: no actor class (plan section 3, the vehicle layer).
		return undefined;
	}
	if ( !isDefined( scale ) )
		scale = 1.0;

	kind = spawnStruct();
	kind.accuracy = t.accuracy * 0.3;
	ai = maps\_survival_utility::spawn_soldier( t.classname, origin, angles, kind );
	if ( !isDefined( ai ) )
		return undefined;
	ai maps\_survival_utility::arm_soldier();
	ai maps\_survival_utility::first_pose();

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

	level.survival_enemies[ level.survival_enemies.size ] = ai;
	ai thread death_watch();
	return ai;
}

is_dog_type( ref )
{
	return isSubStr( ref, "dog" ) || isSubStr( ref, "wolf" );
}

// IW5 spawn_dogs / dog_setup: the survival dog species (aitype\enemy_dog_so,
// animscripts\coop_dog_*, the shared Spec Ops dog set) on the actor budget.  dog_splode / wolf_splode carry C4 (IW5
// dog_detonate_c4_near_sentry / martyrdom): they explode where they die.
spawn_dog( ref, origin, angles, scale )
{
	t = level.survival_ai[ ref ];
	level.survival_dog_wolf = isSubStr( ref, "wolf" );
	ai = maps\_survival_utility::spawn_soldier( "actor_enemy_dog_so", origin, angles, undefined );
	level.survival_dog_wolf = undefined;
	if ( !isDefined( ai ) )
		return undefined;

	ai.isdog = true;
	ai.allowdeath = true;
	ai.ignoreSuppression = true;
	ai.noDodgeMove = true;
	ai.ignoreall = false;
	// IW5 enemy_dog meleeDamage; _survival_mode scales non-bullet damage.
	ai.meleeDamage = 40;
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

// IW5 behavior_special_ai_martyrdom / dog_splode: C4 strapped on, it goes off
// where he dies (IW5 detonate_c4_when_dead: radius 192, 150 at the centre).
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

death_watch()
{
	self waittill( "death", attacker );
	level notify( "survival_axis_died" );
	if ( isDefined( self.survival_boss ) )
		level notify( "survival_boss_died" );

	killer = attacker;
	if ( isDefined( killer ) && !isPlayer( killer ) && isDefined( killer.owner ) && isPlayer( killer.owner ) )
		killer = killer.owner;
	if ( !isDefined( killer ) || !isPlayer( killer ) )
		return;

	// IW5 givexp_kill: the type's xp, as credits (survival_credit is what the
	// armory spends).  CoDOL survivalbounties.csv: headshot +25%, melee +50.
	value = self.survival_type.xp;
	if ( isDefined( self.survival_hitloc ) && self.survival_hitloc == "head" )
		value = int( value * 1.25 );
	if ( isDefined( self.survival_mod ) && self.survival_mod == "MOD_MELEE" )
		value += 50;
	killer maps\_survival::add_credits( value, "kill" );
}

// _survival_mode's CodeCallback_ActorKilled:
// the hit location and means of death the "death" notify does not carry.
on_actor_killed( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset )
{
	self.survival_hitloc = sHitLoc;
	self.survival_mod = sMeansOfDeath;
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
// Behaviour (IW5 default_ai / aggressive_ai)
// --------------------------------------------------------------------------

// IW5 default_squad_leader / aggressive_squad_leader: the leader heads for
// the closest player (IW5 setgoalentity, which this server lacks: the goal is
// refreshed), goal radius 900, or 384 in the aggressive phase.
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
		target = closest_player( self.origin );
		if ( isDefined( target ) )
			self setGoalPos( target.origin );
		wait( 2 + randomFloat( 2 ) );
	}
}

closest_player( origin )
{
	players = maps\_survival_utility::living_players();
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
		loc = maps\_survival_squad::farthest_location();
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
		ai thread hunt();
		spawned++;
		wait( 0.1 );
	}
	return spawned;
}

// IW5 spawn_boss_ai: the juggernauts (IW5 drops them from an Mi-17 at a
// drop_path_start path; until the vehicle layer exists they walk in from the
// farthest location).
spawn_bosses( refs, scale )
{
	level.survival_bosses = [];
	for ( i = 0; i < refs.size; i++ )
	{
		ref = refs[ i ];
		if ( !isSubStr( ref, "jug_" ) )
			continue;
		loc = maps\_survival_squad::farthest_location();
		if ( !isDefined( loc ) )
			continue;
		ai = undefined;
		for ( tries = 0; !isDefined( ai ) && tries < 15; tries++ )
		{
			ai = spawn_type( ref, loc.origin, loc.angles, scale );
			if ( !isDefined( ai ) )
				wait( 1 );
		}
		if ( !isDefined( ai ) )
			continue;
		ai.survival_boss = true;
		level.survival_bosses[ level.survival_bosses.size ] = ai;
		ai thread hunt();
		codolprintln( "CoDOL survival: boss " + ref + " spawned" );
		wait( 0.5 );
	}
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
