// Developer-only test verbs for the zm wonder weapons
// (_zombiemode_weap_thundergun.gsc, _zombiemode_weap_tesla.gsc).
//
// Runs only on a developer server (the live server runs developer 0).  Drive
// it through the dvar scr_codol_weaptest (add a client first with
// scr_codol_fftest "addbot"):
//
//   thunder <client> [up]   line the nearest live zombies up in front of the
//                           client at 80..1100 units, face them, hand over
//                           thundergun_zm (or the upgrade) and raise
//                           "weapon_fired" as a real shot does
//   tesla <client> [up]     bunch the nearest zombies 250 units ahead of the
//                           client and hit the first with a tesla_gun_zm
//                           round (magicBullet), the chain the gun starts
//   status                  every zombie: health, origin, ragdoll

#include maps\_zombiemode_utility;

init()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;

	setDvar( "scr_codol_weaptest", "" );
	level thread weaptest_watch();
}

weaptest_watch()
{
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_codol_weaptest" );
		if ( request == "" )
			continue;
		setDvar( "scr_codol_weaptest", "" );

		tokens = strTok( request, " " );
		switch ( tokens[ 0 ] )
		{
		case "thunder":
			level thread weaptest_thunder( tokens );
			break;
		case "tesla":
			level thread weaptest_tesla( tokens );
			break;
		case "status":
			weaptest_status();
			break;
		default:
			codolprintln( "CoDOL weaptest: unknown verb " + tokens[ 0 ] );
			break;
		}
	}
}

weaptest_player( tokens, index )
{
	if ( tokens.size <= index )
		return undefined;
	num = int( tokens[ index ] );
	for ( i = 0; i < level.players.size; i++ )
	{
		if ( level.players[ i ] getEntityNumber() == num )
			return level.players[ i ];
	}
	codolprintln( "CoDOL weaptest: no client " + tokens[ index ] );
	return undefined;
}

weaptest_zombies( origin, max )
{
	return maps\_zombiemode_weap_thundergun::thundergun_closest( origin, getAiSpeciesArray( "axis", "all" ), 100000 );
}

// The yaw (of 16) with the longest clear line from the client's eye, so the
// test line is not placed inside a wall.
weaptest_open_yaw( player )
{
	best = player.angles[ 1 ];
	best_len = -1;
	eye = player getEye();
	for ( i = 0; i < 16; i++ )
	{
		yaw = i * 22.5;
		end = eye + vector_scale( anglesToForward( ( 0, yaw, 0 ) ), 1300 );
		trace = bulletTrace( eye, end, false, player );
		if ( trace[ "fraction" ] > best_len )
		{
			best_len = trace[ "fraction" ];
			best = yaw;
		}
	}
	codolprintln( "CoDOL weaptest: open yaw " + best + " clear " + int( best_len * 1300 ) + " units" );
	return best;
}

weaptest_face( player, target )
{
	angles = vectorToAngles( target - player getEye() );
	player setPlayerAngles( angles );
}

weaptest_thunder( tokens )
{
	player = weaptest_player( tokens, 1 );
	if ( !isDefined( player ) )
		return;
	weapon = "thundergun_zm";
	if ( tokens.size > 2 && tokens[ 2 ] == "up" )
		weapon = "thundergun_upgraded_zm";
	if ( !is_true( level.zm_thundergun_ok ) )
	{
		codolprintln( "CoDOL weaptest: the thundergun is not mounted on this map" );
		return;
	}

	dists = [];
	dists[ 0 ] = 80;
	dists[ 1 ] = 200;
	dists[ 2 ] = 380;
	dists[ 3 ] = 600;
	dists[ 4 ] = 850;
	dists[ 5 ] = 1100;

	yaw = weaptest_open_yaw( player );
	forward = anglesToForward( ( 0, yaw, 0 ) );
	zombies = weaptest_zombies( player.origin );
	n = 0;
	for ( i = 0; i < zombies.size && n < dists.size; i++ )
	{
		z = zombies[ i ];
		if ( !isAlive( z ) )
			continue;
		spot = player.origin + vector_scale( forward, dists[ n ] );
		trace = bulletTrace( spot + ( 0, 0, 64 ), spot - ( 0, 0, 256 ), false, z );
		if ( trace[ "fraction" ] < 1 )
			spot = trace[ "position" ];
		z forceTeleport( spot, ( 0, yaw + 180, 0 ) );
		codolprintln( "CoDOL weaptest: zombie " + z getEntityNumber() + " placed " + dists[ n ] + " ahead, health " + z.health );
		n++;
	}

	player giveWeapon( weapon );
	player giveMaxAmmo( weapon );
	player switchToWeapon( weapon );
	weaptest_face( player, player.origin + vector_scale( forward, 400 ) + ( 0, 0, 40 ) );
	wait( 1.5 );

	codolprintln( "CoDOL weaptest: " + player.name + " fires " + weapon + " (held " + player getCurrentWeapon() + ") from " + player getWeaponMuzzlePoint() +
		" dir " + player getWeaponForwardDir() );
	player notify( "weapon_fired", weapon );

	wait( 3 );
	for ( i = 0; i < zombies.size && i < n; i++ )
		weaptest_describe( zombies[ i ], player );
}

