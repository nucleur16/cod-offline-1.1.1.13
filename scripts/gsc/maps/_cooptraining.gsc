// coop_training_mp_killhouse_sh: the scripted training mission.
//
// CoDOL's training mission is CoD4's killhouse (F.N.G.) with MW2's jeep ride
// added, on mp_killhouse_sh, with CoDOL's own cast and voice lines.  Its
// level script does not ship.  The reference for the station mechanics is
// CoD4 maps\killhouse.gsc, killhouse_code.gsc and killhouse_anim.gsc (user
// direction 2026-10-08): flags, triggers, door moves and character places
// follow them (docs/CODOL_CAMPAIGN_MODE.md section 12 maps each CoD4 name to
// this map's).  The map's pit (the CQB course) and cargoship mock-up are not
// part of this mission (user, 2026-10-08: the pit belongs to another
// training); _coopcourse.gsc leaves the pit idle here.
// Sources:
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
// The retail mission as a player sees it (a gameplay recording of the CoDOL
// client, user direction 2026-10-08) is the authority for the order of
// stations and lines, who stands where, what glows and what moves;
// docs/CODOL_CAMPAIGN_MODE.md section 14 is that timeline.  Stations, in
// order:
//   0 the jeep ride into camp, Price at the wheel (pit_pri_001..002_3) --
//     _coopintro; on foot Price sends the rookie to Tiny Man (pit_pri_003)
//     and nags (004..006); the floor arrows light the way to the range door,
//     whose frame glows
//   1 the range with Tiny Man: the rifle is a real pickup from the table
//     (the player arrives with only his pistol and offhands, weapons down),
//     hip fire, aim down the sights, targets popping up one at a time,
//     pistol then rifle, knife (pit_tiny_001..025; no plywood lesson)
//   2 demolitions with Sgt. Newcastle: frags into windows two to four, the
//     grenadier rifle at wall one and windows five to seven; each window
//     glows and carries a marker until it is hit (pit_ncastle_001..014)
//   3 the obstacle course with Mac and the crouched recruits at the line
//     (pit_mac_001..008)
//   4 Price by the fence at the end of the course: the debrief
//     (pit_pri_062..065), which completes the mission.
// The camp's soldiers trade the pit_rgr* chatter when a player walks past.
//
// Every task has a time-out so a player is never stuck behind a line the
// engine cannot judge (a grenade landing in a window, a flashbang thrown);
// the doors of each station open when the station is reached.
//
// Coop module (maps/, IW4 singleplayer namespace); names no multiplayer module.

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
// Price drives: see seat_driver().  The players sit in the passenger seats
// (_coopintro board).  When the ride is over spawn_driver() takes him out at
// the driver's door.
ride_lines()
{
	// _coopintro waits for the driver before it sets off.
	level.coop_intro_wait_driver = true;
	level.coop_intro_lines = [];
	level.coop_intro_lines[ 0 ] = line( "pit_pri_001", 1.8 );
	level.coop_intro_lines[ 1 ] = line( "pit_pri_002_1", 2.4 );
	level.coop_intro_lines[ 2 ] = line( "pit_pri_002_2", 3.1 );
	level.coop_intro_lines[ 3 ] = line( "pit_pri_002_3", 4.5 );
	// pit_pri_003 (go see Tiny Man) is spoken on foot, after the ride
	// (station_range), as the recording shows it.
}

// Price at the wheel: an ACTOR of the coop soldier species with Price's body
// and head, the same path that animates and composes the standing cast,
// riding the humvee's driver tag (tag_driver, front left, 15 / -29 / 36 in
// the model's frame -- MEASURED with _cooptest jeeptags), in animmode
// "nophysics" so the actor's own movement leaves the link alone, playing
// technical_driver_idle: the seated driving idle (hands on a wheel) in
// common_actor_shared.animlist, which every coop client loads, and in the
// server's generic_human tree.
//
// The earlier versions sat a SCRIPT_MODEL there with the head attach()ed and
// scriptModelPlayAnim( "bh_pilot_idle" ).  On C1 that showed headless and in
// the bind pose (user reports 2026-10-07 and twice on 2026-10-08): a script
// model's attachments are not composed by C1 the way an actor's head is, and
// bh_pilot_idle is in no animtree the server declares to the client
// (codol_model_animation.h), so the model got no animation.
seat_driver()
{
	intro = level.coop_intro;
	if ( !isDefined( intro ) || !isDefined( intro.mover ) || !intro.ground || intro.done )
		return;
	mover = intro.mover;
	spot = spawnStruct();
	spot.classname = "actor_ally_hero_price_afghan_ar_so";
	spot.origin = mover getTagOrigin( "tag_driver" );
	a = mover getTagAngles( "tag_driver" );
	spot.angles = ( 0, a[ 1 ], 0 );
	spot.script_friendname = "Captain Price";
	ai = spawn_cast( spot, "driver_seat" );
	if ( !isDefined( ai ) )
	{
		codolprintln( "CoDOL coop training: no actor for Price at the wheel" );
		level.coop_intro_wait_driver = undefined;
		return;
	}
	ai animMode( "nophysics" );
	// IW4 maps\_humvee position 0 (tag_driver): humvee_idle_frontL, replayed
	// each cycle (a non-looping clip).  spawn_cast started his idle under the
	// "driver_seat" key; bh_pilot_idle is the common fallback.
	ai setAnimKnobAll( ai.coop_idle_anim, %body, 1, 0.1, 1 );
	ai notify( "coop_cast_pose" );
	// Actors refuse linkTo; codol_actorRide places him at the tag every
	// frame from the jeep's own trajectory (native, coop only).
	ai codol_actorRide( mover, "tag_driver" );
	if ( isDefined( ai.coop_nametag ) )
		ai.coop_nametag linkTo( mover, "tag_driver", ( 0, 0, 0 ), ( 0, 0, 0 ) );
	level.coop_intro_driver = ai;
	level.coop_intro_wait_driver = undefined;
	codolprintln( "CoDOL coop training: Price at the wheel: actor ent " + ai getEntityNumber() + " riding tag_driver at " +
		spot.origin + ", humvee_idle_frontl, nophysics" );
	level thread ride_trace( ai, mover );
}

