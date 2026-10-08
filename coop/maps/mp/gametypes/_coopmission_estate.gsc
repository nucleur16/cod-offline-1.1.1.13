// coop_take_mp_estate_tropical: "Safe House", chapter 1's last mission.
//
// The retail CoDOL client's own run of this mission (a public gameplay
// recording, user direction 2026-10-08) is the authority for the order of
// events, the cast and the moments; docs/CODOL_CAMPAIGN_MODE.md, "Take estate", is
// that timeline.  The mission's level script does not ship (section 3.2), so
// everything here is driven by the mission's own entities:
//
//   * the addon (maps/mp/coop_take_mp_estate_tropical.mapents): the zodiac
//     zodiacs_intro and its node path to boat_last_node, Ghost
//     (actor_ally_hero_ghost_estate_so), Vazquez (actor_enemy_vazquez_so,
//     and the breach room's vazquez_breach), the spawner groups
//     (patrol_initial, patrol_ruinhouse, guard_midhouse, reinforce_*,
//     patrol_prehouse, house_initial, spawner_cell, breach_enemy_spawner,
//     post_breach_outside, ending_attacker, final_wave*), their script_struct
//     patrol chains (script_animation smoke / phone, script_flag_wait,
//     script_flag_set), the flag triggers, the breach (breachIcon trigger,
//     breachroom volume), the checkpoints, Vazquez's run (vazquez_run_away*),
//     the sniper (sniper_shot_start, sniper_glint_org, sniper_sound), the
//     technical's node path and the three Mi-28s' fly-away structs;
//   * the base mp_estate_tropical level's coop_take entities: Ghost's colour
//     triggers (script_noteworthy ghost_color_trigger, script_color_allies
//     r0..r15) and colour nodes, the breach door (breach_solid,
//     trigger_use_breach, breach_fx origins) and the blockers
//     (stairs_blocker, post_breach_player_blocker).
//
// Stages (the recording's order):
//   0 the zodiac up the river to the beach, Ghost aboard; the title card;
//     Price's orders
//   1 stealth through the forest behind Ghost: the beach patrol, the ruin
//     house patrols, the two smokers on the road, the mid-house smokers; a
//     group alerted by an unsuppressed shot, a survivor or a player walking
//     into it opens fire and its reinforcements come
//   2 the house: a big group comes out with Vazquez; "kill Vazquez's guards";
//     Vazquez runs inside and down to his safe room
//   3 inside: the kitchen and the house guards, then the laptop in the
//     basement ("get the intel")
//   4 the breach: charge on the safe-room door, slow motion, Vazquez cowers
//   5 "lead Vazquez out"; he bolts, mercenaries arrive ("catch Vazquez"); the
//     chase east and north to the cliff over the water
//   6 Vazquez surrenders at the cliff and a sniper across the water kills
//     him; the debrief lines; mission complete (the lobby return of
//     _coopmission end_level)
//
// The voice lines are the zone's own aliases (estate_gst_*, estate_pri_*,
// estate_vaz_*), placed by matching the recording's on-screen subtitles to
// the subtitle each alias carries in the zone (a private mapping; no line text
// is reproduced here).  Their lengths are estimates from the subtitle text.
//
// Coop category root only; names no multiplayer module.

#using_animtree ( "generic_human" );

is_estate()
{
	return getDvar( "codol_sessionMap" ) == "coop_take_mp_estate_tropical";
}

// --------------------------------------------------------------------------
// Voice lines.  Ghost speaks from his actor, Price over the radio (local to
// every player), Vazquez from his actor.
// --------------------------------------------------------------------------

line_time( alias )
{
	switch ( alias )
	{
	case "estate_pri_0410": return 4.5;
	case "estate_pri_0412": return 1.0;
	case "estate_pri_0414": return 5.5;
	case "estate_pri_0461": return 2.5;
	case "estate_pri_0470": return 1.5;
	case "estate_pri_0474": return 1.5;
	case "estate_pri_0477": return 6.5;
	case "estate_pri_0478": return 2.5;
	case "estate_gst_112": return 3.0;
	case "estate_gst_113": return 2.0;
	case "estate_gst_114": return 1.0;
	case "estate_gst_115": return 2.5;
	case "estate_gst_116": return 2.5;
	case "estate_gst_117": return 1.2;
	case "estate_gst_118": return 3.5;
	case "estate_gst_120": return 3.5;
	case "estate_gst_121": return 3.5;
	case "estate_gst_122": return 3.5;
	case "estate_gst_123": return 3.0;
	case "estate_gst_126": return 3.0;
	case "estate_gst_138": return 2.0;
	case "estate_gst_140": return 0.8;
	case "estate_gst_141": return 6.0;
	case "estate_gst_146": return 2.0;
	case "estate_gst_147": return 2.0;
	case "estate_gst_148": return 1.5;
	case "estate_gst_150": return 2.0;
	case "estate_gst_151": return 1.2;
	case "estate_gst_152": return 2.5;
	case "estate_gst_153": return 2.5;
	case "estate_gst_154": return 3.0;
	case "estate_gst_158": return 5.0;
	case "estate_gst_159": return 0.8;
	case "estate_gst_160": return 4.0;
	case "estate_gst_161": return 2.5;
	case "estate_gst_162": return 1.0;
	case "estate_gst_163": return 5.0;
	case "estate_vaz_04": return 1.5;
	case "estate_vaz_06": return 2.5;
	case "estate_vaz_08": return 4.0;
	case "estate_vaz_09": return 3.5;
	}
	return 1.5;
}

ghost_says( alias, noWait )
{
	speaker = level.coop_estate.ghost;
	if ( !isDefined( speaker ) )
		speaker = level.coop_estate.boat;
	if ( isDefined( speaker ) )
		speaker snd( alias );
	codolprintln( "CoDOL coop estate: line " + alias );
	if ( !isDefined( noWait ) || !noWait )
		wait( line_time( alias ) + 0.3 );
}

price_says( alias, noWait )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] snd_local( alias );
	codolprintln( "CoDOL coop estate: line " + alias );
	if ( !isDefined( noWait ) || !noWait )
		wait( line_time( alias ) + 0.3 );
}

vazquez_says( alias )
{
	if ( isDefined( level.coop_estate.vazquez ) )
		level.coop_estate.vazquez snd( alias );
	codolprintln( "CoDOL coop estate: line " + alias );
}

// Every sound goes through these: an alias the mounted zones do not carry is
// a fatal script error (MEASURED: a name from the zone's sound index that is
// not an alias stopped the probe), so a missing one is logged and skipped.
snd( alias )
{
	if ( soundExists( alias ) )
		self playSound( alias );
	else
		codolprintln( "CoDOL coop estate: no sound alias " + alias );
}

snd_local( alias )
{
	if ( soundExists( alias ) )
		self playLocalSound( alias );
	else
		codolprintln( "CoDOL coop estate: no sound alias " + alias );
}

snd_loop( alias )
{
	if ( soundExists( alias ) )
		self playLoopSound( alias );
	else
		codolprintln( "CoDOL coop estate: no sound alias " + alias );
}

// Ghost's praise after a kill, one of the eight, at most every 8 s.
ghost_praise()
{
	if ( getTime() - level.coop_estate.praise_time < 8000 )
		return;
	level.coop_estate.praise_time = getTime();
	lines = strTok( "estate_gst_129 estate_gst_130 estate_gst_131 estate_gst_133 estate_gst_134 estate_gst_135 estate_gst_136 estate_gst_137", " " );
	ghost_says( lines[ randomInt( lines.size ) ], true );
}

// The HUD objective line (C1's own mission strings).
objective_text( text )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] iPrintLnBold( text );
}

objective_point( origin )
{
	objective_delete( 0 );
	if ( isDefined( origin ) )
		objective_add( 0, "active", origin );
}

// --------------------------------------------------------------------------
// Map entities.
// --------------------------------------------------------------------------

structs_named( name )
{
	found = [];
	if ( !isDefined( level.struct ) )
		return found;
	for ( i = 0; i < level.struct.size; i++ )
	{
		s = level.struct[ i ];
		if ( isDefined( s.targetname ) && s.targetname == name )
			found[ found.size ] = s;
	}
	return found;
}

struct_named( name )
{
	return maps\mp\gametypes\_coopvehicle::struct_by_targetname( name );
}

ent_named( name )
{
	ents = getEntArray( name, "targetname" );
	if ( ents.size > 0 )
		return ents[ 0 ];
	return undefined;
}

spawners_named( name )
{
	found = [];
	ents = getEntArray( name, "targetname" );
	for ( i = 0; i < ents.size; i++ )
	{
		if ( isDefined( ents[ i ].classname ) && getSubStr( ents[ i ].classname, 0, 12 ) == "actor_enemy_" )
			found[ found.size ] = ents[ i ];
	}
	return found;
}

spawners_noteworthy( value )
{
	found = [];
	list = maps\mp\gametypes\_cooptraining::by_noteworthy( value );
	for ( i = 0; i < list.size; i++ )
	{
		if ( isDefined( list[ i ].classname ) && getSubStr( list[ i ].classname, 0, 12 ) == "actor_enemy_" )
			found[ found.size ] = list[ i ];
	}
	return found;
}

flag_trigger( flagName )
{
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( isDefined( e.script_flag ) && e.script_flag == flagName && isDefined( e.classname ) && getSubStr( e.classname, 0, 8 ) == "trigger_" )
			return e;
	}
	return undefined;
}

flag( name )
{
	return maps\mp\gametypes\_coopmission::flag( name );
}

flag_set( name )
{
	maps\mp\gametypes\_coopmission::flag_set( name );
}

flag_wait( name )
{
	maps\mp\gametypes\_coopmission::flag_wait( name );
}

// What the flow waits for, for the log and the probe harness (_cooptest
// "estateroute" acts on it the way a player would).
step( kind, value, value2 )
{
	s = spawnStruct();
	s.kind = kind;
	s.value = value;
	s.value2 = value2;
	level.coop_estate_step = s;
	text = kind;
	if ( isDefined( value ) && ( kind == "flag" || kind == "near" || kind == "leave" ) )
		text += " " + value;
	codolprintln( "CoDOL coop estate: waiting for " + text );
}

