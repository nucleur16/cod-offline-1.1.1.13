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
//                                           bump. CoDOL Quick Revive speeds
//                                           teammate revival only.
//   perk_think                    :1566  -- perks are lost on down/death.
//   vending_weapon_upgrade        :393   -- Pack-a-Punch: pay, take the gun,
//                                           hand back its _upgraded variant.
//   vending_weapon_upgrade_cost   :519
//   wait_for_player_to_take       :538
//   wait_for_timeout              :595
//   turn_*_on / perk_fx            -- powered models, startup sound and
//                                    delayed tagged lights restored below.
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
//   2026-10-05: perk_hud_create / _destroy / _flash (:1632-:1738) are now
//   PORTED as THE PERK HUD (see that section): C1's own zombies server
//   precache list, mp/constConfigStrings_zm.csv, carries every T5 perk icon
//   (specialty_juggernaut_zombies .. specialty_ads_zombies), their images
//   ship in common_gametype_zm, and C1 has no native perk display a zombies
//   perk could reach.  perk_flash_audio (:1740) is still not taken.
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

#include maps\_zombiemode_utility;

init()
{
	set_zombie_var( "zombie_perk_cost", 2000 );
	// T5 _zombiemode_perks.gsc:60: 160.  CoDOL's coPerkStatsTable row 43
	// (specialty_zombie_armorvest) lists modMaxHealth +150, i.e. 250, but
	// with T5's 60-point zombie hit (_zombiemode_spawner.gsc meleeDamage)
	// 250 survives four hits and goes down on the fifth.  Juggernog must go
	// down on the third hit -- 160 -> 100 -> 40 -> down -- against the plain
	// player's second (100 -> 40 -> down), so T5's value is kept.
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
	perk_machine_power_init();

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

	perk_hud_precache();
	level thread perk_hud_think();
	level thread perk_hud_end_game();

	println( "CoDOL zm: " + vending_triggers.size + " perk machines, " +
		upgrade_triggers.size + " pack-a-punch\n" );

	// Which world models the Pack-a-Punch presentation draws for the starting
	// pistol: the server's own weapon definitions decide it.
	hide = "";
	tags = getWeaponHideTags( "m1911_upgraded_zm" );
	if ( isDefined( tags ) )
	{
		for ( i = 0; i < tags.size; i++ )
			hide = hide + " " + tags[ i ];
	}
	codolprintln( "CoDOL zm pap: m1911_zm world " + getWeaponModel( "m1911_zm" ) + ", m1911_upgraded_zm world " +
		getWeaponModel( "m1911_upgraded_zm" ) + " hiding" + hide );
}

// BO1 _zombiemode_perks::turn_jugger_on / turn_revive_on / turn_sleight_on /
// turn_doubletap_on and perk_fx. C1 calls the sleight model speedcola_on and
// ships separate patch glow effects alongside the original machine lights.
perk_machine_power_definition( target, model, light, glow )
{
	definition = spawnStruct();
	definition.target = target;
	definition.model = model;
	definition.light = light;
	definition.glow = glow;
	return definition;
}

perk_machine_power_init()
{
	if ( isDefined( level.zm_perk_power_machines ) )
		return;
	level.zm_perk_power_machines = [];
	definitions = [];
	definitions[ 0 ] = perk_machine_power_definition( "vending_jugg", "zombie_vending_jugg_on",
		"misc/fx_zombie_cola_jugg_on", "misc/fx_zombie_cola_jugg_on_glow" );
	definitions[ 1 ] = perk_machine_power_definition( "vending_revive", "zombie_vending_revive_on",
		"misc/fx_zombie_cola_revive_on", "misc/fx_zombie_cola_revive_on_glow" );
	definitions[ 2 ] = perk_machine_power_definition( "vending_sleight", "zombie_vending_speedcola_on",
		"misc/fx_zombie_cola_on", "misc/fx_zombie_cola_on_glow" );
	definitions[ 3 ] = perk_machine_power_definition( "vending_doubletap", "zombie_vending_doubletap_on",
		"misc/fx_zombie_cola_dtap_on", "misc/fx_zombie_cola_doubletap_on_glow" );

	for ( i = 0; i < definitions.size; i++ )
	{
		definition = definitions[ i ];
		machines = getEntArray( definition.target, "targetname" );
		for ( j = 0; j < machines.size; j++ )
		{
			machine = machines[ j ];
			// Target groups can also contain collision brushes. Only swap the
			// rendered machine; preserve its trigger, clip and authored pose.
			if ( machine.classname != "script_model" )
				continue;
			precacheModel( definition.model );
			precacheModel( "tag_origin" );
			machine.zm_perk_on_model = definition.model;
			machine.zm_perk_light_fx = loadfx( definition.light );
			machine.zm_perk_glow_fx = loadfx( definition.glow );
			level.zm_perk_power_machines[ level.zm_perk_power_machines.size ] = machine;
			machine thread perk_machine_wait_for_power();
		}
	}
}

