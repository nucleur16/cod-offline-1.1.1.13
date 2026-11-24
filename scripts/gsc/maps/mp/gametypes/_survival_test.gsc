// survival probe harness (operator / developer only).
//
// Inert unless the server is started with +set scr_survival_test 1.  It then
// polls scr_survival_test_cmd (set over rcon) for one command at a time, the
// coop harness's pattern (maps/_cooptest.gsc):
//   addbot          addTestClient(); the bot spawns like a player
//   status          one log line of the match and every live enemy
//   killenemies     kill every live enemy, player 0 the attacker
//   godmode <n>     keep player n's health topped up
//   wave <n>        jump the next wave to n
//   chopper <ref>   a chopper boss now (chopper, missile_chopper)
//   jug <ref>       a juggernaut drop now (jug_regular, jug_riotshield, ...)
//   streak <n> <s>  give player n the killstreak s
//   join            every human joins the survivors and spawns (also at every
//                   connect with scr_survival_test_join 1: a headless client
//                   answers no class menu and would stay spectating)
//   tour <0|1>      player 0 runs a fixed chain of walkable waypoints, pausing
//   log <0|1>       one "SVT" line a second per enemy, helicopter and player
//   bench <s> <n..> log, tour and godmode, then the waves n.., s seconds each
//                   (scr_survival_test_bench runs it once from the start)
// Purchases are maps\_survival_kiosk's scr_survival_buy.

main()
{
	if ( getDvarInt( "scr_survival_test" ) == 0 )
		return;
	setDvar( "scr_survival_test_cmd", "" );
	codolprintln( "CoDOL survivaltest: harness active" );
	if ( getDvarInt( "scr_survival_test_join" ) )
		level thread join_all();
	bench = getDvar( "scr_survival_test_bench" );
	if ( bench != "" )
		level thread bench( strTok( bench, " " ), 0 );
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_survival_test_cmd" );
		if ( request == "" )
			continue;
		setDvar( "scr_survival_test_cmd", "" );
		tokens = strTok( request, " " );
		if ( !tokens.size )
			continue;
		switch ( tokens[ 0 ] )
		{
		case "addbot":
			bot = addTestClient();
			codolprintln( "CoDOL survivaltest: addtestclient" );
			if ( isDefined( bot ) )
				bot thread bot_join();
			break;
		case "status":
			status();
			break;
		case "killenemies":
			players = getPlayers();
			enemies = maps\mp\gametypes\_survival_ai::living_enemies();
			for ( i = 0; i < enemies.size; i++ )
			{
				if ( players.size )
					enemies[ i ] doDamage( enemies[ i ].health + 1000, enemies[ i ].origin, players[ 0 ] );
				else
					enemies[ i ] doDamage( enemies[ i ].health + 1000, enemies[ i ].origin );
			}
			choppers = 0;
			if ( players.size )
				choppers = maps\mp\gametypes\_survival_vehicles::emp_choppers( players[ 0 ] );
			codolprintln( "CoDOL survivaltest: killed " + enemies.size + " and " + choppers + " choppers" );
			break;
		case "godmode":
			players = getPlayers();
			n = 0;
			if ( tokens.size > 1 )
				n = int( tokens[ 1 ] );
			if ( n < players.size )
				players[ n ] thread godmode();
			break;
		case "chopper":
			ref = "chopper";
			if ( tokens.size > 1 )
				ref = tokens[ 1 ];
			maps\mp\gametypes\_survival_vehicles::spawn_chopper_boss( ref, 1.0 );
			break;
		case "jug":
			ref = "jug_regular";
			if ( tokens.size > 1 )
				ref = tokens[ 1 ];
			refs = [];
			refs[ 0 ] = ref;
			level thread maps\mp\gametypes\_survival_ai::spawn_bosses( refs, 1.0 );
			break;
		case "streak":
			players = getPlayers();
			if ( tokens.size > 2 && int( tokens[ 1 ] ) < players.size )
				codolprintln( "CoDOL survivaltest: streak " + tokens[ 2 ] + " " +
					players[ int( tokens[ 1 ] ) ] maps\mp\gametypes\_survival_kiosk::give_streak( tokens[ 2 ] ) );
			break;
		case "hurt":
			players = getPlayers();
			if ( tokens.size > 2 && int( tokens[ 1 ] ) < players.size )
			{
				p = players[ int( tokens[ 1 ] ) ];
				p doDamage( int( tokens[ 2 ] ), p.origin + ( 0, 0, 40 ) );
				wait( 0.1 );
				down = isDefined( p.laststand ) && p.laststand;
				codolprintln( "CoDOL survivaltest: hurt " + p.name + " " + tokens[ 2 ] + ": hp " + p.health + " laststand " + down + " alive " + isAlive( p ) );
			}
			break;
		case "class":
			players = getPlayers();
			if ( tokens.size > 2 && int( tokens[ 1 ] ) < players.size )
			{
				p = players[ int( tokens[ 1 ] ) ];
				before = p getCurrentWeapon();
				// A training bot's loadout is its bot class (mp/botclasstable):
				// the class index picks it, as a player's pick picks his class.
				if ( isDefined( p.codolCombatBot ) && p.codolCombatBot )
				{
					p.codolBotClass = level.classMap[ tokens[ 2 ] ] % 9;
					p.pers[ "codolBotClass" ] = p.codolBotClass;
				}
				p [[level.class]]( tokens[ 2 ] );
				waittillframeend;
				codolprintln( "CoDOL survivaltest: " + p.name + " class " + p.pers[ "class" ] + " weapon " + before + " -> " + p getCurrentWeapon() );
			}
			break;
		case "squad":
			if ( tokens.size > 2 )
			{
				ais = maps\mp\gametypes\_survival_squad::spawn_squad( tokens[ 1 ], int( tokens[ 2 ] ), 1.0 );
				codolprintln( "CoDOL survivaltest: squad " + tokens[ 1 ] + ": " + ais.size );
			}
			break;
		case "spawn":
			if ( tokens.size > 1 )
			{
				loc = maps\mp\gametypes\_survival_squad::farthest_location();
				ai = maps\mp\gametypes\_survival_ai::spawn_type( tokens[ 1 ], loc.origin, loc.angles, 1.0 );
				codolprintln( "CoDOL survivaltest: spawned " + tokens[ 1 ] + ": " + isDefined( ai ) );
			}
			break;
		case "bring":
			players = getPlayers();
			enemies = maps\mp\gametypes\_survival_ai::living_enemies();
			for ( i = 0; i < enemies.size && players.size; i++ )
				enemies[ i ] forceTeleport( players[ 0 ].origin + ( 250 + 40 * i, 0, 8 ), ( 0, 180, 0 ) );
			codolprintln( "CoDOL survivaltest: brought " + enemies.size );
			break;
		case "wave":
			if ( tokens.size > 1 )
				level.survival_wave_num = int( tokens[ 1 ] ) - 1;
			break;
		case "join":
			level thread join_all();
			break;
		case "tour":
			players = getPlayers();
			if ( players.size )
			{
				players[ 0 ] notify( "svt_tour" );
				if ( tokens.size < 2 || int( tokens[ 1 ] ) )
					players[ 0 ] thread tour();
			}
			break;
		case "log":
			level notify( "svt_log" );
			if ( tokens.size < 2 || int( tokens[ 1 ] ) )
				level thread svt_log();
			break;
		case "bench":
			level thread bench( tokens, 1 );
			break;
		}
	}
}

