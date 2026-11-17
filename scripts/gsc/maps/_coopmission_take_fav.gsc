// coop_take_mp_fav_tropical: "the meet" -- the scripted chase mission.
//
// Chapter 1, mission 1 of CoDOL's campaign (after the training): in the
// favela of mp_fav_tropical the rookie and Ghost meet Ortiz, the informant
// who is to bring them Vazquez's people.  The meet is a trap: gunmen kill the
// two informants, Ortiz runs, and the rookie chases him through the favela's
// streets, alleys, rooftops and houses while Ortiz's helpers and a technical
// cut in, until Ghost tackles Ortiz at the far end and Price has him taken to
// the landing zone.  docs/CODOL_CAMPAIGN_MODE.md, section "Mission
// coop_take_mp_fav_tropical", is the timeline of the retail recording this
// script follows (paraphrased; no game text here).
//
// THE ENTITIES (the playlist zone's addon MapEnts; no level script ships):
//   * player_vehicle2 / player_vehicle: the two luxury-sedan interiors the
//     squad sits in at the start (script_vehicle_luxurysedan*_viewmodel);
//   * van (script_vehicle_uaz_van), chase_target_spawner (Ortiz,
//     actor_enemy_villian_ortiz_so), opening_driver/gunner1/gunner2_spawner
//     (the gunmen), opening_scene_node: the meet, group opening_scene;
//   * escape_path_start -> ... -> me0_auto323: Ortiz's run, script_struct /
//     script_origin nodes carrying script_flag_set chase_<n>_arrive and
//     script_flag_wait chase_<n>_leave, the leave flags set by trigger
//     volumes the player crosses (IW4 favela's runner, as Faust runs);
//   * obj_10 .. obj_100: the objective points along the way (script_origin);
//   * combat_zone_<n> flag triggers and spawner groups (Ortiz's helpers),
//     released by _coopmission's flag groups; the AI-only triggers
//     start_technical and combat_zone_51/55_ai_trigger, crossed by Ortiz;
//   * script_vehicle_pickup_technical + vnode_technical1_start (the technical
//     and its crew, the script_vehicleride combat_zone_10 spawners);
//   * ghost_intro / ghost_intro_goal / delete_ghost_intro, ghost_ending and
//     ending_tackle: Ghost's run to the other side and the tackle;
//   * civ_* civilians and their flags, van_blocker / move_van, sprint_hint /
//     ads_hint.
// C1's own localized strings (COOP_TAKE_MP_FAV_TROPICAL_*) and dialogue
// aliases (favela_*) ship in the playlist zone; this script names them only.
//
// Selected by sv_sessionMap like _cooptraining; _coopmission runs none of
// its generic phases while level.coop_mission_flow is set.  Coop category
// root only; names no multiplayer module.

#using_animtree ( "generic_human" );

is_mission()
{
	return getDvar( "sv_sessionMap" ) == "coop_take_mp_fav_tropical";
}

// --------------------------------------------------------------------------
// Flags (the mission layer's).
// --------------------------------------------------------------------------

flag( name )
{
	return maps\_coopmission::flag( name );
}

flag_set( name )
{
	maps\_coopmission::flag_set( name );
}

flag_wait( name )
{
	maps\_coopmission::flag_wait( name );
}

flag_wait_or_timeout( name, timeout )
{
	waited = 0;
	while ( !flag( name ) && waited < timeout )
	{
		wait( 0.25 );
		waited += 0.25;
	}
	return flag( name );
}

log( text )
{
	codolprintln( "CoDOL coop fav: " + text );
}

// --------------------------------------------------------------------------
// Entity lookups.
// --------------------------------------------------------------------------

ent( name )
{
	ents = getEntArray( name, "targetname" );
	if ( ents.size <= 0 )
		return undefined;
	return ents[ 0 ];
}

struct( name )
{
	return maps\_coopvehicle::path_point( name );
}

ground( point )
{
	return maps\_coopvehicle::ground_under( point );
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

flag_trigger( name )
{
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( isDefined( e.script_flag ) && e.script_flag == name && isDefined( e.classname ) && getSubStr( e.classname, 0, 8 ) == "trigger_" )
			return e;
	}
	return undefined;
}

living_players()
{
	return maps\_coopmission::living_players();
}

nearest_player( origin )
{
	best = undefined;
	bestDist = 0;
	players = living_players();
	for ( i = 0; i < players.size; i++ )
	{
		d = distance( origin, players[ i ].origin );
		if ( !isDefined( best ) || d < bestDist )
		{
			best = players[ i ];
			bestDist = d;
		}
	}
	return best;
}

nearest_player_distance( origin )
{
	d = maps\_coopmission::nearest_player_distance( origin );
	if ( !isDefined( d ) )
		return 999999;
	return d;
}

flat_angles( angles )
{
	return ( 0, angles[ 1 ], 0 );
}

// --------------------------------------------------------------------------
// Voice lines.  Durations (s) are read off the recording's subtitles.  Price
// and Ghost speak over the radio (every player hears them); Ortiz and the
// informant speak from where they stand.
// --------------------------------------------------------------------------

radio( alias, time )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] playLocalSound( alias );
	log( "line " + alias );
	if ( isDefined( time ) )
		wait( time );
}

say( speaker, alias, time )
{
	if ( isDefined( speaker ) )
		speaker playSound( alias );
	log( "line " + alias );
	if ( isDefined( time ) )
		wait( time );
}

// --------------------------------------------------------------------------
// HUD: the objective text, the title card, the key hints.  C1 localizes the
// zone's own strings; each one is precached in init().
// --------------------------------------------------------------------------

hud_text( x, y, alignX, horzAlign, vertAlign, scale, color )
{
	h = newHudElem();
	h.alignX = alignX;
	h.alignY = "middle";
	h.horzAlign = horzAlign;
	h.vertAlign = vertAlign;
	h.x = x;
	h.y = y;
	h.font = "objective";
	h.fontScale = scale;
	h.color = color;
	h.foreground = true;
	h.hidewheninmenu = true;
	h.alpha = 0;
	return h;
}

// The objective line at the top centre, as the recording shows it: up for a
// few seconds when the objective changes, then faded.
objective_text( text )
{
	level notify( "coop_fav_objective_text" );
	level endon( "coop_fav_objective_text" );
	if ( !isDefined( level.fav.objective_hud ) )
		level.fav.objective_hud = hud_text( 0, 70, "center", "center", "top", 1.5, ( 0.65, 0.85, 1 ) );
	h = level.fav.objective_hud;
	h setText( text );
	h.alpha = 1;
	wait( 5 );
	h fadeOverTime( 1 );
	h.alpha = 0;
}

// The title card, bottom left: the mission's name, the date and time, the
// country (COOP_TAKE_MP_FAV_TROPICAL_INTROSCREEN_LINE_1..3).
intro_card()
{
	lines = [];
	lines[ 0 ] = &"COOP_TAKE_MP_FAV_TROPICAL_INTROSCREEN_LINE_1";
	lines[ 1 ] = &"COOP_TAKE_MP_FAV_TROPICAL_INTROSCREEN_LINE_2";
	lines[ 2 ] = &"COOP_TAKE_MP_FAV_TROPICAL_INTROSCREEN_LINE_3";
	huds = [];
	for ( i = 0; i < lines.size; i++ )
	{
		huds[ i ] = hud_text( 24, 250 + i * 16, "left", "left", "top", 1.25, ( 0.6, 0.9, 1 ) );
		huds[ i ] setText( lines[ i ] );
		huds[ i ] fadeOverTime( 0.5 );
		huds[ i ].alpha = 1;
		wait( 0.4 );
	}
	wait( 5 );
	for ( i = 0; i < huds.size; i++ )
	{
		huds[ i ] fadeOverTime( 1 );
		huds[ i ].alpha = 0;
	}
	wait( 1.2 );
	for ( i = 0; i < huds.size; i++ )
		huds[ i ] destroy();
}

// The key hints the map's sprint_hint / ads_hint triggers ask for, once each.
hints()
{
	level endon( "coop_mission_over" );
	level thread hint_on_flag( "ads_hint", &"COOP_TAKE_MP_FAV_TROPICAL_ADS_HINT" );
	level thread hint_on_flag( "sprint_hint", &"COOP_TAKE_MP_FAV_TROPICAL_SPRINT_HINT" );
}

hint_on_flag( name, text )
{
	level endon( "coop_mission_over" );
	flag_wait( name );
	h = hud_text( 0, 110, "center", "center", "middle", 1.3, ( 1, 1, 1 ) );
	h setText( text );
	h fadeOverTime( 0.3 );
	h.alpha = 1;
	log( "hint " + name );
	wait( 4 );
	h fadeOverTime( 0.5 );
	h.alpha = 0;
	wait( 0.6 );
	h destroy();
}

// --------------------------------------------------------------------------
// Entry points.
// --------------------------------------------------------------------------

// Synchronous from _coopmode::Callback_StartGameType (the precache window).
init()
{
	level.fav = undefined;
	if ( !is_mission() )
		return;

	// _coopmission runs none of its generic phases (the objective chain and
	// the group waves it would otherwise pick for this map).
	level.coop_mission_flow = "take_fav";

	fav = spawnStruct();
	level.fav = fav;
	fav.opening_done = false;
	fav.riders = [];
	fav.cars = [];
	fav.respawn = undefined;
	// The sedan interiors carry tag_passenger, tag_guy0 / _guy1 and
	// tag_driver (MEASURED, codol_checkmodel on both models).
	fav.seat_tags = true;

	// Every model this mission places ships in coop_take_mp_fav_tropical
	// (MEASURED, docs/zone_assets/coop_take_mp_fav_tropical.txt).
	models = strTok( "body_hero_ghost_socom head_hero_ghost_soccom body_airport_assault_a head_militia_ba_blk " +
		"body_slum_civ_male_aa body_slum_civ_male_ab body_slum_civ_male_ba body_slum_civ_male_bb " +
		"body_slum_civ_male_aa_wht body_slum_civ_male_bb_wht head_slum_civ_male_a head_slum_civ_male_b " +
		"head_slum_civ_male_c head_slum_civ_male_d head_slum_civ_male_e head_slum_civ_male_f head_slum_civ_male_g " +
		"head_slum_civ_male_h vehicle_uaz_van vehicle_pickup_technical vehicle_luxurysedan_viewmodel " +
		"vehicle_luxurysedan_2009_viewmodel weapon_m2_50cal_center tag_origin", " " );
	for ( i = 0; i < models.size; i++ )
		precacheModel( models[ i ] );

	precacheShader( "objpoint_default" );
	precacheString( &"COOP_TAKE_MP_FAV_TROPICAL_GRAB" );
	precacheString( &"COOP_TAKE_MP_FAV_TROPICAL_CAPTURE" );
	precacheString( &"COOP_TAKE_MP_FAV_TROPICAL_SPRINT_HINT" );
	precacheString( &"COOP_TAKE_MP_FAV_TROPICAL_ADS_HINT" );
	precacheString( &"COOP_TAKE_MP_FAV_TROPICAL_INTROSCREEN_LINE_1" );
	precacheString( &"COOP_TAKE_MP_FAV_TROPICAL_INTROSCREEN_LINE_2" );
	precacheString( &"COOP_TAKE_MP_FAV_TROPICAL_INTROSCREEN_LINE_3" );

	// The rookie's weapons in the recording: an M4 with a red-dot sight and
	// an AA-12 with the heartbeat sensor, nothing else (no sidearm, no
	// grenades on the HUD).  Both ship in this mission's zone.
	fav.rifle = first_mounted( "m4_grunt_so", "m16_basic_so" );
	fav.shotgun = first_mounted( "aa12_hb_so", "aa12_so" );
	if ( isDefined( fav.rifle ) )
		precacheItem( fav.rifle );
	if ( isDefined( fav.shotgun ) )
		precacheItem( fav.shotgun );

	setup_cars();

	// The look-only overhead names (_cooptraining's), for Ghost.
	level.coop_cast_list = [];
	level.coop_cast = [];
	level thread maps\_cooptraining::show_nametags();
	level thread loadout_watch();

	// The probe harness's mission verbs (_cooptest forwards what it does
	// not know itself).
	level.coop_mission_testcmd = ::test_command;

	log( "mission flow active (rifle " + fav.rifle + ", shotgun " + fav.shotgun + ", " + fav.cars.size + " cars)" );
}