perk_machine_wait_for_power()
{
	// Persistent flag also covers maps whose power starts on and cannot miss
	// the one-shot per-perk notifications sent by the power module.
	flag_wait( "power_on" );
	self perk_machine_power_on();
}

perk_machine_power_on()
{
	if ( is_true( self.zm_perk_machine_powered ) )
		return;
	self.zm_perk_machine_powered = true;
	self setModel( self.zm_perk_on_model );
	self playSound( "zmb_perks_power_on" );
	// BO1 perk_fx delays the persistent light until the startup finishes.
	wait( 3 );
	// C1's powered machine meshes have an unnamed root bone. Use BO1's
	// revive_solo_fx carrier pattern instead of tagging the machine itself.
	// Untagged LinkTo preserves the machine transform without a parent bone.
	carrier = spawn( "script_model", self.origin );
	carrier.angles = self.angles;
	carrier setModel( "tag_origin" );
	carrier notSolid();
	carrier linkTo( self );
	self.zm_perk_light_carrier = carrier;
	playFxOnTag( self.zm_perk_light_fx, carrier, "tag_origin" );
	playFxOnTag( self.zm_perk_glow_fx, carrier, "tag_origin" );
	self.zm_perk_machine_lights_on = true;
}

// T5 :1083 cost ladder.
perk_cost( perk, solo )
{
	switch( perk )
	{
	case "specialty_armorvest":
		return 2500;

	case "specialty_quickrevive":
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

	// The Pack-a-Punch knuckle crack (upgrade_knuckle_crack_begin) is the same
	// case.  MEASURED (qconsole.log, zm_sumpf_sh PaP buy): its precacheItem
	// at buy time threw "precacheItem must be called before any wait
	// statements", the definition was registered late (engine 118, "wire -1"),
	// and the switchToWeapon that followed went out as "CoDOL selection weapon
	// 118 has no published C1 index; selecting none" -- so the client never
	// raised the knuckle viewmodel and no animation played.  Precached here it
	// is in the native table from map start, mapped to its zm constant slot
	// (CS 5475, index 113) before anyone can buy.
	precacheItem( "zombie_knuckle_crack" );
}

// array() is NOT a registered builtin on this server (MEASURED; the same note
// is already on character/character_zm_zombie.gsc:31), so the list is built
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
		{}
		return;
	}

	// CoDOL Quick Revive is the teammate-revival perk in every roster size.
	// Self-revival belongs to revive tokens, not the vending machine.
	flag_wait( "all_players_connected" );
	cost = perk_cost( perk, false );
	self.cost = cost;

	self setCursorHint( "HINT_NOICON" );
	self useTriggerRequireLookAt();
	self setHintString( &"ZOMBIE_NEED_POWER" );
	flag_wait( "power_on" );
	self setHintString( perk_hint( perk, false ), cost );

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
			player maps\_zombiemode_audio::no_cash_vox();
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
		player maps\_zombiemode_score::minus_to_player_score( cost );

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

	self.zm_knuckle_cracking = true;
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
	self.zm_knuckle_cracking = false;
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

	// Switch back first and take the bottle once the switch has begun: a
	// change from "none" carries no raise event, so the gun came back in
	// silence (see _zombiemode_weapons::weapon_give).
	back = undefined;
	if ( isDefined( gun ) && gun != "none" && self hasWeapon( gun ) )
		back = gun;

	for ( i = 0; i < bottles.size; i++ )
	{
		if ( !self hasWeapon( bottles[ i ] ) )
			continue;
		if ( isDefined( back ) )
			self thread maps\_zombiemode_weapons::take_weapon_after_switch( bottles[ i ], back );
		else
			self takeWeapon( bottles[ i ] );
	}

	if ( isDefined( back ) )
		self switchToWeapon( back );

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

	// C1 describes faster teammate revival; do not award a solo spare life.

	self notify( "perk_bought", perk );
	self thread perk_think( perk );
}

