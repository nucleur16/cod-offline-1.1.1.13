// coop mission layer: the campaign / Spec Ops level, driven by the map's own
// entities.
//
// WHAT THIS IS.  CoDOL's coop missions are IW4 Spec Ops missions authored in
// the SINGLEPLAYER namespace on top of the shared mp_* levels: each coop
// playlist zone ships one addon_map_ents overlay (spawners, triggers, flags,
// objective points; docs/CODOL_CAMPAIGN_MODE.md section 3) and NO level
// script -- the IW4 so_* scripts that drove those entities are not in any
// CoDOL zone.  This module is the server's own generic reading of the IW4 SP
// entity contract, so a coop match plays from the entities alone:
//
//   * actor_enemy_<faction>_<role>_so spawners are spawned as native coop
//     soldiers (species "soldier", aitype/enemy_coop_soldier.gsc);
//   * IW4 _pmc.gsc's ELIMINATION rules run on every map that authors
//     targetname "pmc_spawner" spawners (coop_spec_mp_crash_tropical,
//     coop_spec_mp_estate_tropical, coop_spec_mp_fav_tropical): keep up to
//     level.coop_max_alive enemies alive, spawned out of sight of the
//     players, until level.coop_kills_to_win are dead;
//   * IW4 maps\_spawner / maps\_load trigger semantics run everywhere: a
//     trigger whose target names actor spawners spawns them when a player
//     touches it (trigger_multiple_spawn and friends); a
//     trigger_*_flag_set* sets its script_flag; script_flag_true /
//     script_flag_false gate a trigger; a spawner with script_flag_wait
//     spawns when its flag is set;
//   * a flag-set trigger whose flag names a spawner group releases that group
//     (take fav combat_zone_<n>); numbered obj_<n>_* flags are an objective
//     chain (take fav);
//   * the mission's main phase is, in order of preference: elimination (pmc
//     maps), the objective chain, the trigger chains, group waves, the pit
//     course (killhouse maps); after it, laptop objectives (_coopevents) and
//     the map's exit trigger (evac / escape flag) if it has one; it ends in
//     failure when every player is dead with no lives.
//
// MP ISOLATION.  This file is a coop module (maps/, the IW4 singleplayer
// namespace) named only by _coopmode.gsc, so it exists in no mp or zm match.  It names no
// multiplayer module (no #include of maps\mp\_utility, common_scripts\utility
// or any gametype file) -- every helper below is local or a builtin.
//
// References: D:\CoD\COD-GSC-Source\IW4-GSC\maps\_pmc.gsc (elimination:
// MIN_SPAWN_DISTANCE 1024, MAX_ENEMIES_ALIVE_ELIMINATION 25 -- capped lower
// here because the native actor pool has 32 slots, SEEK_PLAYERS_ENEMY_COUNT
// 6, the spawner pick away from the players), maps\_spawner.gsc and
// maps\_load.gsc (trigger_multiple_spawn, flag_set triggers,
// script_flag_wait), maps\_utility.gsc (flag_init / flag_set / flag_wait).

#using_animtree ( "generic_human" );

// --------------------------------------------------------------------------
// Flags (IW4 maps\_utility flag_init / flag / flag_set / flag_wait, on a
// level array; the notify carries the flag name, as IW4's does).
// --------------------------------------------------------------------------

flag_init( name )
{
	if ( !isDefined( level.coop_flag ) )
		level.coop_flag = [];
	if ( !isDefined( level.coop_flag[ name ] ) )
		level.coop_flag[ name ] = false;
}

flag( name )
{
	if ( !isDefined( level.coop_flag ) || !isDefined( level.coop_flag[ name ] ) )
		return false;
	return level.coop_flag[ name ];
}

flag_set( name )
{
	flag_init( name );
	if ( level.coop_flag[ name ] )
		return;
	level.coop_flag[ name ] = true;
	codolprintln( "CoDOL coop mission: flag " + name + " set" );
	level notify( name );
	level notify( "coop_flag_changed" );
}

flag_wait( name )
{
	flag_init( name );
	while ( !level.coop_flag[ name ] )
		level waittill( name );
}

// --------------------------------------------------------------------------
// Small helpers (no shared utility module may be included; see above).
// --------------------------------------------------------------------------

starts_with( text, prefix )
{
	if ( !isDefined( text ) || text.size < prefix.size )
		return false;
	return getSubStr( text, 0, prefix.size ) == prefix;
}

living_players()
{
	alive = [];
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isAlive( players[ i ] ) && isDefined( players[ i ].sessionstate ) &&
		     players[ i ].sessionstate == "playing" )
			alive[ alive.size ] = players[ i ];
	}
	return alive;
}

nearest_player_distance( origin )
{
	best = undefined;
	players = living_players();
	for ( i = 0; i < players.size; i++ )
	{
		d = distance( origin, players[ i ].origin );
		if ( !isDefined( best ) || d < best )
			best = d;
	}
	return best;
}

shuffle( list )
{
	for ( i = list.size - 1; i > 0; i-- )
	{
		j = randomInt( i + 1 );
		tmp = list[ i ];
		list[ i ] = list[ j ];
		list[ j ] = tmp;
	}
	return list;
}

// --------------------------------------------------------------------------
// Spawner classification.  The IW4 classname is the whole aitype contract:
// actor_enemy_<faction>_<role>_so.  Case varies across the shipped addons
// (actor_enemy_favela_AR_so, actor_enemy_favela_ar_so), so it is lowered.
// --------------------------------------------------------------------------