first_mounted( a, b )
{
	if ( codol_isWeaponMounted( a ) )
		return a;
	if ( codol_isWeaponMounted( b ) )
		return b;
	return undefined;
}

main()
{
	if ( !is_mission() )
		return;
	level endon( "coop_mission_over" );

	while ( getPlayers().size <= 0 )
		wait( 0.5 );
	wait( 0.5 );
	if ( !maps\_coopmission::make_template() )
		return;

	level thread civilians();
	level thread van_blocker();
	// The sprint / ADS key hints are not shown: the retail recording draws
	// neither on its triggers, and on C1 the zone strings' key token prints
	// as "[unbound]" (the 2026-10-08 playthrough, the ADS hint at the
	// technical).  hints() stays for a client that binds the keys.
	level thread ai_triggers();
	level thread soldier_scenes();

	opening();

	level thread objectives();
	level thread technical();
	level thread chase_dialogue();
	// Ghost stays a voice until the end (the recording never shows him at
	// the meet; ghost_intro sits just behind the rookie's car, where the
	// 2026-10-08 playthrough met him and the gunmen shot at him), so
	// ghost_intro_run() is not started.

	chase();
	ending();

	maps\_coopmission::mission_complete( "Ortiz captured" );
}

// --------------------------------------------------------------------------
// The player's weapons and seat.
// --------------------------------------------------------------------------

loadout_watch()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread loadout_player();
	}
}

loadout_player()
{
	self endon( "disconnect" );
	// spawnPlayer can notify spawned_player in the frame "connected" is
	// sent, before this thread waits (as _cooptraining measured).
	wait( 0.05 );
	if ( isAlive( self ) && self.sessionstate == "playing" )
		self player_spawned();
	for ( ;; )
	{
		self waittill( "spawned_player" );
		self player_spawned();
	}
}

player_spawned()
{
	fav = level.fav;
	weapons = self getWeaponsList();
	for ( i = 0; i < weapons.size; i++ )
	{
		if ( weapons[ i ] != "none" )
			self takeWeapon( weapons[ i ] );
	}
	self setOffhandEquipment( "none", "none", "none" );
	if ( isDefined( fav.shotgun ) )
	{
		self giveWeapon( fav.shotgun );
		self giveMaxAmmo( fav.shotgun );
	}
	if ( isDefined( fav.rifle ) )
	{
		self giveWeapon( fav.rifle );
		self giveMaxAmmo( fav.rifle );
		self setSpawnWeapon( fav.rifle );
		self switchToWeapon( fav.rifle );
	}
	primaries = self getWeaponsListPrimaries();
	text = "";
	for ( i = 0; i < primaries.size; i++ )
		text += " " + primaries[ i ];
	log( self.name + " carries" + text );
	for ( i = 0; i < primaries.size; i++ )
		log( primaries[ i ] + " clip " + weaponClipSize( primaries[ i ] ) + " start " + weaponStartAmmo( primaries[ i ] ) + " max " + weaponMaxAmmo( primaries[ i ] ) +
			", given " + self getWeaponAmmoClip( primaries[ i ] ) + " | " + self getWeaponAmmoStock( primaries[ i ] ) );

	if ( !fav.opening_done )
	{
		self seat_in_car();
		return;
	}
	// A respawn (or a teammate who joins late) rejoins the chase where the
	// squad is, not at the cars.
	if ( isDefined( fav.respawn ) )
	{
		spot = fav.respawn;
		buddy = undefined;
		players = living_players();
		for ( i = 0; i < players.size; i++ )
		{
			if ( players[ i ] != self )
				buddy = players[ i ];
		}
		if ( isDefined( buddy ) )
			spot = buddy.origin - anglesToForward( flat_angles( buddy getPlayerAngles() ) ) * 48;
		self setOrigin( ground( spot + ( 0, 0, 16 ) ) );
		log( self.name + " respawns at " + self.origin );
	}
	self.coop_fav_spawned = true;
}

// --------------------------------------------------------------------------
// The cars.  The recording's rookie sits in the front passenger seat of a
// left-hand-drive sedan facing the meet, the driver's seat empty, weapons
// down.  That car is player_vehicle: the recording's interior is the dark
// 2009 sedan (grey dashboard, a radio on it; vehicle_luxurysedan_2009_
// viewmodel, the one with the blood layer IW4 favela hides on the player's
// car), and its authored place (372 2402, facing west down the street) puts
// the van's authored spawn (605 2294, facing west) behind it on the left, so
// the van "passes on the left and stops ahead" as the recording shows.  A
// second player takes player_vehicle2 (the cream sedan across the corner).
// The authored vehicles are interiors (_viewmodel models); each is shown as
// a script_model on the ground under its origin.
// --------------------------------------------------------------------------

setup_cars()
{
	names = [];
	names[ 0 ] = "player_vehicle";
	names[ 1 ] = "player_vehicle2";
	for ( i = 0; i < names.size; i++ )
	{
		v = ent( names[ i ] );
		if ( !isDefined( v ) || !isDefined( v.model ) || v.model == "" )
			continue;
		car = spawn( "script_model", ground( v.origin ) );
		car.angles = flat_angles( v.angles );
		car setModel( v.model );
		// The cracked windscreen stays hidden until the gunmen shoot it.
		car hidePart( "tag_glass_front_d" );
		v hide();
		car.coop_vehicle = v;
		level.fav.cars[ level.fav.cars.size ] = car;
		log( "car " + names[ i ] + " (" + v.model + ") at " + car.origin + " yaw " + car.angles[ 1 ] );
	}
}

// Seats in the car's own frame (forward, right, up) from its origin on the
// ground: the front passenger seat first, then the rear.  The sedan
// interiors' seat tags are read in fav_seat_offset().
seat_in_car()
{
	fav = level.fav;
	if ( fav.cars.size <= 0 )
		return;
	seat = fav.riders.size;
	fav.riders[ seat ] = self;
	car = fav.cars[ seat % fav.cars.size ];
	row = int( seat / fav.cars.size );
	offset = fav_seat_offset( car, row );
	self disableWeapons();
	self setStance( "crouch" );
	self maps\_coopintro::seat_lock( true );
	origin = car.origin + anglesToForward( car.angles ) * offset[ 0 ] + anglesToRight( car.angles ) * offset[ 1 ] + ( 0, 0, offset[ 2 ] );
	self setOrigin( origin );
	self setPlayerAngles( car.angles );
	self.coop_seat_angles = car.angles;
	self.coop_fav_car = car;
	self playerLinkToDelta( car, "tag_origin", 1 );
	self.coop_in_intro = true;
	log( self.name + " seated in " + car.coop_vehicle.targetname + " row " + row + " at " + origin );
}

// Front passenger, then rear right.  MEASURED with codol_checkmodel and
// favtags on both sedan interiors (model frame, forward / right / up):
// tag_driver ( 11, -18, 22 ) and tag_guy0 / tag_guy1 ( -23, -21 / 20, 23 )
// are the seat cushions, but tag_passenger ( -1, 18, -8 ) is under the
// floor (a rider-animation reference, not a seat), so the front passenger
// seat is tag_driver mirrored.  The feet go 12 under the cushion as on the
// training jeep (_coopintro board).
fav_seat_offset( car, row )
{
	if ( isDefined( level.fav.seat_tags ) && level.fav.seat_tags )
	{
		tag = "tag_driver";
		side = -1;
		if ( row > 0 )
		{
			tag = "tag_guy1";
			side = 1;
		}
		o = car getTagOrigin( tag ) - car.origin;
		return ( vectorDot( o, anglesToForward( car.angles ) ), side * vectorDot( o, anglesToRight( car.angles ) ), o[ 2 ] - 12 );
	}
	if ( row > 0 )
		return ( -36, 18, 10 );
	return ( 8, 18, 10 );
}

// The riders get out on their own side (the recording: the rookie opens the
// passenger door and steps out to the right).
leave_cars()
{
	fav = level.fav;
	for ( i = 0; i < fav.riders.size; i++ )
	{
		p = fav.riders[ i ];
		if ( !isDefined( p ) || !isAlive( p ) || !isDefined( p.coop_fav_car ) )
			continue;
		car = p.coop_fav_car;
		car playSound( "scn_mpfav_cardoor_open" );
		eye = p getEye();
		p unlink();
		p maps\_coopintro::seat_lock( false );
		spot = exit_spot( car, eye, int( i / fav.cars.size ) );
		p setOrigin( spot );
		p setPlayerAngles( car.angles );
		p setStance( "stand" );
		p enableWeapons();
		p.coop_in_intro = undefined;
		p.coop_fav_car = undefined;
		p.coop_fav_spawned = true;
		if ( !isDefined( fav.respawn ) )
			fav.respawn = spot;
		log( p.name + " out of the car at " + spot );
	}
	fav.opening_done = true;
}

// Where a rider steps out: beside his door (the passenger side, the right),
// a little forward or back if the door spot is blocked, then the other side.
// A spot counts when a standing player hull fits there (playerPhysicsTrace
// drops onto the ground under it) and the seat can see it (not through a
// wall).  The 2026-10-08 playthrough set the rookie down in a wall corner
// beside the cream sedan.
exit_spot( car, eye, row )
{
	fwd = anglesToForward( car.angles );
	right = anglesToRight( car.angles );
	offsets = [];
	offsets[ offsets.size ] = ( -24 * row, 64, 0 );
	offsets[ offsets.size ] = ( 36, 64, 0 );
	offsets[ offsets.size ] = ( -48, 64, 0 );
	offsets[ offsets.size ] = ( 90, 72, 0 );
	offsets[ offsets.size ] = ( 0, 84, 0 );
	offsets[ offsets.size ] = ( 140, 40, 0 );
	offsets[ offsets.size ] = ( -140, 30, 0 );
	offsets[ offsets.size ] = ( 0, -72, 0 );
	first = undefined;
	for ( k = 0; k < offsets.size; k++ )
	{
		o = offsets[ k ];
		c = car.origin + fwd * o[ 0 ] + right * o[ 1 ];
		tr = bulletTrace( c + ( 0, 0, 64 ), c - ( 0, 0, 200 ), false, car );
		if ( tr[ "fraction" ] >= 1 )
			continue;
		g = tr[ "position" ];
		if ( !isDefined( first ) )
			first = g + ( 0, 0, 1 );
		landed = playerPhysicsTrace( g + ( 0, 0, 40 ), g + ( 0, 0, 1 ) );
		if ( distance( landed, g + ( 0, 0, 1 ) ) > 4 )
			continue;
		if ( !bulletTracePassed( eye, g + ( 0, 0, 40 ), false, car ) )
			continue;
		log( "exit spot " + k + " " + g );
		return g + ( 0, 0, 1 );
	}
	if ( isDefined( first ) )
		return first;
	return car.origin + right * 64;
}

// A seated rider who turns his view is looking at the world (a real client
// fades in for seconds after it spawns); _coopintro's rule.
wait_riders_looking( limit )
{
	waited = 0;
	while ( level.fav.riders.size <= 0 && waited < 30 )
	{
		wait( 0.25 );
		waited += 0.25;
	}
	waited = 0;
	while ( waited < limit )
	{
		riders = level.fav.riders;
		for ( i = 0; i < riders.size; i++ )
		{
			p = riders[ i ];
			if ( !isDefined( p ) || !isDefined( p.coop_seat_angles ) )
				continue;
			a = p getPlayerAngles();
			if ( maps\_coopintro::abs_angle( a[ 1 ] - p.coop_seat_angles[ 1 ] ) > 2 ||
			     maps\_coopintro::abs_angle( a[ 0 ] - p.coop_seat_angles[ 0 ] ) > 2 )
				return;
		}
		wait( 0.25 );
		waited += 0.25;
	}
}

