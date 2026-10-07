// coop_training_mp_killhouse_sh: the scripted training mission.
//
// CoDOL's training mission is IW4's trainer (S.S.D.D.) rebuilt on
// mp_killhouse_sh with CoDOL's own cast and voice lines.  Its level script
// does not ship; this module rebuilds its progression from three sources:
//
//   * the map entities: the addon's actors (script_friendname Captain Price,
//     Tiny Man, Sgt. Newcastle, Mac, Colonel Harker, Soap, Ghost, Pvt. Lootz,
//     Sgt. Lovejoy and the camp's ambient soldiers, each with the idle
//     animation it is authored with), its flag triggers (inside_firing_range,
//     wait_line_up, start_course, crawl_dialog, obstacle_course_end,
//     come_to_price, open_ship_hanger, at_ssdd_hanger, ...) and the base
//     map's gates, doors, pop-up targets and course triggers;
//   * IW4 maps\trainer.gsc and maps\so_killspree_trainer.gsc for the shape of
//     each station and how its doors move (make_door_from_prefab, door_open:
//     a prefab door rotates about its script_origin; gate_cqb_enter_main
//     opens -170, gate_cqb_exit -95, others +90);
//   * the voice lines themselves: the order and station of every pit_* alias
//     below was established by listening to the shipped lines (a private
//     transcription; no line text is reproduced here).  Durations are the
//     shipped files' own lengths.
//
// Stations, in order:
//   0 the jeep ride into camp (Price, pit_pri_001..003) -- _coopintro
//   1 to the range (Price nags pit_pri_004..006 until inside_firing_range)
//   2 the range with Tiny Man: hip fire, aim down the sights, penetration,
//     timed targets, sidearm, knife (pit_tiny_001..025); the range's targets
//     are the base map's rifle / ADS dummies and the knife dummy
//   3 demolitions with Sgt. Newcastle: frags into the windows, the grenade
//     launcher at wall one and windows five to seven (pit_ncastle_001..014)
//   4 the obstacle course with Mac (pit_mac_001..008)
//   5 Price at the end of the course, then the hangar: Colonel Harker's
//     welcome and the pit briefing (pit_har_*, pit_pri_007..011, pit_mct,
//     pit_gst), the pit CQB course (_coopcourse.gsc) and Price's verdict
//   6 the cargoship deck mock-up with Price calling the positions
//     (pit_pri_012..044), his verdict, and the debrief at the monitors
//     (pit_pri_061..065), which completes the mission.
// The camp's soldiers trade the pit_rgr* chatter when a player walks past.
//
// Every task has a time-out so a player is never stuck behind a line the
// engine cannot judge (a grenade landing in a window, a flashbang thrown);
// the doors of each station open when the station is reached.
//
// Coop category root only; names no multiplayer module.

#using_animtree ( "generic_human" );

is_training()
{
	return getDvar( "codol_sessionMap" ) == "coop_training_mp_killhouse_sh";
}

// --------------------------------------------------------------------------
// The voice lines.  Durations (s) are the shipped files'.
// --------------------------------------------------------------------------

line( alias, time )
{
	l = spawnStruct();
	l.alias = alias;
	l.time = time;
	return l;
}

// The jeep: Price welcomes the rookie and sends him to Tiny Man.
ride_lines()
{
	level.coop_intro_lines = [];
	level.coop_intro_lines[ 0 ] = line( "pit_pri_001", 1.8 );
	level.coop_intro_lines[ 1 ] = line( "pit_pri_002_1", 2.4 );
	level.coop_intro_lines[ 2 ] = line( "pit_pri_002_2", 3.1 );
	level.coop_intro_lines[ 3 ] = line( "pit_pri_002_3", 4.5 );
	level.coop_intro_lines[ 4 ] = line( "pit_pri_003", 3.6 );
}

say( speaker, alias, time )
{
	if ( isDefined( speaker ) )
		speaker playSound( alias );
	codolprintln( "CoDOL coop training: line " + alias );
	if ( isDefined( time ) )
		wait( time + 0.3 );
}

// --------------------------------------------------------------------------
// Doors and gates.
// --------------------------------------------------------------------------

// IW4 make_door_from_prefab, per instance: a targetname may name several
// prefab copies (gate_cqb_exit2 has two), so every script_origin of the name
// is a door and every brush / model joins the nearest one.
make_doors( name )
{
	ents = getEntArray( name, "targetname" );
	doors = [];
	for ( i = 0; i < ents.size; i++ )
	{
		if ( ents[ i ].classname == "script_origin" )
		{
			d = ents[ i ];
			d.brushes = [];
			d.models = [];
			doors[ doors.size ] = d;
		}
	}
	if ( doors.size <= 0 )
		return doors;
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( e.classname == "script_origin" )
			continue;
		best = doors[ 0 ];
		for ( j = 1; j < doors.size; j++ )
		{
			if ( distanceSquared( e.origin, doors[ j ].origin ) < distanceSquared( e.origin, best.origin ) )
				best = doors[ j ];
		}
		if ( e.classname == "script_brushmodel" && isDefined( e.script_noteworthy ) && e.script_noteworthy == "blocker" )
		{
			best.blocker = e;
			continue;
		}
		if ( e.classname == "script_brushmodel" )
			best.brushes[ best.brushes.size ] = e;
		else if ( e.classname == "script_model" )
			best.models[ best.models.size ] = e;
		e linkTo( best );
	}
	return doors;
}

