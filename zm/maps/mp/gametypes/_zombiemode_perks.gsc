// zm perk machines and Pack-a-Punch.
//
// Port of T5's maps\_zombiemode_perks.gsc (2,118 lines).
//
// TAKEN, by T5 function:
//   init                          :5     -- the zombie_vending /
//                                           zombie_vending_upgrade sweeps and
//                                           the zombie_perk_cost tuning.
//   vending_trigger_think         :1083  -- THE MACHINE: per-perk cost, the
//                                           power gate (level waittill
//                                           "<perk>_power_on"), the hint
//                                           string, and the buy loop with its
//                                           already-have / too-poor / four-perk
//                                           refusals.
//   perk_give_bottle_begin        :1811  -- the bottle weapon per perk.
//   perk_give_bottle_end          :1882
//   give_perk                     :1456  -- SetPerk, the juggernog health
//                                           bump, the solo quick-revive life.
//   perk_think                    :1566  -- perks are lost on down/death.
//   vending_weapon_upgrade        :393   -- Pack-a-Punch: pay, take the gun,
//                                           hand back its _upgraded variant.
//   vending_weapon_upgrade_cost   :519
//   wait_for_player_to_take       :538
//   wait_for_timeout              :595
//   turn_*_on                     :709-:1005 -- collapsed into the single
//                                           power notify fan-out that
//                                           _zombiemode_power.gsc sends.
//
// NOT TAKEN, and why:
//   third_person_weapon_upgrade (:283) and do_knuckle_crack /
//   upgrade_knuckle_crack_begin / _end (:611,:624,:663) -- the Pack-a-Punch
//   presentation is a scripted animation on a rig of "packa_rollers" plus a
//   viewmodel knuckle-crack; both are animscripted() and neither builtin
//   exists here.  The machine still takes the gun, waits packapunch_timeout,
//   and hands back the upgrade.
//   perk_hud_create / _destroy / _flash / perk_flash_audio (:1632-:1809) --
//   they SetShader the perk icons; none of those shaders is precached under a
//   zm zone and precaching a shader that is in no mounted zone is not
//   something worth risking at map load for an icon.  Perk ownership is
//   readable from hasPerk() and from the machine's own hint.
//   check_player_has_perk (:1520) -- SetInvisibleToPlayer, not registered.
//   revive_solo_fx / electric_perks_dialog / place_additionalprimaryweapon_
//   machine / give_random_perk / lose_random_perk / quantum_bomb_* -- separate
//   systems or FX only.
//
// FOUR BUILTINS T5 USES HERE THAT THIS SERVER DOES NOT REGISTER (MEASURED
// 2026-09-20): AllowLean, AllowMelee, AllowCrouch, AllowProne.  T5 clamps the
// player's movement while they drink.  AllowAds and AllowSprint ARE
// registered and are used; the other four clamps are simply absent, so a
// player can still crouch and melee mid-drink.
//
// ONE MORE: SetMaxHealth is not registered either.  Juggernog therefore writes
// self.maxhealth and self.health directly, which is how the zm spawn path
// already sets a player's health (_zombiemode.gsc spawnPlayer).

#include maps\mp\gametypes\_zombiemode_utility;

init()
{
	set_zombie_var( "zombie_perk_cost", 2000 );
	set_zombie_var( "zombie_perk_juggernaut_health", 160 );

	if ( !isDefined( level.packapunch_timeout ) )
		level.packapunch_timeout = 15;

	add_sound( "perk_deny", "zmb_perks_deny" );
	add_sound( "bottle_dispense", "zmb_perks_bottle_dispense" );

	vending_triggers = getEntArray( "zombie_vending", "targetname" );
	for ( i = 0; i < vending_triggers.size; i++ )
		vending_triggers[ i ] thread vending_trigger_think();

	upgrade_triggers = getEntArray( "zombie_vending_upgrade", "targetname" );
	for ( i = 0; i < upgrade_triggers.size; i++ )
		upgrade_triggers[ i ] thread vending_weapon_upgrade();

	println( "CoDOL zm: " + vending_triggers.size + " perk machines, " +
		upgrade_triggers.size + " pack-a-punch\n" );
}

// T5 :1083 cost ladder.
perk_cost( perk, solo )
{
	switch( perk )
	{
	case "specialty_armorvest":
		return 2500;

	case "specialty_quickrevive":
		if ( solo )
			return 500;
		return 1500;

	case "specialty_fastreload":
		return 3000;

	case "specialty_rof":
		return 2000;

	case "specialty_longersprint":
		return 2000;

	case "specialty_flakjacket":
		return 2000;

	case "specialty_deadshot":
		return 1000;

	case "specialty_additionalprimaryweapon":
		return 4000;
	}

	return level.zombie_vars[ "zombie_perk_cost" ];
}