// --------------------------------------------------------------------------
// Actors: Ortiz, Ghost, the informants, the civilians.  Soldier-species
// actors of the addon's own classnames (actor_enemy_villian_ortiz_so,
// actor_ally_hero_ghost_socom_so, actor_civilian_slum_male_so), whose aitypes
// and character scripts give them the bodies and heads the zone ships;
// char_index picks a civilian's character the way an authored
// script_char_index does.  The gunmen and helpers are _coopmission's enemy
// soldiers.
// --------------------------------------------------------------------------

spawn_actor( origin, angles, key, cls, char_index, team, name )
{
	kind = spawnStruct();
	kind.role = "pistol";
	kind.health = 1000;
	kind.accuracy = 0;
	kind.char_index = char_index;
	ai = maps\_coopmission::spawn_class_actor( cls, team, kind );
	if ( !isDefined( ai ) )
	{
		log( "could not spawn " + key );
		return undefined;
	}
	ai forceTeleport( origin, angles );
	ai.ignoreall = true;
	ai.ignoreme = true;
	ai.coop_cast = key;
	ai.coop_idle_anim = %casual_stand_idle;
	ai setAnimKnobAll( ai.coop_idle_anim, %body, 1, 0.1, 1 );
	ai notify( "coop_cast_pose" );
	if ( isDefined( name ) )
	{
		ai.coop_name = name;
		level.coop_cast_list[ level.coop_cast_list.size ] = ai;
		level.coop_cast[ key ] = ai;
	}
	return ai;
}

// Walk or run an actor to a point: the native planner over the map's bot
// waypoint graph, then "goal".  An actor that makes no headway for a few
// seconds (a ledge, a jump the planner cannot take: Ortiz's rail vault and
// big rooftop jump are map animations C1 does not load) is put on the point.
// With `ahead` (Ortiz), a long leg is cut short while no player can see him,
// so he keeps the lead the recording's runner has over the rookie.
move_actor( dest, run, timeout, ahead )
{
	self endon( "death" );
	dest = ground( dest + ( 0, 0, 8 ) );
	if ( distance( self.origin, dest ) < 40 )
		return true;
	self.coop_hunting = undefined;
	if ( run )
		self.coop_hunting = true;
	self.goalradius = 40;
	self setGoalPos( dest );
	if ( !isDefined( timeout ) )
		timeout = distance( self.origin, dest ) / 120 + 6;
	waited = 0;
	last = self.origin;
	still = 0;
	while ( waited < timeout )
	{
		wait( 0.25 );
		waited += 0.25;
		d = self.origin - dest;
		if ( length( ( d[ 0 ], d[ 1 ], 0 ) ) < 48 && d[ 2 ] < 80 && d[ 2 ] > -80 )
			return true;
		if ( distance( self.origin, last ) < 6 )
			still += 0.25;
		else
			still = 0;
		last = self.origin;
		if ( still >= 3 )
			break;
		if ( isDefined( ahead ) && ahead && waited >= 2 && length( ( d[ 0 ], d[ 1 ], 0 ) ) > 400 && !seen_by_player( self ) &&
		     nearest_player_distance( dest ) < nearest_player_distance( self.origin ) + 300 )
		{
			log( self.coop_cast + " out of sight at " + self.origin + "; on to " + dest );
			self forceTeleport( dest, self.angles );
			return true;
		}
	}
	log( self.coop_cast + " made no headway from " + self.origin + " to " + dest + "; placed there" );
	self forceTeleport( dest, self.angles );
	return false;
}

seen_by_player( ai )
{
	players = living_players();
	for ( i = 0; i < players.size; i++ )
	{
		if ( distance( players[ i ].origin, ai.origin ) > 2500 )
			continue;
		if ( bulletTracePassed( players[ i ] getEye(), ai.origin + ( 0, 0, 48 ), false, ai ) )
			return true;
	}
	return false;
}

// --------------------------------------------------------------------------
// The meet (the recording, 1:29 .. 1:58).
// --------------------------------------------------------------------------

// The van brings Ortiz and the two informants: it comes from behind the
// rookie's car on the left, passes it and stops ahead in the street, its
// rear doors toward him (the recording, 1:30-1:35).  It starts on the
// authored van (605 2294, facing west: behind player_vehicle's left), as
// IW4 favela's van does; its authored path (van_start) is not in the addon,
// so it drives two straight legs: down the left of the street past the car,
// then in to the stop.  MEASURED from the recording's frame at 1:36: the
// van's rear stands about 300 units ahead and a little left of the eye.
van_pullup()
{
	fav = level.fav;
	v = ent( "van" );
	car = fav.cars[ 0 ];
	if ( !isDefined( car ) )
		return undefined;
	fwd = anglesToForward( car.angles );
	left = anglesToRight( car.angles ) * -1;
	stop = ground( car.origin + fwd * 390 + left * 50 + ( 0, 0, 40 ) );
	start = ground( car.origin - fwd * 230 + left * 108 + ( 0, 0, 40 ) );
	angles = car.angles;
	model = "vehicle_uaz_van";
	if ( isDefined( v ) )
	{
		if ( isDefined( v.model ) && v.model != "" )
			model = v.model;
		start = ground( v.origin + ( 0, 0, 16 ) );
		angles = flat_angles( v.angles );
	}
	// The first leg holds the van's lane on the car's left; the second
	// eases in to the stop.
	mid = start + vectorDot( ( car.origin + fwd * 170 ) - start, fwd ) * fwd;
	mid = ground( mid + ( 0, 0, 40 ) );
	van = spawn( "script_model", start );
	van.angles = angles;
	van setModel( model );
	if ( isDefined( v ) )
		v hide();
	fav.van = van;
	van playSound( "scn_favela_van_pullup" );
	van playLoopSound( "veh_uaz_engine_low" );
	wheels = van maps\_coopscene_fav::play_vehicle_clip( "uaz_driving_idle_forward" );
	van moveTo( mid, 2.4, 0.4, 0 );
	log( "van pulls up from " + start + " via " + mid + " to " + stop );
	wait( 2.4 );
	van moveTo( stop, 1.8, 0, 1.2 );
	van rotateTo( flat_angles( vectorToAngles( stop - mid ) ) * 0.5 + angles * 0.5, 0.9 );
	wait( 0.9 );
	van rotateTo( angles, 0.9 );
	wait( 0.9 );
	van maps\_coopscene_fav::stop_vehicle_clip( wheels );
	van maps\_coopscene_fav::play_vehicle_clip( "favela_van_shootout_van", 0.5 );
	van stopLoopSound();
	van playSound( "scn_favela_van_skid2stop" );
	wait( 0.6 );
	van playSound( "scn_favela_van_door_open" );
	return van;
}

// Ortiz (the authored chase target) and the two informants step out of the
// van and walk to the meeting point; Kortez's gunmen (the three authored
// opening spawners) wait there by the blue car.
spawn_meet( van )
{
	fav = level.fav;
	meet = meet_point();
	back = van.origin - anglesToForward( van.angles ) * 90 + anglesToRight( van.angles ) * 48;

	spawner = ent( "chase_target_spawner" );
	yaw = vectorToAngles( meet - back );
	fav.ortiz = spawn_actor( ground( back + ( 0, 0, 16 ) ), flat_angles( yaw ), "ortiz", "actor_enemy_villian_ortiz_so", undefined, "axis" );
	if ( isDefined( fav.ortiz ) )
	{
		fav.ortiz.coop_cast = "ortiz";
		fav.ortiz.health = 100000;
		fav.ortiz.allowpain = false;
		fav.ortiz setCanDamage( true );
		fav.ortiz thread ortiz_damage_watch();
		if ( isDefined( spawner ) )
			fav.ortiz.coop_spawner = spawner;
	}
	fav.informants = [];
	for ( i = 0; i < 2; i++ )
	{
		spot = ground( back - anglesToRight( van.angles ) * ( 30 + i * 30 ) - anglesToForward( van.angles ) * ( 20 + i * 24 ) + ( 0, 0, 16 ) );
		inf = spawn_actor( spot, flat_angles( yaw ), "informant" + i, "actor_civilian_slum_male_so", i + 1, "allies" );
		if ( isDefined( inf ) )
		{
			inf.health = 60;
			fav.informants[ fav.informants.size ] = inf;
		}
	}

	// They walk to the meet side by side.
	if ( isDefined( fav.ortiz ) )
		fav.ortiz thread move_actor( meet + ( -30, 30, 0 ), false, 10 );
	for ( i = 0; i < fav.informants.size; i++ )
		fav.informants[ i ] thread move_actor( meet + ( 20 + i * 30, -20 - i * 25, 0 ), false, 10 );
	log( "the meet: Ortiz and " + fav.informants.size + " informants walk to " + meet );
}

// Where Ortiz brings the informants: in the street between the rookie's car
// and the van, a little left of the car's line, so the meet plays in front
// of the windscreen as the recording frames it (the authored
// opening_scene_node, 157 2239, sits 40 degrees off the car's nose: IW4
// favela's scene animations carried the actors well away from it).
meet_point()
{
	car = level.fav.cars[ 0 ];
	if ( !isDefined( car ) )
	{
		node = struct( "opening_scene_node" );
		if ( isDefined( node ) )
			return ground( node.origin + ( 0, 0, 16 ) );
		return ground( ( 157, 2239, 299 ) );
	}
	fwd = anglesToForward( car.angles );
	left = anglesToRight( car.angles ) * -1;
	return ground( car.origin + fwd * 210 + left * 70 + ( 0, 0, 40 ) );
}

// The gunmen: the three authored opening spawners (deserteagle), enemy
// soldiers holding fire until the informants are shot.  They wait at the
// meet's left side, clear of the van's lane, facing it (the recording: three men already standing
// there when the van pulls up).
spawn_gunmen()
{
	fav = level.fav;
	fav.gunmen = [];
	names = strTok( "opening_driver_spawner opening_gunner1_spawner opening_gunner2_spawner", " " );
	for ( i = 0; i < names.size; i++ )
	{
		s = ent( names[ i ] );
		if ( !isDefined( s ) )
			continue;
		ai = maps\_coopmission::spawn_soldier( s );
		if ( !isDefined( ai ) )
			continue;
		ai.ignoreall = true;
		car = fav.cars[ 0 ];
		if ( isDefined( car ) )
		{
			meet = meet_point();
			fwd = anglesToForward( car.angles );
			left = anglesToRight( car.angles ) * -1;
			spot = ground( meet + left * 115 + fwd * ( -30 + 45 * fav.gunmen.size ) + ( 0, 0, 40 ) );
			ai forceTeleport( spot, flat_angles( vectorToAngles( meet - spot ) ) );
			log( "gunman " + fav.gunmen.size + " waits at " + spot );
		}
		fav.gunmen[ fav.gunmen.size ] = ai;
	}
	log( fav.gunmen.size + " gunmen wait at the meet" );
}

opening()
{
	fav = level.fav;
	level.coop_fav_step = "opening";

	// As the training jeep waits for its riders to look around.
	wait_riders_looking( 8 );
	log( "the meet begins with " + fav.riders.size + " rider(s)" );
	level notify( "coop_fav_meet" );

	// The meet as C1 ships it: the four scene clips at opening_scene_node.
	if ( maps\_coopscene_fav::meet_scene_available() && isDefined( ent( "opening_scene_node" ) ) )
	{
		opening_scene();
		return;
	}

	spawn_gunmen();
	level thread intro_card();
	level thread objective_text( &"COOP_TAKE_MP_FAV_TROPICAL_GRAB" );
	level thread radio( "favela_pri_simplejob01", 5.5 );
	wait( 1.5 );
	van = van_pullup();
	if ( isDefined( van ) )
		spawn_meet( van );
	else
		spawn_meet( fav.cars[ 0 ] );

	// 1:38 Ghost picks out Ortiz and the informants; 1:40 Ortiz calms them.
	wait( 2 );
	radio( "favela_gst_02", 3 );
	say( fav.ortiz, "favela_ortiz_01", 3 );

	// 1:44 the trap: Ghost sees it, an informant regrets coming, the gunmen
	// shoot both informants.
	level thread radio( "favela_gst_03", 3 );
	if ( fav.informants.size > 0 )
		say( fav.informants[ 0 ], "favela_inf2_01", 0.8 );
	shoot_informants();
	wait( 1.2 );

	// 1:46 Ortiz runs; the gunmen turn on the car (the windshield shatters).
	level thread ortiz_flees();
	for ( i = 0; i < fav.cars.size; i++ )
	{
		fav.cars[ i ] playSound( "scn_favela_car_glass_shatter" );
		fav.cars[ i ] hidePart( "tag_glass_front" );
		fav.cars[ i ] showPart( "tag_glass_front_d" );
	}
	earthquake( 0.25, 0.6, fav.cars[ 0 ].origin, 400 );
	wait( 0.6 );

	// 1:47 Price: chase Ortiz, Ghost cuts him off.
	level thread objective_text( &"COOP_TAKE_MP_FAV_TROPICAL_CAPTURE" );
	ortiz_marker();
	radio( "favela_pri_intercept01", 2.5 );

	// 1:49 out of the car; the gunmen open fire.
	leave_cars();
	for ( i = 0; i < fav.gunmen.size; i++ )
	{
		if ( isDefined( fav.gunmen[ i ] ) && isAlive( fav.gunmen[ i ] ) )
			fav.gunmen[ i ].ignoreall = false;
	}
	level.coop_fav_step = "chase";
	level notify( "coop_fav_chase" );

	// 1:51 .. 1:59 Ghost and Price over the radio.
	level thread opening_banter();
}

