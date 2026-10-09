// Developer-only test verbs for the mystery box and the power-up drop rule.
//
// Runs only on a developer server.  Drive it through scr_codol_boxtest:
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

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;

	setDvar( "scr_codol_boxtest", "" );
	level thread boxtest_watch();
}

boxtest_watch()
{
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_codol_boxtest" );
		if ( request == "" )
			continue;
		setDvar( "scr_codol_boxtest", "" );

		tokens = strTok( request, " " );
		switch ( tokens[ 0 ] )
		{
		case "addbot":
			addtestclient();
			codolprintln( "CoDOL boxtest: addtestclient" );
			break;

		case "use":
			if ( tokens.size < 3 )
				break;
			boxtest_use( int( tokens[ 1 ] ), int( tokens[ 2 ] ) );
			codolprintln( "CoDOL boxtest: use done" );
			break;

		case "dump":
			boxtest_dump();
			break;

		case "weapon":
			// The live box's floating weapon: where it is right now (it rises
			// out of the box while it cycles and sinks back while it waits).
			chest = level.chests[ level.chest_index ];
			if ( isDefined( chest.weapon_model ) )
				codolprintln( "CoDOL boxtest: weapon model " + chest.weapon_model.model + " at " + chest.weapon_model.origin + ", " + ( chest.weapon_model.origin[ 2 ] - chest.chest_origin.origin[ 2 ] ) + " above the chest origin" );
			else
				codolprintln( "CoDOL boxtest: no weapon model" );
			break;

		case "dropscan":
			boxtest_dropscan();
			break;

		case "firesale":
			level thread maps\mp\gametypes\_zombiemode_powerups::start_fire_sale();
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
			vols = maps\mp\gametypes\_zombiemode_powerups::playable_area_volumes();
			for ( i = 0; i < vols.size; i++ )
				codolprintln( "CoDOL boxtest: player_volume ent " + vols[ i ] getEntityNumber() + " " + vols[ i ].targetname + " at " + vols[ i ].origin );
			players = getPlayers();
			for ( i = 0; i < players.size; i++ )
			{
				inside = false;
				for ( k = 0; k < vols.size; k++ )
				{
					if ( players[ i ] isTouching( vols[ k ] ) )
						inside = true;
				}
				codolprintln( "CoDOL boxtest: client " + players[ i ] getEntityNumber() + " at " + players[ i ].origin + " in a player_volume " + inside );
			}
			break;

		case "kismodels":
			keys = getArrayKeys( level.zombie_powerups );
			for ( i = 0; i < keys.size; i++ )
			{
				p = level.zombie_powerups[ keys[ i ] ];
				codolprintln( "CoDOL boxtest: power-up " + keys[ i ] + " weapon " + p.weapon_name + " weapon model " + getWeaponModel( p.weapon_name ) + " drawn as " + p.model_name );
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
			codolprintln( "CoDOL boxtest: no live client " + clientnum );
			return;
		}

		while ( flag( "moving_chest_now" ) )
			wait( 0.5 );

		chest = level.chests[ level.chest_index ];
		while ( is_true( chest.disabled ) || is_true( chest._box_open ) )
			wait( 0.25 );

		if ( player.score < 2000 )
			player maps\mp\gametypes\_zombiemode_score::add_to_player_score( 5000 );

		// A test client has nothing in hand; the box's "none" guard lets it
		// through on this developer-only mark.
		player.codol_boxtest = true;

		before = player.score;
		moves = level.chest_moves;
		player setOrigin( chest.origin - ( 0, 0, 40 ) );
		player codol_botPressButtons( 40, 1 );
		wait( 0.1 );
		codolprintln( "CoDOL boxtest: pass " + pass + " valid " + is_player_valid( player ) + " use " + player useButtonPressed() + " holds " + player getCurrentWeapon() + " score " + player.score );
		chest notify( "trigger", player );
		level thread boxtest_weapon_track( chest, pass );

		// The roll: 40 cycles, about 4.5 s.
		wait( 5.5 );

		if ( level.chest_moves != moves )
		{
			codolprintln( "CoDOL boxtest: pass " + pass + " teddy bear at chest " + chest getEntityNumber() + "; refund " + ( player.score - before ) );
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
		codolprintln( "CoDOL boxtest: pass " + pass + " chest " + chest getEntityNumber() + " offers " + weapon + " model " + model + " charged " + ( before - player.score ) );

		if ( pass % 3 != 2 )
		{
			player codol_botPressButtons( 40, 1 );
			wait( 0.1 );
			chest notify( "trigger", player );
			wait( 0.5 );
			codolprintln( "CoDOL boxtest: pass " + pass + " took it; holds " + player getCurrentWeapon() );
		}
		else
			codolprintln( "CoDOL boxtest: pass " + pass + " left it to time out" );

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
			codolprintln( "CoDOL boxtest: pass " + pass + " t+" + ( t + 1 ) + " no weapon model" );
			return;
		}
		codolprintln( "CoDOL boxtest: pass " + pass + " t+" + ( t + 1 ) + " weapon " + chest.weapon_model.model + " " + ( chest.weapon_model.origin[ 2 ] - chest.chest_origin.origin[ 2 ] ) + " above the chest origin" );
	}
}

boxtest_dump()
{
	codolprintln( "CoDOL boxtest: live index " + level.chest_index + " of " + level.chests.size + ", moves " + level.chest_moves + ", accessed " + level.chest_accessed );
	for ( c = 0; c < level.chests.size; c++ )
	{
		chest = level.chests[ c ];
		sn = "<none>";
		if ( isDefined( chest.script_noteworthy ) )
			sn = chest.script_noteworthy;
		codolprintln( "CoDOL boxtest: chest " + c + " trigger ent " + chest getEntityNumber() + " " + sn + " hidden " + is_true( chest.hidden ) +
			" disabled " + is_true( chest.disabled ) + " rubble " + chest.chest_rubble.size );
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
		codolprintln( "CoDOL boxtest:   " + label + " <undefined>" );
		return;
	}
	m = "<none>";
	if ( isDefined( ent.model ) )
		m = ent.model;
	codolprintln( "CoDOL boxtest:   " + label + " ent " + ent getEntityNumber() + " model " + m + " at " + ent.origin );
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
	codolprintln( "CoDOL boxtest: dropscan outside " + o + "/" + outside.size + " dropped, inside " + inn + "/" + inside.size + " dropped" );
}

boxtest_drop_points( label, points )
{
	dropped = 0;
	for ( i = 0; i < points.size; i++ )
	{
		level.powerup_drop_count = 0;
		level.zombie_vars[ "zombie_drop_item" ] = 1;
		before = getEntArray( "script_model", "classname" ).size;
		maps\mp\gametypes\_zombiemode_powerups::powerup_drop( points[ i ] );
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
			codolprintln( "CoDOL boxtest: " + label + " " + points[ i ] + " -> " + made.powerup_name + " model " + made.model + " ent " + made getEntityNumber() );
			made.boxtest_seen = true;
			made notify( "powerup_grabbed" );
			made notify( "powerup_timedout" );
			made delete();
		}
		else
			codolprintln( "CoDOL boxtest: " + label + " " + points[ i ] + " -> no drop (" + before + " models)" );
		wait( 0.05 );
	}
	level.powerup_drop_count = 0;
	level.zombie_vars[ "zombie_drop_item" ] = 0;
	return dropped;
}
