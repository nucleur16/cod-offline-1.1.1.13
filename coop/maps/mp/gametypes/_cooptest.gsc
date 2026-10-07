// coop mission probe harness (operator / developer only).
//
// Inert unless the server is started with +set scr_coop_test 1.  It then
// polls the dvar scr_coop_test_cmd (set over rcon) for one command at a time,
// the same pattern zm's _zombiemode_blockertest uses:
//
//   addbot                 addtestclient(); the bot spawns like a player
//   status                 one log line of mission state and every live enemy
//   goto <n> <x> <y> <z>   setOrigin player n (index into getPlayers())
//   gotoflag <n> <flag>    setOrigin player n onto the first trigger that sets
//                          <flag> (fires it the way walking into it would)
//   gototrigger <n> <i>    setOrigin player n onto the i-th spawn trigger
//   killenemies [k]        kill up to k live enemies (all by default), with
//                          player 0 as the attacker
//   godmode <n>            keep player n's health topped up
//   weapons <name> ...     log codol_isWeaponMounted / codol_isWeaponPublished
//   playroute              training mission as a player plays it: player 0
//                          WALKS (botMoveTo) into each trigger and to each
//                          instructor, fires its rifle at the range dummies
//                          and throws real frags; a walk that makes no
//                          progress is logged as STUCK (a closed door) and
//                          then skipped by teleport so the run goes on
//   route                  training mission: do what each station waits for
//                          (walk there, shoot its targets, throw, run the
//                          course) as player 0, until the mission ends
//
// Lives under the coop category root and is named only by _coopmode, so it
// exists in no mp or zm match.

main()
{
	if ( getDvar( "scr_coop_test" ) == "" || getDvarInt( "scr_coop_test" ) == 0 )
		return;

	setDvar( "scr_coop_test_cmd", "" );
	codolprintln( "CoDOL cooptest: harness active" );
	level thread watch();
}

watch()
{
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_coop_test_cmd" );
		if ( request == "" )
			continue;
		setDvar( "scr_coop_test_cmd", "" );

		tokens = strTok( request, " " );
		if ( tokens.size <= 0 )
			continue;

		switch ( tokens[ 0 ] )
		{
		case "addbot":
			bot = addtestclient();
			codolprintln( "CoDOL cooptest: addtestclient" );
			if ( isDefined( bot ) )
				bot thread bot_body();
			break;
		case "status":
			status();
			break;
		case "goto":
			if ( tokens.size >= 5 )
				teleport( int( tokens[ 1 ] ), ( float( tokens[ 2 ] ), float( tokens[ 3 ] ), float( tokens[ 4 ] ) ) );
			break;
		case "gotoflag":
			if ( tokens.size >= 3 )
				goto_flag( int( tokens[ 1 ] ), tokens[ 2 ] );
			break;
		case "gototrigger":
			if ( tokens.size >= 3 )
				goto_trigger( int( tokens[ 1 ] ), int( tokens[ 2 ] ) );
			break;
		case "killenemies":
			count = 999;
			if ( tokens.size >= 2 )
				count = int( tokens[ 1 ] );
			kill_enemies( count );
			break;
		case "weapons":
			for ( i = 1; i < tokens.size; i++ )
				codolprintln( "CoDOL cooptest: weapon " + tokens[ i ] + " mounted " + codol_isWeaponMounted( tokens[ i ] ) +
					" published " + codol_isWeaponPublished( tokens[ i ] ) );
			break;
		case "uselaptops":
			p = player( 0 );
			objs = getEntArray( "pmc_objective", "targetname" );
			for ( i = 0; i < objs.size; i++ )
			{
				if ( !isDefined( objs[ i ].target ) )
					continue;
				use = getEnt( objs[ i ].target, "targetname" );
				if ( isDefined( use ) && isDefined( p ) )
					use notify( "trigger", p );
				wait( 0.2 );
			}
			codolprintln( "CoDOL cooptest: used " + objs.size + " laptops" );
			break;
		case "goextract":
			evac = maps\mp\gametypes\_coopmission::find_extraction();
			if ( isDefined( evac ) )
				teleport( 0, evac.origin );
			else
				codolprintln( "CoDOL cooptest: no extraction trigger" );
			break;
		case "route":
			level thread route();
			break;
		case "playroute":
			level thread play_route();
			break;
		case "opengates":
			maps\mp\gametypes\_cooptraining::open_everything();
			break;
		case "aimcheck":
			level thread aim_check();
			break;
		case "walk":
			// walk <x> <y> <z> [<x> <y> <z> ...]: player 0 walks the points.
			level thread walk_points( tokens );
			break;
		case "runcourse":
			level thread run_course();
			break;
		case "checkstarts":
			level thread check_starts();
			break;
		case "godmode":
			if ( tokens.size >= 2 )
				level thread godmode( int( tokens[ 1 ] ) );
			break;
		}
	}
}

