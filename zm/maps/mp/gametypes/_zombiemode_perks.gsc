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
//   2026-09-20: third_person_weapon_upgrade (:283) and do_knuckle_crack /
//   upgrade_knuckle_crack_begin / _end (:611,:624,:663) -- the Pack-a-Punch
//   presentation rig -- are now PORTED, and the reason they could be is that
//   neither actually needed a scripted animation.  Re-reading T5:
//     * third_person_weapon_upgrade is a script_model rig driven by setModel
//       / rotateto / moveto / delete.  Every one of those is registered.
//     * the knuckle crack is a WEAPON SWAP: T5 takes the held gun, gives
//       "zombie_knuckle_crack" and switches to it, and the animation is the
//       weapon's own viewmodel anim, played by the CLIENT.  MEASURED,
//       docs/zone_assets/ALL_weapon.txt: "zombie_knuckle_crack" ships in
//       common_gametype_zm, so the client has the viewmodel and its anims.
//   The two calls T5 makes that are genuinely unavailable are named at their
//   call sites below: useweaponhidetags (the world-gun tag cull) and the
//   AllowLean / AllowCrouch / AllowProne / AllowMelee quartet (of T5's six
//   movement locks only AllowAds and AllowSprint are registered here).
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

	// MEASURED, docs/zone_assets/ALL_fx.txt: CoDOL ships T5's own
	// Pack-a-Punch effect as "maps/zombie/fx_zombie_packapunch" in
	// common_gametype_zm.  third_person_weapon_upgrade guards the lookup.
	level._effect[ "packapunch_fx" ] = loadfx( "maps/zombie/fx_zombie_packapunch" );

	// CoDOL aliases, not T5 names.  MEASURED: the T5 names are absent from
	// docs/zone_assets/ALL_sound.txt; these ship in common_gametype_zm.
	add_sound( "perk_deny", "evt_perk_deny" );
	add_sound( "bottle_dispense", "evt_bottle_dispense" );

	vending_triggers = getEntArray( "zombie_vending", "targetname" );
	// The perks this MAP carries, for _zombiemode_powerups::free_perk_powerup:
	// T5 :2293 draws the free perk from the machines that exist, so a player
	// cannot be handed a perk the map has no machine for.
	level.zombie_perk_machines = [];
	for ( i = 0; i < vending_triggers.size; i++ )
	{
		if ( isDefined( vending_triggers[ i ].script_noteworthy ) )
			level.zombie_perk_machines[ level.zombie_perk_machines.size ] = vending_triggers[ i ].script_noteworthy;
		vending_triggers[ i ] thread vending_trigger_think();
	}

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
// MEASURED (qconsole.log, 2 occurrences): "precacheItem must be called before
// any wait statements in the gametype or level script", thrown at the
// precacheItem( bottle ) that used to sit after `level waittill(
// perk + "_power_on" )` in vending().  Every bottle is precached here instead,
// from init, which is inside the precache window.
precache_perk_bottles()
{
	perks = perk_list();
	for ( i = 0; i < perks.size; i++ )
	{
		bottle = perk_bottle( perks[ i ] );
		if ( isDefined( bottle ) )
			precacheItem( bottle );
	}
}

// array() is NOT a registered builtin on this server (MEASURED; the same note
// is already on zm/character/character_zm_zombie.gsc:31), so the list is built
// by assignment.
perk_list()
{
	list = [];
	list[ list.size ] = "specialty_armorvest";
	list[ list.size ] = "specialty_quickrevive";
	list[ list.size ] = "specialty_fastreload";
	list[ list.size ] = "specialty_rof";
	list[ list.size ] = "specialty_longersprint";
	list[ list.size ] = "specialty_flakjacket";
	list[ list.size ] = "specialty_deadshot";
	list[ list.size ] = "specialty_additionalprimaryweapon";
	return list;
}