// One line a second during the ride, for checking a user report against the
// server log: the jeep, Price against his tag, and every rider's place,
// stance and link.
ride_trace( driver, mover )
{
	level endon( "coop_intro_done" );
	for ( n = 0; ; n++ )
	{
		if ( !isDefined( mover ) )
			return;
		text = "CoDOL coop ride: t " + n + " jeep " + mover.origin;
		if ( isDefined( driver ) )
			text += " price " + driver.origin + " off tag " + int( distance( driver.origin, mover getTagOrigin( "tag_driver" ) ) );
		riders = level.coop_intro.riders;
		for ( i = 0; i < riders.size; i++ )
		{
			p = riders[ i ];
			if ( !isDefined( p ) )
				continue;
			text += " | " + p.name + " " + p.origin + " " + p getStance() + " linked " + p isLinked() + " " + p codol_moveStateText();
		}
		codolprintln( text );
		wait( 1 );
	}
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

// The range's entrance, CoD4 killhouse.gsc open_firing_range_door:
// rifle_range_door rotates +88 about its own origin (1384 -1182, the brush's
// west edge: the door spans x 1388..1428 by trace), door_metal_slow_open,
// connectpaths.  It opens when a player reaches the range (at_rifle_range,
// navigationTraining) and stays open: CoD4 closes it behind the one player
// and reopens it after the knife, which in coop would shut a teammate out.
// The pass-9 version never moved this door (user report 2026-10-07, "the
// door to the firing range didn't open").
open_range_door()
{
	door = getEnt( "rifle_range_door", "targetname" );
	if ( !isDefined( door ) || isDefined( door.coop_open ) )
		return;
	door.coop_open = true;
	door playSound( "door_metal_slow_open" );
	door rotateTo( door.angles + ( 0, 88, 0 ), 1, 0.5, 0 );
	door connectPaths();
	frame = getEnt( "obj_door_frame", "targetname" );
	if ( isDefined( frame ) )
	{
		frame hide();
		frame notSolid();
	}
	codolprintln( "CoDOL coop training: opened rifle_range_door (+88 about its origin)" );
}

// Every door, gate and blocker on the route, opened as the stations open
// them (the probe harness's "opengates", for walking each one in turn).
open_everything()
{
	open_range_door();
	raise_door( "arsenal_door", 120 );
	open_doors( "gate_cqb_enter" );
	remove_named( "door_fake_collision" );
	raise_wall_one();
	open_doors( "gate_enter_grenade" );
	open_doors( "gate_cqb_exit", -95 );
	open_doors( "gate_cqb_exit2" );
	remove_named( "clip_obstacle_start" );
	open_doors( "gate_cqb_enter_main", -170 );
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

// Single brush doors with no hinge origin (arsenal_door): raised out of the
// way.
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

// A cast member's look is his authored classname's: spawn_cast spawns him
// through that classname's aitype (aitype\ally_*_so), whose character\
// scripts give him his body and head, as IW4 gives every actor.  Where the
// recording shows a cast member in another character than his classname's,
// the key names it here: Mac in the boonie and sunglasses of the woodland SAS
// (his spawner is a SEAL classname), Newcastle in the tan Task Force uniform
// and cap, the recruits at the obstacle line in T-shirts.  Each is precached by
// precache_cast; every model ships in coop_training_mp_killhouse_sh.
cast_character( key )
{
	if ( !isDefined( key ) )
		return undefined;
	switch ( key )
	{
	case "mac":
		return character\character_sas_woodland_mac::main;
	case "nwc":
		return character\character_codo_complete_tf_tan::main;
	case "buddy1":
	case "buddy2":
	case "buddy3":
		return character\character_codo_tf_ustshirt_clean_yel::main;
	}
	return undefined;
}

// The props the recording shows in the cast's hands, on tag_inhand (C1 ships
// each model in this mission's zone, MEASURED docs/zone_assets): the
// mechanics' welder handle, the texters' PDA, the reader's book, the map
// writer's pencil, the basketball.
prop_for( key )
{
	switch ( key )
	{
	case "repair_bottom":
	case "engine":
	case "package_guy":
		return "machinery_welder_handle";
	case "phoneguy":
	case "phoneguy2":
		return "electronics_pda";
	case "sitman":
		return "open_book_static";
	case "writter":
		return "characters_accessories_pencil";
	case "friendlies_basketball":
		return "basketball";
	}
	return undefined;
}

precache_props()
{
	props = strTok( "machinery_welder_handle electronics_pda open_book_static characters_accessories_pencil basketball body_complete_tf_tan", " " );
	for ( i = 0; i < props.size; i++ )
		precacheModel( props[ i ] );
	// The welders' sparks: the effect the zone's own fx script loads for
	// them (maps/mp/coop_training_mp_killhouse_sh_fx.gsc, welding_runner).
	level._effect[ "coop_training_welding" ] = loadfx( "misc/welding_runner" );
}

// Sparks at a crouched welder's hands, every few seconds.
welding_fx()
{
	self endon( "death" );
	level endon( "coop_mission_over" );
	if ( !isDefined( level._effect ) || !isDefined( level._effect[ "coop_training_welding" ] ) )
		return;
	for ( ;; )
	{
		spot = self.origin + anglesToForward( self.angles ) * 22 + ( 0, 0, 26 );
		playFx( level._effect[ "coop_training_welding" ], spot );
		wait( 1.5 + randomFloat( 2.5 ) );
	}
}

precache_cast()
{
	precache_props();
	// The keyed characters (cast_character); every other cast member's
	// character is precached by his classname's aitype at level load.
	character\character_sas_woodland_mac::precache();
	character\character_codo_complete_tf_tan::precache();
	character\character_codo_tf_ustshirt_clean_yel::precache();
}

// The idle a cast member stands in, and the gesture he talks with.
//
// The common FALLBACK idle of a cast member.  The authored idles themselves
// (and the instructors' scene sets) are played by _coopscene_killhouse since
// codol_coopClientMapName puts this mission's animlist on the client
// (2026-10-14).  What follows is the reason these fallbacks exist:
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
		case "price2":
		case "driver":
		case "price":
			return %casual_stand_v2_idle;
		// The recording has Mac and the three recruits crouched at the
		// obstacle course's start line.
		case "mac":
		case "buddy1":
		case "buddy2":
		case "buddy3":
		case "chair_guy":
			return %casual_crouch_idle;
		case "sas1":
		case "sas2":
		case "sas3":
			return %exposed_aim_5; // exposed_idle_alert_v1 is additive: bind pose
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
// a teammate.  C1 draws no name for an actor; it draws one over a script
// model whose entityState +0x64 names a client slot (C1 0x43a3b0, gated by
// client dvar "772"), at the model's origin + 82.  codol_setOverheadName
// (native, src/codol/script/codol_script_overhead_name.c) gives an
// invisible model a slot no player occupies, holding the name.  The names
// are the map's own script_friendname values.
//
// How C1 draws them (MEASURED, static disassembly of C1.exe):
//   * 0x43a3b0 (the mover path) draws EVERY such model, every frame, at full
//     alpha -- there is no look test on that path.  The look test with its
//     fade belongs to the teammate path 0x43a140 (players only: same team,
//     0x439ef0 look check, fade timer, 0x439e90 alpha).
//   * The colour comes from 0x43e260: dvar "291" for a name on the viewer's
//     team (0.4 0.6 0.85), "292" for any other team (0.75 0.25 0.25, red),
//     "1250" for a party member, "1257" for team 6.  A borrowed name slot has
//     no clientState, so its team is 0: never the viewer's, hence red.
// So the look-only behaviour is made here: each player has ONE name model,
// shown only to him (showToPlayer), placed over the named NPC he is looking
// at and hidden otherwise (look_nametag_player).  And because no other
// player in a coop match is ever on another team, each coop client's "292"
// is set to green: the only names C1 colours with it are these.
// +set scr_coop_cast_nametags 0 turns them off.
nametag( name, origin )
{
	// The cast's names are drawn by the per-player look model; nothing is
	// placed per NPC any more.
	return undefined;
}

// Every player is told to draw those names (C1 dvar "772", default 0) in
// green ("292", see above), and gets his look model.
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
	// As training_loadout_player: the spawn can come before this waits.
	wait( 0.05 );
	if ( !isAlive( self ) || self.sessionstate != "playing" )
		self waittill( "spawned_player" );
	self setClientDvar( "772", "1" );
	self setClientDvar( "292", "0.4 0.85 0.4 1" );
	self thread look_nametag_player();
}

// The named NPC the player is looking at: within 1500 units, his line of
// sight passing within 28 units of the NPC's chest or head, nothing solid in
// between; the nearest such.
looked_at_cast()
{
	eye = self getEye();
	forward = anglesToForward( self getPlayerAngles() );
	best = undefined;
	bestDist = 1500;
	list = level.coop_cast_list;
	if ( isDefined( level.coop_intro_driver ) )
	{
		list = [];
		for ( i = 0; i < level.coop_cast_list.size; i++ )
			list[ list.size ] = level.coop_cast_list[ i ];
		list[ list.size ] = level.coop_intro_driver;
	}
	for ( i = 0; i < list.size; i++ )
	{
		ai = list[ i ];
		if ( !isDefined( ai ) || !isDefined( ai.coop_name ) )
			continue;
		for ( h = 0; h < 2; h++ )
		{
			point = ai.origin + ( 0, 0, 36 + h * 28 );
			d = point - eye;
			dist = length( d );
			if ( dist < 1 || dist >= bestDist )
				continue;
			along = vectorDot( d, forward );
			if ( along <= 0 )
				continue;
			off = dist * dist - along * along;
			if ( off > 28 * 28 )
				continue;
			if ( !bulletTracePassed( eye, point, false, ai ) )
				continue;
			best = ai;
			bestDist = dist;
			break;
		}
	}
	return best;
}

// The player's look model: over the named NPC he looks at (held 0.6 s after
// he looks away, so a glance does not flicker it), shown to him alone.
look_nametag_player()
{
	self endon( "disconnect" );
	level endon( "coop_mission_over" );
	tag = spawn( "script_model", self.origin );
	tag setModel( "tag_origin" );
	tag hide();
	shownNpc = undefined;
	lastSeen = 0;
	for ( ;; )
	{
		wait( 0.1 );
		target = undefined;
		if ( isAlive( self ) && isDefined( level.coop_cast_list ) )
			target = self looked_at_cast();
		if ( isDefined( target ) )
			lastSeen = getTime();
		else if ( isDefined( shownNpc ) && getTime() - lastSeen < 600 )
			target = shownNpc;
		if ( !isDefined( target ) )
		{
			if ( isDefined( shownNpc ) )
			{
				tag hide();
				shownNpc = undefined;
				codolprintln( "CoDOL coop training: " + self.name + " name tag hidden" );
			}
			continue;
		}
		if ( !isDefined( shownNpc ) || shownNpc != target )
		{
			tag codol_setOverheadName( target.coop_name );
			shownNpc = target;
			tag showToPlayer( self );
			codolprintln( "CoDOL coop training: " + self.name + " looks at " + target.coop_name );
		}
		// C1 draws the name 82 above the model; the recording's names sit
		// level with the speaker's head and shoulders, not over his hat.
		tag.origin = target.origin - ( 0, 0, 20 );
	}
}

// Talk: the speaker's own talk clip from the killhouse scene table (CoD4
// gaz_animation: killhouse_gaz_talk_side / point_side / idleA; Price's
// killhouse_price_stand_talk), else the casual talking twitch, for the line's
// length; then back to his idle.
gesture( time )
{
	self maps\_coopscene_killhouse::talk( time );
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
	kind = spawnStruct();
	kind.faction = "cast";
	kind.role = "pistol";
	kind.health = 1000;
	kind.accuracy = 0;
	kind.character = cast_character( key );
	if ( isDefined( spawner.script_char_index ) )
		kind.char_index = int( spawner.script_char_index );

	ai = maps\_coopmission::spawn_class_actor( spawner.classname, "allies", kind );
	if ( !isDefined( ai ) )
	{
		codolprintln( "CoDOL coop training: could not spawn cast member " + key );
		return undefined;
	}

	angles = spawner.angles;
	if ( !isDefined( angles ) )
		angles = ( 0, 0, 0 );
	origin = spawner.origin;
	// CoD4 chair_guy_setup: Lootz sits at chair_guy_origin (1548 -688).
	if ( key == "chair_guy" )
		origin = ( 1548, -688, 4 );
	ai forceTeleport( origin, angles );
	ai setCanDamage( false );
	ai.ignoreall = true;
	ai.ignoreme = true;
	ai.coop_cast = key;
	// The common idle (idle_anim) is the fallback; the scene idle comes from
	// the killhouse scene table or the spawner's authored animation, now that
	// this mission's animlist reaches the client (codol_coopClientMapName),
	// and is replayed cycle by cycle (_coopscene_killhouse idle_loop).
	fallback = idle_anim( spawner.animation, key );
	if ( !isDefined( fallback ) )
		fallback = %casual_stand_idle;
	// Mac and the recruits crouch at the obstacle line (the recording):
	// they keep their crouch while Mac talks.
	if ( key == "mac" || key == "buddy1" || key == "buddy2" || key == "buddy3" )
		ai.coop_crouched = true;
	ai.coop_idle_anim = fallback;
	ai maps\_coopscene_killhouse::start_idle( key, spawner.animation, fallback );
	if ( isDefined( spawner.script_friendname ) )
	{
		ai.coop_name = spawner.script_friendname;
		ai.coop_nametag = nametag( ai.coop_name, ai.origin );
	}
	ai setAnimKnobAll( ai.coop_idle_anim, %body, 1, 0.1, 1 );
	ai notify( "coop_cast_pose" );
	prop = prop_for( key );
	if ( isDefined( prop ) && !isDefined( level.coop_training_prop_done[ key ] ) )
	{
		// One basketball for the pair that share the key.
		level.coop_training_prop_done[ key ] = true;
		ai attach( prop, "tag_inhand" );
		if ( prop == "machinery_welder_handle" )
			ai thread welding_fx();
	}
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
	level.coop_training_prop_done = [];
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
		if ( pass == 1 )
			spawn_recruits();
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
			// The recruits at the obstacle line are placed below.
			if ( key == "buddy" )
				continue;
			if ( pass == 1 )
			{
				// Ambient soldiers: the ones with an authored idle animation.
				if ( !isDefined( idle_anim( e.animation ) ) || ambient >= 15 )
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

// The three recruits (buddy1..3) crouch on their crouch nodes at the
// obstacle course's start line, in T-shirts, and stay there while the
// player runs (the recording's minimap keeps them at the line).  One wears
// the name the recording shows over him.
spawn_recruits()
{
	for ( n = 1; n <= 3; n++ )
	{
		spawner = getEnt( "buddy" + n, "targetname" );
		if ( !isDefined( spawner ) || isDefined( spawner.coop_cast_done ) )
			continue;
		spawner.coop_cast_done = true;
		spot = spawnStruct();
		spot.classname = spawner.classname;
		spot.origin = spawner.origin;
		spot.angles = spawner.angles;
		if ( isDefined( spawner.target ) )
		{
			node = getNode( spawner.target, "targetname" );
			if ( isDefined( node ) )
			{
				spot.origin = node.origin;
				spot.angles = node.angles;
			}
		}
		if ( n == 2 )
			spot.script_friendname = "Pvt. Ritchie";
		key = "buddy" + n;
		ai = spawn_cast( spot, key );
		if ( isDefined( ai ) )
		{
			level.coop_cast[ key ] = ai;
			level.coop_cast_list[ level.coop_cast_list.size ] = ai;
		}
	}
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
			spot.origin = maps\_coopvehicle::ground_under( mover.origin + anglesToRight( mover.angles ) * -72 + anglesToForward( mover.angles ) * 8 );
			spot.angles = ( 0, mover.angles[ 1 ] - 90, 0 );
		}
		if ( isDefined( level.coop_intro_driver ) )
		{
			// The actor at the wheel climbs out: unlinked, back under normal
			// actor movement, standing at the driver's door.
			ai = level.coop_intro_driver;
			level.coop_intro_driver = undefined;
			// IW4 maps\_humvee position 0 getout: humvee_driver_climb_out from
			// the seat, its own root motion carrying him out of the door,
			// then on his feet by the jeep in Price's stand idle.
			// The clip's root ends below the seat by more than the seat's
			// height (MEASURED: 38.7 -> -34 on the probe); he stands on
			// the ground under where it put him.
			seatZ = ai.origin[ 2 ];
			if ( ai climb_out_of_jeep() > 0 && isAlive( ai ) )
				spot.origin = maps\_coopvehicle::ground_under( ( ai.origin[ 0 ], ai.origin[ 1 ], seatZ ) );
			ai forceTeleport( spot.origin, spot.angles );
			ai.coop_cast = "driver";
			ai maps\_coopscene_killhouse::start_idle( "driver", undefined, idle_anim( undefined, "driver" ) );
			ai setAnimKnobAll( ai.coop_idle_anim, %body, 1, 0.2, 1 );
			ai notify( "coop_cast_pose" );
			if ( isDefined( ai.coop_nametag ) )
			{
				ai.coop_nametag unlink();
				ai.coop_nametag.origin = ai.origin;
			}
			codolprintln( "CoDOL coop training: Price out of the jeep at " + ai.origin );
			level.coop_cast[ "driver" ] = ai;
			level.coop_cast_list[ level.coop_cast_list.size ] = ai;
			return;
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

// Price gets out at the driver's door: the climb-out clip played from the
// seat with its root motion (noclip: the clip carries him over the door
// sill and down), the ride released first.  Without the clip he is simply
// placed by the door.
climb_out_of_jeep()
{
	self endon( "death" );
	seat = self.origin;
	self maps\_coopscene_killhouse::stop_idle();
	self codol_actorRide();
	self animMode( "none" );
	// The jeep's driver-side door clip with his climb-out (IW4 _humvee
	// set_vehicle_anims position 0 vehicle_getoutanim).
	if ( isDefined( level.coop_intro ) && isDefined( level.coop_intro.mover ) )
		level.coop_intro.mover maps\_coopintro::door_anim( "uaz_driver_exit_into_run_door" );
	len = maps\_coopanim::anim_single_solo( self, "getout", undefined, "noclip" );
	if ( len > 0 )
	{
		wait( 0.1 );
		self stopanimscripted();
	}
	codolprintln( "CoDOL coop training: Price climbed out (" + len + " s) from " + seat + " to " + self.origin );
	return len;
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
	maps\_cooptraininghud::marker( point );
}

// A new task: the recording's objective line (the zone's own localized
// string, top middle) and its 3D marker; `log` is the English the server log
// and the probe read.
task( text, log, point, ent, height )
{
	codolprintln( "CoDOL coop mission: " + log );
	maps\_cooptraininghud::objective_text( text );
	if ( isDefined( point ) || isDefined( ent ) )
		maps\_cooptraininghud::marker( point, ent, height );
}

hint_list( a, b, c )
{
	l = [];
	l[ 0 ] = a;
	if ( isDefined( b ) )
		l[ 1 ] = b;
	if ( isDefined( c ) )
		l[ 2 ] = c;
	return l;
}

// The recording's first hints, off the jeep: look up, look down, the move
// keys.
look_hints()
{
	return hint_list( &"KILLHOUSE_LOOK_UP", &"KILLHOUSE_LOOK_DOWN", &"KILLHOUSE_HINT_MOVE_AROUND" );
}

// The throwing hint, then -- while windows are still to be hit after a few
// tries -- the "aim above the target" hint, as the recording shows them
// (3:39-3:52).
too_low_hint( first, low )
{
	level endon( "coop_windows_over" );
	level endon( "coop_mission_over" );
	if ( isDefined( first ) )
	{
		maps\_cooptraininghud::hint( first, 8 );
		wait( 8 );
	}
	wait( 10 );
	maps\_cooptraininghud::hint( low, 8 );
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
	while ( !maps\_coopmission::flag( name ) && waited < timeout )
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
		// A brush whose health reaches 0 stops sending "damage" (a knife does
		// 200): keep it high, as CoD4 / IW4 reset target health to 1000.
		targets[ i ].health = 100000;
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
		if ( isDefined( level.coop_training_hip_only ) && attacker playerADS() > 0.5 )
		{
			level notify( "coop_training_ads_hit" );
			continue;
		}
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
		players[ i ] maps\_coopmode::coop_give_weapon( weapon, isDefined( slot ) && slot );
	}
	report_inventory();
}

// Each player's weapons, for the log and the probe's two-primary check.
report_inventory()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( !isAlive( players[ i ] ) )
			continue;
		primaries = players[ i ] getWeaponsListPrimaries();
		text = "";
		for ( k = 0; k < primaries.size; k++ )
			text += " " + primaries[ k ];
		codolprintln( "CoDOL coop training: " + players[ i ].name + " carries " + primaries.size + " primaries:" + text );
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
	maps\_cooptraininghud::precache();
	// The Spec Ops gates (so_gate: a brush across the demolition pit's exit
	// at 1868 -2484 and the pit course entrance at 391 -884) belong to
	// so_killspree_trainer, not to this mission.
	remove_named( "so_gate" );
	// CoD4 killhouse main(): silently_lowerPlywoodWalls, the melon hidden
	// (melon_think), the chair guy at chair_guy_origin (chair_guy_setup).
	plywood_walls( false );
	hide_melee_dummy();
	level thread show_nametags();
	// The road gates are open for the jeep from the start: the double car
	// gates at camp (slide_gate_01 / _02) and the east road gate
	// (slide_gate_03 at x 4466); the jeep path runs through all three.
	open_slide_gate( "slide_gate_03", ( 4466, -1360, 59 ) );
	open_slide_gate( "slide_gate_01" );
	open_slide_gate( "slide_gate_02" );
	// The recording's lights: the floor arrows, the station-one marker and
	// the demolition windows light only while they are the way or the
	// target; the range dummies lie flat until a lesson raises them.
	glow_setup();
	range_targets_setup();
	// The range's sheet-metal screen (script_noteworthy range_metalwall,
	// across lane one at y -455) belongs to a penetration lesson the
	// recording does not have; standing, it took every shot at the far
	// dummy (MEASURED, _cooptest aimcheck / colmap: ent 672 at 16..56).
	range_screen( "range_metalwall", false );
	level thread training_loadout_watch();
	codolprintln( "CoDOL coop training: mission flow active" );
}

// --------------------------------------------------------------------------
// The player's weapons.  The recording's rookie arrives with his pistol and
// his offhands only, weapons down: he walks the camp and the range
// empty-handed, picks the rifle off the range table, and is told to pull
// out the pistol he has had all along.
// --------------------------------------------------------------------------

training_pistol()
{
	return first_mounted( "usp_so", "usp_silencer_so" );
}

training_rifle()
{
	return first_mounted( "m4techgrip_eotech_so", "m4_grunt_reflex_so" );
}

training_loadout_watch()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread training_loadout_player();
	}
}