player_near( point, radius )
{
	players = maps\mp\gametypes\_coopmission::living_players();
	for ( i = 0; i < players.size; i++ )
	{
		if ( distanceSquared( players[ i ].origin, point ) < radius * radius )
			return players[ i ];
	}
	return undefined;
}

wait_player_near( point, radius, timeout )
{
	step( "near", point, radius );
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

wait_flag( name, timeout )
{
	t = flag_trigger( name );
	if ( isDefined( t ) )
		step( "flag", name, t.origin );
	else
		step( "flag", name );
	waited = 0;
	while ( !flag( name ) )
	{
		if ( isDefined( timeout ) && waited >= timeout )
			return false;
		wait( 0.25 );
		waited += 0.25;
	}
	return true;
}

// --------------------------------------------------------------------------
// Entry points.
// --------------------------------------------------------------------------

// Synchronous from Callback_StartGameType (the precache window).
init()
{
	if ( !is_estate() )
		return;
	level.coop_mission_flow = "estate";
	// This flow releases its own spawner groups (they come when the stealth
	// breaks or a stage begins, not on their flags) and places its own
	// markers (_coopmission, _coopevents).
	level.coop_mission_owns_groups = true;
	level.coop_mission_owns_markers = true;
	// Probe verbs (_cooptest forwards what it does not know).
	level.coop_mission_testcmd = ::test_command;

	e = spawnStruct();
	e.praise_time = 0;
	e.alerted = [];
	e.checkpoint = undefined;
	e.boat_done = false;
	e.riders = [];
	e.vazquez_alive_needed = true;
	level.coop_estate = e;

	// The cast's looks (the zone's own models, docs/zone_assets): Ghost in
	// the Task Force forest kit, Vazquez in his authored white-shirted body
	// with the zone's civilian head.
	precacheModel( "body_forest_tf141_ghost" );
	precacheModel( "head_hero_ghost_forest" );
	precacheModel( "body_militia_smg_aa_wht" );
	precacheModel( "head_airport_a" );
	// The vehicles and the breach and intel props.
	precacheModel( "vehicle_zodiac" );
	precacheModel( "vehicle_mi-28_flying" );
	precacheModel( "vehicle_pickup_technical" );
	precacheModel( "mil_frame_charge" );
	precacheModel( "mil_frame_charge_obj" );
	precacheModel( "com_laptop_2_open_obj" );
	precacheModel( "tag_origin" );

	level._effect[ "estate_breach_door" ] = loadfx( "explosions/breach_door" );
	level._effect[ "estate_breach_room" ] = loadfx( "explosions/breach_room" );
	level._effect[ "estate_scope_glint" ] = loadfx( "misc/scope_glint" );
	level._effect[ "estate_sniper_blood" ] = loadfx( "impacts/sniper_escape_blood" );

	// The title card (C1's own mission strings).
	precacheString( &"COOP_TAKE_MP_ESTATE_TROPICAL_INTROSCREEN_LINE_0" );
	precacheString( &"COOP_TAKE_MP_ESTATE_TROPICAL_INTROSCREEN_LINE_1" );
	precacheString( &"COOP_TAKE_MP_ESTATE_TROPICAL_INTROSCREEN_LINE_2" );

	// The authored vehicles stand in for script_model movers (_coopvehicle);
	// the authored entities themselves are hidden.
	vehicles = getEntArray();
	for ( i = 0; i < vehicles.size; i++ )
	{
		v = vehicles[ i ];
		if ( isDefined( v.classname ) && v.classname.size > 14 && getSubStr( v.classname, 0, 14 ) == "script_vehicle" )
			v hide();
	}
	// A foliage model the coop mission removes (base map,
	// script_noteworthy plant_delete_coop_take_estate).
	plants = maps\mp\gametypes\_cooptraining::by_noteworthy( "plant_delete_coop_take_estate" );
	for ( i = 0; i < plants.size; i++ )
		plants[ i ] delete();

	level thread loadout_watch();
	level thread maps\mp\gametypes\_cooptraining::show_nametags();
	codolprintln( "CoDOL coop estate: mission flow active" );
}

main()
{
	if ( !is_estate() )
		return;
	level endon( "coop_mission_over" );

	while ( getPlayers().size <= 0 )
		wait( 0.25 );
	// _coopmission::main initialises its counters on the same frame; spawn
	// nothing before it has.
	while ( !isDefined( level.coop_alive ) || !isDefined( level.coop_enemies ) )
		wait( 0.05 );
	// The house and the chase put up to a dozen enemies in play with Ghost
	// and Vazquez; the native pool is 32 actors.
	if ( getDvarInt( "scr_coop_max_alive" ) <= 0 )
		level.coop_max_alive = 18;

	level thread unsuppressed_watch();
	level thread checkpoint_triggers();

	spawn_ghost();
	stage_boat();
	stage_stealth();
	stage_house_front();
	stage_house_inside();
	stage_intel();
	stage_breach();
	stage_chase();
	stage_ending();

	maps\mp\gametypes\_coopmission::mission_complete( "Vazquez's laptop secured" );
}

// --------------------------------------------------------------------------
// The player's weapons (the recording: an M21 EBR with scope and silencer,
// and a silenced MP5K with a red dot; offhands only from the safe room's
// shelves).
// --------------------------------------------------------------------------

estate_primary()
{
	if ( codol_isWeaponMounted( "m14_scoped_silencer_woodland_codo_so" ) )
		return "m14_scoped_silencer_woodland_codo_so";
	return undefined;
}

estate_secondary()
{
	if ( codol_isWeaponMounted( "mp5_silencer_reflex_codo_so" ) )
		return "mp5_silencer_reflex_codo_so";
	if ( codol_isWeaponMounted( "mp5_silencer_so" ) )
		return "mp5_silencer_so";
	return undefined;
}

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
	// As _cooptraining: the spawn can come before this thread waits.
	wait( 0.05 );
	if ( isAlive( self ) && self.sessionstate == "playing" )
		self spawned();
	for ( ;; )
	{
		self waittill( "spawned_player" );
		self spawned();
	}
}

spawned()
{
	primary = estate_primary();
	secondary = estate_secondary();
	if ( isDefined( primary ) && isDefined( secondary ) )
	{
		self takeAllWeapons();
		self giveWeapon( primary );
		self giveMaxAmmo( primary );
		self giveWeapon( secondary );
		self giveMaxAmmo( secondary );
		self setOffhandEquipment( "none", "none", "none" );
		self setSpawnWeapon( primary );
		self switchToWeapon( primary );
	}
	codolprintln( "CoDOL coop estate: " + self.name + " armed " + primary + " / " + secondary );

	if ( !level.coop_estate.boat_done && isDefined( level.coop_estate.boat ) )
	{
		self thread board_boat();
		return;
	}
	// A respawn comes back at the stage's checkpoint, as the recording's
	// retry resumes at the last checkpoint.
	if ( isDefined( level.coop_estate.checkpoint ) )
	{
		self setOrigin( level.coop_estate.checkpoint + ( randomFloatRange( -24, 24 ), randomFloatRange( -24, 24 ), 4 ) );
		codolprintln( "CoDOL coop estate: " + self.name + " back at checkpoint " + level.coop_estate.checkpoint );
	}
}

// The authored checkpoints: trigger checkpoint_midhouse, the breach
// structs and the cliff structs; each stage moves the checkpoint on.
checkpoint_triggers()
{
	level endon( "coop_mission_over" );
	t = ent_named( "checkpoint_midhouse" );
	if ( !isDefined( t ) )
		return;
	for ( ;; )
	{
		t waittill( "trigger", who );
		if ( isDefined( who ) && isPlayer( who ) )
			break;
	}
	set_checkpoint( t.origin, "checkpoint_midhouse" );
}

set_checkpoint( origin, why )
{
	ground = maps\mp\gametypes\_coopvehicle::ground_under( origin + ( 0, 0, 8 ) );
	level.coop_estate.checkpoint = ground;
	codolprintln( "CoDOL coop estate: checkpoint " + why + " at " + ground );
}

// --------------------------------------------------------------------------
// Ghost: a friendly soldier of the coop species who leads the way, kills his
// half of every pair, and fights once the shooting starts.  Nobody can hurt
// him (IW4 heroes carry magic_bullet_shield).
// --------------------------------------------------------------------------

spawn_ghost()
{
	spawner = ent_named( "ghost" );
	template = maps\mp\gametypes\_cooptraining::ally_template();
	if ( !isDefined( spawner ) || !isDefined( template ) )
	{
		codolprintln( "CoDOL coop estate: no Ghost" );
		return;
	}
	kind = spawnStruct();
	kind.faction = "cast";
	kind.role = "smg";
	kind.team = "allies";
	kind.health = 1000;
	kind.accuracy = 0.6;
	kind.body = "body_forest_tf141_ghost";
	kind.head = "head_hero_ghost_forest";
	level.coop_soldier_next = kind;
	ai = template spawnActor();
	level.coop_soldier_next = undefined;
	if ( !isDefined( ai ) )
	{
		codolprintln( "CoDOL coop estate: Ghost refused by the actor pool" );
		return;
	}
	ai forceTeleport( spawner.origin, spawner.angles );
	ai show();
	ai setCanDamage( false );
	ai setAnimKnobAll( %exposed_aim_5, %body, 1, 0.1, 1 );
	ai.goalradius = 64;
	ai.ignoreall = true;
	ai.coop_name = spawner.script_friendname;
	if ( !isDefined( ai.coop_name ) )
		ai.coop_name = "Ghost";
	// His silenced SMG (a coop window slot, see aitype role_weapons).
	if ( codol_isWeaponMounted( "mp5_silencer_so" ) )
		ai.coop_weapon = "mp5_silencer_so";
	ai maps\mp\gametypes\_coopmission::arm_soldier();
	level.coop_estate.ghost = ai;
	if ( !isDefined( level.coop_cast_list ) )
		level.coop_cast_list = [];
	level.coop_cast_list[ level.coop_cast_list.size ] = ai;
	codolprintln( "CoDOL coop estate: Ghost (" + ai.coop_name + ") at " + ai.origin );
	level thread ghost_colors();
	level thread ghost_catch_up();
}

