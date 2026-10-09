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
//   board <barrier> [client]  tear every board of level.exterior_goals[barrier]
//                             through remove_chunk (the real flight), then
//                             repair them -- with a client (a bot) by standing
//                             it in the repair trigger holding USE, else by
//                             threading replace_chunk directly -- and log each
//                             flying board's origin/angles/state every 50 ms
//
// Written for the stairs-debris report (zm_prototype_sh: the second-floor
// trigger survived a first-floor buy).  See _zombiemode_blockers.gsc
// delete_blocker_triggers().

#include maps\_zombiemode_utility;

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

		case "board":
			if ( tokens.size < 2 )
				break;
			client = -1;
			if ( tokens.size > 2 )
				client = int( tokens[ 2 ] );
			level thread blockertest_board( int( tokens[ 1 ] ), client );
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
		player maps\_zombiemode_score::add_to_player_score( cost - player.score + 500 );

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

// Tear, then rebuild, one window; log the boards while they fly back.
blockertest_board( index, clientnum )
{
	if ( !isDefined( level.exterior_goals ) || index < 0 || index >= level.exterior_goals.size )
	{
		codolprintln( "CoDOL blockertest: no barrier " + index );
		return;
	}
	node = level.exterior_goals[ index ];
	if ( !isDefined( node.barrier_chunks ) || node.barrier_chunks.size < 1 )
	{
		codolprintln( "CoDOL blockertest: barrier " + index + " has no chunks" );
		return;
	}
	chunks = node.barrier_chunks;

	codolprintln( "CoDOL blockertest: board barrier " + index + " at " + node.origin + ", " + chunks.size + " chunks" );
	for ( i = 0; i < chunks.size; i++ )
	{
		maps\_zombiemode_blockers::remove_chunk( chunks[ i ], node, true );
		wait( 0.1 );
	}
	wait( 2 );
	for ( i = 0; i < chunks.size; i++ )
		codolprintln( "CoDOL blockertest:   torn chunk " + chunks[ i ].script_noteworthy + " rests at " + chunks[ i ].origin +
			" angles " + chunks[ i ].angles + " state " + chunks[ i ] get_chunk_state() + " hidden slot " + chunks[ i ].og_origin );

	player = undefined;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] getEntityNumber() == clientnum )
			player = players[ i ];
	}

	if ( isDefined( player ) )
	{
		spot = node.origin;
		if ( isDefined( node.trigger_location ) )
			spot = node.trigger_location.origin;
		player setOrigin( groundpos( spot ) + ( 0, 0, 4 ) );
		codolprintln( "CoDOL blockertest: client " + clientnum + " holds USE in the repair trigger at " + player.origin + ", score " + player.score );
		player codol_botPressButtons( 40, 15 );
	}
	else
	{
		for ( i = 0; i < chunks.size; i++ )
		{
			node thread maps\_zombiemode_blockers::replace_chunk( chunks[ i ] );
			wait( 1 );
		}
	}

	end = getTime() + 16000;
	while ( getTime() < end )
	{
		flying = 0;
		for ( i = 0; i < chunks.size; i++ )
		{
			c = chunks[ i ];
			if ( c get_chunk_state() != "mid_repair" )
				continue;
			flying++;
			codolprintln( "CoDOL blockertest:   t " + getTime() + " chunk " + c.script_noteworthy + " origin " + c.origin + " angles " + c.angles +
				" destroyed " + c.destroyed );
		}
		if ( flying == 0 && all_chunks_intact( chunks ) )
			break;
		wait( 0.05 );
	}

	for ( i = 0; i < chunks.size; i++ )
	{
		ok = 0;
		if ( distance( chunks[ i ].origin, chunks[ i ].og_origin ) < 0.1 )
			ok = 1;
		codolprintln( "CoDOL blockertest:   final chunk " + chunks[ i ].script_noteworthy + " at " + chunks[ i ].origin + " angles " + chunks[ i ].angles +
			" state " + chunks[ i ] get_chunk_state() + " in slot " + ok );
	}
	if ( isDefined( player ) )
		codolprintln( "CoDOL blockertest: client " + clientnum + " score now " + player.score + ", intact " + all_chunks_intact( chunks ) );
}
