// coop mission probe harness (operator / developer only).
//
// Inert unless the server is started with +set scr_coop_test 1.  It then
// polls the dvar scr_coop_test_cmd (set over rcon) for one command at a time,
// the same pattern zm's _zombiemode_blockertest uses:
//
//   addbot                 addtestclient(); the bot spawns like a player
//   status                 one log line of mission state and every live enemy
//   characters             spawn one actor of every authored actor_ classname
//                          through its aitype and log its body, head and
//                          attachments, flagging any model no zone ships
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
//   complete / fail        end the mission now, complete or failed, through
//                          _coopmission::mission_complete / mission_failed
//                          (the result report and the lobby return)
//   route                  training mission: do what each station waits for
//                          (walk there, shoot its targets, throw, run the
//                          course) as player 0, until the mission ends
//
// A coop module (maps/) named only by _coopmode, so it exists in no mp or zm
// match.

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
		case "characters":
			characters();
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
		case "hurt":
			// hurt <damage>: wound every live enemy without killing him
			// (pain animscript probe)
			if ( tokens.size >= 2 )
				hurt_enemies( int( tokens[ 1 ] ) );
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
			evac = maps\_coopmission::find_extraction();
			if ( isDefined( evac ) )
				teleport( 0, evac.origin );
			else
				codolprintln( "CoDOL cooptest: no extraction trigger" );
			break;
		case "complete":
			maps\_coopmission::mission_complete( "cooptest complete" );
			break;
		case "fail":
			maps\_coopmission::mission_failed( "cooptest fail" );
			break;
		case "route":
			level thread route();
			break;
		case "playroute":
			level thread play_route();
			break;
		case "opengates":
			maps\_cooptraining::open_everything();
			break;
		case "aimcheck":
			level thread aim_check();
			break;
		case "pickuptest":
			// pickuptest <weapon_classname>: drop a world weapon at player 0's
			// feet and report his primaries before and after he walks over
			// it (the native pickup's two-primary rule in coop).
			if ( tokens.size >= 2 )
				level thread pickup_test( tokens[ 1 ] );
			break;
		case "jeeptags":
			// jeeptags: the intro vehicle's seat tags in its own frame
			// (forward, right, up), and where the driver model stands.
			if ( isDefined( level.coop_intro ) && isDefined( level.coop_intro.mover ) )
			{
				m = level.coop_intro.mover;
				tags = strTok( "tag_origin tag_body tag_driver tag_passenger tag_player tag_guy0 tag_guy1 tag_seat_rb", " " );
				for ( j = 0; j < tags.size; j++ )
				{
					o = m getTagOrigin( tags[ j ] ) - m.origin;
					codolprintln( "CoDOL cooptest: jeep " + tags[ j ] + " local ( " + vectorDot( o, anglesToForward( m.angles ) ) + ", " +
						vectorDot( o, anglesToRight( m.angles ) ) + ", " + o[ 2 ] + " ) angles " + m getTagAngles( tags[ j ] ) );
				}
				codolprintln( "CoDOL cooptest: jeep at " + m.origin + " angles " + m.angles );
			}
			break;
		case "knifetest":
			// knifetest: pop the pit's melee-only target and knife it for
			// real from the upstairs walkway.
			for ( j = 0; j < level.coop_course_targets.size; j++ )
			{
				g = level.coop_course_targets[ j ];
				if ( isDefined( g.script_parameters ) && g.script_parameters == "melee" )
				{
					g notify( "pop_up" );
					teleport( 0, ( 142, 1270, 150 ) );
					level thread knife_target( player( 0 ), g );
				}
			}
			break;
		case "pitwalk":
			// pitwalk: player 0 from the hangar through the whole pit course
			// on foot (walk_course), the entrance gates opened as the
			// briefing opens them.
			maps\_cooptraining::open_doors( "gate_cqb_out" );
			maps\_cooptraining::open_doors( "gate_cqb_start" );
			teleport( 0, ( 89, -1315, 4 ) );
			level thread walk_course( player( 0 ) );
			break;
		case "colmap":
			// colmap <x0> <y0> <x1> <y1> <z> <step>: a top-down map of what
			// stops a player there (see col_map).
			if ( tokens.size >= 7 )
				level thread col_map( float( tokens[ 1 ] ), float( tokens[ 2 ] ), float( tokens[ 3 ] ), float( tokens[ 4 ] ),
					float( tokens[ 5 ] ), float( tokens[ 6 ] ) );
			break;
		case "brushes":
			// brushes <targetname>: each brush's measured extent (brush_extent).
			if ( tokens.size >= 2 )
				level thread brush_report( tokens[ 1 ] );
			break;
		case "lookcast":
			// lookcast <cast key>: player 0 turns to face that cast member
			// (for the look-only name tag), or away with "lookcast none".
			p = player( 0 );
			if ( tokens.size >= 2 && isDefined( p ) )
			{
				c = maps\_cooptraining::cast( tokens[ 1 ] );
				if ( isDefined( c ) )
				{
					p botLookAt( c.origin + ( 0, 0, 50 ), 2 );
					p setPlayerAngles( vectorToAngles( c.origin + ( 0, 0, 50 ) - p getEye() ) );
				}
				else
					p setPlayerAngles( ( -80, 0, 0 ) );
				codolprintln( "CoDOL cooptest: lookcast " + tokens[ 1 ] + " defined " + isDefined( c ) );
			}
			break;
		case "bounds":
			// bounds <targetname|noteworthy>: each entity's model bounds in
			// the world (getPointInBounds -1 / +1), solid or not, and the
			// ground under its middle.
			if ( tokens.size >= 2 )
			{
				ents = getEntArray( tokens[ 1 ], "targetname" );
				if ( ents.size <= 0 )
					ents = maps\_cooptraining::by_noteworthy( tokens[ 1 ] );
				for ( j = 0; j < ents.size; j++ )
				{
					lo = ents[ j ] getPointInBounds( -1, -1, -1 );
					hi = ents[ j ] getPointInBounds( 1, 1, 1 );
					mid = ( ( lo[ 0 ] + hi[ 0 ] ) / 2, ( lo[ 1 ] + hi[ 1 ] ) / 2, 0 );
					g = bulletTrace( mid + ( 0, 0, 600 ), mid - ( 0, 0, 900 ), false, ents[ j ] );
					codolprintln( "CoDOL cooptest: bounds " + tokens[ 1 ] + " ent " + ents[ j ] getEntityNumber() + " " + ents[ j ].classname +
						" origin " + ents[ j ].origin + " min " + lo + " max " + hi + " ground " + g[ "position" ] );
				}
			}
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
		case "duel":
			// duel <role> <dist> <secs> [stand|crouch|prone] [still|strafe]
			if ( tokens.size >= 4 )
			{
				stance = "stand";
				moving = "still";
				if ( tokens.size >= 5 )
					stance = tokens[ 4 ];
				if ( tokens.size >= 6 )
					moving = tokens[ 5 ];
				level thread duel( tokens[ 1 ], int( tokens[ 2 ] ), int( tokens[ 3 ] ), stance, moving );
			}
			break;
		case "spawnat":
			// spawnat <role> <x> <y> <z>: one soldier there (placement checks)
			if ( tokens.size >= 5 )
				level thread spawn_at( tokens[ 1 ], ( float( tokens[ 2 ] ), float( tokens[ 3 ] ), float( tokens[ 4 ] ) ) );
			break;
		case "gunmodels":
			gun_models();
			break;
		default:
			// A mission script's own verbs (level.coop_mission_testcmd).
			if ( isDefined( level.coop_mission_testcmd ) )
				level thread [[ level.coop_mission_testcmd ]]( tokens );
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
	names = maps\_coopmode::spawnClassnames();
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
				maps\_coopmode::isUsableStart( s ) + " ground frac " + trace[ "fraction" ] +
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