// --------------------------------------------------------------------------
// The meet from C1's own scene clips (IW4 favela opening_scene: makarov /
// gunner1 / gunner2 / driver at opening_scene_node; here Ortiz and the three
// authored opening spawners).  The clips' timing, MEASURED from their delta
// tracks and notetracks (maps\_coopscene_fav.gsc):
//   driver (guy4)  sits in the van 0 .. 4.5 s, rides it in 4.5 .. 7.3 s (the
//                  van stops at opening_scene_node_vehicles), climbs out
//                  9 .. 11 s, walks to the meet and fires at 13.7 s
//                  (scn_favela_shootoutguy4), 20.1 s long
//   gunner1 / 2    (guy2 / guy3, C1's two informants) walk a few steps and
//                  fall 7.3 s into their 10.9 s clips (scn_favela_intro_
//                  bodyfall): started 6.4 s in, they fall to the driver's shot
//   Ortiz (guy1)   stands by the van's side, walks up, fires at the car
//                  14.9 .. 21.6 s (the windscreen), runs west at 20.2 s and
//                  ends 24.5 s in beside the first chase node
// which puts the lines where the recording has them (scene start ~1:31:
// Ghost 1:38, Ortiz 1:40, the shots 1:44, the windscreen 1:46, out 1:49).
// --------------------------------------------------------------------------

opening_scene()
{
	fav = level.fav;
	node = ent( "opening_scene_node" );
	start = getTime();

	level thread intro_card();
	level thread objective_text( &"COOP_TAKE_MP_FAV_TROPICAL_GRAB" );
	level thread radio( "favela_pri_simplejob01", 5.5 );

	ortiz_clip = maps\_coopscene_fav::get( "ortiz", "opening_scene" );
	driver_clip = maps\_coopscene_fav::get( "driver", "opening_scene" );

	// Ortiz, at his clip's start.
	org = getStartOrigin( node.origin, node.angles, ortiz_clip );
	ang = getStartAngles( node.origin, node.angles, ortiz_clip );
	fav.ortiz = spawn_actor( org, flat_angles( ang ), "ortiz", "actor_enemy_villian_ortiz_so", undefined, "axis" );
	if ( isDefined( fav.ortiz ) )
	{
		fav.ortiz.animname = "ortiz";
		fav.ortiz.coop_cast = "ortiz";
		fav.ortiz.health = 100000;
		fav.ortiz.allowpain = false;
		fav.ortiz setCanDamage( true );
		fav.ortiz thread ortiz_damage_watch();
		fav.ortiz.coop_fav_scene = true;
		spawner = ent( "chase_target_spawner" );
		if ( isDefined( spawner ) )
			fav.ortiz.coop_spawner = spawner;
	}

	// The informants (gunner1 / gunner2), waiting at the meet.
	fav.informants = [];
	fav.gunmen = [];
	names = strTok( "opening_gunner1_spawner opening_gunner2_spawner", " " );
	keys = strTok( "gunner1 gunner2", " " );
	for ( i = 0; i < names.size; i++ )
	{
		s = ent( names[ i ] );
		cls = "actor_civilian_slum_male_so";
		if ( isDefined( s ) )
			cls = s.classname;
		clip = maps\_coopscene_fav::get( keys[ i ], "opening_scene" );
		org = getStartOrigin( node.origin, node.angles, clip );
		ang = getStartAngles( node.origin, node.angles, clip );
		inf = spawn_actor( org, flat_angles( ang ), "informant" + i, cls, i + 1, "allies" );
		if ( !isDefined( inf ) )
			continue;
		inf.animname = keys[ i ];
		inf setCanDamage( false );
		fav.informants[ fav.informants.size ] = inf;
	}

	// The driver: an enemy soldier, who fights once his clip is over.
	s = ent( "opening_driver_spawner" );
	driver = undefined;
	if ( isDefined( s ) )
		driver = maps\_coopmission::spawn_soldier( s );
	if ( isDefined( driver ) )
	{
		driver.animname = "driver";
		driver.ignoreall = true;
		driver.coop_cast = "driver";
		driver forceTeleport( getStartOrigin( node.origin, node.angles, driver_clip ),
			flat_angles( getStartAngles( node.origin, node.angles, driver_clip ) ) );
		fav.gunmen[ 0 ] = driver;
	}

	// The van carries the driver in (it follows his seat), as IW4 favela's
	// van drives its path while guy4 rides it.
	level thread van_scene( node, driver_clip );

	// Everybody's clip from the same moment (IW4 anim_single on the group);
	// the informants' from 6.4 s, so they fall to the driver's shot.
	log( "the meet: the scene clips at " + node.origin );
	if ( isDefined( fav.ortiz ) )
		fav.ortiz thread scene_then( ortiz_clip, node, "zonly_physics", ::ortiz_scene_done );
	if ( isDefined( driver ) )
	{
		driver thread scene_then( driver_clip, node, "zonly_physics", ::driver_scene_done );
		driver thread driver_in_van( node );
	}
	for ( i = 0; i < fav.informants.size; i++ )
		fav.informants[ i ] thread informant_scene( node, 6.4 );

	// 1:38 Ghost picks out Ortiz and the informants; 1:40 Ortiz calms them;
	// 1:43 the trap: Ghost sees it, an informant regrets coming.
	wait_scene( start, 7.4 );
	level thread radio( "favela_gst_02", 3 );
	wait_scene( start, 9.6 );
	level thread say( fav.ortiz, "favela_ortiz_01", 3 );
	wait_scene( start, 12.4 );
	level thread radio( "favela_gst_03", 3 );
	if ( fav.informants.size > 0 )
		level thread say( fav.informants[ 0 ], "favela_inf2_01", 0.8 );
	// 13.7 s: the driver's shots (the clip's fire note); the informants fall.
	wait_scene( start, 13.5 );
	if ( isDefined( driver ) && isAlive( driver ) && isDefined( driver.coop_weapon ) && driver.coop_weapon != "" )
	{
		for ( i = 0; i < fav.informants.size; i++ )
		{
			inf = fav.informants[ i ];
			if ( isDefined( inf ) )
				driver maps\_coopmission::actor_fire_round( driver.coop_weapon, driver.origin + ( 0, 0, 56 ), inf.origin + ( 0, 0, 44 ) );
			wait( 0.15 );
		}
	}
	log( "the informants are shot" );

	// 14.9 s: Ortiz turns on the car; the windscreen shatters.
	wait_scene( start, 14.9 );
	level thread ortiz_fires_on_car( start );
	wait_scene( start, 15.3 );
	level thread ortiz_flees();
	for ( i = 0; i < fav.cars.size; i++ )
	{
		fav.cars[ i ] playSound( "scn_favela_car_glass_shatter" );
		fav.cars[ i ] hidePart( "tag_glass_front" );
		fav.cars[ i ] showPart( "tag_glass_front_d" );
	}
	earthquake( 0.25, 0.6, fav.cars[ 0 ].origin, 400 );
	wait( 0.6 );

	// 1:47 Price: chase Ortiz, Ghost cuts him off.
	level thread objective_text( &"COOP_TAKE_MP_FAV_TROPICAL_CAPTURE" );
	ortiz_marker();
	radio( "favela_pri_intercept01", 2.5 );

	// 1:49 out of the car.
	leave_cars();
	level.coop_fav_step = "chase";
	level notify( "coop_fav_chase" );
	level thread opening_banter();
}

// Waits until <t> seconds after the scene's start.
wait_scene( start, t )
{
	left = t - ( getTime() - start ) / 1000;
	if ( left > 0 )
		wait( left );
}

// One actor's scene clip at the node, then <done>.
scene_then( clip, node, mode, done )
{
	self endon( "death" );
	self maps\_coopanim::play_anim( clip, node, mode );
	if ( isDefined( done ) )
		self [[ done ]]();
}

ortiz_scene_done()
{
	self.coop_fav_scene = undefined;
	log( "Ortiz's scene is over at " + self.origin );
	self notify( "coop_fav_scene_done" );
}

// The driver rides the van in seated (his clip's first 9 s, the seat 49
// units up); the soldier is not shown until he climbs out, and if the van or
// the street held him back he is put where his clip has him then.
driver_in_van( node )
{
	self endon( "death" );
	self hide();
	wait( 10.6 );
	spot = node.origin + ( -46, -40, 0 );
	if ( distance2D( self.origin, spot ) > 60 )
	{
		log( "the driver was held back at " + self.origin + "; placed at " + spot );
		self forceTeleport( ground( spot + ( 0, 0, 16 ) ), self.angles );
	}
	self show();
}

// The driver fights once his clip is over.
driver_scene_done()
{
	self.coop_cast = undefined;
	self.ignoreall = false;
	self notify( "coop_cast_pose" );
	log( "the driver joins the fight at " + self.origin );
}

// An informant waits at his start (the casual idle), plays his clip from
// <delay> s (he falls 7.3 s into it) and lies there on his back.  They are
// gone once the chase has moved on.
informant_scene( node, delay )
{
	self endon( "death" );
	self.coop_idle_anim = %casual_stand_idle;
	self notify( "coop_cast_pose" );
	wait( delay );
	clip = maps\_coopscene_fav::get( self.animname, "opening_scene" );
	// The pose he is left in when the clip is over (on his back).
	self.coop_idle_anim = %dying_back_idle;
	self maps\_coopanim::play_anim( clip, node, "zonly_physics" );
	self.coop_pose_anim = undefined;
	self notify( "coop_cast_pose" );
	flag_wait_or_timeout( "combat_zone_20", 240 );
	while ( isDefined( self ) && seen_by_player( self ) )
		wait( 1 );
	if ( isDefined( self ) )
		self delete();
}

// Ortiz's rounds at the windscreen on his clip's fire notes (0.61 .. 0.88 of
// 24.5 s), from his own gun.
ortiz_fires_on_car( start )
{
	ortiz = level.fav.ortiz;
	car = level.fav.cars[ 0 ];
	if ( !isDefined( ortiz ) || !isDefined( car ) || !isDefined( ortiz.coop_weapon ) || ortiz.coop_weapon == "" )
		return;
	notes = strTok( "14.9 15.7 16.7 17.9 19.1 19.6 19.8 21.3 21.6", " " );
	for ( i = 0; i < notes.size; i++ )
	{
		wait_scene( start, float( notes[ i ] ) );
		if ( !isDefined( ortiz ) || !isAlive( ortiz ) )
			return;
		target = car.origin + anglesToForward( car.angles ) * 40 + ( randomFloatRange( -20, 20 ), randomFloatRange( -20, 20 ), 44 );
		ortiz maps\_coopmission::actor_fire_round( ortiz.coop_weapon, ortiz.origin + ( 0, 0, 56 ), target );
	}
}