is_enemy_spawner( ent )
{
	if ( !isDefined( ent ) || !isDefined( ent.classname ) )
		return false;
	cls = toLower( ent.classname );
	if ( !starts_with( cls, "actor_enemy_" ) )
		return false;
	// No coop species exists yet for dogs (actor_enemy_dog_so) or for the
	// scripted civilian hostages (actor_enemy_vazquez_civilian_so); those
	// spawners stay unspawned and are listed in the doc's "left" table.
	if ( isSubStr( cls, "_dog" ) || isSubStr( cls, "civilian" ) )
		return false;
	// The runtime soldier template itself (spawnactorspawner) is not an
	// authored spawner: the group-wave scan once made it a wave of its own.
	if ( cls == "actor_enemy_coop_soldier" || isDefined( ent.coop_template ) )
		return false;
	return true;
}

soldier_kind( ent )
{
	cls = toLower( ent.classname );
	kind = spawnStruct();
	kind.faction = "militia";
	kind.role = "ar";
	kind.accuracy = 0.3;
	kind.health = 150;

	if ( isSubStr( cls, "desert_elite" ) )
	{
		kind.faction = "shadow";
		kind.health = 200;
		kind.accuracy = 0.35;
	}

	if ( isSubStr( cls, "_smg" ) )
		kind.role = "smg";
	else if ( isSubStr( cls, "_lmg" ) )
		kind.role = "lmg";
	else if ( isSubStr( cls, "_shgn" ) )
		kind.role = "shgn";
	else if ( isSubStr( cls, "_snpr" ) )
		kind.role = "snpr";
	else if ( isSubStr( cls, "makarov" ) || isSubStr( cls, "deserteagle" ) ||
	          isSubStr( cls, "villian" ) || isSubStr( cls, "vazquez" ) )
		kind.role = "pistol";

	if ( isSubStr( cls, "juggernaut" ) || ( isDefined( ent.script_noteworthy ) && ent.script_noteworthy == "juggernaut" ) )
	{
		kind.faction = "juggernaut";
		kind.role = "lmg";
		// IW4 maps\_juggernaut gives the juggernaut 3600 health behind armour
		// that soaks most of it; one number stands in for both here.
		kind.health = 1500;
		kind.accuracy = 0.25;
	}

	// IW4 authors script_accuracy on a spawner to override the aitype's.
	if ( isDefined( ent.script_accuracy ) )
		kind.accuracy = ent.script_accuracy;

	return kind;
}

// --------------------------------------------------------------------------
// The soldier itself.
// --------------------------------------------------------------------------

make_template()
{
	if ( isDefined( level.coop_soldier_template ) )
		return true;

	// The aitype's own spawner() runs on this template (setspawnerteam), so
	// spawnactor has authored fields to duplicate -- the zm zombie and mp dog
	// build theirs the same way.  -1 is the unlimited count.
	level.coop_soldier_template = spawnActorSpawner( "actor_enemy_coop_soldier", ( 0, 0, 0 ), ( 0, 0, 0 ), -1 );
	if ( !isDefined( level.coop_soldier_template ) )
	{
		codolprintln( "CoDOL coop mission: spawnactorspawner produced no soldier template; no enemies this mission" );
		return false;
	}
	level.coop_soldier_template.targetname = "coop_soldier_template";
	return true;
}

// A spawner of an authored classname, built once per classname and team: an
// unlimited copy (count -1) of what the map authors, so spawnactor runs that
// classname's aitype -- aitype/<classname without actor_>.gsc, which the
// engine loaded and precached at level load -- exactly as IW4 spawns from the
// authored spawner itself.  The missions spawn from these rather than from the
// authored entities because they reuse a spawner many times (waves, pmc
// respawns) and place the soldier themselves (forceTeleport).  `team`
// overrides the aitype spawner()'s team (Vazquez and the civilians fight on
// the players' side).  Undefined when the classname has no loaded aitype.
template_for( cls, team )
{
	if ( !isDefined( cls ) )
		return undefined;
	key = toLower( cls );
	if ( isDefined( team ) )
		key = key + "@" + team;
	if ( !isDefined( level.coop_templates ) )
	{
		level.coop_templates = [];
		level.coop_template_missing = [];
	}
	if ( isDefined( level.coop_templates[ key ] ) )
		return level.coop_templates[ key ];
	if ( isDefined( level.coop_template_missing[ key ] ) )
		return undefined;
	t = spawnActorSpawner( toLower( cls ), ( 0, 0, 0 ), ( 0, 0, 0 ), -1 );
	if ( !isDefined( t ) )
	{
		level.coop_template_missing[ key ] = true;
		codolprintln( "CoDOL coop mission: no aitype template for " + cls + "; the generic soldier stands in" );
		return undefined;
	}
	if ( isDefined( team ) )
		t setSpawnerTeam( team );
	// Not an authored spawner: every scan for those skips it (is_enemy_spawner).
	t.coop_template = true;
	t.targetname = "coop_template_" + key;
	level.coop_templates[ key ] = t;
	return t;
}

