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
//
// Price drives: a model of him (his body and head) sits in the driver's seat,
// linked to the jeep, playing bh_pilot_idle -- the seated pilot idle C1 ships
// in common_actor_shared.animlist (generic_human), the one seated idle every
// coop client has (the trainer's own humvee_idle_frontl is only in the
// playlist animlist, which C1 does not load; see idle_anim).  The players sit
// in the passenger seats (_coopintro seat_offset).  When the ride is over
// spawn_driver() removes him and places the soldier at the driver's door.
ride_lines()
{
	seat_driver();
	level.coop_intro_lines = [];
	level.coop_intro_lines[ 0 ] = line( "pit_pri_001", 1.8 );
	level.coop_intro_lines[ 1 ] = line( "pit_pri_002_1", 2.4 );
	level.coop_intro_lines[ 2 ] = line( "pit_pri_002_2", 3.1 );
	level.coop_intro_lines[ 3 ] = line( "pit_pri_002_3", 4.5 );
	level.coop_intro_lines[ 4 ] = line( "pit_pri_003", 3.6 );
}

seat_driver()
{
	intro = level.coop_intro;
	if ( !isDefined( intro ) || !isDefined( intro.mover ) || !intro.ground )
		return;
	m = cast_models( "actor_ally_hero_price_afghan_ar_so" );
	mover = intro.mover;
	// Driver's seat: front left (the players' seat 0 is front right).
	offset = ( -6, -17, 20 );
	driver = spawn( "script_model", local_to_world( mover.origin, mover.angles, offset ) );
	driver.angles = mover.angles;
	driver setModel( m.body );
	if ( isDefined( m.head ) )
		driver attach( m.head, "", true );
	driver linkTo( mover, "tag_origin", offset, ( 0, 0, 0 ) );
	driver scriptModelPlayAnim( "bh_pilot_idle" );
	level.coop_intro_driver = driver;
	tag = nametag( "Captain Price", driver.origin );
	if ( isDefined( tag ) )
	{
		tag linkTo( mover, "tag_origin", offset - ( 0, 0, 20 ), ( 0, 0, 0 ) );
		driver.coop_nametag = tag;
	}
	codolprintln( "CoDOL coop training: Price at the wheel (" + m.body + ", bh_pilot_idle)" );
}

local_to_world( origin, angles, offset )
{
	return origin + anglesToForward( angles ) * offset[ 0 ] + anglesToRight( angles ) * offset[ 1 ] + ( 0, 0, offset[ 2 ] );
}

