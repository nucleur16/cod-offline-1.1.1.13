// coop scripted events read from the map entities.
//
// The coop missions' level scripts do not ship (docs/CODOL_CAMPAIGN_MODE.md
// section 3.2), so every event here is inferred from an entity's own keys --
// classname, targetname, script_noteworthy, script_flag*, target links --
// and implemented with this engine's support.  Each one is listed, with its
// evidence and status, in docs/CODOL_CAMPAIGN_MODE.md section 8.
//
//   cleanup          MP game objects (script_gameobjectname) and
//                    "delete_on_load" entities are removed, as IW4 MP
//                    _gameobjects::main and IW4 SP _load do.
//   escape warning   script_noteworthy player_trying_to_escape triggers warn
//                    a player once per entry -- only on a map where they ring
//                    the area (no player start inside one); the exit flag
//                    itself is the extraction (_coopmission
//                    finish_after_elimination), not a failure, because on
//                    these maps the volumes are interior (MEASURED).
//   laptops          script_model pmc_objective + its trigger_use (IW4 _pmc
//                    mode_objective, coop_spec_mp_crash_tropical A/B/C): a
//                    compass objective each, done on use.
//   intel            trigger_use intel_collectible + its script_model (IW4
//                    _intelligence): picked up on use.
//   pickups          trigger_use c4_pickup (training): gives the C4.
//   obj markers      script_origin obj_<x> with a trigger flag obj_<x>_reach
//                    (coop_spec_mp_estate_tropical obj_house): compass
//                    objective until the flag is set.
//   vehicle groups   a trigger's script_vehiclespawngroup /
//                    script_vehiclestartmove spawns and starts the vehicles
//                    of that group along their authored paths
//                    (_coopvehicle.gsc), honouring script_flag_true.
//
// Coop module (maps/, IW4 singleplayer namespace); names no multiplayer module.

// Synchronous, from Callback_StartGameType (precache window).
init()
{
	cleanup();
	precache_vehicle_groups();
}

// Threaded once a player is in.
main()
{
	// A mission script that places its own markers and boundaries
	// (level.coop_mission_owns_markers): take estate's obj_house marker and
	// its interior "escape" volumes would otherwise nag along its route.
	if ( !isDefined( level.coop_mission_owns_markers ) )
		level thread escape_watch();
	level thread laptops();
	level thread intel_pickups();
	level thread weapon_pickups();
	if ( !isDefined( level.coop_mission_owns_markers ) )
		level thread objective_markers();
	level thread vehicle_groups();
}

// --------------------------------------------------------------------------
// cleanup
// --------------------------------------------------------------------------

cleanup()
{
	deleted = 0;
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( !isDefined( e ) || isPlayer( e ) )
			continue;
		// IW4 MP _gameobjects::main( allowed ) deletes every game object whose
		// script_gameobjectname is not an allowed gametype; coop allows none
		// (MEASURED on mp_killhouse_sh: hq crates, sab/dd bombzones, dom flags).
		if ( isDefined( e.script_gameobjectname ) && e.script_gameobjectname != "" )
		{
			e delete();
			deleted++;
			continue;
		}
		// IW4 _specialops keeps an entity tagged script_specialopsname only in
		// the Spec Ops mission it names: mp_killhouse_sh carries blockers for
		// coop_training_mp_killhouse_sh (course_end_blocker) and for a
		// coop_spec2_mp_killhouse_sh that does not ship (MEASURED).
		if ( isDefined( e.script_specialopsname ) && e.script_specialopsname != "" &&
		     e.script_specialopsname != getDvar( "codol_sessionMap" ) )
		{
			e delete();
			deleted++;
			continue;
		}
		// IW4 _specialops_code specialops_remove_flag_check: script_specialops 0
		// marks an entity that exists only outside Spec Ops (the base maps'
		// multiplayer clip walls, S&D bomb sites; estate's *190 walls off the
		// top of the beach). killhouse's minimap_corner origins carry it too,
		// and coop_minimap reads them.
		if ( isDefined( e.script_specialops ) && ( "" + e.script_specialops ) == "0" &&
		     !( isDefined( e.targetname ) && e.targetname == "minimap_corner" ) )
		{
			e delete();
			deleted++;
			continue;
		}
		// IW4 SP _load deletes "delete_on_load" entities.
		if ( isDefined( e.targetname ) && e.targetname == "delete_on_load" )
		{
			e delete();
			deleted++;
		}
	}
	codolprintln( "CoDOL coop events: cleanup removed " + deleted + " MP game-object / delete_on_load entities" );
}

// --------------------------------------------------------------------------
// escape
// --------------------------------------------------------------------------

