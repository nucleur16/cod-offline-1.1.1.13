// Helicopter rotors, for every helicopter CoDOL flies: the MP killstreak
// vehicles (spawnHelicopter), the coop movers and the zombies extraction
// helicopter (script_models).
//
// How IW4 turns them: maps\_vehicle build_drive loops the vehicle's rotor clip
// on the vehicles animtree while it exists (maps\_blackhawk, _cobra, _hind,
// _pavelow: %bh_rotors; _littlebird, _mi28: %mi28_rotors; _mi17:
// %mi17_heli_rotors; _seaknight: %sniper_escape_ch46_rotors).  C1 does the
// same: it has no procedural rotor spin (no rotor tag name in the client), and
// the blur materials only look right on turning blades.
//
// What C1 ships (MEASURED, docs/zone_assets and the animlists):
//   bh_rotors                  common_mp, and listed in
//                              animlists/common.animlist's =vehicles block
//                              (code_post_gfx_mp), so EVERY match's client
//                              has it.  Two bones: main_rotor_jnt,
//                              tail_rotor_jnt, which every shipped flying
//                              helicopter carries (vehicle_mi24p_hind_mp,
//                              vehicle_mi-28_mp, vehicle_pavelow,
//                              vehicle_little_bird_armed, ...).
//   mi28_rotors                the coop training, spec1 killhouse, spec
//                              crash and take estate mission animlists.
//   mi17_heli_rotors           spec crash and the prolog.
//   sniper_escape_ch46_rotors  five coop missions (the CH-46 has two main
//                              rotors and no tail rotor, so bh_rotors does
//                              not fit it).
// A model's own clip is used when the match's client loads it; otherwise
// bh_rotors.  Clips are looked up by name (codol_getAnim), never with %anim:
// a %anim the client's lists lack shifts every later index of the tree.
//
// The first setAnim binds the entity to the server vehicles tree: a
// script_model or a helicopter/vehicle entity (codol_script_model_tree.c);
// the snapshot's animation commands then play the loop on C1.

// The rotor clip IW4 builds for this model, or undefined for a model that is
// not a helicopter.
rotor_anim_name( model )
{
	if ( !isDefined( model ) || model == "" )
		return undefined;
	if ( isSubStr( model, "mi17" ) )
		return "mi17_heli_rotors";
	if ( isSubStr( model, "ch46" ) || isSubStr( model, "seaknight" ) )
		return "sniper_escape_ch46_rotors";
	if ( isSubStr( model, "mi-28" ) || isSubStr( model, "mi28" ) || isSubStr( model, "little_bird" ) || isSubStr( model, "littlebird" ) )
		return "mi28_rotors";
	if ( isSubStr( model, "blackhawk" ) || isSubStr( model, "hind" ) || isSubStr( model, "mi24" ) || isSubStr( model, "cobra" ) || isSubStr( model, "pavelow" ) || isSubStr( model, "apache" ) || isSubStr( model, "helicopter" ) )
		return "bh_rotors";
	return undefined;
}

is_helicopter_model( model )
{
	return isDefined( rotor_anim_name( model ) );
}

// The clip this match's client can play for the model: its own, else
// bh_rotors (common.animlist) when the model has main/tail rotor joints.
rotor_anim( model )
{
	name = rotor_anim_name( model );
	if ( !isDefined( name ) )
		return undefined;
	a = codol_getAnim( "vehicles", name );
	if ( !isDefined( a ) && name != "bh_rotors" && name != "sniper_escape_ch46_rotors" )
		a = codol_getAnim( "vehicles", "bh_rotors" );
	return a;
}

// self = the helicopter.  Starts its rotor loop (once); returns the clip or
// undefined when the model is no helicopter or the clip does not ship.
spin_rotors()
{
	if ( isDefined( self.codol_rotor_anim ) )
		return self.codol_rotor_anim;
	// +set scr_heli_rotors 0 leaves every helicopter's rotors still.
	if ( getDvar( "scr_heli_rotors" ) == "0" )
		return undefined;
	a = rotor_anim( self.model );
	if ( !isDefined( a ) )
	{
		if ( is_helicopter_model( self.model ) )
			codolprintln( "CoDOL heli rotors: no rotor clip for " + self.model + " in this match's vehicles tree" );
		return undefined;
	}
	self setAnim( a, 1, 0, 1 );
	self.codol_rotor_anim = a;
	codolprintln( "CoDOL heli rotors: ent " + self getEntityNumber() + " " + self.model + " spins its rotors" );
	return a;
}
