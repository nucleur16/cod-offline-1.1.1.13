// Developer-only test verbs for the zombies player-damage rules
// (_zombiemode.gsc Callback_PlayerDamage: players never hurt each other).
//
// Runs only on a developer server (the live server runs developer 0).  Drive it
// through the dvar scr_codol_fftest:
//
//   addbot                                  add one test client
//   watch <client>                          log every hit the client takes
//   bullet <attacker> <victim> <weapon>     magicBullet from the attacker's eye
//                                           at the victim's chest (real
//                                           Bullet_Fire / projectile path)
//   hit <attacker> <victim> <mod> <weapon> <damage>
//                                           doDamage through native G_Damage
//   radius <attacker> <victim> <mod> <weapon> <damage>
//                                           radiusDamage at the victim
//   zhit <victim> <damage>                  the nearest zombie hits the victim
//   zdmg <attacker> <weapon>                magicBullet at the nearest zombie
//   tozombie <client>                       stand the client by a zombie
//   move <client> <dx> <dy> <dz>            shift the client
//   heal <client>                           full health, out of last stand
//   jug <client>                            buy Juggernog for the client
//   invuln <client> <0|1>                   the client takes no damage
//
// <attacker> may equal <victim> for self damage.

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;

	setDvar( "scr_codol_fftest", "" );
	level thread fftest_watch();
}

fftest_watch()
{
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_codol_fftest" );
		if ( request == "" )
			continue;
		setDvar( "scr_codol_fftest", "" );

		tokens = strTok( request, " " );
		switch ( tokens[ 0 ] )
		{
		case "addbot":
			addtestclient();
			codolprintln( "CoDOL fftest: addtestclient" );
			break;

		case "watch":
			p = fftest_player( tokens, 1 );
			if ( isDefined( p ) )
				p thread fftest_log_damage();
			break;

		case "bullet":
		case "hit":
		case "radius":
			level thread fftest_hit( tokens );
			break;

		case "zhit":
			level thread fftest_zombie_hit( tokens );
			break;

		case "zdmg":
			level thread fftest_zombie_damage( tokens );
			break;

		case "tozombie":
			p = fftest_player( tokens, 1 );
			z = fftest_nearest_zombie( p );
			if ( isDefined( p ) && isDefined( z ) )
			{
				p setOrigin( z.origin + ( 40, 0, 0 ) );
				codolprintln( "CoDOL fftest: " + p.name + " moved to zombie " + z getEntityNumber() );
			}
			break;

		case "move":
			p = fftest_player( tokens, 1 );
			if ( isDefined( p ) && tokens.size > 4 )
			{
				p setOrigin( p.origin + ( int( tokens[ 2 ] ), int( tokens[ 3 ] ), int( tokens[ 4 ] ) ) );
				codolprintln( "CoDOL fftest: " + p.name + " moved to " + p.origin );
			}
			break;

		case "jug":
			p = fftest_player( tokens, 1 );
			if ( isDefined( p ) )
			{
				p maps\mp\gametypes\_zombiemode_perks::give_perk( "specialty_armorvest" );
				codolprintln( "CoDOL fftest: " + p.name + " juggernog, health " + p.health + "/" + p.maxhealth );
			}
			break;

		case "invuln":
			p = fftest_player( tokens, 1 );
			if ( isDefined( p ) && tokens.size > 2 )
			{
				if ( int( tokens[ 2 ] ) )
					p.codolTestInvulnerable = true;
				else
					p.codolTestInvulnerable = undefined;
				codolprintln( "CoDOL fftest: " + p.name + " invulnerable " + tokens[ 2 ] );
			}
			break;

		case "heal":
			p = fftest_player( tokens, 1 );
			if ( isDefined( p ) )
			{
				if ( p maps\mp\gametypes\_zombiemode_laststand::player_is_in_laststand() )
					p maps\mp\gametypes\_zombiemode_laststand::revive_success();
				p.health = p.maxhealth;
				codolprintln( "CoDOL fftest: " + p.name + " health " + p.health );
			}
			break;
		}
	}
}

fftest_player( tokens, index )
{
	if ( tokens.size <= index )
		return undefined;
	num = int( tokens[ index ] );
	for ( i = 0; i < level.players.size; i++ )
	{
		if ( level.players[ i ] getEntityNumber() == num )
			return level.players[ i ];
	}
	codolprintln( "CoDOL fftest: no client " + tokens[ index ] );
	return undefined;
}