weaptest_tesla( tokens )
{
	player = weaptest_player( tokens, 1 );
	if ( !isDefined( player ) )
		return;
	weapon = "tesla_gun_zm";
	if ( tokens.size > 2 && tokens[ 2 ] == "up" )
		weapon = "tesla_gun_upgraded_zm";

	yaw = weaptest_open_yaw( player );
	forward = anglesToForward( ( 0, yaw, 0 ) );
	right = anglesToRight( ( 0, yaw, 0 ) );
	zombies = weaptest_zombies( player.origin );
	offsets = [];
	offsets[ 0 ] = ( 0, 0, 0 );
	offsets[ 1 ] = ( 70, 0, 0 );
	offsets[ 2 ] = ( 0, 70, 0 );
	offsets[ 3 ] = ( 0, -70, 0 );
	offsets[ 4 ] = ( 140, 0, 0 );
	first = undefined;
	n = 0;
	for ( i = 0; i < zombies.size && n < offsets.size; i++ )
	{
		z = zombies[ i ];
		if ( !isAlive( z ) )
			continue;
		o = offsets[ n ];
		spot = player.origin + vector_scale( forward, 250 + o[ 0 ] ) + vector_scale( right, o[ 1 ] );
		trace = bulletTrace( spot + ( 0, 0, 64 ), spot - ( 0, 0, 256 ), false, z );
		if ( trace[ "fraction" ] < 1 )
			spot = trace[ "position" ];
		z forceTeleport( spot, ( 0, yaw + 180, 0 ) );
		if ( !isDefined( first ) )
			first = z;
		n++;
	}
	if ( !isDefined( first ) )
	{
		codolprintln( "CoDOL weaptest: no live zombie" );
		return;
	}
	wait( 0.3 );
	player giveWeapon( weapon );
	player switchToWeapon( weapon );
	weaptest_face( player, first.origin + ( 0, 0, 40 ) );
	start = player getEye();
	target = first.origin + ( 0, 0, 40 );
	codolprintln( "CoDOL weaptest: " + player.name + " fires " + weapon + " at zombie " + first getEntityNumber() + " model " + first.model );
	magicBullet( weapon, start, target, player );
	wait( 4 );
	for ( i = 0; i < zombies.size && i < n; i++ )
		weaptest_describe( zombies[ i ], player );
}

weaptest_describe( z, player )
{
	if ( !isDefined( z ) )
	{
		codolprintln( "CoDOL weaptest: a zombie is gone (freed)" );
		return;
	}
	codolprintln( "CoDOL weaptest: zombie " + z getEntityNumber() + " health " + z.health + " alive " + isAlive( z ) +
		" at " + int( distance( z.origin, player.origin ) ) + " units, ragdoll " + z isRagdoll() +
		" thundergun_death " + is_true( z.thundergun_death ) + " tesla_death " + is_true( z.tesla_death ) );
}

weaptest_status()
{
	zombies = getAiSpeciesArray( "axis", "all" );
	codolprintln( "CoDOL weaptest: " + zombies.size + " live axis actors, getaiarray " + getAiArray( "axis" ).size );
	for ( i = 0; i < zombies.size; i++ )
		codolprintln( "CoDOL weaptest:   " + zombies[ i ] getEntityNumber() + " health " + zombies[ i ].health + " at " + zombies[ i ].origin );
}
