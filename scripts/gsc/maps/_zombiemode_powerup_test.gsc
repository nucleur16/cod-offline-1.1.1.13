// Developer-only power-up diagnostic tools preserved from rawfiles a1dbc86.
// They drive real engine inputs/current power-up handlers; they never replace gameplay.
// Inert unless developer=1 AND scr_zm_powerup_test_enabled=1 at map startup.
// Commands use scr_zm_powerup_test: drop/pandora/fire/state/zap/sentrydiag/
// gather/press/dive/dmtest/switch <clientnum> <argument> [extra].
#include maps\_zombiemode_utility;
// The ZM utility owns all helpers below; importing common utility too duplicates VM exports.

main()
{
	if ( getDvarInt( "developer" ) == 0 || getDvarInt( "scr_zm_powerup_test_enabled" ) == 0 )
		return;
	level thread powerup_test_watch();
}

powerup_test_deathmachine( phase, yaw )
{
	self endon( "disconnect" );
	idle = 8192;
	
	
	if ( self getCurrentWeapon() != "none" )
		self switchToWeapon( self getCurrentWeapon() );
	self codol_botPressButtons( idle, 60 );
	if ( isDefined( yaw ) )
		self setPlayerAngles( ( 0, yaw, 0 ) );
	wait( 0.5 );

	if ( phase == "grab" )
	{
		maps\_zombiemode_powerups::specific_powerup_drop( "minigun", self.origin - ( 0, 0, 40 ) );
		wait( 3.0 );
	}
	codolPrintLn( "CoDOL zm dmtest [" + phase + "]: " + self.name + " holds " + self getCurrentWeapon() +
		", has_powerup_weapon " + is_true( self.has_powerup_weapon ) );

	start = self.origin;
	self codol_botPressButtons( idle + 2, 1.5, 1 );
	self thread powerup_test_sample( "dm sprint", 1.4 );
	wait( 1.5 );
	self codol_botPressButtons( idle, 60 );
	codolPrintLn( "CoDOL zm dmtest [" + phase + "]: sprint covered " + int( distance( start, self.origin ) ) + " units in 1.5 s" );
	self setOrigin( start );
	wait( 0.6 );

	self codol_botPressButtons( idle + 4, 0.4 );
	codolPrintLn( "CoDOL zm dmtest [" + phase + "]: melee pressed (see CoDOL melee: fire)" );
	wait( 1.0 );
	self codol_botPressButtons( idle, 60 );
	wait( 0.5 );

	self codol_botPressButtons( idle + 2, 0.8, 1 );
	wait( 0.7 );
	self codol_botPressButtons( idle + 8388608 + 256 + 2, 1.2, 1 );
	self thread powerup_test_sample( "dm dive", 1.0, 0.05 );
	wait( 1.2 );
	self codol_botPressButtons( idle, 60 );
	codolPrintLn( "CoDOL zm dmtest [" + phase + "]: after the dive press, stance " + self getStance() );
	self setStance( "stand" );
	self setOrigin( start );
	wait( 1.0 );

	primaries = self getWeaponsListPrimaries();
	other = "none";
	for ( i = 0; i < primaries.size; i++ )
	{
		if ( primaries[ i ] != self getCurrentWeapon() )
			other = primaries[ i ];
	}
	held = self getCurrentWeapon();
	if ( other != "none" )
	{
		self switchToWeapon( other );
		wait( 1.5 );
	}
	codolPrintLn( "CoDOL zm dmtest [" + phase + "]: switch " + held + " -> " + other + " asked; holds " + self getCurrentWeapon() );

	all = getEntArray( "weapon_upgrade", "targetname" );
	trigs = [];
	for ( i = 0; i < all.size; i++ )
	{
		if ( isDefined( all[ i ].zombie_weapon_upgrade ) && !self hasWeapon( all[ i ].zombie_weapon_upgrade ) )
			trigs[ trigs.size ] = all[ i ];
	}
	if ( trigs.size > 0 )
	{
		if ( self.score < 5000 )
			self maps\_zombiemode_score::add_to_player_score( 5000 );
		self setOrigin( trigs[ 0 ].origin - ( 0, 0, 40 ) );
		wait( 0.3 );
		before = self.score;
		self codol_botPressButtons( idle + 40, 1.0 );
		wait( 2.0 );
		codolPrintLn( "CoDOL zm dmtest [" + phase + "]: native use on " + trigs[ 0 ].zombie_weapon_upgrade +
			" wall buy, score " + before + " -> " + self.score + ", holds " + self getCurrentWeapon() );
		self setOrigin( start );
	}
	self codol_botPressButtons( idle, 60 );
}

