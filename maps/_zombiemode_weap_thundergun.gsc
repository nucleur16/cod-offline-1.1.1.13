// Thundergun's authoritative shot damage. The C1 weapon is a scripted
// special weapon; listing/precache alone does not produce its blast.
//
// Behavioral donor: shipped T5 maps/_zombiemode_weap_thundergun.gsc,
// thundergun_fired / thundergun_get_enemies_in_range / thundergun_fling /
// thundergun_knockdown (Black Ops raw). T5 selects a forward cylinder with
// radius180, max distance1200, lethal inner distance480. Its ordinary outer
// path calls DoDamage(15) twice, before/after the gib/pain setup; the declared
// gib_damage75 is not consumed by that donor. Keep those actual damage calls.
//
// C1 adaptation: real weapon_fired(name), produced by CODOL_NotifyWeaponFired,
// is observed once per player across respawns. getEye is C1's native shot
// source (G_GetPlayerViewOrigin); getCentroid reads live actor collision
// bounds. level.zombies is the actual live registry, copied per shot so
// synchronous damage/death callbacks cannot skip a target or mix shooters.
// damageConeTrace is retained CanDamage(mask0x802011), so walls occlude.
// Full doDamage arguments preserve shooter, inflictor, MOD_PROJECTILE and
// weapon for normal zombie damage/death/drop/round accounting. That existing
// death consumer pays points; this module never pays an extra duplicate kill.
// T5 ragdoll launch, knockdown animation and power-cell FX are not ported here.

#include maps\_zombiemode_utility;

init()
{
	// Observe real shots on every map. The box table contains only base
	// weapons, so membership is not an availability test for an upgraded or
	// separately granted weapon. This listener precaches/registers no asset;
	// only an actual shot carrying either exact weapon name produces damage.
	level thread watch_connections();
	{}
}

watch_connections()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
		players[ i ] thread watch_shots();
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread watch_shots();
	}
}

watch_shots()
{
	self endon( "disconnect" );
	if ( isDefined( self.zm_thundergun_watcher ) && self.zm_thundergun_watcher )
		return;
	self.zm_thundergun_watcher = true;
	for ( ;; )
	{
		self waittill( "weapon_fired", weapon );
		// Use the event's exact weapon, not GetCurrentWeapon after switching.
		if ( is_thundergun( weapon ) )
			self fire( weapon );
	}
}

is_thundergun( weapon )
{
	return isDefined( weapon ) &&
		( weapon == "thundergun_zm" || weapon == "thundergun_upgraded_zm" );
}

// Squared Euclidean distance or -1 when outside the stock T5 cylinder.
// With distance<=range and dot>=0 the projection lies on the finite segment,
// so distSquared-dotSquared is PointOnSegmentNearestToPoint's lateral test.
cylinder_distance_squared( origin, forward, center )
{
	offset = center - origin;
	d = vectorDot( offset, offset );
	if ( d > 1200 * 1200 )
		return -1;
	axial = vectorDot( offset, forward );
	if ( axial < 0 || d - axial * axial > 180 * 180 )
		return -1;
	return d;
}

fire( weapon )
{
	if ( !is_thundergun( weapon ) || !isDefined( level.zombies ) )
		return;
	origin = self getEye();
	forward = anglesToForward( self getPlayerAngles() );
	targets = [];
	for ( i = 0; i < level.zombies.size; i++ )
		targets[ targets.size ] = level.zombies[ i ];
	for ( i = 0; i < targets.size; i++ )
	{
		zombie = targets[ i ];
		if ( !isDefined( zombie ) || !isAlive( zombie ) )
			continue;
		d = cylinder_distance_squared( origin, forward, zombie getCentroid() );
		if ( d < 0 || zombie damageConeTrace( origin, self ) == 0 )
			continue;
		self damage_target( zombie, d, origin, weapon );
	}
}

damage_target( zombie, distance_squared, origin, weapon )
{
	if ( distance_squared < 480 * 480 )
	{
		zombie doDamage( zombie.health + 666, origin, self, self, 0, "MOD_PROJECTILE", 0, weapon );
		return;
	}
	zombie doDamage( 15, origin, self, self, 0, "MOD_PROJECTILE", 0, weapon );
	if ( isDefined( zombie ) && isAlive( zombie ) )
		zombie doDamage( 15, origin, self, self, 0, "MOD_PROJECTILE", 0, weapon );
}
