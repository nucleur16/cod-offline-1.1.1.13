// Developer-only test verbs for the power switch and the map traps.
//
// Runs only on a developer server.  Drive it through scr_codol_powertest:
//
//   addbot                  add one test client
//   god <client> 0|1        the test client ignores damage
//   points <client> <n>     set the client's points
//   goto <client> <x> <y> <z>   move a client
//   power <client>          stand the client at the power switch trigger and
//                           hold USE for a second: the real trigger path
//   perk <client>           the same at each zombie_vending machine in turn
//                           (plus the trigger notify: the machines need a
//                           look-at a test client cannot aim); reports the
//                           perks the client then has
//   buy <client> <id>       the same at trap <id>'s handle (id = script_string)
//   lure <id> <n>           forceTeleport up to n live zombies onto trap
//                           <id>'s effect points and report what happens
//   probe <client> <id>     walk the client over a grid round trap <id>'s
//                           handle; report where it reaches the handle and
//                           whether it is then standing in the trap
//   state                   power flag, every trap's state and kill counts

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;

	setDvar( "scr_codol_powertest", "" );
	level thread powertest_watch();
}

powertest_watch()
{
	for ( ;; )
	{
		wait( 0.25 );
		request = getDvar( "scr_codol_powertest" );
		if ( request == "" )
			continue;
		setDvar( "scr_codol_powertest", "" );

		tokens = strTok( request, " " );
		codolprintln( "CoDOL powertest: > " + request );
		switch ( tokens[ 0 ] )
		{
		case "addbot":
			addtestclient();
			break;

		case "god":
			p = powertest_player( tokens, 1 );
			if ( isDefined( p ) && tokens.size > 2 )
			{
				if ( tokens[ 2 ] == "1" )
					p.codolTestInvulnerable = true;
				else
					p.codolTestInvulnerable = undefined;
			}
			break;

		case "points":
			p = powertest_player( tokens, 1 );
			if ( isDefined( p ) && tokens.size > 2 )
			{
				p.score = int( tokens[ 2 ] );
				codolprintln( "CoDOL powertest: client " + p getEntityNumber() + " has " + p.score + " points" );
			}
			break;

		case "goto":
			p = powertest_player( tokens, 1 );
			if ( isDefined( p ) && tokens.size > 4 )
				p setOrigin( ( float( tokens[ 2 ] ), float( tokens[ 3 ] ), float( tokens[ 4 ] ) ) );
			break;

		case "power":
			p = powertest_player( tokens, 1 );
			if ( !isDefined( p ) )
				break;
			trigs = maps\mp\gametypes\_zombiemode_power::power_switch_triggers();
			if ( trigs.size < 1 )
			{
				codolprintln( "CoDOL powertest: no power switch trigger left; power_on " + flag( "power_on" ) );
				break;
			}
			lever = maps\mp\gametypes\_zombiemode_power::power_switch_lever();
			before = "<none>";
			if ( isDefined( lever ) )
				before = "" + lever.angles;
			powertest_press( p, trigs[ 0 ].origin );
			wait( 1 );
			after = "<none>";
			if ( isDefined( lever ) )
				after = "" + lever.angles;
			codolprintln( "CoDOL powertest: power_on " + flag( "power_on" ) + "; lever angles " + before + " -> " + after );
			break;

		case "perk":
			p = powertest_player( tokens, 1 );
			if ( !isDefined( p ) )
				break;
			machines = getEntArray( "zombie_vending", "targetname" );
			for ( i = 0; i < machines.size; i++ )
			{
				p.score = 10000;
				powertest_press( p, machines[ i ].origin );
				// The machines require the user to look at them, which a
				// test client cannot aim for; with USE held, notify as the
				// engine would.
				wait( 0.1 );
				machines[ i ] notify( "trigger", p );
				wait( 4 );
				codolprintln( "CoDOL powertest: vending " + machines[ i ].script_noteworthy + ": client has it " + p hasPerk( machines[ i ].script_noteworthy ) + ", points " + p.score );
			}
			break;

		case "buy":
			p = powertest_player( tokens, 1 );
			trap = powertest_trap( tokens, 2 );
			if ( !isDefined( p ) || !isDefined( trap ) )
				break;
			before = p.score;
			sw = "<none>";
			if ( trap._trap_switches.size )
				sw = "" + trap._trap_switches[ 0 ].angles;
			powertest_press( p, trap._trap_use_trigs[ 0 ].origin );
			wait( 1 );
			activated = 0;
			if ( isDefined( p.zm_traps_activated ) )
				activated = p.zm_traps_activated;
			sw2 = "<none>";
			if ( trap._trap_switches.size )
				sw2 = "" + trap._trap_switches[ 0 ].angles;
			codolprintln( "CoDOL powertest: trap " + trap._trap_id + " in use " + trap._trap_in_use + ", charged " + ( before - p.score ) +
				", handle " + sw + " -> " + sw2 + ", traps activated by client " + activated );
			break;

		case "lure":
			trap = powertest_trap( tokens, 1 );
			if ( !isDefined( trap ) || tokens.size < 3 )
				break;
			level thread powertest_lure( trap, int( tokens[ 2 ] ) );
			break;

		case "probe":
			p = powertest_player( tokens, 1 );
			trap = powertest_trap( tokens, 2 );
			if ( !isDefined( p ) || !isDefined( trap ) )
				break;
			powertest_probe( p, trap );
			break;

		case "state":
			codolprintln( "CoDOL powertest: power_on " + flag( "power_on" ) + ", traps " + level.zm_map_traps.size + ", trap kills " + level.zombie_trap_killed_count +
				", zombies " + level.zombies.size );
			for ( i = 0; i < level.zm_map_traps.size; i++ )
			{
				t = level.zm_map_traps[ i ];
				codolprintln( "CoDOL powertest: trap " + t._trap_id + " ent " + t getEntityNumber() + " at " + t.origin + " in use " + t._trap_in_use +
					" cooling " + t._trap_cooling_down + " handle " + t._trap_use_trigs[ 0 ].origin );
			}
			break;
		}
	}
}