// IW4 colour movement, from the base map's coop_take colour triggers: a
// player in a ghost_color_trigger sends Ghost to the node of that colour.
// The nodes are not entities on this server, so their origins are listed
// (MEASURED, mp_estate_tropical entity string: script_color_allies r0..r15,
// the info_volume colours r4 / r14 by their target node).
color_node( c )
{
	switch ( c )
	{
	case "r0": return ( -560, 2112, -48 );
	case "r1": return ( 1217, 1374, 161 );
	case "r2": return ( -4772, 2356, -426 );
	case "r3": return ( -3842, 1878, -300 );
	case "r4": return ( -3512, 1660, -288 );
	case "r5": return ( 1756, 3850, 44 );
	case "r6": return ( 1512, 2472, 168 );
	case "r7": return ( -2248, 1212, -246 );
	case "r8": return ( 1108, 1896, 160 );
	case "r9": return ( 136, 664, 224 );
	case "r10": return ( 968, 1032, 93 );
	case "r11": return ( 550, 380, 220 );
	case "r12": return ( 644, 644, 62 );
	case "r13": return ( -4336, 3380, -240 );
	case "r14": return ( -36, 2464, 42 );
	case "r15": return ( 360, 1408, 220 );
	}
	return undefined;
}

ghost_colors()
{
	level endon( "coop_mission_over" );
	triggers = maps\mp\gametypes\_cooptraining::by_noteworthy( "ghost_color_trigger" );
	for ( i = 0; i < triggers.size; i++ )
	{
		if ( isDefined( triggers[ i ].script_color_allies ) )
			triggers[ i ] thread ghost_color_trigger();
	}
	codolprintln( "CoDOL coop estate: " + triggers.size + " ghost colour triggers" );
}

ghost_color_trigger()
{
	level endon( "coop_mission_over" );
	for ( ;; )
	{
		self waittill( "trigger", who );
		if ( !isDefined( who ) || !isPlayer( who ) )
			continue;
		ghost_to( color_node( self.script_color_allies ), self.script_color_allies );
		wait( 2 );
	}
}

ghost_to( point, why )
{
	g = level.coop_estate.ghost;
	if ( !isDefined( g ) || !isDefined( point ) || isDefined( level.coop_estate.ghost_riding ) )
		return;
	if ( isDefined( level.coop_estate.ghost_goal_why ) && level.coop_estate.ghost_goal_why == why )
		return;
	level.coop_estate.ghost_goal = point;
	level.coop_estate.ghost_goal_why = why;
	g setGoalPos( point );
	codolprintln( "CoDOL coop estate: Ghost to " + why + " " + point );
}

// The coop actors path over the bot waypoint graph and can wedge on the
// estate's clip; a Ghost left far behind, out of every player's sight, is
// put at his goal (or near the lead player).
ghost_catch_up()
{
	level endon( "coop_mission_over" );
	for ( ;; )
	{
		wait( 3 );
		g = level.coop_estate.ghost;
		if ( !isDefined( g ) || isDefined( level.coop_estate.ghost_riding ) )
			continue;
		players = maps\mp\gametypes\_coopmission::living_players();
		if ( players.size <= 0 )
			continue;
		// Only when he is far from every player; then toward the player
		// furthest along (nearest his goal), not one left at the beach.
		lead = undefined;
		d = undefined;
		for ( i = 0; i < players.size; i++ )
		{
			pd = distance( g.origin, players[ i ].origin );
			if ( !isDefined( d ) || pd < d )
				d = pd;
			if ( !isDefined( lead ) || ( isDefined( level.coop_estate.ghost_goal ) &&
			     distance( players[ i ].origin, level.coop_estate.ghost_goal ) < distance( lead.origin, level.coop_estate.ghost_goal ) ) )
				lead = players[ i ];
		}
		if ( d < 1100 )
			continue;
		seen = false;
		for ( i = 0; i < players.size; i++ )
		{
			if ( bulletTracePassed( players[ i ] getEye(), g.origin + ( 0, 0, 48 ), false, g ) )
				seen = true;
		}
		if ( seen )
			continue;
		dest = level.coop_estate.ghost_goal;
		if ( !isDefined( dest ) || distance( dest, lead.origin ) > 900 )
			dest = lead.origin - anglesToForward( lead.angles ) * 160;
		dest = maps\mp\gametypes\_coopvehicle::ground_under( dest + ( 0, 0, 32 ) );
		g forceTeleport( dest, g.angles );
		if ( isDefined( level.coop_estate.ghost_goal ) )
			g setGoalPos( level.coop_estate.ghost_goal );
		codolprintln( "CoDOL coop estate: Ghost caught up to " + dest + " (was " + int( d ) + " away)" );
	}
}

ghost_fights( fight )
{
	g = level.coop_estate.ghost;
	if ( !isDefined( g ) )
		return;
	g.ignoreall = !fight;
}

// Ghost takes his man: his silenced shot at a stealth soldier (his own
// weapon's fire, codol_actorShoot), then the kill (one round may not finish
// him).
ghost_shoots( target )
{
	g = level.coop_estate.ghost;
	if ( !isDefined( g ) || !isDefined( target ) || !isAlive( target ) )
		return;
	weapon = "mp5_silencer_so";
	if ( isDefined( g.coop_weapon ) && g.coop_weapon != "" )
		weapon = g.coop_weapon;
	start = g.origin + ( 0, 0, 56 );
	head = target.origin + ( 0, 0, 62 );
	start = start + vectorNormalize( head - start ) * 16;
	if ( isDefined( g.coop_weapon_model ) )
		start = g getTagOrigin( "tag_flash" );
	if ( codol_isWeaponMounted( weapon ) )
		g maps\mp\gametypes\_coopmission::actor_fire_round( weapon, start, head );
	if ( isAlive( target ) )
		target doDamage( target.health + 200, start );
	codolprintln( "CoDOL coop estate: Ghost takes ent " + target getEntityNumber() );
}

// --------------------------------------------------------------------------
// Enemies.
// --------------------------------------------------------------------------

// One soldier per spawner, through the shared coop spawn (_coopmission
// spawn_soldier: faction and role from the classname, the native budget, the
// kill bookkeeping).  stealth: the soldier holds fire and never hunts until
// his group is alerted.
spawn_group( spawners, stealth, group )
{
	ais = [];
	for ( i = 0; i < spawners.size; i++ )
	{
		s = spawners[ i ];
		if ( isDefined( s.coop_estate_spawned ) )
			continue;
		s.coop_estate_spawned = true;
		if ( stealth )
			s.coop_no_hunt = true;
		ai = undefined;
		for ( tries = 0; tries < 20 && !isDefined( ai ); tries++ )
		{
			ai = maps\mp\gametypes\_coopmission::spawn_soldier( s );
			if ( !isDefined( ai ) )
				wait( 0.5 );
		}
		if ( !isDefined( ai ) )
		{
			codolprintln( "CoDOL coop estate: spawner at " + s.origin + " refused" );
			continue;
		}
		ai.coop_group = group;
		if ( stealth )
		{
			ai.ignoreall = true;
			ai.coop_stealth = true;
			ai thread stealth_soldier( group );
		}
		ais[ ais.size ] = ai;
		wait( 0.05 );
	}
	text = "";
	if ( stealth )
		text = " (stealth)";
	codolprintln( "CoDOL coop estate: group " + group + " spawned " + ais.size + " of " + spawners.size + text );
	return ais;
}

alive_of( ais )
{
	alive = [];
	for ( i = 0; i < ais.size; i++ )
	{
		if ( isDefined( ais[ i ] ) && isAlive( ais[ i ] ) )
			alive[ alive.size ] = ais[ i ];
	}
	return alive;
}

wait_group_dead( ais, timeout, what )
{
	step( "kill", ais, what );
	waited = 0;
	while ( alive_of( ais ).size > 0 )
	{
		if ( isDefined( timeout ) && waited >= timeout )
			return false;
		wait( 0.5 );
		waited += 0.5;
	}
	return true;
}

// Wait until at most `left` of the list are alive.
wait_group_down_to( ais, left, timeout, what )
{
	step( "kill", ais, what );
	waited = 0;
	while ( alive_of( ais ).size > left )
	{
		if ( isDefined( timeout ) && waited >= timeout )
			return false;
		wait( 0.5 );
		waited += 0.5;
	}
	return true;
}

// IW4 _patrol over the soldier's script_struct chain: walk each point, wait
// on its script_flag_wait, set its script_flag_set, and stand at the last
// one with its script_animation (smoke / phone, played from the common
// animlist's patrol idles).
stealth_soldier( group )
{
	self endon( "death" );
	self endon( "coop_alerted" );
	self thread stealth_damage_watch( group );
	self thread stealth_sight_watch( group );
	spawner = self.coop_spawner;
	if ( !isDefined( spawner ) || !isDefined( spawner.target ) )
	{
		self patrol_idle( undefined );
		return;
	}
	path = maps\mp\gametypes\_coopvehicle::get_path( spawner.target );
	self.goalradius = 24;
	for ( n = 0; n < path.size; n++ )
	{
		p = path[ n ];
		if ( isDefined( p.script_flag_wait ) )
		{
			// The flags set by the unshipped level script are capped.
			waited = 0;
			while ( !flag( p.script_flag_wait ) && waited < 600 )
			{
				wait( 0.5 );
				waited += 0.5;
			}
		}
		self setGoalPos( p.origin );
		for ( t = 0; t < 40; t += 0.5 )
		{
			if ( distance( ( self.origin[ 0 ], self.origin[ 1 ], 0 ), ( p.origin[ 0 ], p.origin[ 1 ], 0 ) ) < 40 )
				break;
			wait( 0.5 );
		}
		if ( isDefined( p.script_flag_set ) )
			flag_set( p.script_flag_set );
		if ( isDefined( p.script_delay_post ) )
			wait( float( p.script_delay_post ) );
		if ( n == path.size - 1 || isDefined( p.script_animation ) && !isDefined( p.target ) )
		{
			self patrol_idle( p.script_animation );
			return;
		}
	}
}

