// revive_knife_zm -- the "rescue throwing knife", special of zm4 (lg).
//
// C1 IS THE SPECIFICATION.  SF_UI_REVIVE_KNIFE_ZM_DESC (GBK): "Thrown weapon:
// revives a downed teammate at range, and does small-area damage."
// (docs/CODOL_ZM_EQUIPMENT_EVIDENCE_20260924.md section 2.)
//
// WEAPON DATA (codol_checkweapon, same doc section 3): type/inventory/class
// 1/1/5, offhand class 4 (throwing knife), world model weapon_parabolic_knife,
// base damage 0.  The engine throws it and raises grenade_fire; the knife
// itself damages nothing, so both halves of C1's text are this module's.
//
// BEHAVIOUR REFERENCE.  T5 has the same idea in the upgraded ballistic knife:
// a hit on a downed player raises "remote_revive" (T5 _zombiemode.gsc:2087
// remote_revive_watch -> maps\_laststand.gsc:818 remote_revive -> :768
// auto_revive, which revives with no hold and credits the reviver).  In T5
// the "remote_revive" notify is raised by the SP engine, not script, and no
// MP engine (IW3, IW4, OpenBLOPS MP) has it.  Here the knife is followed from
// grenade_fire instead:
//   * in flight, a downed teammate within KNIFE_HIT_RADIUS of the knife is a
//     hit (the throw reached them);
//   * where the knife comes to rest -- stuck, dropped, or bounced off the
//     downed player's own body, which is what a missile does to a player it
//     strikes on this engine -- a downed teammate within the level's revive
//     trigger radius (zombie_revive_trigger_radius, T5 :431) is revived;
//   * at rest the knife does its small-area damage to zombies.
// The revive is _zombiemode_laststand::revive_success( thrower ): the same
// teardown the hold-to-revive path uses (laststandrevive(), weapons back,
// the "reviver" award, "player_revived").
//
// CoDOL INTERPRETATION (C1 gives no numbers): KNIFE_HIT_RADIUS 40,
// KNIFE_DAMAGE_RADIUS 96 and KNIFE_DAMAGE 200 -> 50 linear falloff; that is
// enough to kill a round-1 robot zombie (150 health) at the centre.  Damage is
// applied per zombie with doDamage rather than radiusDamage, because
// radiusDamage would also hit the thrower and teammates through the zm
// Callback_PlayerDamage.
//
// Presentation, C1 assets: maps/zombie/fx_reviveknife and zmb_revive_knife
// (both patch_common_gametype_zm) on the revived player.

#include maps\_zombiemode_utility;

REVIVE_KNIFE_WEAPON = "revive_knife_zm";
KNIFE_HIT_RADIUS = 40;
KNIFE_DAMAGE_RADIUS = 96;
KNIFE_DAMAGE_MAX = 200;
KNIFE_DAMAGE_MIN = 50;

init()
{
	// Test hook for _codol_equiptest's "down <client>" verb: the shared test
	// file cannot name a zombies-only function without dragging the zombies
	// chain into multiplayer, so it calls through this pointer.
	level.codolEquipTestDown = maps\_zombiemode_laststand::player_laststand;

	if ( getWeaponIndexFromName( REVIVE_KNIFE_WEAPON ) <= 0 )
	{
		{}
		return;
	}

	level._effect[ "revive_knife" ] = loadfx( "maps/zombie/fx_reviveknife" );

	maps\_zombiemode_equipment::register_equipment( REVIVE_KNIFE_WEAPON, ::revive_knife_fired );
	{}
}