powerup_test_fire( seconds )
{
	self endon( "disconnect" );

	weapon = self getCurrentWeapon();
	codolPrintLn( "CoDOL zm powerup test: " + self.name + " fires " + weapon + " for " + seconds + " s" );
	killed_before = level.total_zombies_killed;

	end_time = getTime() + int( seconds * 1000 );
	while ( getTime() < end_time )
	{
		target = get_closest( self.origin, maps\_zombiemode_weap_tesla::tesla_sorted_zombies( self.origin ) );
		if ( isDefined( target ) )
		{
			eye = self.origin + ( 0, 0, 60 );
			self setPlayerAngles( vectorToAngles( ( target.origin + ( 0, 0, 40 ) ) - eye ) );
		}
		self codol_botPressButtons( 1, 0.15 );
		wait( 0.1 );
	}

	weapon = self getCurrentWeapon();
	ammo = "";
	if ( weapon != "none" )
		ammo = " clip " + self getWeaponAmmoClip( weapon ) + " stock " + self getWeaponAmmoStock( weapon );
	codolPrintLn( "CoDOL zm powerup test: " + self.name + " stopped firing, holding " + weapon + ammo +
		"; zombies killed meanwhile " + ( level.total_zombies_killed - killed_before ) );
}

powerup_test_gather( radius )
{
	eye = self.origin + ( 0, 0, 60 );
	moved = 0;
	zombies = maps\_zombiemode_weap_tesla::tesla_sorted_zombies( self.origin );
	for ( i = 0; i < zombies.size; i++ )
	{
		for ( tries = 0; tries < 12; tries++ )
		{
			yaw = ( ( i * 5 + tries ) * 30 ) % 360;
			spot = groundpos( self.origin + vector_scale( anglesToForward( ( 0, yaw, 0 ) ), radius ) + ( 0, 0, 30 ) );
			if ( abs( spot[ 2 ] - self.origin[ 2 ] ) > 40 )
				continue;
			if ( !bulletTracePassed( eye, spot + ( 0, 0, 40 ), false, self ) )
				continue;
			zombies[ i ] forceTeleport( spot, ( 0, yaw + 180, 0 ) );
			moved++;
			break;
		}
	}
	codolPrintLn( "CoDOL zm powerup test: gathered " + moved + " of " + zombies.size + " zombies " + radius + " units from " + self.name );
}

powerup_test_player( client_num )
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] getEntityNumber() == client_num )
			return players[ i ];
	}
	return undefined;
}

powerup_test_sample( mask, seconds, step )
{
	self endon( "disconnect" );
	if ( !isDefined( step ) )
		step = 0.2;
	end_time = getTime() + int( seconds * 1000 );
	while ( getTime() < end_time )
	{
		v = self getVelocity();
		codolPrintLn( "CoDOL zm powerup test: press " + mask + " " + self.name + " holds " + self getCurrentWeapon() +
			" stance " + self getStance() + " speed " + int( length( ( v[ 0 ], v[ 1 ], 0 ) ) ) + " vz " + int( v[ 2 ] ) +
			" at " + self.origin );
		wait( step );
	}
}