training_loadout_player()
{
	self endon( "disconnect" );
	// spawnPlayer runs in the same frame as the "connected" notify and can
	// notify spawned_player before this thread waits for it (MEASURED with a
	// test client), so a player already in the world is armed now.
	wait( 0.05 );
	if ( isAlive( self ) && self.sessionstate == "playing" )
		self training_loadout();
	for ( ;; )
	{
		self waittill( "spawned_player" );
		self training_loadout();
	}
}

// Frags in the lethal slot.  A grenade reaches C1's frag button only through
// setOffhandEquipment (as _coopmode giveStartingLoadout and the multiplayer
// _class::codol_giveEquipment do); giveWeapon alone left the player with
// nothing to throw at Newcastle's windows (user report 2026-10-08).
training_give_frags()
{
	frag = level.coop_training_frags;
	if ( !isDefined( frag ) || !codol_isWeaponMounted( frag ) )
		return;
	self giveWeapon( frag );
	self giveMaxAmmo( frag );
	self setOffhandEquipment( frag, "none", "none" );
	codolprintln( "CoDOL coop training: " + self.name + " given " + frag );
}

training_loadout()
{
	pistol = training_pistol();
	// The rookie carries the pistol and nothing else: no grenades, specials
	// or equipment from the coop class (user, 2026-10-08: "it should just
	// give me frag grenades when I get to the grenade stage").  Frags come
	// from Newcastle (station_demolitions sets level.coop_training_frags);
	// a player who (re)spawns after that gets them back.
	weapons = self getWeaponsList();
	for ( i = 0; i < weapons.size; i++ )
	{
		if ( weapons[ i ] != pistol && weapons[ i ] != "none" )
			self takeWeapon( weapons[ i ] );
	}
	self setOffhandEquipment( "none", "none", "none" );
	if ( isDefined( level.coop_training_frags ) )
		self training_give_frags();
	if ( codol_isWeaponMounted( pistol ) && !self hasWeapon( pistol ) )
	{
		self giveWeapon( pistol );
		self giveMaxAmmo( pistol );
	}
	if ( isDefined( level.coop_training_rifle ) )
	{
		// A teammate who (re)spawns after the range has the rifle too.
		self maps\_coopmode::coop_give_weapon( level.coop_training_rifle, true );
		self enableWeapons();
	}
	else
	{
		self switchToWeapon( pistol );
		if ( !isDefined( self.coop_in_intro ) )
			self disableWeapons();
	}
	report_inventory();
}