// A test client has no uploaded profile, so _coopmode composes no character
// for it (codol_applyProfileCharacter raises for one), and with no model the
// player has no server DObj -- and the retained player_die refuses to kill a
// client without one (MEASURED: "CoDOL player_die refused: ... gate missing
// server DObj health -999").  Give the probe bot the one body every coop
// match mounts, after each spawn, so it can die and respawn like a player.
bot_body()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "spawned_player" );
		self setModel( "body_complete_sp_juggernaut" );
		codolprintln( "CoDOL cooptest: bot " + self getEntityNumber() + " given a body" );
	}
}

// runcourse: walk player 0 through every pit-course trigger and shoot every
// target that is up (as player 0), then report.
run_course()
{
	p = player( 0 );
	if ( !isDefined( p ) || !isDefined( level.coop_course_triggers ) )
	{
		codolprintln( "CoDOL cooptest: no player or no course" );
		return;
	}
	triggers = level.coop_course_triggers;
	for ( i = 0; i < triggers.size; i++ )
	{
		p setOrigin( triggers[ i ].origin );
		wait( 1.5 );
		if ( isDefined( triggers[ i ].script_delay ) )
			wait( triggers[ i ].script_delay );
		up = 0;
		for ( j = 0; j < level.coop_course_targets.size; j++ )
		{
			t = level.coop_course_targets[ j ];
			if ( t.coop_up && t.coop_target_type == "enemy" )
			{
				t notify( "damage", 50, p, ( 0, 0, 0 ), t.origin, "MOD_RIFLE_BULLET" );
				up++;
			}
		}
		codolprintln( "CoDOL cooptest: course trigger " + i + " at " + triggers[ i ].origin + " (" + triggers[ i ].coop_targets.size +
			" linked), shot " + up + " targets, hits " + level.coop_course_hits );
		wait( 0.5 );
	}
	codolprintln( "CoDOL cooptest: runcourse done, running " + level.coop_course_running );
}

// checkstarts: every start classname the coop spawn order knows, with the
// verdict _coopmode::isUsableStart gives it, a ground trace and the nearest
// waypoint, then player 0 is dropped on each one for 3 s to see whether he
// stays put (a start outside the world falls or is stuck in solid).
check_starts()
{
	names = maps\mp\gametypes\_coopmode::spawnClassnames();
	p = player( 0 );
	for ( n = 0; n < names.size; n++ )
	{
		points = getEntArray( names[ n ], "classname" );
		for ( i = 0; i < points.size; i++ )
		{
			s = points[ i ];
			trace = bulletTrace( s.origin + ( 0, 0, 32 ), s.origin - ( 0, 0, 1024 ), false, undefined );
			nav = codol_botNavPointNear( s.origin, 768 );
			navText = "none within 768";
			if ( isDefined( nav ) )
				navText = "" + int( distance( nav, s.origin ) );
			line = "CoDOL cooptest: start " + names[ n ] + " " + s.origin + " usable " +
				maps\mp\gametypes\_coopmode::isUsableStart( s ) + " ground frac " + trace[ "fraction" ] +
				" at " + trace[ "position" ] + " surface " + trace[ "surfacetype" ] + " nav " + navText;
			if ( isDefined( p ) && isAlive( p ) )
			{
				p setOrigin( s.origin );
				wait( 3 );
				line += " | player after 3 s at " + p.origin + " dz " + int( p.origin[ 2 ] - s.origin[ 2 ] );
			}
			codolprintln( line );
		}
		// Only the first few MP fallbacks are interesting.
		if ( names[ n ] == "info_player_start" )
			break;
	}
	codolprintln( "CoDOL cooptest: checkstarts done" );
}

player( index )
{
	players = getPlayers();
	if ( index < 0 || index >= players.size )
		return undefined;
	return players[ index ];
}

