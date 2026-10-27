// survival: the enemy helicopters -- the chopper bosses and the juggernaut
// drop.
//
// Ported from Infinity Ward's Modern Warfare 3 Spec Ops Survival:
//   * spawn_chopper_boss / maps\_chopperboss chopper_boss_behavior_little_bird
//     (IW5 scripts\1564.gsc, 1561.gsc): the attack chopper flies the level's
//     so_chopper_boss_path_struct network (script_linkname / script_linkto),
//     stops at a node that sees a player and fires on him; the missile chopper
//     fires rockets as well.  IW5 chopper_boss_sentient makes it a priority
//     target (threatbias 10000); here it is in level.helis on the axis team,
//     so the multiplayer SAM turret, stinger lock-on and EMP find it.
//   * spawn_juggernaut / drop_jug_by_chopper (1564.gsc): the Mi-17 flies a
//     drop_path_start chain to the node with script_unload, the juggernaut
//     gets out under it, and the chopper flies on and leaves.  IW5 fast-ropes
//     him down; this server's actor has no rope animation, so he is set on
//     the ground under the hover.
// The helicopters are this server's ownerless spawnHelicopter (the native
// helicopter path, G_SpawnHelicopter) on the multiplayer vehicle definitions
// (cobra_mp, pavelow_mp) the multiplayer killstreaks use, with the models the
// survival zone and common_mp carry.  The path data is the level's
// maps\mp\survival\<level>.gsc (level.survival_heli_nodes).

precache()
{
	precacheVehicle( "cobra_mp" );
	precacheVehicle( "pavelow_mp" );

	level.survival_chopper_model = pick_model( "vehicle_little_bird_armed", "vehicle_little_bird_armed" );
	level.survival_missile_chopper_model = pick_model( "vehicle_mi24p_hind_desert", "vehicle_mi24p_hind_mp" );
	level.survival_drop_chopper_model = pick_model( "vehicle_mi17_woodland_fly_cheap", "vehicle_pavelow" );

	level.survival_chopper_gun = "cobra_20mm_mp";
	level.survival_chopper_rocket = "rpg_mp";
	if ( codol_isWeaponMounted( level.survival_chopper_gun ) )
		precacheItem( level.survival_chopper_gun );
	if ( codol_isWeaponMounted( level.survival_chopper_rocket ) )
		precacheItem( level.survival_chopper_rocket );
	level._effect[ "survival_chopper_explode" ] = loadFx( "explosions/tanker_explosion" );
	level._effect[ "survival_drop_smoke" ] = loadFx( "smoke/signal_smoke_airdrop" );
}

// The later build's model when the survival zone carries it, else the
// multiplayer one.
pick_model( preferred, fallback )
{
	if ( codescripts\character::modelMounted( preferred ) )
	{
		precacheModel( preferred );
		return preferred;
	}
	precacheModel( fallback );
	return fallback;
}

// IW5 chopper_boss_locs_populate / survival_drop_chopper_init over the
// level's helicopter structs.
init()
{
	level.survival_choppers = [];
	level.survival_boss_nodes = [];
	level.survival_drop_starts = [];
	if ( !isDefined( level.helis ) )
		level.helis = [];
	nodes = [];
	if ( isDefined( level.survival_heli_nodes ) )
		nodes = level.survival_heli_nodes;
	byLink = [];
	byName = [];
	for ( i = 0; i < nodes.size; i++ )
	{
		n = nodes[ i ];
		if ( isDefined( n.script_linkname ) )
			byLink[ n.script_linkname ] = n;
		if ( isDefined( n.targetname ) && n.targetname != "drop_path_start" && n.targetname != "chopper_boss_path_start" )
			byName[ n.targetname ] = n;
		if ( isDefined( n.script_noteworthy ) && n.script_noteworthy == "so_chopper_boss_path_struct" )
			level.survival_boss_nodes[ level.survival_boss_nodes.size ] = n;
		if ( isDefined( n.targetname ) && n.targetname == "drop_path_start" )
			level.survival_drop_starts[ level.survival_drop_starts.size ] = n;
	}
	for ( i = 0; i < level.survival_boss_nodes.size; i++ )
	{
		n = level.survival_boss_nodes[ i ];
		n.links = [];
		if ( !isDefined( n.script_linkto ) )
			continue;
		names = strTok( n.script_linkto, " " );
		for ( j = 0; j < names.size; j++ )
		{
			if ( isDefined( byLink[ names[ j ] ] ) )
				n.links[ n.links.size ] = byLink[ names[ j ] ];
		}
	}
	for ( i = 0; i < nodes.size; i++ )
	{
		n = nodes[ i ];
		if ( isDefined( n.target ) && isDefined( byName[ n.target ] ) )
			n.next = byName[ n.target ];
	}
	codolprintln( "CoDOL survival: " + level.survival_boss_nodes.size + " chopper boss nodes, " +
		level.survival_drop_starts.size + " drop paths" );
}