weapons_down_all( down )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( !isAlive( players[ i ] ) )
			continue;
		if ( down )
			players[ i ] disableWeapons();
		else
			players[ i ] enableWeapons();
	}
	codolprintln( "CoDOL coop training: weapons down " + down );
}

// --------------------------------------------------------------------------
// Glowing markers and floor arrows.  The base map carries them as
// script_brushmodels: the floor chevrons (script_noteworthy
// navigation_arrows: arrows_to_hanger, _station_one, _newcastle, _obstacle,
// _end, _price, _debrief, _heli), the range stall's marker and number
// (obj_station_one_glowing / _number), the demolition windows
// (glowing_window_two .. _seven) and wall one (glowing_wall_one).  All start
// hidden; each lights while the recording shows it lit.
// --------------------------------------------------------------------------

glow_setup()
{
	level.coop_training_arrow_names = strTok( "arrows_to_hanger arrows_to_station_one arrows_to_newcastle arrows_to_obstacle arrows_to_end arrows_to_price arrows_to_debrief arrows_to_heli", " " );
	glows = strTok( "obj_station_one_glowing obj_station_one_number glowing_window_two glowing_window_three glowing_window_four glowing_window_five glowing_window_six glowing_window_seven glowing_wall_one", " " );
	names = level.coop_training_arrow_names;
	for ( i = 0; i < glows.size; i++ )
		names[ names.size ] = glows[ i ];
	n = 0;
	for ( i = 0; i < names.size; i++ )
	{
		parts = getEntArray( names[ i ], "targetname" );
		for ( j = 0; j < parts.size; j++ )
		{
			parts[ j ] hide();
			parts[ j ] notSolid();
			n++;
		}
	}
	codolprintln( "CoDOL coop training: " + n + " glow / arrow brush(es) hidden" );
}

glow( name, on )
{
	parts = getEntArray( name, "targetname" );
	for ( i = 0; i < parts.size; i++ )
	{
		if ( on )
			parts[ i ] show();
		else
			parts[ i ] hide();
	}
	codolprintln( "CoDOL coop training: glow " + name + " " + on + " (" + parts.size + ")" );
}

// Light one set of floor arrows (undefined: none).
arrows( name )
{
	if ( isDefined( level.coop_training_arrow_lit ) && isDefined( name ) && name == level.coop_training_arrow_lit )
		return;
	if ( isDefined( level.coop_training_arrow_lit ) )
		glow( level.coop_training_arrow_lit, false );
	level.coop_training_arrow_lit = name;
	if ( isDefined( name ) )
		glow( name, true );
}

// --------------------------------------------------------------------------
// The range dummies (lane1, ads_tutorial_target) pop up and lie down as the
// recording's do.  CoD4 killhouse targetDummyThink lowers each one at the
// start and moveTargetDummy raises it (rotatePitch on a pivot entity); this
// map's dummies have their pivot at the brush origin, on the floor, and face
// down the range along y, so they tip about x: rotateRoll -90 lays one flat
// away from the shooter, +90 stands it up.
// --------------------------------------------------------------------------

range_targets_setup()
{
	level.coop_training_lane = getEntArray( "lane1", "targetname" );
	level.coop_training_ads = by_noteworthy( "ads_tutorial_target" );
	all = range_targets_all();
	for ( i = 0; i < all.size; i++ )
	{
		all[ i ].coop_raised = true;
		all[ i ] range_target( false, true );
	}
}

range_targets_all()
{
	all = [];
	for ( i = 0; i < level.coop_training_lane.size; i++ )
		all[ all.size ] = level.coop_training_lane[ i ];
	for ( i = 0; i < level.coop_training_ads.size; i++ )
		all[ all.size ] = level.coop_training_ads[ i ];
	return all;
}

range_target( up, silent )
{
	if ( self.coop_raised == up )
		return;
	self.coop_raised = up;
	if ( up )
	{
		self rotateRoll( 90, 0.25, 0.1, 0.1 );
		self playSound( "killhouse_target_up_metal" );
	}
	else if ( isDefined( silent ) && silent )
		self rotateRoll( -90, 0.05 );
	else
	{
		self rotateRoll( -90, 0.5, 0.2, 0.2 );
		self playSound( "killhouse_target_up" );
	}
}