// A mission's own cast member (Ghost, Price, Vazquez, Ortiz, a civilian): an
// actor of the authored classname `cls`, so his aitype and character decide
// his look, with the mission's `kind` (aitype\enemy_coop_soldier::soldier_setup
// lists its fields) and `team`.  Falls back to the generic soldier template
// when the classname's aitype did not load.  Returns the actor or undefined;
// the caller places him.
spawn_class_actor( cls, team, kind )
{
	if ( !isDefined( kind ) )
		kind = spawnStruct();
	if ( isDefined( team ) )
		kind.team = team;
	template = template_for( cls, team );
	if ( !isDefined( template ) )
	{
		if ( isDefined( team ) && team == "allies" )
			template = maps\_cooptraining::ally_template();
		else if ( make_template() )
			template = level.coop_soldier_template;
		if ( !isDefined( template ) )
			return undefined;
		// character_coop_soldier downgrades this to what the zones carry.
		if ( !isDefined( kind.faction ) || kind.faction == "cast" )
			kind.faction = "militia";
		if ( !isDefined( kind.role ) )
			kind.role = "pistol";
	}
	level.coop_soldier_next = kind;
	ai = template spawnActor();
	level.coop_soldier_next = undefined;
	return ai;
}

// What an authored spawner asks of its soldier beyond its aitype (IW4 honours
// both keys on the spawner): script_accuracy and script_char_index.
spawner_overrides( spawner )
{
	kind = spawnStruct();
	if ( isDefined( spawner.script_accuracy ) )
		kind.accuracy = float( spawner.script_accuracy );
	if ( isDefined( spawner.script_char_index ) )
		kind.char_index = int( spawner.script_char_index );
	return kind;
}

// Spawns one soldier where the authored spawner stands.  Returns the actor, or
// undefined when the actor budget or the native spawn refuses.
spawn_soldier( spawner )
{
	if ( level.coop_mission_over )
		return undefined;
	if ( level.coop_alive >= level.coop_max_alive )
		return undefined;
	if ( !make_template() )
		return undefined;

	// The authored classname's own aitype (IW4: the spawner's aitype main()
	// and its character), through a template of that classname; the generic
	// template with the classname's faction and role only when that aitype
	// did not load.
	template = template_for( spawner.classname );
	if ( isDefined( template ) )
		level.coop_soldier_next = spawner_overrides( spawner );
	else
	{
		template = level.coop_soldier_template;
		level.coop_soldier_next = soldier_kind( spawner );
	}
	ai = template spawnActor();
	level.coop_soldier_next = undefined;

	if ( !isDefined( ai ) )
	{
		level.coop_spawn_failures++;
		return undefined;
	}

	angles = spawner.angles;
	if ( !isDefined( angles ) )
		angles = ( 0, randomInt( 360 ), 0 );
	// A collision-checked placement: an authored SP spawner can stand inside
	// the MP level's geometry, and the native placement moves the soldier to
	// clear floor on the near side of it (or reports that it could not).
	if ( !ai forceTeleport( spawner.origin, angles ) )
		codolprintln( "CoDOL coop mission: spawner at " + spawner.origin + " has no clear floor nearby; soldier ent " +
			ai getEntityNumber() + " left at " + ai.origin );
	ai show();

	ai.coop_spawner = spawner;
	ai.targetname = "coop_enemy";
	if ( isDefined( spawner.script_noteworthy ) )
		ai.script_noteworthy = spawner.script_noteworthy;

	ai arm_soldier();

	// The bootstrap pose: the animscripts' own main() runs too early to
	// animate (soldier_init.gsc), so the first pose is set here.
	ai setAnimKnobAll( %exposed_aim_5, %body, 1, 0.1, 1 );

	level.coop_alive++;
	level.coop_spawned++;
	level.coop_enemies[ level.coop_enemies.size ] = ai;

	ai thread soldier_death_watch();
	ai thread soldier_think();
	return ai;
}

// The weapon in his hands: the world model on tag_weapon_right (its
// tag_flash is where C1 draws the muzzle flash of his fire), and the weapon
// named in his entity state (codol_actorSetWeapon), which C1 reads for every
// fire event on him.  On by default; +set scr_coop_ai_weapon_models 0 hides
// the model.  (It was off while attaching weapon_cheytac failed as "not
// precached" after a map change; that was a native model-slot clobber, now
// fixed -- docs/CODOL_CAMPAIGN_MODE.md, coop AI combat.)  Mission scripts
// that spawn their own cast call this too.
arm_soldier()
{
	if ( !isDefined( self.coop_weapon ) || self.coop_weapon == "" )
		return;
	self codol_actorSetWeapon( self.coop_weapon );
	if ( getDvar( "scr_coop_ai_weapon_models" ) == "0" || isDefined( self.coop_weapon_model ) )
		return;
	model = getWeaponModel( self.coop_weapon );
	if ( isDefined( model ) && model != "" && isDefined( level.coop_weapon_models ) &&
	     isDefined( level.coop_weapon_models[ model ] ) )
	{
		self attach( model, "tag_weapon_right" );
		self.coop_weapon_model = model;
	}
}

// One round from an actor's own weapon (codol_actorShoot), for the mission
// scripts' scripted fire: the round, the fire sound and the muzzle flash on
// the actor, where magicBullet gave only the round.
actor_fire_round( weapon, start, end )
{
	if ( !isDefined( weapon ) || weapon == "" )
		return;
	self codol_actorShoot( weapon, start, end );
}

soldier_death_watch()
{
	self waittill( "death", attacker );

	level.coop_alive--;
	level.coop_killed++;

	if ( isDefined( attacker ) && isPlayer( attacker ) )
	{
		// int(): .kills and .score are engine client fields that read back
		// as floats here, and ++ on a float is a fatal runtime error
		// (MEASURED: "++ must be applied to an int (applied to float)").
		attacker.kills = int( attacker.kills ) + 1;
		attacker.score = int( attacker.score ) + 100;
	}

	// Leave the corpse its weapon model; the native corpse keeps the DObj.
	level notify( "coop_enemy_died" );
}