// The van: it follows the driver's seat through his clip's ride (4.5 ..
// 7.3 s) and stands at opening_scene_node_vehicles, its rear doors to the
// rookie's car; it opens them as the driver climbs out.
van_scene( node, driver_clip )
{
	fav = level.fav;
	v = ent( "van" );
	vnode = ent( "opening_scene_node_vehicles" );
	park = node.origin + ( 112, 0, 0 );
	if ( isDefined( vnode ) )
		park = vnode.origin;
	park = ground( park + ( -1.9, 3.6, 40 ) );
	yaw = node.angles[ 1 ] - 171;
	// The seat's offset from the van: the driver's spot when the van stops
	// (7.3 s) against the van's parked place.
	seatStart = getStartOrigin( node.origin, node.angles, driver_clip );
	seatParked = node.origin + ( -18, -19, 0 );
	start = ground( seatStart + ( park - seatParked ) + ( 0, 0, 40 ) );
	model = "vehicle_uaz_van";
	if ( isDefined( v ) && isDefined( v.model ) && v.model != "" )
		model = v.model;
	van = spawn( "script_model", start );
	van.angles = ( 0, yaw, 0 );
	van setModel( model );
	if ( isDefined( v ) )
		v hide();
	fav.van = van;
	// IW4 favela: van anim_single_solo( van, "door_open" ) from the moment
	// the four start theirs (favela_van_shootout_van, 19.2 s: the doors the
	// driver climbs out of), its wheels turning while it rolls in.
	van maps\_coopscene_fav::play_vehicle_clip( "favela_van_shootout_van" );
	// The cars' own scene clips (car1 / car2: their root sits exactly on
	// player_vehicle / player_vehicle2 from this node, MEASURED), the
	// passenger door only, as the riders get out on that side.
	level thread cars_scene();
	van playLoopSound( "veh_uaz_engine_low" );
	wait( 4.5 );
	van playSound( "scn_favela_van_pullup" );
	wheels = van maps\_coopscene_fav::play_vehicle_clip( "uaz_driving_idle_forward" );
	van moveTo( park, 2.8, 0.3, 1.4 );
	log( "van pulls up from " + start + " to " + park );
	wait( 2.8 );
	van maps\_coopscene_fav::stop_vehicle_clip( wheels );
	van stopLoopSound();
	van playSound( "scn_favela_van_skid2stop" );
	wait( 1.8 );
	van playSound( "scn_favela_van_door_open" );
}

// The cars' passenger-door clips (favela_van_shootout_car1 / car2
// _passenger_door_open_only, 25.5 s, the scene's length).  C1's player
// exit clip in this zone notes scn_mpfav_cardoor_open at 20.1 s; the riders
// get out about 18.4 s into the scene here, so the car clips start 1.7 s in
// (INFERRED: the door bones' timing was not measured, only that note).
cars_scene()
{
	names = strTok( "favela_van_shootout_car1_passenger_door_open_only favela_van_shootout_car2_passenger_door_open_only", " " );
	cars = level.fav.cars;
	for ( i = 0; i < cars.size && i < names.size; i++ )
	{
		if ( isDefined( cars[ i ] ) )
			cars[ i ] maps\_coopscene_fav::play_vehicle_clip( names[ i ], 1.7 / 25.5 );
	}
}

opening_banter()
{
	level endon( "coop_mission_over" );
	wait( 1.5 );
	radio( "favela_gst_04", 2.5 );
	radio( "favela_pri_alive01", 2 );
	wait( 0.5 );
	radio( "favela_gst_07", 2.5 );
}

shoot_informants()
{
	fav = level.fav;
	for ( i = 0; i < fav.informants.size; i++ )
	{
		inf = fav.informants[ i ];
		if ( !isDefined( inf ) || !isAlive( inf ) )
			continue;
		shooter = undefined;
		if ( fav.gunmen.size > 0 )
			shooter = fav.gunmen[ i % fav.gunmen.size ];
		if ( isDefined( shooter ) && isAlive( shooter ) && isDefined( shooter.coop_weapon ) && shooter.coop_weapon != "" )
		{
			start = shooter.origin + ( 0, 0, 56 ) + anglesToForward( vectorToAngles( inf.origin - shooter.origin ) ) * 16;
			if ( isDefined( shooter.coop_weapon_model ) )
				start = shooter getTagOrigin( "tag_flash" );
			shooter maps\_coopmission::actor_fire_round( shooter.coop_weapon, start, inf.origin + ( 0, 0, 48 ) );
			wait( 0.15 );
			if ( isDefined( shooter ) && isAlive( shooter ) )
				shooter maps\_coopmission::actor_fire_round( shooter.coop_weapon, start, inf.origin + ( 0, 0, 40 ) );
		}
		if ( isAlive( inf ) )
		{
			if ( isDefined( shooter ) && isAlive( shooter ) )
				inf doDamage( inf.health + 100, shooter.origin, shooter );
			else
				inf doDamage( inf.health + 100, inf.origin );
		}
		log( "informant " + i + " shot, alive " + isAlive( inf ) );
		wait( 0.3 );
	}
}

// The "capture" marker on Ortiz (objective 1, on him; the recording's
// floating label is objective_setpointertextoverride, which this server does
// not register -- see the doc's mission section).
ortiz_marker()
{
	fav = level.fav;
	if ( !isDefined( fav.ortiz ) )
		return;
	objective_add( 1, "active", fav.ortiz.origin );
	objective_onEntity( 1, fav.ortiz );
	ortiz_marker_hud( true );
}

// Price's "we need him alive" when a player shoots Ortiz (the recording,
// 3:32); Ortiz cannot die.
ortiz_damage_watch()
{
	self endon( "death" );
	level endon( "coop_mission_over" );
	lines = strTok( "favela_pri_0269 favela_pri_0266 favela_pri_0268", " " );
	n = 0;
	last = -100000;
	for ( ;; )
	{
		self waittill( "damage", amount, attacker );
		self.health = 100000;
		if ( !isDefined( attacker ) || !isPlayer( attacker ) )
			continue;
		if ( getTime() - last < 8000 )
			continue;
		last = getTime();
		level thread radio( lines[ n % lines.size ], 2.5 );
		n++;
	}
}

// --------------------------------------------------------------------------
// The chase: Ortiz runs node to node (escape_path_start -> me0_auto323),
// setting each node's arrive flag and waiting at a node with a leave flag
// until the player crosses its trigger (IW4 favela's runner).
// --------------------------------------------------------------------------

ortiz_flees()
{
	level.fav.ortiz_fleeing = true;
	level notify( "coop_fav_ortiz_flees" );
}

chase()
{
	fav = level.fav;
	ortiz = fav.ortiz;
	if ( !isDefined( ortiz ) )
	{
		log( "no Ortiz; the chase is skipped" );
		return;
	}
	ortiz endon( "death" );
	level endon( "coop_mission_over" );

	path = maps\_coopvehicle::get_path( "escape_path_start" );
	fav.chase_path = path;
	log( "chase path of " + path.size + " nodes" );
	fav.chase_reached = -1;
	// The meet's clip carries him off to the first node.
	while ( isDefined( ortiz.coop_fav_scene ) )
		wait( 0.1 );
	for ( n = 0; n < path.size; n++ )
	{
		node = path[ n ];
		fav.chase_node = n;
		// A node with an `animation` key is the scene node of a traversal
		// clip (favela_escape's runner: the rail vault, the rooftop jump):
		// he runs to the clip's start spot and plays it there.
		clip = undefined;
		if ( isDefined( node.animation ) )
			clip = maps\_coopscene_fav::get( "ortiz", node.animation );
		dest = node.origin;
		if ( isDefined( clip ) )
			dest = getStartOrigin( node.origin, maps\_coopanim::node_angles( node, ortiz ), clip );
		ortiz move_actor( dest, true, undefined, true );
		fav.chase_reached = n;
		if ( isDefined( node.script_flag_set ) )
			flag_set( node.script_flag_set );
		if ( isDefined( node.script_flag_wait ) && !flag( node.script_flag_wait ) )
		{
			ortiz.coop_hunting = undefined;
			log( "Ortiz waits at " + node.origin + " for " + node.script_flag_wait );
			// He faces back toward the nearest player while he waits, and
			// bolts if one gets right up to him.
			p = nearest_player( ortiz.origin );
			if ( isDefined( p ) )
				ortiz.angles = flat_angles( vectorToAngles( p.origin - ortiz.origin ) );
			fav.ortiz_waiting = true;
			while ( !flag( node.script_flag_wait ) && nearest_player_distance( ortiz.origin ) > 220 )
				wait( 0.25 );
			fav.ortiz_waiting = undefined;
			while ( isDefined( ortiz.coop_fav_scene ) )
				wait( 0.1 );
		}
		if ( isDefined( clip ) )
		{
			log( "Ortiz plays " + node.animation + " at " + node.origin );
			ortiz.coop_fav_scene = true;
			ortiz maps\_coopanim::play_anim( clip, node, "noclip" );
			ortiz.coop_fav_scene = undefined;
			ortiz forceTeleport( ground( ortiz.origin + ( 0, 0, 16 ) ), ortiz.angles );
		}
	}
	fav.chase_done = true;
	log( "Ortiz at the end of his run, " + ortiz.origin );
}

// The chase node that waits for a flag (its index on the run), or -1.
chase_node_waiting_for( name )
{
	path = level.fav.chase_path;
	for ( k = 0; k < path.size; k++ )
	{
		if ( isDefined( path[ k ].script_flag_wait ) && path[ k ].script_flag_wait == name )
			return k;
	}
	return -1;
}

// The objective points obj_10 .. obj_100 in order: each is the compass /
// on-screen marker until a player reaches it or crosses the trigger the map
// pairs with it (the leave or arrive flag beside it); reaching the point
// counts as crossing that trigger.  A point whose flag releases Ortiz from a
// chase node is shown once he is waiting there, so the marker never runs
// ahead of him.
objectives()
{
	level endon( "coop_mission_over" );
	list = strTok( "obj_10 obj_11 obj_15 obj_20 obj_25 obj_30 obj_40 obj_41 obj_45 obj_47 obj_50 obj_60 obj_70 obj_72 obj_75 obj_80 obj_90 obj_100", " " );
	pairs = strTok( "chase_05_leave chase_11_leave chase_15_leave chase_20_leave chase_25_leave chase_30_leave chase_40_leave chase_41_leave obj_45_arrive obj_47_arrive obj_50_complete chase_60_leave chase_70_leave chase_72_leave obj_75_arrive chase_80_leave chase_90_leave ending_tackle", " " );
	// The helper group whose trigger volume lies on the way to each point:
	// a player who reaches the point by another way (or the probe's walker,
	// which teleports past a wall) still meets that group.
	zones = strTok( "- - combat_zone_10 combat_zone_20 - combat_zone_30 combat_zone_35 - - - combat_zone_40 combat_zone_45 - combat_zone_50 - - combat_zone_60 -", " " );
	while ( !isDefined( level.fav.chase_path ) )
		wait( 0.25 );
	for ( i = 0; i < list.size; i++ )
	{
		o = ent( list[ i ] );
		if ( !isDefined( o ) )
			continue;
		k = chase_node_waiting_for( pairs[ i ] );
		waited = 0;
		while ( k >= 0 && level.fav.chase_reached < k && waited < 30 && !flag( pairs[ i ] ) )
		{
			wait( 0.25 );
			waited += 0.25;
		}
		if ( flag( pairs[ i ] ) )
		{
			if ( zones[ i ] != "-" && !flag( zones[ i ] ) )
				flag_set( zones[ i ] );
			continue;
		}
		objective_add( 0, "active", o.origin );
		level.fav.objective_point = o;
		point_marker( o.origin );
		log( "objective point " + list[ i ] + " at " + o.origin + " (until " + pairs[ i ] + ")" );
		for ( ;; )
		{
			if ( zones[ i ] != "-" && !flag( zones[ i ] ) && ( flag( pairs[ i ] ) || nearest_player_distance( o.origin ) < 400 ) )
			{
				log( "a player came to " + list[ i ] + " without crossing " + zones[ i ] + "'s trigger" );
				flag_set( zones[ i ] );
			}
			if ( flag( pairs[ i ] ) )
				break;
			if ( list[ i ] != "obj_100" && nearest_player_distance( o.origin ) < 96 )
			{
				flag_set( pairs[ i ] );
				break;
			}
			if ( list[ i ] == "obj_100" && isDefined( level.fav.tackled ) )
				break;
			wait( 0.25 );
		}
		level.fav.respawn = o.origin;
	}
	objective_delete( 0 );
	point_marker();
}