// The localized buy prompt for a perk machine.  Every key below is MEASURED
// present in CoDOL's shipped localize set (docs/zone_assets/ALL_localize.txt,
// localized_code_post_gfx_mp + zm_prototype_sh + zm_zombie_training), and each
// takes the cost as an &&1 format argument -- the same shape T5 uses at
// _zombiemode_perks.gsc:1202-1244.  Mule Kick DOES ship, as
// ZOMBIE_PERK_ADDITIONALPRIMARYWEAPON.
perk_hint( perk, solo )
{
	switch( perk )
	{
	case "specialty_armorvest":
		return &"ZOMBIE_PERK_JUGGERNAUT";
	case "specialty_quickrevive":
		if ( is_true( solo ) )
			return &"ZOMBIE_PERK_QUICKREVIVE_SOLO";
		return &"ZOMBIE_PERK_QUICKREVIVE";
	case "specialty_fastreload":
		return &"ZOMBIE_PERK_FASTRELOAD";
	case "specialty_rof":
		return &"ZOMBIE_PERK_DOUBLETAP";
	case "specialty_longersprint":
		return &"ZOMBIE_PERK_MARATHON";
	case "specialty_flakjacket":
		return &"ZOMBIE_PERK_DIVETONUKE";
	case "specialty_deadshot":
		return &"ZOMBIE_PERK_DEADSHOT";
	case "specialty_additionalprimaryweapon":
		return &"ZOMBIE_PERK_ADDITIONALPRIMARYWEAPON";
	}

	// CoDOL ships a generic "buy a perk" line for anything unmapped rather
	// than T5's &"ZOMBIE_UNDEFINED", which CoDOL does not ship.
	return &"ZOMBIE_PERK_GENERIC";
}

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
	self setHintString( &"ZOMBIE_NEED_POWER" );

	// T5 gates every machine but solo quick-revive behind the power switch.
	if ( !( solo && perk == "specialty_quickrevive" ) )
		level waittill( perk + "_power_on" );

	self setHintString( perk_hint( perk, solo ), cost );

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
	self setHintString( &"ZOMBIE_NEED_POWER" );

	level waittill( "Pack_A_Punch_on" );

	self setHintString( &"ZOMBIE_PERK_PACKAPUNCH", cost );

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

		// T5 :450.  Take the gun, run the machine's presentation, hand the
		// upgrade back.  All three are ported now.
		self setHintString( "" );

		// The knuckle crack takes the held weapon itself (T5 :641), so the
		// take is not repeated here.
		player thread do_knuckle_crack();
		player thread third_person_weapon_upgrade( current, upgrade, self );

		wait( level.packapunch_timeout );

		player notify( "weapon_change_complete" );

		if ( is_player_valid( player ) )
		{
			player giveWeapon( upgrade );
			player giveMaxAmmo( upgrade );
			player switchToWeapon( upgrade );
			player play_sound_on_ent( "purchase" );
		}

		self setHintString( &"ZOMBIE_PERK_PACKAPUNCH", cost );
		flag_clear( "pack_machine_in_use" );
	}
}

// ==========================================================================
// THE PACK-A-PUNCH PRESENTATION.  T5 _zombiemode_perks.gsc:283
// third_person_weapon_upgrade, :611 do_knuckle_crack, :624
// upgrade_knuckle_crack_begin, :663 upgrade_knuckle_crack_end.
//
// Listed as deferred by the previous pass on the grounds that both were
// animscripted().  Neither is -- see the header.  Ported.
// ==========================================================================

// T5 :611.  Self is the player.  Swap to the knuckles for the duration of
// the upgrade, then swap back.
//
// waittill_any is not a registered builtin and _zombiemode_utility has only
// the two-message waittill_either; the four T5 messages are covered by a
// notify-relay thread per message onto one struct, which is what waittill_any
// is.
do_knuckle_crack()
{
	self endon( "disconnect" );

	gun = self upgrade_knuckle_crack_begin();
	if ( !isDefined( gun ) )
		return;

	waiter = spawnStruct();
	self thread knuckle_crack_relay( waiter, "death" );
	self thread knuckle_crack_relay( waiter, "fake_death" );
	self thread knuckle_crack_relay( waiter, "player_downed" );
	self thread knuckle_crack_relay( waiter, "weapon_change_complete" );

	waiter waittill( "done" );

	self upgrade_knuckle_crack_end( gun );
}

knuckle_crack_relay( waiter, message )
{
	self endon( "disconnect" );
	waiter endon( "done" );

	self waittill( message );

	waiter notify( "done" );
}