// A test client joins as the MP training bots do (_codol_bots: a bot class from mp/botclasstable, autoassign, class0); the
// survival gametype puts it on allies.
bot_join()
{
	self.codolCombatBot = true;
	self.codolBotClass = maps\mp\gametypes\_codol_bots::classForDifficulty( 0 );
	self.pers[ "codolCombatBot" ] = true;
	self.pers[ "codolBotClass" ] = self.codolBotClass;
	self maps\mp\gametypes\_codol_bots::enterGame();
	codolprintln( "CoDOL survivaltest: " + self.name + " joined " + self.pers[ "team" ] + " as " + self.pers[ "class" ] );
}

godmode()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		if ( isAlive( self ) )
			self.health = self.maxhealth;
		wait( 0.05 );
	}
}

status()
{
	enemies = maps\mp\gametypes\_survival_ai::living_enemies();
	codolprintln( "CoDOL survivaltest: wave " + level.survival_wave_num + " alive " + level.survival_alive +
		" spawned " + level.survival_spawned + " killed " + level.survival_killed + " failures " + level.survival_spawn_failures );
	for ( i = 0; i < enemies.size; i++ )
	{
		e = enemies[ i ];
		enemy = "none";
		if ( isDefined( e.enemy ) )
			enemy = "" + e.enemy getEntityNumber();
		head = "none";
		if ( isDefined( e.headModel ) )
			head = e.headModel;
		weapon = "";
		if ( isDefined( e.coop_weapon ) )
			weapon = e.coop_weapon;
		codolprintln( "CoDOL survivaltest:   ai " + e getEntityNumber() + " " + e.survival_ref + " " + weapon + " hp " + e.health +
			" at " + e.origin + " enemy " + enemy + " body " + e.model + " head " + head );
	}
	if ( isDefined( level.survival_choppers ) )
	{
		for ( i = 0; i < level.survival_choppers.size; i++ )
		{
			c = level.survival_choppers[ i ];
			if ( isDefined( c ) )
				codolprintln( "CoDOL survivaltest:   chopper " + c.survival_ref + " at " + c.origin + " damage " + c.damageTaken + "/" + c.maxhealth );
		}
	}
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		codolprintln( "CoDOL survivaltest:   player " + i + " " + p.sessionstate + " hp " + p.health + " at " + p.origin +
			" credits " + p.survival_credit + " kills " + p.kills );
	}
}