patrol_idle( animation )
{
	idle = %patrol_bored_idle;
	if ( isDefined( animation ) )
	{
		if ( animation == "smoke" )
			idle = %patrol_bored_idle_smoke;
		else if ( animation == "phone" )
			idle = %patrol_bored_idle_cellphone;
	}
	self.coop_cast = "patrol";
	self.coop_idle_anim = idle;
	self notify( "coop_cast_pose" );
	self.coop_estate_idle = animation;
}

// A stealth soldier hit and alive, or a squadmate killed near him with the
// shooter's partner not following up within a second and a half, alerts
// the group (IW4 _stealth: a corpse seen, a bullet felt).
stealth_damage_watch( group )
{
	self endon( "coop_alerted" );
	self waittill( "damage", amount, attacker );
	if ( isDefined( attacker ) && isPlayer( attacker ) )
	{
		level.coop_estate.last_stealth_shot = getTime();
		level notify( "coop_estate_stealth_shot", self, group );
	}
	// A second round within the moment (a follow-up shot, Ghost's) still
	// counts as a clean kill.
	for ( t = 0; t < 0.8 && isAlive( self ); t += 0.05 )
		wait( 0.05 );
	if ( isAlive( self ) )
	{
		alert_group( group, "a survivor" );
		return;
	}
	// Dead: anyone of the group who saw it alerts in 2.5 s unless he dies too (time for the shooter to take the third man).
	level thread corpse_alert( group, self.origin );
}

corpse_alert( group, origin )
{
	wait( 2.5 );
	ais = level.coop_enemies;
	for ( i = 0; i < ais.size; i++ )
	{
		ai = ais[ i ];
		if ( !isDefined( ai ) || !isAlive( ai ) || !isDefined( ai.coop_group ) || ai.coop_group != group )
			continue;
		if ( distance( ai.origin, origin ) < 900 && bulletTracePassed( ai.origin + ( 0, 0, 60 ), origin + ( 0, 0, 30 ), false, ai ) )
		{
			alert_group( group, "a body seen" );
			return;
		}
	}
}

// A player walking right up to a stealth soldier in the open is seen.
stealth_sight_watch( group )
{
	self endon( "death" );
	self endon( "coop_alerted" );
	for ( ;; )
	{
		wait( 0.5 );
		players = maps\mp\gametypes\_coopmission::living_players();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			d = distance( p.origin, self.origin );
			limit = 240;
			if ( p getStance() == "crouch" )
				limit = 160;
			else if ( p getStance() == "prone" )
				limit = 96;
			if ( d < limit && bulletTracePassed( self.origin + ( 0, 0, 60 ), p getEye(), false, self ) )
			{
				alert_group( group, "a player at " + int( d ) );
				return;
			}
		}
	}
}

alert_group( group, why )
{
	if ( isDefined( level.coop_estate.alerted[ group ] ) )
		return;
	level.coop_estate.alerted[ group ] = true;
	codolprintln( "CoDOL coop estate: group " + group + " alerted (" + why + ")" );
	ais = level.coop_enemies;
	n = 0;
	for ( i = 0; i < ais.size; i++ )
	{
		ai = ais[ i ];
		if ( !isDefined( ai ) || !isAlive( ai ) || !isDefined( ai.coop_group ) || ai.coop_group != group )
			continue;
		ai notify( "coop_alerted" );
		ai.ignoreall = false;
		ai.coop_stealth = undefined;
		ai.coop_cast = undefined;
		ai.coop_hunting = true;
		ai notify( "coop_cast_pose" );
		players = maps\mp\gametypes\_coopmission::living_players();
		if ( players.size > 0 )
			ai setGoalPos( players[ randomInt( players.size ) ].origin );
		n++;
	}
	if ( n > 0 )
	{
		ghost_fights( true );
		if ( getTime() - level.coop_estate.praise_time > 4000 )
			ghost_says( "estate_gst_126", true );
		level notify( "coop_estate_alert", group );
	}
}

// A loud weapon in the forest gives the player away (IW4 so_forest
// contingency: unsuppressed fire alerts everyone in earshot).
unsuppressed_watch()
{
	level endon( "coop_mission_over" );
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread unsuppressed_player();
	}
}

unsuppressed_player()
{
	self endon( "disconnect" );
	warned = false;
	for ( ;; )
	{
		self waittill( "weapon_fired", weapon );
		if ( !isDefined( weapon ) )
			weapon = self getCurrentWeapon();
		if ( isSubStr( weapon, "silenc" ) || isSubStr( weapon, "_sd_" ) || isSubStr( weapon, "_sd" ) )
			continue;
		if ( !isDefined( level.coop_estate.in_stealth ) )
			continue;
		ais = level.coop_enemies;
		for ( i = 0; i < ais.size; i++ )
		{
			ai = ais[ i ];
			if ( isDefined( ai ) && isAlive( ai ) && isDefined( ai.coop_stealth ) && distance( ai.origin, self.origin ) < 2000 )
				alert_group( ai.coop_group, "unsuppressed " + weapon );
		}
		if ( !warned )
		{
			warned = true;
			ghost_says( "estate_gst_123", true );
		}
	}
}

// Ghost's half of a pair: when a player opens on a stealth group, Ghost kills
// another of it.
ghost_partner_watch( group, count )
{
	level endon( "coop_mission_over" );
	for ( k = 0; k < count; k++ )
	{
		for ( ;; )
		{
			level waittill( "coop_estate_stealth_shot", victim, g );
			if ( g == group )
				break;
		}
		wait( 0.3 );
		ais = level.coop_enemies;
		best = undefined;
		for ( i = 0; i < ais.size; i++ )
		{
			ai = ais[ i ];
			if ( !isDefined( ai ) || !isAlive( ai ) || ai == victim || !isDefined( ai.coop_group ) || ai.coop_group != group )
				continue;
			if ( !isDefined( best ) || distance( ai.origin, level.coop_estate.ghost.origin ) < distance( best.origin, level.coop_estate.ghost.origin ) )
				best = ai;
		}
		if ( isDefined( best ) )
			ghost_shoots( best );
	}
}

// Ghost praises each stealth kill.
praise_watch()
{
	level endon( "coop_mission_over" );
	level endon( "coop_estate_stealth_over" );
	for ( ;; )
	{
		level waittill( "coop_enemy_died" );
		wait( 0.6 );
		ghost_praise();
	}
}

// --------------------------------------------------------------------------
// Stage 0: the zodiac.
// --------------------------------------------------------------------------

boat_seat( index )
{
	seats = [];
	seats[ 0 ] = ( 20, 18, 22 );
	seats[ 1 ] = ( 20, -18, 22 );
	seats[ 2 ] = ( -14, 18, 22 );
	seats[ 3 ] = ( -14, -18, 22 );
	return seats[ index % seats.size ];
}

local_to_world( origin, angles, offset )
{
	return origin + anglesToForward( angles ) * offset[ 0 ] + anglesToRight( angles ) * offset[ 1 ] + ( 0, 0, offset[ 2 ] );
}

board_boat()
{
	self endon( "disconnect" );
	boat = level.coop_estate.boat;
	if ( !isDefined( boat ) || level.coop_estate.boat_done )
		return;
	seat = level.coop_estate.riders.size;
	level.coop_estate.riders[ seat ] = self;
	self disableWeapons();
	self setStance( "crouch" );
	self maps\mp\gametypes\_coopintro::seat_lock( true );
	self setOrigin( local_to_world( boat.origin, boat.angles, boat_seat( seat ) ) );
	self setPlayerAngles( boat.angles );
	self.coop_seat_angles = boat.angles;
	self playerLinkToDelta( boat, "tag_origin", 1 );
	self.coop_in_intro = true;
	codolprintln( "CoDOL coop estate: " + self.name + " aboard the zodiac, seat " + seat );
}

boat_riders_looking()
{
	riders = level.coop_estate.riders;
	for ( i = 0; i < riders.size; i++ )
	{
		p = riders[ i ];
		if ( !isDefined( p ) || !isDefined( p.coop_seat_angles ) )
			continue;
		a = p getPlayerAngles();
		dy = a[ 1 ] - p.coop_seat_angles[ 1 ];
		while ( dy > 180 )
			dy -= 360;
		while ( dy < -180 )
			dy += 360;
		if ( dy > 2 || dy < -2 )
			return true;
	}
	return false;
}

stage_boat()
{
	vehicle = ent_named( "zodiacs_intro" );
	if ( !isDefined( vehicle ) || !isDefined( vehicle.target ) )
	{
		level.coop_estate.boat_done = true;
		return;
	}
	path = maps\mp\gametypes\_coopvehicle::get_path( vehicle.target );
	boat = maps\mp\gametypes\_coopvehicle::spawn_mover( vehicle );
	if ( !isDefined( boat ) || path.size <= 0 )
	{
		level.coop_estate.boat_done = true;
		return;
	}
	level.coop_estate.boat = boat;
	path = on_water( path );
	boat.origin = water_point( boat.origin );
	codolprintln( "CoDOL coop estate: zodiac at " + boat.origin + ", " + path.size + " nodes to " + path[ path.size - 1 ].origin );

	// Players already in the world board now; later ones board on spawn.
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isAlive( players[ i ] ) && !isDefined( players[ i ].coop_in_intro ) )
			players[ i ] thread board_boat();
	}

	// Ghost at the back of the boat, riding it (codol_actorRide).
	g = level.coop_estate.ghost;
	if ( isDefined( g ) )
	{
		g forceTeleport( local_to_world( boat.origin, boat.angles, ( -48, 0, 22 ) ), boat.angles );
		wait( 0.1 );
		g codol_actorRide( boat );
		level.coop_estate.ghost_riding = true;
	}

	waited = 0;
	while ( level.coop_estate.riders.size <= 0 && waited < 20 )
	{
		wait( 0.25 );
		waited += 0.25;
	}
	limit = getDvarFloat( "scr_coop_intro_wait" );
	if ( limit <= 0 )
		limit = 8;
	waited = 0;
	while ( waited < limit && !boat_riders_looking() )
	{
		wait( 0.25 );
		waited += 0.25;
	}

	codolprintln( "CoDOL coop estate: zodiac ride starts with " + level.coop_estate.riders.size + " rider(s)" );
	level thread title_card();
	level thread boat_lines();
	level thread spawn_forest();
	boat snd_loop( "zodiac_player_sustain" );
	boat snd( "scn_boat_intro_wave" );
	boat maps\mp\gametypes\_coopvehicle::follow_path( path, 0, 25, false );
	boat stopLoopSound();
	boat snd( "zodiac_player_shutdown" );
	wait( 0.3 );

	// Out into the shallows beside the boat, weapons up.  The beached boat
	// stays, but walks no one into its hull (MEASURED: it stopped the test
	// client stepping off).
	level.coop_estate.boat_done = true;
	boat notSolid();
	riders = level.coop_estate.riders;
	for ( i = 0; i < riders.size; i++ )
	{
		p = riders[ i ];
		if ( !isDefined( p ) || !isAlive( p ) )
			continue;
		p unlink();
		p maps\mp\gametypes\_coopintro::seat_lock( false );
		side = 1;
		if ( i % 2 )
			side = -1;
		spot = boat.origin + anglesToRight( boat.angles ) * ( 64 * side ) + anglesToForward( boat.angles ) * ( 40 - 30 * int( i / 2 ) );
		spot = maps\mp\gametypes\_coopvehicle::ground_under( spot + ( 0, 0, 64 ) );
		p setOrigin( spot + ( 0, 0, 2 ) );
		p setStance( "stand" );
		p enableWeapons();
		p.coop_in_intro = undefined;
	}
	if ( isDefined( g ) )
	{
		g codol_actorRide();
		level.coop_estate.ghost_riding = undefined;
		spot = maps\mp\gametypes\_coopvehicle::ground_under( boat.origin - anglesToForward( boat.angles ) * 60 + ( 0, 0, 64 ) );
		g forceTeleport( spot, boat.angles );
		ghost_to( color_node( "r2" ), "r2 (landing)" );
	}
	set_checkpoint( path[ path.size - 1 ].origin, "the beach" );
	codolprintln( "CoDOL coop estate: zodiac ride over at " + boat.origin );
	level notify( "coop_estate_landed" );
}