living_choppers()
{
	n = 0;
	out = [];
	for ( i = 0; i < level.survival_choppers.size; i++ )
	{
		c = level.survival_choppers[ i ];
		if ( isDefined( c ) && !( isDefined( c.survival_dead ) && c.survival_dead ) )
			out[ out.size ] = c;
	}
	level.survival_choppers = out;
	return out.size;
}

// --------------------------------------------------------------------------
// Helicopters
// --------------------------------------------------------------------------

spawn_heli( origin, angles, vehicle, model )
{
	heli = spawnHelicopter( undefined, origin, angles, vehicle, model );
	if ( !isDefined( heli ) )
		return undefined;
	heli.team = "axis";
	heli.survival_enemy = true;
	heli setMaxPitchRoll( 25, 35 );
	heli Vehicle_SetSpeed( 60, 25 );
	heli setYawSpeed( 90, 45, 45 );
	heli setHoverParams( 40, 20, 15 );
	return heli;
}

// Fly to `origin`; true when it got there before the timeout.
fly_to( origin, timeout, stop )
{
	self endon( "death" );
	self endon( "survival_heli_dead" );
	if ( !isDefined( stop ) )
		stop = 1;
	self setVehGoalPos( origin, stop );
	self.survival_at_goal = false;
	self thread goal_watch();
	for ( t = 0; t < timeout && !self.survival_at_goal; t += 0.25 )
		wait( 0.25 );
	return self.survival_at_goal;
}

goal_watch()
{
	self endon( "death" );
	self endon( "survival_heli_dead" );
	self notify( "survival_goal_watch" );
	self endon( "survival_goal_watch" );
	self waittill( "goal" );
	self.survival_at_goal = true;
}

// --------------------------------------------------------------------------
// The chopper bosses (AI table rows chopper / missile_chopper)
// --------------------------------------------------------------------------

spawn_chopper_boss( ref, scale )
{
	if ( ref == "jug_drop_chopper" )
		return undefined;
	if ( !level.survival_boss_nodes.size )
	{
		codolprintln( "CoDOL survival: " + ref + " has no chopper boss path on " + getDvar( "mapname" ) );
		return undefined;
	}
	t = level.survival_ai[ ref ];
	start = boss_start();
	model = level.survival_chopper_model;
	if ( ref == "missile_chopper" )
		model = level.survival_missile_chopper_model;
	// IW5 brings it in from far out along the path start's facing.
	entry = start.origin + ( 0, 0, 1500 );
	heli = spawn_heli( entry, start.angles, "cobra_mp", model );
	if ( !isDefined( heli ) )
	{
		codolprintln( "CoDOL survival: " + ref + " could not be spawned" );
		return undefined;
	}
	if ( !isDefined( scale ) )
		scale = 1.0;
	health = 1100;
	xp = 600;
	if ( isDefined( t ) )
	{
		health = t.health;
		xp = t.xp;
	}
	heli.maxhealth = int( health * scale );
	heli.damageTaken = 0;
	heli.survival_ref = ref;
	heli.survival_xp = xp;
	heli.survival_missiles = ( ref == "missile_chopper" );
	heli setCanDamage( true );
	heli.damageCallback = ::chopper_damage;
	level.helis[ heli getEntityNumber() ] = heli;
	level.survival_choppers[ level.survival_choppers.size ] = heli;
	heli thread chopper_boss_behaviour( start );
	heli thread chopper_attack();
	codolprintln( "CoDOL survival: boss " + ref + " spawned, health " + heli.maxhealth );
	return heli;
}

boss_start()
{
	starts = [];
	for ( i = 0; i < level.survival_boss_nodes.size; i++ )
	{
		n = level.survival_boss_nodes[ i ];
		if ( isDefined( n.targetname ) && n.targetname == "chopper_boss_path_start" && n.links.size )
			starts[ starts.size ] = n;
	}
	if ( !starts.size )
		return level.survival_boss_nodes[ randomInt( level.survival_boss_nodes.size ) ];
	return starts[ randomInt( starts.size ) ];
}

