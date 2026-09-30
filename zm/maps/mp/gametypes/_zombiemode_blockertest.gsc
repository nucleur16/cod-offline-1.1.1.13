// Developer-only test verbs for purchasable doors and debris.
//
// Runs only on a developer server (the live server runs developer 0).  Drive it
// through the dvar scr_codol_blockertest:
//
//   addbot                    add one test client
//   dump                      print every zombie_debris / zombie_door trigger
//                             still alive, each player's score, and the flags
//   buy <client> <trig ent>   put the client (a bot) on the trigger, give it
//                             enough points, face the trigger and hold USE --
//                             the engine's own trigger_use activation path
//   fire <client> <trig ent>  same, but also notify "trigger" from script,
//                             for when the real activation cannot be reached
//
// Written for the stairs-debris report (zm_prototype_sh: the second-floor
// trigger survived a first-floor buy).  See _zombiemode_blockers.gsc
// delete_blocker_triggers().

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;

	setDvar( "scr_codol_blockertest", "" );
	level thread blockertest_watch();
}

blockertest_watch()
{
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_codol_blockertest" );
		if ( request == "" )
			continue;
		setDvar( "scr_codol_blockertest", "" );

		tokens = strTok( request, " " );
		switch ( tokens[ 0 ] )
		{
		case "addbot":
			addtestclient();
			codolprintln( "CoDOL blockertest: addtestclient" );
			break;

		case "dump":
			blockertest_dump();
			break;

		case "buy":
		case "fire":
			if ( tokens.size < 3 )
				break;
			level thread blockertest_buy( tokens[ 0 ], int( tokens[ 1 ] ), int( tokens[ 2 ] ) );
			break;
		}
	}
}

blockertest_triggers()
{
	all = [];
	names = [];
	names[ 0 ] = "zombie_debris";
	names[ 1 ] = "zombie_door";
	for ( n = 0; n < names.size; n++ )
	{
		trigs = getEntArray( names[ n ], "targetname" );
		for ( i = 0; i < trigs.size; i++ )
			all[ all.size ] = trigs[ i ];
	}
	return all;
}

blockertest_dump()
{
	trigs = blockertest_triggers();
	codolprintln( "CoDOL blockertest: " + trigs.size + " door/debris triggers alive" );
	for ( i = 0; i < trigs.size; i++ )
	{
		t = trigs[ i ];
		target = "none";
		if ( isDefined( t.target ) )
			target = t.target;
		flag = "none";
		if ( isDefined( t.script_flag ) )
			flag = t.script_flag;
		pieces = 0;
		if ( isDefined( t.target ) )
			pieces = getEntArray( t.target, "targetname" ).size;
		codolprintln( "CoDOL blockertest:   ent " + t getEntityNumber() + " " + t.targetname + " origin " + t.origin +
			" target " + target + " (" + pieces + " pieces) flag " + flag );
	}

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		codolprintln( "CoDOL blockertest:   client " + players[ i ] getEntityNumber() + " score " + players[ i ].score +
			" origin " + players[ i ].origin );

	blockers = [];
	blockers[ 0 ] = "upstairs_blocker";
	blockers[ 1 ] = "upstairs_blocker2";
	blockers[ 2 ] = "north_upstairs_blocker";
	blockers[ 3 ] = "south_upstairs_blocker";
	for ( b = 0; b < blockers.size; b++ )
	{
		pieces = getEntArray( blockers[ b ], "targetname" );
		for ( i = 0; i < pieces.size; i++ )
		{
			kind = "piece";
			if ( isDefined( pieces[ i ].script_noteworthy ) )
				kind = pieces[ i ].script_noteworthy;
			codolprintln( "CoDOL blockertest:   " + blockers[ b ] + " " + kind + " ent " + pieces[ i ] getEntityNumber() +
				" origin " + pieces[ i ].origin );
		}
	}

	flags = [];
	flags[ 0 ] = "start_2_upstairs";
	flags[ 1 ] = "box_2_upstairs";
	flags[ 2 ] = "start_2_box";
	for ( i = 0; i < flags.size; i++ )
	{
		if ( isDefined( level.flag ) && isDefined( level.flag[ flags[ i ] ] ) )
			codolprintln( "CoDOL blockertest:   flag " + flags[ i ] + " = " + level.flag[ flags[ i ] ] );
	}
}

blockertest_buy( verb, clientnum, entnum )
{
	player = undefined;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] getEntityNumber() == clientnum )
			player = players[ i ];
	}
	trig = undefined;
	trigs = blockertest_triggers();
	for ( i = 0; i < trigs.size; i++ )
	{
		if ( trigs[ i ] getEntityNumber() == entnum )
			trig = trigs[ i ];
	}
	if ( !isDefined( player ) || !isDefined( trig ) )
	{
		codolprintln( "CoDOL blockertest: no client " + clientnum + " or no live trigger ent " + entnum );
		return;
	}

	cost = 1000;
	if ( isDefined( trig.zombie_cost ) )
		cost = trig.zombie_cost;
	if ( player.score < cost )
		player maps\mp\gametypes\_zombiemode_score::add_to_player_score( cost - player.score + 500 );

	before = player.score;
	player setOrigin( trig.origin - ( 0, 0, 40 ) );
	player setPlayerAngles( ( 0, 0, 0 ) );
	codolprintln( "CoDOL blockertest: " + verb + " client " + clientnum + " on ent " + entnum + " (" + trig.targetname +
		" " + trig.origin + ") score before " + before + " cost " + cost );

	player codol_botPressButtons( 40, 2 );
	if ( verb == "fire" )
	{
		wait( 0.1 );
		if ( isDefined( trig ) )
			trig notify( "trigger", player );
	}

	wait( 2.5 );
	codolprintln( "CoDOL blockertest: after " + verb + " client " + clientnum + " score " + player.score +
		" (charged " + ( before - player.score ) + ")" );
	blockertest_dump();
}