perk_name( perk )
{
	switch( perk )
	{
	case "specialty_armorvest":
		return "Juggernog";
	case "specialty_quickrevive":
		return "Quick Revive";
	case "specialty_fastreload":
		return "Speed Cola";
	case "specialty_rof":
		return "Double Tap";
	case "specialty_longersprint":
		return "Stamin-Up";
	case "specialty_flakjacket":
		return "PhD Flopper";
	case "specialty_deadshot":
		return "Deadshot Daiquiri";
	case "specialty_additionalprimaryweapon":
		return "Mule Kick";
	}
	return perk;
}

// T5 :1811.
perk_bottle( perk )
{
	switch( perk )
	{
	case "specialty_armorvest":
		return "zombie_perk_bottle_jugg";
	case "specialty_quickrevive":
		return "zombie_perk_bottle_revive";
	case "specialty_fastreload":
		return "zombie_perk_bottle_sleight";
	case "specialty_rof":
		return "zombie_perk_bottle_doubletap";
	case "specialty_longersprint":
		return "zombie_perk_bottle_marathon";
	case "specialty_flakjacket":
		return "zombie_perk_bottle_nuke";
	case "specialty_deadshot":
		return "zombie_perk_bottle_deadshot";
	case "specialty_additionalprimaryweapon":
		return "zombie_perk_bottle_additionalprimaryweapon";
	}
	return undefined;
}

// T5 :1083.  self is a zombie_vending trigger; script_noteworthy is the perk.
vending_trigger_think()
{
	perk = self.script_noteworthy;

	if ( !isDefined( perk ) )
	{
		println( "CoDOL zm: a zombie_vending trigger has no script_noteworthy; ignored\n" );
		return;
	}

	solo = false;
	players = getPlayers();
	if ( players.size == 1 )
		solo = true;

	cost = perk_cost( perk, solo );
	self.cost = cost;

	self setCursorHint( "HINT_NOICON" );
	self useTriggerRequireLookAt();
	self setHintString( "Requires Power" );

	// T5 gates every machine but solo quick-revive behind the power switch.
	if ( !( solo && perk == "specialty_quickrevive" ) )
		level waittill( perk + "_power_on" );

	self setHintString( "Hold ^3[{+activate}]^7 to Buy " + perk_name( perk ) + " [Cost: " + cost + "]" );

	bottle = perk_bottle( perk );
	if ( isDefined( bottle ) )
		precacheItem( bottle );

	for ( ;; )
	{
		self waittill( "trigger", player );

		if ( !isDefined( player ) || !isPlayer( player ) )
			continue;

		if ( !is_player_valid( player ) )
			continue;

		if ( !player useButtonPressed() )
			continue;

		if ( player in_revive_trigger() )
			continue;

		if ( is_true( player.is_drinking ) )
		{
			wait( 0.1 );
			continue;
		}

		if ( player hasPerk( perk ) )
		{
			self play_sound_on_ent( "perk_deny" );
			wait( 0.5 );
			continue;
		}

		if ( player.score < cost )
		{
			self play_sound_on_ent( "perk_deny" );
			wait( 0.5 );
			continue;
		}

		if ( isDefined( player.num_perks ) && player.num_perks >= 4 )
		{
			self play_sound_on_ent( "perk_deny" );
			wait( 0.5 );
			continue;
		}

		self play_sound_on_ent( "bottle_dispense" );
		player maps\mp\gametypes\_zombiemode_score::minus_to_player_score( cost );

		gun = player perk_give_bottle_begin( perk );

		// T5 waits for the drink animation to finish via
		// waittill_any( "fake_death", "death", "player_downed",
		// "weapon_change_complete" ).  waittill_any is not a builtin here and
		// there is no weapon_change_complete notify without the animation
		// layer, so the drink is timed: 2.5s is T5's bottle length.
		player thread perk_drink_timer();
		player waittill_either( "player_downed", "perk_drink_done" );

		player perk_give_bottle_end( gun );

		if ( !is_player_valid( player ) )
			continue;

		player give_perk( perk );
	}
}

perk_drink_timer()
{
	self endon( "disconnect" );
	self endon( "death" );
	wait( 2.5 );
	self notify( "perk_drink_done" );
}

// T5 :1811, minus the four unregistered movement clamps (see the header).
perk_give_bottle_begin( perk )
{
	self.is_drinking = true;

	self allowAds( false );
	self allowSprint( false );

	wait( 0.05 );

	if ( self getStance() == "prone" )
		self setStance( "crouch" );

	gun = self getCurrentWeapon();

	bottle = perk_bottle( perk );
	if ( isDefined( bottle ) && getWeaponIndexFromName( bottle ) > 0 )
	{
		self giveWeapon( bottle );
		self switchToWeapon( bottle );
	}

	return gun;
}

// T5 :1882.
perk_give_bottle_end( gun )
{
	self allowAds( true );
	self allowSprint( true );

	bottles = [];
	bottles[ 0 ] = "zombie_perk_bottle_jugg";
	bottles[ 1 ] = "zombie_perk_bottle_revive";
	bottles[ 2 ] = "zombie_perk_bottle_sleight";
	bottles[ 3 ] = "zombie_perk_bottle_doubletap";
	bottles[ 4 ] = "zombie_perk_bottle_marathon";
	bottles[ 5 ] = "zombie_perk_bottle_nuke";
	bottles[ 6 ] = "zombie_perk_bottle_deadshot";
	bottles[ 7 ] = "zombie_perk_bottle_additionalprimaryweapon";

	for ( i = 0; i < bottles.size; i++ )
	{
		if ( self hasWeapon( bottles[ i ] ) )
			self takeWeapon( bottles[ i ] );
	}

	if ( isDefined( gun ) && gun != "none" && self hasWeapon( gun ) )
		self switchToWeapon( gun );

	self.is_drinking = false;
}