say( speaker, alias, time )
{
	if ( isDefined( speaker ) )
	{
		speaker playSound( alias );
		if ( isDefined( speaker.coop_cast ) && isDefined( time ) )
			speaker thread gesture( time );
	}
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

// The range's entrance (user report 2026-10-07: "the door to the firing range
// didn't open so I couldn't progress").  rifle_range_door is a single brush
// door (1384 -1182, *104) in the range hangar's south wall that the first
// version never moved; obj_door_frame (*102, 1408 -1184) is the objective glow
// round the opening and the trainer's path nodes run through it at x 1407
// (MEASURED, base mp_killhouse_sh entities).  The door has no hinge entity
// (the script_origin beside it, pf388_auto566, is the target of an ambience
// trigger), so its extent along the wall is measured with traces and it
// swings about its west edge, inward, toward the range; it is left
// non-solid so a swing that clips the wall can never block the way.  The
// trainer's inside_firing_range trigger stands in the doorway; the door opens
// when the player is sent to Tiny Man.
open_range_door()
{
	door = getEnt( "rifle_range_door", "targetname" );
	if ( !isDefined( door ) || isDefined( door.coop_open ) )
		return;
	door.coop_open = true;
	west = undefined;
	east = undefined;
	for ( x = door.origin[ 0 ] - 96; x <= door.origin[ 0 ] + 96; x += 4 )
	{
		trace = bulletTrace( ( x, door.origin[ 1 ] - 40, door.origin[ 2 ] ), ( x, door.origin[ 1 ] + 40, door.origin[ 2 ] ), false, undefined );
		if ( isDefined( trace[ "entity" ] ) && trace[ "entity" ] == door )
		{
			if ( !isDefined( west ) )
				west = x;
			east = x;
		}
	}
	door notSolid();
	if ( isDefined( west ) )
	{
		hinge = spawn( "script_origin", ( west - 2, door.origin[ 1 ], door.origin[ 2 ] ) );
		door linkTo( hinge );
		hinge rotateYaw( 95, 1.5, 0.3, 0.3 );
		codolprintln( "CoDOL coop training: rifle_range_door spans x " + west + ".." + east + "; swung in about its west edge" );
	}
	else
	{
		door moveTo( door.origin - ( 0, 0, 128 ), 1.5 );
		codolprintln( "CoDOL coop training: rifle_range_door extent not found by trace; lowered" );
	}
	door playSound( "scn_training_fence_open" );
	frame = getEnt( "obj_door_frame", "targetname" );
	if ( isDefined( frame ) )
	{
		frame hide();
		frame notSolid();
	}
}

// Every door, gate and blocker on the route, opened as the stations open
// them (the probe harness's "opengates", for walking each one in turn).
open_everything()
{
	open_range_door();
	raise_door( "arsenal_door", 120 );
	open_doors( "gate_cqb_enter" );
	remove_named( "door_fake_collision" );
	open_doors( "gate_enter_grenade" );
	open_doors( "gate_cqb_exit", -95 );
	open_doors( "gate_cqb_exit2" );
	remove_named( "clip_obstacle_start" );
	open_doors( "gate_cqb_enter_main", -170 );
	open_slide_gate( "slide_gate_03", ( -1237, -1200, 8 ) );
	open_doors( "gate_cqb_start" );
	open_doors( "gate_cqb_finish" );
	remove_named( "course_end_blocker" );
	open_doors( "gate_cqb_out" );
	raise_door( "ship_hanger_door", 200 );
	raise_door( "ship_hanger_clip", 200 );
	raise_door( "hanger_one_door", 200 );
	remove_named( "ship_melee_clip" );
}

// Take a blocker out of the way: hidden and not solid (by targetname).
remove_named( name )
{
	parts = getEntArray( name, "targetname" );
	for ( i = 0; i < parts.size; i++ )
	{
		parts[ i ] hide();
		parts[ i ] notSolid();
	}
	codolprintln( "CoDOL coop training: removed " + parts.size + " part(s) of " + name );
}

// Show or remove one of the range's screens (by script_noteworthy).
range_screen( name, present )
{
	parts = by_noteworthy( name );
	for ( i = 0; i < parts.size; i++ )
	{
		if ( present )
		{
			parts[ i ] show();
			parts[ i ] solid();
		}
		else
		{
			parts[ i ] hide();
			parts[ i ] notSolid();
		}
	}
	codolprintln( "CoDOL coop training: range screen " + name + " (" + parts.size + ") " + present );
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

// The idle a cast member stands in, and the gesture he talks with.
//
// Only animations from the animlists EVERY coop client loads are used:
// animlists/common_human and common_actor_shared (common_gametype_coop), whose
// xanims ship in that zone.  The trainer's own idles (killhouse_*,
// civilian_sitting_*, training_*, cliffhanger_welder_*, ...) are listed only in
// animlists/coop_training_mp_killhouse_sh.animlist, and C1 loads the map
// animlist named by its `mapname` dvar (C1 0x450b3e Dvar_FindVar("mapname") ->
// 0x408310), which the gamestate sets to the level, mp_killhouse_sh, whose
// animlist lists only a fan.  A client therefore has no generic_human entry for
// them, and the server tree indices the first version published for them
// (1026..1045) named nothing the client could play (MEASURED: user report
// 2026-10-07, "the characters need their animations").  Each authored idle is
// mapped to the closest common one.
idle_anim( name, key )
{
	if ( isDefined( key ) )
	{
		switch ( key )
		{
		case "waters":
		case "nwc":
			return %patrol_bored_idle;
		case "mac":
		case "price2":
		case "driver":
		case "price":
			return %casual_stand_v2_idle;
		case "chair_guy":
			return %casual_crouch_idle;
		case "sas1":
		case "sas2":
		case "sas3":
			return %exposed_idle_alert_v1;
		}
	}
	if ( !isDefined( name ) )
		return undefined;
	switch ( toLower( name ) )
	{
	case "civilian_reader_2":
	case "civilian_sitting_talking_a_1":
	case "civilian_sitting_talking_a_2":
	case "civilian_sitting_talking_b_1":
	case "sitting_guard_loadak_idle":
	case "training_humvee_repair":
	case "cliffhanger_welder_engine":
	case "cliffhanger_welder_wing":
	case "training_pushups_guy1":
		return %casual_crouch_idle;
	case "parabolic_leaning_guy_idle_training":
		return %parabolic_leaning_guy_smoking_idle;
	case "civilian_texting_standing":
		return %patrol_bored_idle_cellphone;
	case "killhouse_sas_price_idle":
	case "bunker_toss_idle_guy1":
		return %patrol_bored_idle_smoke;
	case "killhouse_gaz_idleb":
	case "training_pit_stand_idle":
	case "killhouse_price_stand_talk":
		return %casual_stand_v2_idle;
	case "roadkill_humvee_map_sequence_quiet_idle":
	case "training_basketball_guy1":
	case "training_basketball_guy2":
	case "training_intro_trainee_1_end_idle":
		return %patrol_bored_idle;
	}
	return undefined;
}

// The overhead name of a friendly NPC, as vanilla shows "Captain Price" over
// him.  C1 draws no name for an actor; it draws one over a script model whose
// entityState +0x64 names a client slot (C1 0x43a3b0, gated by client dvar
// "772"), at the model's origin + 82.  codol_setOverheadName (native,
// src/codol/script/codol_script_overhead_name.c) gives an invisible model a
// slot no player occupies, holding the name.  The names are the map's own
// script_friendname values (C1's localized strings carry none for this cast).
// +set scr_coop_cast_nametags 0 turns them off.
nametag( name, origin )
{
	if ( getDvar( "scr_coop_cast_nametags" ) != "" && getDvarInt( "scr_coop_cast_nametags" ) == 0 )
		return undefined;
	tag = spawn( "script_model", origin );
	tag setModel( "tag_origin" );
	tag codol_setOverheadName( name );
	return tag;
}

// Every player is told to draw those names (C1 dvar "772", default 0).
show_nametags()
{
	if ( getDvar( "scr_coop_cast_nametags" ) != "" && getDvarInt( "scr_coop_cast_nametags" ) == 0 )
		return;
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread show_nametags_player();
	}
}

show_nametags_player()
{
	self endon( "disconnect" );
	self waittill( "spawned_player" );
	self setClientDvar( "772", "1" );
}

// Talk: the casual talking twitch (common_human), then back to the idle.
gesture( time )
{
	self endon( "death" );
	self notify( "coop_gesture" );
	self endon( "coop_gesture" );
	self.coop_pose_anim = %casual_stand_v2_twitch_talk;
	self notify( "coop_cast_pose" );
	length = getAnimLength( %casual_stand_v2_twitch_talk );
	if ( length > time + 0.3 )
		length = time + 0.3;
	if ( length < 1 )
		length = 1;
	wait( length );
	self.coop_pose_anim = undefined;
	self notify( "coop_cast_pose" );
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
	ai.coop_idle_anim = idle_anim( spawner.animation, key );
	if ( !isDefined( ai.coop_idle_anim ) )
		ai.coop_idle_anim = %casual_stand_idle;
	if ( isDefined( spawner.script_friendname ) )
	{
		ai.coop_name = spawner.script_friendname;
		ai.coop_nametag = nametag( ai.coop_name, ai.origin );
	}
	ai setAnimKnobAll( ai.coop_idle_anim, %body, 1, 0.1, 1 );
	ai notify( "coop_cast_pose" );
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

// Captain Price drives the jeep (_coopintro seats a model of him at the
// wheel) and gets out at the driver's door when it stops.
spawn_driver()
{
	ents = getEntArray( "secretservice_friendly", "targetname" );
	for ( i = 0; i < ents.size; i++ )
	{
		if ( !isDefined( ents[ i ].script_noteworthy ) || ents[ i ].script_noteworthy != "driver" )
			continue;
		spot = ents[ i ];
		if ( isDefined( level.coop_intro ) && isDefined( level.coop_intro.mover ) )
		{
			mover = level.coop_intro.mover;
			spot = spawnStruct();
			spot.classname = ents[ i ].classname;
			spot.script_friendname = ents[ i ].script_friendname;
			spot.animation = ents[ i ].animation;
			spot.origin = maps\mp\gametypes\_coopvehicle::ground_under( mover.origin + anglesToRight( mover.angles ) * -72 + anglesToForward( mover.angles ) * 8 );
			spot.angles = ( 0, mover.angles[ 1 ] - 90, 0 );
		}
		if ( isDefined( level.coop_intro_driver ) )
		{
			if ( isDefined( level.coop_intro_driver.coop_nametag ) )
				level.coop_intro_driver.coop_nametag delete();
			level.coop_intro_driver delete();
			level.coop_intro_driver = undefined;
		}
		ai = spawn_cast( spot, "driver" );
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
		codolprintln( "CoDOL coop training: target " + self getEntityNumber() + " hit by " + attacker.name + ", " + level.coop_training_hits );
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
	precacheMpAnim( "bh_pilot_idle" );
	// The Spec Ops gates (so_gate: a brush across the demolition pit's exit
	// at 1868 -2484 and the pit course entrance at 391 -884) belong to
	// so_killspree_trainer, not to this mission.
	remove_named( "so_gate" );
	level thread show_nametags();
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
	open_range_door();
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

	// The lanes are screened (MEASURED with aimcheck in _cooptest: from the
	// firing stall every line to every dummy ends on range_metalwall, a
	// brush at 1888 -455, or on the plywood sheet at 1888 -367).  The wall
	// comes down for the lesson; the plywood stands only for the penetration
	// part, where Tiny tells the player to shoot through it.
	range_screen( "range_metalwall", false );
	range_screen( "plywood", false );
	say( tiny, "pit_tiny_001", 2.8 );
	give_all( first_mounted( "m4_grunt_reflex_so", "m16_basic_so" ), true );
	say( tiny, "pit_tiny_002", 2.0 );
	// Firing station one (obj_station_one_glowing, 1939 -851), lane 1's
	// stall; the knife dummy stands in front of obj_rifle_stall (1888 -836).
	objective_at( ( 1939, -851, 60 ) );
	say( tiny, "pit_tiny_003", 2.7 );
	if ( !wait_hits( targets, 3, 40 ) )
		say( tiny, "pit_tiny_015", 0.7 );
	say( tiny, "pit_tiny_005", 1.5 );
	say( tiny, "pit_tiny_006", 2.1 );
	wait_hits( targets, 3, 40 );
	say( tiny, "pit_tiny_007", 0.8 );
	range_screen( "plywood", true );
	say( tiny, "pit_tiny_008_1", 2.8 );
	say( tiny, "pit_tiny_008_2", 2.1 );
	wait_hits( targets, 2, 30 );
	say( tiny, "pit_tiny_009", 10.8 );
	range_screen( "plywood", false );
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
	// The enter gate's stand-in collision (1959 -2267) stays behind otherwise.
	remove_named( "door_fake_collision" );
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
	// GO: the clip across the start line (clip_obstacle_start, 308 -2556).
	remove_named( "clip_obstacle_start" );
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

	// The way in.  The pit's start room (x -370..-125, y -836..-980) and the
	// corridor east of it are walled off from the hangar along y -989; the
	// one opening is gate_cqb_out (x 84..196), and gate_cqb_start (x -121,
	// y -836..-922) joins the corridor to the start room.  The first version
	// opened gate_cqb_out only after the course, so a player could never reach
	// the start trigger (user report 2026-10-07, "one of the gates doesn't open
	// for the cqb course"; MEASURED with _cooptest colmap: the y -989 wall is
	// closed from x -376 to 300 except that gate).  Both open for the run;
	// the finish (gate_cqb_finish + course_end_blocker) opens after it.
	open_doors( "gate_cqb_out" );
	open_doors( "gate_cqb_start" );
	objective_at( ( -218, -758, 48 ) );
	maps\mp\gametypes\_coopmission::announce( "Run the pit: under 60 seconds" );

	// The pit course itself (_coopcourse.gsc); repeat until it is cleared in
	// time, as Price demands.
	// The finish (the lane x 476..572 ending at y -840) is closed by
	// gate_cqb_finish and course_end_blocker (532 -761) so the course is not
	// run backward from the corridor; both open when a run ends, made or not,
	// so a slow player walks out by the finish into the same corridor and
	// back to the start for another try (MEASURED with colmap: the lane joins
	// the corridor south of y -856).
	for ( ;; )
	{
		step( "course" );
		level waittill( "coop_course_run_over", finished, elapsed );
		open_doors( "gate_cqb_finish" );
		remove_named( "course_end_blocker" );
		if ( finished && elapsed <= 60 )
			break;
		say( price, "pit_pri_050", 3.9 );
	}
	if ( level.coop_course_best <= 23 )
		say( price, "pit_pri_058", 3.1 );
	else
		say( price, "pit_pri_056", 4.3 );
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
	// The knife position's clip (ship_melee_clip, 3761 -3078) opens with it.
	remove_named( "ship_melee_clip" );
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