range_targets( targets, up )
{
	for ( i = 0; i < targets.size; i++ )
		targets[ i ] range_target( up );
	wait( 0.5 );
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
	// Price at the wheel before the jeep sets off (_coopintro waits for him).
	seat_driver();
	level thread ambient_chatter();

	// Station 0: the jeep (_coopintro plays Price's lines from the jeep).
	if ( isDefined( level.coop_intro ) && !level.coop_intro.done )
		level waittill( "coop_intro_done" );
	// Off the jeep empty-handed (the ride's end raises the riders' weapons).
	if ( !isDefined( level.coop_training_rifle ) )
		weapons_down_all( true );
	spawn_driver();

	station_range();
	station_demolitions();
	station_obstacle();
	station_price();

	maps\_coopmission::mission_complete( "training complete" );
}

// --------------------------------------------------------------------------
// CoD4 killhouse helpers.
// --------------------------------------------------------------------------

// Move a cast member to a node, as CoD4 sends Gaz and Newcastle between
// their nodes (anim_reach / setgoalnode).  The coop actors stand still (no
// route), so they are placed there.
move_cast( ai, origin, yaw )
{
	if ( !isDefined( ai ) )
		return;
	maps\_coopanim::stop_scripted( ai );
	ai forceTeleport( origin, ( 0, yaw, 0 ) );
	codolprintln( "CoDOL coop training: " + ai.coop_cast + " to " + origin );
}

// A trigger touched by a living player (CoD4 `trigger waittill( "trigger" )`),
// or the time-out.  True when touched.
trigger_wait( trig, timeout, what )
{
	if ( !isDefined( trig ) )
		return false;
	step( "trigger", trig, what );
	trig endon( "death" );
	waited = 0;
	while ( waited < timeout )
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( isAlive( players[ i ] ) && players[ i ] isTouching( trig ) )
				return true;
		}
		wait( 0.25 );
		waited += 0.25;
	}
	codolprintln( "CoDOL coop training: " + what + " timed out" );
	return false;
}

// Nag with `nag` every `every` seconds until the trigger is touched.
trigger_wait_nag( trig, timeout, what, speaker, nag )
{
	waited = 0;
	for ( ;; )
	{
		if ( trigger_wait( trig, 10, what ) )
			return true;
		waited += 10;
		if ( waited >= timeout )
			return false;
		say( speaker, nag.alias, nag.time );
	}
}

// Some living player holding `weapon` as his current weapon (CoD4
// `getCurrentWeapon() == ...`), or the time-out.
weapon_wait( weapon, timeout, what )
{
	step( "weapon", weapon, what );
	waited = 0;
	while ( waited < timeout )
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( isAlive( players[ i ] ) && players[ i ] getCurrentWeapon() == weapon )
				return true;
		}
		wait( 0.25 );
		waited += 0.25;
	}
	codolprintln( "CoDOL coop training: " + what + " timed out" );
	return false;
}

// CoD4 frag_trigger_think / launcher_trigger_think: each trigger_damage of
// the windows fires once, with the buzzer; returns when all have, or at
// the time-out.
//
// The recording lights each window still to be hit (its glowing frame) and
// puts an objective marker on it (objectives 1..3 beside the safety pit's
// 0); both go when it is hit.  `glows` names the frame brush per trigger.
damage_triggers_wait( trigs, timeout, what, glows )
{
	level.coop_training_windows = 0;
	for ( i = 0; i < trigs.size; i++ )
	{
		trigs[ i ].coop_hit = false;
		trigs[ i ].coop_glow = undefined;
		trigs[ i ].coop_obj = i + 1;
		if ( isDefined( glows ) && isDefined( glows[ i ] ) )
		{
			trigs[ i ].coop_glow = glows[ i ];
			glow( glows[ i ], true );
		}
		objective_delete( i + 1 );
		objective_add( i + 1, "active", trigs[ i ].origin );
		maps\_cooptraininghud::target_marker( i + 1, trigs[ i ].origin );
		trigs[ i ] enableGrenadeTouchDamage();
		trigs[ i ] thread damage_trigger_think();
	}
	step( "damage", trigs, what );
	level thread window_grenade_watch( trigs );
	level thread demolition_ammo_refill();
	waited = 0;
	while ( level.coop_training_windows < trigs.size && waited < timeout )
	{
		wait( 0.25 );
		waited += 0.25;
	}
	level notify( "coop_windows_over" );
	for ( i = 0; i < trigs.size; i++ )
	{
		trigs[ i ] notify( "coop_stop_window" );
		trigs[ i ] window_done();
	}
	codolprintln( "CoDOL coop training: " + what + " " + level.coop_training_windows + "/" + trigs.size );
	return level.coop_training_windows >= trigs.size;
}

// A window's frame stops glowing and its marker goes.
window_done()
{
	if ( isDefined( self.coop_glow ) )
	{
		glow( self.coop_glow, false );
		self.coop_glow = undefined;
	}
	if ( isDefined( self.coop_obj ) )
	{
		maps\_cooptraininghud::target_marker_clear( self.coop_obj );
		objective_delete( self.coop_obj );
		self.coop_obj = undefined;
	}
}

ent_name( e )
{
	if ( isDefined( e.targetname ) )
		return e.targetname;
	if ( isDefined( e.script_noteworthy ) )
		return e.script_noteworthy;
	return "ent " + e getEntityNumber();
}

// Real grenades never fired the windows' trigger_damage on a live server
// (user report 2026-10-08: frags thrown through the windows, no feedback;
// the VPS log showed the station waiting with no hit), so every live grenade
// -- thrown frags and launcher rounds alike, which no longer notify
// grenade_fire once they have an arming distance -- is checked against each
// open window's volume every frame as well.
window_grenade_watch( trigs )
{
	level endon( "coop_windows_over" );
	level endon( "coop_mission_over" );
	for ( ;; )
	{
		nades = getEntArray( "grenade", "classname" );
		for ( n = 0; n < nades.size; n++ )
		{
			nade = nades[ n ];
			for ( i = 0; i < trigs.size; i++ )
			{
				t = trigs[ i ];
				if ( t.coop_hit || !nade isTouching( t ) )
					continue;
				who = undefined;
				if ( isDefined( nade.owner ) )
					who = nade.owner;
				t notify( "coop_stop_window" );
				t window_hit( who );
			}
		}
		wait( 0.05 );
	}
}

// Out of frags or launcher rounds during demolitions: topped up, so a player
// can keep trying for the windows (user request 2026-10-08).
demolition_ammo_refill()
{
	level endon( "coop_windows_over" );
	level endon( "coop_mission_over" );
	for ( ;; )
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( !isAlive( p ) )
				continue;
			frag = level.coop_training_frags;
			if ( isDefined( frag ) && p hasWeapon( frag ) && p getAmmoCount( frag ) < 1 )
			{
				p giveMaxAmmo( frag );
				codolprintln( "CoDOL coop training: " + p.name + " frags refilled" );
			}
			weapons = p getWeaponsListPrimaries();
			for ( w = 0; w < weapons.size; w++ )
			{
				gl = weaponAltWeaponName( weapons[ w ] );
				if ( isDefined( gl ) && gl != "none" && gl != "" && p getAmmoCount( gl ) < 1 )
				{
					p giveMaxAmmo( gl );
					codolprintln( "CoDOL coop training: " + p.name + " " + gl + " refilled" );
				}
			}
		}
		wait( 0.5 );
	}
}

damage_trigger_think()
{
	self endon( "coop_stop_window" );
	self waittill( "trigger", who );
	self window_hit( who );
}

window_hit( who )
{
	if ( self.coop_hit )
		return;
	self.coop_hit = true;
	level.coop_training_windows++;
	self window_done();
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] playLocalSound( "killhouse_buzzer" );
	who_name = "?";
	if ( isDefined( who ) && isPlayer( who ) )
		who_name = who.name;
	codolprintln( "CoDOL coop training: window " + ent_name( self ) + " hit by " + who_name );
}

// --------------------------------------------------------------------------
// Station 1/2: the range (CoD4 killhouse.gsc navigationTraining,
// rifleTraining, rifle_hip_shooting, rifle_penetration_shooting,
// rifle_timed_shooting, sideArm_Training, melee_training).  Tiny Man has
// Gaz's part (waters_think, script_noteworthy "waters"); C1's pit_tiny lines
// are Gaz's in CoD4 order.
// --------------------------------------------------------------------------