powertest_player( tokens, index )
{
	if ( tokens.size <= index )
		return undefined;
	n = int( tokens[ index ] );
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] getEntityNumber() == n )
			return players[ i ];
	}
	codolprintln( "CoDOL powertest: no client " + n );
	return undefined;
}

powertest_trap( tokens, index )
{
	if ( tokens.size <= index )
		return undefined;
	for ( i = 0; i < level.zm_map_traps.size; i++ )
	{
		if ( level.zm_map_traps[ i ]._trap_id == tokens[ index ] )
			return level.zm_map_traps[ i ];
	}
	codolprintln( "CoDOL powertest: no trap " + tokens[ index ] );
	return undefined;
}

// Stand the client inside a trigger brush and hold C1 USE (0x28) in its
// usercmds, so the engine's own use path fires the trigger.
powertest_press( p, origin )
{
	p setOrigin( origin - ( 0, 0, 40 ) );
	wait( 0.2 );
	codolprintln( "CoDOL powertest: client " + p getEntityNumber() + " at " + p.origin + " pressing USE" );
	p codol_botPressButtons( 40, 1 );
}

powertest_lure( trap, n )
{
	pts = trap._trap_fx_structs;
	if ( pts.size < 1 )
		pts = trap._trap_audio_structs;
	if ( pts.size < 1 )
		return;

	moved = 0;
	for ( i = 0; i < level.zombies.size && moved < n; i++ )
	{
		z = level.zombies[ i ];
		if ( !isDefined( z ) || !isAlive( z ) || isDefined( z.marked_for_death ) )
			continue;
		spot = pts[ moved % pts.size ].origin + ( 0, 0, 4 );
		z forceTeleport( spot, ( 0, 0, 0 ) );
		moved++;
		wait( 0.05 );
		codolprintln( "CoDOL powertest: zombie ent " + z getEntityNumber() + " at " + z.origin + " touching trap " + trap._trap_id + " " + z isTouching( trap ) );
	}

	before = level.zombie_trap_killed_count;
	wait( 3 );
	codolprintln( "CoDOL powertest: lure " + trap._trap_id + ": moved " + moved + ", trap kills +" + ( level.zombie_trap_killed_count - before ) + ", live trap " + trap._trap_in_use );
}

powertest_probe( p, trap )
{
	use = trap._trap_use_trigs[ 0 ];
	both = 0;
	safe = 0;
	for ( dx = -96; dx <= 96; dx += 16 )
	{
		for ( dy = -96; dy <= 96; dy += 16 )
		{
			p setOrigin( use.origin + ( dx, dy, -40 ) );
			wait( 0.05 );
			if ( !p isTouching( use ) )
				continue;
			if ( p isTouching( trap ) )
				both++;
			else
			{
				safe++;
				if ( safe <= 3 )
					codolprintln( "CoDOL powertest: probe " + trap._trap_id + " reaches the handle outside the trap at " + p.origin );
			}
		}
	}
	codolprintln( "CoDOL powertest: probe " + trap._trap_id + ": handle reachable from " + ( both + safe ) + " grid points, " + safe + " of them outside the trap" );
}