// IW4 _pmc enemy_set_goal_when_player_spotted / enemy_seek_player, reduced:
// a soldier holds near where he spawned until he has seen someone, then the
// native chase owns him; a "hunter" (one in three, IW4's randomint(3) == 0,
// and every soldier once few are left) walks toward the players meanwhile.
soldier_think()
{
	self endon( "death" );

	// A spawner a mission script drives itself (coop_no_hunt, e.g. the take
	// estate stealth patrols) is left to it: no hunting.
	if ( isDefined( self.coop_spawner ) && isDefined( self.coop_spawner.coop_no_hunt ) )
		return;

	self.goalradius = 450;
	hunter = ( randomInt( 3 ) == 0 );

	for ( ;; )
	{
		if ( level.coop_mission_over )
			return;

		if ( !isDefined( self.enemy ) && ( hunter || level.coop_seek_players ) )
		{
			players = living_players();
			if ( players.size > 0 )
			{
				target = players[ randomInt( players.size ) ];
				self.coop_hunting = true;
				self setGoalPos( target.origin );
			}
		}

		wait( 6 + randomFloat( 6 ) );
	}
}

// --------------------------------------------------------------------------
// Elimination (IW4 _pmc.gsc mode_elimination).
// --------------------------------------------------------------------------

pick_pmc_spawner()
{
	best = undefined;
	fallback = undefined;
	candidates = shuffle( level.coop_pmc_spawners );
	for ( i = 0; i < candidates.size; i++ )
	{
		spawner = candidates[ i ];
		if ( isDefined( spawner.coop_used ) )
			continue;
		d = nearest_player_distance( spawner.origin );
		if ( !isDefined( d ) )
			return spawner;
		// IW4 _pmc MIN_SPAWN_DISTANCE: never in the players' laps.
		if ( d >= level.coop_min_spawn_distance )
			return spawner;
		if ( !isDefined( fallback ) || d > fallback.coop_dist )
		{
			fallback = spawner;
			fallback.coop_dist = d;
		}
	}
	return fallback;
}

run_elimination()
{
	level endon( "coop_mission_over" );

	remaining_spawners = level.coop_pmc_spawners.size;
	level.coop_kills_to_win = getDvarInt( "scr_coop_enemies" );
	if ( level.coop_kills_to_win <= 0 )
		level.coop_kills_to_win = 20;
	if ( level.coop_kills_to_win > remaining_spawners )
		level.coop_kills_to_win = remaining_spawners;

	codolprintln( "CoDOL coop mission: elimination, " + level.coop_kills_to_win + " enemies from " +
		level.coop_pmc_spawners.size + " pmc spawners, " + level.coop_max_alive + " alive at once" );
	announce( "Eliminate all enemies: " + level.coop_kills_to_win );

	to_spawn = level.coop_kills_to_win;
	for ( ;; )
	{
		remaining = level.coop_kills_to_win - level.coop_killed;
		if ( remaining <= 0 )
		{
			level thread finish_after_elimination( "all enemies eliminated" );
			return;
		}

		// IW4 SEEK_PLAYERS_ENEMY_COUNT: the last few come looking.
		if ( remaining <= 6 && !level.coop_seek_players )
		{
			level.coop_seek_players = true;
			announce( remaining + " enemies remaining" );
			level thread mark_remaining_enemies();
		}

		while ( to_spawn > 0 && level.coop_alive < level.coop_max_alive )
		{
			spawner = pick_pmc_spawner();
			if ( !isDefined( spawner ) )
				break;
			spawner.coop_used = true;
			ai = spawn_soldier( spawner );
			if ( isDefined( ai ) )
				to_spawn--;
			wait( 0.1 );
		}

		// Spawners that refused (actor budget) never hand out kills, so a
		// mission cannot hang waiting for soldiers that will never exist.
		if ( to_spawn > 0 && !isDefined( pick_pmc_spawner() ) )
		{
			level.coop_kills_to_win = level.coop_killed + level.coop_alive;
			to_spawn = 0;
		}

		level waittill_any_timeout( "coop_enemy_died", 2 );
	}
}

waittill_any_timeout( note, timeout )
{
	self endon( note );
	wait( timeout );
}

// IW4 _pmc marks the last enemies on the compass.
mark_remaining_enemies()
{
	level endon( "coop_mission_over" );
	for ( ;; )
	{
		for ( i = 0; i < 6; i++ )
			objective_delete( 1 + i );

		slot = 0;
		for ( i = 0; i < level.coop_enemies.size && slot < 6; i++ )
		{
			ai = level.coop_enemies[ i ];
			if ( !isDefined( ai ) || !isAlive( ai ) )
				continue;
			objective_add( 1 + slot, "active", ai.origin );
			objective_onEntity( 1 + slot, ai );
			slot++;
		}

		level waittill_any_timeout( "coop_enemy_died", 3 );
	}
}

// --------------------------------------------------------------------------
// IW4 SP trigger / spawner semantics (maps\_spawner, maps\_load).
// --------------------------------------------------------------------------

trigger_spawners( trigger )
{
	spawners = [];
	if ( !isDefined( trigger.target ) )
		return spawners;
	targets = getEntArray( trigger.target, "targetname" );
	for ( i = 0; i < targets.size; i++ )
	{
		if ( is_enemy_spawner( targets[ i ] ) )
			spawners[ spawners.size ] = targets[ i ];
	}
	return spawners;
}

