// The zm3 Cyborg's bouncingbetty_zm.  C1's item description says a nearby
// robot zombie makes the placed mine spring and explode.  The shipped
// common_gametype_zm WeaponDef gives an explosion radius of 200, inner damage
// 200 and outer damage 50 (+0x778/+0x780/+0x784).  Its timedDetonation byte
// (+0xbe2) is zero.  The retained G_FireGrenade nevertheless gives an idle
// missile a 30-second fallback nextthink, so replace it after the native
// stationary notification with a planted model rather than letting it expire.

init()
{
	precacheModel( "projectile_bouncing_betty_grenade" );
	level.zm_betty_explosion = loadFx( "weapon/bouncing_betty/fx_explosion_betty_generic" );
	maps\mp\gametypes\_zombiemode_equipment::register_equipment( "bouncingbetty_zm", ::on_betty_thrown );
	codolprintln( "CoDOL zm betty: bouncingbetty_zm proximity registered" );
}

// Called by the Zombies equipment registry on the engine's grenade_fire.
// self is the live grenade missile; player is the owner.
on_betty_thrown( player, weapon )
{
	self thread wait_for_betty( player );
}

wait_for_betty( owner )
{
	self endon( "death" );
	self waittill( "stationary" );

	if ( !isDefined( owner ) || !isPlayer( owner ) )
	{
		self delete();
		return;
	}

	origin = self.origin;
	trace = bulletTrace( origin + ( 0, 0, 16 ), origin - ( 0, 0, 64 ), false, self );
	if ( trace[ "fraction" ] < 1 )
		origin = trace[ "position" ];

	mine = spawn( "script_model", origin );
	mine setModel( "projectile_bouncing_betty_grenade" );
	mine.angles = ( 0, randomInt( 360 ), 0 );
	mine thread planted_betty( owner );
	owner thread remove_betty_on_disconnect( mine );
	mine thread remove_betty_on_game_end();
	codolprintln( "CoDOL zm betty: planted by " + owner.name + " at " + origin );
	self delete();
}

// self = planted model.  Two seconds follows the established MP arming delay;
// the C1 Zombies asset does not specify a delay.  No guessed placed-mine cap or
// lifetime is imposed here.
planted_betty( owner )
{
	self endon( "death" );
	wait( 2.0 );
	for ( ;; )
	{
		if ( self betty_has_nearby_zombie() )
		{
			self playSound( "betty_activated" );
			codolprintln( "CoDOL zm betty: triggered at " + self.origin );
			wait( 0.3 );
			origin = self.origin;
			playFx( level.zm_betty_explosion, origin );
			// Entity radiusDamage supplies weapon and MOD_EXPLOSIVE to the
			// normal damage path.  Zombies' player callback excludes this
			// placeable mine from friendly/self damage.
			if ( isDefined( owner ) && isPlayer( owner ) )
				self radiusDamage( origin, 200, 200, 50, owner, "MOD_EXPLOSIVE", "bouncingbetty_zm" );
			else
				self radiusDamage( origin, 200, 200, 50 );
			self delete();
			return;
		}
		wait( 0.2 );
	}
}

remove_betty_on_disconnect( mine )
{
	self waittill( "disconnect" );
	if ( isDefined( mine ) )
		mine delete();
}

remove_betty_on_game_end()
{
	self endon( "death" );
	level waittill( "end_game" );
	self delete();
}

// Poll the existing live-zombie list.  A cover trace keeps zombies on the
// other side of a wall from triggering a mine.
betty_has_nearby_zombie()
{
	if ( !isDefined( level.zombies ) )
		return false;
	origin = self.origin;
	for ( i = 0; i < level.zombies.size; i++ )
	{
		zombie = level.zombies[ i ];
		if ( !isDefined( zombie ) || !isAlive( zombie ) )
			continue;
		if ( abs( zombie.origin[ 2 ] - origin[ 2 ] ) > 72 )
			continue;
		if ( distanceSquared( ( zombie.origin[ 0 ], zombie.origin[ 1 ], 0 ),
			( origin[ 0 ], origin[ 1 ], 0 ) ) > 100 * 100 )
			continue;
		if ( !sightTracePassed( origin + ( 0, 0, 8 ),
			zombie.origin + ( 0, 0, 30 ), false, self ) )
			continue;
		return true;
	}
	return false;
}