// The zodiac rides the river's surface: its authored nodes lie 15..48 units
// under it (MEASURED, _cooptest trace: water at -512 over boat_last_node's
// -527, -552 over -568, -580 over -629).
water_point( point )
{
	tr = bulletTrace( point + ( 0, 0, 160 ), point - ( 0, 0, 160 ), false, undefined );
	if ( tr[ "fraction" ] < 1 && isDefined( tr[ "surfacetype" ] ) && tr[ "surfacetype" ] == "water" )
		return ( point[ 0 ], point[ 1 ], tr[ "position" ][ 2 ] - 6 );
	return point;
}

on_water( path )
{
	out = [];
	for ( i = 0; i < path.size; i++ )
	{
		n = spawnStruct();
		n.origin = water_point( path[ i ].origin );
		n.speed = path[ i ].speed;
		n.script_flag_set = path[ i ].script_flag_set;
		out[ i ] = n;
	}
	return out;
}

boat_lines()
{
	level endon( "coop_mission_over" );
	wait( 0.5 );
	price_says( "estate_pri_0410" );
	ghost_says( "estate_gst_141" );
}

// The title card: the mission's name, the date and time, the place (C1's
// own INTROSCREEN strings), lower left, for six seconds.
title_card()
{
	lines = [];
	lines[ 0 ] = &"COOP_TAKE_MP_ESTATE_TROPICAL_INTROSCREEN_LINE_0";
	lines[ 1 ] = &"COOP_TAKE_MP_ESTATE_TROPICAL_INTROSCREEN_LINE_1";
	lines[ 2 ] = &"COOP_TAKE_MP_ESTATE_TROPICAL_INTROSCREEN_LINE_2";
	huds = [];
	for ( i = 0; i < lines.size; i++ )
	{
		h = newHudElem();
		h.x = 40;
		h.y = 300 + i * 18;
		h.alignX = "left";
		h.alignY = "middle";
		h.horzAlign = "left";
		h.vertAlign = "top";
		h.fontScale = 1.4;
		h.alpha = 1;
		h setText( lines[ i ] );
		huds[ i ] = h;
		wait( 0.6 );
	}
	codolprintln( "CoDOL coop estate: title card shown" );
	wait( 5 );
	for ( i = 0; i < huds.size; i++ )
	{
		huds[ i ] fadeOverTime( 1 );
		huds[ i ].alpha = 0;
	}
	wait( 1.1 );
	for ( i = 0; i < huds.size; i++ )
		huds[ i ] destroy();
}

// --------------------------------------------------------------------------
// Stage 1: stealth through the forest.
// --------------------------------------------------------------------------

stage_stealth()
{
	level.coop_estate.in_stealth = true;
	objective_text( &"COOP_TAKE_MP_ESTATE_TROPICAL_STAGE_HINT_STEALTH" );
	house = ent_named( "obj_house" );
	if ( isDefined( house ) )
		objective_point( house.origin );
	// Ghost's "follow" marker.
	if ( isDefined( level.coop_estate.ghost ) )
	{
		objective_add( 1, "active", level.coop_estate.ghost.origin );
		objective_onEntity( 1, level.coop_estate.ghost );
	}
	level thread praise_watch();
	level thread stealth_reinforcements();

	while ( !isDefined( level.coop_estate.forest_spawned ) )
		wait( 0.25 );
	beach = level.coop_estate.beach;
	ruin1 = level.coop_estate.ruin1;
	ruin2 = level.coop_estate.ruin2;
	level thread ruin1_done_flag( ruin1 );
	level thread ghost_partner_watch( "beach", 1 );
	level thread ghost_partner_watch( "ruin1", 1 );
	level thread ghost_partner_watch( "ruin2", 2 );

	wait( 2 );
	ghost_says( "estate_gst_112" );
	wait( 3 );
	ghost_says( "estate_gst_113" );
	wait_group_dead( beach, 120, "the beach patrol" );
	wait( 1 );
	ghost_says( "estate_gst_114" );
	ghost_to( color_node( "r3" ), "r3 (up the hill)" );

	// Up the hill to the ruin house.
	wait_player_near( ( -3842, 1878, -300 ), 700, 90 );
	ghost_says( "estate_gst_115" );
	flag_set( "patrol_start_ruinhouse" );
	wait( 3 );
	ghost_says( "estate_gst_116" );
	level thread smoke_line_when_idle( ruin1, "estate_gst_121" );
	level thread smoke_line_when_idle( ruin2, "estate_gst_122" );
	wait_group_dead( ruin1, 150, "the ruin house pair" );
	wait_group_dead( ruin2, 150, "the ruin house trio" );
	wait( 1 );
	ghost_says( "estate_gst_117" );
	ghost_to( color_node( "r7" ), "r7 (mid house)" );

	// The mid-house smokers (guard_midhouse, released by their own flag
	// trigger as the player passes the ruin house, or now).
	if ( !flag( "guard_midhouse" ) )
		flag_set( "guard_midhouse" );
	mid = spawn_group( spawners_named( "guard_midhouse" ), true, "midhouse" );
	level thread ghost_partner_watch( "midhouse", 2 );
	wait_player_near( ( -1072, 1992, -169 ), 1400, 120 );
	if ( alive_of( mid ).size >= 3 )
		ghost_says( "estate_gst_120" );
	else
		ghost_says( "estate_gst_118" );
	wait_group_dead( mid, 150, "the mid house smokers" );
	wait( 0.5 );
	ghost_says( "estate_gst_140" );
	ghost_says( "estate_gst_138" );
	ghost_to( color_node( "r14" ), "r14 (road forward)" );
	level.coop_estate.in_stealth = undefined;
	level notify( "coop_estate_stealth_over" );
}

ruin1_done_flag( ais )
{
	level endon( "coop_mission_over" );
	wait( 1 );
	while ( alive_of( ais ).size > 0 )
		wait( 0.5 );
	flag_set( "ruinhouse1_enemies_done" );
}

// The forest's patrols, placed while the zodiac comes up the river: the beach
// patrol of three walking to their smoke and phone spots, and the ruin house
// patrols, who wait on patrol_start_ruinhouse at their first point (their
// own struct chains), the second trio on ruinhouse1_enemies_done.
spawn_forest()
{
	level.coop_estate.beach = spawn_group( spawners_named( "patrol_initial" ), true, "beach" );
	level.coop_estate.ruin1 = spawn_group( spawners_noteworthy( "ruinhouse1_encounter_enemy" ), true, "ruin1" );
	level.coop_estate.ruin2 = spawn_group( spawners_noteworthy( "leave_ruinhouse_patrol" ), true, "ruin2" );
	level.coop_estate.forest_spawned = true;
}

// "Two (three) stopped on the road for a smoke": when a patrol reaches its
// smoking spot.
smoke_line_when_idle( ais, alias )
{
	level endon( "coop_mission_over" );
	for ( t = 0; t < 120; t += 0.5 )
	{
		alive = alive_of( ais );
		if ( alive.size <= 0 )
			return;
		idle = 0;
		for ( i = 0; i < alive.size; i++ )
		{
			if ( isDefined( alive[ i ].coop_estate_idle ) )
				idle++;
		}
		if ( idle >= alive.size )
		{
			ghost_says( alias, true );
			return;
		}
		wait( 0.5 );
	}
}

// An alerted group brings its area's reinforcements (the addon's
// reinforce_ruinhouse and reinforce_midhouse groups, spawned at once only
// when the shooting starts).
stealth_reinforcements()
{
	level endon( "coop_mission_over" );
	level endon( "coop_estate_stealth_over" );
	for ( ;; )
	{
		level waittill( "coop_estate_alert", group );
		switch ( group )
		{
		case "ruin1":
		case "ruin2":
			if ( !isDefined( level.coop_estate.reinforced_ruin ) )
			{
				level.coop_estate.reinforced_ruin = true;
				level thread spawn_group( first_n( spawners_named( "reinforce_ruinhouse" ), 5 ), false, "reinforce_ruinhouse" );
			}
			break;
		case "midhouse":
			if ( !isDefined( level.coop_estate.reinforced_mid ) )
			{
				level.coop_estate.reinforced_mid = true;
				level thread spawn_group( first_n( spawners_named( "reinforce_midhouse" ), 6 ), false, "reinforce_midhouse" );
			}
			break;
		}
	}
}