// T5 :1566.  Perks are lost when the player goes down or dies.
perk_think( perk )
{
	self endon( "disconnect" );

	self waittill_either( "death", "player_downed" );

	// Preserve perks only after the lobby acknowledges an actual token spend.
	if ( isDefined( self.revive_token_window ) && self.revive_token_window )
	{
		self waittill_either( "player_revived", "bled_out" );
		if ( isDefined( self.revive_used_token ) && self.revive_used_token )
		{
			self thread perk_think( perk );
			return;
		}
	}

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
// THE PERK HUD.  T5 :1632 perk_hud_create, :1722 perk_hud_destroy and :1728
// perk_hud_flash: one small icon per perk the player holds, left to right in
// the order they were gained, destroyed when the perk is lost.
//
// WHY A HUDELEM AND NOT A NATIVE DISPLAY (C1 static analysis, 2026-10-05):
//   * C1 SetPerk (518a10 -> 40f0b0) ends the game on a no_slot row, so a
//     zombies perk (coPerkStatsTable 43-57, specialty_zombie_*) never enters
//     the playerState perk slots; this server keeps them in its own set
//     (codol_script_perks.c).  Their icon column is specialty_null anyway.
//   * The zombies player HUD, ownerdraw 288 "playerscores" (common_mp menu
//     zombiemodescores, C1 0x00460c50 -> 0x00460590), draws only portrait,
//     name and score.  perk_display / perk_hide (common_mp) are the
//     multiplayer spawn popups, not a zombies HUD.
//   * C1's zombies server precache list mp/constConfigStrings_zm.csv
//     (code_post_gfx_mp, ids 4364-4381) holds exactly T5's perk icons --
//     specialty_juggernaut_zombies, _quickrevive_, _fastreload_, _doubletap_,
//     _marathon_, _divetonuke_, _ads_, _extraprimaryweapon_ -- and the perk
//     pop icons of coKillstreakStatsTable column 15 (_discount_, _bandolier_,
//     _electro_, _carpenter_).  Their images are in common_gametype_zm.  So
//     C1's server drew them as script hudelems, as T5 does.
//
// SOURCES.  Machine perks (and the free-perk power-up) by hasPerk on their T5
// names; perk pops by _zombiemode_perkpops' self.perkpop_active, keyed by
// effect.  A machine perk and a perk pop with the same icon (Deadshot,
// Stamin-Up, PhD Flopper, Mule Kick) show one icon.  The state is polled, so
// every path that gives or takes a perk -- the machines, give_perk from the
// free-perk power-up and the test hooks, perk_think on a down or death, the
// perk pops' award and remove_effects -- is covered without a hook in each.
//
// PLACEMENT (2026-10-05, against the original 2014 CoDOL zombies HUD).  The
// original draws the owned perks, then the active power-ups, as ONE row of
// small icons to the RIGHT of the local player's panel, level with the score
// line under the name -- not above the portrait, where C1 draws the round
// text ("final round" / round N) that the first version of this row covered.
//
// The local entry of ownerdraw 288 is left_adjustable / bottom_adjustable at
// x 10, y -55, scale 1.1 (C1 0x00460db7): a 44-unit portrait from y -55 to
// -11, the name from x ~57 on the top line (y -50..-40), the name bar's rule
// at y ~-37 fading out by x ~160, and the score below it (y -28..-18, centre
// -23).  MEASURED off a 1680x1050 C1 frame (2.1875 px/unit, perk icons of
// the first version as the ruler) and the original frame (portrait = 44
// units): the original's first icon starts 163 units right of the portrait's
// left edge (x 173), the icons are ~0.45 of the portrait's height and step
// ~29 units, centred a few units under the score's centre.
//
// So the row: x from 173, icons 20 units square every 26, vertically centred
// on y -23 (top -33, bottom -13) -- under the name rule (-37) and above the
// screen edge, never over the round text, the name or the score.  Both axes
// are adjustable-aligned and the panel is too, so the row keeps its place
// beside the panel at 4:3, 16:10 and 16:9.  The power-up HUD
// (_zombiemode_powerups) calls zm_hud_row_x with the perk count added, so the
// power-ups follow the perks, with their countdown in the band under the row.
// ==========================================================================

ZM_HUD_ROW_X = 173;
ZM_HUD_ROW_Y = -23;
ZM_HUD_ROW_SIZE = 20;
ZM_HUD_ROW_STEP = 26;
PERK_HUD_FLASH_SIZE = 26;

// The x of column n of the shared perk / power-up row.
zm_hud_row_x( column )
{
	return ZM_HUD_ROW_X + column * ZM_HUD_ROW_STEP;
}

zm_hud_row_y()
{
	return ZM_HUD_ROW_Y;
}

zm_hud_row_size()
{
	return ZM_HUD_ROW_SIZE;
}

// self = player.  How many perk icons lead the row.
zm_hud_perk_count()
{
	if ( !isDefined( self.zm_perk_hud ) )
		return 0;
	return self.zm_perk_hud.size;
}

perk_hud_precache()
{
	level.zm_perk_hud_machine = [];
	level.zm_perk_hud_pop = [];

	// T5 :1652 shader switch, by the T5 perk names the machines use.
	perk_hud_machine( "specialty_armorvest",               "specialty_juggernaut_zombies" );
	perk_hud_machine( "specialty_quickrevive",             "specialty_quickrevive_zombies" );
	perk_hud_machine( "specialty_fastreload",              "specialty_fastreload_zombies" );
	perk_hud_machine( "specialty_rof",                     "specialty_doubletap_zombies" );
	perk_hud_machine( "specialty_longersprint",            "specialty_marathon_zombies" );
	perk_hud_machine( "specialty_flakjacket",              "specialty_divetonuke_zombies" );
	perk_hud_machine( "specialty_deadshot",                "specialty_ads_zombies" );
	perk_hud_machine( "specialty_additionalprimaryweapon", "specialty_extraprimaryweapon_zombies" );

	// The perk pops, by _zombiemode_perkpops::effect_key; the icons are
	// column 15 of their mp/coKillstreakStatsTable.csv rows 59-67.
	perk_hud_pop( "deadshot",          "specialty_ads_zombies" );
	perk_hud_pop( "longersprint",      "specialty_marathon_zombies" );
	perk_hud_pop( "flakjacket",        "specialty_divetonuke_zombies" );
	perk_hud_pop( "additionalprimary", "specialty_extraprimaryweapon_zombies" );
	perk_hud_pop( "discount",          "specialty_discount_zombies" );
	perk_hud_pop( "scavenger",         "specialty_bandolier_zombies" );
	perk_hud_pop( "electro",           "specialty_electro_zombies" );
	perk_hud_pop( "carpenter",         "specialty_carpenter_zombies" );
}

perk_hud_machine( perk, shader )
{
	precacheShader( shader );
	k = spawnStruct();
	k.perk = perk;
	k.shader = shader;
	level.zm_perk_hud_machine[ level.zm_perk_hud_machine.size ] = k;
}

perk_hud_pop( key, shader )
{
	precacheShader( shader );
	k = spawnStruct();
	k.key = key;
	k.shader = shader;
	level.zm_perk_hud_pop[ level.zm_perk_hud_pop.size ] = k;
}

perk_hud_think()
{
	level endon( "end_game" );

	for ( ;; )
	{
		wait( 0.1 );

		// Test clients draw nothing -- except on a developer server, where
		// the probes exercise this path through them (as the power-up HUD).
		with_bots = getDvarInt( "developer" ) > 0;

		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( isDefined( players[ i ] ) && ( with_bots || !is_true( players[ i ].isbot ) ) )
				players[ i ] perk_hud_update();
		}
	}
}