// characters: one actor of every authored actor_ classname on the map,
// spawned through its own aitype where its spawner stands, and a log line per
// actor naming its body, head and every attachment, each with whether a
// mounted zone ships it.  A "MISSING" is a model C1 would draw as its default.
characters()
{
	seen = [];
	ents = getEntArray();
	n = 0;
	bad = 0;
	for ( i = 0; i < ents.size; i++ )
	{
		e = ents[ i ];
		if ( !isDefined( e ) || !isDefined( e.classname ) || isDefined( e.coop_template ) )
			continue;
		cls = toLower( e.classname );
		if ( getSubStr( cls, 0, 6 ) != "actor_" || cls == "actor_enemy_coop_soldier" || isDefined( seen[ cls ] ) )
			continue;
		// No coop dog species: its spawners stay inert.
		if ( isSubStr( cls, "_dog" ) )
			continue;
		seen[ cls ] = true;
		kind = spawnStruct();
		kind.health = 1000;
		ai = maps\_coopmission::spawn_class_actor( cls, undefined, kind );
		if ( !isDefined( ai ) )
		{
			codolprintln( "CoDOL cooptest: characters " + cls + " -> no actor" );
			continue;
		}
		ai forceTeleport( e.origin, ( 0, 0, 0 ) );
		ai.ignoreall = true;
		n++;
		line = "CoDOL cooptest: characters " + cls + " ent " + ai getEntityNumber() + " team " + ai.team + " body " + ai.model;
		if ( !codol_isModelMounted( ai.model ) )
		{
			line += " MISSING";
			bad++;
		}
		head = "none";
		if ( isDefined( ai.headModel ) )
			head = ai.headModel;
		line += " head " + head;
		count = ai getAttachSize();
		for ( a = 0; a < count; a++ )
		{
			m = ai getAttachModelName( a );
			line += " +" + m;
			if ( !codol_isModelMounted( m ) )
			{
				line += " MISSING";
				bad++;
			}
		}
		codolprintln( line );
	}
	codolprintln( "CoDOL cooptest: characters done, " + n + " classnames spawned, " + bad + " missing models" );
}

