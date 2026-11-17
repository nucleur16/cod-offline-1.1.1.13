// Developer-only test verbs for the mystery box and the power-up drop rule.
//
// Runs only on a developer server.  Drive it through scr_boxtest:
//
//   addbot              add one test client
//   use <client> <n>    n real purchases at the live box by that client:
//                       points, stand at the trigger, hold USE and notify the
//                       trigger, then take the weapon (every 3rd pull lets the
//                       box time out instead).  Waits out a teddy-bear move.
//   dump                each spot's pieces, visibility and rubble.
//   weapon              the live box's floating weapon model and its height.
//   dropscan            run powerup_drop (armed) at every zombie_spawner and
//                       exterior_goal struct (outside) and at every player
//                       spawn, wall buy and box spot (inside); report which
//                       produced a power-up and with what model.
//   kismodels           each power-up's kis_* world model and drawn model.
//   firesale            start a fire sale; dump the spots during and after.
//   volumes             list the player_volumes and whether each client is in one.
//   goto <client> <x> <y> <z>   move a client.

#include maps\_zombiemode_utility;

init()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;

	setDvar( "scr_boxtest", "" );
	level thread boxtest_watch();
}

boxtest_watch()
{
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_boxtest" );
		if ( request == "" )
			continue;
		setDvar( "scr_boxtest", "" );

		tokens = strTok( request, " " );
		switch ( tokens[ 0 ] )
		{
		case "addbot":
			addtestclient();
			{}
			break;

		case "use":
			if ( tokens.size < 3 )
				break;
			boxtest_use( int( tokens[ 1 ] ), int( tokens[ 2 ] ) );
			{}
			break;

		case "dump":
			boxtest_dump();
			break;

		case "weapon":
			// The live box's floating weapon: where it is right now (it rises
			// out of the box while it cycles and sinks back while it waits).
			chest = level.chests[ level.chest_index ];
			if ( isDefined( chest.weapon_model ) )
				{}
			else
				{}
			break;

		case "dropscan":
			boxtest_dropscan();
			break;

		case "firesale":
			level thread maps\_zombiemode_powerups::start_fire_sale();
			wait( 1 );
			boxtest_dump();
			level waittill( "fire_sale_off" );
			wait( 1 );
			boxtest_dump();
			break;

		case "goto":
			if ( tokens.size < 5 )
				break;
			p = boxtest_player( int( tokens[ 1 ] ) );
			if ( isDefined( p ) )
				p setOrigin( ( float( tokens[ 2 ] ), float( tokens[ 3 ] ), float( tokens[ 4 ] ) ) );
			break;

		case "volumes":
			vols = maps\_zombiemode_powerups::playable_area_volumes();
			for ( i = 0; i < vols.size; i++ )
				{}
			players = getPlayers();
			for ( i = 0; i < players.size; i++ )
			{
				inside = false;
				for ( k = 0; k < vols.size; k++ )
				{
					if ( players[ i ] isTouching( vols[ k ] ) )
						inside = true;
				}
				{}
			}
			break;

		case "kismodels":
			keys = getArrayKeys( level.zombie_powerups );
			for ( i = 0; i < keys.size; i++ )
			{
				p = level.zombie_powerups[ keys[ i ] ];
				{}
			}
			break;
		}
	}
}

boxtest_player( clientnum )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] getEntityNumber() == clientnum )
			return players[ i ];
	}
	return undefined;
}

boxtest_use( clientnum, n )
{
	for ( pass = 0; pass < n; pass++ )
	{
		player = boxtest_player( clientnum );
		if ( !isDefined( player ) || !isAlive( player ) )
		{
			{}
			return;
		}

		while ( flag( "moving_chest_now" ) )
			wait( 0.5 );

		chest = level.chests[ level.chest_index ];
		while ( is_true( chest.disabled ) || is_true( chest._box_open ) )
			wait( 0.25 );

		if ( player.score < 2000 )
			player maps\_zombiemode_score::add_to_player_score( 5000 );

		// A test client has nothing in hand; the box's "none" guard lets it
		// through on this developer-only mark.
		player.codol_boxtest = true;

		before = player.score;
		moves = level.chest_moves;
		player setOrigin( chest.origin - ( 0, 0, 40 ) );
		player codol_botPressButtons( 40, 1 );
		wait( 0.1 );
		{}
		chest notify( "trigger", player );
		level thread boxtest_weapon_track( chest, pass );

		// The roll: 40 cycles, about 4.5 s.
		wait( 5.5 );

		if ( level.chest_moves != moves )
		{
			{}
			while ( flag( "moving_chest_now" ) )
				wait( 0.5 );
			boxtest_dump();
			continue;
		}

		weapon = "<none>";
		if ( isDefined( chest.grabbed_weapon ) )
			weapon = chest.grabbed_weapon;
		model = "<none>";
		if ( isDefined( chest.weapon_model ) && isDefined( chest.weapon_model.model ) )
			model = chest.weapon_model.model;
		{}

		if ( pass % 3 != 2 )
		{
			player codol_botPressButtons( 40, 1 );
			wait( 0.1 );
			chest notify( "trigger", player );
			wait( 0.5 );
			{}
		}
		else
			{}

		while ( is_true( chest._box_open ) )
			wait( 0.25 );
	}
}