// The 3D markers the recording draws: a dot with the distance on the next
// objective point (e.g. "21m"), and one over Ortiz.  A world-space waypoint
// HUD element, as the crash-site mission draws its markers; the floating
// "take alive" label over Ortiz is objective_setpointertextoverride, which
// this server does not register (M2.5), so he carries the dot alone.
point_marker( origin )
{
	if ( isDefined( level.fav.point_marker ) )
		level.fav.point_marker destroy();
	level.fav.point_marker = undefined;
	if ( !isDefined( origin ) )
		return;
	wp = newHudElem();
	wp.x = origin[ 0 ];
	wp.y = origin[ 1 ];
	wp.z = origin[ 2 ] + 24;
	wp.alpha = 0.9;
	wp.hidewheninmenu = true;
	wp setShader( "objpoint_default", 8, 8 );
	wp setWaypoint( true, true );
	level.fav.point_marker = wp;
}

ortiz_marker_hud( on )
{
	if ( isDefined( level.fav.ortiz_marker ) )
		level.fav.ortiz_marker destroy();
	level.fav.ortiz_marker = undefined;
	ortiz = level.fav.ortiz;
	if ( !on || !isDefined( ortiz ) )
		return;
	wp = newHudElem();
	wp.x = ortiz.origin[ 0 ];
	wp.y = ortiz.origin[ 1 ];
	wp.z = ortiz.origin[ 2 ] + 80;
	wp setTargetEnt( ortiz );
	wp.alpha = 0.9;
	wp.hidewheninmenu = true;
	wp setShader( "objpoint_default", 8, 8 );
	wp setWaypoint( true, false );
	level.fav.ortiz_marker = wp;
}

// The AI-only triggers Ortiz crosses (spawnflags 9: AI_AXIS | NOTPLAYER):
// start_technical sends the technical, combat_zone_51 / _55_ai_trigger
// release those groups.
ai_triggers()
{
	level endon( "coop_mission_over" );
	level waittill( "coop_fav_ortiz_flees" );
	trigs = [];
	names = strTok( "start_technical combat_zone_51_ai_trigger combat_zone_55_ai_trigger", " " );
	flags = strTok( "start_technical combat_zone_51 combat_zone_55", " " );
	for ( i = 0; i < names.size; i++ )
	{
		t = ent( names[ i ] );
		if ( isDefined( t ) )
			level thread ai_trigger_think( t, flags[ i ] );
	}
}

ai_trigger_think( t, name )
{
	level endon( "coop_mission_over" );
	for ( ;; )
	{
		wait( 0.25 );
		ortiz = level.fav.ortiz;
		if ( !isDefined( ortiz ) )
			continue;
		if ( ortiz isTouching( t ) || distance( ( ortiz.origin[ 0 ], ortiz.origin[ 1 ], 0 ), ( t.origin[ 0 ], t.origin[ 1 ], 0 ) ) < 96 )
			break;
	}
	log( "Ortiz crossed " + t.targetname );
	flag_set( name );
	if ( name == "combat_zone_51" || name == "combat_zone_55" )
	{
		spawners = getEntArray( name, "targetname" );
		for ( i = 0; i < spawners.size; i++ )
		{
			if ( maps\_coopmission::is_enemy_spawner( spawners[ i ] ) )
				level thread maps\_coopmission::spawner_spawn_count( spawners[ i ] );
		}
	}
}

// IW4 _spawner: a soldier whose spawner targets a struct with an
// `animation` key runs to it and plays that clip there before he fights
// (combat_zone_45's two men smash the windows of the house they hold:
// me0_auto606 / 616, window_smash_stop_inside).  The helpers are
// _coopmission's soldiers; this only watches for the new ones.
soldier_scenes()
{
	level endon( "coop_mission_over" );
	for ( ;; )
	{
		wait( 0.5 );
		ais = level.coop_enemies;
		if ( !isDefined( ais ) )
			continue;
		for ( i = 0; i < ais.size; i++ )
		{
			ai = ais[ i ];
			if ( !isDefined( ai ) || !isAlive( ai ) || isDefined( ai.coop_fav_spawn_scene ) || !isDefined( ai.coop_spawner ) )
				continue;
			ai.coop_fav_spawn_scene = true;
			if ( !isDefined( ai.coop_spawner.target ) )
				continue;
			node = struct( ai.coop_spawner.target );
			if ( !isDefined( node ) || !isDefined( node.animation ) )
				continue;
			// "a,b" lists a sequence; the first that ships is played.
			names = strTok( node.animation, "," );
			for ( k = 0; k < names.size; k++ )
			{
				clip = maps\_coopscene_fav::get( "generic", names[ k ] );
				if ( isDefined( clip ) )
				{
					ai thread soldier_spawn_scene( node, clip, names[ k ] );
					break;
				}
			}
		}
	}
}

soldier_spawn_scene( node, clip, name )
{
	self endon( "death" );
	self.ignoreall = true;
	self.coop_cast = "spawn_scene";
	maps\_coopanim::reach_spot( getStartOrigin( node.origin, maps\_coopanim::node_angles( node, self ), clip ),
		getStartAngles( node.origin, maps\_coopanim::node_angles( node, self ), clip )[ 1 ], 16, 6 );
	log( "soldier plays " + name + " at " + node.origin );
	self maps\_coopanim::play_anim( clip, node, "zonly_physics" );
	self.coop_cast = undefined;
	self.ignoreall = false;
	self notify( "coop_cast_pose" );
}

// The chase's lines, on the flags the player and Ortiz set (the recording,
// 2:05 .. 2:45; there are none on the rooftops or in the houses).
chase_dialogue()
{
	level endon( "coop_mission_over" );
	flag_wait( "combat_zone_20" );
	level thread ortiz_orders();
	say( level.fav.ortiz, "favela_ortiz_02", 1.5 );
	wait( 4 );
	radio( "favela_pri_tangos01", 2 );
	flag_wait( "combat_zone_30" );
	radio( "favela_pri_ortiz01", 1.5 );
	flag_wait( "combat_zone_35" );
	level thread ortiz_orders();
	say( level.fav.ortiz, "favela_ortiz_04", 1.5 );
}

// Ortiz waves his men on as he shouts (favela_order_grunts_ortiz, IW4
// favela_escape's runner ordering his grunts), when he is waiting at a chase
// node and not in another clip; the clip ends with him running on.
ortiz_orders()
{
	ortiz = level.fav.ortiz;
	if ( !isDefined( ortiz ) || !isAlive( ortiz ) || !isDefined( level.fav.ortiz_waiting ) || isDefined( ortiz.coop_fav_scene ) )
		return;
	clip = maps\_coopscene_fav::get( "ortiz", "order_grunts" );
	if ( !isDefined( clip ) )
		return;
	ortiz.coop_fav_scene = true;
	ortiz maps\_coopanim::play_anim( clip, undefined, "zonly_physics" );
	if ( isDefined( ortiz ) )
		ortiz.coop_fav_scene = undefined;
}

// --------------------------------------------------------------------------
// The technical: when Ortiz reaches the street corner (start_technical) the
// pickup drives its authored path (vnode_technical1_start, 25 mph) round the
// corner toward the rookie and stops.  Its crew are the three
// script_vehicleride combat_zone_10 spawners (script_startingposition 0, 1,
// 2): the driver and passenger sit on tag_driver / tag_passenger in the
// seated driving idle, the gunner stands on tag_gunner behind the M2
// (weapon_m2_50cal_center on tag_50cal) -- actors ride a tag through
// codol_actorRide.  At the stop the driver and passenger jump off and fight;
// the gunner fires bursts from the mount until he is killed.  Price calls
// the ambush, Ortiz calls fire, Price calls cover (2:05 .. 2:16).
// --------------------------------------------------------------------------

technical()
{
	level endon( "coop_mission_over" );
	vehicles = getEntArray( "script_vehicle_pickup_technical", "classname" );
	if ( vehicles.size <= 0 )
		return;
	v = vehicles[ 0 ];
	path = maps\_coopvehicle::get_path( v.target );
	truck = spawn( "script_model", ground( v.origin + ( 0, 0, 16 ) ) );
	truck.angles = flat_angles( v.angles );
	truck setModel( "vehicle_pickup_technical" );
	truck attach( "weapon_m2_50cal_center", "tag_50cal" );
	v hide();
	level.fav.technical = truck;

	// Either Ortiz's corner or the player reaching the combat_zone_10 street.
	while ( !flag( "start_technical" ) && !flag( "combat_zone_10" ) )
		wait( 0.25 );

	// The crew climbs on (they are out of sight round the corner).
	crew = [];
	tags = strTok( "tag_driver tag_passenger tag_gunner", " " );
	riders = by_noteworthy( "combat_zone_10" );
	for ( i = 0; i < riders.size; i++ )
	{
		r = riders[ i ];
		if ( !maps\_coopmission::is_enemy_spawner( r ) || ( isDefined( r.targetname ) && r.targetname == "combat_zone_10" ) )
			continue;
		pos = 0;
		if ( isDefined( r.script_startingposition ) )
			pos = int( r.script_startingposition );
		if ( pos < 0 || pos > 2 || isDefined( crew[ pos ] ) )
			continue;
		spot = spawnStruct();
		spot.classname = r.classname;
		spot.origin = truck getTagOrigin( tags[ pos ] );
		spot.angles = truck.angles;
		spot.script_noteworthy = "combat_zone_10";
		ai = maps\_coopmission::spawn_soldier( spot );
		if ( !isDefined( ai ) )
			continue;
		ai.ignoreall = true;
		ai animMode( "nophysics" );
		// The rider idles C1 ships for this mission (IW4 _vehicle_aianim's
		// technical seats); technical_driver_idle itself ships in no list.
		seat = "gunner_idle";
		if ( pos == 0 )
			seat = "driver_idle";
		else if ( pos == 1 )
			seat = "passenger_idle";
		idle = maps\_coopscene_fav::get( "technical", seat );
		if ( !isDefined( idle ) && pos < 2 )
			idle = %casual_crouch_idle;
		if ( isDefined( idle ) )
		{
			ai.coop_cast = "technical";
			ai.coop_idle_anim = idle;
			ai setAnimKnobAll( idle, %body, 1, 0.1, 1 );
			ai notify( "coop_cast_pose" );
		}
		ai codol_actorRide( truck, tags[ pos ] );
		crew[ pos ] = ai;
	}

	log( "technical rolls (" + path.size + " nodes), crew " + crew.size );
	level thread radio( "favela_pri_ambush01", 3.5 );
	truck playLoopSound( "veh_pickuptruck_engine_high" );
	// IW4 vehicles\technical's driving idle (wheels and suspension) while it
	// rolls.
	wheels = truck maps\_coopscene_fav::play_vehicle_clip( "technical_driving_idle_forward" );
	truck maps\_coopvehicle::follow_path( path, 0, 25, true );
	truck maps\_coopscene_fav::stop_vehicle_clip( wheels );
	truck stopLoopSound();
	truck playLoopSound( "veh_pickuptruck_idle_low" );
	say( level.fav.ortiz, "favela_ortiz_03", 0.8 );
	level thread radio( "favela_pri_getcover01", 4.5 );

	// Driver and passenger off, one to each side.
	for ( pos = 0; pos < 2; pos++ )
	{
		ai = crew[ pos ];
		if ( !isDefined( ai ) || !isAlive( ai ) )
			continue;
		side = -1;
		if ( pos == 1 )
			side = 1;
		ai codol_actorRide();
		ai animMode( "normal" );
		out = "driver_out";
		if ( pos == 1 )
			out = "passenger_out";
		clip = maps\_coopscene_fav::get( "technical", out );
		if ( isDefined( clip ) )
		{
			// IW4 _vehicle_aianim's unload: the seat's climb-out clip, from
			// the seat down to the street (42 units, out of the door).
			ai thread technical_climb_out( clip );
			continue;
		}
		ai forceTeleport( ground( truck.origin + anglesToRight( truck.angles ) * ( 72 * side ) + ( 0, 0, 16 ) ), truck.angles );
		ai.coop_cast = undefined;
		ai notify( "coop_cast_pose" );
		ai.ignoreall = false;
	}
	log( "technical stopped at " + truck.origin );
	if ( isDefined( crew[ 2 ] ) )
		level thread technical_gunner( truck, crew[ 2 ] );
	wait( 6 );
	truck stopLoopSound();
}