// --------------------------------------------------------------------------
// The headless player: a team and a class without the client's menus.
// --------------------------------------------------------------------------

join_all()
{
	level notify( "svt_join" );
	level endon( "svt_join" );
	for ( ;; )
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( isDefined( p.svt_joining ) || isDefined( p.pers[ "codolCombatBot" ] ) )
				continue;
			p.svt_joining = true;
			p thread join_one();
		}
		wait( 1 );
	}
}

join_one()
{
	self endon( "disconnect" );
	wait( 3 );
	for ( tries = 0; tries < 6 && !( isAlive( self ) && self.sessionstate == "playing" ); tries++ )
	{
		if ( !isDefined( self.pers[ "team" ] ) || self.pers[ "team" ] != "allies" )
			self [[level.autoassign]]();
		wait( 0.25 );
		self.selectedClass = true;
		self [[level.class]]( "class0" );
		wait( 2 );
	}
	codolprintln( "CoDOL survivaltest: " + self.name + " team " + self.pers[ "team" ] + " state " + self.sessionstate + " at " + self.origin );
}

// --------------------------------------------------------------------------
// The tour: the player runs a chain of walkable waypoints at 190 units a
// second, four legs at a time, then stands for 8 s. The chain is the same on
// every run of a map.
// --------------------------------------------------------------------------

tour_chain( origin )
{
	chain = [];
	points = level.survival_waypoints;
	if ( !isDefined( points ) || !points.size )
		return chain;
	used = [];
	cur = origin;
	trail = origin;
	for ( n = 0; n < 48; n++ )
	{
		best = -1;
		bestScore = 0;
		for ( i = 0; i < points.size; i++ )
		{
			if ( isDefined( used[ i ] ) )
				continue;
			p = points[ i ];
			d = distance( p, cur );
			if ( d < 96 || d > 420 || abs( p[ 2 ] - cur[ 2 ] ) > 72 )
				continue;
			score = distance( p, trail ) - 0.3 * d;
			if ( best >= 0 && score <= bestScore )
				continue;
			if ( !tour_clear( cur, p ) )
				continue;
			best = i;
			bestScore = score;
		}
		if ( best < 0 )
			break;
		used[ best ] = true;
		chain[ chain.size ] = points[ best ];
		cur = points[ best ];
		trail = trail * 0.6 + cur * 0.4;
		if ( n % 4 == 3 )
			wait( 0.05 );
	}
	return chain;
}

tour_clear( a, b )
{
	end = playerPhysicsTrace( a + ( 0, 0, 24 ), b + ( 0, 0, 24 ) );
	return distance( end, b + ( 0, 0, 24 ) ) < 8;
}

tour()
{
	self endon( "disconnect" );
	self notify( "svt_tour" );
	self endon( "svt_tour" );
	chain = tour_chain( self.origin );
	codolprintln( "CoDOL survivaltest: tour of " + chain.size + " points from " + self.origin );
	if ( !chain.size )
		return;
	dir = 1;
	i = 0;
	legs = 0;
	for ( ;; )
	{
		if ( !isAlive( self ) )
		{
			wait( 1 );
			continue;
		}
		self glide_to( chain[ i ] );
		legs++;
		if ( legs % 4 == 0 )
			wait( 8 );
		if ( i + dir < 0 || i + dir >= chain.size )
			dir = 0 - dir;
		i += dir;
		if ( i < 0 || i >= chain.size )
			i = 0;
	}
}

glide_to( goal )
{
	self endon( "disconnect" );
	from = self.origin;
	v = goal - from;
	d = length( v );
	if ( d < 1 )
		return;
	steps = int( d / 9.5 ) + 1;
	self setPlayerAngles( ( 0, vectorToYaw( v ), 0 ) );
	for ( s = 1; s <= steps && isAlive( self ); s++ )
	{
		p = from + v * ( s / steps );
		ground = playerPhysicsTrace( p + ( 0, 0, 40 ), p - ( 0, 0, 120 ) );
		self setOrigin( ground );
		wait( 0.05 );
	}
}

