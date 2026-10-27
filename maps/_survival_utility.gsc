// survival helpers: players, the soldier spawn, the end of the match.
//
// The coop mission layer's spawn contract (_coopmission.gsc spawn_soldier /
// template_for / arm_soldier) in the survival category, which may not compile
// a coop module: a soldier of an aitype's classname comes from an unlimited
// runtime spawner of that classname (spawnActorSpawner, count -1), so
// spawnactor runs aitype/<classname without actor_>::main() -- IW5's
// spawner maps\_utility::spawn_ai.  The engine loaded and precached every
// survival aitype at level load from the AI type table
// (codol_actor_scripts.cpp).  Also the match end: the mission report and the
// lobby return (_coopmission's end_level, which survival shares with the coop
// missions natively: codol_coop_progress.c).

#using_animtree ( "generic_human" );

init()
{
	level.survival_over = false;
	level.survival_success = undefined;
	level.survival_alive = 0;
	level.survival_spawned = 0;
	level.survival_killed = 0;
	level.survival_spawn_failures = 0;
	level.survival_templates = [];
	level.survival_template_missing = [];

	level.survival_max_alive = getDvarInt( "scr_survival_max_alive" );
	if ( level.survival_max_alive <= 0 )
		level.survival_max_alive = 24;
	// 32 native actor slots (CODOL_MAX_ACTOR_SLOTS); keep room for the dogs
	// and bosses a wave adds.
	if ( level.survival_max_alive > 28 )
		level.survival_max_alive = 28;

	precacheString( &"SCRIPT_MISSION_COMPLETE" );
}

living_players()
{
	out = [];
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isAlive( players[ i ] ) && isDefined( players[ i ].sessionstate ) && players[ i ].sessionstate == "playing" )
			out[ out.size ] = players[ i ];
	}
	return out;
}

nearest_player_distance( origin )
{
	players = living_players();
	best = undefined;
	for ( i = 0; i < players.size; i++ )
	{
		d = distance( players[ i ].origin, origin );
		if ( !isDefined( best ) || d < best )
			best = d;
	}
	return best;
}

// --------------------------------------------------------------------------
// Spawning
// --------------------------------------------------------------------------

template_for( cls )
{
	if ( !isDefined( cls ) || cls == "" )
		return undefined;
	key = toLower( cls );
	if ( isDefined( level.survival_templates[ key ] ) )
		return level.survival_templates[ key ];
	if ( isDefined( level.survival_template_missing[ key ] ) )
		return undefined;
	t = spawnActorSpawner( key, ( 0, 0, 0 ), ( 0, 0, 0 ), -1 );
	if ( !isDefined( t ) )
	{
		level.survival_template_missing[ key ] = true;
		codolprintln( "CoDOL survival: no aitype template for " + cls );
		return undefined;
	}
	t.targetname = "survival_template_" + key;
	level.survival_templates[ key ] = t;
	return t;
}

// A soldier of classname `cls` at origin/angles, or undefined when the actor
// budget, the template or the native spawn refuses.  `kind` is what the
// aitype's soldier_setup may take (accuracy, role, health), handed over
// through level.coop_soldier_next, the shared aitype contract.
spawn_soldier( cls, origin, angles, kind )
{
	if ( level.survival_over )
		return undefined;
	if ( level.survival_alive >= level.survival_max_alive )
		return undefined;
	template = template_for( cls );
	if ( !isDefined( template ) )
		return undefined;
	level.coop_soldier_next = kind;
	ai = template spawnActor();
	level.coop_soldier_next = undefined;
	if ( !isDefined( ai ) )
	{
		level.survival_spawn_failures++;
		return undefined;
	}
	if ( !isDefined( angles ) )
		angles = ( 0, randomInt( 360 ), 0 );
	// The soldier species answers whether it found clear floor; the dog's
	// teleport answers nothing.
	placed = ai forceTeleport( origin, angles );
	if ( isDefined( placed ) && !placed )
		codolprintln( "CoDOL survival: " + cls + " at " + origin + " has no clear floor nearby; left at " + ai.origin );
	ai show();
	ai.targetname = "survival_enemy";
	level.survival_alive++;
	level.survival_spawned++;
	ai thread count_death();
	return ai;
}

// The soldier's weapon in his hands (the coop contract, _coopmission
// arm_soldier): the world model on tag_weapon_right and the weapon named in
// his entity state for C1's fire events.
arm_soldier()
{
	if ( !isDefined( self.coop_weapon ) || self.coop_weapon == "" )
		return;
	self codol_actorSetWeapon( self.coop_weapon );
	if ( isDefined( self.coop_weapon_model ) )
		return;
	model = getWeaponModel( self.coop_weapon );
	if ( isDefined( model ) && model != "" && isDefined( level.coop_weapon_models ) &&
	     isDefined( level.coop_weapon_models[ model ] ) )
	{
		self attach( model, "tag_weapon_right" );
		self.coop_weapon_model = model;
	}
}

// The bootstrap pose (soldier_init.gsc's main() runs too early to animate).
first_pose()
{
	self setAnimKnobAll( %exposed_aim_5, %body, 1, 0.1, 1 );
}

count_death()
{
	self waittill( "death", attacker );
	level.survival_alive--;
	level.survival_killed++;
	if ( isDefined( attacker ) && isPlayer( attacker ) )
		attacker.kills = int( attacker.kills ) + 1;
	level notify( "survival_axis_died" );
}

remove_actor( ai )
{
	if ( !isDefined( ai ) || !isAlive( ai ) )
		return;
	ai hide();
	ai doDamage( ai.health + 1000, ai.origin );
}

// --------------------------------------------------------------------------
// The end
// --------------------------------------------------------------------------

announce( text )
{
	codolprintln( "CoDOL survival: " + text );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] iPrintLnBold( text );
}

match_over( success, reason )
{
	if ( level.survival_over )
		return;
	level.survival_over = true;
	level.survival_success = success;
	level notify( "survival_over" );
	codolprintln( "CoDOL survival: over, success " + success + " (" + reason + "), " +
		level.survival_killed + " killed, " + level.survival_spawned + " spawned" );
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
	delay = getDvarInt( "scr_survival_end_delay" );
	if ( delay <= 0 )
		delay = 10;
	wait( delay );

	// The mission report (IW5 survival_leaderboard_wave_func): the session as
	// the client named it (coop_survival_<level>), success at five waves
	// (IW5 congrat_min_wave).  exitLevel then returns everyone to the lobby
	// and loads nothing further (codol_coop_progress.c).
	mission = "coop_survival_" + getDvar( "mapname" );
	success = isDefined( level.survival_success ) && level.survival_success;
	for ( i = 0; i < players.size; i++ )
	{
		if ( isDefined( players[ i ] ) )
			players[ i ] codol_reportCoopMission( mission, success );
	}
	exitLevel( false );
}