station_range()
{
	price = cast( "driver" );
	tiny = cast( "waters" );

	// navigationTraining: obj_enter_range at rifle_range_obj; the door opens
	// at at_rifle_range; Price (Gaz's "illletyouin" in CoD4) nags until a
	// player is in.
	obj = getEnt( "rifle_range_obj", "targetname" );
	if ( isDefined( obj ) )
		objective_at( obj.origin );
	// On foot by the jeep, Price sends the rookie on; the chevrons on the
	// yard light the way to the range door (its frame glows until it opens).
	arrows( "arrows_to_hanger" );
	task( &"KILLHOUSE_USE_YOUR_OBJECTIVE_INDICATOR", "Follow the objective marker to the shooting range" );
	// After the ride: look up, look down, then the move keys (recording 1:39-1:45).
	level thread maps\_cooptraininghud::hint_sequence( look_hints(), 3 );
	say( price, "pit_pri_003", 3.6 );
	nags = [];
	nags[ 0 ] = line( "pit_pri_004", 1.9 );
	nags[ 1 ] = line( "pit_pri_005", 2.6 );
	nags[ 2 ] = line( "pit_pri_006", 2.7 );
	n = 0;
	while ( !maps\_coopmission::flag( "at_rifle_range" ) )
	{
		flag_wait_timeout( "at_rifle_range", 15 );
		if ( maps\_coopmission::flag( "at_rifle_range" ) )
			break;
		if ( n < nags.size )
		{
			say( price, nags[ n ].alias, nags[ n ].time );
			n++;
		}
		else
			break;
	}
	open_range_door();
	raise_door( "arsenal_door", 120 );
	flag_wait_timeout( "inside_firing_range", 60 );
	codolprintln( "CoDOL coop training: station range" );

	// door_to_rifle_handler / rifleTraining: Gaz greets the player, sends
	// him to station one (obj_rifle_stall, rifleTraining_stall) and moves to
	// stationone_node once the player is past him (past_gaz).
	arrows( "arrows_to_station_one" );
	level thread tiny_to_station( tiny );

	// The rifle is the one on the table by Tiny Man (the addon's
	// range_m4tech: the foregrip and holographic sight the recording shows),
	// picked up by hand.  Tiny Man's two lines run back to back, as in the
	// recording; the station-one marker and its number light once the
	// rifle is in hand.
	// CoD4 door_to_rifle_handler: Gaz's intro (killhouse_gaz_intro) as he
	// greets the rookie; his lines run over it.
	if ( isDefined( tiny ) )
		tiny maps\_coopscene_killhouse::scene( "intro" );
	say( tiny, "pit_tiny_001", 2.8 );
	rifle = training_rifle();
	table = range_rifle( rifle );
	if ( isDefined( table ) )
		objective_at( table.origin );
	task( &"KILLHOUSE_PICK_UP_A_RIFLE_FROM", "Pick up a rifle from the table" );
	say( tiny, "pit_tiny_002", 2.0 );
	pickup_wait( rifle, table, 45, "the range rifle" );
	level.coop_training_rifle = rifle;
	give_all( rifle, true );
	weapons_down_all( false );

	glow( "obj_station_one_glowing", true );
	glow( "obj_station_one_number", true );
	stallObj = getEnt( "obj_rifle_stall", "targetname" );
	if ( isDefined( stallObj ) )
		objective_at( stallObj.origin );
	stall = getEnt( "rifleTraining_stall", "targetname" );
	task( &"KILLHOUSE_ENTER_STATION_NUMBER", "Enter station one and aim at the targets" );
	trigger_wait( stall, 30, "station one" );
	maps\_cooptraininghud::marker_clear();
	arrows( undefined );
	glow( "obj_station_one_glowing", false );
	glow( "obj_station_one_number", false );

	lane = level.coop_training_lane;
	ads = level.coop_training_ads;

	// rifle_hip_shooting: the lane's dummies pop up; from the hip (hits
	// while aiming down the sights do not count and draw "stop aiming"),
	// then the bullseye pops up for the sights.
	say( tiny, "pit_tiny_003", 2.7 );
	range_targets( lane, true );
	task( &"KILLHOUSE_SHOOT_EACH_TARGET_WHILE1", "Shoot each target from the hip" );
	level thread maps\_cooptraininghud::hint_sequence( hint_list( &"KILLHOUSE_HINT_HIP_ATTACK_PC", &"KILLHOUSE_HINT_CROSSHAIR_CHANGES" ), 4 );
	level.coop_training_hip_only = true;
	level thread hip_nags( tiny );
	if ( !wait_hits( lane, 3, 40 ) )
		say( tiny, "pit_tiny_015", 0.7 );
	level.coop_training_hip_only = undefined;
	level notify( "coop_hip_done" );
	range_targets( lane, false );
	say( tiny, "pit_tiny_005", 1.5 );
	say( tiny, "pit_tiny_006", 2.1 );
	range_targets( ads, true );
	task( &"KILLHOUSE_SHOOT_EACH_TARGET_WHILE", "Shoot the target while aiming down the sights" );
	level thread maps\_cooptraininghud::hint_sequence( hint_list( &"KILLHOUSE_HINT_ADS_THROW", &"KILLHOUSE_HINT_ADS_ACCURACY" ), 4 );
	wait_hits( ads, 3, 40 );
	range_targets( ads, false );
	say( tiny, "pit_tiny_007", 0.8 );

	// rifle_timed_shooting (the recording has no plywood lesson): the
	// dummies pop up one at a time; rounds until enough are hit in time,
	// with the "too slow" lines between.
	say( tiny, "pit_tiny_011_1", 2.5 );
	say( tiny, "pit_tiny_011_2", 1.8 );
	task( &"KILLHOUSE_SHOOT_EACH_TARGET_AS", "Shoot each target as quickly as you can" );
	maps\_cooptraininghud::hint( &"KILLHOUSE_HINT_RELOAD", 5 );
	say( tiny, "pit_tiny_012", 5.5 );
	tooSlow = [];
	tooSlow[ 0 ] = line( "pit_tiny_014", 1.3 );
	tooSlow[ 1 ] = line( "pit_tiny_015", 0.7 );
	tooSlow[ 2 ] = line( "pit_tiny_016", 0.8 );
	tooSlow[ 3 ] = line( "pit_tiny_017", 3.4 );
	for ( round = 0; round < 4; round++ )
	{
		if ( popup_round( 8, 6 ) )
			break;
		if ( round == 0 )
			say( tiny, "pit_tiny_013", 1.6 );
		else
		{
			l = tooSlow[ randomInt( tooSlow.size ) ];
			say( tiny, l.alias, l.time );
		}
		wait( 2 );
	}
	say( tiny, "pit_tiny_018", 0.8 );

	// sideArm_Training as the recording plays it: no pickup -- the pistol
	// is already carried -- "pull out your sidearm", then "good, now switch
	// to your rifle", with the lane's dummies up.
	pistol = training_pistol();
	give_all( pistol, false );
	range_targets( lane, true );
	say( tiny, "pit_tiny_021", 1.2 );
	task( &"KILLHOUSE_SWITCH_TO_YOUR_PISTOL", "Switch to your sidearm" );
	maps\_cooptraininghud::hint( &"KILLHOUSE_HINT_SIDEARM_SWAP", 6 );
	weapon_wait( pistol, 20, "pull out the sidearm" );
	// The recording's next task is a pistol round on the raised dummies
	// (2:59-3:05), then the switch back.
	task( &"KILLHOUSE_SHOOT_EACH_TARGET_PISTOL", "Shoot each target with your pistol" );
	wait_hits( lane, 3, 20 );
	say( tiny, "pit_tiny_020", 2.0 );
	maps\_cooptraininghud::hint( &"KILLHOUSE_HINT_PRIMARY_SWAP", 6 );
	weapon_wait( rifle, 20, "switch to the rifle" );
	say( tiny, "pit_tiny_022", 3.3 );
	range_targets( lane, false );
	maps\_coopmission::flag_set( "sidearm_complete" );

	// melee_training: the "watermelon" -- melee_target_dummy, hidden until
	// now as CoD4 hides the melon (melon_think) -- at the stall; near_melee
	// (the trigger_radius at 1888 -844), then a knife.
	melee = melee_dummies();
	for ( i = 0; i < melee.size; i++ )
	{
		melee[ i ] show();
		melee[ i ] solid();
	}
	objective_at( ( 1887, -854, 35 ) );
	maps\_cooptraininghud::marker_clear();
	say( tiny, "pit_tiny_023", 1.4 );
	flag_wait_timeout( "near_melee", 30 );
	task( &"KILLHOUSE_MELEE_THE_TARGET", "Knife the target" );
	maps\_cooptraininghud::hint( &"KILLHOUSE_HINT_MELEE_TARGET", 6 );
	say( tiny, "pit_tiny_024", 1.0 );
	if ( melee.size > 0 )
		wait_hits( melee, 1, 45, true );
	for ( i = 0; i < melee.size; i++ )
	{
		melee[ i ] hide();
		melee[ i ] notSolid();
	}
	arrows( "arrows_to_newcastle" );
	say( tiny, "pit_tiny_025", 3.7 );
	maps\_coopmission::flag_set( "melee_complete" );
	maps\_coopmission::flag_set( "start_frag_training" );
}

melee_dummies()
{
	melee = getEntArray( "melee_target_dummy", "targetname" );
	return melee;
}

// The melon is hidden until the knife lesson (CoD4 melon_think); in coop it
// also stands in front of the station-one stall, where it took every shot
// meant for the lanes (MEASURED, _cooptest aimcheck: ent 699 on every line).
hide_melee_dummy()
{
	melee = melee_dummies();
	for ( i = 0; i < melee.size; i++ )
	{
		melee[ i ] hide();
		melee[ i ] notSolid();
	}
}