technical_climb_out( clip )
{
	self endon( "death" );
	self maps\_coopanim::play_anim( clip, undefined, "noclip" );
	self forceTeleport( ground( self.origin + ( 0, 0, 16 ) ), self.angles );
	self.coop_cast = undefined;
	self notify( "coop_cast_pose" );
	self.ignoreall = false;
}

// The gunner's bursts from the M2 at the nearest player he can see.  His
// shots use his own role weapon (a slot of the coop weapon window C1 has
// registered; the turret's 50cal_turret_technical is not in that window).
technical_gunner( truck, ai )
{
	level endon( "coop_mission_over" );
	ai endon( "death" );
	if ( !isDefined( ai.coop_weapon ) || ai.coop_weapon == "" )
		return;
	truck playSound( "scn_favela_technicaltruck_attack" );
	for ( ;; )
	{
		wait( 0.8 + randomFloat( 1.2 ) );
		muzzle = truck getTagOrigin( "tag_50cal" ) + ( 0, 0, 12 );
		target = undefined;
		players = living_players();
		for ( i = 0; i < players.size; i++ )
		{
			eye = players[ i ] getEye();
			if ( distance( eye, muzzle ) < 2000 && bulletTracePassed( muzzle, eye, false, truck ) )
				target = players[ i ];
		}
		if ( !isDefined( target ) )
			continue;
		for ( b = 0; b < 5; b++ )
		{
			aim = target getEye() - ( 0, 0, 10 ) + ( randomFloatRange( -40, 40 ), randomFloatRange( -40, 40 ), randomFloatRange( -24, 24 ) );
			// The round leaves the truck's gun; the fire event (sound, flash)
			// is the gunner's own (codol_actorShoot).
			ai maps\_coopmission::actor_fire_round( ai.coop_weapon, muzzle + anglesToForward( vectorToAngles( aim - muzzle ) ) * 30, aim );
			wait( 0.1 );
		}
	}
}

// The parked van in the street east of the houses backs up to block it when
// the player comes near (move_van; scn_favela_van_backup_skid).
van_blocker()
{
	level endon( "coop_mission_over" );
	van = ent( "van_blocker" );
	if ( !isDefined( van ) || !isDefined( van.target ) )
		return;
	dest = struct( van.target );
	if ( !isDefined( dest ) )
		return;
	flag_wait( "move_van" );
	van playSound( "scn_favela_van_backup_skid" );
	van moveTo( dest.origin, 1.5, 0.2, 0.6 );
	log( "van_blocker backs up to " + dest.origin );
}

// --------------------------------------------------------------------------
// Civilians: each civ_* spawner group comes out when its flag is set (the
// first ones a few seconds into the meet), runs to the struct it targets
// and is gone; they cannot be hurt.
// --------------------------------------------------------------------------

civilians()
{
	level endon( "coop_mission_over" );
	ents = getEntArray();
	groups = [];
	n = 0;
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( !isDefined( e.classname ) || !isSubStr( e.classname, "actor_civilian" ) || !isDefined( e.targetname ) ||
		     isDefined( e.coop_template ) )
			continue;
		e.coop_civ_index = n;
		n++;
		if ( !isDefined( groups[ e.targetname ] ) )
		{
			groups[ e.targetname ] = true;
			level thread civ_group( e.targetname );
		}
	}
	log( n + " civilian spawners" );
}

civ_group( name )
{
	level endon( "coop_mission_over" );
	if ( name == "civ_start" )
		level waittill( "coop_fav_meet" );
	else
		flag_wait( name );
	spawners = getEntArray( name, "targetname" );
	for ( i = 0; i < spawners.size; i++ )
		level thread civ_run( spawners[ i ] );
}

// The scene a civilian's node carries: its `animation` key (a struct keeps
// it; a script_origin node may not), else the addon's own pairing of the
// window and garage spawners with their nodes (me0_auto567 / me0_auto570).
civ_scene_name( dest, s )
{
	if ( isDefined( dest.animation ) )
		return dest.animation;
	if ( isDefined( s.targetname ) )
	{
		if ( s.targetname == "civ_window_1" )
			return "unarmed_shout_window";
		if ( s.targetname == "civ_garage_door" )
			return "unarmed_close_garage";
	}
	return undefined;
}

// IW4 _hiding_door_anims: the man behind the door opens it, peeks while the
// rookie is near (or a few seconds), and shuts it.  No door model ships for
// this spot, so it is his part alone.
civ_hiding_door( node )
{
	self endon( "death" );
	open = maps\_coopscene_fav::get( "hiding_door_guy", "open" );
	maps\_coopanim::place_at_start( self, open, node );
	log( "the hiding-door man peeks at " + node.origin );
	self maps\_coopanim::play_anim( open, node, "noclip" );
	idle = maps\_coopscene_fav::get( "hiding_door_guy", "idle" );
	if ( isDefined( idle ) )
		self maps\_coopanim::loop_idle( idle );
	for ( t = 0; t < 8 && nearest_player_distance( self.origin ) < 900; t += 0.5 )
		wait( 0.5 );
	wait( 2 );
	close = maps\_coopscene_fav::get( "hiding_door_guy", "close" );
	if ( isDefined( close ) )
		self maps\_coopanim::play_anim( close, node, "noclip" );
}

civ_run( s )
{
	level endon( "coop_mission_over" );
	if ( isDefined( s.script_delay_spawn ) )
		wait( float( s.script_delay_spawn ) );
	ai = spawn_actor( s.origin, flat_angles( s.angles ), "civilian", s.classname, s.coop_civ_index, "allies" );
	if ( !isDefined( ai ) )
		return;
	ai setCanDamage( false );
	dest = undefined;
	if ( isDefined( s.target ) )
		dest = struct( s.target );
	// The hiding-door man (IW4 _hiding_door: doorpeek open / idle / close).
	if ( isDefined( s.script_noteworthy ) && s.script_noteworthy == "hiding_door_guy" && isDefined( dest ) &&
	     maps\_coopscene_fav::has( "hiding_door_guy", "open" ) )
	{
		ai civ_hiding_door( dest );
		if ( isDefined( ai ) )
			ai delete();
		return;
	}
	// A node with an `animation` key is the civilian's scene there (favela's
	// civilian_window_1 shouting from the window, the man pulling the garage
	// door down): he goes to its start spot, plays it, and runs on to the
	// node's own target.
	clip = undefined;
	scene = undefined;
	if ( isDefined( dest ) )
		scene = civ_scene_name( dest, s );
	if ( isDefined( scene ) )
		clip = maps\_coopscene_fav::get( "generic", scene );
	if ( isDefined( clip ) )
	{
		ai move_actor( getStartOrigin( dest.origin, maps\_coopanim::node_angles( dest, ai ), clip ), true, 12 );
		log( "civilian plays " + scene + " at " + dest.origin );
		ai maps\_coopanim::play_anim( clip, dest, "noclip" );
		if ( isDefined( dest.script_flag_set ) )
			flag_set( dest.script_flag_set );
		next = undefined;
		if ( isDefined( dest.target ) )
			next = struct( dest.target );
		if ( isDefined( next ) )
			ai move_actor( next.origin, true, 12 );
	}
	else if ( isDefined( dest ) )
		ai move_actor( dest.origin, true, 12 );
	else
		wait( 4 );
	wait( 1 );
	if ( isDefined( ai ) )
		ai delete();
}

// --------------------------------------------------------------------------
// Ghost: at the start by the meet (ghost_intro); when Price sends the rookie
// after Ortiz he runs off toward the far side (ghost_intro_goal) and is gone
// once the player is well into the chase (delete_ghost_intro); at the end
// he comes in from ghost_ending and tackles Ortiz.
// --------------------------------------------------------------------------

ghost_intro_run()
{
	level endon( "coop_mission_over" );
	s = ent( "ghost_intro" );
	if ( !isDefined( s ) )
		return;
	ghost = spawn_actor( s.origin, flat_angles( s.angles ), "ghost", "actor_ally_hero_ghost_socom_so", undefined, "allies", "Ghost" );
	if ( !isDefined( ghost ) )
		return;
	ghost setCanDamage( false );
	// exposed_idle_alert_v1 is additive in common_gametype_coop: alone it
	// leaves the bind pose. exposed_aim_5 is a whole stand.
	ghost.coop_idle_anim = %exposed_aim_5;
	ghost notify( "coop_cast_pose" );
	goal = struct( "ghost_intro_goal" );
	if ( isDefined( goal ) )
		ghost thread move_actor( goal.origin, true, 30 );
	flag_wait_or_timeout( "delete_ghost_intro", 240 );
	if ( isDefined( ghost ) )
	{
		ghost delete();
		log( "Ghost leaves for the far side" );
	}
	level.coop_cast_list = [];
}

// --------------------------------------------------------------------------
// The end (3:57 .. 4:15): Ortiz at the last node waits for the player to
// come through (ending_tackle); Ghost runs in, takes him down and kneels on
// him; Price stops Ghost from beating him and has him taken to the landing
// zone; mission complete.  The tackle and beating are map animations C1
// does not load, so Ortiz falls (run_pain_fall), lies on his back
// (dying_back_idle) and Ghost kneels over him (casual_crouch_idle), with the
// zone's punch and takedown sounds.  The view then blurs out as the
// recording's does (setBlurForPlayer).
// --------------------------------------------------------------------------

ending()
{
	fav = level.fav;
	level endon( "coop_mission_over" );
	level.coop_fav_step = "ending";
	flag_wait_or_timeout( "ending_tackle", 120 );
	ortiz = fav.ortiz;
	s = ent( "ghost_ending" );
	origin = ( 978, 1191, 196 );
	angles = ( 0, 0, 0 );
	if ( isDefined( s ) )
	{
		origin = s.origin;
		angles = flat_angles( s.angles );
	}
	ghost = spawn_actor( ground( origin + ( 0, 0, 16 ) ), angles, "ghost", "actor_ally_hero_ghost_socom_so", undefined, "allies", "Ghost" );
	if ( isDefined( ghost ) )
		ghost setCanDamage( false );
	fav.ghost = ghost;
	level thread radio( "favela_gst_08", 3 );
	if ( isDefined( ghost ) && isDefined( ortiz ) && tackle_scene( ghost, ortiz ) )
	{
		ending_close();
		return;
	}
	if ( isDefined( ghost ) && isDefined( ortiz ) )
		ghost move_actor( ortiz.origin, true, 6 );
	if ( isDefined( ortiz ) )
	{
		ortiz playSound( "scn_ghost_takedown" );
		ortiz setGoalPos( ortiz.origin );
		ortiz.coop_hunting = undefined;
		ortiz animMode( "nophysics" );
		ortiz.coop_pose_anim = undefined;
		ortiz.coop_idle_anim = %dying_back_idle;
		ortiz setFlaggedAnimKnobAll( "pain", %run_pain_fall, %body, 1, 0.1, 1 );
		wait( 0.9 );
		ortiz setAnimKnobAll( %dying_back_idle, %body, 1, 0.3, 1 );
		ortiz notify( "coop_cast_pose" );
	}
	objective_delete( 1 );
	ortiz_marker_hud( false );
	fav.tackled = true;
	if ( isDefined( ghost ) && isDefined( ortiz ) )
	{
		ghost forceTeleport( ground( ortiz.origin + anglesToForward( ortiz.angles ) * 16 + ( 0, 0, 8 ) ),
			flat_angles( vectorToAngles( ortiz.origin - ghost.origin ) ) );
		// casual_crouch_idle, the kneel the other coop missions' cast hold
		// (common_human, common_gametype_coop).  exposed_crouch_idle_alert_v1
		// is listed there too, but the 2026-10-08 playthrough shows Ghost
		// standing in the bind pose while the server played it (its clip
		// ran to t 1.000 and held: a non-looping clip the client drops).
		ghost.coop_idle_anim = %casual_crouch_idle;
		ghost setAnimKnobAll( ghost.coop_idle_anim, %body, 1, 0.3, 1 );
		ghost notify( "coop_cast_pose" );
		level thread ghost_punches( ortiz );
	}
	log( "Ortiz taken down at " + ortiz.origin );

	ending_close();
}

