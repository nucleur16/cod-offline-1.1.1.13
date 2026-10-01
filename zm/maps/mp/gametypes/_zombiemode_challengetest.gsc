// Developer-only test verbs for zm challenge progress
// (_zombiemode_challenges.gsc).  Runs only on a developer server (the live
// server runs developer 0).  Drive it through the dvar scr_zm_chtest:
//
//   use <client> <targetname> [index] [presses]
//                              put the client (a bot) on that trigger, give it
//                              points, hold USE `presses` times (default 1) --
//                              the trigger's own activation path.  Wall buys
//                              are "weapon_upgrade", the box is
//                              "treasure_chest_use" (presses 2: buy, then take)
//   window <client> [index]    knock one board off an exterior_goal and hold
//                              USE on its repair trigger until it is whole
//   perk <client> <perk>       _zombiemode_perks::give_perk, the bottle's end
//   revive <reviver> <revivee> revive_success on a downed revivee
//   powerup <client> <name>    specific_powerup_drop at the client's feet
//   trap <client> <weapon>     spawn_trap at the client's feet, owned by it
//   zkill <client> <weapon> <mod> <head 0|1>
//                              doDamage the nearest zombie dead as the client,
//                              through native G_Damage with that weapon, means
//                              of death and (head 1) the head hit location
//   arm <client> <weapon>      give the client the weapon and put it in hand
//                              (a test client never switches on its own)
//   show <client>              print the client's nonzero zm challenges and XP
//
// Pair it with scr_codol_fftest (addbot, zhit, zdmg) for test clients, kills
// and downs, and scr_zm_challenges_testclients 1 so test clients are counted.

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;

	setDvar( "scr_zm_chtest", "" );
	level thread chtest_watch();
}

chtest_watch()
{
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_zm_chtest" );
		if ( request == "" )
			continue;
		setDvar( "scr_zm_chtest", "" );

		tokens = strTok( request, " " );
		p = chtest_player( tokens, 1 );
		if ( !isDefined( p ) )
			continue;

		switch ( tokens[ 0 ] )
		{
		case "use":
			if ( tokens.size > 2 )
				level thread chtest_use( p, tokens );
			break;

		case "window":
			level thread chtest_window( p, tokens );
			break;

		case "perk":
			if ( tokens.size > 2 )
				p maps\mp\gametypes\_zombiemode_perks::give_perk( tokens[ 2 ] );
			codolprintln( "CoDOL chtest: give_perk done" );
			break;

		case "revive":
			q = chtest_player( tokens, 2 );
			if ( isDefined( q ) && q maps\mp\gametypes\_zombiemode_laststand::player_is_in_laststand() )
				q maps\mp\gametypes\_zombiemode_laststand::revive_success( p );
			else
				codolprintln( "CoDOL chtest: revivee is not down" );
			break;

		case "powerup":
			if ( tokens.size > 2 )
				level thread maps\mp\gametypes\_zombiemode_powerups::specific_powerup_drop( tokens[ 2 ], p.origin );
			break;

		case "trap":
			if ( tokens.size > 2 )
				level thread maps\mp\gametypes\_zombiemode_traps_placeable::spawn_trap( p.origin, p, tokens[ 2 ] );
			break;

		case "zkill":
			if ( tokens.size > 4 )
				p chtest_zkill( tokens[ 2 ], tokens[ 3 ], int( tokens[ 4 ] ) );
			break;

		case "arm":
			if ( tokens.size > 2 )
			{
				p giveWeapon( tokens[ 2 ] );
				p switchToWeapon( tokens[ 2 ] );
				codolprintln( "CoDOL chtest: " + p.name + " now holds " + p getCurrentWeapon() );
			}
			break;

		case "show":
			p chtest_show();
			break;
		}
	}
}

chtest_player( tokens, index )
{
	if ( tokens.size <= index )
		return undefined;
	num = int( tokens[ index ] );
	for ( i = 0; i < level.players.size; i++ )
	{
		if ( level.players[ i ] getEntityNumber() == num )
			return level.players[ i ];
	}
	codolprintln( "CoDOL chtest: no client " + tokens[ index ] );
	return undefined;
}

chtest_use( p, tokens )
{
	index = 0;
	if ( tokens.size > 3 )
		index = int( tokens[ 3 ] );
	presses = 1;
	if ( tokens.size > 4 )
		presses = int( tokens[ 4 ] );

	trigs = getEntArray( tokens[ 2 ], "targetname" );
	if ( index >= trigs.size )
	{
		codolprintln( "CoDOL chtest: " + trigs.size + " " + tokens[ 2 ] + " trigger(s), no index " + index );
		return;
	}
	trig = trigs[ index ];

	if ( p.score < 5000 )
		p maps\mp\gametypes\_zombiemode_score::add_to_player_score( 5000 );

	p setOrigin( trig.origin - ( 0, 0, 40 ) );
	codolprintln( "CoDOL chtest: client " + p getEntityNumber() + " on " + tokens[ 2 ] + "[" + index + "] at " + trig.origin + ", score " + p.score );

	for ( n = 0; n < presses; n++ )
	{
		p codol_botPressButtons( 40, 2 );
		wait( 0.1 );
		if ( isDefined( trig ) )
			trig notify( "trigger", p );
		wait( 5 );
	}
	codolprintln( "CoDOL chtest: use done, score " + p.score + ", weapon " + p getCurrentWeapon() );
}

chtest_window( p, tokens )
{
	index = 0;
	if ( tokens.size > 2 )
		index = int( tokens[ 2 ] );

	if ( !isDefined( level.exterior_goals ) || index >= level.exterior_goals.size )
	{
		codolprintln( "CoDOL chtest: no exterior_goal " + index );
		return;
	}
	goal = level.exterior_goals[ index ];
	if ( !isDefined( goal.barrier_chunks ) || goal.barrier_chunks.size < 1 )
	{
		codolprintln( "CoDOL chtest: exterior_goal " + index + " has no boards" );
		return;
	}

	goal maps\mp\gametypes\_zombiemode_blockers::remove_chunk( goal.barrier_chunks[ 0 ], undefined, true );
	wait( 1.5 );

	where = goal.origin;
	if ( isDefined( goal.trigger_location ) )
		where = goal.trigger_location.origin;
	p setOrigin( groundpos( where ) + ( 0, 0, 4 ) );
	codolprintln( "CoDOL chtest: client " + p getEntityNumber() + " repairing exterior_goal " + index + " at " + where );

	for ( n = 0; n < 6; n++ )
	{
		p codol_botPressButtons( 40, 2 );
		wait( 1 );
	}
}

chtest_zkill( weapon, mod, head )
{
	best = maps\mp\gametypes\_zombiemode_fftest::fftest_nearest_zombie( self );
	if ( !isDefined( best ) )
	{
		codolprintln( "CoDOL chtest: no live zombie" );
		return;
	}
	num = best getEntityNumber();
	best doDamage( best.health + 50, best.origin + ( 0, 0, 60 ), self, self, head, mod, 0, weapon );
	codolprintln( "CoDOL chtest: zkill " + weapon + " " + mod + " head " + head + " by " + self.name + " -> zombie " + num );
}

chtest_show()
{
	line = "CoDOL chtest: " + self.name + " XP " + self getRankExperience() + ";";
	refs = getArrayKeys( level.zm_challenge_ids );
	for ( i = 0; i < refs.size; i++ )
	{
		value = self getPlayerData( "challenges", level.zm_challenge_ids[ refs[ i ] ] );
		if ( value > 0 )
			line = line + " " + refs[ i ] + "=" + value;
	}
	codolprintln( line );
}
