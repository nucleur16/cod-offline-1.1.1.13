// Outpost's authored explodable_barrel / com_barrel_benzin prefabs.
// PROVENANCE: IW4 maps/mp/_explosive_barrels.gsc, retained in this tree.
// Preserve its 150 damage cook-off, first-hit ignition, grenade detonation,
// tilted-barrel offsets, 250-unit blast and damaged model swap. CoDOL changes:
// use the map's shipped barrel_mtl_explode sound; omit MP-only barrel_mp and
// the MP shellshock compile dependency; use the existing native earthquake.
// All three props/barrel_* FX and both damaged models ship in zm_prototype_sh.
// The map's ordinary ambient/createfx emitters remain client-owned.

main()
{
	if ( isDefined( level.zm_barrels_initialized ) )
		return;
	level.zm_barrels_initialized = true;
	barrels = getEntArray( "explodable_barrel", "targetname" );
	if ( !isDefined( barrels ) || barrels.size == 0 )
		return;
	level.zm_barrel_fx = [];
	level.zm_barrel_fx[ "explode" ] = loadFx( "props/barrelexp" );
	level.zm_barrel_fx[ "ignite" ] = loadFx( "props/barrel_ignite" );
	level.zm_barrel_fx[ "burn" ] = loadFx( "props/barrel_fire_top" );
	precacheModel( "com_barrel_piece" );
	precacheModel( "com_barrel_piece2" );
	level.zm_barrel_exploding_this_frame = false;
	for ( i = 0; i < barrels.size; i++ )
		barrels[ i ] thread barrel_think();
	codolPrintLn( "CoDOL Outpost barrels: initialized " + barrels.size + " authored barrels" );
}

barrel_think()
{
	if ( self.classname != "script_model" || isDefined( self.zm_barrel_initialized ) )
		return;
	self.zm_barrel_initialized = true;
	self.zm_barrel_exploded = false;
	self endon( "exploding" );
	if ( isDefined( self.target ) )
	{
		clip = getEnt( self.target, "targetname" );
		if ( isDefined( clip ) && clip.classname == "script_brushmodel" )
			self.zm_barrel_clip = clip;
	}
	self.damageTaken = 0;
	self setCanDamage( true );
	for ( ;; )
	{
		self waittill( "damage", amount, attacker, direction, point, mod );
		if ( amount <= 0 || mod == "MOD_MELEE" || mod == "MOD_IMPACT" )
			continue;
		self.damagetype = mod;
		self.damageOwner = attacker;
		if ( level.zm_barrel_exploding_this_frame )
			wait( randomFloat( 1 ) );
		self.damageTaken += amount;
		if ( self.damageTaken == amount )
			self thread barrel_burn();
	}
}

barrel_burn()
{
	self endon( "exploding" );
	count = 0;
	started = false;
	up = anglesToUp( self.angles );
	dot = vectorDot( up, anglesToUp( ( 0, 90, 0 ) ) );
	ignite = ( 0, 0, 0 );
	burn = up * 44;
	if ( dot < 0.5 )
	{
		ignite = up * 22 - ( 0, 0, 30 );
		burn = up * 22 + ( 0, 0, 14 );
	}
	if ( self.damagetype != "MOD_GRENADE_SPLASH" && self.damagetype != "MOD_GRENADE" )
	{
		while ( self.damageTaken < 150 )
		{
			if ( !started )
			{
				playFx( level.zm_barrel_fx[ "ignite" ], self.origin + ignite );
				started = true;
			}
			if ( count > 20 )
				count = 0;
			playFx( level.zm_barrel_fx[ "burn" ], self.origin + burn );
			if ( count == 0 )
				self.damageTaken += 10 + randomFloat( 10 );
			count++;
			wait( 0.05 );
		}
	}
	self thread barrel_explode();
}

barrel_explode()
{
	// Consume the barrel before blast damage/notifies can re-enter its path.
	if ( self.zm_barrel_exploded )
		return;
	self.zm_barrel_exploded = true;
	self setCanDamage( false );
	self notify( "exploding" );
	self notify( "death" );
	up = anglesToUp( self.angles );
	dot = vectorDot( up, anglesToUp( ( 0, 90, 0 ) ) );
	offset = ( 0, 0, 0 );
	if ( dot < 0.5 )
	{
		start = self.origin + up * 22;
		offset = physicsTrace( start, start + ( 0, 0, -64 ) ) - self.origin;
	}
	offset += ( 0, 0, 4 );
	self playSound( "barrel_mtl_explode" );
	playFx( level.zm_barrel_fx[ "explode" ], self.origin + offset );
	level.zm_barrel_exploding_this_frame = true;
	if ( isDefined( self.zm_barrel_clip ) )
		self.zm_barrel_clip delete();
	radius = 250;
	if ( isDefined( self.radius ) )
		radius = self.radius;
	// Retained GScr_EntityRadiusDamage accepts an omitted weapon (index -1).
	// Its inflictor remains this barrel; the owner is the last real attacker.
	self radiusDamage( self.origin + ( 0, 0, 30 ), radius, 250, 1, self.damageOwner, "MOD_EXPLOSIVE" );
	physicsExplosionSphere( self.origin + ( 0, 0, 30 ), radius, radius / 2, 2 );
	earthquake( 0.4, 0.5, self.origin, 512 );
	if ( randomInt( 2 ) == 0 )
		self setModel( "com_barrel_piece" );
	else
		self setModel( "com_barrel_piece2" );
	if ( dot < 0.5 )
	{
		start = self.origin + up * 22;
		self.origin = physicsTrace( start, start + ( 0, 0, -64 ) );
		self.angles += ( 0, 0, 90 );
	}
	wait( 0.05 );
	level.zm_barrel_exploding_this_frame = false;
}