teleport( index, origin )
{
	p = player( index );
	if ( !isDefined( p ) )
		return;
	p setOrigin( origin );
	codolprintln( "CoDOL cooptest: player " + index + " to " + origin );
}

goto_flag( index, name )
{
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( !isDefined( e.script_flag ) || e.script_flag != name )
			continue;
		if ( !isDefined( e.classname ) || getSubStr( e.classname, 0, 8 ) != "trigger_" )
			continue;
		teleport( index, e.origin );
		return;
	}
	codolprintln( "CoDOL cooptest: no trigger sets flag " + name );
}

goto_trigger( index, which )
{
	found = 0;
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( !isDefined( e.classname ) || getSubStr( e.classname, 0, 8 ) != "trigger_" || !isDefined( e.target ) )
			continue;
		targets = getEntArray( e.target, "targetname" );
		spawners = 0;
		for ( j = 0; j < targets.size; j++ )
		{
			if ( isDefined( targets[ j ].classname ) && getSubStr( targets[ j ].classname, 0, 12 ) == "actor_enemy_" )
				spawners++;
		}
		if ( spawners <= 0 )
			continue;
		if ( found == which )
		{
			codolprintln( "CoDOL cooptest: spawn trigger " + which + " is " + e.classname + " targeting " + e.target +
				" (" + spawners + " spawners)" );
			teleport( index, e.origin );
			return;
		}
		found++;
	}
	codolprintln( "CoDOL cooptest: only " + found + " spawn triggers" );
}

status()
{
	line = "CoDOL cooptest: status over " + level.coop_mission_over + " alive " + level.coop_alive +
		" spawned " + level.coop_spawned + " killed " + level.coop_killed + " failures " + level.coop_spawn_failures;
	if ( isDefined( level.coop_kills_to_win ) )
		line += " goal " + level.coop_kills_to_win;
	codolprintln( line );

	for ( i = 0; i < level.coop_enemies.size; i++ )
	{
		ai = level.coop_enemies[ i ];
		if ( !isDefined( ai ) || !isAlive( ai ) )
			continue;
		enemy = "none";
		if ( isDefined( ai.enemy ) )
			enemy = "" + ai.enemy getEntityNumber();
		codolprintln( "CoDOL cooptest:   ai " + ai getEntityNumber() + " " + ai.coop_role + " " + ai.coop_weapon +
			" hp " + ai.health + " at " + ai.origin + " enemy " + enemy );
	}

	if ( isDefined( level.coop_cast_list ) )
	{
		for ( i = 0; i < level.coop_cast_list.size; i++ )
		{
			ai = level.coop_cast_list[ i ];
			if ( !isDefined( ai ) )
				continue;
			name = ai.coop_cast;
			if ( isDefined( ai.coop_name ) )
				name += " (" + ai.coop_name + ")";
			codolprintln( "CoDOL cooptest:   cast " + name + " team " + ai.team + " alive " + isAlive( ai ) + " at " + ai.origin +
				" idle " + isDefined( ai.coop_idle_anim ) );
		}
	}

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		codolprintln( "CoDOL cooptest:   player " + i + " " + p.sessionstate + " hp " + p.health + " at " + p.origin +
			" lives " + p.coop_lives_left + " kills " + p.kills );
	}
}

kill_enemies( count )
{
	attacker = player( 0 );
	killed = 0;
	for ( i = 0; i < level.coop_enemies.size && killed < count; i++ )
	{
		ai = level.coop_enemies[ i ];
		if ( !isDefined( ai ) || !isAlive( ai ) )
			continue;
		if ( isDefined( attacker ) )
			ai doDamage( ai.health + 100, ai.origin, attacker );
		else
			ai doDamage( ai.health + 100, ai.origin );
		killed++;
	}
	codolprintln( "CoDOL cooptest: killed " + killed );
}

godmode( index )
{
	p = player( index );
	if ( !isDefined( p ) )
		return;
	p endon( "disconnect" );
	for ( ;; )
	{
		if ( isAlive( p ) && p.health < p.maxhealth )
			p.health = p.maxhealth;
		wait( 0.05 );
	}
}