// IW4 trainer.gsc door_open.
open_doors( name, angles )
{
	doors = make_doors( name );
	if ( !isDefined( angles ) )
		angles = 90;
	for ( i = 0; i < doors.size; i++ )
	{
		d = doors[ i ];
		if ( isDefined( d.coop_open ) )
			continue;
		d.coop_open = true;
		d rotateTo( d.angles + ( 0, angles, 0 ), 2, 0.5, 0.5 );
		d playSound( "scn_training_fence_open" );
		if ( isDefined( d.blocker ) )
		{
			d.blocker hide();
			d.blocker notSolid();
		}
		for ( b = 0; b < d.brushes.size; b++ )
			d.brushes[ b ] notSolid();
	}
	codolprintln( "CoDOL coop training: opened " + doors.size + " door(s) " + name );
}

// The sliding gates (slide_gate_01/02/03): brush panels on wheels (script
// models).  A targetname may name gates far apart (slide_gate_03: the east
// road gate and the hangar yard gate), so only the parts near `nearPoint` are
// taken.  Each panel slides 320 units straight away from the wheels' centre
// along the gate's line; the hangar yard gate has its wheels between its two
// halves, so its halves part.  Which way a gate opens is not authored
// anywhere (INFERRED from the wheels, which sit where the gate closes).
open_slide_gate( name, nearPoint )
{
	ents = getEntArray( name, "targetname" );
	parts = [];
	for ( i = 0; i < ents.size; i++ )
	{
		if ( isDefined( nearPoint ) && distance( ents[ i ].origin, nearPoint ) > 900 )
			continue;
		parts[ parts.size ] = ents[ i ];
	}
	if ( parts.size <= 0 )
		return;

	wheels = ( 0, 0, 0 );
	nw = 0;
	for ( i = 0; i < parts.size; i++ )
	{
		if ( parts[ i ].classname == "script_model" )
		{
			wheels += parts[ i ].origin;
			nw++;
		}
	}
	if ( nw <= 0 )
		return;
	wheels = wheels / nw;

	moved = 0;
	for ( i = 0; i < parts.size; i++ )
	{
		p = parts[ i ];
		if ( p.classname != "script_brushmodel" || isDefined( p.coop_open ) )
			continue;
		d = p.origin - wheels;
		d = ( d[ 0 ], d[ 1 ], 0 );
		if ( length( d ) < 1 )
			continue;
		p.coop_open = true;
		p moveTo( p.origin + vectorNormalize( d ) * 320, 3, 0.5, 0.5 );
		p notSolid();
		moved++;
	}
	parts[ 0 ] playSound( "scn_training_fence_open" );
	codolprintln( "CoDOL coop training: slid gate " + name + " (" + moved + " panels) from wheels at " + wheels );
}

// Single brush doors with no hinge origin (ship_hanger_door,
// hanger_one_door, arsenal_door): raised out of the way.
raise_door( name, height )
{
	ents = getEntArray( name, "targetname" );
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( isDefined( e.coop_open ) )
			continue;
		e.coop_open = true;
		e moveTo( e.origin + ( 0, 0, height ), 2.5, 0.5, 0.5 );
		if ( e.classname == "script_brushmodel" )
			e notSolid();
	}
	codolprintln( "CoDOL coop training: raised " + ents.size + " part(s) of " + name );
}

// --------------------------------------------------------------------------
// The cast.
// --------------------------------------------------------------------------

// Body and head per authored ally classname; every model ships in the
// coop_training_mp_killhouse_sh zone (MEASURED, docs/zone_assets).
cast_models( cls )
{
	m = spawnStruct();
	cls = toLower( cls );
	if ( isSubStr( cls, "price" ) )
	{
		m.body = "body_desert_tf141_assault_a";
		m.head = "head_hero_price_desert";
	}
	else if ( isSubStr( cls, "soap" ) )
	{
		m.body = "body_hero_soap_socom";
		m.head = "head_hero_soap_soccom_a";
	}
	else if ( isSubStr( cls, "ghost_forest" ) )
	{
		m.body = "body_forest_tf141_ghost";
		m.head = "head_hero_ghost_forest";
	}
	else if ( isSubStr( cls, "ghost" ) )
	{
		m.body = "body_hero_ghost_socom";
		m.head = "head_hero_ghost_soccom";
	}
	else if ( isSubStr( cls, "harker" ) || isSubStr( cls, "socom_shotgun" ) )
	{
		m.body = "body_seal_soccom_assault_c_blk";
		m.head = "head_seal_soccom_d";
	}
	else if ( isSubStr( cls, "tf_141_forest_shotgun" ) )
	{
		m.body = "body_forest_tf141_shotgun";
		m.head = "head_tf141_forest_a";
	}
	else if ( isSubStr( cls, "tf_141_forest" ) )
	{
		m.body = "body_forest_tf141_smg";
		m.head = "head_tf141_forest_b";
	}
	else if ( isSubStr( cls, "sas_woodland" ) )
	{
		m.body = "body_sp_sas_woodland_assault_a";
		m.head = "head_sp_sas_woodland_mac";
	}
	else if ( isSubStr( cls, "blackkit" ) )
	{
		m.body = "body_complete_sp_sas_ct_benjamin";
	}
	else if ( isSubStr( cls, "ustshirt" ) )
	{
		m.body = "body_ustshirt_tf_m_clean_yel";
		m.head = "head_complete_tf_tan";
	}
	else
	{
		m.body = "body_complete_sp_lw";
		m.head = "head_complete_sp_lw";
	}
	return m;
}