castHead( ai )
{
	if ( isDefined( ai.headModel ) )
		return ai.headModel;
	return "none";
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
		head = "none";
		if ( isDefined( ai.headModel ) )
			head = ai.headModel;
		codolprintln( "CoDOL cooptest:   ai " + ai getEntityNumber() + " " + ai.classname + " " + ai.coop_role + " " + ai.coop_weapon +
			" hp " + ai.health + " at " + ai.origin + " enemy " + enemy + " body " + ai.model + " head " + head );
	}

	if ( isDefined( level.coop_cast_list ) )
	{
		for ( i = 0; i < level.coop_cast_list.size; i++ )
		{
			ai = level.coop_cast_list[ i ];
			if ( !isDefined( ai ) )
				continue;
			name = ai.coop_cast;
			// A fighting ally (take estate's Ghost) is no cast member.
			if ( !isDefined( name ) )
				name = "actor";
			if ( isDefined( ai.coop_name ) )
				name += " (" + ai.coop_name + ")";
			codolprintln( "CoDOL cooptest:   cast " + name + " team " + ai.team + " alive " + isAlive( ai ) + " at " + ai.origin +
				" idle " + isDefined( ai.coop_idle_anim ) + " " + ai.classname + " body " + ai.model + " head " + castHead( ai ) );
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

hurt_enemies( amount )
{
	attacker = player( 0 );
	hurt = 0;
	for ( i = 0; i < level.coop_enemies.size; i++ )
	{
		ai = level.coop_enemies[ i ];
		if ( !isDefined( ai ) || !isAlive( ai ) || ai.health <= amount )
			continue;
		if ( isDefined( attacker ) )
			ai doDamage( amount, ai.origin + ( 0, 0, 48 ), attacker );
		else
			ai doDamage( amount, ai.origin + ( 0, 0, 48 ) );
		hurt++;
	}
	codolprintln( "CoDOL cooptest: hurt " + hurt );
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
		case "pickup":
			p giveWeapon( s.value2 );
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
		if ( isDefined( last ) && last == s && s.kind != "hits" && s.kind != "note" && s.kind != "ship" && s.kind != "weapon" )
			continue;
		last = s;
		switch ( s.kind )
		{
		case "flag":
			if ( s.value == "obstacle_course_end" )
			{
				// Mac's course, station by station (the addon's
				// obstacleTraining_* trigger boxes); a mantle the walker
				// cannot jump is reported as STUCK with what stops it.
				obstacle = [];
				obstacle[ obstacle.size ] = ( 304, -2580, 4 );
				obstacle[ obstacle.size ] = ( 306, -2659, 4 );
				obstacle[ obstacle.size ] = ( 285, -3236, 4 );
				obstacle[ obstacle.size ] = ( 367, -3569, 4 );
				obstacle[ obstacle.size ] = ( -352, -4250, 4 );
				obstacle[ obstacle.size ] = ( -1084, -4173, 4 );
				obstacle[ obstacle.size ] = ( -1106, -2824, 4 );
				for ( i = 0; i < obstacle.size; i++ )
					walk_to( p, obstacle[ i ], "obstacle " + i );
			}
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
			walk_course( p );
			break;
		case "trigger":
			// Walk into the trigger (the safety pit, station one, a deck
			// position, the debrief).
			walk_to( p, s.value.origin, "trigger " + s.value2 );
			break;
		case "weapon":
			// A player's weapon switch.
			p switchToWeapon( s.value );
			codolprintln( "CoDOL cooptest: playroute switched to " + s.value );
			break;
		case "pickup":
			// A weapon lying in the world (the range table's rifle): in
			// through the range door, up to it, look at it, press use.
			if ( p.origin[ 1 ] < -1150 )
			{
				walk_to( p, ( 1408, -1240, 4 ), "range door, outside" );
				walk_to( p, ( 1408, -1100, 4 ), "range door, inside" );
			}
			if ( isDefined( s.value ) )
			{
				walk_to( p, s.value.origin + ( 0, -40, 0 ), "pickup " + s.value2 );
				p botLookAt( s.value.origin, 1 );
				p setPlayerAngles( vectorToAngles( s.value.origin - p getEye() ) );
				wait( 0.5 );
				p codol_botPressButtons( 8, 0.5 );
				wait( 1.5 );
			}
			codolprintln( "CoDOL cooptest: playroute pickup " + s.value2 + " real " + p hasWeapon( s.value2 ) +
				", weapons " + p getWeaponsListPrimaries().size );
			if ( !p hasWeapon( s.value2 ) )
			{
				p giveWeapon( s.value2 );
				codolprintln( "CoDOL cooptest: playroute pickup " + s.value2 + " SIMULATED" );
			}
			break;
		case "damage":
			play_windows( p, s.value, s.value2 );
			break;
		case "flash":
			play_flash( p, s.value, s.value2 );
			break;
		case "use":
			walk_to( p, s.value.origin + ( 0, 0, -40 ), "use " + s.value2 );
			p codol_botPressButtons( 8, 0.3 );
			wait( 1.5 );
			if ( level.coop_training_step == s )
			{
				s.value notify( "trigger", p );
				codolprintln( "CoDOL cooptest: playroute use button did not take; use SIMULATED" );
			}
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

// The pit as a player runs it: from the hangar north through gate_cqb_out
// into the corridor, west through gate_cqb_start into the start room, over
// the start trigger, then each course trigger in the order the course runs
// (script_linkTo pf1492_1..5, the stairs 14 and 6, back south by 12 and 13),
// shooting every enemy target that is up, and out by the finish.
walk_course( p )
{
	path = [];
	if ( p.origin[ 1 ] < -989 )
	{
		path[ path.size ] = ( 140, -1100, 4 );
		path[ path.size ] = ( 140, -920, 4 );
	}
	path[ path.size ] = ( -60, -880, 4 );
	path[ path.size ] = ( -200, -880, 4 );
	path[ path.size ] = ( -144, -756, 4 );
	for ( i = 0; i < path.size; i++ )
		walk_to( p, path[ i ], "pit entrance " + i );

	order = [];
	order[ order.size ] = "1";
	order[ order.size ] = "2";
	order[ order.size ] = "3";
	order[ order.size ] = "4";
	order[ order.size ] = "5";
	order[ order.size ] = "14";
	order[ order.size ] = "6";
	order[ order.size ] = "12";
	order[ order.size ] = "13";
	triggers = getEntArray( "target_trigger", "targetname" );
	for ( k = 0; k < order.size; k++ )
	{
		for ( i = 0; i < triggers.size; i++ )
		{
			t = triggers[ i ];
			if ( !isDefined( t.script_linkTo ) || t.script_linkTo != "pf1492_" + order[ k ] )
				continue;
			walk_to( p, t.origin, "course trigger " + order[ k ] );
			wait( 1 );
			if ( isDefined( t.script_delay ) )
				wait( t.script_delay );
			for ( j = 0; j < level.coop_course_targets.size; j++ )
			{
				g = level.coop_course_targets[ j ];
				if ( !g.coop_up || g.coop_target_type != "enemy" )
					continue;
				if ( isDefined( g.script_parameters ) && g.script_parameters == "melee" )
				{
					knife_target( p, g );
					continue;
				}
				g notify( "damage", 50, p, ( 0, 0, 0 ), g.origin, "MOD_RIFLE_BULLET" );
			}
		}
	}
	codolprintln( "CoDOL cooptest: playroute course triggers done, hits " + level.coop_course_hits );
	// The finish: wait for the gates the mission opens after a good run.
	wait( 6 );
	walk_to( p, ( 528, -700, 4 ), "course finish" );
	walk_to( p, ( 528, -860, 4 ), "through gate_cqb_finish" );
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
	// What stops him: the first thing a player-sized move toward the goal
	// meets (a script_brushmodel by name, else world/clip).
	dir = vectorNormalize( ( dest[ 0 ] - p.origin[ 0 ], dest[ 1 ] - p.origin[ 1 ], 0 ) );
	from = p.origin + ( 0, 0, 30 );
	blocker = "world or clip";
	tr = bulletTrace( from, from + dir * 48, false, p );
	if ( isDefined( tr[ "entity" ] ) )
	{
		blocker = "ent " + tr[ "entity" ] getEntityNumber() + " " + tr[ "entity" ].classname;
		if ( isDefined( tr[ "entity" ].targetname ) )
			blocker += " " + tr[ "entity" ].targetname;
	}
	else if ( tr[ "fraction" ] >= 1 )
		blocker = "player clip or step (bullets pass)";
	codolprintln( "CoDOL cooptest: playroute STUCK walking to " + what + " " + dest + " at " + p.origin + ", blocked by " + blocker + "; teleporting" );
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

// colmap: one log row per y (north first), one character per cell: '.' a
// player passes east and north from the cell centre at height z (+40), '#'
// world blocks, a letter a script_brushmodel blocks (the legend follows),
// '~' no floor within 96 units.  physicsTrace is the player's collision
// (clip brushes included); bulletTrace names the entity.
col_map( x0, y0, x1, y1, z, step )
{
	legend = [];
	letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	codolprintln( "CoDOL cooptest: colmap x " + x0 + ".." + x1 + " y " + y1 + "->" + y0 + " z " + z + " step " + step );
	for ( y = y1; y >= y0; y -= step )
	{
		row = "";
		for ( x = x0; x <= x1; x += step )
		{
			c = ( x, y, z + 40 );
			floor = bulletTrace( c, c - ( 0, 0, 136 ), false, undefined );
			if ( floor[ "fraction" ] >= 1 )
			{
				row += "~";
				continue;
			}
			ch = ".";
			ends = [];
			ends[ 0 ] = c + ( step, 0, 0 );
			ends[ 1 ] = c + ( 0, step, 0 );
			for ( k = 0; k < 2 && ch == "."; k++ )
			{
				pos = physicsTrace( c, ends[ k ] );
				if ( distanceSquared( pos, ends[ k ] ) < 1 )
					continue;
				ch = "#";
				t = bulletTrace( c, ends[ k ], false, undefined );
				if ( isDefined( t[ "entity" ] ) && t[ "entity" ].classname == "script_brushmodel" )
				{
					e = t[ "entity" ];
					idx = -1;
					for ( l = 0; l < legend.size; l++ )
					{
						if ( legend[ l ] == e )
							idx = l;
					}
					if ( idx < 0 && legend.size < letters.size )
					{
						idx = legend.size;
						legend[ idx ] = e;
					}
					if ( idx >= 0 )
						ch = getSubStr( letters, idx, idx + 1 );
				}
			}
			row += ch;
		}
		codolprintln( "CoDOL colmap " + int( y ) + " " + row );
		wait( 0.05 );
	}
	for ( l = 0; l < legend.size; l++ )
	{
		e = legend[ l ];
		name = "";
		if ( isDefined( e.targetname ) )
			name = e.targetname;
		if ( isDefined( e.script_noteworthy ) )
			name += "/" + e.script_noteworthy;
		codolprintln( "CoDOL colmap legend " + getSubStr( letters, l, l + 1 ) + " = ent " + e getEntityNumber() + " " + name + " at " + e.origin );
	}
	codolprintln( "CoDOL cooptest: colmap done" );
}

// brushes: for every script_brushmodel of the name, the x / y / z range a
// grid of bullet traces through it hits that brush (its real hull, whatever
// its origin says).
brush_report( name )
{
	parts = getEntArray( name, "targetname" );
	for ( i = 0; i < parts.size; i++ )
	{
		b = parts[ i ];
		if ( b.classname != "script_brushmodel" )
			continue;
		r = brush_extent( b );
		codolprintln( "CoDOL cooptest: brush " + name + " ent " + b getEntityNumber() + " origin " + b.origin + " hull " + r );
		wait( 0.05 );
	}
}

brush_extent( b )
{
	o = b.origin;
	minx = 99999; maxx = -99999; miny = 99999; maxy = -99999; minz = 99999; maxz = -99999;
	for ( dz = -200; dz <= 200; dz += 8 )
	{
		for ( d = -256; d <= 256; d += 8 )
		{
			// along y at x = o + d, and along x at y = o + d
			t = bulletTrace( ( o[ 0 ] + d, o[ 1 ] - 300, o[ 2 ] + dz ), ( o[ 0 ] + d, o[ 1 ] + 300, o[ 2 ] + dz ), false, undefined );
			if ( isDefined( t[ "entity" ] ) && t[ "entity" ] == b )
			{
				p = t[ "position" ];
				if ( p[ 0 ] < minx ) minx = p[ 0 ];
				if ( p[ 0 ] > maxx ) maxx = p[ 0 ];
				if ( p[ 1 ] < miny ) miny = p[ 1 ];
				if ( p[ 2 ] < minz ) minz = p[ 2 ];
				if ( p[ 2 ] > maxz ) maxz = p[ 2 ];
			}
			t = bulletTrace( ( o[ 0 ] - 300, o[ 1 ] + d, o[ 2 ] + dz ), ( o[ 0 ] + 300, o[ 1 ] + d, o[ 2 ] + dz ), false, undefined );
			if ( isDefined( t[ "entity" ] ) && t[ "entity" ] == b )
			{
				p = t[ "position" ];
				if ( p[ 0 ] < minx ) minx = p[ 0 ];
				if ( p[ 1 ] < miny ) miny = p[ 1 ];
				if ( p[ 1 ] > maxy ) maxy = p[ 1 ];
				if ( p[ 2 ] < minz ) minz = p[ 2 ];
				if ( p[ 2 ] > maxz ) maxz = p[ 2 ];
			}
		}
	}
	if ( minx > maxx && miny > maxy )
		return "not hit by any trace";
	return "x " + minx + ".." + maxx + " y " + miny + ".." + maxy + " z " + minz + ".." + maxz;
}

// A real knife: walk up to the target, look at it and press melee (C1
// usercmd button 4), up to four times; never a faked damage notify.  The
// melee target's own log line (_coopcourse) shows what the engine delivered.
knife_target( p, g )
{
	wait( 1 );
	// The pit's melee target stands up as a panel across the upstairs
	// walkway at x 164..172, y 1224..1280 (MEASURED with colmap; its pivot,
	// 168 1214, is in the wall corner), so it is knifed from the west, facing
	// east, at the walkway's height.
	spot = ( 128, 1260, 150 );
	aimAt = ( 168, 1260, 200 );
	for ( n = 0; n < 4 && g.coop_up; n++ )
	{
		walk_to( p, spot, "melee target" );
		aim = aimAt;
		p botLookAt( aim, 3 );
		p setPlayerAngles( vectorToAngles( aim - p getEye() ) );
		wait( 0.6 );
		p codol_botPressButtons( 4, 0.2 );
		wait( 1.2 );
		codolprintln( "CoDOL cooptest: knife " + n + " at melee target from " + p.origin + " (" + int( distance( p.origin, g.origin ) ) +
			" units), target up " + g.coop_up );
	}
}

// The demolition windows as a player does them: from the safety pit, look
// at each window not yet hit and throw a frag (button 0x4000) or fire the
// launcher (button 1).  A window the throw misses is reported and then
// triggered (SIMULATED) so the run goes on.
play_windows( p, trigs, what )
{
	for ( i = 0; i < trigs.size; i++ )
	{
		t = trigs[ i ];
		if ( t.coop_hit )
			continue;
		aim = t.origin;
		p botLookAt( aim, 3 );
		p setPlayerAngles( vectorToAngles( aim - p getEye() ) + ( -8, 0, 0 ) );
		wait( 0.6 );
		if ( isSubStr( what, "frag" ) )
			p codol_botPressButtons( 16384, 0.4 );
		else
			p codol_botPressButtons( 1, 0.1 );
		wait( 3.5 );
		codolprintln( "CoDOL cooptest: playroute " + what + " at " + t getEntityNumber() + " real hit " + t.coop_hit );
	}
	for ( i = 0; i < trigs.size; i++ )
	{
		if ( !trigs[ i ].coop_hit )
		{
			trigs[ i ] notify( "trigger", p );
			codolprintln( "CoDOL cooptest: playroute " + what + " " + trigs[ i ] getEntityNumber() + " SIMULATED" );
		}
	}
}

// A flashbang into the room (button 0x8000), else SIMULATED.
play_flash( p, volume, what )
{
	aim = volume.origin;
	p botLookAt( aim, 3 );
	p setPlayerAngles( vectorToAngles( aim - p getEye() ) );
	wait( 0.6 );
	p codol_botPressButtons( 32768, 0.4 );
	wait( 3.5 );
	codolprintln( "CoDOL cooptest: playroute flash into " + what + " real " + level.coop_training_flashed );
	if ( !level.coop_training_flashed )
	{
		level.coop_training_flashed = true;
		codolprintln( "CoDOL cooptest: playroute flash " + what + " SIMULATED" );
	}
}

pickup_test( classname )
{
	p = player( 0 );
	if ( !isDefined( p ) )
		return;
	before = p getWeaponsListPrimaries();
	spot = p.origin + anglesToForward( p.angles ) * 64 + ( 0, 0, 8 );
	item = spawn( classname, spot );
	codolprintln( "CoDOL cooptest: pickup " + classname + " defined " + isDefined( item ) + "; primaries before " + before.size );
	wait( 0.5 );
	walk_to( p, spot, "pickup " + classname );
	wait( 1 );
	p codol_botPressButtons( 8, 0.5 );
	wait( 1.5 );
	after = p getWeaponsListPrimaries();
	text = "";
	for ( i = 0; i < after.size; i++ )
		text += " " + after[ i ];
	codolprintln( "CoDOL cooptest: pickup " + classname + " primaries after " + after.size + ":" + text + " (item still there " + isDefined( item ) + ")" );
}

// --------------------------------------------------------------------------
// duel: one soldier of <role> against player 0, standing in the open at
// <dist> units, for <secs> seconds or until the player dies.  Measures what
// the player receives (every hit through _coopmode's damage watch: raw and
// scaled damage, time) and what the soldier fires (the animscript's shot
// counters).  The mission's own spawns are held off for the duel
// (level.coop_max_alive 0) and every other enemy is removed first.
// --------------------------------------------------------------------------
duel( role, dist, secs, stance, moving )
{
	// The mission's own spawns stay off from the first duel on.
	level.coop_max_alive = 0;
	kill_enemies( 999 );
	p = undefined;
	for ( t = 0; t < 200; t++ )
	{
		p = player( 0 );
		if ( isDefined( p ) && isAlive( p ) )
			break;
		wait( 0.1 );
	}
	if ( !isDefined( p ) || !isAlive( p ) )
	{
		codolprintln( "CoDOL duel: no live player 0" );
		return;
	}
	wait( 0.5 );

	spot = duel_find_spot( p, dist );
	if ( !isDefined( spot ) )
	{
		// Somewhere in the open: the mission's authored enemy spawners
		// stand outdoors; try the player on each until one has a spot.
		cands = getEntArray( "pmc_spawner", "targetname" );
		if ( isDefined( level.coop_duel_starts ) )
			cands = level.coop_duel_starts;
		for ( i = 0; i < cands.size && !isDefined( spot ); i++ )
		{
			p setOrigin( cands[ i ].origin + ( 0, 0, 4 ) );
			wait( 0.1 );
			spot = duel_find_spot( p, dist );
		}
	}
	if ( !isDefined( spot ) )
	{
		codolprintln( "CoDOL duel: no open spot " + dist + " units from player 0 at " + p.origin );
		return;
	}
	p.health = p.maxhealth;
	if ( stance != "stand" )
		p setStance( stance );

	fake = spawnStruct();
	fake.classname = "actor_enemy_favela_" + role + "_so";
	fake.origin = spot;
	fake.angles = vectorToAngles( p.origin - spot );
	fake.coop_no_hunt = true;
	level.coop_max_alive = level.coop_alive + 1;
	ai = maps\_coopmission::spawn_soldier( fake );
	level.coop_max_alive = 0;
	if ( !isDefined( ai ) )
	{
		codolprintln( "CoDOL duel: spawn refused" );
		return;
	}
	// The clock starts when the soldier has the player as his enemy (the
	// native sight test is his, not this script's).
	acquired = false;
	for ( t = 0; t < 80; t++ )
	{
		if ( isDefined( ai.enemy ) )
		{
			acquired = true;
			break;
		}
		wait( 0.1 );
	}
	if ( !acquired )
	{
		codolprintln( "CoDOL duel: soldier at " + ai.origin + " never acquired player 0; skipped" );
		ai doDamage( ai.health + 100, ai.origin );
		return;
	}
	p.health = p.maxhealth;
	gun = "none";
	if ( isDefined( ai.coop_weapon_model ) )
		gun = ai.coop_weapon_model;
	codolprintln( "CoDOL duel: start role " + role + " weapon " + ai.coop_weapon + " gun model " + gun + " attached " +
		ai getAttachSize() + " dist " + int( distance( ai.origin, p.origin ) ) + " stance " + stance + " " + moving +
		" player at " + p.origin + " soldier ent " + ai getEntityNumber() + " at " + ai.origin );

	level.coop_duel = spawnStruct();
	level.coop_duel.player = p;
	level.coop_duel.hits = 0;
	level.coop_duel.raw = 0;
	level.coop_duel.dealt = 0;
	level.coop_duel.first_hit = 0;
	level.coop_duel.start = getTime();
	level.coop_duel.deaths = 0;
	level.coop_duel.death_time = 0;
	level.coop_damage_watch = ::duel_damage;
	ai.coop_stat_shots = 0;
	ai.coop_stat_bursts = 0;
	ai.coop_stat_aimed_hits = 0;
	ai.coop_stat_miss_dist = 0;
	ai.coop_stat_miss_ground = 0;

	if ( moving == "strafe" )
		p thread duel_strafe( ai );

	start = getTime();
	while ( getTime() - start < secs * 1000 && isAlive( ai ) && level.coop_duel.deaths == 0 )
		wait( 0.05 );
	p notify( "duel_over" );
	elapsed = ( getTime() - start ) / 1000;
	if ( level.coop_duel.deaths > 0 )
		elapsed = ( level.coop_duel.death_time - start ) / 1000;
	if ( elapsed <= 0 )
		elapsed = 0.05;

	shots = "?";
	bursts = "?";
	aimed = "?";
	misses = "?";
	if ( isDefined( ai ) && isDefined( ai.coop_stat_shots ) )
	{
		shots = ai.coop_stat_shots;
		bursts = ai.coop_stat_bursts;
		aimed = ai.coop_stat_aimed_hits;
		missed = shots - aimed;
		misses = missed + " (" + ai.coop_stat_miss_ground + " into the ground by his feet";
		if ( missed > 0 )
			misses += ", aimed " + int( ai.coop_stat_miss_dist / missed ) + " units from his chest on average";
		misses += ")";
	}
	first = "none";
	if ( level.coop_duel.first_hit > 0 )
		first = ( level.coop_duel.first_hit - start ) / 1000;
	codolprintln( "CoDOL duel: result role " + role + " dist " + dist + " " + stance + " " + moving + " secs " + elapsed +
		" hits " + level.coop_duel.hits + " raw " + level.coop_duel.raw + " dealt " + level.coop_duel.dealt +
		" dps " + ( level.coop_duel.dealt / elapsed ) + " first_hit " + first + " died " + level.coop_duel.deaths +
		" shots " + shots + " bursts " + bursts + " aimed_hits " + aimed + " misses " + misses + " health_left " + p.health );
	level.coop_damage_watch = undefined;
	level.coop_duel = undefined;
	if ( isDefined( ai ) && isAlive( ai ) )
		ai doDamage( ai.health + 100, ai.origin );
	if ( stance != "stand" && isAlive( p ) )
		p setStance( "stand" );
}

duel_damage( victim, raw, dealt, eAttacker, sWeapon, sHitLoc )
{
	d = level.coop_duel;
	if ( !isDefined( d ) || victim != d.player )
		return;
	d.hits++;
	d.raw += raw;
	d.dealt += dealt;
	if ( d.first_hit == 0 )
		d.first_hit = getTime();
	attacker = "world";
	if ( isDefined( eAttacker ) )
		attacker = "ent " + eAttacker getEntityNumber();
	codolprintln( "CoDOL duel: hit t " + ( getTime() - d.start ) + " raw " + raw + " dealt " + dealt + " from " + attacker +
		" weapon " + sWeapon + " loc " + sHitLoc + " health before " + victim.health );
	if ( dealt >= victim.health )
	{
		d.deaths++;
		d.death_time = getTime();
	}
}

// Side to side across the soldier's line, 96 units each way.
duel_strafe( ai )
{
	self endon( "duel_over" );
	self endon( "death" );
	self endon( "disconnect" );
	base = self.origin;
	dir = vectorNormalize( ai.origin - base );
	side = ( 0 - dir[ 1 ], dir[ 0 ], 0 );
	sign = 1;
	for ( ;; )
	{
		self botMoveTo( base + side * ( 96 * sign ) );
		wait( 1.2 );
		sign = 0 - sign;
	}
}

// The first spot <dist> from the player, 16 headings, where a soldier has
// floor under him, room to stand and a clear line to the player's chest.
duel_find_spot( p, dist )
{
	eye = p.origin + ( 0, 0, 48 );
	for ( k = 0; k < 16; k++ )
	{
		dir = anglesToForward( ( 0, k * 22.5, 0 ) );
		cand = p.origin + dir * dist;
		down = bulletTrace( cand + ( 0, 0, 64 ), cand - ( 0, 0, 256 ), false, undefined );
		if ( down[ "fraction" ] >= 1 )
			continue;
		spot = down[ "position" ] + ( 0, 0, 1 );
		if ( abs_f( spot[ 2 ] - p.origin[ 2 ] ) > 96 )
			continue;
		if ( !bulletTracePassed( eye, spot + ( 0, 0, 56 ), false, p ) )
			continue;
		// The soldier's own sight line: capsule centre to capsule centre.
		if ( !bulletTracePassed( spot + ( 0, 0, 36 ), p.origin + ( 0, 0, 35 ), false, p ) )
			continue;
		if ( !bulletTracePassed( spot + ( 0, 0, 8 ), spot + ( 0, 0, 72 ), false, undefined ) )
			continue;
		return spot;
	}
	return undefined;
}

abs_f( v )
{
	if ( v < 0 )
		return 0 - v;
	return v;
}

spawn_at( role, origin )
{
	fake = spawnStruct();
	fake.classname = "actor_enemy_favela_" + role + "_so";
	fake.origin = origin;
	fake.angles = ( 0, 0, 0 );
	fake.coop_no_hunt = true;
	old = level.coop_max_alive;
	level.coop_max_alive = level.coop_alive + 1;
	ai = maps\_coopmission::spawn_soldier( fake );
	level.coop_max_alive = old;
	if ( isDefined( ai ) )
		codolprintln( "CoDOL cooptest: spawnat " + origin + " -> ent " + ai getEntityNumber() + " at " + ai.origin );
	else
		codolprintln( "CoDOL cooptest: spawnat " + origin + " refused" );
}

// gunmodels: every live soldier's weapon and attached gun model.
gun_models()
{
	n = 0;
	for ( i = 0; i < level.coop_enemies.size; i++ )
	{
		ai = level.coop_enemies[ i ];
		if ( !isDefined( ai ) || !isAlive( ai ) )
			continue;
		gun = "none";
		if ( isDefined( ai.coop_weapon_model ) )
			gun = ai.coop_weapon_model;
		codolprintln( "CoDOL cooptest: soldier ent " + ai getEntityNumber() + " weapon " + ai.coop_weapon + " gun " + gun +
			" attached " + ai getAttachSize() );
		n++;
	}
	codolprintln( "CoDOL cooptest: gunmodels " + n + " soldiers" );
}
