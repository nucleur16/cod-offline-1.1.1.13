// money_ball_zm -- the "money grenade", lethal of zm2 (Danby) and zm4 (lg).
//
// C1 IS THE SPECIFICATION.  SF_UI_MONEY_KNIFE_ZM_DESC (localized_code_post_gfx_mp,
// GBK): "Upgraded sticky grenade: sticks to objects, and every robot zombie
// it kills earns an extra 100 points."  (docs/CODOL_ZM_EQUIPMENT_EVIDENCE_20260924.md
// section 2.)  T5 has no such item; there is no T5/IW4 script to port.
//
// WHAT THE ENGINE ALREADY DOES.  money_ball_zm's weapon data is identical to
// semtex_grenade_zm (codol_checkweapon: type/inventory/class 1/1/5, offhand
// class 1, world model weapon_semtex_grenade, ammo 'semtex_grenade'), so the
// throw, the stick and the explosion are the engine's own.  Nothing here
// simulates any of that.
//
// WHAT THIS MODULE ADDS.
//   1. The kill bonus.  An explosion kill reaches zombie_death_event in
//      _zombiemode_spawner.gsc; the killing weapon is the 10th argument of the
//      actor "damage" notify (G_Damage pushes the weapon's internal name,
//      src/codol/script/codol_equipment_lifecycle.c CODOL_AddDamageWeaponArgument),
//      which zombie_damage_watch records as zombie.damageweapon.  The spawner
//      then calls the per-weapon kill callback registered below, on the
//      attacker, i.e. the thrower (the missile's parent is the G_Damage
//      attacker).  The bonus is paid through add_to_player_score, the same
//      purse every other award uses; it is scaled by the double-points
//      multiplier like every other award (get_points_multiplier) -- that
//      scaling is CoDOL interpretation, C1's text only says "100".
//   2. Presentation from C1's own zm zone (common_gametype_zm): the
//      misc/light_moneyball_geotrail trail while it flies,
//      misc/light_moneyball_blink while it sits stuck, and zmb_money_ball_coin
//      where a bonus is paid.

#include maps\_zombiemode_utility;

MONEY_BALL_WEAPON = "money_ball_zm";
MONEY_BALL_BONUS = 100;

init()
{
	if ( getWeaponIndexFromName( MONEY_BALL_WEAPON ) <= 0 )
	{
		{}
		return;
	}

	level._effect[ "money_ball_geotrail" ] = loadfx( "misc/light_moneyball_geotrail" );
	level._effect[ "money_ball_blink" ] = loadfx( "misc/light_moneyball_blink" );
	precacheModel( "tag_origin" );

	maps\_zombiemode_equipment::register_equipment( MONEY_BALL_WEAPON, ::money_ball_fired );

	if ( !isDefined( level.zombie_kill_weapon_callbacks ) )
		level.zombie_kill_weapon_callbacks = [];
	level.zombie_kill_weapon_callbacks[ MONEY_BALL_WEAPON ] = ::money_ball_kill;

	{}
}

// self = the thrown grenade, from _zombiemode_equipment::watch_equipment_fire.
// Presentation only; the engine owns flight, stick and explosion.
money_ball_fired( player, weapon )
{
	grenade = self;

	// The trail rides a tag_origin model linked to the missile rather than a
	// tag of weapon_semtex_grenade: playFxOnTag with a tag the model lacks is
	// a script error, and a script error stops the server.
	trail = spawn( "script_model", grenade.origin );
	trail setModel( "tag_origin" );
	trail linkTo( grenade );
	playFxOnTag( level._effect[ "money_ball_geotrail" ], trail, "tag_origin" );

	last = grenade.origin;
	still = 0;
	blinking = false;
	for ( tick = 0; tick < 400; tick++ )
	{
		wait( 0.05 );
		if ( !isDefined( grenade ) )
			break;

		pos = grenade.origin;
		if ( distanceSquared( pos, last ) < 1 )
			still++;
		else
			still = 0;
		last = pos;

		// Stuck (or resting): C1's light_moneyball_blink.  It loops forever
		// (MEASURED, FxEffectDef msecLoopingLife 0x7fffffff), so it is
		// started once on the trail carrier and ends when that is deleted;
		// a playFx at the resting point would never be removed.
		if ( still >= 2 && !blinking && isDefined( trail ) )
		{
			playFxOnTag( level._effect[ "money_ball_blink" ], trail, "tag_origin" );
			blinking = true;
		}
	}

	if ( isDefined( trail ) )
		trail delete();
}

// self = the attacking player, from _zombiemode_spawner::zombie_death_event
// when the killing hit's weapon is money_ball_zm.
money_ball_kill( zombie, weapon )
{
	if ( !isDefined( self ) || !isPlayer( self ) )
		return;

	origin = undefined;
	if ( isDefined( zombie ) )
		origin = zombie.origin;

	bonus = MONEY_BALL_BONUS * maps\_zombiemode_score::get_points_multiplier();
	before = self.score;
	self maps\_zombiemode_score::add_to_player_score( bonus );

	if ( isDefined( origin ) )
		playSoundAtPosition( "zmb_money_ball_coin", origin );

	if ( !isDefined( self.money_ball_kills ) )
		self.money_ball_kills = 0;
	self.money_ball_kills = self.money_ball_kills + 1;

	{}
}