// The game is over: every perk icon goes.  perk_hud_think has stopped.
perk_hud_end_game()
{
	level waittill( "end_game" );

	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isDefined( players[ i ] ) )
			players[ i ] perk_hud_clear( "game over" );
	}
}

// self = player.  The icons it should show, one per distinct shader.
perk_hud_owned()
{
	owned = [];

	for ( i = 0; i < level.zm_perk_hud_machine.size; i++ )
	{
		k = level.zm_perk_hud_machine[ i ];
		if ( self hasPerk( k.perk ) )
			owned = perk_hud_add_unique( owned, k.shader );
	}

	if ( isDefined( self.perkpop_active ) )
	{
		for ( i = 0; i < level.zm_perk_hud_pop.size; i++ )
		{
			k = level.zm_perk_hud_pop[ i ];
			if ( is_true( self.perkpop_active[ k.key ] ) )
				owned = perk_hud_add_unique( owned, k.shader );
		}
	}

	return owned;
}

perk_hud_add_unique( list, shader )
{
	if ( !perk_hud_has( list, shader ) )
		list[ list.size ] = shader;
	return list;
}

perk_hud_has( list, shader )
{
	for ( i = 0; i < list.size; i++ )
	{
		if ( list[ i ] == shader )
			return true;
	}
	return false;
}

