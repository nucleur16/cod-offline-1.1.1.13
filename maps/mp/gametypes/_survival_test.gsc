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
// Purchases are maps\_survival_kiosk's scr_survival_buy.

main()
{
	if ( getDvarInt( "scr_survival_test" ) == 0 )
		return;
	setDvar( "scr_survival_test_cmd", "" );
	codolprintln( "CoDOL survivaltest: harness active" );
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