fftest_log_damage()
{
	self endon( "disconnect" );
	self notify( "fftest_log_damage" );
	self endon( "fftest_log_damage" );
	codolprintln( "CoDOL fftest: watching " + self.name );
	for ( ;; )
	{
		self waittill( "damage", amount, attacker, dir, point, mod );
		if ( !isDefined( mod ) )
			mod = "?";
		who = "world";
		if ( isDefined( attacker ) )
		{
			who = attacker.classname + " " + attacker getEntityNumber();
			if ( is_true( attacker.is_zombie ) )
				who = "zombie " + attacker getEntityNumber();
		}
		codolprintln( "CoDOL fftest: " + self.name + " took " + amount + " " + mod + " from " + who + ", health " + self.health );
	}
}

fftest_hit( tokens )
{
	attacker = fftest_player( tokens, 1 );
	victim = fftest_player( tokens, 2 );
	if ( !isDefined( attacker ) || !isDefined( victim ) )
		return;

	before = victim.health;
	codolprintln( "CoDOL fftest: " + attacker.name + " at " + attacker.origin + ", " + victim.name + " at " + victim.origin );
	switch ( tokens[ 0 ] )
	{
	case "bullet":
		if ( tokens.size < 4 )
			return;
		start = attacker getEye();
		if ( attacker == victim )
			start = victim.origin + ( 0, 0, 120 );
		magicBullet( tokens[ 3 ], start, victim.origin + ( 0, 0, 40 ), attacker );
		desc = "bullet " + tokens[ 3 ];
		break;
	case "hit":
		if ( tokens.size < 6 )
			return;
		victim doDamage( int( tokens[ 5 ] ), attacker.origin, attacker, attacker, 0, tokens[ 3 ], 0, tokens[ 4 ] );
		desc = "hit " + tokens[ 3 ] + " " + tokens[ 4 ] + " " + tokens[ 5 ];
		break;
	default:
		if ( tokens.size < 6 )
			return;
		radiusDamage( victim.origin + ( 0, 0, 30 ), 48, int( tokens[ 5 ] ), int( tokens[ 5 ] ), attacker, tokens[ 3 ], tokens[ 4 ] );
		desc = "radius " + tokens[ 3 ] + " " + tokens[ 4 ] + " " + tokens[ 5 ];
		break;
	}

	wait( 0.5 );
	if ( !isDefined( victim ) )
		return;
	codolprintln( "CoDOL fftest: " + desc + " " + attacker.name + " -> " + victim.name + ": health " + before + " -> " + victim.health +
		" laststand " + victim maps\mp\gametypes\_zombiemode_laststand::player_is_in_laststand() + " state " + victim.sessionstate );
}

fftest_nearest_zombie( player )
{
	if ( !isDefined( player ) || !isDefined( level.zombies ) )
		return undefined;
	best = undefined;
	for ( i = 0; i < level.zombies.size; i++ )
	{
		z = level.zombies[ i ];
		if ( !isDefined( z ) || !isAlive( z ) )
			continue;
		if ( !isDefined( best ) || distanceSquared( z.origin, player.origin ) < distanceSquared( best.origin, player.origin ) )
			best = z;
	}
	return best;
}

fftest_zombie_hit( tokens )
{
	victim = fftest_player( tokens, 1 );
	z = fftest_nearest_zombie( victim );
	if ( !isDefined( victim ) || !isDefined( z ) || tokens.size < 3 )
	{
		codolprintln( "CoDOL fftest: zhit needs a victim, a damage and a live zombie" );
		return;
	}
	before = victim.health;
	victim doDamage( int( tokens[ 2 ] ), z.origin, z, z, 0, "MOD_MELEE", 0, "none" );
	wait( 0.5 );
	codolprintln( "CoDOL fftest: zhit zombie " + z getEntityNumber() + " -> " + victim.name + ": health " + before + " -> " + victim.health +
		" laststand " + victim maps\mp\gametypes\_zombiemode_laststand::player_is_in_laststand() );
}

fftest_zombie_damage( tokens )
{
	attacker = fftest_player( tokens, 1 );
	z = fftest_nearest_zombie( attacker );
	if ( !isDefined( attacker ) || !isDefined( z ) || tokens.size < 3 )
	{
		codolprintln( "CoDOL fftest: zdmg needs an attacker, a weapon and a live zombie" );
		return;
	}
	num = z getEntityNumber();
	before = z.health;
	magicBullet( tokens[ 2 ], z.origin + ( 0, 0, 120 ), z.origin + ( 0, 0, 40 ), attacker );
	wait( 0.5 );
	after = "dead";
	if ( isDefined( z ) && isAlive( z ) )
		after = z.health;
	codolprintln( "CoDOL fftest: zdmg " + tokens[ 2 ] + " " + attacker.name + " -> zombie " + num + ": health " + before + " -> " + after );
}