perk_hud_list_has( list, shader )
{
	for ( i = 0; i < list.size; i++ )
	{
		if ( list[ i ].shader == shader )
			return true;
	}
	return false;
}

// self = player.
perk_hud_update()
{
	if ( !isDefined( self.zm_perk_hud ) )
		self.zm_perk_hud = [];

	owned = self perk_hud_owned();

	// T5 :1722 perk_hud_destroy for every icon whose perk is gone.
	kept = [];
	changed = false;
	for ( i = 0; i < self.zm_perk_hud.size; i++ )
	{
		h = self.zm_perk_hud[ i ];
		if ( perk_hud_has( owned, h.shader ) )
		{
			kept[ kept.size ] = h;
			continue;
		}
		if ( isDefined( h.elem ) )
			h.elem destroy();
		changed = true;
		{}
	}

	// T5 :1632 perk_hud_create for every new one, appended on the right.
	for ( i = 0; i < owned.size; i++ )
	{
		if ( perk_hud_list_has( kept, owned[ i ] ) )
			continue;
		h = spawnStruct();
		h.shader = owned[ i ];
		h.elem = self perk_hud_elem( owned[ i ], zm_hud_row_x( kept.size ) );
		kept[ kept.size ] = h;
		changed = true;
		h.elem thread perk_hud_flash();
		{}
	}

	self.zm_perk_hud = kept;
	if ( !changed )
		return;

	// Close the gap a lost perk leaves.
	for ( i = 0; i < kept.size; i++ )
	{
		x = zm_hud_row_x( i );
		if ( isDefined( kept[ i ].elem ) && kept[ i ].elem.x != x )
			kept[ i ].elem.x = x;
	}
}

// self = player.
perk_hud_elem( shader, x )
{
	elem = newClientHudElem( self );
	elem.alignX = "left";
	elem.alignY = "middle";
	elem.horzAlign = "left_adjustable";
	elem.vertAlign = "bottom_adjustable";
	elem.x = x;
	elem.y = ZM_HUD_ROW_Y;
	elem.foreground = true;
	elem.hidewheninmenu = true;
	elem.archived = false;
	elem.sort = 1;
	elem.alpha = 1;
	elem setShader( shader, ZM_HUD_ROW_SIZE, ZM_HUD_ROW_SIZE );
	return elem;
}

// T5 :1728 perk_hud_flash: a short pulse (T5 24 -> 32, here 20 -> 26) when
// the perk is gained.
// self = the icon; destroy() notifies "death" on it.
perk_hud_flash()
{
	self endon( "death" );

	self scaleOverTime( 0.05, PERK_HUD_FLASH_SIZE, PERK_HUD_FLASH_SIZE );
	wait( 0.3 );
	if ( !isDefined( self ) )
		return;
	self scaleOverTime( 0.05, ZM_HUD_ROW_SIZE, ZM_HUD_ROW_SIZE );
}

// self = player.
perk_hud_clear( reason )
{
	if ( !isDefined( self.zm_perk_hud ) )
		return;

	for ( i = 0; i < self.zm_perk_hud.size; i++ )
	{
		if ( isDefined( self.zm_perk_hud[ i ].elem ) )
			self.zm_perk_hud[ i ].elem destroy();
	}
	if ( self.zm_perk_hud.size > 0 )
		{}
	self.zm_perk_hud = [];
}

// ==========================================================================
// PACK-A-PUNCH -- T5 :393 vending_weapon_upgrade, :519 cost, :538 / :595 the
// take-or-time-out pair.
//
// The upgrade names are real content here: MEASURED from
// docs/zone_assets/ALL_weapon.txt, CoDOL's zm zones ship 40-odd
// "<base>_upgraded_zm" weapons alongside their bases, which is exactly the
// pairing maps\_zombiemode_weapons::add_zombie_weapon records.
// ==========================================================================