escape_watch()
{
	level endon( "coop_mission_over" );

	warn = by_noteworthy( "player_trying_to_escape" );
	if ( warn.size <= 0 )
		return;

	// IW4's warning triggers ring the playable area.  If a player start lies
	// inside one, the volumes mean something else on this map (estate spec:
	// MEASURED, its estate_player_start is inside the west warning box) and
	// warning there would nag from the first second.
	starts = [];
	names = maps\_coopmode::spawnClassnames();
	for ( n = 0; n < names.size; n++ )
	{
		points = getEntArray( names[ n ], "classname" );
		for ( i = 0; i < points.size; i++ )
			starts[ starts.size ] = points[ i ];
	}
	// A start is an unlinked point entity, so probe with a linked
	// script_origin placed on it.
	probe = spawn( "script_origin", ( 0, 0, 0 ) );
	inside = false;
	for ( i = 0; i < starts.size && !inside; i++ )
	{
		probe.origin = starts[ i ].origin + ( 0, 0, 16 );
		wait( 0.05 );
		for ( t = 0; t < warn.size; t++ )
		{
			if ( probe isTouching( warn[ t ] ) )
			{
				inside = true;
				break;
			}
		}
	}
	probe delete();
	if ( inside )
	{
		codolprintln( "CoDOL coop events: escape warning triggers contain a player start; not a boundary here, no warnings" );
		return;
	}

	codolprintln( "CoDOL coop events: " + warn.size + " escape warning triggers" );
	for ( i = 0; i < warn.size; i++ )
		warn[ i ] thread escape_warning_trigger();
}

// getEntArray( value, "script_noteworthy" ) answers undefined on this server
// (MEASURED: only the native search keys work), so scan.
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

escape_warning_trigger()
{
	level endon( "coop_mission_over" );
	for ( ;; )
	{
		self waittill( "trigger", who );
		if ( !isDefined( who ) || !isPlayer( who ) )
			continue;
		if ( isDefined( who.coop_escape_warned ) && getTime() - who.coop_escape_warned < 4000 )
			continue;
		who.coop_escape_warned = getTime();
		who iPrintLnBold( "Return to the mission area" );
	}
}

// --------------------------------------------------------------------------
// laptops (IW4 _pmc mode_objective)
// --------------------------------------------------------------------------

laptops()
{
	objs = getEntArray( "pmc_objective", "targetname" );
	if ( objs.size <= 0 )
		return;

	level.coop_objectives_left = 0;
	slot = 1;
	for ( i = 0; i < objs.size; i++ )
	{
		o = objs[ i ];
		if ( !isDefined( o.target ) )
			continue;
		use = getEnt( o.target, "targetname" );
		if ( !isDefined( use ) )
			continue;
		level.coop_objectives_left++;
		o thread laptop_think( use, slot );
		slot++;
	}
	codolprintln( "CoDOL coop events: " + level.coop_objectives_left + " laptop objectives" );
	maps\_coopmission::announce( "Secure the intel: " + level.coop_objectives_left + " laptops" );
}

laptop_think( use, slot )
{
	objective_add( slot, "active", self.origin );
	for ( ;; )
	{
		use waittill( "trigger", who );
		if ( isDefined( who ) && isPlayer( who ) )
			break;
	}
	objective_delete( slot );
	level.coop_objectives_left--;
	who iPrintLnBold( "Intel secured" );
	codolprintln( "CoDOL coop events: laptop " + slot + " secured, " + level.coop_objectives_left + " left" );
	level notify( "coop_objective_done" );
	use delete();
}

// --------------------------------------------------------------------------
// intel pickups (IW4 _intelligence)
// --------------------------------------------------------------------------

intel_pickups()
{
	triggers = getEntArray( "intel_collectible", "targetname" );
	for ( i = 0; i < triggers.size; i++ )
		triggers[ i ] thread intel_think();
	if ( triggers.size > 0 )
		codolprintln( "CoDOL coop events: " + triggers.size + " intel pickups" );
}

intel_think()
{
	model = undefined;
	if ( isDefined( self.target ) )
		model = getEnt( self.target, "targetname" );
	for ( ;; )
	{
		self waittill( "trigger", who );
		if ( isDefined( who ) && isPlayer( who ) )
			break;
	}
	who iPrintLnBold( "Intel collected" );
	codolprintln( "CoDOL coop events: intel " + self.script_index + " collected" );
	if ( isDefined( model ) )
		model delete();
	self delete();
}

// --------------------------------------------------------------------------
// weapon pickups: a trigger_use named <weapon>_pickup (training c4_pickup,
// targeting the three C4 models on the table) gives that weapon.
// --------------------------------------------------------------------------