// T5 :1456.
give_perk( perk )
{
	self setPerk( perk );

	if ( !isDefined( self.num_perks ) )
		self.num_perks = 0;
	self.num_perks++;

	if ( !isDefined( self.zm_perks ) )
		self.zm_perks = [];
	self.zm_perks[ perk ] = true;

	if ( perk == "specialty_armorvest" )
	{
		// T5 SetMaxHealth( zombie_perk_juggernaut_health ); not registered
		// here, so the two fields the spawn path already writes are written.
		self.premaxhealth = self.maxhealth;
		self.maxhealth = level.zombie_vars[ "zombie_perk_juggernaut_health" ];
		self.health = self.maxhealth;
	}

	// T5: solo quick revive is an extra life rather than a revive.
	players = getPlayers();
	if ( players.size == 1 && perk == "specialty_quickrevive" )
	{
		if ( !isDefined( self.pers ) )
			self.pers = [];
		self.pers[ "lives" ] = 1;
	}

	self notify( "perk_bought", perk );
	self thread perk_think( perk );
}

// T5 :1566.  Perks are lost when the player goes down or dies.
perk_think( perk )
{
	self endon( "disconnect" );

	self waittill_either( "death", "player_downed" );

	self unsetPerk( perk );

	if ( isDefined( self.num_perks ) && self.num_perks > 0 )
		self.num_perks--;

	if ( isDefined( self.zm_perks ) )
		self.zm_perks[ perk ] = undefined;

	if ( perk == "specialty_armorvest" )
	{
		self.maxhealth = 100;
		if ( self.health > self.maxhealth )
			self.health = self.maxhealth;
	}

	self notify( "perk_lost", perk );
}

// ==========================================================================
// PACK-A-PUNCH -- T5 :393 vending_weapon_upgrade, :519 cost, :538 / :595 the
// take-or-time-out pair.
//
// The upgrade names are real content here: MEASURED from
// docs/zone_assets/ALL_weapon.txt, CoDOL's zm zones ship 40-odd
// "<base>_upgraded_zm" weapons alongside their bases, which is exactly the
// pairing maps\mp\gametypes\_zombiemode_weapons::add_zombie_weapon records.
// ==========================================================================

vending_weapon_upgrade_cost()
{
	return 5000;
}

vending_weapon_upgrade()
{
	flag_init( "pack_machine_in_use" );

	cost = vending_weapon_upgrade_cost();

	self setCursorHint( "HINT_NOICON" );
	self useTriggerRequireLookAt();
	self setHintString( "Requires Power" );

	level waittill( "Pack_A_Punch_on" );

	self setHintString( "Hold ^3[{+activate}]^7 to Pack-a-Punch [Cost: " + cost + "]" );

	for ( ;; )
	{
		self waittill( "trigger", player );

		if ( !isDefined( player ) || !isPlayer( player ) )
			continue;

		if ( !is_player_valid( player ) )
			continue;

		if ( !player useButtonPressed() )
			continue;

		if ( flag( "pack_machine_in_use" ) )
		{
			wait( 0.1 );
			continue;
		}

		current = player getCurrentWeapon();
		if ( !isDefined( current ) || current == "none" )
			continue;

		upgrade = maps\mp\gametypes\_zombiemode_weapons::get_upgrade_name( current );
		if ( !isDefined( upgrade ) )
		{
			// The held weapon has no upgraded variant in any mounted zone.
			// Refuse without charging -- T5's machine simply will not take a
			// weapon it cannot upgrade either.
			self play_sound_on_ent( "perk_deny" );
			wait( 0.5 );
			continue;
		}

		if ( player.score < cost )
		{
			self play_sound_on_ent( "perk_deny" );
			wait( 0.5 );
			continue;
		}

		flag_set( "pack_machine_in_use" );
		player maps\mp\gametypes\_zombiemode_score::minus_to_player_score( cost );

		// T5 takes the gun, plays the machine's animation, then offers the
		// upgraded gun back on a trigger for packapunch_timeout seconds.  The
		// animation is not ported (see the header); the take, the wait and
		// the hand-back are.
		player takeWeapon( current );
		self setHintString( "" );

		wait( level.packapunch_timeout );

		if ( is_player_valid( player ) )
		{
			player giveWeapon( upgrade );
			player giveMaxAmmo( upgrade );
			player switchToWeapon( upgrade );
			player play_sound_on_ent( "purchase" );
		}

		self setHintString( "Hold ^3[{+activate}]^7 to Pack-a-Punch [Cost: " + cost + "]" );
		flag_clear( "pack_machine_in_use" );
	}
}