precache_cast()
{
	models = [];
	models[ models.size ] = "body_desert_tf141_assault_a";
	models[ models.size ] = "head_hero_price_desert";
	models[ models.size ] = "body_hero_soap_socom";
	models[ models.size ] = "head_hero_soap_soccom_a";
	models[ models.size ] = "body_forest_tf141_ghost";
	models[ models.size ] = "head_hero_ghost_forest";
	models[ models.size ] = "body_hero_ghost_socom";
	models[ models.size ] = "head_hero_ghost_soccom";
	models[ models.size ] = "body_seal_soccom_assault_c_blk";
	models[ models.size ] = "head_seal_soccom_d";
	models[ models.size ] = "body_forest_tf141_shotgun";
	models[ models.size ] = "head_tf141_forest_a";
	models[ models.size ] = "body_forest_tf141_smg";
	models[ models.size ] = "head_tf141_forest_b";
	models[ models.size ] = "body_sp_sas_woodland_assault_a";
	models[ models.size ] = "head_sp_sas_woodland_mac";
	models[ models.size ] = "body_complete_sp_sas_ct_benjamin";
	models[ models.size ] = "body_ustshirt_tf_m_clean_yel";
	models[ models.size ] = "head_complete_tf_tan";
	models[ models.size ] = "body_complete_sp_lw";
	models[ models.size ] = "head_complete_sp_lw";
	for ( i = 0; i < models.size; i++ )
		precacheModel( models[ i ] );
}

// The authored idle animation of an ally spawner, as an animation value the
// soldier animscripts can play (soldier_stop.gsc reads self.coop_idle_anim).
// Every name below is a generic_human node with an xanim in the
// coop_training_mp_killhouse_sh zone (MEASURED).
idle_anim( name )
{
	if ( !isDefined( name ) )
		return undefined;
	switch ( toLower( name ) )
	{
	case "killhouse_gaz_idleb":
		return %killhouse_gaz_idleB;
	case "civilian_reader_2":
		return %civilian_reader_2;
	case "killhouse_sas_price_idle":
		return %killhouse_sas_price_idle;
	case "parabolic_leaning_guy_idle_training":
		return %parabolic_leaning_guy_idle_training;
	case "civilian_sitting_talking_a_2":
		return %civilian_sitting_talking_A_2;
	case "civilian_texting_standing":
		return %civilian_texting_standing;
	case "killhouse_price_stand_talk":
		return %killhouse_Price_stand_talk;
	case "civilian_sitting_talking_a_1":
		return %civilian_sitting_talking_A_1;
	case "training_humvee_repair":
		return %training_humvee_repair;
	case "civilian_sitting_talking_b_1":
		return %civilian_sitting_talking_B_1;
	case "bunker_toss_idle_guy1":
		return %bunker_toss_idle_guy1;
	case "training_pit_stand_idle":
		return %training_pit_stand_idle;
	case "cliffhanger_welder_engine":
		return %cliffhanger_welder_engine;
	case "sitting_guard_loadak_idle":
		return %sitting_guard_loadAK_idle;
	case "roadkill_humvee_map_sequence_quiet_idle":
		return %roadkill_humvee_map_sequence_quiet_idle;
	case "training_basketball_guy1":
		return %training_basketball_guy1;
	case "training_basketball_guy2":
		return %training_basketball_guy2;
	case "cliffhanger_welder_wing":
		return %cliffhanger_welder_wing;
	case "training_pushups_guy1":
		return %training_pushups_guy1;
	case "training_intro_trainee_1_end_idle":
		return %training_intro_trainee_1_end_idle;
	}
	return undefined;
}

ally_template()
{
	if ( isDefined( level.coop_ally_template ) )
		return level.coop_ally_template;
	t = spawnActorSpawner( "actor_enemy_coop_soldier", ( 0, 0, 0 ), ( 0, 0, 0 ), -1 );
	if ( !isDefined( t ) )
		return undefined;
	t setSpawnerTeam( "allies" );
	t.targetname = "coop_ally_template";
	level.coop_ally_template = t;
	return t;
}