// T5 :624.
//
// T5 locks six movement modes.  MEASURED: of AllowLean / AllowAds /
// AllowSprint / AllowCrouch / AllowProne / AllowMelee, only AllowAds and
// AllowSprint are registered on this server, so those two are set and the
// other four are not faked.  T5's "if prone, stand up to crouch" is done with
// getStance / setStance, which ARE registered.
upgrade_knuckle_crack_begin()
{
	weapon = "zombie_knuckle_crack";

	// MEASURED, docs/zone_assets/ALL_weapon.txt: zombie_knuckle_crack ships
	// in common_gametype_zm.  Validated the same way every other zm weapon
	// is, so a zone without it degrades to "no presentation" rather than to
	// a player left holding nothing.
	precacheItem( weapon );
	if ( getWeaponIndexFromName( weapon ) <= 0 )
		return undefined;

	gun = self getCurrentWeapon();
	if ( !isDefined( gun ) || gun == "none" )
		return undefined;

	self allowAds( false );
	self allowSprint( false );

	if ( self getStance() == "prone" )
		self setStance( "crouch" );

	self notify( "zmb_lost_knife" );
	self takeWeapon( gun );

	self giveWeapon( weapon );
	self switchToWeapon( weapon );

	return gun;
}

// T5 :663.
upgrade_knuckle_crack_end( gun )
{
	weapon = "zombie_knuckle_crack";

	self allowAds( true );
	self allowSprint( true );

	self takeWeapon( weapon );

	if ( self maps\mp\gametypes\_zombiemode_laststand::player_is_in_laststand() )
		return;

	// The caller gives and switches to the upgraded weapon straight after
	// this returns; falling back to a primary only matters when it did not
	// (the player went down, or the machine timed out).
	primaries = self getWeaponsListPrimaries();
	if ( isDefined( primaries ) && primaries.size > 0 )
		self switchToWeapon( primaries[ 0 ] );
}

// T5 :283 third_person_weapon_upgrade.  Self is the player, machine is the
// Pack-a-Punch trigger.
//
// T5 takes (current_weapon, origin, angles, packa_rollers, perk_machine) off
// the machine's own authored struct.  CoDOL's zm maps author the machine as a
// single trigger with no rollers child (docs/CODOL_ZOMBIES_STARTUP_20260915.md
// lists no packa_rollers entity on any of the three zm maps), so the rig is
// built off the trigger's own origin and angles and the roller rotation is
// dropped -- there is nothing to rotate.
//
// ONE T5 CALL IS UNAVAILABLE: worldgun useweaponhidetags( weapon ), which
// culls the attachment tags the world model should not draw.  MEASURED, it is
// not a registered builtin.  The world gun therefore draws with every tag
// visible.  That is a cosmetic difference on a model shown for four seconds,
// and it is NOT stubbed -- the call is simply absent and named here.
//
// The dual-wield branch (T5 :296) is dropped with it: it needs
// weapon_is_dual_wield and get_left_hand_weapon_model_name, neither of which
// this port's _zombiemode_weapons carries.
third_person_weapon_upgrade( current_weapon, upgrade_name, machine )
{
	self endon( "disconnect" );

	if ( !isDefined( machine ) )
		return;

	origin = machine.origin;
	angles = machine.angles;
	if ( !isDefined( angles ) )
		angles = ( 0, 0, 0 );

	forward = anglesToForward( angles );
	interact_pos = origin + ( forward * -25 );

	if ( isDefined( level._effect[ "packapunch_fx" ] ) )
		playFx( level._effect[ "packapunch_fx" ], origin + ( 0, 1, -34 ), forward );

	// The gun goes in.
	model = getWeaponModel( current_weapon );
	worldgun = undefined;
	if ( isDefined( model ) && model != "" )
	{
		worldgun = spawn( "script_model", interact_pos );
		worldgun.angles = self.angles;
		worldgun setModel( model );
		worldgun rotateto( angles + ( 0, 90, 0 ), 0.35, 0, 0 );
	}

	wait( 0.5 );

	if ( isDefined( worldgun ) )
		worldgun moveto( origin, 0.5, 0, 0 );

	self playSound( "zmb_perks_packa_upgrade" );

	wait( 0.35 );

	if ( isDefined( worldgun ) )
		worldgun delete();

	// T5 waits 3 s here; this port waits out whatever is left of
	// level.packapunch_timeout so the presentation and the hand-back finish
	// together however the machine is tuned.
	remaining = level.packapunch_timeout - 1.5;
	if ( remaining < 0.5 )
		remaining = 0.5;
	wait( remaining );

	self playSound( "zmb_perks_packa_ready" );

	// The upgraded gun comes out.
	upgrade_model = getWeaponModel( upgrade_name );
	if ( !isDefined( upgrade_model ) || upgrade_model == "" )
		return;

	worldgun = spawn( "script_model", origin );
	worldgun.angles = angles + ( 0, 90, 0 );
	worldgun setModel( upgrade_model );
	worldgun moveto( interact_pos, 0.5, 0, 0 );

	wait( 1 );

	worldgun delete();
}