// CoD4 move_gaz_once_player_past.
tiny_to_station( tiny )
{
	level endon( "coop_mission_over" );
	flag_wait_timeout( "past_gaz", 60 );
	move_cast( tiny, ( 1866, -1103.5, 8 ), 89 );
}

// CoD4 targetDummyThink's hip-fire rule, as nags.
hip_nags( tiny )
{
	level endon( "coop_hip_done" );
	alt = 0;
	for ( ;; )
	{
		level waittill( "coop_training_ads_hit" );
		if ( alt == 0 )
			say( tiny, "pit_tiny_004_1", 2.0 );
		else
			say( tiny, "pit_tiny_004_2", 1.4 );
		alt = 1 - alt;
	}
}

waittill_or_timeout( note, timeout )
{
	self endon( note );
	wait( timeout );
}

// One timed round as the recording shows it: `pops` dummies stand up one at
// a time, each for up to three seconds or until it is hit; the round is
// passed with `needed` hits.
popup_round( pops, needed )
{
	all = range_targets_all();
	got = 0;
	last = -1;
	for ( n = 0; n < pops && all.size > 0; n++ )
	{
		k = randomInt( all.size );
		if ( k == last && all.size > 1 )
			k = ( k + 1 ) % all.size;
		last = k;
		t = [];
		t[ 0 ] = all[ k ];
		// The ADS dummy is two brushes (the bullseye and its board).
		if ( isDefined( all[ k ].script_noteworthy ) && all[ k ].script_noteworthy == "ads_tutorial_target" )
			t = level.coop_training_ads;
		range_targets( t, true );
		if ( wait_hits( t, 1, 3 ) )
			got++;
		range_targets( t, false );
	}
	codolprintln( "CoDOL coop training: pop-up round " + got + "/" + pops + " hit (" + needed + " needed)" );
	return got >= needed;
}

// The rifle on the range table: the addon's weapon entity (range_m4tech),
// or one placed where it lies if the entity is gone.
range_rifle( rifle )
{
	item = getEnt( "range_m4tech", "targetname" );
	if ( isDefined( item ) )
		return item;
	if ( !codol_isWeaponMounted( rifle ) )
		return undefined;
	item = spawn( "weapon_" + rifle, ( 1873, -797.5, 41 ) );
	if ( isDefined( item ) )
		item.angles = ( 0, 161.2, 90 );
	codolprintln( "CoDOL coop training: range rifle placed, " + isDefined( item ) );
	return item;
}

// Some living player has picked `weapon` up, or the time-out; the others
// are given it by give_all after.  Either the native world pickup (walking
// onto the item) or, as in the recording where the rifle is taken off the
// table from the floor beside it, the use key within reach of it -- the
// item lies on the table top, beyond the touch of a player standing at the
// table's edge (MEASURED: a test client stopped 35 units off and never
// touched it).
pickup_wait( weapon, item, timeout, what )
{
	step( "pickup", item, weapon );
	waited = 0;
	while ( waited < timeout )
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( !isAlive( p ) )
				continue;
			how = undefined;
			if ( p hasWeapon( weapon ) )
				how = "picked up";
			else if ( isDefined( item ) && distance( p.origin, item.origin ) < 96 && p useButtonPressed() )
				how = "took (use)";
			if ( isDefined( how ) )
			{
				codolprintln( "CoDOL coop training: " + p.name + " " + how + " " + what );
				if ( isDefined( item ) )
					item delete();
				return true;
			}
		}
		wait( 0.1 );
		waited += 0.1;
	}
	codolprintln( "CoDOL coop training: " + what + " pickup timed out" );
	return false;
}

// CoD4 silently_lowerPlywoodWalls / raisePlywoodWalls / lowerPlywoodWalls.
plywood_walls( up )
{
	walls = by_noteworthy( "plywood" );
	for ( i = 0; i < walls.size; i++ )
	{
		if ( up )
		{
			walls[ i ] rotateRoll( 90, 0.25, 0.1, 0.1 );
			walls[ i ] playSound( "killhouse_target_up_wood" );
		}
		else
			walls[ i ] rotateRoll( -90, 0.25, 0.1, 0.1 );
	}
	codolprintln( "CoDOL coop training: plywood " + up );
}

// --------------------------------------------------------------------------
// Station 3: demolitions with Sgt. Newcastle (CoD4 frag_Training and
// launcherTraining; C1 cut the C4 part and went straight to
// C4_complete_dialog).  The windows are the map's trigger_damage boxes:
// grenade_damage_trigger2/3/4 (windows 2, 3, 4), launcher_wall_trigger
// (wall 1), launcher_damage_trigger5/6/7 (windows 5, 6, 7).
// --------------------------------------------------------------------------

// Wall one, the launcher's arming-distance target: the free-standing wall
// with the number 1 in front of the safety pit (the recording, 3:59-4:10).
// The map stores it, its number and its trigger_damage 376 units under the
// ground (MEASURED, _cooptest bounds: glowing_wall_one -- the wall itself,
// 100 x 15 x 92 -- spans z -354..-262, wall_num_1 the "1" on its face
// -324..-278, launcher_wall_trigger at -307, ground there z 21.6), for the
// level script to raise.  Nothing did, so the wall was missing (user report
// 2026-10-08).  It stands for the whole station, solid.
raise_wall_one()
{
	if ( isDefined( level.coop_training_wall_raised ) )
		return;
	level.coop_training_wall_raised = true;
	parts = getEntArray( "glowing_wall_one", "targetname" );
	num = getEntArray( "wall_num_1", "targetname" );
	for ( i = 0; i < num.size; i++ )
		parts[ parts.size ] = num[ i ];
	trig = by_noteworthy( "launcher_wall_trigger" );
	for ( i = 0; i < trig.size; i++ )
		parts[ parts.size ] = trig[ i ];
	for ( i = 0; i < parts.size; i++ )
	{
		parts[ i ].origin = parts[ i ].origin + ( 0, 0, 376 );
		if ( parts[ i ].classname == "script_brushmodel" )
			parts[ i ] show();
	}
	walls = getEntArray( "glowing_wall_one", "targetname" );
	for ( i = 0; i < walls.size; i++ )
		walls[ i ] solid();
	codolprintln( "CoDOL coop training: wall one raised (" + parts.size + " parts)" );
}

