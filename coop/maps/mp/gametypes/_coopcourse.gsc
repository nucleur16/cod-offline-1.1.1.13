// coop training course: pop-up targets, the pit course and its timer.
//
// mp_killhouse_sh carries IW4's whole trainer level in its own entity string
// (MEASURED, docs/CODOL_CAMPAIGN_MODE.md section 8.2): the pit course's
// pop-up targets (script_brushmodel script_noteworthy target_enemy /
// target_friendly, each targeting a script_origin pivot that targets its
// aim-assist brushmodel), its nine course triggers (trigger_multiple
// targetname target_trigger, script_noteworthy course_triggers_01,
// script_linkTo pf1492_<n> naming the targets whose script_linkName is <n>),
// the cargoship mock-up's targets (cargoship_target groups position_*_targets,
// deck_target hostile / friendly) and the frag range (grenade_target_dummy).
// In an mp match the multiplayer scripts never touch them; in a coop match on
// mp_killhouse_sh they are the training mission.
//
// Ported from IW4 maps\trainer.gsc target_think / target_triggers_think and
// maps\so_killspree_trainer.gsc (the course loop, its timer and its
// enemy / civilian counts):
//   * a target starts down (its pivot pitched -90, "reverse" +90, "sideways"
//     yawed -180), pops up on "pop_up" with target_up_metal, takes damage
//     only while up, and goes back down when a player hits it
//     (target_metal_hit; a friendly plays target_mistake_buzzer);
//   * the pit course starts at the first course trigger a player touches
//     (killhouse_buzzer), pops each trigger's linked targets as the player
//     reaches it, and finishes when every course enemy target is down or
//     after scr_coop_course_time (120 s): time, enemies hit, civilians hit;
//     then it resets and can be run again;
//   * the cargoship and deck targets pop by group when a player comes within
//     512 units of the group, and re-arm 20 s after the group is down.
//
// The lateral rail movement (script_parameters use_rail) and the melee-only
// target are not ported; rail targets pop up in place.
//
// Coop category root only; names no multiplayer module.

init()
{
	level.coop_course_targets = [];
	level.coop_course_enemies = [];
	level.coop_course_running = false;
	level.coop_course_runs = 0;
	level.coop_course_hits = 0;
	level.coop_course_civs_hit = 0;

	brushes = getEntArray( "script_brushmodel", "classname" );
	pivots = 0;
	for ( i = 0; i < brushes.size; i++ )
	{
		b = brushes[ i ];
		type = target_type( b );
		if ( !isDefined( type ) || !isDefined( b.target ) )
			continue;
		pivot = getEnt( b.target, "targetname" );
		if ( !isDefined( pivot ) || pivot.classname != "script_origin" )
			continue;
		b.coop_target_type = type;
		b.coop_pivot = pivot;
		b thread target_think();
		level.coop_course_targets[ level.coop_course_targets.size ] = b;
		pivots++;
	}

	if ( pivots <= 0 )
		return;

	codolprintln( "CoDOL coop course: " + pivots + " pop-up targets" );
	level thread course_triggers();
	level thread proximity_groups();
}

// "enemy" / "friendly", or undefined for a brushmodel that is not a target.
target_type( b )
{
	if ( !isDefined( b.script_noteworthy ) )
		return undefined;
	switch ( b.script_noteworthy )
	{
	case "target_enemy":
	case "cargoship_target":
	case "grenade_target_dummy":
		return "enemy";
	case "target_friendly":
		return "friendly";
	case "deck_target":
		if ( isDefined( b.targetname ) && b.targetname == "friendly" )
			return "friendly";
		return "enemy";
	}
	return undefined;
}

pivot_down( pivot, time )
{
	mode = pivot.script_noteworthy;
	if ( !isDefined( mode ) )
		mode = "standard";
	if ( mode == "reverse" )
		pivot rotatePitch( 90, time );
	else if ( mode == "sideways" )
		pivot rotateYaw( -180, time );
	else
		pivot rotatePitch( -90, time );
}

pivot_up( pivot, time )
{
	mode = pivot.script_noteworthy;
	if ( !isDefined( mode ) )
		mode = "standard";
	if ( mode == "reverse" )
		pivot rotatePitch( -90, time );
	else if ( mode == "sideways" )
		pivot rotateYaw( 180, time );
	else
		pivot rotatePitch( 90, time );
}