// Spawn one cast member where his authored spawner stands: a friendly soldier
// of the coop species on the allies team that nobody can hurt and that never
// fights or walks.
spawn_cast( spawner, key )
{
	template = ally_template();
	if ( !isDefined( template ) )
		return undefined;

	kind = spawnStruct();
	kind.faction = "cast";
	kind.role = "pistol";
	kind.team = "allies";
	kind.health = 1000;
	kind.accuracy = 0;
	m = cast_models( spawner.classname );
	kind.body = m.body;
	kind.head = m.head;

	level.coop_soldier_next = kind;
	ai = template spawnActor();
	level.coop_soldier_next = undefined;
	if ( !isDefined( ai ) )
	{
		codolprintln( "CoDOL coop training: could not spawn cast member " + key );
		return undefined;
	}

	angles = spawner.angles;
	if ( !isDefined( angles ) )
		angles = ( 0, 0, 0 );
	ai forceTeleport( spawner.origin, angles );
	ai setCanDamage( false );
	ai.ignoreall = true;
	ai.ignoreme = true;
	ai.coop_cast = key;
	ai.coop_idle_anim = idle_anim( spawner.animation );
	if ( isDefined( spawner.script_friendname ) )
		ai.coop_name = spawner.script_friendname;
	if ( isDefined( ai.coop_idle_anim ) )
		ai setAnimKnobAll( ai.coop_idle_anim, %body, 1, 0.1, 1 );
	else
		ai setAnimKnobAll( %casual_stand_idle, %body, 1, 0.1, 1 );
	return ai;
}

// The cast, from the addon's ally spawners (script_noteworthy is the key the
// level script used for each one).  The native actor pool has 32 slots and
// this mission has no enemies, so up to 28 are placed: the named
// instructors first, then the camp's ambient soldiers.
spawn_cast_all()
{
	level.coop_cast = [];
	level.coop_cast_list = [];
	ents = getEntArray();
	wanted = [];
	order = [];
	order[ order.size ] = "waters";
	order[ order.size ] = "nwc";
	order[ order.size ] = "mac";
	order[ order.size ] = "price2";
	order[ order.size ] = "chair_guy";
	order[ order.size ] = "price";
	order[ order.size ] = "sas1";
	order[ order.size ] = "sas2";
	order[ order.size ] = "sas3";
	ambient = 0;
	for ( pass = 0; pass < 2; pass++ )
	{
		for ( i = 0; i < ents.size; i++ )
		{
			e = ents[ i ];
			if ( !isDefined( e.classname ) || getSubStr( e.classname, 0, 11 ) != "actor_ally_" )
				continue;
			if ( !isDefined( e.script_noteworthy ) || isDefined( e.coop_cast_done ) )
				continue;
			// Price drives the jeep; he is placed at his drop-off spot when
			// the ride is over (spawn_driver).
			if ( e.script_noteworthy == "driver" )
				continue;
			key = e.script_noteworthy;
			named = false;
			for ( k = 0; k < order.size; k++ )
			{
				if ( order[ k ] == key )
					named = true;
			}
			if ( pass == 0 && !named )
				continue;
			if ( pass == 1 )
			{
				// Ambient soldiers: the ones with an authored idle animation.
				if ( !isDefined( idle_anim( e.animation ) ) || ambient >= 18 )
					continue;
				ambient++;
			}
			e.coop_cast_done = true;
			ai = spawn_cast( e, key );
			if ( isDefined( ai ) )
			{
				level.coop_cast[ key ] = ai;
				level.coop_cast_list[ level.coop_cast_list.size ] = ai;
			}
			if ( level.coop_cast_list.size >= 27 )
				break;
		}
	}
	codolprintln( "CoDOL coop training: cast of " + level.coop_cast.size + " placed (" + ambient + " ambient)" );
}

// Captain Price gets out of the jeep where the level places him for the
// walk to the range.
spawn_driver()
{
	ents = getEntArray( "secretservice_friendly", "targetname" );
	for ( i = 0; i < ents.size; i++ )
	{
		if ( !isDefined( ents[ i ].script_noteworthy ) || ents[ i ].script_noteworthy != "driver" )
			continue;
		ai = spawn_cast( ents[ i ], "driver" );
		if ( isDefined( ai ) )
		{
			codolprintln( "CoDOL coop training: Price out of the jeep at " + ai.origin );
			level.coop_cast[ "driver" ] = ai;
			level.coop_cast_list[ level.coop_cast_list.size ] = ai;
		}
		return;
	}
}

cast( key )
{
	if ( !isDefined( level.coop_cast ) )
		return undefined;
	return level.coop_cast[ key ];
}

// --------------------------------------------------------------------------
// Helpers.
// --------------------------------------------------------------------------

objective_at( point )
{
	objective_delete( 0 );
	objective_add( 0, "active", point );
}

// What the flow is waiting for, for the log and the probe harness
// (_cooptest.gsc "route" acts on it the way a player would).
step( kind, value, value2 )
{
	s = spawnStruct();
	s.kind = kind;
	s.value = value;
	s.value2 = value2;
	level.coop_training_step = s;
	text = kind;
	if ( isDefined( value ) && ( kind == "flag" || kind == "note" || kind == "near" ) )
		text += " " + value;
	codolprintln( "CoDOL coop training: waiting for " + text );
}

flag_wait_timeout( name, timeout )
{
	step( "flag", name );
	level endon( name );
	waited = 0;
	while ( !maps\mp\gametypes\_coopmission::flag( name ) && waited < timeout )
	{
		wait( 0.5 );
		waited += 0.5;
	}
}

player_near( point, radius )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( isAlive( p ) && distanceSquared( p.origin, point ) < radius * radius )
			return p;
	}
	return undefined;
}

