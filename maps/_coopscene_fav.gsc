// coop_take_mp_fav_tropical: the mission's scene table (IW4 favela_anim.gsc /
// favela_escape_anim.gsc, as C1 ships them for this mission).
//
// Every clip below is listed only in animlists/coop_take_mp_fav_tropical
// .animlist, so it is looked up by name (maps\_coopanim::add_anim ->
// codol_getAnim), never named with %anim: see maps\_coopanim.gsc.  A clip this
// match's client does not load is simply absent from the table and the
// mission falls back to the common-list staging it had before.
//
// Root positions are MEASURED from the zone's xanims (first and last frame of
// the delta track, relative to the scene node):
//   opening_scene_node (157.8 2239.7 282.5, yaw 0):
//     favela_van_shootout_guy1  Ortiz, 24.5 s: from (14 2593) by the van, fire
//                               notes at 0.61 .. 0.88 (the windscreen), ends
//                               running west at (-483 2101) beside the first
//                               chase node escape_path_start (-864 2072)
//     favela_van_shootout_guy2 / _guy3   gunner1 / gunner2 (IW4 animnames;
//                               C1's two informants), 10.9 s, bodyfall at 0.67
//     favela_van_shootout_guy4  driver, 20.1 s, walks in from the east, fires
//                               (scn_favela_shootoutguy4) at 0.68
//   opening_scene_node_vehicles: the van's parked place, (-1.9 3.6) yaw -171
//   me0_auto571 (1088 1216 220, the end of Ortiz's run):
//     favela_tackle_ghost / _ortiz, 17.2 s, then their _idle (6 s, not looping)
//   chase nodes' own `animation` keys: escape_path_start
//     favela_traverse_rail_ortiz (the rail vault), me0_auto318
//     favela_escape_bigjump_faust (the rooftop jump, 172 units down)
//   civilians' nodes: me0_auto567 unarmed_shout_window, me0_auto570
//     unarmed_close_garage; the hiding-door man (me0_auto615) doorpeek_*
//   combat_zone_45's two soldiers: me0_auto606 / 616 window_smash_stop_inside
//
// Coop category only; names no multiplayer module.

#using_animtree ( "generic_human" );

init_anims()
{
	if ( isDefined( level.coop_fav_anims_done ) )
		return;
	level.coop_fav_anims_done = true;

	// The meet (IW4 favela_anims: makarov / gunner1 / gunner2 / driver).
	maps\_coopanim::add_anim( "ortiz", "opening_scene", "favela_van_shootout_guy1" );
	maps\_coopanim::add_anim( "gunner1", "opening_scene", "favela_van_shootout_guy2" );
	maps\_coopanim::add_anim( "gunner2", "opening_scene", "favela_van_shootout_guy3" );
	maps\_coopanim::add_anim( "driver", "opening_scene", "favela_van_shootout_guy4" );

	// Ortiz's run (favela_escape's runner) and his orders to his men.
	maps\_coopanim::add_anim( "ortiz", "favela_traverse_rail_ortiz", "favela_traverse_rail_ortiz" );
	maps\_coopanim::add_anim( "ortiz", "favela_escape_bigjump_faust", "favela_escape_bigjump_faust" );
	maps\_coopanim::add_anim( "ortiz", "order_grunts", "favela_order_grunts_ortiz" );

	// The end: Ghost tackles Ortiz and kneels on him.
	maps\_coopanim::add_anim( "ortiz", "tackle", "favela_tackle_ortiz" );
	maps\_coopanim::add_anim( "ortiz", "tackle_idle", "favela_tackle_ortiz_idle" );
	maps\_coopanim::add_anim( "ghost", "tackle", "favela_tackle_ghost" );
	maps\_coopanim::add_anim( "ghost", "tackle_idle", "favela_tackle_ghost_idle" );

	// The technical's crew (IW4 vehicles\technical / _vehicle_aianim).  C1
	// ships no technical_driver_idle in this list; the seated driving idle it
	// does ship is the UAZ driver's.
	maps\_coopanim::add_anim( "technical", "driver_idle", "uaz_driver_idle_drive" );
	maps\_coopanim::add_anim( "technical", "passenger_idle", "technical_passenger_idle" );
	maps\_coopanim::add_anim( "technical", "gunner_idle", "technical_turret_driveidle" );
	maps\_coopanim::add_anim( "technical", "driver_out", "technical_driver_climb_out" );
	maps\_coopanim::add_anim( "technical", "passenger_out", "technical_passenger_climb_out" );

	// Civilians (favela_anim civilian_window_1, IW4 _hiding_door_anims).
	maps\_coopanim::add_anim( "generic", "unarmed_shout_window", "unarmed_shout_window" );
	maps\_coopanim::add_anim( "generic", "unarmed_close_garage", "unarmed_close_garage" );
	maps\_coopanim::add_anim( "hiding_door_guy", "open", "doorpeek_open" );
	maps\_coopanim::add_anim( "hiding_door_guy", "idle", "doorpeek_idle" );
	maps\_coopanim::add_anim( "hiding_door_guy", "close", "doorpeek_close" );

	// Soldiers' spawn moves (IW4 _spawner: a spawner's target struct with an
	// animation key).
	maps\_coopanim::add_anim( "generic", "window_smash_stop_inside", "window_smash_stop_inside" );
	maps\_coopanim::add_anim( "generic", "jump_down_56", "jump_down_56" );
}