// IW5 chopper_boss_behavior_little_bird: from node to linked node, choosing
// one that sees the player it hunts and keeps IW5's min 2D distance from him.
chopper_boss_behaviour( node )
{
	self endon( "death" );
	self endon( "survival_heli_dead" );
	self fly_to( node.origin, 30, 1 );
	for ( ;; )
	{
		target = maps\mp\gametypes\_survival_ai::closest_player( self.origin );
		self.survival_target = target;
		next = undefined;
		if ( node.links.size )
		{
			best = undefined;
			bestScore = 0;
			for ( i = 0; i < node.links.size; i++ )
			{
				n = node.links[ i ];
				score = randomInt( 100 );
				if ( isDefined( target ) )
				{
					d = distance2d( n.origin, target.origin );
					if ( d > 128 )
						score += 1000 - int( d / 10 );
					if ( sightTracePassed( n.origin, target getEye(), false, self ) )
						score += 2000;
				}
				if ( !isDefined( best ) || score > bestScore )
				{
					best = n;
					bestScore = score;
				}
			}
			next = best;
		}
		if ( !isDefined( next ) )
			next = level.survival_boss_nodes[ randomInt( level.survival_boss_nodes.size ) ];
		node = next;
		self fly_to( node.origin, 20, 1 );
		// IW5 hovers at a stop node to fire.
		if ( isDefined( node.script_stopnode ) )
			wait( 2 + randomFloat( 3 ) );
		else
			wait( 0.5 );
	}
}

// The gun (IW4 _helicopter attack_primary's burst) and the missile chopper's
// rockets at the player it hunts.
chopper_attack()
{
	self endon( "death" );
	self endon( "survival_heli_dead" );
	gun = codol_isWeaponMounted( level.survival_chopper_gun );
	rockets = self.survival_missiles && codol_isWeaponMounted( level.survival_chopper_rocket );
	for ( ;; )
	{
		wait( 1 );
		target = self.survival_target;
		if ( !isDefined( target ) || !isAlive( target ) || ( isDefined( target.laststand ) && target.laststand ) )
			continue;
		if ( !sightTracePassed( self.origin + ( 0, 0, -64 ), target getEye(), false, self ) )
			continue;
		if ( gun )
		{
			self setVehWeapon( level.survival_chopper_gun );
			self setTurretTargetEnt( target, ( 0, 0, 40 ) );
			wait( 0.5 );
			shots = 10 + randomInt( 10 );
			for ( i = 0; i < shots && isDefined( target ) && isAlive( target ); i++ )
			{
				self setTurretTargetEnt( target, ( randomFloatRange( -48, 48 ), randomFloatRange( -48, 48 ), 40 ) );
				self fireWeapon( "tag_flash" );
				wait( 0.1 );
			}
		}
		if ( rockets && isDefined( target ) && isAlive( target ) )
		{
			for ( i = 0; i < 2; i++ )
			{
				start = self.origin + ( 0, 0, -96 );
				magicBullet( level.survival_chopper_rocket, start, target.origin + ( randomFloatRange( -64, 64 ), randomFloatRange( -64, 64 ), 0 ) );
				wait( 0.4 );
			}
		}
		wait( 1.5 + randomFloat( 1.5 ) );
	}
}

// CodeCallback_VehicleDamage for an enemy chopper: the players' damage counts
// against the AI table's health (the native helicopter keeps 99999).
chopper_damage( inflictor, attacker, damage, dFlags, meansOfDeath, weapon, point, dir, hitLoc, timeOffset, modelIndex, partName )
{
	if ( isDefined( self.survival_dead ) && self.survival_dead )
		return;
	if ( isDefined( attacker ) && !isPlayer( attacker ) && ( isAI( attacker ) || ( isDefined( attacker.survival_enemy ) && attacker.survival_enemy ) ) )
		return;
	if ( isDefined( attacker ) && attacker == self )
		return;
	// IW5's choppers take explosives harder (rockets, the SAM).
	if ( meansOfDeath == "MOD_PROJECTILE" || meansOfDeath == "MOD_PROJECTILE_SPLASH" || meansOfDeath == "MOD_EXPLOSIVE" )
		damage = damage * 2;
	self.damageTaken += damage;
	if ( isDefined( attacker ) && isPlayer( attacker ) )
		attacker thread maps\mp\gametypes\_damagefeedback::updateDamageFeedback( "standard" );
	self Vehicle_FinishDamage( inflictor, attacker, damage, dFlags, meansOfDeath, weapon, point, dir, hitLoc, timeOffset, modelIndex, partName );
	if ( self.damageTaken >= self.maxhealth )
		self thread chopper_die( attacker );
}