vending_weapon_upgrade_cost()
{
	return 5000;
}

vending_weapon_upgrade()
{
	flag_init( "pack_machine_in_use" );

	cost = vending_weapon_upgrade_cost();

	// T5 :264 packa_timer: the script_origin the tick-tock and deny sounds
	// play on while the upgraded gun waits in the machine.
	packa_timer = spawn( "script_origin", self.origin );

	self setCursorHint( "HINT_NOICON" );
	self useTriggerRequireLookAt();
	self setHintString( &"ZOMBIE_NEED_POWER" );

	level waittill( "Pack_A_Punch_on" );

	self setHintString( &"ZOMBIE_PERK_PACKAPUNCH", cost );
	self.pap_owner = undefined;

	for ( ;; )
	{
		self waittill( "trigger", player );

		// A trigger while the upgraded gun waits in the machine belongs to
		// wait_for_player_to_take, not to a new purchase.
		if ( flag( "pack_machine_in_use" ) )
			continue;

		if ( !isDefined( player ) || !isPlayer( player ) || !is_player_valid( player ) )
			continue;

		current = player getCurrentWeapon();
		if ( !isDefined( current ) || current == "none" )
		{
			wait( 0.1 );
			continue;
		}

		// T5 :291 refuses an upgraded weapon outright (no charge, no sound).
		if ( maps\_zombiemode_weapons::is_weapon_upgraded( current ) )
		{
			wait( 0.1 );
			continue;
		}

		upgrade = maps\_zombiemode_weapons::get_upgrade_name( current );
		if ( !isDefined( upgrade ) )
		{
			// T5 :302: a weapon the table does not know is ignored.
			continue;
		}

		if ( player.score < cost )
		{
			// T5 :305 "deny" on the machine and the perk_deny dialog.
			self play_sound_on_ent( "perk_deny" );
			player maps\_zombiemode_audio::no_cash_vox();
			continue;
		}

		flag_set( "pack_machine_in_use" );
		player maps\_zombiemode_score::minus_to_player_score( cost );

		// T5 :311-314: the dispense sound and the Pack-a-Punch sting.
		playSoundAtPosition( "evt_bottle_dispense", self.origin );
		playSoundAtPosition( "mus_perks_packa_sting", self.origin );

		self setHintString( "" );
		player thread do_knuckle_crack();

		// T5 :325: the presentation returns with the upgraded gun sitting in
		// the machine's mouth, already sinking back over packapunch_timeout.
		weaponmodel = player third_person_weapon_upgrade( current, upgrade, self );

		// T5 :327-330: only the buyer may take it.  This server has no
		// SetVisibleToPlayer, so the "take" prompt is shown to everyone at
		// the machine and wait_for_player_to_take ignores anyone else.
		self.pap_owner = player;
		self setHintString( &"ZOMBIE_GET_UPGRADED" );
		self thread wait_for_player_to_take( player, current, upgrade, packa_timer );
		self thread wait_for_timeout( current, packa_timer );
		self waittill_either( "pap_timeout", "pap_taken" );

		// T5 loses the gun on a timeout.  A buyer whose only gun it was would
		// then hold nothing, and the box and wall buys refuse an empty hand,
		// so that buyer gets the base weapon back instead.
		if ( isDefined( player ) && is_player_valid( player ) && !player hasWeapon( upgrade ) )
		{
			primaries = player getWeaponsListPrimaries();
			has_primary = false;
			for ( i = 0; isDefined( primaries ) && i < primaries.size; i++ )
			{
				if ( primaries[ i ] != "zombie_knuckle_crack" )
					has_primary = true;
			}
			if ( !has_primary )
			{
				player maps\_zombiemode_weapons::weapon_give( current );
				codolprintln( "CoDOL zm pap: " + player.name + " left " + upgrade + " in the machine with no other gun; " + current + " given back" );
			}
			else
				player release_knuckle_crack();
		}

		self.pap_owner = undefined;
		if ( isDefined( weaponmodel ) )
			weaponmodel delete();

		self setHintString( &"ZOMBIE_PERK_PACKAPUNCH", cost );
		flag_clear( "pack_machine_in_use" );
	}
}

