// Developer-only test verbs for zombie dismemberment (_zombiemode_gib.gsc).
//
// Runs only on a developer server (the live server runs developer 0).  Drive
// it through the dvar scr_codol_gibtest (add a client first with
// scr_codol_fftest "addbot"):
//
//   shoot <client> <bone> <weapon> [health]
//                        magicBullet from the client's side of the nearest
//                        zombie into <bone> (the real Bullet_Fire path; the
//                        notify names no bone, so this exercises
//                        gib_location_from_ray); the zombie's health is set
//                        to [health] first
//   blast <client> <bone> <damage>
//                        grenade splash through native G_Damage at <bone>
//   gib <ref>            force do_gib( <ref> ) on the nearest zombie
//                        (right_arm, left_arm, right_leg, left_leg, no_legs,
//                        guts, head)
//   status               every zombie's model, attachments and gib state,
//                        and every live thrown piece

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;

	setDvar( "scr_codol_gibtest", "" );
	level thread gibtest_watch();
}

gibtest_watch()
{
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_codol_gibtest" );
		if ( request == "" )
			continue;
		setDvar( "scr_codol_gibtest", "" );

		// From the first verb on, the damage watcher logs how it read each hit.
		level.zm_gib_diag = true;

		tokens = strTok( request, " " );
		switch ( tokens[ 0 ] )
		{
		case "shoot":
			level thread gibtest_shoot( tokens );
			break;
		case "blast":
			level thread gibtest_blast( tokens );
			break;
		case "gib":
			level thread gibtest_force( tokens );
			break;
		case "status":
			gibtest_status();
			break;
		default:
			codolprintln( "CoDOL gibtest: unknown verb " + tokens[ 0 ] );
			break;
		}
	}
}

gibtest_player( tokens, index )
{
	if ( tokens.size <= index )
		return undefined;
	num = int( tokens[ index ] );
	for ( i = 0; i < level.players.size; i++ )
	{
		if ( level.players[ i ] getEntityNumber() == num )
			return level.players[ i ];
	}
	codolprintln( "CoDOL gibtest: no client " + tokens[ index ] );
	return undefined;
}

gibtest_zombie( origin )
{
	if ( !isDefined( level.zombies ) )
		return undefined;
	best = undefined;
	for ( i = 0; i < level.zombies.size; i++ )
	{
		z = level.zombies[ i ];
		if ( !isDefined( z ) || !isAlive( z ) )
			continue;
		if ( !isDefined( origin ) )
			return z;
		if ( !isDefined( best ) || distanceSquared( z.origin, origin ) < distanceSquared( best.origin, origin ) )
			best = z;
	}
	return best;
}

gibtest_describe( z, label )
{
	if ( !isDefined( z ) )
	{
		codolprintln( "CoDOL gibtest: " + label + ": zombie is gone" );
		return;
	}
	parts = "";
	size = z getAttachSize();
	for ( i = 0; i < size; i++ )
		parts = parts + " " + z getAttachModelName( i );
	ref = "none";
	if ( isDefined( z.a ) && isDefined( z.a.gib_ref ) )
		ref = z.a.gib_ref;
	codolprintln( "CoDOL gibtest: " + label + ": zombie " + z getEntityNumber() + " health " + z.health +
		" model " + z.model + " attached[" + parts + " ] gib_ref " + ref +
		" gibbed " + is_true( z.gibbed ) + " head_gibbed " + is_true( z.head_gibbed ) +
		" has_legs " + is_true( z.has_legs ) + " eye_glow " + is_true( z.eye_glow_on ) +
		" j_head-origin " + ( z getTagOrigin( "j_head" ) - z.origin ) +
		" j_knee_ri-origin " + ( z getTagOrigin( "j_knee_ri" ) - z.origin ) );
}