is_flag_trigger( trigger )
{
	if ( !isDefined( trigger.script_flag ) )
		return false;
	cls = toLower( trigger.classname );
	if ( isSubStr( cls, "flag_set" ) )
		return true;
	// IW4 also accepts a plain trigger whose targetname is "flag_set".
	return isDefined( trigger.targetname ) && trigger.targetname == "flag_set";
}

trigger_wait_player( trigger )
{
	for ( ;; )
	{
		trigger waittill( "trigger", who );
		if ( isDefined( who ) && isPlayer( who ) )
			return who;
	}
}

trigger_think( trigger, spawners, setsFlag )
{
	level endon( "coop_mission_over" );

	if ( isDefined( trigger.script_flag_true ) )
		flag_wait( trigger.script_flag_true );

	for ( ;; )
	{
		who = trigger_wait_player( trigger );

		// IW4: a trigger whose script_flag_false flag is set does nothing.
		if ( isDefined( trigger.script_flag_false ) && flag( trigger.script_flag_false ) )
		{
			wait( 0.5 );
			continue;
		}

		if ( setsFlag )
			flag_set( trigger.script_flag );

		if ( spawners.size > 0 )
		{
			codolprintln( "CoDOL coop mission: " + trigger.classname + " at " + trigger.origin +
				" spawns " + spawners.size );
			for ( i = 0; i < spawners.size; i++ )
				level thread spawner_spawn_count( spawners[ i ] );
			// IW4 trigger_multiple_spawn and spawn triggers fire once.
			return;
		}

		if ( !setsFlag )
			return;
		wait( 0.5 );
	}
}

// An IW4 spawner's count is how many soldiers it hands out (default 1).
spawner_spawn_count( spawner )
{
	level endon( "coop_mission_over" );

	if ( isDefined( spawner.coop_spawning ) )
		return;
	spawner.coop_spawning = true;

	if ( isDefined( spawner.script_flag_wait ) )
		flag_wait( spawner.script_flag_wait );

	count = 1;
	if ( isDefined( spawner.count ) && spawner.count > 1 )
		count = spawner.count;

	while ( count > 0 )
	{
		ai = spawn_soldier( spawner );
		if ( isDefined( ai ) )
		{
			count--;
			wait( 0.2 );
			continue;
		}
		if ( level.coop_mission_over )
			return;
		// Budget full: wait for a slot rather than lose the soldier.
		level waittill_any_timeout( "coop_enemy_died", 2 );
	}
	spawner.coop_spent = true;
	level notify( "coop_spawner_spent" );
}

// --------------------------------------------------------------------------
// Flag-named spawner groups.  A flag-set trigger whose script_flag equals an
// enemy spawner group's targetname releases that group when the flag is set
// (MEASURED, coop_take_mp_fav_tropical: triggers set combat_zone_10 .. _60
// and the spawners of each zone carry targetname combat_zone_<n>; the IW4
// level script that joined them does not ship, the shared name is the link).
// --------------------------------------------------------------------------

setup_flag_groups()
{
	level.coop_flag_groups = 0;
	if ( !isDefined( level.coop_flag ) )
		return;
	ents = getEntArray();
	groups = [];
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( !is_enemy_spawner( e ) || !isDefined( e.targetname ) )
			continue;
		if ( !isDefined( level.coop_flag[ e.targetname ] ) )
			continue;
		if ( isDefined( e.targetname ) && e.targetname == "pmc_spawner" )
			continue;
		if ( !isDefined( groups[ e.targetname ] ) )
		{
			groups[ e.targetname ] = true;
			level.coop_flag_groups++;
			level thread flag_group_think( e.targetname );
		}
	}
	if ( level.coop_flag_groups > 0 )
		codolprintln( "CoDOL coop mission: " + level.coop_flag_groups + " spawner groups released by their own flag" );
}

flag_group_think( name )
{
	level endon( "coop_mission_over" );
	flag_wait( name );
	spawners = getEntArray( name, "targetname" );
	count = 0;
	for ( i = 0; i < spawners.size; i++ )
	{
		if ( !is_enemy_spawner( spawners[ i ] ) )
			continue;
		level thread spawner_spawn_count( spawners[ i ] );
		count++;
	}
	codolprintln( "CoDOL coop mission: flag " + name + " released its group (" + count + " spawners)" );
}

// --------------------------------------------------------------------------
// Numbered objective chain.  Flag-set triggers named obj_<n>_arrive /
// obj_<n>_complete (coop_take_mp_fav_tropical: obj_40_arrive, obj_45_arrive,
// obj_47_arrive, obj_50_complete, obj_75_arrive) are the mission's waypoints
// in number order: each is a compass objective until its flag is set.
// --------------------------------------------------------------------------

setup_objective_chain()
{
	level.coop_obj_chain = [];
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( !isDefined( e.classname ) || !starts_with( e.classname, "trigger_" ) || !isDefined( e.script_flag ) )
			continue;
		tokens = strTok( e.script_flag, "_" );
		if ( tokens.size < 3 || tokens[ 0 ] != "obj" )
			continue;
		n = int( tokens[ 1 ] );
		if ( n <= 0 )
			continue;
		e.coop_obj_number = n;
		// Insertion sort by number.
		chain = [];
		placed = false;
		for ( j = 0; j < level.coop_obj_chain.size; j++ )
		{
			if ( !placed && n < level.coop_obj_chain[ j ].coop_obj_number )
			{
				chain[ chain.size ] = e;
				placed = true;
			}
			chain[ chain.size ] = level.coop_obj_chain[ j ];
		}
		if ( !placed )
			chain[ chain.size ] = e;
		level.coop_obj_chain = chain;
	}
}