// T5 :351.  Self is the machine.  The tick-tock loops until the buyer takes
// the gun; anyone else's use does nothing.
wait_for_player_to_take( player, weapon, upgrade_weapon, packa_timer )
{
	self endon( "pap_timeout" );

	for ( ;; )
	{
		packa_timer playLoopSound( "zmb_perks_packa_ticktock" );
		self waittill( "trigger", trigger_player );
		packa_timer stopLoopSound( 0.05 );

		if ( isDefined( trigger_player ) && isDefined( player ) && trigger_player == player )
		{
			// T5 :801 does not require a weapon in hand: a player whose only
			// gun went into the machine holds nothing ("none") until taking it.
			current = player getCurrentWeapon();
			if ( is_player_valid( player ) && !is_true( player.zm_knuckle_cracking ) &&
				( !isDefined( current ) || !maps\_zombiemode::is_placeable_mine( current ) ) )
			{
				self notify( "pap_taken" );
				player notify( "pap_taken" );
				player.pap_used = true;

				// T5 :366-374: over the weapon limit weapon_give swaps out the
				// held gun; under it the upgrade is simply added.
				player maps\_zombiemode_weapons::weapon_give( upgrade_weapon );
				player playSound( "zmb_perks_packa_ready" );
				return;
			}
		}
		wait( 0.05 );
	}
}

// T5 :385.  The gun goes back into the machine and is lost.
wait_for_timeout( weapon, packa_timer )
{
	self endon( "pap_taken" );

	wait( level.packapunch_timeout );

	self notify( "pap_timeout" );
	packa_timer stopLoopSound( 0.05 );
	packa_timer playSound( "zmb_perks_packa_deny" );
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
	// T5 ends the knuckle crack on "weapon_change_complete", which C1's
	// engine sends when the knuckle weapon's raise (viewmodel_zombie_packopunch,
	// 70 frames at 30 fps = 2.3 s) finishes.  This server never sends that
	// notify, so the raise's own length ends it.
	self thread knuckle_crack_timer( waiter, 2.3 );

	waiter waittill( "done" );

	self upgrade_knuckle_crack_end( gun );
}

knuckle_crack_timer( waiter, seconds )
{
	self endon( "disconnect" );
	waiter endon( "done" );

	wait( seconds );

	waiter notify( "done" );
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
	// in common_gametype_zm.  Precached by precache_perk_bottles (inside the
	// precache window; a precacheItem here, after waits, throws).  Validated
	// so a zone without it degrades to "no presentation" rather than to a
	// player left holding nothing.
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

	if ( self maps\_zombiemode_laststand::player_is_in_laststand() || !self hasWeapon( weapon ) )
	{
		if ( self hasWeapon( weapon ) )
			self takeWeapon( weapon );
		return;
	}

	// CoDOL (user decision 2026-10-21): the knuckles stay in hand until the
	// upgraded gun is taken from the machine.  Taking them here left the
	// player holding "none" (or switched to another gun), and the upgrade
	// then raised from "none": C1 sends EV_RAISE_WEAPON /
	// EV_FIRST_RAISE_WEAPON only for a change from a held weapon (0x419efc),
	// so the Mustang & Sally pullout had no raise sound.  weapon_give's
	// switch now leaves the knuckles, and knuckle_crack_hold takes them once
	// the player holds anything else.
	//
	// That only applies when the gun in the machine was the player's only
	// one (user, 2026-10-21): with another primary, switch to it now, as T5
	// does, raising it from the held knuckles; knuckle_crack_hold then takes
	// the knuckles once that gun is in hand.
	self thread knuckle_crack_hold( weapon );

	primaries = self getWeaponsListPrimaries();
	for ( i = 0; isDefined( primaries ) && i < primaries.size; i++ )
	{
		if ( primaries[ i ] != weapon && primaries[ i ] != gun )
		{
			self switchToWeapon( primaries[ i ] );
			break;
		}
	}
}

// Take the held knuckles as soon as the player holds another weapon: the
// upgrade from the machine, the gun handed back on a timeout
// (release_knuckle_crack), or anything the player switched to.
knuckle_crack_hold( weapon )
{
	self endon( "disconnect" );
	self notify( "zm_knuckle_crack_hold" );
	self endon( "zm_knuckle_crack_hold" );

	while ( self hasWeapon( weapon ) )
	{
		current = self getCurrentWeapon();
		if ( isDefined( current ) && current != weapon && current != "none" )
			break;
		if ( self maps\_zombiemode_laststand::player_is_in_laststand() )
			break;
		wait( 0.05 );
	}

	if ( self hasWeapon( weapon ) )
		self takeWeapon( weapon );
}

