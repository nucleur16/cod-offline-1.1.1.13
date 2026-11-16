// Developer-only test verbs for the round cap, the hound rounds and the
// rescue (_zombiemode_ai_dogs.gsc, _zombiemode_extraction.gsc).  Runs only
// on a developer server (the live server runs developer 0).  Drive it
// through the dvar scr_zm_extest:
//
//   status                     round, cap, HUD dvars, hound state, enemies
//   endround                   end the current round now: every enemy dies
//                              (no power-up drops) and the budget is spent
//   dogs                       make the next round a hound round
//   killdog <client>           the nearest hellhound dies to the client
//                              (native G_Damage, the client as attacker)
//   lz <client>                stand the client in the rescue landing zone
//   board <client>             put the client aboard the rescue helicopter
//
// Every round start also logs the four round HUD dvars updateRoundHud sets.
//
// Pair it with scr_fftest (addbot, invuln) for test clients and
// scr_zm_roundcap <n> for a short game.

#include maps\_zombiemode_utility;

init()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;

	setDvar( "scr_zm_extest", "" );
	level thread extest_watch();
	level thread extest_log_round_hud();
}

extest_watch()
{
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_zm_extest" );
		if ( request == "" )
			continue;
		setDvar( "scr_zm_extest", "" );

		tokens = strTok( request, " " );
		switch ( tokens[ 0 ] )
		{
		case "status":
			extest_status();
			break;

		case "endround":
			level thread extest_end_round();
			break;

		case "dogs":
			setDvar( "scr_zm_force_dogs", "1" );
			codolprintln( "CoDOL extest: the next round is a hound round" );
			break;

		case "killdog":
			p = extest_player( tokens, 1 );
			if ( isDefined( p ) )
				p extest_kill_dog();
			break;

		case "lz":
			p = extest_player( tokens, 1 );
			if ( isDefined( p ) )
				p extest_lz();
			break;

		case "board":
			p = extest_player( tokens, 1 );
			if ( isDefined( p ) && isDefined( level.zm_rescue_heli ) )
				p maps\_zombiemode_extraction::board( level.zm_rescue_heli, "extest board" );
			else
				codolprintln( "CoDOL extest: no client or no helicopter" );
			break;
		}
	}
}

extest_player( tokens, index )
{
	if ( tokens.size <= index )
		return undefined;
	num = int( tokens[ index ] );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] getEntityNumber() == num )
			return players[ i ];
	}
	codolprintln( "CoDOL extest: no client " + tokens[ index ] );
	return undefined;
}

extest_status()
{
	dogs = 0;
	alive = 0;
	if ( isDefined( level.zombies ) )
	{
		for ( i = 0; i < level.zombies.size; i++ )
		{
			z = level.zombies[ i ];
			if ( !isDefined( z ) || !isAlive( z ) )
				continue;
			alive++;
			if ( is_true( z.isdog ) )
				dogs++;
		}
	}
	extracted = 0;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( is_true( players[ i ].zm_extracted ) )
			extracted++;
	}
	codolprintln( "CoDOL extest: round " + level.round_number + "/" + level.zm_round_cap +
		" final " + maps\_zombiemode::is_final_round() +
		" dog_round " + flag( "dog_round" ) + " next_dog_round " + diag_val( level.next_dog_round ) +
		" dog_round_count " + level.dog_round_count +
		" enemies " + alive + " (hounds " + dogs + ") budget " + level.zombie_total +
		" extraction " + is_true( level.zm_extraction_active ) + " extracted " + extracted +
		" players " + players.size );
}

extest_end_round()
{
	level.zombie_total = 0;
	if ( isDefined( level.zombies ) )
	{
		zombies = level.zombies;
		for ( i = 0; i < zombies.size; i++ )
		{
			z = zombies[ i ];
			if ( !isDefined( z ) || !isAlive( z ) )
				continue;
			z.no_powerup_drop = true;
			z doDamage( z.health + 1000, z.origin );
		}
	}
	codolprintln( "CoDOL extest: round " + level.round_number + " ended by test" );
}

extest_kill_dog()
{
	best = undefined;
	best_dist = 0;
	for ( i = 0; i < level.zombies.size; i++ )
	{
		z = level.zombies[ i ];
		if ( !isDefined( z ) || !isAlive( z ) || !is_true( z.isdog ) )
			continue;
		d = distanceSquared( z.origin, self.origin );
		if ( !isDefined( best ) || d < best_dist )
		{
			best = z;
			best_dist = d;
		}
	}
	if ( !isDefined( best ) )
	{
		codolprintln( "CoDOL extest: no live hellhound" );
		return;
	}
	num = best getEntityNumber();
	origin = best.origin;
	best doDamage( best.health + 10, origin, self, self, 0, "MOD_RIFLE_BULLET", 0, "m1911_zm" );
	codolprintln( "CoDOL extest: " + self.name + " killed hellhound " + num + " at " + origin );
}

extest_lz()
{
	if ( !isDefined( level.zm_extraction_route ) )
	{
		codolprintln( "CoDOL extest: no rescue landing zone yet" );
		return;
	}
	lz = level.zm_extraction_route.lz;
	self setOrigin( lz + ( 0, 0, 8 ) );
	codolprintln( "CoDOL extest: " + self.name + " placed at the landing zone " + lz );
}

// Every round boundary: the four round HUD dvars as updateRoundHud sets them.
extest_log_round_hud()
{
	level endon( "end_game" );
	for ( ;; )
	{
		level waittill( "start_of_round" );
		wait( 0.1 );
		goal = 0;
		if ( level.zm_round_cap > 0 )
			goal = level.zm_round_cap;
		codolprintln( "CoDOL extest: round HUD ui_hud_round " + level.round_number + " ui_hud_round_goal " + goal +
			" ui_hud_special_round " + flag( "dog_round" ) +
			" ui_hud_final_round " + maps\_zombiemode::is_final_round() );
	}
}