run_objective_chain()
{
	level endon( "coop_mission_over" );
	chain = level.coop_obj_chain;
	codolprintln( "CoDOL coop mission: objective chain of " + chain.size );
	for ( i = 0; i < chain.size; i++ )
	{
		t = chain[ i ];
		objective_add( 0, "active", t.origin );
		announce( "Objective " + ( i + 1 ) + " of " + chain.size );
		codolprintln( "CoDOL coop mission: objective " + ( i + 1 ) + " is " + t.script_flag + " at " + t.origin );
		flag_wait( t.script_flag );
		objective_delete( 0 );
	}
	level thread finish_after_elimination( "objective chain complete" );
}

setup_entity_driven()
{
	level.coop_chain_spawners = [];

	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		ent = ents[ i ];
		if ( !isDefined( ent.classname ) )
			continue;
		cls = toLower( ent.classname );

		if ( starts_with( cls, "trigger_" ) )
		{
			spawners = trigger_spawners( ent );
			setsFlag = is_flag_trigger( ent );
			if ( setsFlag )
				flag_init( ent.script_flag );
			if ( spawners.size > 0 || setsFlag )
			{
				for ( j = 0; j < spawners.size; j++ )
					level.coop_chain_spawners[ level.coop_chain_spawners.size ] = spawners[ j ];
				level.coop_trigger_count++;
				level thread trigger_think( ent, spawners, setsFlag );
			}
			continue;
		}

		if ( is_enemy_spawner( ent ) )
		{
			level.coop_spawner_count++;
			if ( isDefined( ent.targetname ) && ent.targetname == "pmc_spawner" )
			{
				level.coop_pmc_spawners[ level.coop_pmc_spawners.size ] = ent;
				continue;
			}
			// A flag-gated spawner no trigger targets spawns when its flag is.
			if ( isDefined( ent.script_flag_wait ) )
			{
				level.coop_chain_spawners[ level.coop_chain_spawners.size ] = ent;
				level thread spawner_spawn_count( ent );
			}
		}
	}
}

// --------------------------------------------------------------------------
// End conditions.
// --------------------------------------------------------------------------

// Generic completion flags.  NOT player_has_escaped or the evac flag on their
// own: those are set by triggers a player can walk into at any time
// (MEASURED, coop_spec_mp_estate_tropical: the player_has_escaped /
// player_trying_to_escape volumes are interior boxes up to 2500 units wide
// around the house, one of them containing the estate_player_start, so a
// player ended the mission seconds after spawning).  They are not IW4's
// out-of-bounds ring either (enable_escape_warning's triggers ring the map
// edge; these do not), so they are read as the mission's EXIT: the
// extraction phase after the main objective, see finish_after_elimination.
end_flags()
{
	names = [];
	names[ names.size ] = "mission_complete";
	names[ names.size ] = "level_complete";
	return names;
}

// Every elimination enemy is dead.  On a map that authors laptop objectives
// (IW4 _pmc mode_objective: script_model pmc_objective + trigger_use) those
// must be done too; on a map that authors an evac trigger (a flag-set trigger
// whose flag names evac / extract) the squad then has to reach it.
finish_after_elimination( reason )
{
	level endon( "coop_mission_over" );

	while ( isDefined( level.coop_objectives_left ) && level.coop_objectives_left > 0 )
	{
		announce( "Enemies cleared: complete the objectives (" + level.coop_objectives_left + " left)" );
		level waittill( "coop_objective_done" );
	}

	evac = find_extraction();
	if ( isDefined( evac ) )
	{
		name = evac.script_flag;
		level.coop_flag[ name ] = false;
		announce( "Area clear. Get to the extraction point" );
		objective_add( 0, "active", evac.origin );
		codolprintln( "CoDOL coop mission: extraction at " + evac.origin + " (flag " + name + ")" );
		flag_wait( name );
		objective_delete( 0 );
		mission_complete( reason + ", extracted" );
		return;
	}

	mission_complete( reason );
}

// The exit trigger: an evac / extract flag first (estate spec
// reach_1st_evac_point), else the escape flag (player_has_escaped on estate
// spec and fav spec, player_escape on take estate).
find_extraction()
{
	words = [];
	words[ 0 ] = "evac";
	words[ 1 ] = "extract";
	words[ 2 ] = "has_escaped";
	words[ 3 ] = "player_escape";
	ents = getEntArray();
	for ( w = 0; w < words.size; w++ )
	{
		for ( i = 0; i < ents.size; i++ )
		{
			e = ents[ i ];
			if ( !isDefined( e.classname ) || !starts_with( e.classname, "trigger_" ) || !isDefined( e.script_flag ) )
				continue;
			if ( isSubStr( toLower( e.script_flag ), words[ w ] ) )
				return e;
		}
	}
	return undefined;
}

watch_end_flag( name )
{
	level endon( "coop_mission_over" );
	flag_wait( name );
	mission_complete( "end trigger (" + name + ")" );
}

// --------------------------------------------------------------------------
// Group waves: the fallback for a map whose enemies neither come from pmc
// spawners nor hang off a trigger.  coop_defense_mp_crash_tropical,
// coop_take_mp_estate_tropical and coop_take_mp_fav_tropical spawn theirs
// from their (unshipped) so_* level scripts by targetname group
// (group_wave_1, reinforce_midhouse, combat_zone_30, ...).  Without that
// script the server spawns each authored group as one wave, the group nearest
// the players first, and the mission is complete when the last is cleared.
// --------------------------------------------------------------------------