// Price and Ghost close it out as the rookie comes up; the view blurs.
ending_close()
{
	ortiz = level.fav.ortiz;
	// Price as the rookie comes up (the recording: 4:01).
	waited = 0;
	while ( waited < 25 && isDefined( ortiz ) && nearest_player_distance( ortiz.origin ) > 400 )
	{
		wait( 0.25 );
		waited += 0.25;
	}
	radio( "favela_pri_0270", 2.5 );
	radio( "favela_gst_09", 1.5 );
	radio( "favela_pri_0272", 4.5 );
	level notify( "coop_fav_punches_over" );
	radio( "favela_gst_10", 2 );
	// 4:15 the view blurs and the mission is over.
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] setBlurForPlayer( 8, 2 );
	wait( 1.5 );
}

// The tackle as C1 ships it (favela_tackle_ghost / _ortiz, 17.2 s, at the
// end of Ortiz's run, me0_auto571 / me0_auto323: 1088 1216 220): Ghost dives
// into Ortiz from behind, they go down together 175 units on, and Ghost
// kneels on him and beats him (the clips' rest), then their _idle clips hold
// (not looping clips: replayed, maps\_coopscene_fav::hold_loop).
tackle_scene( ghost, ortiz )
{
	if ( !maps\_coopscene_fav::has( "ghost", "tackle" ) || !maps\_coopscene_fav::has( "ortiz", "tackle" ) )
		return false;
	node = ent( "me0_auto571" );
	if ( !isDefined( node ) )
		node = struct( "me0_auto323" );
	if ( !isDefined( node ) )
		return false;
	ghost.animname = "ghost";
	ortiz.animname = "ortiz";
	ortiz.coop_hunting = undefined;
	ortiz setGoalPos( ortiz.origin );
	// Ghost runs in to his dive's start, just behind Ortiz.
	maps\_coopanim::anim_reach_solo( ghost, "tackle", node, 24, 4 );
	objective_delete( 1 );
	ortiz_marker_hud( false );
	log( "the tackle at " + node.origin );
	// What each holds when his clip is over (replayed by tackle_hold).
	ortiz.coop_idle_anim = maps\_coopscene_fav::get( "ortiz", "tackle_idle" );
	ghost.coop_idle_anim = maps\_coopscene_fav::get( "ghost", "tackle_idle" );
	if ( !isDefined( ortiz.coop_idle_anim ) )
		ortiz.coop_idle_anim = %dying_back_idle;
	if ( !isDefined( ghost.coop_idle_anim ) )
		ghost.coop_idle_anim = %casual_crouch_idle;
	maps\_coopanim::anim_single_solo( ortiz, "tackle", node, "zonly_physics", true );
	level thread ghost_punches( ortiz );
	len = maps\_coopanim::anim_single_solo( ghost, "tackle", node, "zonly_physics", true );
	level.fav.tackled = true;
	level thread tackle_hold( ghost, ortiz, len );
	return true;
}

tackle_hold( ghost, ortiz, len )
{
	level endon( "coop_mission_over" );
	wait( 2.5 );
	if ( isDefined( ortiz ) )
		log( "Ortiz taken down at " + ortiz.origin );
	wait( len - 2.5 );
	if ( isDefined( ortiz ) )
		ortiz thread maps\_coopscene_fav::hold_loop( maps\_coopscene_fav::get( "ortiz", "tackle_idle" ) );
	if ( isDefined( ghost ) )
		ghost thread maps\_coopscene_fav::hold_loop( maps\_coopscene_fav::get( "ghost", "tackle_idle" ) );
	return true;
}

ghost_punches( ortiz )
{
	level endon( "coop_mission_over" );
	level endon( "coop_fav_punches_over" );
	for ( i = 0; i < 6 && isDefined( ortiz ); i++ )
	{
		ortiz playSound( "scn_favela_ortiz_punch" );
		wait( 0.9 + randomFloat( 0.6 ) );
	}
}

// --------------------------------------------------------------------------
// Probe harness verbs (_cooptest forwards unknown commands here):
//   favstatus            the chase state
//   favroute             player 0 plays the mission: waits out the meet,
//                        then WALKS (botMoveTo) to each objective point in
//                        turn, firing at live enemies in sight on the way
//                        (a walk that stalls is teleported, logged STUCK)
//   favjump <obj>        teleport player 0 to an objective point
// --------------------------------------------------------------------------

test_command( tokens )
{
	switch ( tokens[ 0 ] )
	{
	case "favstatus":
		fav_status();
		break;
	case "favroute":
		level thread fav_route();
		break;
	case "favflag":
		// favflag <flag>: set a mission flag as its trigger would (scene probes).
		if ( tokens.size >= 2 )
		{
			flag_set( tokens[ 1 ] );
			codolprintln( "CoDOL cooptest: favflag " + tokens[ 1 ] );
		}
		break;
	case "favhitortiz":
		// favhitortiz: player 0 wounds Ortiz (Price's "alive" line).
		p = getPlayers()[ 0 ];
		if ( isDefined( level.fav.ortiz ) && isDefined( p ) )
		{
			level.fav.ortiz doDamage( 30, p.origin, p );
			codolprintln( "CoDOL cooptest: favhitortiz, Ortiz alive " + isAlive( level.fav.ortiz ) + " health " + level.fav.ortiz.health );
		}
		break;
	case "favtrace":
		// favtrace <x0> <y0> <x1> <y1> <height>: what a straight line at
		// that height over the ground meets (the van's drive-in line).
		if ( tokens.size >= 6 )
		{
			a = ground( ( float( tokens[ 1 ] ), float( tokens[ 2 ] ), 400 ) ) + ( 0, 0, float( tokens[ 5 ] ) );
			b = ground( ( float( tokens[ 3 ] ), float( tokens[ 4 ] ), 400 ) ) + ( 0, 0, float( tokens[ 5 ] ) );
			tr = bulletTrace( a, b, false, undefined );
			hit = "nothing";
			if ( tr[ "fraction" ] < 1 )
			{
				hit = "world at " + tr[ "position" ];
				if ( isDefined( tr[ "entity" ] ) )
					hit = "ent " + tr[ "entity" ] getEntityNumber() + " " + tr[ "entity" ].classname + " at " + tr[ "position" ];
			}
			codolprintln( "CoDOL cooptest: favtrace " + a + " -> " + b + ": " + hit );
		}
		break;
	case "favtags":
		// favtags: each car's, the technical's and the van's tags in its
		// own frame (forward, right, up).
		movers = [];
		for ( m = 0; m < level.fav.cars.size; m++ )
			movers[ movers.size ] = level.fav.cars[ m ];
		if ( isDefined( level.fav.technical ) )
			movers[ movers.size ] = level.fav.technical;
		if ( isDefined( level.fav.van ) )
			movers[ movers.size ] = level.fav.van;
		for ( m = 0; m < movers.size; m++ )
		{
			c = movers[ m ];
			tags = strTok( "tag_origin tag_body tag_driver tag_passenger tag_guy0 tag_guy1 tag_wheel_front_left tag_wheel_back_right", " " );
			if ( c.model == "vehicle_pickup_technical" )
				tags = strTok( "tag_origin tag_body tag_driver tag_passenger tag_gunner tag_50cal tag_wheel_front_left tag_wheel_back_right", " " );
			if ( c.model == "vehicle_uaz_van" )
				tags = strTok( "tag_origin tag_body tag_driver tag_passenger tag_wheel_front_left tag_wheel_back_right", " " );
			for ( j = 0; j < tags.size; j++ )
			{
				o = c getTagOrigin( tags[ j ] ) - c.origin;
				codolprintln( "CoDOL cooptest: " + c.model + " " + tags[ j ] + " local ( " + vectorDot( o, anglesToForward( c.angles ) ) + ", " +
					vectorDot( o, anglesToRight( c.angles ) ) + ", " + o[ 2 ] + " )" );
			}
		}
		break;
	case "favjump":
		if ( tokens.size >= 2 )
		{
			o = ent( tokens[ 1 ] );
			p = getPlayers()[ 0 ];
			if ( isDefined( o ) && isDefined( p ) )
				p setOrigin( ground( o.origin + ( 0, 0, 16 ) ) );
		}
		break;
	}
}

fav_status()
{
	fav = level.fav;
	text = "CoDOL cooptest: fav step";
	if ( isDefined( level.coop_fav_step ) )
		text += " " + level.coop_fav_step;
	if ( isDefined( fav.ortiz ) )
		text += " ortiz " + fav.ortiz.origin;
	if ( isDefined( fav.chase_node ) )
		text += " node " + fav.chase_node;
	if ( isDefined( fav.objective_point ) )
		text += " objective " + fav.objective_point.targetname;
	text += " alive " + level.coop_alive + " spawned " + level.coop_spawned + " killed " + level.coop_killed;
	codolprintln( text );
}

fav_route()
{
	level endon( "coop_mission_over" );
	codolprintln( "CoDOL cooptest: favroute started" );
	while ( !level.fav.opening_done )
		wait( 0.5 );
	p = getPlayers()[ 0 ];
	p endon( "disconnect" );
	level thread fav_route_fire( p );
	last = undefined;
	for ( ;; )
	{
		wait( 0.5 );
		o = level.fav.objective_point;
		if ( !isDefined( o ) || !isAlive( p ) )
			continue;
		if ( isDefined( last ) && last == o )
		{
			if ( o.targetname == "obj_100" )
			{
				if ( isDefined( level.fav.ortiz ) )
					maps\_cooptest::walk_to( p, level.fav.ortiz.origin + ( 160, -160, 0 ), "near Ortiz" );
			}
			continue;
		}
		last = o;
		maps\_cooptest::walk_to( p, o.origin, "objective " + o.targetname );
		t = undefined;
		trig_flags = strTok( "chase_05_leave chase_11_leave chase_15_leave chase_20_leave chase_25_leave chase_30_leave chase_40_leave chase_41_leave obj_45_arrive obj_47_arrive obj_50_complete chase_60_leave chase_70_leave chase_72_leave obj_75_arrive chase_80_leave chase_90_leave ending_tackle", " " );
		objs = strTok( "obj_10 obj_11 obj_15 obj_20 obj_25 obj_30 obj_40 obj_41 obj_45 obj_47 obj_50 obj_60 obj_70 obj_72 obj_75 obj_80 obj_90 obj_100", " " );
		for ( i = 0; i < objs.size; i++ )
		{
			if ( objs[ i ] == o.targetname )
				t = flag_trigger( trig_flags[ i ] );
		}
		if ( isDefined( t ) && !flag( t.script_flag ) )
			maps\_cooptest::walk_to( p, t.origin, "trigger " + t.script_flag );
	}
}

// The probe player shoots the nearest live enemy he can see.
fav_route_fire( p )
{
	level endon( "coop_mission_over" );
	p endon( "disconnect" );
	for ( ;; )
	{
		wait( 0.6 );
		if ( !isAlive( p ) )
			continue;
		best = undefined;
		bestDist = 1500;
		for ( i = 0; i < level.coop_enemies.size; i++ )
		{
			ai = level.coop_enemies[ i ];
			if ( !isDefined( ai ) || !isAlive( ai ) )
				continue;
			d = distance( ai.origin, p.origin );
			if ( d >= bestDist )
				continue;
			if ( !bulletTracePassed( p getEye(), ai.origin + ( 0, 0, 48 ), false, ai ) )
				continue;
			best = ai;
			bestDist = d;
		}
		if ( !isDefined( best ) )
			continue;
		p setPlayerAngles( vectorToAngles( best.origin + ( 0, 0, 44 ) - p getEye() ) );
		p codol_botPressButtons( 1, 0.15 );
	}
}