// --------------------------------------------------------------------------
// The log. Every line starts with SVT:
//   A  an enemy:    t wave ent ref script dist moved enemy visible shots aimed
//                   grenades badpath stalled relocated at
//   H  a chopper:   t wave ent ref dist2d dist sight yaw-error moved shots
//                   rockets moves damage at
//   P  player 0:    t hp at
//   D  damage to a player, per source, at the end of a wave
// --------------------------------------------------------------------------

svt_log()
{
	level endon( "svt_log" );
	level.svt_hits = [];
	level.svt_taken = [];
	if ( !isDefined( level.svt_player_damage ) )
	{
		level.svt_player_damage = level.callbackPlayerDamage;
		level.callbackPlayerDamage = ::svt_player_damage;
	}
	for ( ;; )
	{
		now = getTime();
		players = getPlayers();
		if ( players.size )
			codolprintln( "SVT P t" + now + " w" + level.survival_wave_num + " hp " + players[ 0 ].health + " at " + players[ 0 ].origin );
		enemies = maps\mp\gametypes\_survival_ai::living_enemies();
		for ( i = 0; i < enemies.size; i++ )
			enemies[ i ] svt_enemy( now );
		for ( i = 0; i < level.survival_choppers.size; i++ )
		{
			c = level.survival_choppers[ i ];
			if ( isDefined( c ) && !( isDefined( c.survival_dead ) && c.survival_dead ) )
				c svt_chopper( now );
		}
		wait( 1 );
	}
}

svt_enemy( now )
{
	if ( !isDefined( self.svt_last ) )
	{
		self.svt_last = self.origin;
		self.svt_grenades = 0;
		// the soldier_combat probe counters
		self.coop_stat_shots = 0;
		self.coop_stat_aimed_hits = 0;
		self.coop_stat_bursts = 0;
		self.coop_stat_miss_dist = 0;
		self.coop_stat_miss_ground = 0;
	}
	if ( isDefined( self.coop_grenades ) )
	{
		if ( isDefined( self.svt_grenade_count ) && self.coop_grenades < self.svt_grenade_count )
			self.svt_grenades += self.svt_grenade_count - self.coop_grenades;
		self.svt_grenade_count = self.coop_grenades;
	}
	target = maps\mp\gametypes\_survival_ai::closest_player( self.origin );
	d = -1;
	sees = 0;
	if ( isDefined( target ) )
	{
		d = int( distance( self.origin, target.origin ) );
		sees = sightTracePassed( self.origin + ( 0, 0, 60 ), target getEye(), false, self );
	}
	move = int( distance( self.origin, self.svt_last ) );
	self.svt_last = self.origin;
	script = "none";
	if ( isDefined( self.a ) && isDefined( self.a.script ) )
		script = self.a.script;
	else if ( isDefined( self.a ) && isDefined( self.a.movement ) )
		script = self.a.movement;
	enemy = -1;
	if ( isDefined( self.enemy ) )
		enemy = self.enemy getEntityNumber();
	badpath = 0;
	if ( isDefined( self.survival_badpath ) )
		badpath = self.survival_badpath;
	moved = 0;
	if ( isDefined( self.survival_relocations ) )
		moved = self.survival_relocations;
	codolprintln( "SVT A t" + now + " w" + level.survival_wave_num + " e" + self getEntityNumber() + " " + self.survival_ref + " " + script +
		" d " + d + " mv " + move + " en " + enemy + " vis " + sees + " sh " + self.coop_stat_shots + " ah " + self.coop_stat_aimed_hits +
		" gr " + self.svt_grenades + " bp " + badpath + " stl " + isDefined( self.survival_stalled ) + " rl " + moved + " at " + self.origin );
}

svt_chopper( now )
{
	if ( !isDefined( self.svt_last ) )
		self.svt_last = self.origin;
	target = maps\mp\gametypes\_survival_ai::closest_player( self.origin );
	d = -1;
	d2 = -1;
	sees = 0;
	yaw = 0;
	if ( isDefined( target ) )
	{
		d = int( distance( self.origin, target.origin ) );
		d2 = int( distance2d( self.origin, target.origin ) );
		sees = sightTracePassed( self.origin + ( 0, 0, -64 ), target getEye(), false, self );
		yaw = int( abs( angleClamp180( vectorToYaw( target.origin - self.origin ) - self.angles[ 1 ] ) ) );
	}
	move = int( distance( self.origin, self.svt_last ) );
	self.svt_last = self.origin;
	codolprintln( "SVT H t" + now + " w" + level.survival_wave_num + " e" + self getEntityNumber() + " " + self.survival_ref + " d2 " + d2 + " d " + d +
		" los " + sees + " yaw " + yaw + " mv " + move + " sh " + svt_count( self.survival_shots ) + " rk " + svt_count( self.survival_rockets ) +
		" nd " + svt_count( self.survival_moves ) + " dmg " + self.damageTaken + " at " + self.origin );
}