wait_player_near( point, radius, timeout )
{
	step( "near", point );
	waited = 0;
	for ( ;; )
	{
		p = player_near( point, radius );
		if ( isDefined( p ) )
			return p;
		if ( isDefined( timeout ) && waited >= timeout )
			return undefined;
		wait( 0.25 );
		waited += 0.25;
	}
}

// Hits by a player on a set of brushmodels (the range dummies), counted until
// `count` or the time-out.
wait_hits( targets, count, timeout, meleeOnly )
{
	step( "hits", targets, meleeOnly );
	level.coop_training_hits = 0;
	for ( i = 0; i < targets.size; i++ )
	{
		targets[ i ] setCanDamage( true );
		targets[ i ] thread count_hits( meleeOnly );
	}
	waited = 0;
	while ( level.coop_training_hits < count && waited < timeout )
	{
		wait( 0.25 );
		waited += 0.25;
	}
	for ( i = 0; i < targets.size; i++ )
		targets[ i ] notify( "coop_stop_hits" );
	return level.coop_training_hits >= count;
}

count_hits( meleeOnly )
{
	self endon( "coop_stop_hits" );
	for ( ;; )
	{
		self waittill( "damage", amount, attacker, dir, point, type );
		if ( !isDefined( attacker ) || !isPlayer( attacker ) )
			continue;
		if ( isDefined( meleeOnly ) && meleeOnly && ( !isDefined( type ) || type != "MOD_MELEE" ) )
			continue;
		self playSound( "target_metal_hit" );
		level.coop_training_hits++;
	}
}

by_noteworthy( value )
{
	found = [];
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		if ( isDefined( ents[ i ].script_noteworthy ) && ents[ i ].script_noteworthy == value )
			found[ found.size ] = ents[ i ];
	}
	return found;
}

// The first of several weapons the mounted zones carry.
first_mounted( a, b )
{
	if ( codol_isWeaponMounted( a ) )
		return a;
	return b;
}

give_all( weapon, slot )
{
	if ( !codol_isWeaponMounted( weapon ) )
		return;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( !isAlive( players[ i ] ) )
			continue;
		players[ i ] giveWeapon( weapon );
		players[ i ] giveMaxAmmo( weapon );
		if ( isDefined( slot ) && slot )
			players[ i ] switchToWeapon( weapon );
	}
}

// Counts `note` (and `note2`) from every player.  A grenade launcher round
// is a grenade_fire in IW3 weapons and a missile_fire in IW4 ones, so the
// launcher task counts both.
waittill_any_player( note, count, timeout, note2 )
{
	step( "note", note );
	level.coop_training_events = 0;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		players[ i ] thread count_note( note );
		if ( isDefined( note2 ) )
			players[ i ] thread count_note( note2 );
	}
	waited = 0;
	while ( level.coop_training_events < count && waited < timeout )
	{
		wait( 0.25 );
		waited += 0.25;
	}
	level notify( "coop_stop_notes" );
}

count_note( note )
{
	self endon( "disconnect" );
	level endon( "coop_stop_notes" );
	for ( ;; )
	{
		self waittill( note );
		level.coop_training_events++;
	}
}

// --------------------------------------------------------------------------
// Entry points.
// --------------------------------------------------------------------------

// Synchronous from Callback_StartGameType (precache window), before the
// course and intro modules.
init()
{
	level.coop_training_flow = false;
	if ( !is_training() )
		return;
	level.coop_training_flow = true;
	precache_cast();
	// The road gates are open for the jeep from the start: the double car
	// gates at camp (slide_gate_01 / _02) and the east road gate
	// (slide_gate_03 at x 4466); the jeep path runs through all three.
	open_slide_gate( "slide_gate_03", ( 4466, -1360, 59 ) );
	open_slide_gate( "slide_gate_01" );
	open_slide_gate( "slide_gate_02" );
	codolprintln( "CoDOL coop training: mission flow active" );
}

main()
{
	if ( !is_training() )
		return;
	level endon( "coop_mission_over" );

	// Wait for a player, then place the cast (spawnActor needs the actor
	// template, which needs the precached species).
	while ( getPlayers().size <= 0 )
		wait( 0.5 );
	wait( 0.5 );
	spawn_cast_all();
	level thread ambient_chatter();

	// Station 0: the jeep (_coopintro plays Price's lines from the jeep).
	if ( isDefined( level.coop_intro ) && !level.coop_intro.done )
		level waittill( "coop_intro_done" );
	spawn_driver();

	station_range();
	station_demolitions();
	station_obstacle();
	station_pit();
	station_ship();

	maps\mp\gametypes\_coopmission::mission_complete( "training complete" );
}

// --------------------------------------------------------------------------
// Station 1/2: the range.
// --------------------------------------------------------------------------