first_n( list, n )
{
	out = [];
	for ( i = 0; i < list.size && i < n; i++ )
		out[ out.size ] = list[ i ];
	return out;
}

// --------------------------------------------------------------------------
// Stage 2: the house front.
// --------------------------------------------------------------------------

stage_house_front()
{
	wait_player_near( ( -449, 1872, 26 ), 700, 150 );
	ghost_says( "estate_gst_146" );
	ghost_says( "estate_gst_150" );
	ghost_to( color_node( "r0" ), "r0 (road)" );

	// The group coming out of the house, with Vazquez among them (the
	// addon's patrol_prehouse: six men walking out and two snipers on the
	// balcony; Vazquez's authored spawner is inside, so he walks out with
	// them from the front door).
	wait_player_near( ( 49, 1881, 95 ), 900, 120 );
	flag_set( "patrol_prehouse" );
	front = spawn_group( spawners_named( "patrol_prehouse" ), true, "prehouse" );
	spawn_vazquez_front();
	wait( 2 );
	ghost_says( "estate_gst_147" );
	ghost_says( "estate_gst_148" );
	ghost_says( "estate_gst_151", true );
	wait( 4 );
	// Loud from here on.
	alert_group( "prehouse", "the house front" );
	set_checkpoint( ( -36, 2464, 42 ), "the road to the house" );
	objective_text( &"COOP_TAKE_MP_ESTATE_TROPICAL_OBJ_KILL_GUARDS" );
	if ( isDefined( level.coop_estate.vazquez ) )
	{
		objective_add( 2, "active", level.coop_estate.vazquez.origin );
		objective_onEntity( 2, level.coop_estate.vazquez );
	}
	vazquez_says( "estate_vaz_04" );
	wait( 1.5 );
	ghost_says( "estate_gst_152" );
	// Vazquez runs back inside, for the stairs to his safe room.
	level thread vazquez_runs_inside();
	ghost_says( "estate_gst_153" );
	vazquez_says( "estate_vaz_06" );
	wait_group_down_to( front, 1, 120, "Vazquez's guards in front of the house" );
}

spawn_vazquez( spawner, origin, angles )
{
	template = maps\mp\gametypes\_cooptraining::ally_template();
	if ( !isDefined( template ) )
		return undefined;
	kind = spawnStruct();
	kind.faction = "cast";
	kind.role = "pistol";
	kind.team = "allies";
	kind.health = 1000;
	kind.accuracy = 0;
	kind.body = "body_militia_smg_aa_wht";
	kind.head = "head_airport_a";
	level.coop_soldier_next = kind;
	ai = template spawnActor();
	level.coop_soldier_next = undefined;
	if ( !isDefined( ai ) )
	{
		codolprintln( "CoDOL coop estate: Vazquez refused by the actor pool" );
		return undefined;
	}
	ai forceTeleport( origin, angles );
	ai show();
	// Nobody fights him and nobody but the sniper may kill him: the mission
	// needs him alive until the cliff.
	ai setCanDamage( false );
	ai setDontTarget( true );
	ai.ignoreall = true;
	ai.goalradius = 32;
	ai.coop_cast = "vazquez";
	ai.coop_idle_anim = %exposed_idle_reacta;
	ai setAnimKnobAll( ai.coop_idle_anim, %body, 1, 0.1, 1 );
	ai notify( "coop_cast_pose" );
	level.coop_estate.vazquez = ai;
	codolprintln( "CoDOL coop estate: Vazquez at " + ai.origin );
	return ai;
}

spawn_vazquez_front()
{
	spawner = ent_named( "vazquez" );
	if ( !isDefined( spawner ) )
		return;
	spawn_vazquez( spawner, ( 320, 1180, 200 ), ( 0, 250, 0 ) );
}

vazquez_runs_inside()
{
	v = level.coop_estate.vazquez;
	if ( !isDefined( v ) )
		return;
	v endon( "death" );
	v.coop_cast = undefined;
	v.coop_hunting = true;
	v notify( "coop_cast_pose" );
	run = struct_named( "vazquez_run_away" );
	if ( !isDefined( run ) )
		return;
	v setGoalPos( run.origin );
	for ( t = 0; t < 20; t += 0.5 )
	{
		if ( distance( v.origin, run.origin ) < 64 )
			break;
		wait( 0.5 );
	}
	// The authored struct: script_noteworthy delete_enemy, script_flag_set
	// vazquez_out_of_sight.  He reappears in the safe room.
	flag_set( "vazquez_out_of_sight" );
	objective_delete( 2 );
	v delete();
	level.coop_estate.vazquez = undefined;
	codolprintln( "CoDOL coop estate: Vazquez out of sight" );
}

// --------------------------------------------------------------------------
// Stage 3: the house.
// --------------------------------------------------------------------------

stage_house_inside()
{
	// His last known way: through the house to the basement stairs.
	o1 = struct_named( "obj_vazquez_1" );
	o2 = struct_named( "obj_vazquez_2" );
	if ( isDefined( o1 ) )
		objective_point( o1.origin );
	ghost_to( color_node( "r15" ), "r15 (outside the house)" );
	wait_flag( "in_house", 120 );
	inside = [];
	group = spawn_group( spawners_named( "house_initial" ), false, "house" );
	for ( i = 0; i < group.size; i++ )
		inside[ inside.size ] = group[ i ];
	group = spawn_group( first_n( spawners_named( "reinforce_house" ), 5 ), false, "house" );
	for ( i = 0; i < group.size; i++ )
		inside[ inside.size ] = group[ i ];
	ghost_to( color_node( "r9" ), "r9 (room one)" );
	if ( isDefined( o2 ) )
		objective_point( o2.origin );
	wait_group_down_to( inside, 0, 150, "the house guards" );
	set_checkpoint( ( 774, 438, 194 ), "checkpoint_breach_player" );
}

// --------------------------------------------------------------------------
// Stage 3b: the laptop in the basement.
// --------------------------------------------------------------------------

laptop_spot()
{
	// The basement's game room, by the bar counter beside the safe room door
	// (the recording's laptop; the exact spot is INFERRED from the room's
	// layout: no laptop entity is authored).
	if ( getDvar( "scr_coop_estate_laptop" ) != "" )
	{
		t = strTok( getDvar( "scr_coop_estate_laptop" ), " " );
		if ( t.size >= 3 )
			return ( float( t[ 0 ] ), float( t[ 1 ] ), float( t[ 2 ] ) );
	}
	// MEASURED (_cooptest trace): a ceramic counter top at z 80 over the
	// basement floor (z 38), x 420..520 y 680..780, the bar of the game room
	// on the way to the safe room door.
	return ( 440, 730, 81 );
}

stage_intel()
{
	// The basement stairs open (base map stairs_blocker) and the two cell
	// guards stand below.
	blockers = getEntArray( "stairs_blocker", "targetname" );
	for ( i = 0; i < blockers.size; i++ )
	{
		blockers[ i ] notSolid();
		blockers[ i ] hide();
	}
	cell = spawn_group( spawners_named( "spawner_cell" ), false, "cell" );
	ghost_to( color_node( "r12" ), "r12 (basement)" );

	objective_text( &"COOP_TAKE_MP_ESTATE_TROPICAL_OBJ_INTEL" );
	spot = laptop_spot();
	laptop = spawn( "script_model", spot );
	laptop setModel( "com_laptop_2_open_obj" );
	laptop.angles = ( 0, 163, 0 );
	objective_point( spot );
	level.coop_estate.laptop = laptop;
	codolprintln( "CoDOL coop estate: laptop at " + spot );

	ghost_says( "estate_gst_154", true );
	step( "use", laptop, "the laptop" );
	nag = 0;
	nags = strTok( "estate_gst_155 estate_gst_156 estate_gst_157", " " );
	hinted = [];
	for ( t = 0; ; t += 0.1 )
	{
		taker = undefined;
		players = maps\mp\gametypes\_coopmission::living_players();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( distance( p.origin, spot ) > 90 )
				continue;
			if ( !isDefined( hinted[ p getEntityNumber() ] ) )
			{
				hinted[ p getEntityNumber() ] = true;
				p iPrintLn( &"COOP_TAKE_MP_ESTATE_TROPICAL_HINT_INTEL" );
			}
			if ( p useButtonPressed() )
				taker = p;
		}
		if ( isDefined( level.coop_estate.laptop_taken_by ) )
			taker = level.coop_estate.laptop_taken_by;
		if ( isDefined( taker ) )
			break;
		if ( t >= 20 * ( nag + 1 ) && nag < nags.size )
		{
			ghost_says( nags[ nag ], true );
			nag++;
		}
		wait( 0.1 );
	}
	laptop delete();
	flag_set( "keyboard_used" );
	codolprintln( "CoDOL coop estate: laptop taken by " + taker.name );
	wait_group_dead( cell, 30, "the cell guards" );
}

// --------------------------------------------------------------------------
// Stage 4: the breach.
// --------------------------------------------------------------------------