gibtest_shoot( tokens )
{
	attacker = gibtest_player( tokens, 1 );
	if ( !isDefined( attacker ) || tokens.size < 4 )
	{
		codolprintln( "CoDOL gibtest: shoot needs <client> <bone> <weapon> [health]" );
		return;
	}
	z = gibtest_zombie( attacker.origin );
	if ( !isDefined( z ) )
	{
		codolprintln( "CoDOL gibtest: no live zombie" );
		return;
	}
	if ( tokens.size > 4 )
		z.health = int( tokens[ 4 ] );

	// magicBullet's damage reaches the watcher without a weapon name, which
	// then reads the attacker's held weapon, as a real shot would carry it.
	attacker giveWeapon( tokens[ 3 ] );
	attacker setSpawnWeapon( tokens[ 3 ] );

	target = z getTagOrigin( tokens[ 2 ] );
	dir = vectorNormalize( ( attacker.origin[ 0 ] - z.origin[ 0 ], attacker.origin[ 1 ] - z.origin[ 1 ], 0 ) );
	if ( lengthSquared( dir ) < 0.01 )
		dir = ( 1, 0, 0 );
	start = target + ( dir[ 0 ] * 48, dir[ 1 ] * 48, 0 );

	gibtest_describe( z, "before shoot " + tokens[ 2 ] );
	magicBullet( tokens[ 3 ], start, target, attacker );
	wait( 0.5 );
	gibtest_describe( z, "after shoot " + tokens[ 2 ] );
	gibtest_pieces();
}

gibtest_blast( tokens )
{
	attacker = gibtest_player( tokens, 1 );
	if ( !isDefined( attacker ) || tokens.size < 4 )
	{
		codolprintln( "CoDOL gibtest: blast needs <client> <bone> <damage>" );
		return;
	}
	z = gibtest_zombie( attacker.origin );
	if ( !isDefined( z ) )
	{
		codolprintln( "CoDOL gibtest: no live zombie" );
		return;
	}
	point = z getTagOrigin( tokens[ 2 ] );
	gibtest_describe( z, "before blast " + tokens[ 2 ] );
	z doDamage( int( tokens[ 3 ] ), point, attacker, attacker, 0, "MOD_GRENADE_SPLASH" );
	wait( 0.5 );
	gibtest_describe( z, "after blast " + tokens[ 2 ] );
	gibtest_pieces();
}

gibtest_force( tokens )
{
	if ( tokens.size < 2 )
	{
		codolprintln( "CoDOL gibtest: gib needs a ref" );
		return;
	}
	origin = undefined;
	if ( isDefined( level.players ) && level.players.size > 0 )
		origin = level.players[ 0 ].origin;
	z = gibtest_zombie( origin );
	if ( !isDefined( z ) || !isDefined( z.a ) )
	{
		codolprintln( "CoDOL gibtest: no live zombie" );
		return;
	}
	gibtest_describe( z, "before gib " + tokens[ 1 ] );
	if ( tokens[ 1 ] == "head" )
		z maps\mp\gametypes\_zombiemode_gib::zombie_head_gib();
	else
	{
		z.a.gib_ref = tokens[ 1 ];
		z maps\mp\gametypes\_zombiemode_gib::do_gib();
		if ( z.health > 0 && tokens[ 1 ] != "right_arm" && tokens[ 1 ] != "left_arm" && tokens[ 1 ] != "guts" )
			z maps\mp\gametypes\_zombiemode_spawner::make_crawler();
	}
	wait( 0.2 );
	gibtest_describe( z, "after gib " + tokens[ 1 ] );
	gibtest_pieces();
}

gibtest_status()
{
	count = 0;
	if ( isDefined( level.zombies ) )
	{
		for ( i = 0; i < level.zombies.size; i++ )
		{
			if ( !isDefined( level.zombies[ i ] ) || !isAlive( level.zombies[ i ] ) )
				continue;
			gibtest_describe( level.zombies[ i ], "status" );
			count++;
		}
	}
	codolprintln( "CoDOL gibtest: " + count + " live zombies" );
	gibtest_pieces();
}

gibtest_pieces()
{
	live = 0;
	for ( i = 0; i < level.zm_gib_pieces.size; i++ )
	{
		piece = level.zm_gib_pieces[ i ];
		if ( !isDefined( piece ) )
			continue;
		live++;
		codolprintln( "CoDOL gibtest: piece " + piece getEntityNumber() + " " + piece.classname + " model " + piece.model + " at " + piece.origin );
	}
	codolprintln( "CoDOL gibtest: " + live + " live pieces" );
}
