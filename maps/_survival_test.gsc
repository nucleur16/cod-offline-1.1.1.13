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
			break;
		case "status":
			status();
			break;
		case "killenemies":
			players = getPlayers();
			enemies = maps\_survival_ai::living_enemies();
			for ( i = 0; i < enemies.size; i++ )
			{
				if ( players.size )
					enemies[ i ] doDamage( enemies[ i ].health + 1000, enemies[ i ].origin, players[ 0 ] );
				else
					enemies[ i ] doDamage( enemies[ i ].health + 1000, enemies[ i ].origin );
			}
			codolprintln( "CoDOL survivaltest: killed " + enemies.size );
			break;
		case "godmode":
			players = getPlayers();
			n = 0;
			if ( tokens.size > 1 )
				n = int( tokens[ 1 ] );
			if ( n < players.size )
				players[ n ] thread godmode();
			break;
		case "wave":
			if ( tokens.size > 1 )
				level.survival_wave_num = int( tokens[ 1 ] ) - 1;
			break;
		}
	}
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
	enemies = maps\_survival_ai::living_enemies();
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
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		codolprintln( "CoDOL survivaltest:   player " + i + " " + p.sessionstate + " hp " + p.health + " at " + p.origin +
			" credits " + p.survival_credit + " kills " + p.kills );
	}
}