// IW4 trainer.gsc target_think.
target_think()
{
	pivot = self.coop_pivot;
	self linkTo( pivot );
	if ( isDefined( pivot.target ) )
	{
		assist = getEnt( pivot.target, "targetname" );
		if ( isDefined( assist ) && assist != self )
		{
			assist hide();
			assist notSolid();
		}
	}
	pivot_down( pivot, 0.25 );
	self.coop_up = false;

	for ( ;; )
	{
		self waittill( "pop_up" );
		if ( self.coop_up )
			continue;
		wait( randomFloatRange( 0, 0.2 ) );

		self solid();
		self playSound( "target_up_metal" );
		self setCanDamage( true );
		pivot_up( pivot, 0.25 );
		self.coop_up = true;
		wait( 0.25 );

		for ( ;; )
		{
			self waittill( "damage", amount, attacker, dir, point, type );
			if ( isDefined( type ) && type == "scripted_target_drop" )
				break;
			if ( !isDefined( attacker ) || !isPlayer( attacker ) )
				continue;
			if ( isDefined( type ) && type == "MOD_IMPACT" )
				continue;

			self playSound( "target_metal_hit" );
			if ( self.coop_target_type == "friendly" )
			{
				attacker playLocalSound( "target_mistake_buzzer" );
				level.coop_course_civs_hit++;
				attacker iPrintLnBold( "Civilian hit" );
			}
			else
			{
				level.coop_course_hits++;
				if ( !isDefined( attacker.coop_targets_hit ) )
					attacker.coop_targets_hit = 0;
				attacker.coop_targets_hit++;
			}
			break;
		}

		self notify( "hit" );
		level notify( "coop_target_down" );
		self setCanDamage( false );
		self notSolid();
		pivot_down( pivot, 0.25 );
		self.coop_up = false;
		wait( 0.25 );
	}
}

drop( targets )
{
	for ( i = 0; i < targets.size; i++ )
	{
		if ( targets[ i ].coop_up )
			targets[ i ] notify( "damage", 0, undefined, ( 0, 0, 0 ), targets[ i ].origin, "scripted_target_drop" );
	}
}

// --------------------------------------------------------------------------
// The pit course (so_killspree_trainer course loop).
// --------------------------------------------------------------------------

link_suffix( name )
{
	// script_linkTo "pf1492_3" names the targets whose script_linkName is "3".
	tokens = strTok( name, "_" );
	if ( tokens.size <= 0 )
		return name;
	return tokens[ tokens.size - 1 ];
}

course_triggers()
{
	level endon( "coop_mission_over" );

	triggers = getEntArray( "target_trigger", "targetname" );
	if ( triggers.size <= 0 )
		return;

	level.coop_course_triggers = triggers;
	for ( i = 0; i < triggers.size; i++ )
	{
		t = triggers[ i ];
		t.coop_targets = [];
		if ( !isDefined( t.script_linkTo ) )
			continue;
		links = strTok( t.script_linkTo, " " );
		for ( l = 0; l < links.size; l++ )
		{
			key = link_suffix( links[ l ] );
			for ( j = 0; j < level.coop_course_targets.size; j++ )
			{
				target = level.coop_course_targets[ j ];
				if ( isDefined( target.script_linkName ) && target.script_linkName == key )
				{
					t.coop_targets[ t.coop_targets.size ] = target;
					if ( target.coop_target_type == "enemy" )
						level.coop_course_enemies[ level.coop_course_enemies.size ] = target;
				}
			}
		}
		t thread course_trigger_think();
	}
	codolprintln( "CoDOL coop course: pit course, " + triggers.size + " triggers, " + level.coop_course_enemies.size + " enemy targets" );
}

course_trigger_think()
{
	level endon( "coop_mission_over" );
	for ( ;; )
	{
		self waittill( "trigger", who );
		if ( !isDefined( who ) || !isPlayer( who ) )
			continue;
		if ( !level.coop_course_running )
			level thread course_run( who );
		if ( isDefined( self.coop_fired_run ) && self.coop_fired_run == level.coop_course_runs )
			continue;
		self.coop_fired_run = level.coop_course_runs;
		if ( isDefined( self.script_delay ) )
			wait( self.script_delay );
		for ( i = 0; i < self.coop_targets.size; i++ )
			self.coop_targets[ i ] notify( "pop_up" );
	}
}