powerup_test_watch()
{
	setDvar( "scr_zm_powerup_test", "" );

	for ( ;; )
	{
		wait( 0.5 );

		request = getDvar( "scr_zm_powerup_test" );
		if ( request == "" )
			continue;
		setDvar( "scr_zm_powerup_test", "" );

		tokens = strTok( request, " " );
		if ( tokens.size < 3 )
			continue;

		player = powerup_test_player( int( tokens[ 1 ] ) );
		if ( !isDefined( player ) || !isAlive( player ) )
		{
			codolPrintLn( "CoDOL zm powerup test: no live client " + tokens[ 1 ] );
			continue;
		}

		switch ( tokens[ 0 ] )
		{
		case "drop":
			if ( !isDefined( level.zombie_powerups[ tokens[ 2 ] ] ) )
			{
				codolPrintLn( "CoDOL zm powerup test: " + tokens[ 2 ] + " is not registered" );
				break;
			}
			
			
			offset = ( 0, 0, 0 );
			if ( tokens.size > 3 )
				offset = ( float( tokens[ 3 ] ), 0, 0 );
			codolPrintLn( "CoDOL zm powerup test: dropping " + tokens[ 2 ] + " at " + player.name + " + " + offset );
			maps\_zombiemode_powerups::specific_powerup_drop( tokens[ 2 ], player.origin + offset - ( 0, 0, 40 ) );
			break;
		case "pandora":
			if ( !isDefined( level.quantum_bomb_results[ tokens[ 2 ] ] ) )
			{
				codolPrintLn( "CoDOL zm powerup test: pandora result " + tokens[ 2 ] + " is not registered" );
				break;
			}
			player thread maps\_zombiemode_powerups::pandora_powerup( player.origin, tokens[ 2 ] );
			break;
		case "fire":
			if ( tokens.size > 3 && player hasWeapon( tokens[ 3 ] ) )
				player setSpawnWeapon( tokens[ 3 ] );
			player thread powerup_test_fire( float( tokens[ 2 ] ) );
			break;
		case "state":
			w = player getCurrentWeapon();
			codolPrintLn( "CoDOL zm powerup test: " + player.name + " holds " + w + ", primaries " + player getWeaponsListPrimaries().size +
				", remote " + player isRemoteControlling() );
			break;
		case "zap":
			
			
			
			
			z = get_closest( player.origin, maps\_zombiemode_weap_tesla::tesla_sorted_zombies( player.origin ) );
			if ( !isDefined( z ) )
			{
				codolPrintLn( "CoDOL zm powerup test: no zombie to zap" );
				break;
			}
			codolPrintLn( "CoDOL zm powerup test: tesla projectile hit on zombie " + z getEntityNumber() + " from " + player.name );
			z doDamage( 1, player.origin, player, player, 0, "MOD_PROJECTILE", 0, "tesla_gun_zm" );
			break;
		case "sentrydiag":
			if ( isDefined( level.zm_sentries ) )
			{
				for ( k = 0; k < level.zm_sentries.size; k++ )
					level.zm_sentries[ k ] maps\_zombiemode_auto_turret::auto_turret_diag( level.zm_sentries[ k ] getTurretTarget( false ) );
			}
			break;
		case "gather":
			player powerup_test_gather( float( tokens[ 2 ] ) );
			break;
		case "press":
			seconds = 1.0;
			direction = 0;
			if ( tokens.size > 3 )
				seconds = float( tokens[ 3 ] );
			if ( tokens.size > 4 )
				direction = int( tokens[ 4 ] );
			player codol_botPressButtons( int( tokens[ 2 ] ), seconds, direction );
			player thread powerup_test_sample( tokens[ 2 ], seconds );
			break;
		case "dive":
			player codol_botPressButtons( 2, 1.3, 1 );
			player thread powerup_test_sample( "2", 1.2 );
			wait( 1.2 );
			player codol_botPressButtons( 8388866, 1.0, 1 );
			player thread powerup_test_sample( "8388866", 1.2 );
			break;
		case "dmtest":
			yaw = undefined;
			if ( tokens.size > 3 )
				yaw = float( tokens[ 3 ] );
			player thread powerup_test_deathmachine( tokens[ 2 ], yaw );
			break;
		case "switch":
			codolPrintLn( "CoDOL zm powerup test: " + player.name + " asks for " + tokens[ 2 ] + " holding " + player getCurrentWeapon() );
			player switchToWeapon( tokens[ 2 ] );
			break;
		}
	}
}