has( animname, scene )
{
	init_anims();
	return maps\_coopanim::has_anim( animname, scene );
}

get( animname, scene )
{
	init_anims();
	return maps\_coopanim::get_anim( animname, scene );
}

// The meet is played from the scene clips only when all four ship to this
// client; otherwise the mission keeps its common-list staging.
meet_scene_available()
{
	return has( "ortiz", "opening_scene" ) && has( "gunner1", "opening_scene" ) &&
		has( "gunner2", "opening_scene" ) && has( "driver", "opening_scene" );
}

// A clip that does not loop, held as a loop: played from its start again each
// time it ends (C1 drops a non-looping clip held at its end, and the actor
// shows his bind pose).  Ends on "coop_scene_stop" or death.
hold_loop( clip )
{
	self endon( "death" );
	self endon( "coop_scene_stop" );
	self notify( "coop_fav_hold" );
	self endon( "coop_fav_hold" );
	if ( !isDefined( clip ) )
		return;
	for ( ;; )
		self maps\_coopanim::play_anim( clip, undefined, "inplace" );
}

// --------------------------------------------------------------------------
// Vehicle clips (IW4 favela_anim vehicle_anims: the van's "door_open", the
// cars' scene clips; _vehicle's driving idles).  They are in the mission
// animlist's =vehicles section, which the server's vehicles tree mirrors, so
// codol_getAnim( "vehicles", name ) answers them; a script_model's first
// setAnim binds it to that tree (codol_script_model_tree).
// --------------------------------------------------------------------------

vehicle_clip( name )
{
	a = codol_getAnim( "vehicles", name );
	if ( !isDefined( a ) )
	{
		if ( !isDefined( level.coop_fav_vehicle_missing ) )
			level.coop_fav_vehicle_missing = [];
		if ( !isDefined( level.coop_fav_vehicle_missing[ name ] ) )
		{
			level.coop_fav_vehicle_missing[ name ] = true;
			codolprintln( "CoDOL coop scene: vehicle clip " + name + " is not in this mission's animlists; skipped" );
		}
	}
	return a;
}

// The model plays the vehicle clip <name> (from <fraction> of its length).
// Returns the clip, or undefined when it does not ship to this client.
play_vehicle_clip( name, fraction, once )
{
	// +set scr_coop_fav_vehicle_anims 0 leaves the vehicles unanimated.
	if ( getDvar( "scr_coop_fav_vehicle_anims" ) == "0" )
		return undefined;
	a = vehicle_clip( name );
	if ( !isDefined( a ) )
		return undefined;
	self setAnim( a, 1, 0, 1 );
	if ( isDefined( fraction ) && fraction > 0 )
		self setAnimTime( a, fraction );
	else
		fraction = 0;
	codolprintln( "CoDOL coop scene: " + self.model + " plays " + name );
	if ( isDefined( once ) && once )
		self thread hold_last_frame( a, ( 1 - fraction ) * getAnimLength( a ) );
	return a;
}

// A clip that plays once (the van's and cars' doors) is frozen just before
// its end and held there (rate 0), so the doors stay as the scene leaves
// them.  MEASURED 2026-10-14 (probes fav6 / fav8 on 17815 / 17817): letting
// favela_van_shootout_van run to its end on the script_model was followed
// within two seconds by "SL_AddRefToString: script string ... was already
// freed (stale handle)" and then a fatal MT_GetRefByIndex; with the vehicle
// clips off (fav7) neither appeared.  A native defect in a script_model's
// end-of-clip handling, reported to the parent; this keeps the clip from
// ever ending.
hold_last_frame( a, length )
{
	self endon( "death" );
	if ( length > 0.2 )
		wait( length - 0.2 );
	if ( isDefined( self ) )
		self setAnim( a, 1, 0, 0 );
}

stop_vehicle_clip( a )
{
	if ( isDefined( a ) )
		self clearAnim( a, 0.2 );
}