collect_groups()
{
	groups = [];
	keys = [];
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		ent = ents[ i ];
		if ( !is_enemy_spawner( ent ) )
			continue;
		key = "unnamed";
		if ( isDefined( ent.targetname ) && ent.targetname != "" )
			key = ent.targetname;
		if ( !isDefined( groups[ key ] ) )
		{
			groups[ key ] = [];
			keys[ keys.size ] = key;
		}
		groups[ key ][ groups[ key ].size ] = ent;
	}
	level.coop_group_keys = keys;
	level.coop_groups = groups;
}

group_distance( key )
{
	best = undefined;
	spawners = level.coop_groups[ key ];
	for ( i = 0; i < spawners.size; i++ )
	{
		d = nearest_player_distance( spawners[ i ].origin );
		if ( isDefined( d ) && ( !isDefined( best ) || d < best ) )
			best = d;
	}
	if ( !isDefined( best ) )
		best = 999999;
	return best;
}

run_group_waves()
{
	level endon( "coop_mission_over" );

	collect_groups();
	remaining = level.coop_group_keys;
	codolprintln( "CoDOL coop mission: group waves, " + remaining.size + " groups" );
	announce( "Clear the area: " + remaining.size + " waves" );

	wave = 0;
	while ( remaining.size > 0 )
	{
		// The group nearest the players goes next.
		pick = 0;
		pickDist = group_distance( remaining[ 0 ] );
		for ( i = 1; i < remaining.size; i++ )
		{
			d = group_distance( remaining[ i ] );
			if ( d < pickDist )
			{
				pick = i;
				pickDist = d;
			}
		}
		key = remaining[ pick ];
		rest = [];
		for ( i = 0; i < remaining.size; i++ )
		{
			if ( i != pick )
				rest[ rest.size ] = remaining[ i ];
		}
		remaining = rest;

		wave++;
		spawners = level.coop_groups[ key ];
		codolprintln( "CoDOL coop mission: wave " + wave + " is group " + key + " (" + spawners.size + " spawners)" );
		for ( i = 0; i < spawners.size; i++ )
			level thread spawner_spawn_count( spawners[ i ] );

		// Cleared once every spawner of the group is spent and nobody is left.
		for ( ;; )
		{
			level waittill_any_timeout( "coop_enemy_died", 2 );
			spent = 0;
			for ( i = 0; i < spawners.size; i++ )
			{
				if ( isDefined( spawners[ i ].coop_spent ) )
					spent++;
			}
			if ( spent >= spawners.size && level.coop_alive <= 0 )
				break;
		}

		if ( remaining.size > 0 )
			announce( "Wave " + wave + " cleared" );
	}

	level thread finish_after_elimination( "every group wave cleared" );
}

// The killhouse maps author no enemies: the mission is the pit course
// (_coopcourse.gsc, IW4 so_killspree_trainer), complete when a run clears
// every course target.
run_course()
{
	level endon( "coop_mission_over" );
	announce( "Run the course: clear every target" );
	level waittill( "coop_course_complete" );
	mission_complete( "course cleared in " + level.coop_course_best + " s" );
}

// No pmc spawners: the mission is the map's trigger chains.  It is over once
// every chained spawner has spawned and every soldier is dead.
run_chain_completion()
{
	level endon( "coop_mission_over" );

	if ( level.coop_chain_spawners.size <= 0 )
		return;

	for ( ;; )
	{
		level waittill_any_timeout( "coop_enemy_died", 2 );
		if ( level.coop_spawned <= 0 || level.coop_alive > 0 )
			continue;
		spent = 0;
		for ( i = 0; i < level.coop_chain_spawners.size; i++ )
		{
			if ( isDefined( level.coop_chain_spawners[ i ].coop_spent ) )
				spent++;
		}
		if ( spent >= level.coop_chain_spawners.size )
		{
			mission_complete( "every spawner spent and cleared" );
			return;
		}
	}
}

announce( text )
{
	codolprintln( "CoDOL coop mission: " + text );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] iPrintLnBold( text );
}

// Log the English text for probes; players see the zone's localized string.
announce_localized( text, logtext )
{
	codolprintln( "CoDOL coop mission: " + logtext );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] iPrintLnBold( text );
}

mission_complete( reason )
{
	// Threaded: every caller endons "coop_mission_over", which mission_over
	// itself notifies, so run inline it would kill itself before the result.
	level thread mission_over( true, reason );
}

mission_failed( reason )
{
	level thread mission_over( false, reason );
}

mission_over( success, reason )
{
	if ( level.coop_mission_over )
		return;
	level.coop_mission_over = true;
	level.coop_mission_success = success;
	level notify( "coop_mission_over" );

	// Retail draws its localized banner, and none at all after the training
	// (that mission ends on Price's debrief and the fade).
	if ( success && isDefined( level.coop_training_flow ) && level.coop_training_flow )
		codolprintln( "CoDOL coop mission: MISSION COMPLETE" );
	else if ( success )
		announce_localized( &"SCRIPT_MISSION_COMPLETE", "MISSION COMPLETE" );
	else
		announce( "MISSION FAILED" );
	codolprintln( "CoDOL coop mission: over, success " + success + " (" + reason + "), " +
		level.coop_killed + " killed, " + level.coop_spawned + " spawned" );

	level thread end_level();
}