svt_count( n )
{
	if ( isDefined( n ) )
		return n;
	return 0;
}

// Runs the mode's own handler, then books the health it took per source.
svt_player_damage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime )
{
	source = "world";
	if ( isDefined( eAttacker ) && isDefined( eAttacker.survival_ref ) )
		source = eAttacker.survival_ref;
	else if ( isDefined( eAttacker ) && isPlayer( eAttacker ) )
		source = "player";
	else if ( isDefined( sWeapon ) && sWeapon != "" )
		source = "weapon:" + sWeapon;
	if ( isDefined( sMeansOfDeath ) )
		source = source + "/" + sMeansOfDeath;
	if ( isDefined( level.svt_god ) )
	{
		// never the kill: the mode's own arithmetic, on a full player
		if ( isAlive( self ) )
			self.health = self.maxhealth;
		if ( iDamage >= self.maxhealth )
			iDamage = self.maxhealth - 1;
	}
	before = self.health;
	self [[level.svt_player_damage]]( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime );
	taken = before - self.health;
	if ( !isDefined( level.svt_hits[ source ] ) )
	{
		level.svt_hits[ source ] = 0;
		level.svt_taken[ source ] = 0;
	}
	level.svt_hits[ source ]++;
	level.svt_taken[ source ] += taken;
	if ( isDefined( level.svt_god ) && isAlive( self ) && !( isDefined( self.laststand ) && self.laststand ) )
		self.health = self.maxhealth;
}

svt_damage_report()
{
	keys = getArrayKeys( level.svt_hits );
	for ( i = 0; i < keys.size; i++ )
		codolprintln( "SVT D w" + level.survival_wave_num + " " + keys[ i ] + " hits " + level.svt_hits[ keys[ i ] ] + " damage " + level.svt_taken[ keys[ i ] ] );
	level.svt_hits = [];
	level.svt_taken = [];
}

// bench <seconds> <wave..>: each wave for that long, its boss_delay waived,
// then everything is killed and the next one comes after a short intermission.
bench( tokens, first )
{
	level notify( "svt_bench" );
	level endon( "svt_bench" );
	if ( tokens.size < first + 2 )
		return;
	seconds = int( tokens[ first ] );
	level.survival_intermission = 5;
	level.survival_wave_num = int( tokens[ first + 1 ] );
	level.svt_god = true;
	while ( maps\mp\gametypes\_survival_utility::living_players().size <= 0 )
		wait( 0.25 );
	player = maps\mp\gametypes\_survival_utility::living_players()[ 0 ];
	player thread godmode();
	level thread svt_log();
	for ( i = first + 1; i < tokens.size; i++ )
	{
		num = int( tokens[ i ] );
		w = maps\mp\gametypes\_survival_ai::get_wave( num );
		w.boss_delay = 0;
		for ( t = 0; t < 400; t++ )
		{
			if ( level.survival_wave_num == num && !level.survival_in_intermission &&
			     maps\mp\gametypes\_survival_ai::living_enemies().size > 0 )
				break;
			wait( 0.25 );
		}
		level.svt_hits = [];
		level.svt_taken = [];
		codolprintln( "SVT W t" + getTime() + " start " + num );
		player thread tour();
		wait( seconds );
		player notify( "svt_tour" );
		codolprintln( "SVT W t" + getTime() + " end " + num );
		svt_damage_report();
		if ( i + 1 < tokens.size )
			level.survival_wave_num = int( tokens[ i + 1 ] ) - 1;
		for ( t = 0; t < 60; t++ )
		{
			enemies = maps\mp\gametypes\_survival_ai::living_enemies();
			for ( k = 0; k < enemies.size; k++ )
			{
				if ( isDefined( enemies[ k ] ) && isAlive( enemies[ k ] ) )
					enemies[ k ] doDamage( enemies[ k ].health + 100000, enemies[ k ].origin, player );
			}
			maps\mp\gametypes\_survival_vehicles::emp_choppers( player );
			wait( 0.5 );
			if ( level.survival_in_intermission )
				break;
		}
	}
	codolprintln( "SVT bench done" );
}