station_range()
{
	price = cast( "driver" );
	tiny = cast( "waters" );
	tinyPos = ( 1372, -1114, 6 );
	if ( isDefined( tiny ) )
		tinyPos = tiny.origin;
	objective_at( tinyPos );
	maps\mp\gametypes\_coopmission::announce( "Report to Tiny Man on the shooting range" );
	raise_door( "arsenal_door", 120 );

	// Price nags until a player is inside the range.
	nags = [];
	nags[ 0 ] = line( "pit_pri_004", 1.9 );
	nags[ 1 ] = line( "pit_pri_005", 2.6 );
	nags[ 2 ] = line( "pit_pri_006", 2.7 );
	n = 0;
	while ( !maps\mp\gametypes\_coopmission::flag( "inside_firing_range" ) && !isDefined( player_near( tinyPos, 300 ) ) )
	{
		flag_wait_timeout( "inside_firing_range", 15 );
		if ( maps\mp\gametypes\_coopmission::flag( "inside_firing_range" ) || isDefined( player_near( tinyPos, 300 ) ) )
			break;
		if ( n < nags.size )
		{
			say( price, nags[ n ].alias, nags[ n ].time );
			n++;
		}
	}
	codolprintln( "CoDOL coop training: station range" );

	lane = getEntArray( "lane1", "targetname" );
	ads = by_noteworthy( "ads_tutorial_target" );
	targets = lane;
	for ( i = 0; i < ads.size; i++ )
		targets[ targets.size ] = ads[ i ];

	say( tiny, "pit_tiny_001", 2.8 );
	give_all( first_mounted( "m4_grunt_reflex_so", "m16_basic_so" ), true );
	say( tiny, "pit_tiny_002", 2.0 );
	objective_at( ( 1880, -200, 40 ) );
	say( tiny, "pit_tiny_003", 2.7 );
	if ( !wait_hits( targets, 3, 40 ) )
		say( tiny, "pit_tiny_015", 0.7 );
	say( tiny, "pit_tiny_005", 1.5 );
	say( tiny, "pit_tiny_006", 2.1 );
	wait_hits( targets, 3, 40 );
	say( tiny, "pit_tiny_007", 0.8 );
	say( tiny, "pit_tiny_008_1", 2.8 );
	say( tiny, "pit_tiny_008_2", 2.1 );
	wait_hits( targets, 2, 30 );
	say( tiny, "pit_tiny_009", 10.8 );
	say( tiny, "pit_tiny_011_1", 2.5 );
	say( tiny, "pit_tiny_011_2", 1.8 );
	start = getTime();
	if ( wait_hits( targets, 5, 30 ) && getTime() - start < 15000 )
		say( tiny, "pit_tiny_018", 0.8 );
	else
		say( tiny, "pit_tiny_017", 3.4 );
	say( tiny, "pit_tiny_012", 5.5 );

	// Sidearm.
	say( tiny, "pit_tiny_019", 1.8 );
	give_all( first_mounted( "usp_so", "usp_silencer_so" ), true );
	maps\mp\gametypes\_coopmission::flag_set( "sidearm_complete" );
	say( tiny, "pit_tiny_020", 2.0 );
	say( tiny, "pit_tiny_021", 1.2 );
	say( tiny, "pit_tiny_022", 3.3 );

	// Knife: the melee dummy behind the hanger_melee trigger.
	say( tiny, "pit_tiny_023", 1.4 );
	say( tiny, "pit_tiny_024", 1.0 );
	melee = getEntArray( "melee_target_dummy", "targetname" );
	melee2 = by_noteworthy( "melee_target_dummy" );
	for ( i = 0; i < melee2.size; i++ )
		melee[ melee.size ] = melee2[ i ];
	objective_at( ( 1887, -854, 35 ) );
	if ( melee.size > 0 )
		wait_hits( melee, 1, 30, true );
	else
		wait_player_near( ( 1887, -854, 35 ), 120, 30 );
	say( tiny, "pit_tiny_025", 3.7 );
	maps\mp\gametypes\_coopmission::flag_set( "start_frag_training" );
}

// --------------------------------------------------------------------------
// Station 3: demolitions with Sgt. Newcastle.
// --------------------------------------------------------------------------

station_demolitions()
{
	nwc = cast( "nwc" );
	pos = ( 2092, -2451, 25 );
	if ( isDefined( nwc ) )
		pos = nwc.origin;
	objective_at( pos );
	maps\mp\gametypes\_coopmission::announce( "Report to Sgt. Newcastle" );
	open_doors( "gate_cqb_enter" );
	wait_player_near( pos, 320, 90 );
	codolprintln( "CoDOL coop training: station demolitions" );

	say( nwc, "pit_ncastle_001", 2.1 );
	say( nwc, "pit_ncastle_002", 2.4 );
	give_all( "fraggrenade_so" );
	open_doors( "gate_enter_grenade" );
	objective_at( ( 1912, -2447, 12 ) );
	flag_wait_timeout( "near_grenade_area", 30 );
	if ( !maps\mp\gametypes\_coopmission::flag( "near_grenade_area" ) )
		say( nwc, "pit_ncastle_003", 1.2 );

	say( nwc, "pit_ncastle_004", 3.4 );
	waittill_any_player( "grenade_fire", 3, 45 );
	say( nwc, "pit_ncastle_005", 2.0 );
	give_all( "m16_grenadier_reddot_so", true );
	say( nwc, "pit_ncastle_006", 1.4 );
	say( nwc, "pit_ncastle_007", 1.9 );
	waittill_any_player( "missile_fire", 1, 30, "grenade_fire" );
	say( nwc, "pit_ncastle_008", 1.4 );
	say( nwc, "pit_ncastle_009", 3.3 );
	say( nwc, "pit_ncastle_010", 3.3 );
	waittill_any_player( "missile_fire", 3, 40, "grenade_fire" );
	say( nwc, "pit_ncastle_012", 1.1 );
	say( nwc, "pit_ncastle_013", 2.0 );
	say( nwc, "pit_ncastle_014_1", 2.1 );
	say( nwc, "pit_ncastle_014_2", 2.6 );
	maps\mp\gametypes\_coopmission::flag_set( "c4_equiped" );
	open_doors( "gate_cqb_exit", -95 );
}