// The kill: credits and a kill to whoever brought it down (or owns what did),
// a spin and an explosion.
chopper_die( attacker )
{
	if ( isDefined( self.survival_dead ) && self.survival_dead )
		return;
	self.survival_dead = true;
	self notify( "survival_heli_dead" );
	self notify( "crashing" );
	level.helis[ self getEntityNumber() ] = undefined;
	level notify( "survival_axis_died" );
	killer = maps\mp\gametypes\_survival_ai::credit_owner( attacker );
	if ( isDefined( killer ) )
	{
		killer maps\mp\gametypes\_survival::add_credits( self.survival_xp, "chopper" );
		killer maps\mp\gametypes\_survival_ai::award_kill();
	}
	codolprintln( "CoDOL survival: " + self.survival_ref + " destroyed" );
	self clearTargetYaw();
	self setYawSpeed( 400, 100, 100 );
	self setTargetYaw( self.angles[ 1 ] + 170 );
	self Vehicle_SetSpeed( 25, 10 );
	self setVehGoalPos( self.origin + ( randomFloatRange( -300, 300 ), randomFloatRange( -300, 300 ), -400 ), 0 );
	wait( 2 );
	if ( isDefined( level._effect[ "survival_chopper_explode" ] ) )
		playFx( level._effect[ "survival_chopper_explode" ], self.origin );
	playSoundAtPosition( "cobra_helicopter_crash", self.origin );
	radiusDamage( self.origin, 256, 120, 20 );
	self delete();
}

// The survival EMP: every enemy chopper goes down.
emp_choppers( attacker )
{
	living_choppers();
	choppers = level.survival_choppers;
	for ( i = 0; i < choppers.size; i++ )
		choppers[ i ] thread chopper_die( attacker );
	return choppers.size;
}

// --------------------------------------------------------------------------
// The juggernaut drop
// --------------------------------------------------------------------------

// Launches one drop; false when the map has no drop path.  The boss is
// pending (level.survival_boss_pending) until he is on the ground.
drop_juggernaut( ref, scale )
{
	if ( !isDefined( level.survival_drop_starts ) || !level.survival_drop_starts.size )
		return false;
	start = level.survival_drop_starts[ randomInt( level.survival_drop_starts.size ) ];
	path = [];
	n = start;
	unload = undefined;
	for ( i = 0; isDefined( n ) && i < 32; i++ )
	{
		path[ path.size ] = n;
		if ( !isDefined( unload ) && isDefined( n.script_unload ) )
			unload = path.size - 1;
		n = n.next;
	}
	if ( !isDefined( unload ) )
		return false;
	heli = spawn_heli( start.origin, start.angles, "pavelow_mp", level.survival_drop_chopper_model );
	if ( !isDefined( heli ) )
		return false;
	// IW5 godon: the drop chopper cannot be shot down.
	heli setCanDamage( false );
	heli Vehicle_SetSpeed( 60 + randomInt( 15 ), 30 );
	level.survival_boss_pending++;
	heli thread drop_flight( path, unload, ref, scale );
	return true;
}

drop_flight( path, unload, ref, scale )
{
	ai = undefined;
	for ( i = 1; i < path.size; i++ )
	{
		stop = 0;
		if ( i == unload )
			stop = 1;
		self fly_to( path[ i ].origin, 25, stop );
		if ( i != unload )
			continue;
		// IW5 chopper_drop_smoke_at_unloading and the drop.
		ground = groundpos( path[ i ].origin, self );
		if ( isDefined( level._effect[ "survival_drop_smoke" ] ) )
			playFx( level._effect[ "survival_drop_smoke" ], ground );
		wait( 2 );
		for ( tries = 0; !isDefined( ai ) && tries < 10 && !level.survival_over; tries++ )
		{
			ai = maps\mp\gametypes\_survival_ai::spawn_boss( ref, ground, path[ i ].angles, scale );
			if ( !isDefined( ai ) )
				wait( 1 );
		}
		if ( isDefined( ai ) )
			codolprintln( "CoDOL survival: boss " + ref + " dropped at " + ground );
		else
			codolprintln( "CoDOL survival: boss " + ref + " drop at " + ground + " failed" );
		level.survival_boss_pending--;
		self Vehicle_SetSpeed( 80, 40 );
	}
	// IW5 script_vehicle_selfremove: gone at the end of its path.
	self delete();
}

groundpos( origin, ignore )
{
	trace = bulletTrace( origin, origin + ( 0, 0, -10000 ), false, ignore );
	return trace[ "position" ];
}