stage_breach()
{
	objective_text( &"COOP_TAKE_MP_ESTATE_TROPICAL_STAGE_HINT_BREACH" );
	fxOrigin = ( 691.6, 799.8, 109.8 );
	o = ent_named( "obj_house_breach" );
	if ( isDefined( o ) )
		objective_point( o.origin );
	price_says( "estate_pri_0461", true );
	cp = structs_named( "checkpoint_breach_ghost" );
	if ( cp.size > 0 )
		ghost_to( cp[ 0 ].origin, "checkpoint_breach_ghost" );

	// The glowing charge frame on the door (mil_frame_charge_obj), at the
	// base map's breach_fx origin on the door.
	door = ent_named( "breach_solid" );
	angles = ( 0, 163.7, 0 );
	charge = spawn( "script_model", fxOrigin );
	charge setModel( "mil_frame_charge_obj" );
	charge.angles = angles;

	icon = undefined;
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		if ( isDefined( ents[ i ].classname ) && ents[ i ].classname == "trigger_multiple_breachIcon" )
			icon = ents[ i ];
	}
	step( "use", charge, "the breach" );
	breacher = undefined;
	while ( !isDefined( breacher ) )
	{
		players = maps\mp\gametypes\_coopmission::living_players();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			inside = distance( p.origin, fxOrigin ) < 110;
			if ( isDefined( icon ) && p isTouching( icon ) )
				inside = true;
			if ( inside && p useButtonPressed() )
				breacher = p;
		}
		if ( isDefined( level.coop_estate.breach_by ) )
			breacher = level.coop_estate.breach_by;
		wait( 0.05 );
	}
	codolprintln( "CoDOL coop estate: breach by " + breacher.name );
	charge setModel( "mil_frame_charge" );
	charge snd( "scn_estate_breach_door_impact" );
	wait( 1.5 );

	// The door goes.
	playFx( level._effect[ "estate_breach_door" ], fxOrigin );
	playFx( level._effect[ "estate_breach_room" ], ( 724, 920, 60 ) );
	charge delete();
	if ( isDefined( door ) )
	{
		door notSolid();
		door hide();
	}
	use = getEntArray( "trigger_use_breach", "classname" );
	for ( i = 0; i < use.size; i++ )
		use[ i ] delete();

	// The room: four guards reacting and Vazquez cowering (the addon's
	// breach_enemy_spawner and vazquez_breach).
	setSlowMotion( 1.0, 0.3, 0.4 );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] snd_local( "slomo_whoosh" );
	visionSetNaked( "slomo_breach", 0.4 );
	room = spawn_group( spawners_named( "breach_enemy_spawner" ), false, "breach" );
	hostage = maps\mp\gametypes\_cooptraining::by_noteworthy( "vazquez_breach" );
	if ( hostage.size > 0 )
	{
		v = spawn_vazquez( hostage[ 0 ], hostage[ 0 ].origin, hostage[ 0 ].angles );
		if ( isDefined( v ) )
		{
			v.coop_idle_anim = %covercrouch_hide_idle;
			v notify( "coop_cast_pose" );
		}
	}
	ghost_fights( true );
	wait_group_dead( room, 4, "the breach room" );
	setSlowMotion( 0.3, 1.0, 0.5 );
	visionSetNaked( vision_name(), 0.5 );
	wait_group_dead( room, 60, "the breach room" );
	flag_set( "room_1_cleared" );
	set_checkpoint( ( 672, 752, 72 ), "post_breach_scene_node" );
}

vision_name()
{
	if ( isDefined( level.coop_vision ) )
		return level.coop_vision;
	return "mp_estate_tropical";
}

// --------------------------------------------------------------------------
// Stage 5: lead him out; he runs; the chase.
// --------------------------------------------------------------------------

stage_chase()
{
	v = level.coop_estate.vazquez;
	objective_text( &"COOP_TAKE_MP_ESTATE_TROPICAL_OBJ_LEAD" );
	if ( isDefined( v ) )
	{
		objective_add( 2, "active", v.origin );
		objective_onEntity( 2, v );
	}
	evac = ent_named( "obj_evac_hostage" );
	if ( isDefined( evac ) )
		objective_point( evac.origin );
	wait( 1 );
	ghost_says( "estate_gst_158" );
	vazquez_says( "estate_vaz_08" );
	// The shelves' weapons are the player's to take (the native world
	// pickups); he bolts when a player heads out or after 25 s.
	room = ( 724, 920, 60 );
	step( "leave", room, 260 );
	for ( t = 0; t < 25; t += 0.5 )
	{
		players = maps\mp\gametypes\_coopmission::living_players();
		gone = false;
		for ( i = 0; i < players.size; i++ )
		{
			if ( distance( players[ i ].origin, room ) > 260 )
				gone = true;
		}
		if ( gone )
			break;
		wait( 0.5 );
	}

	ghost_says( "estate_gst_159", true );
	objective_text( &"COOP_TAKE_MP_ESTATE_TROPICAL_OBJ_CAPTURE" );
	objective_point( undefined );
	blockers = getEntArray( "post_breach_player_blocker", "targetname" );
	for ( i = 0; i < blockers.size; i++ )
	{
		blockers[ i ] notSolid();
		blockers[ i ] hide();
	}
	level thread vazquez_flees();
	wait( 1 );
	price_says( "estate_pri_0470" );
	ghost_says( "estate_gst_160" );

	// The mercenaries (the addon's shadow company ending_attacker sets, by
	// where the chase reaches them) and the militia outside the house.
	level thread spawn_group( spawners_named( "post_breach_outside" ), false, "outside" );
	level thread chase_wave( ( 1400, 2120, 185 ), 900, "ending_east" );
	level thread chase_wave( ( 880, 1550, 175 ), 700, "ending_garden" );
	level thread chase_wave( ( 850, 3100, 170 ), 1100, "ending_north" );
	level thread final_wave();
	ghost_to( color_node( "r10" ), "r10 (walk out)" );

	// To the cliff.
	cliff = struct_named( "vazquez_run_away_cliff" );
	cliffPos = ( 1708, 4102, 35 );
	if ( isDefined( cliff ) )
		cliffPos = cliff.origin;
	wait_flag( "vazquez_run_away_to_cliff_trigger", 240 );
	flag_set( "vazquez_run_away_to_cliff_trigger" );
	level.coop_estate.to_cliff = true;
	level notify( "coop_estate_vazquez_to_cliff" );
	ghost_says( "estate_gst_162" );
	ghost_says( "estate_gst_161" );
	set_checkpoint( ( 1418, 3080, 180 ), "vazquez_run_away_first" );
	wait_player_near( cliffPos, 420, 240 );
}

// Ending attackers: the addon's ending_attacker spawners near `centre`,
// released when a player comes within `radius` of it.
chase_wave( centre, radius, group )
{
	level endon( "coop_mission_over" );
	list = [];
	all = spawners_named( "ending_attacker" );
	for ( i = 0; i < all.size; i++ )
	{
		if ( distance( all[ i ].origin, centre ) < 450 )
			list[ list.size ] = all[ i ];
	}
	if ( list.size <= 0 )
		return;
	while ( !isDefined( player_near( centre, radius ) ) )
		wait( 0.5 );
	if ( isDefined( level.coop_estate.vazquez_dead ) )
		return;
	spawn_group( first_n( list, 5 ), false, group );
}

// final_enemy_wave: the technical drives in down its node path with its
// riders, who get out at its end, and the final_wave group joins.
final_wave()
{
	level endon( "coop_mission_over" );
	flag_wait( "final_enemy_wave" );
	if ( isDefined( level.coop_estate.vazquez_dead ) )
		return;
	codolprintln( "CoDOL coop estate: the final wave" );
	level thread spawn_group( spawners_named( "final_wave" ), false, "final_wave" );
	tech = undefined;
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		if ( isDefined( ents[ i ].classname ) && ents[ i ].classname == "script_vehicle_pickup_technical" )
			tech = ents[ i ];
	}
	if ( !isDefined( tech ) || !isDefined( tech.target ) )
		return;
	path = maps\mp\gametypes\_coopvehicle::get_path( tech.target );
	mover = maps\mp\gametypes\_coopvehicle::spawn_mover( tech );
	if ( !isDefined( mover ) || path.size <= 0 )
		return;
	mover.origin = maps\mp\gametypes\_coopvehicle::ground_under( mover.origin + ( 0, 0, 32 ) );
	mover snd_loop( "veh_pickuptruck_engine_high" );
	mover maps\mp\gametypes\_coopvehicle::follow_path( path, 0, 25, true );
	mover stopLoopSound();
	mover snd_loop( "veh_pickuptruck_idle_low" );
	riders = spawners_noteworthy( "final_wave_enemies" );
	drop = [];
	for ( i = 0; i < riders.size; i++ )
	{
		if ( !isDefined( riders[ i ].script_vehicleride ) )
			continue;
		s = spawnStruct();
		s.classname = riders[ i ].classname;
		s.origin = maps\mp\gametypes\_coopvehicle::ground_under( mover.origin + anglesToRight( mover.angles ) * ( 60 - 60 * i ) + ( 0, 0, 48 ) );
		s.angles = mover.angles;
		s.targetname = "final_wave_enemies";
		drop[ drop.size ] = s;
	}
	spawn_group( drop, false, "technical" );
}

vazquez_flees()
{
	v = level.coop_estate.vazquez;
	if ( !isDefined( v ) )
		return;
	v endon( "death" );
	v.coop_cast = undefined;
	v.coop_hunting = true;
	v notify( "coop_cast_pose" );
	first = struct_named( "vazquez_run_away_first" );
	cliff = struct_named( "vazquez_run_away_cliff" );
	firstPos = ( 1418, 3080, 180 );
	if ( isDefined( first ) )
		firstPos = first.origin;
	cliffPos = ( 1708, 4102, 35 );
	if ( isDefined( cliff ) )
		cliffPos = cliff.origin;
	// Out of the safe room's east door and north through the garden, by
	// Ghost's walk-out colour nodes (r10, r1, r8), to his authored first
	// stop; then on to the cliff by r5.
	route = [];
	route[ 0 ] = color_node( "r10" );
	route[ 1 ] = color_node( "r1" );
	route[ 2 ] = color_node( "r8" );
	route[ 3 ] = firstPos;
	codolprintln( "CoDOL coop estate: Vazquez runs" );
	v vazquez_route( route );
	if ( !isDefined( level.coop_estate.to_cliff ) )
		level waittill( "coop_estate_vazquez_to_cliff" );
	codolprintln( "CoDOL coop estate: Vazquez for the cliff from " + v.origin );
	route = [];
	route[ 0 ] = color_node( "r5" );
	route[ 1 ] = cliffPos;
	v vazquez_route( route );
	// Stand at the water's edge with his back to it.
	v.coop_cast = "vazquez";
	v.coop_idle_anim = %exposed_idle_reacta;
	v notify( "coop_cast_pose" );
	codolprintln( "CoDOL coop estate: Vazquez at the cliff " + v.origin );
}