// --------------------------------------------------------------------------
// Station 4: the obstacle course with Mac.
// --------------------------------------------------------------------------

station_obstacle()
{
	mac = cast( "mac" );
	objective_at( ( 330, -2537, 40 ) );
	maps\mp\gametypes\_coopmission::announce( "Report to Mac on the obstacle course" );
	open_doors( "gate_cqb_exit2" );
	flag_wait_timeout( "wait_line_up", 120 );
	codolprintln( "CoDOL coop training: station obstacle course" );

	say( mac, "pit_mac_001", 3.2 );
	say( mac, "pit_mac_002", 1.9 );
	wait( 1.5 );
	say( mac, "pit_mac_003", 0.5 );
	maps\mp\gametypes\_coopmission::flag_set( "start_obstacle" );
	maps\mp\gametypes\_coopmission::flag_set( "start_course" );
	objective_at( ( -1106, -2824, 72 ) );

	level thread obstacle_nags( mac );
	flag_wait_timeout( "obstacle_course_end", 180 );
	level notify( "coop_obstacle_done" );
	say( mac, "pit_mac_008_1", 3.6 );
	maps\mp\gametypes\_coopmission::flag_set( "obstacle_complete" );
	open_doors( "gate_cqb_enter_main", -170 );
}

obstacle_nags( mac )
{
	level endon( "coop_obstacle_done" );
	level endon( "coop_mission_over" );
	level thread obstacle_line_on_flag( mac, "obstacleTraining_mantle2", "pit_mac_005", 1.7 );
	level thread obstacle_line_on_flag( mac, "crawl_dialog", "pit_mac_006", 2.1 );
	wait( 12 );
	say( mac, "pit_mac_004", 3.8 );
	wait( 20 );
	say( mac, "pit_mac_007", 3.2 );
}

obstacle_line_on_flag( mac, flag, alias, time )
{
	level endon( "coop_obstacle_done" );
	maps\mp\gametypes\_coopmission::flag_wait( flag );
	say( mac, alias, time );
}

// --------------------------------------------------------------------------
// Station 5: Price, the hangar, Harker and the pit.
// --------------------------------------------------------------------------

station_pit()
{
	price2 = cast( "price2" );
	pos = ( -1390, -2514, 0 );
	if ( isDefined( price2 ) )
		pos = price2.origin;
	objective_at( pos );
	maps\mp\gametypes\_coopmission::announce( "Report to Captain Price" );
	flag_wait_timeout( "come_to_price", 90 );

	// Through the hangar yard to the S.S.D.D. hangar.
	open_slide_gate( "slide_gate_03", ( -1237, -1200, 8 ) );
	objective_at( ( 89, -1315, 46 ) );
	maps\mp\gametypes\_coopmission::announce( "Head to the hangar" );
	flag_wait_timeout( "at_ssdd_hanger", 120 );
	codolprintln( "CoDOL coop training: station pit (briefing)" );

	harker = cast( "sas3" );
	price = cast( "price" );
	ghost = cast( "sas2" );
	soap = cast( "sas1" );
	say( harker, "pit_har_001", 7.5 );
	say( harker, "pit_har_002", 8.7 );
	say( harker, "pit_har_003", 8.1 );
	say( price, "pit_pri_007", 6.5 );
	say( harker, "pit_har_004", 4.1 );
	say( soap, "pit_mct_001", 2.9 );
	say( harker, "pit_har_005", 4.4 );
	say( ghost, "pit_gst_001", 6.3 );
	say( price, "pit_pri_008", 2.2 );
	say( price, "pit_pri_009", 5.2 );
	say( price, "pit_pri_010", 2.8 );
	say( price, "pit_pri_011", 5.0 );

	open_doors( "gate_cqb_start" );
	objective_at( ( -218, -758, 48 ) );
	maps\mp\gametypes\_coopmission::announce( "Run the pit: under 60 seconds" );

	// The pit course itself (_coopcourse.gsc); repeat until it is cleared in
	// time, as Price demands.
	for ( ;; )
	{
		step( "course" );
		level waittill( "coop_course_run_over", finished, elapsed );
		if ( finished && elapsed <= 60 )
			break;
		say( price, "pit_pri_050", 3.9 );
	}
	if ( level.coop_course_best <= 23 )
		say( price, "pit_pri_058", 3.1 );
	else
		say( price, "pit_pri_056", 4.3 );
	open_doors( "gate_cqb_finish" );
	open_doors( "gate_cqb_out" );
}

// --------------------------------------------------------------------------
// Station 6: the cargoship deck mock-up and the debrief.
// --------------------------------------------------------------------------