// route: the coop_training_mp_killhouse_sh autopilot.  _cooptraining.gsc
// publishes what it is waiting for in level.coop_training_step; this does it
// as player 0 would, once per step, and logs where player 0 stands.
route()
{
	level endon( "coop_mission_over" );
	codolprintln( "CoDOL cooptest: route started" );
	last = undefined;
	for ( ;; )
	{
		wait( 1 );
		s = level.coop_training_step;
		p = player( 0 );
		if ( !isDefined( s ) || !isDefined( p ) || !isAlive( p ) )
			continue;
		if ( isDefined( p.coop_in_intro ) )
			continue;
		if ( isDefined( last ) && last == s && s.kind != "hits" && s.kind != "note" && s.kind != "ship" )
			continue;
		last = s;
		switch ( s.kind )
		{
		case "flag":
			goto_flag( 0, s.value );
			break;
		case "near":
			teleport( 0, s.value );
			break;
		case "hits":
			type = "MOD_RIFLE_BULLET";
			if ( isDefined( s.value2 ) && s.value2 )
				type = "MOD_MELEE";
			for ( i = 0; i < s.value.size; i++ )
				s.value[ i ] notify( "damage", 50, p, ( 0, 0, 0 ), s.value[ i ].origin, type );
			break;
		case "note":
			p notify( s.value );
			break;
		case "course":
			run_course();
			break;
		case "ship":
			teleport( 0, s.value[ 0 ].origin + ( 0, 0, 8 ) );
			for ( i = 0; i < s.value.size; i++ )
			{
				t = s.value[ i ];
				if ( t.coop_up && t.coop_target_type == "enemy" )
					t notify( "damage", 50, p, ( 0, 0, 0 ), t.origin, "MOD_RIFLE_BULLET" );
			}
			break;
		}
		codolprintln( "CoDOL cooptest: route " + s.kind + ", player 0 at " + p.origin );
	}
}