weapon_pickups()
{
	uses = getEntArray( "trigger_use", "classname" );
	for ( i = 0; i < uses.size; i++ )
	{
		t = uses[ i ];
		if ( !isDefined( t.targetname ) || !isSubStr( t.targetname, "_pickup" ) )
			continue;
		base = getSubStr( t.targetname, 0, t.targetname.size - 7 );
		weapon = base + "_so";
		if ( !codol_isWeaponMounted( weapon ) )
			continue;
		t thread weapon_pickup_think( weapon );
		codolprintln( "CoDOL coop events: pickup " + t.targetname + " gives " + weapon );
	}
}

weapon_pickup_think( weapon )
{
	for ( ;; )
	{
		self waittill( "trigger", who );
		if ( !isDefined( who ) || !isPlayer( who ) )
			continue;
		who maps\_coopmode::coop_give_weapon( weapon );
		who iPrintLnBold( "Picked up " + weapon );
		wait( 1 );
	}
}

// --------------------------------------------------------------------------
// obj_<x> markers completed by a obj_<x>_reach flag
// --------------------------------------------------------------------------

objective_markers()
{
	level endon( "coop_mission_over" );
	origins = getEntArray( "script_origin", "classname" );
	slot = 4;
	for ( i = 0; i < origins.size; i++ )
	{
		o = origins[ i ];
		if ( !isDefined( o.targetname ) || getSubStr( o.targetname, 0, 4 ) != "obj_" )
			continue;
		flag = o.targetname + "_reach";
		if ( !isDefined( level.coop_flag ) || !isDefined( level.coop_flag[ flag ] ) )
			continue;
		o thread objective_marker_think( slot, flag );
		codolprintln( "CoDOL coop events: objective " + o.targetname + " until flag " + flag );
		slot++;
		if ( slot > 7 )
			break;
	}
}

objective_marker_think( slot, flag )
{
	level endon( "coop_mission_over" );
	objective_add( slot, "active", self.origin );
	maps\_coopmission::flag_wait( flag );
	objective_delete( slot );
}

// --------------------------------------------------------------------------
// vehicle spawn groups
// --------------------------------------------------------------------------

vehicles_in_group( group )
{
	found = [];
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( !isDefined( e.classname ) || e.classname.size < 14 || getSubStr( e.classname, 0, 14 ) != "script_vehicle" )
			continue;
		if ( !isDefined( e.script_vehiclespawngroup ) || e.script_vehiclespawngroup != group )
			continue;
		found[ found.size ] = e;
	}
	return found;
}

precache_vehicle_groups()
{
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( !isDefined( e.classname ) || e.classname.size < 14 || getSubStr( e.classname, 0, 14 ) != "script_vehicle" )
			continue;
		if ( !isDefined( e.script_vehiclespawngroup ) )
			continue;
		model = maps\_coopvehicle::vehicle_model( e );
		if ( isDefined( model ) )
			precacheModel( model );
	}
}

vehicle_groups()
{
	triggers = getEntArray();
	count = 0;
	for ( i = 0; i < triggers.size; i++ )
	{
		t = triggers[ i ];
		if ( !isDefined( t.classname ) || getSubStr( t.classname, 0, 8 ) != "trigger_" )
			continue;
		if ( !isDefined( t.script_vehiclespawngroup ) && !isDefined( t.script_vehiclestartmove ) )
			continue;
		t thread vehicle_group_trigger();
		count++;
	}
	if ( count > 0 )
		codolprintln( "CoDOL coop events: " + count + " vehicle spawn-group triggers" );
}

vehicle_group_trigger()
{
	level endon( "coop_mission_over" );
	if ( isDefined( self.script_flag_true ) )
		maps\_coopmission::flag_wait( self.script_flag_true );

	for ( ;; )
	{
		self waittill( "trigger", who );
		if ( isDefined( who ) && isPlayer( who ) )
			break;
	}

	group = self.script_vehiclespawngroup;
	if ( !isDefined( group ) )
		group = self.script_vehiclestartmove;
	vehicles = vehicles_in_group( group );
	codolprintln( "CoDOL coop events: vehicle group " + group + " (" + vehicles.size + " vehicles) released" );
	for ( i = 0; i < vehicles.size; i++ )
		vehicles[ i ] thread vehicle_run();
}

vehicle_run()
{
	if ( isDefined( self.coop_released ) )
		return;
	self.coop_released = true;
	if ( !isDefined( self.target ) )
		return;
	path = maps\_coopvehicle::get_path( self.target );
	if ( path.size <= 0 )
		return;
	mover = maps\_coopvehicle::spawn_mover( self );
	if ( !isDefined( mover ) )
		return;
	// Ground vehicles drive on the ground under their nodes (see
	// _coopvehicle::ground_under); helicopters fly the points themselves.
	ground = maps\_coopvehicle::is_ground_path( path );
	mover maps\_coopvehicle::follow_path( path, 0, 40, ground );
	// IW4 script_vehicle_selfremove / a path ending far off the map: gone.
	mover delete();
}