station_ship()
{
	price = cast( "price" );
	raise_door( "ship_hanger_door", 200 );
	raise_door( "ship_hanger_clip", 200 );
	raise_door( "hanger_one_door", 200 );
	objective_at( ( 3530, -2467, 32 ) );
	maps\mp\gametypes\_coopmission::announce( "Report to the ship mock-up" );
	speaker = price;
	wait_player_near( ( 3530, -2467, 32 ), 400, 180 );
	codolprintln( "CoDOL coop training: station ship mock-up" );

	// Price calls the course from the top of the ladder.
	say( speaker, "pit_pri_012", 1.5 );
	say( speaker, "pit_pri_013", 3.5 );
	give_all( first_mounted( "mp5k_codo_so", "mp5_silencer_so" ), true );
	give_all( "flash_grenade_so" );
	say( speaker, "pit_pri_014", 4.4 );
	say( speaker, "pit_pri_015", 3.7 );
	say( speaker, "pit_pri_016", 5.4 );
	say( speaker, "pit_pri_017", 2.5 );
	wait( 1 );
	say( speaker, "pit_pri_018", 2.0 );
	start = getTime();

	positions = [];
	positions[ 0 ] = "position_one_targets";
	positions[ 1 ] = "position_two_targets";
	positions[ 2 ] = "position_4_targets";
	positions[ 3 ] = "position_melee_targets";
	positions[ 4 ] = "position_5_targets";
	positions[ 5 ] = "position_6_targets";
	calls = [];
	calls[ 0 ] = line( "pit_pri_019", 2.1 );
	calls[ 1 ] = line( "pit_pri_025", 2.1 );
	calls[ 2 ] = line( "pit_pri_030", 1.9 );
	calls[ 3 ] = line( "pit_pri_029", 2.3 );
	calls[ 4 ] = line( "pit_pri_035", 2.6 );
	calls[ 5 ] = line( "pit_pri_039", 2.2 );
	for ( k = 0; k < positions.size; k++ )
	{
		targets = getEntArray( positions[ k ], "targetname" );
		say( speaker, calls[ k ].alias, calls[ k ].time );
		if ( targets.size <= 0 )
			continue;
		objective_at( targets[ 0 ].origin );
		for ( i = 0; i < targets.size; i++ )
			targets[ i ] notify( "pop_up" );
		ship_wait_cleared( targets, 45 );
	}
	say( speaker, "pit_pri_041", 2.1 );
	elapsed = ( getTime() - start ) / 1000.0;
	codolprintln( "CoDOL coop training: ship mock-up in " + elapsed + " s" );
	if ( elapsed < 60 )
		say( speaker, "pit_pri_042", 3.4 );
	else
		say( speaker, "pit_pri_055", 4.8 );
	say( speaker, "pit_pri_044", 3.4 );

	// The debrief at the monitors.
	objective_at( ( 4275, -2448, 26 ) );
	wait_player_near( ( 4275, -2448, 26 ), 260, 90 );
	say( speaker, "pit_pri_061", 9.3 );
	say( speaker, "pit_pri_062", 6.8 );
	say( speaker, "pit_pri_063", 7.0 );
	say( speaker, "pit_pri_064", 3.2 );
	say( speaker, "pit_pri_065_1", 5.0 );
	say( speaker, "pit_pri_065_2", 4.4 );
	objective_delete( 0 );
}

ship_wait_cleared( targets, timeout )
{
	step( "ship", targets );
	waited = 0;
	for ( ;; )
	{
		up = 0;
		for ( i = 0; i < targets.size; i++ )
		{
			if ( isDefined( targets[ i ].coop_up ) && targets[ i ].coop_up && targets[ i ].coop_target_type == "enemy" )
				up++;
		}
		if ( up == 0 && waited > 1 )
			return;
		if ( waited >= timeout )
		{
			maps\mp\gametypes\_coopcourse::drop( targets );
			return;
		}
		wait( 0.5 );
		waited += 0.5;
	}
}

// --------------------------------------------------------------------------
// The camp's chatter: ambient soldiers trade pit_rgr* lines when a player
// walks past.
// --------------------------------------------------------------------------

ambient_chatter()
{
	level endon( "coop_mission_over" );
	lines = [];
	counts = [];
	counts[ 1 ] = 13;
	counts[ 2 ] = 28;
	counts[ 3 ] = 11;
	counts[ 4 ] = 10;
	for ( g = 1; g <= 4; g++ )
	{
		for ( n = 1; n <= counts[ g ]; n++ )
		{
			num = "" + n;
			if ( n < 10 )
				num = "00" + n;
			else
				num = "0" + n;
			lines[ lines.size ] = "pit_rgr" + g + "_" + num;
		}
	}
	next = 0;
	for ( ;; )
	{
		wait( 6 );
		for ( k = 0; k < level.coop_cast_list.size; k++ )
		{
			ai = level.coop_cast_list[ k ];
			if ( !isDefined( ai ) || isDefined( ai.coop_name ) )
				continue;
			if ( isDefined( ai.coop_last_chatter ) && getTime() - ai.coop_last_chatter < 25000 )
				continue;
			if ( !isDefined( player_near( ai.origin, 350 ) ) )
				continue;
			ai.coop_last_chatter = getTime();
			ai playSound( lines[ next % lines.size ] );
			codolprintln( "CoDOL coop training: chatter " + lines[ next % lines.size ] + " from " + ai.coop_cast );
			next++;
			break;
		}
	}
}