// playroute: route() with a player's means (see the header).
play_route()
{
	level endon( "coop_mission_over" );
	codolprintln( "CoDOL cooptest: playroute started" );
	last = undefined;
	for ( ;; )
	{
		wait( 0.5 );
		s = level.coop_training_step;
		p = player( 0 );
		if ( !isDefined( s ) || !isDefined( p ) || !isAlive( p ) || isDefined( p.coop_in_intro ) )
			continue;
		if ( isDefined( last ) && last == s && s.kind != "hits" && s.kind != "note" && s.kind != "ship" )
			continue;
		last = s;
		switch ( s.kind )
		{
		case "flag":
			t = flag_trigger( s.value );
			if ( isDefined( t ) )
				walk_to( p, t.origin, "flag " + s.value );
			break;
		case "near":
			walk_to( p, s.value, "near" );
			break;
		case "hits":
			// The range: in through its door to the firing stall first.
			stall = getEnt( "obj_station_one_glowing", "targetname" );
			if ( isDefined( stall ) && distance( p.origin, stall.origin ) > 200 && p.origin[ 1 ] < -1150 )
			{
				walk_to( p, ( 1408, -1240, 4 ), "range door, outside" );
				walk_to( p, ( 1408, -1100, 4 ), "range door, inside" );
				walk_to( p, stall.origin, "firing stall" );
			}
			if ( isDefined( s.value2 ) && s.value2 )
			{
				// The knife: walk up to the dummy and press melee.
				walk_to( p, s.value[ 0 ].origin + ( 48, -48, 0 ), "melee dummy" );
				p botLookAt( s.value[ 0 ].origin, 1 );
				wait( 0.4 );
				p codol_botPressButtons( 4, 0.3 );
				wait( 1.5 );
				if ( level.coop_training_step == s )
				{
					s.value[ 0 ] notify( "damage", 50, p, ( 0, 0, 0 ), s.value[ 0 ].origin, "MOD_MELEE" );
					codolprintln( "CoDOL cooptest: playroute melee button gave no hit; knife simulated" );
				}
				break;
			}
			for ( i = 0; i < s.value.size; i++ )
			{
				// Aim and pull the trigger (C1 usercmd button 1);
				// codol_botFireAt only serves combat bots.
				p botLookAt( s.value[ i ].origin + ( 0, 0, 40 ), 0.8 );
				wait( 0.4 );
				p codol_botPressButtons( 1, 0.1 );
				wait( 0.5 );
			}
			codolprintln( "CoDOL cooptest: playroute fired at " + s.value.size + " dummies, hits " + level.coop_training_hits );
			break;
		case "note":
			if ( s.value == "grenade_fire" )
			{
				p codol_botPressButtons( 16384, 0.3 );
				wait( 2 );
			}
			else
			{
				p notify( s.value );
				p notify( "grenade_fire" );
			}
			break;
		case "course":
			run_course();
			break;
		case "ship":
			for ( i = 0; i < s.value.size; i++ )
			{
				t = s.value[ i ];
				if ( t.coop_up && t.coop_target_type == "enemy" )
					t notify( "damage", 50, p, ( 0, 0, 0 ), t.origin, "MOD_RIFLE_BULLET" );
			}
			break;
		}
	}
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

// Walk player 0 to `dest`.  STUCK: under 16 units of progress in 4 s.
walk_to( p, dest, what )
{
	// Out of the range hangar the way a player leaves it: by its door.
	if ( isDefined( level.coop_training_flow ) && level.coop_training_flow && !isDefined( p.coop_walk_nested ) &&
	     p.origin[ 0 ] > 1300 && p.origin[ 0 ] < 2100 && p.origin[ 1 ] > -1180 && p.origin[ 1 ] < -500 && dest[ 1 ] < -1200 )
	{
		p.coop_walk_nested = true;
		walk_to( p, ( 1408, -1100, 4 ), "range door, inside" );
		walk_to( p, ( 1408, -1260, 4 ), "range door, outside" );
		p.coop_walk_nested = undefined;
	}
	radius = 40;
	if ( isSubStr( what, "near" ) )
		radius = 150;
	start = p.origin;
	p botMoveTo( dest );
	lastPos = p.origin;
	still = 0;
	for ( t = 0; t < 90; t += 0.5 )
	{
		wait( 0.5 );
		if ( !isDefined( p ) || !isAlive( p ) )
			return false;
		d = p.origin - dest;
		if ( length( ( d[ 0 ], d[ 1 ], 0 ) ) < radius )
		{
			p botStop();
			codolprintln( "CoDOL cooptest: playroute walked to " + what + " from " + start + " in " + t + " s" );
			return true;
		}
		if ( distance( p.origin, lastPos ) < 16 )
			still += 0.5;
		else
			still = 0;
		lastPos = p.origin;
		if ( still >= 4 )
			break;
	}
	p botStop();
	codolprintln( "CoDOL cooptest: playroute STUCK walking to " + what + " " + dest + " at " + p.origin + "; teleporting" );
	// Beside the point, not on it: an instructor stands on his own spot.
	if ( radius > 40 )
		p setOrigin( dest + ( 0, -80, 0 ) );
	else
		p setOrigin( dest );
	return false;
}

walk_points( tokens )
{
	p = player( 0 );
	if ( !isDefined( p ) )
		return;
	for ( i = 1; i + 2 < tokens.size; i += 3 )
		walk_to( p, ( float( tokens[ i ] ), float( tokens[ i + 1 ] ), float( tokens[ i + 2 ] ) ), "point " + int( ( i - 1 ) / 3 ) );
	codolprintln( "CoDOL cooptest: walk done at " + p.origin );
}

// aimcheck: for each range dummy (lane1, ads_tutorial_target), what a trace
// from player 0's eye to the dummy at several heights hits.
aim_check()
{
	p = player( 0 );
	if ( !isDefined( p ) )
		return;
	eye = p getEye();
	targets = getEntArray( "lane1", "targetname" );
	ents = getEntArray();
	for ( i = 0; i < ents.size; i++ )
	{
		if ( isDefined( ents[ i ].script_noteworthy ) && ents[ i ].script_noteworthy == "ads_tutorial_target" )
			targets[ targets.size ] = ents[ i ];
	}
	for ( i = 0; i < targets.size; i++ )
	{
		t = targets[ i ];
		line = "CoDOL cooptest: aimcheck target " + t getEntityNumber() + " at " + t.origin + " angles " + t.angles + ":";
		for ( h = -8; h <= 80; h += 8 )
		{
			trace = bulletTrace( eye, t.origin + ( 0, 0, h ), true, p );
			hit = "world";
			if ( trace[ "fraction" ] >= 1 )
				hit = "none";
			else if ( isDefined( trace[ "entity" ] ) )
			{
				if ( trace[ "entity" ] == t )
					hit = "TARGET";
				else
					hit = "ent" + trace[ "entity" ] getEntityNumber() + "/" + trace[ "entity" ].classname;
			}
			line += " " + h + "=" + hit;
		}
		codolprintln( line );
	}
	codolprintln( "CoDOL cooptest: aimcheck from " + eye );
}