end_level()
{
	level.gameEnded = true;

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		players[ i ] closeMenu();
		players[ i ] closeInGameMenu();
		players[ i ] freezeControls( true );
	}

	delay = getDvarInt( "scr_coop_end_delay" );
	if ( delay <= 0 )
		delay = 10;
	wait( delay );

	report_mission_result();

	// exitLevel returns every player to the lobby (t "EXE_MATCHENDED", as an
	// mp match ends), writes the match report carrying the results above,
	// and -- in a coop match -- loads nothing further: the server waits for
	// the lobby's next map instead of replaying coop missions for nobody
	// (src/codol/script/codol_coop_progress.c).
	exitLevel( false );
}

// The mission's result for every player still in it, by playlist name
// (sv_sessionMap, e.g. coop_training_mp_killhouse_sh).  The match report
// carries it to the lobby, which records a completion in the profile and
// unlocks the next mission; a failure unlocks nothing.  IW4 Spec Ops credits
// every player in the mission, including one who is out of lives.
report_mission_result()
{
	mission = getDvar( "sv_sessionMap" );
	if ( mission == "" )
		mission = getDvar( "mapname" );
	success = isDefined( level.coop_mission_success ) && level.coop_mission_success;

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] codol_reportCoopMission( mission, success );
	codolprintln( "CoDOL coop mission: " + mission + " result " + success + " reported for " +
		players.size + " player(s); returning them to the lobby" );
}

// --------------------------------------------------------------------------
// Entry point, threaded from _coopmode::Callback_StartGameType.
// --------------------------------------------------------------------------

init()
{
	level.coop_mission_over = false;
	level.coop_mission_success = undefined;
	level.coop_alive = 0;
	level.coop_spawned = 0;
	level.coop_killed = 0;
	level.coop_spawn_failures = 0;
	level.coop_enemies = [];
	level.coop_pmc_spawners = [];
	level.coop_trigger_count = 0;
	level.coop_spawner_count = 0;
	level.coop_seek_players = false;
	level.coop_min_spawn_distance = 1024;

	// Retail shows the localized banner (localized_common_gametype_coop).
	precacheString( &"SCRIPT_MISSION_COMPLETE" );

	// The native actor pool is 32 slots (CODOL_MAX_ACTOR_SLOTS); IW4's 25
	// alive is a singleplayer engine number.
	level.coop_max_alive = getDvarInt( "scr_coop_max_alive" );
	if ( level.coop_max_alive <= 0 )
		level.coop_max_alive = 10;
	if ( level.coop_max_alive > 24 )
		level.coop_max_alive = 24;
}

main()
{
	init();
	setup_entity_driven();

	codolprintln( "CoDOL coop mission: " + getDvar( "mapname" ) + " (" + getDvar( "sv_sessionMap" ) + "): " +
		level.coop_spawner_count + " enemy spawners, " + level.coop_pmc_spawners.size + " pmc, " +
		level.coop_chain_spawners.size + " chained, " + level.coop_trigger_count + " triggers" );

	names = end_flags();
	for ( i = 0; i < names.size; i++ )
		level thread watch_end_flag( names[ i ] );

	// IW4 Spec Ops starts the clock when the first player is in the level.
	while ( living_players().size <= 0 )
		wait( 0.5 );
	wait( 3 );

	if ( !make_template() )
		return;

	// A mission script that releases its own spawner groups
	// (level.coop_mission_owns_groups) does not want them released by flag
	// name: take estate's guard_midhouse / reinforce_midhouse / patrol_prehouse
	// flags name groups that come only when its stealth breaks or a stage
	// begins.
	if ( !isDefined( level.coop_mission_owns_groups ) )
		setup_flag_groups();
	setup_objective_chain();

	// The training mission runs its own stations (_cooptraining.gsc).
	if ( isDefined( level.coop_training_flow ) && level.coop_training_flow )
		codolprintln( "CoDOL coop mission: the training flow runs this mission" );
	// A mission with its own script (_coopmission_<name>.gsc, selected by
	// sv_sessionMap) runs its own flow; the shared flag triggers, flag
	// groups, spawn helpers and end conditions still serve it.
	else if ( isDefined( level.coop_mission_flow ) )
		codolprintln( "CoDOL coop mission: the " + level.coop_mission_flow + " flow runs this mission" );
	else if ( level.coop_pmc_spawners.size > 0 )
		level thread run_elimination();
	else if ( level.coop_obj_chain.size >= 2 )
		level thread run_objective_chain();
	else if ( level.coop_chain_spawners.size > 0 )
		level thread run_chain_completion();
	else if ( level.coop_spawner_count > 0 )
		level thread run_group_waves();
	else if ( isDefined( level.coop_course_enemies ) && level.coop_course_enemies.size > 0 )
		level thread run_course();

	level thread watch_players_failed();
}

// Spec Ops fails when nobody is left standing.  _coopmode decides lives.
watch_players_failed()
{
	level endon( "coop_mission_over" );
	for ( ;; )
	{
		level waittill( "coop_player_out" );
		players = getPlayers();
		anyone = false;
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( isAlive( p ) || !isDefined( p.coop_out ) || !p.coop_out )
				anyone = true;
		}
		if ( !anyone )
			mission_failed( "every player is out of lives" );
	}
}

// _coopmode's CodeCallback_ActorKilled hook.
onActorKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset )
{
	if ( isDefined( self.coop_weapon_model ) )
		self.coop_weapon_model = undefined;
}