// self = the thrown knife.
revive_knife_fired( player, weapon )
{
	knife = self;
	thrower = player;
	start = knife.origin;
	last = start;
	still = 0;
	revived = 0;

	for ( tick = 0; tick < 200; tick++ )
	{
		wait( 0.05 );
		if ( !isDefined( knife ) )
			break;

		pos = knife.origin;

		target = downed_teammate_near( thrower, pos, KNIFE_HIT_RADIUS );
		if ( isDefined( target ) )
		{
			if ( knife_revive( target, thrower, pos, "hit" ) )
				revived++;
		}

		if ( distanceSquared( pos, last ) < 1 )
			still++;
		else
			still = 0;
		last = pos;

		if ( still >= 2 )
			break;
	}

	// last is where the knife came to rest (or was when the engine removed it).
	radius = KNIFE_HIT_RADIUS;
	if ( isDefined( level.zombie_vars ) && isDefined( level.zombie_vars[ "zombie_revive_trigger_radius" ] ) )
		radius = level.zombie_vars[ "zombie_revive_trigger_radius" ];

	target = downed_teammate_near( thrower, last, radius );
	if ( isDefined( target ) )
	{
		if ( knife_revive( target, thrower, last, "landed" ) )
			revived++;
	}

	kills = knife_area_damage( thrower, last, weapon );

	who = "(gone)";
	if ( isDefined( thrower ) )
		who = thrower.name;
	{}
}

// The closest downed teammate whose body is within radius of pos, or
// undefined.  The body is an upright cylinder (horizontal radius, and from
// 16 below the feet to 72 above them): a knife that strikes a player stops
// against the player's bounding box, not at the feet.
downed_teammate_near( thrower, pos, radius )
{
	best = undefined;
	best_dsq = radius * radius;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( !isDefined( p ) || !isAlive( p ) )
			continue;
		if ( isDefined( thrower ) && p == thrower )
			continue;
		if ( !p maps\_zombiemode_laststand::player_is_in_laststand() )
			continue;

		delta = pos - p.origin;
		if ( delta[ 2 ] < -16 || delta[ 2 ] > 72 )
			continue;
		dsq = delta[ 0 ] * delta[ 0 ] + delta[ 1 ] * delta[ 1 ];
		if ( dsq <= best_dsq )
		{
			best = p;
			best_dsq = dsq;
		}
	}
	return best;
}

knife_revive( target, thrower, pos, how )
{
	if ( !isDefined( target ) || !target maps\_zombiemode_laststand::player_is_in_laststand() )
		return false;

	reviver = undefined;
	reviver_name = "(disconnected)";
	if ( isDefined( thrower ) && isPlayer( thrower ) )
	{
		reviver = thrower;
		reviver_name = thrower.name;
	}

	{}

	playFx( level._effect[ "revive_knife" ], target.origin + ( 0, 0, 12 ) );
	playSoundAtPosition( "zmb_revive_knife", target.origin );

	// T5 auto_revive waits out a revive already in progress (:772); the
	// hold loop is cancelled by the "player_revived" notify revive_success
	// raises, so revive immediately.
	target maps\_zombiemode_laststand::revive_success( reviver );
	return true;
}

// Small-area damage at the knife's rest point.  Returns how many zombies it hit.
knife_area_damage( thrower, pos, weapon )
{
	if ( !isDefined( thrower ) || !isPlayer( thrower ) || !isDefined( level.zombies ) )
		return 0;

	// Work from a copy: a kill below runs zombie_death_event, which rebuilds
	// level.zombies.
	targets = [];
	for ( i = 0; i < level.zombies.size; i++ )
		targets[ targets.size ] = level.zombies[ i ];

	hit = 0;
	for ( i = 0; i < targets.size; i++ )
	{
		z = targets[ i ];
		if ( !isDefined( z ) || !isAlive( z ) )
			continue;
		if ( !isDefined( thrower ) )
			break;

		d = distance( pos, z.origin + ( 0, 0, 30 ) );
		if ( d > KNIFE_DAMAGE_RADIUS )
			continue;

		amount = int( KNIFE_DAMAGE_MAX - ( KNIFE_DAMAGE_MAX - KNIFE_DAMAGE_MIN ) * ( d / KNIFE_DAMAGE_RADIUS ) );
		z doDamage( amount, pos, thrower, thrower, 0, "MOD_EXPLOSIVE", 0, weapon );
		hit++;
	}
	return hit;
}