course_run( runner )
{
	level endon( "coop_mission_over" );

	level.coop_course_running = true;
	level.coop_course_runs++;
	level.coop_course_hits = 0;
	level.coop_course_civs_hit = 0;
	start = getTime();

	limit = getDvarInt( "scr_coop_course_time" );
	if ( limit <= 0 )
		limit = 120;

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] playLocalSound( "killhouse_buzzer" );
	maps\mp\gametypes\_coopmission::announce( "Course started: " + level.coop_course_enemies.size + " targets" );
	codolprintln( "CoDOL coop course: run " + level.coop_course_runs + " started by " + runner.name );

	timer = newHudElem();
	timer.x = 0;
	timer.y = 80;
	timer.alignX = "center";
	timer.alignY = "top";
	timer.horzAlign = "center";
	timer.vertAlign = "top";
	timer.fontScale = 1.6;
	timer setTenthsTimerUp( 0.1 );

	finished = false;
	while ( ( getTime() - start ) < limit * 1000 )
	{
		level waittill_or_timeout( "coop_target_down", 1 );
		if ( level.coop_course_hits >= level.coop_course_enemies.size )
		{
			finished = true;
			break;
		}
	}

	elapsed = ( getTime() - start ) / 1000.0;
	timer destroy();

	for ( i = 0; i < players.size; i++ )
	{
		if ( isDefined( players[ i ] ) )
			players[ i ] playLocalSound( "killhouse_buzzer" );
	}
	if ( finished )
		maps\mp\gametypes\_coopmission::announce( "Course complete: " + elapsed + " s, civilians hit " + level.coop_course_civs_hit );
	else
		maps\mp\gametypes\_coopmission::announce( "Out of time: " + level.coop_course_hits + " of " + level.coop_course_enemies.size + " targets" );
	codolprintln( "CoDOL coop course: run " + level.coop_course_runs + " " + finished + " in " + elapsed + " s, hits " +
		level.coop_course_hits + "/" + level.coop_course_enemies.size + ", civilians " + level.coop_course_civs_hit );

	if ( finished )
	{
		level.coop_course_best = elapsed;
		level notify( "coop_course_complete" );
	}

	wait( 3 );
	drop( level.coop_course_targets );
	wait( 2 );
	level.coop_course_running = false;
}

waittill_or_timeout( note, timeout )
{
	self endon( note );
	wait( timeout );
}

// --------------------------------------------------------------------------
// The cargoship mock-up and deck targets: by group, on approach.
// --------------------------------------------------------------------------

proximity_groups()
{
	level endon( "coop_mission_over" );

	groups = [];
	keys = [];
	for ( i = 0; i < level.coop_course_targets.size; i++ )
	{
		t = level.coop_course_targets[ i ];
		if ( t.script_noteworthy != "cargoship_target" && t.script_noteworthy != "deck_target" && t.script_noteworthy != "grenade_target_dummy" )
			continue;
		key = t.script_noteworthy;
		if ( isDefined( t.targetname ) && t.script_noteworthy == "cargoship_target" )
			key = t.targetname;
		if ( !isDefined( groups[ key ] ) )
		{
			groups[ key ] = [];
			keys[ keys.size ] = key;
		}
		groups[ key ][ groups[ key ].size ] = t;
	}
	for ( k = 0; k < keys.size; k++ )
		level thread proximity_group_think( keys[ k ], groups[ keys[ k ] ] );
	if ( keys.size > 0 )
		codolprintln( "CoDOL coop course: " + keys.size + " proximity target groups" );
}

proximity_group_think( key, targets )
{
	level endon( "coop_mission_over" );
	for ( ;; )
	{
		near = false;
		players = getPlayers();
		for ( p = 0; p < players.size && !near; p++ )
		{
			if ( !isAlive( players[ p ] ) )
				continue;
			for ( i = 0; i < targets.size; i++ )
			{
				if ( distanceSquared( players[ p ].origin, targets[ i ].origin ) < 512 * 512 )
				{
					near = true;
					break;
				}
			}
		}
		if ( !near )
		{
			wait( 0.5 );
			continue;
		}

		for ( i = 0; i < targets.size; i++ )
			targets[ i ] notify( "pop_up" );
		codolprintln( "CoDOL coop course: target group " + key + " up" );

		// Down once every enemy of the group is down, then re-arm.
		for ( ;; )
		{
			level waittill_or_timeout( "coop_target_down", 2 );
			up = 0;
			for ( i = 0; i < targets.size; i++ )
			{
				if ( targets[ i ].coop_up && targets[ i ].coop_target_type == "enemy" )
					up++;
			}
			if ( up == 0 )
				break;
		}
		drop( targets );
		wait( 20 );
	}
}