// Run point to point.  The coop actors path over the MP bot graph, which
// does not reach the basement and wedges on the estate's clip: a Vazquez
// with no progress for 4 s out of every player's sight (or 12 s in sight) is
// put on the point he is running to.
vazquez_route( points )
{
	self endon( "death" );
	for ( n = 0; n < points.size; n++ )
	{
		goal = points[ n ];
		self setGoalPos( goal );
		best = flat_distance( self.origin, goal );
		still = 0;
		for ( t = 0; flat_distance( self.origin, goal ) > 72; t += 0.5 )
		{
			wait( 0.5 );
			// Progress is getting closer, not moving about.
			d = flat_distance( self.origin, goal );
			if ( d < best - 24 )
			{
				best = d;
				still = 0;
			}
			else
				still += 0.5;
			if ( still < 4 )
				continue;
			seen = false;
			players = maps\mp\gametypes\_coopmission::living_players();
			for ( i = 0; i < players.size; i++ )
			{
				if ( bulletTracePassed( players[ i ] getEye(), self.origin + ( 0, 0, 48 ), false, self ) )
					seen = true;
			}
			if ( seen && still < 12 )
				continue;
			self forceTeleport( maps\mp\gametypes\_coopvehicle::ground_under( goal + ( 0, 0, 16 ) ), self.angles );
			codolprintln( "CoDOL coop estate: Vazquez put on " + goal + " (no progress for " + still + " s)" );
			break;
		}
		codolprintln( "CoDOL coop estate: Vazquez reached " + goal + " at " + self.origin );
	}
}

flat_distance( a, b )
{
	return distance( ( a[ 0 ], a[ 1 ], 0 ), ( b[ 0 ], b[ 1 ], 0 ) );
}

// --------------------------------------------------------------------------
// Stage 6: the surrender, the sniper, the debrief.
// --------------------------------------------------------------------------

stage_ending()
{
	v = level.coop_estate.vazquez;
	cliff = struct_named( "vazquez_run_away_cliff" );
	if ( isDefined( v ) && isDefined( cliff ) && distance( v.origin, cliff.origin ) > 200 )
	{
		// He is not where the scene is: put him there (out of the actor's
		// own pathing).
		v forceTeleport( cliff.origin, ( 0, 200, 0 ) );
	}
	vazquez_says( "estate_vaz_09" );
	wait( 2.5 );

	// The shot from across the water (sniper_shot_start, the glint at
	// sniper_glint_org, the sound at sniper_sound).
	glint = maps\mp\gametypes\_cooptraining::by_noteworthy( "sniper_glint_org" );
	from = ( 836, 7036, 468 );
	s = struct_named( "sniper_shot_start" );
	if ( isDefined( s ) )
		from = s.origin;
	if ( glint.size > 0 )
		playFx( level._effect[ "estate_scope_glint" ], glint[ 0 ].origin );
	snd = struct_named( "sniper_sound" );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] snd_local( "scn_estate_vazqueztakeout" );
	wait( 0.4 );
	level.coop_estate.vazquez_dead = true;
	if ( isDefined( v ) && isAlive( v ) )
	{
		head = v.origin + ( 0, 0, 64 );
		if ( codol_isWeaponMounted( "cheytac_silencer_so" ) )
			magicBullet( "cheytac_silencer_so", from, head );
		playFx( level._effect[ "estate_sniper_blood" ], head );
		v setCanDamage( true );
		v.coop_cast = undefined;
		v doDamage( v.health + 500, from );
	}
	flag_set( "vazquez_dead" );
	objective_delete( 2 );
	objective_point( undefined );
	codolprintln( "CoDOL coop estate: Vazquez shot by the sniper" );
	level thread helicopters_leave();
	calm_enemies();

	wait( 2 );
	price_says( "estate_pri_0474" );
	ghost_says( "estate_gst_163" );
	price_says( "estate_pri_0477" );
	wait( 2 );
}

// The survivors stand down once the scene is over.
calm_enemies()
{
	ais = level.coop_enemies;
	for ( i = 0; i < ais.size; i++ )
	{
		if ( isDefined( ais[ i ] ) && isAlive( ais[ i ] ) )
			ais[ i ].ignoreall = true;
	}
	ghost_fights( false );
}

// The three Mi-28s (shadow_company_heli) leave from their first struct
// (script_flag_wait vazquez_dead) to the far end of their path.
helicopters_leave()
{
	helis = getEntArray( "shadow_company_heli", "targetname" );
	for ( i = 0; i < helis.size; i++ )
		helis[ i ] thread heli_leave();
}

heli_leave()
{
	if ( !isDefined( self.target ) )
		return;
	path = maps\mp\gametypes\_coopvehicle::get_path( self.target );
	if ( path.size <= 0 )
		return;
	mover = maps\mp\gametypes\_coopvehicle::spawn_mover( self );
	if ( !isDefined( mover ) )
		return;
	mover.origin = path[ 0 ].origin;
	mover snd_loop( "havoc_engine_high" );
	if ( isDefined( path[ 0 ].script_delay_post ) )
		wait( float( path[ 0 ].script_delay_post ) );
	mover maps\mp\gametypes\_coopvehicle::follow_path( path, 0, 120, false );
	mover delete();
}

// --------------------------------------------------------------------------
// Probe verbs (_cooptest, +set scr_coop_test 1; inert otherwise).
//
//   estateroute            the mission as player 0 plays it: the flow
//                          publishes what it waits for in level.coop_estate_step
//                          and this does it -- walk to a point or a flag
//                          trigger (_cooptest walk_to: botMoveTo, STUCK walks
//                          teleport on), shoot a group dead (aim with
//                          botLookAt, fire with C1 button 1; an enemy still up
//                          after six rounds is killed and logged SIMULATED),
//                          press use at the laptop and the breach (button 8;
//                          SIMULATED when it does not take), walk out of the
//                          safe room
//   trace <x> <y> <z> ...  what a bullet straight down from 40 above each
//                          point meets (floor, water, a counter top)
// --------------------------------------------------------------------------

test_command( tokens )
{
	switch ( tokens[ 0 ] )
	{
	case "estateroute":
		level thread test_route();
		break;
	case "trace":
		for ( j = 1; j + 2 < tokens.size; j += 3 )
		{
			pt = ( float( tokens[ j ] ), float( tokens[ j + 1 ] ), float( tokens[ j + 2 ] ) );
			tr = bulletTrace( pt + ( 0, 0, 40 ), pt - ( 0, 0, 600 ), false, undefined );
			codolprintln( "CoDOL cooptest: trace " + pt + " hits " + tr[ "position" ] + " surface " + tr[ "surfacetype" ] + " fraction " + tr[ "fraction" ] );
		}
		break;
	}
}

test_route()
{
	level endon( "coop_mission_over" );
	codolprintln( "CoDOL cooptest: estateroute started" );
	last = undefined;
	for ( ;; )
	{
		wait( 0.5 );
		s = level.coop_estate_step;
		p = maps\mp\gametypes\_cooptest::player( 0 );
		// A respawned player 0 does the current step again.
		if ( isDefined( p ) && !isAlive( p ) )
			last = undefined;
		if ( !isDefined( s ) || !isDefined( p ) || !isAlive( p ) || isDefined( p.coop_in_intro ) )
			continue;
		if ( isDefined( last ) && last == s && s.kind != "kill" )
			continue;
		last = s;
		switch ( s.kind )
		{
		case "near":
			// Into the radius the flow waits on, not onto the point (the
			// mid-house smokers stand on theirs).
			dest = s.value;
			if ( isDefined( s.value2 ) && distance( p.origin, s.value ) > s.value2 )
			{
				dir = vectorNormalize( ( p.origin[ 0 ] - s.value[ 0 ], p.origin[ 1 ] - s.value[ 1 ], 0 ) );
				dest = s.value + ( dir[ 0 ] * s.value2 * 0.6, dir[ 1 ] * s.value2 * 0.6, 0 );
			}
			maps\mp\gametypes\_cooptest::walk_to( p, dest, "near estate point" );
			break;
		case "flag":
			if ( isDefined( s.value2 ) )
				maps\mp\gametypes\_cooptest::walk_to( p, s.value2, "flag " + s.value );
			else
				maps\mp\gametypes\_cooptest::goto_flag( 0, s.value );
			break;
		case "kill":
			test_shoot_group( p, s.value, s.value2 );
			break;
		case "use":
			maps\mp\gametypes\_cooptest::walk_to( p, s.value.origin + ( 0, -36, 0 ), "use " + s.value2 );
			p botLookAt( s.value.origin, 1 );
			p setPlayerAngles( vectorToAngles( s.value.origin - p getEye() ) );
			wait( 0.4 );
			p codol_botPressButtons( 8, 0.5 );
			wait( 1.5 );
			if ( level.coop_estate_step == s )
			{
				if ( s.value2 == "the laptop" )
					level.coop_estate.laptop_taken_by = p;
				else
					level.coop_estate.breach_by = p;
				codolprintln( "CoDOL cooptest: estateroute use on " + s.value2 + " did not take; SIMULATED" );
			}
			break;
		case "leave":
			maps\mp\gametypes\_cooptest::walk_to( p, s.value + ( -100, -360, 0 ), "out of the safe room" );
			break;
		}
	}
}

test_shoot_group( p, ais, what )
{
	for ( i = 0; i < ais.size; i++ )
	{
		ai = ais[ i ];
		if ( !isDefined( ai ) || !isAlive( ai ) )
			continue;
		// Close to rifle range first, then up to six aimed rounds.
		if ( distance( p.origin, ai.origin ) > 900 )
		{
			dir = vectorNormalize( p.origin - ai.origin );
			maps\mp\gametypes\_cooptest::walk_to( p, ai.origin + ( dir[ 0 ] * 500, dir[ 1 ] * 500, 0 ), "towards " + what );
		}
		for ( shot = 0; shot < 6 && isDefined( ai ) && isAlive( ai ); shot++ )
		{
			head = ai.origin + ( 0, 0, 58 );
			p botLookAt( head, 0.3 );
			p setPlayerAngles( vectorToAngles( head - p getEye() ) );
			wait( 0.3 );
			p codol_botPressButtons( 1, 0.1 );
			wait( 0.6 );
		}
		if ( isDefined( ai ) && isAlive( ai ) )
		{
			ai doDamage( ai.health + 100, ai.origin, p );
			codolprintln( "CoDOL cooptest: estateroute " + what + ": ent " + ai getEntityNumber() + " killed SIMULATED" );
		}
		else
			codolprintln( "CoDOL cooptest: estateroute " + what + ": shot dead for real" );
	}
}