station_demolitions()
{
	nwc = cast( "nwc" );
	frag_obj = getEnt( "obj_frag_ammo", "targetname" );
	if ( isDefined( frag_obj ) )
		objective_at( frag_obj.origin );
	task( &"KILLHOUSE_GO_OUTSIDE_AND_REPORT", "Report to Sgt. Newcastle" );
	open_doors( "gate_cqb_enter" );
	// The enter gate's stand-in collision (1959 -2267) stays behind otherwise.
	remove_named( "door_fake_collision" );
	raise_wall_one();
	flag_wait_timeout( "near_grenade_area", 120 );
	arrows( undefined );
	codolprintln( "CoDOL coop training: station demolitions" );

	say( nwc, "pit_ncastle_001", 2.1 );
	say( nwc, "pit_ncastle_002", 2.4 );
	level.coop_training_frags = first_mounted( "fraggrenade_training_so", "fraggrenade_so" );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isAlive( players[ i ] ) )
			players[ i ] training_give_frags();
	}
	open_doors( "gate_enter_grenade" );
	pit = getEnt( "safety_pit", "targetname" );
	if ( isDefined( pit ) )
		objective_at( pit.origin );
	task( &"KILLHOUSE_ENTER_THE_SAFETY_PIT", "Enter the safety pit" );
	maps\_cooptraininghud::hint( &"KILLHOUSE_HINT_FRAG", 6 );
	trigger_wait_nag( pit, 60, "safety pit", nwc, line( "pit_ncastle_003", 1.2 ) );
	// Newcastle watches from the platform above the pit, by the camera on
	// its tripod (a static model of the map).
	move_cast( nwc, ( 2075, -2364, 72 ), 270 );

	say( nwc, "pit_ncastle_004", 3.4 );
	task( &"KILLHOUSE_THROW_A_GRENADE_INTO", "Throw a grenade into windows two, three and four" );
	maps\_cooptraininghud::marker_clear();
	level thread too_low_hint( &"KILLHOUSE_HINT_FRAG", &"KILLHOUSE_HINT_GRENADE_TOO_LOW" );
	frags = [];
	fragGlows = [];
	names = strTok( "two three four", " " );
	for ( k = 2; k <= 4; k++ )
	{
		t = getEnt( "grenade_damage_trigger" + k, "targetname" );
		if ( isDefined( t ) )
		{
			fragGlows[ frags.size ] = "glowing_window_" + names[ k - 2 ];
			frags[ frags.size ] = t;
		}
	}
	damage_triggers_wait( frags, 90, "frag windows", fragGlows );

	// launcherTraining: back to the table, the launcher, back to the pit.
	// The recording's launcher is the M4 with an underbarrel grenade
	// launcher and a red-dot sight (m4techm203_reflex_so; its launcher half
	// is m203_m4tech_reflex_so), which the player switches to.
	move_cast( nwc, ( 2148.5, -2481.5, 48 ), 185 );
	say( nwc, "pit_ncastle_005", 2.0 );
	launcher = first_mounted( "m4techm203_reflex_so", "m79_so" );
	give_all( launcher, true );
	say( nwc, "pit_ncastle_006", 1.4 );
	trigger_wait( pit, 30, "safety pit (launcher)" );
	move_cast( nwc, ( 2075, -2364, 72 ), 270 );
	gl = weaponAltWeaponName( launcher );
	if ( isDefined( gl ) && gl != "" && gl != "none" )
	{
		maps\_cooptraininghud::hint( &"KILLHOUSE_HINT_FIREMODE", 6 );
		task( &"KILLHOUSE_HINT_EQUIP_LAUNCHER", "Switch to the grenade launcher" );
		weapon_wait( gl, 30, "equip the grenade launcher" );
	}
	say( nwc, "pit_ncastle_007", 1.9 );
	task( &"KILLHOUSE_FIRE_AT_THE_WALL_WITH", "Fire at wall one" );
	maps\_cooptraininghud::marker_clear();
	maps\_cooptraininghud::hint( &"KILLHOUSE_HINT_ATTACK_PC", 5 );
	wall = by_noteworthy( "launcher_wall_trigger" );
	// Wall one stands for the whole station (raise_wall_one); only its
	// marker goes with the hit.
	damage_triggers_wait( wall, 45, "launcher wall" );
	say( nwc, "pit_ncastle_008", 1.4 );
	say( nwc, "pit_ncastle_009", 3.3 );
	say( nwc, "pit_ncastle_010", 3.3 );
	task( &"KILLHOUSE_FIRE_YOUR_GRENADE_LAUNCHER", "Fire the grenade launcher into windows five, six and seven" );
	level thread too_low_hint( undefined, &"KILLHOUSE_HINT_GL_TOO_LOW" );
	windows = [];
	winGlows = [];
	names = strTok( "five six seven", " " );
	for ( k = 5; k <= 7; k++ )
	{
		t = getEnt( "launcher_damage_trigger" + k, "targetname" );
		if ( isDefined( t ) )
		{
			winGlows[ windows.size ] = "glowing_window_" + names[ k - 5 ];
			windows[ windows.size ] = t;
		}
	}
	damage_triggers_wait( windows, 90, "launcher windows", winGlows );

	// C4_complete_dialog.  The recording's rookie leaves with the rifle
	// again (no launcher): the range rifle takes the launcher's slot.
	say( nwc, "pit_ncastle_011", 1.3 );
	say( nwc, "pit_ncastle_012", 1.1 );
	say( nwc, "pit_ncastle_013", 2.0 );
	if ( isDefined( level.coop_training_rifle ) && launcher != level.coop_training_rifle )
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( isAlive( players[ i ] ) && players[ i ] hasWeapon( launcher ) )
				players[ i ] takeWeapon( launcher );
		}
		give_all( level.coop_training_rifle, true );
	}
	arrows( "arrows_to_obstacle" );
	task( &"KILLHOUSE_RUN_THE_OBSTACLE_COURSE", "Complete the obstacle course" );
	say( nwc, "pit_ncastle_014_1", 2.1 );
	say( nwc, "pit_ncastle_014_2", 2.6 );
	maps\_coopmission::flag_set( "c4_equiped" );
	open_doors( "gate_cqb_exit", -95 );
}

// --------------------------------------------------------------------------
// Station 4: the obstacle course with Mac (CoD4 obstacle_Training and
// obstacleTraining_dialog).
// --------------------------------------------------------------------------

station_obstacle()
{
	mac = cast( "mac" );
	o = getEnt( "obstacleTraining_objective", "targetname" );
	if ( isDefined( o ) )
		objective_at( o.origin );
	else
		objective_at( ( 330, -2537, 40 ) );
	arrows( "arrows_to_obstacle" );
	open_doors( "gate_cqb_exit2" );
	trigger_wait( getEnt( "obstacle_course_start", "targetname" ), 180, "obstacle course start" );
	arrows( undefined );
	maps\_coopmission::flag_set( "start_obstacle" );
	codolprintln( "CoDOL coop training: station obstacle course" );

	// misssoap, lineup (until the start_course trigger, the line), GO.
	say( mac, "pit_mac_001", 3.2 );
	if ( !maps\_coopmission::flag( "start_course" ) )
	{
		say( mac, "pit_mac_002", 1.9 );
		flag_wait_timeout( "start_course", 30 );
	}
	// The recording counts the line down before Mac's GO (4:47-4:49).
	maps\_cooptraininghud::countdown();
	say( mac, "pit_mac_003", 0.5 );
	// GO: the clip across the start line (clip_obstacle_start, 308 -2556).
	remove_named( "clip_obstacle_start" );
	maps\_coopmission::flag_set( "start_course" );
	objective_at( ( -1106, -2824, 72 ) );
	// The course's chevrons (the big one on the first hurdle, the ones on
	// the ground after it) light for the run.
	arrows( "arrows_to_end" );

	level thread obstacle_nags( mac );
	flag_wait_timeout( "obstacle_course_end", 180 );
	level notify( "coop_obstacle_done" );
	arrows( undefined );
	say( mac, "pit_mac_008_1", 3.6 );
	maps\_coopmission::flag_set( "obstacle_complete" );
	open_doors( "gate_cqb_enter_main", -170 );
}

// --------------------------------------------------------------------------
// The end, as the recording plays it: Price waits by the fence at the end of
// the obstacle course (his authored spot, price2, beside obj_price_nearby);
// the arrows light the way, and walking up to him (come_to_price) plays the
// debrief -- the simple intelligence job, the source called Ortiz -- with
// the rookie's weapons down, and the mission is complete.  The recording
// starts the debrief at pit_pri_062 (061, about the "fancy obstacle course"
// of the pit, is not heard).  Price stands with his arms crossed in the
// recording: CoD4's killhouse_price_stand_idle, and he talks the debrief in
// killhouse_price_stand_talk (_coopscene_killhouse "price_stand"), now that
// the mission animlist reaches C1; the casual stand is the fallback.
// --------------------------------------------------------------------------

station_price()
{
	price = cast( "price2" );
	pos = ( -1390, -2514, 0 );
	if ( isDefined( price ) )
		pos = price.origin;
	objective_at( pos );
	if ( isDefined( price ) )
		maps\_cooptraininghud::marker( undefined, price, 80 );
	arrows( "arrows_to_price" );
	task( &"KILLHOUSE_DEBRIEF_WITH_CPT_PRICE", "Report to Captain Price" );
	flag_wait_timeout( "come_to_price", 120 );
	if ( !maps\_coopmission::flag( "come_to_price" ) )
		wait_player_near( pos, 200, 60 );
	arrows( undefined );
	maps\_cooptraininghud::marker_clear();
	codolprintln( "CoDOL coop training: debrief by the course" );

	// He turns to the rookie who reached him.
	p = player_near( pos, 600 );
	if ( isDefined( price ) && isDefined( p ) )
	{
		d = p.origin - price.origin;
		face = vectorToAngles( ( d[ 0 ], d[ 1 ], 0 ) );
		move_cast( price, price.origin, face[ 1 ] );
	}
	weapons_down_all( true );
	say( price, "pit_pri_062", 6.8 );
	say( price, "pit_pri_063", 7.0 );
	say( price, "pit_pri_064", 3.2 );
	say( price, "pit_pri_065_1", 5.0 );
	say( price, "pit_pri_065_2", 4.4 );
	objective_delete( 0 );
	// The recording blurs and darkens the view before the lobby (5:44-5:50).
	maps\_cooptraininghud::end_fade();
	wait( 2 );
}

// CoD4 obstacleTraining_dialog: charity walk at once, jump on mantle2, crawl
// on crawl_dialog, the "commandos" nag while the course runs.
obstacle_nags( mac )
{
	level endon( "coop_obstacle_done" );
	level endon( "coop_mission_over" );
	say( mac, "pit_mac_004", 3.8 );
	level thread obstacle_line_on_flag( mac, "obstacleTraining_mantle2", "pit_mac_005", 1.7 );
	level thread obstacle_line_on_flag( mac, "crawl_dialog", "pit_mac_006", 2.1 );
	maps\_coopmission::flag_wait( "crawl_dialog" );
	wait( 4 );
	say( mac, "pit_mac_007", 3.2 );
	// Out of the crawl, the sprint to the end (recording 5:10-5:15).
	maps\_coopmission::flag_wait( "stop_shoot_at_crawling" );
	level thread maps\_cooptraininghud::hint_sequence( hint_list( &"KILLHOUSE_HINT_SPRINT_PC", &"KILLHOUSE_HINT_HOLDING_SPRINT" ), 4 );
}

obstacle_line_on_flag( mac, flag, alias, time )
{
	level endon( "coop_obstacle_done" );
	maps\_coopmission::flag_wait( flag );
	say( mac, alias, time );
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