// The machine timed out (or the buyer could not take the gun): leave the
// knuckles for a primary, raising it from the held knuckles.
release_knuckle_crack()
{
	weapon = "zombie_knuckle_crack";

	if ( !self hasWeapon( weapon ) || self getCurrentWeapon() != weapon )
		return;

	primaries = self getWeaponsListPrimaries();
	for ( i = 0; isDefined( primaries ) && i < primaries.size; i++ )
	{
		if ( primaries[ i ] != weapon )
		{
			self switchToWeapon( primaries[ i ] );
			return;
		}
	}
}

// T5 :283 third_person_weapon_upgrade.  Self is the player, machine is the
// Pack-a-Punch trigger.
//
// T5 :669-681 places the gun off the machine model the trigger targets:
// origin = perk_machine.origin + (0,0,35), angles = perk_machine.angles +
// (0,90,0), falling back to the trigger's own only when there is no model.
// The trigger is no substitute: on zm_sumpf_sh it has no angles and sits
// 50 units above and 17 units in front of the machine (trigger 10670 791 -611,
// zombie_vending_packapunch_on 10683.5 801.55 -661 yaw 315), which put the gun
// too high and turned 45 degrees.  CoDOL's machine faces the way T5's does:
// the trigger lies on the model's -Y side, where T5's forward * -25 slides the
// gun out.  CoDOL's zm maps author no packa_rollers child
// (docs/CODOL_ZOMBIES_STARTUP_20260915.md), so the roller rotation is dropped.
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
		return undefined;

	origin = machine.origin;
	angles = ( 0, 0, 0 );
	if ( isDefined( machine.angles ) )
		angles = machine.angles;

	perk_machine = undefined;
	if ( isDefined( machine.target ) )
		perk_machine = getEnt( machine.target, "targetname" );
	if ( isDefined( perk_machine ) )
	{
		origin = perk_machine.origin + ( 0, 0, 35 );
		angles = perk_machine.angles + ( 0, 90, 0 );
	}

	forward = anglesToForward( angles );
	interact_pos = origin + ( forward * -25 );

	if ( isDefined( level._effect[ "packapunch_fx" ] ) )
		playFx( level._effect[ "packapunch_fx" ], origin + ( 0, 1, -34 ), forward );

	// T5 :196-212: the gun turns to face the machine (0.35 s), waits 0.5 s,
	// slides in (0.5 s) with zmb_perks_packa_upgrade, and is gone 0.35 s later.
	model = getWeaponModel( current_weapon );
	worldgun = undefined;
	if ( isDefined( model ) && model != "" )
	{
		worldgun = spawn( "script_model", interact_pos );
		worldgun.angles = self.angles;
		worldgun setModel( model );
		worldgun useWeaponHideTags( current_weapon );
		worldgun rotateto( angles + ( 0, 90, 0 ), 0.35, 0, 0 );
	}

	wait( 0.5 );

	if ( isDefined( worldgun ) )
		worldgun moveto( origin, 0.5, 0, 0 );

	machine playSound( "zmb_perks_packa_upgrade" );

	wait( 0.35 );

	if ( isDefined( worldgun ) )
		worldgun delete();

	// T5 :222: three seconds inside the machine.
	wait( 3 );

	machine playSound( "zmb_perks_packa_ready" );

	// T5 :225-246: the upgraded gun slides out (0.5 s), rests 0.5 s, then
	// sinks back into the machine over the whole take window
	// (level.packapunch_timeout); the caller deletes it when the window closes.
	upgrade_model = getWeaponModel( upgrade_name );
	if ( !isDefined( upgrade_model ) || upgrade_model == "" )
		return undefined;

	worldgun = spawn( "script_model", origin );
	worldgun.angles = angles + ( 0, 90, 0 );
	worldgun setModel( upgrade_model );
	worldgun useWeaponHideTags( upgrade_name );
	worldgun moveto( interact_pos, 0.5, 0, 0 );

	wait( 0.5 );

	worldgun moveto( origin, level.packapunch_timeout, 0, 0 );
	return worldgun;
}