// The floating weapon's height above the chest origin, once a second for the
// pull: it rises 40 while it cycles and sinks back while it waits.
boxtest_weapon_track( chest, pass )
{
	for ( t = 0; t < 18; t++ )
	{
		wait( 1 );
		if ( !isDefined( chest.weapon_model ) || !isDefined( chest.chest_origin ) )
		{
			{}
			return;
		}
		{}
	}
}

boxtest_dump()
{
	{}
	for ( c = 0; c < level.chests.size; c++ )
	{
		chest = level.chests[ c ];
		sn = "<none>";
		if ( isDefined( chest.script_noteworthy ) )
			sn = chest.script_noteworthy;
		{}
		boxtest_piece( "lid", chest.chest_lid );
		boxtest_piece( "box", chest.chest_box );
		for ( r = 0; r < chest.chest_rubble.size; r++ )
			boxtest_piece( "rubble", chest.chest_rubble[ r ] );
	}
}

boxtest_piece( label, ent )
{
	if ( !isDefined( ent ) )
	{
		{}
		return;
	}
	m = "<none>";
	if ( isDefined( ent.model ) )
		m = ent.model;
	{}
}

boxtest_dropscan()
{
	outside = [];
	a = get_struct_array( "zombie_spawner", "script_noteworthy" );
	for ( i = 0; i < a.size; i++ )
		outside[ outside.size ] = a[ i ].origin;
	a = get_struct_array( "exterior_goal", "targetname" );
	for ( i = 0; i < a.size; i++ )
		outside[ outside.size ] = a[ i ].origin;

	inside = [];
	names = [];
	names[ 0 ] = "mp_tdm_spawn_allies_start";
	names[ 1 ] = "mp_dm_spawn";
	names[ 2 ] = "mp_tdm_spawn";
	names[ 3 ] = "info_player_start";
	for ( n = 0; n < names.size; n++ )
	{
		a = getEntArray( names[ n ], "classname" );
		for ( i = 0; i < a.size; i++ )
			inside[ inside.size ] = a[ i ].origin;
	}
	a = getEntArray( "weapon_upgrade", "targetname" );
	for ( i = 0; i < a.size; i++ )
		inside[ inside.size ] = a[ i ].origin - ( 0, 0, 40 );
	for ( i = 0; i < level.chests.size; i++ )
	{
		if ( isDefined( level.chests[ i ].chest_box ) )
			inside[ inside.size ] = level.chests[ i ].chest_box.origin + ( 0, 0, 10 );
	}

	o = boxtest_drop_points( "outside", outside );
	inn = boxtest_drop_points( "inside", inside );
	{}
}

boxtest_drop_points( label, points )
{
	dropped = 0;
	for ( i = 0; i < points.size; i++ )
	{
		level.powerup_drop_count = 0;
		level.zombie_vars[ "zombie_drop_item" ] = 1;
		before = getEntArray( "script_model", "classname" ).size;
		maps\_zombiemode_powerups::powerup_drop( points[ i ] );
		models = getEntArray( "script_model", "classname" );
		made = undefined;
		for ( k = 0; k < models.size; k++ )
		{
			if ( isDefined( models[ k ].powerup_name ) && !isDefined( models[ k ].boxtest_seen ) )
				made = models[ k ];
		}
		if ( isDefined( made ) )
		{
			dropped++;
			{}
			made.boxtest_seen = true;
			made notify( "powerup_grabbed" );
			made notify( "powerup_timedout" );
			made delete();
		}
		else
			{}
		wait( 0.05 );
	}
	level.powerup_drop_count = 0;
	level.zombie_vars[ "zombie_drop_item" ] = 0;
	return dropped;
}
