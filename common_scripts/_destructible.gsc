// Native overlay of alpha0022/gsc_overlay/common_scripts/_destructible.gsc.
// Fix: common_mp sound asset11/3475 is car_explode (three exp_car_close waves).
// vehicle_car_exp is absent and aborted the map when a Semtex damaged a car.
// Retained Python-era whole-entity behavior below is not a complete native
// destructible-part implementation; this change repairs the verified alias.
// PROVENANCE (2026-09-02): ADAPTED from vendor/iw4/common_scripts/_destructible.gsc
// for DesignDoc MP #9 ("models placed by GSC are broken -- cars that can
// explode").  This overlay WINS over vendor (gsc/loader.py script_roots()).
//
// WHY ADAPTED, NOT VERBATIM: the vendor file drives destruction through a
// per-part / per-tag animation state machine (destructible_think ->
// destructible_update_part -> xanim part states) built from the huge
// _destructible_types data tables.  That machine needs xmodel tags, an anim
// tree, and physics the Python VM does not model, so on this build it ran but
// set no health and produced no explosion (measured: cars spawned with
// SetCanDamage(true) but self.health == undefined, no fx, no radius damage).
//
// This adaptation keeps the SAME public entry points the rest of the tree
// calls -- init(), find_destructibles(), setup_destructibles(), getDamageType()
// (verbatim, _damage.gsc depends on it), plus force_explosion()/
// disable_explosion()/precache_destructibles()/add_destructible_fx()/
// cleanupVars() so no caller halts -- but replaces the part machine with a
// faithful WHOLE-ENTITY model: each destructible gets a health pool, takes
// damage through the engine "damage" notify, and on death explodes = a car
// explosion fx + RadiusDamage (the same damage path the killstreak ordnance
// uses) + it hides and sets self.exploded, which gsc_props.destroyed_origins()
// reads to drop the wreck from the wire.
//
// Publication of the models themselves (so they APPEAR at all) is gsc_props.py
// + game-server.py; see docs/gsc_props_fix.md.

#include common_scripts\utility;

init()
{
	level.destructible_carfx = LoadFX( "explosions/small_vehicle_explosion" );
	level.destructible_toyfx = LoadFX( "explosions/small_vehicle_explosion" );
	level.destructibleSpawnedEnts = [];

	thread find_destructibles();
}

find_destructibles()
{
	// getEntArray() spawns the MapEnts script_model rows as live entities
	// (gscbuiltins._map_ents), so this is what makes the cars exist AND wires
	// each one for damage.
	vehicles = GetEntArray( "destructible_vehicle", "targetname" );
	foreach ( vehicle in vehicles )
		vehicle setup_destructibles();

	toys = GetEntArray( "destructible_toy", "targetname" );
	foreach ( toy in toys )
		toy setup_destructibles();
}

setup_destructibles( cached )
{
	if ( isDefined( self.destructible_setup_done ) )
		return;
	self.destructible_setup_done = true;

	self.destructible_is_vehicle = ( self.targetname == "destructible_vehicle" );
	self.maxhealth = health_for( self.destructible_type, self.destructible_is_vehicle );
	self.health = self.maxhealth;
	self.exploded = false;

	self SetCanDamage( true );
	self thread destructible_think();
}

health_for( type, isVehicle )
{
	// Vendor bias: MP_EXPLOSIVE_DAMAGE_BIAS 13.0 (_destructible.gsc:15), so a
	// single frag (~200 splash) should NOT one-shot a car but a rocket or a
	// close cluster should.  Toys (electric boxes, transformers, TVs) are far
	// weaker.  Values chosen so one rocket (radius 200-256, inner up to 200)
	// or two frags cook a car; one frag pops a toy.
	if ( isDefined( isVehicle ) && isVehicle )
		return 350;
	return 120;
}

destructible_think()
{
	self endon( "death" );

	while ( 1 )
	{
		self waittill( "damage", amount, attacker );

		if ( !isDefined( amount ) )
			amount = 100;
		if ( self.exploded )
			continue;

		self.health = self.health - amount;
		if ( self.health <= 0 )
		{
			self destructible_explode( attacker );
			return;
		}
	}
}

destructible_explode( attacker )
{
	if ( self.exploded )
		return;
	self.exploded = true;

	fxOrigin = self.origin;

	if ( self.destructible_is_vehicle )
		fx = level.destructible_carfx;
	else
		fx = level.destructible_toyfx;
	PlayFX( fx, fxOrigin );

	sndAlias = "car_explode";
	if ( isDefined( self.sound_csv_include ) )
		sndAlias = self.sound_csv_include;
	self PlaySound( sndAlias );

	// RadiusDamage( origin, radius, maxdamage, mindamage, attacker ) -- the
	// engine's linear-falloff radius damage.  Same damage_queue the killstreak
	// ordnance rides, applied to players by the damage owner.  A car blast is
	// lethal at the centre and hurts out to ~300u.
	if ( self.destructible_is_vehicle )
		RadiusDamage( fxOrigin, 300, 200, 75, attacker );
	else
		RadiusDamage( fxOrigin, 128, 90, 25, attacker );

	// death drops it from the wire (gsc_props reads self.exploded) and hides
	// the intact model.
	self notify( "death" );
	self Hide();
}

// force_explosion() -- external callers (map scripts, _dynamic_world) blow a
// destructible on cue.  Route it through the same path.
force_explosion()
{
	if ( !isDefined( self.destructible_is_vehicle ) )
		self.destructible_is_vehicle = ( self.targetname == "destructible_vehicle" );
	if ( !isDefined( self.exploded ) )
		self.exploded = false;
	self destructible_explode( undefined );
}

disable_explosion()
{
	self.dontAllowExplode = true;
}

// -- API stubs kept so external callers never halt a thread ---------------
precache_destructibles()
{
}

add_destructible_fx()
{
}

cleanupVars()
{
}

// getDamageType() -- VERBATIM from vendor/iw4/common_scripts/_destructible.gsc.
// _damage.gsc and _gameobjects.gsc call it every damage event, so it must
// answer exactly as the engine build's own copy did.
getDamageType( type )
{
	if ( !isdefined( type ) )
		return "unknown";

	type = ToLower( type );
	switch( type )
	{
		case "mod_melee":
		case "mod_crush":
		case "melee":
			return "melee";
		case "mod_pistol_bullet":
		case "mod_rifle_bullet":
		case "bullet":
			return "bullet";
		case "mod_grenade":
		case "mod_grenade_splash":
		case "mod_projectile":
		case "mod_projectile_splash":
		case "mod_explosive":
		case "splash":
			return "splash";
		case "mod_impact":
			return "impact";
		case "unknown":
			return "unknown";
		default:
			return "unknown";
	}
}
