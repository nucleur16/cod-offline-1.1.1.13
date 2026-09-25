// zm perk pops: C1's "survival streak" rewards.
//
// Every zombies character carries two perk pops (mp/cyborgclasstable.csv
// columns 10/11, recorded by _zombiemode_equipment as
// self.zombie_perkpop_names).  C1 files them as killstreak-style rewards:
//
// HOW ONE IS EARNED.  MEASURED, localized_code_post_gfx_mp (GBK, decoded
// 2026-09-24 from the shipped zone): every perk pop's reward description ends
// "连续存活N回合激活" -- "activated by surviving N consecutive rounds":
//
//   KILLSTREAKS_DEADSHOT_DESC          ... 连续存活3回合激活   (3)
//   KILLSTREAKS_LONGERSPRINT_DESC      ... 连续存活3回合激活   (3)
//   KILLSTREAKS_DISCOUNT_DESC          ... 连续存活3回合激活   (3)
//   KILLSTREAKS_FLAKJACKET_DESC        ... 连续存活4回合激活   (4)
//   KILLSTREAKS_CARPENTER_DESC         ... 连续存活6回合激活   (6)
//   KILLSTREAKS_ELECTRO_DESC           ... 连续存活6回合激活   (6)
//   KILLSTREAKS_SCAVENGER_DESC         ... 连续存活6回合激活   (6)
//   KILLSTREAKS_ADDITIONALPRIMARY_DESC ... 连续存活6回合激活   (6)
//
// and N is exactly the cost column 4 of the perk pop's row in
// mp/coKillstreakStatsTable.csv (rows 59-68), so the cost is read from that
// column rather than written here.  The earned notice is C1's own
// ZOMBIE_ROUNDSTREAK "存活&&1轮奖励！" ("Survived &&1 rounds -- reward!").
//
// WHAT BREAKS THE STREAK.  MEASURED, mp/coPerkStatsTable.csv row 63:
// specialty_perkpop_zm (PERKS_PERKPOP_ZM, "连续生存奖励" / "survival streak
// reward", a zombies perk1 choice) carries the ability modifier
// modLastStandKeepRoundStreak (mp/coAbilityModifiersTable.csv row 96).  A perk
// whose whole job is to KEEP the round streak through last stand means going
// down otherwise loses it.  So: a round counts when the player ends it up
// (alive, not in last stand) and was not downed during it; going down resets
// the streak to zero unless the player holds that perk; dying resets it.
// INTERPRETATION: "not downed during the round" (a player revived before the
// round ends does not count that round) -- C1's wording is "survive N
// consecutive rounds", and a down is the one thing the perk above forgives.
//
// Past the largest cost the rewards roll over the way the multiplayer
// killstreaks do (_class.gsc "defcon rollover": streakVal + maxVal*n), so a
// 3/6 character is rewarded at 3, 6, 9, 12, ...
//
// HOW IT IS DELIVERED.  The multiplayer killstreak HUD, which the zombies
// chain must not compile (_killstreaks.gsc names multiplayer modules), is
// reproduced here with the same native builtins:
//   setKillstreak( count )            the streak progress
//   setKillstreakID( row, hud )       coKillstreakStatsTable row id preview
//   setKillstreakTarget( cost, hud )  the preview's target
//   setActionSlot( n, "weapon", w )   the reward's activation weapon
// Perk pop a is HUD entry 1 / action slot 2, perk pop b HUD entry 2 / action
// slot 3 -- the layout the multiplayer tree gives the first two selected
// rewards (_class.gsc setKillstreaks: HUD index0 is action slot1, selected
// rewards occupy slots 2-4 / HUD 1-3).  The activation weapon is column 13 of
// the row, the perkpop_*_zm item itself (MEASURED, codol_checkweapon:
// inventory 2 = item, clip 1, fire time 2000 ms, view model codo_perk_can_ads).
//
// HOW IT IS USED.  C1 switches to the action slot's weapon client-side; the
// server sees the native "weapon_change" to that item, exactly as
// _killstreaks.gsc killstreakUseWaiter does for a multiplayer reward.  The
// drink then takes the item's 2000 ms fire time; the effect is applied when
// it completes, one charge is spent, and the player is switched back.  Leaving
// the item early, going down or dying cancels the drink without spending it.
//
// WHAT EACH ONE DOES.  The zombies perk rows of mp/coPerkStatsTable.csv
// (43-57, specialty_zombie_*) are C1's own definition of every effect; their
// modifier values are aggregated the way CODOL_BuildPerkEffects does (base
// column 3 of the modifier row plus the perk's delta).  Those rows are
// "no_slot" perks, which the native setPerk cannot hold (a C1 perk occupies
// one of eight slots), so the modifiers are set directly with the registered
// setAbilityModifier / setAbilityModifierValue, which ARE the native effect
// path: modSprintTime is consumed by codol_sprint_state.c, modReloadSpeedScale
// and modRateOfFire by codol_weapon_state.c weapon_frame_msec.  Effects that
// are gameplay rules rather than movement/weapon timing are script rules here.
//
//   deadshot          row 49  modBulletSpreadScale 1-0.35 = 0.65, modADSNoViewBob
//                             (both client-published; see the report on
//                             server-side spread)
//   longersprint      row 47  modSprintTime 4+6 = 10 s (native), modViewBobScale
//                             0.75, modFastMantle, move speed 1+0.1 = 1.1
//   flakjacket        row 48  explosive and fall damage immunity (C1 text:
//                             免疫爆炸及摔落伤害) and the dive-landing explosion
//                             (从上往下飞扑后会造成小范围爆炸), T5
//                             divetonuke_explode radius/damage
//   additionalprimary row 50  modAdditionalPrimary: a third primary
//   discount          row 56  modDiscount 0.1: 10% of every purchase back
//   scavenger         row 57  modScavenger: ammo from every robot zombie the
//                             player kills
//   electro           row 55  modElectro 30: 30% chance to electrocute a robot
//                             zombie that hits you; powers nearby perk machines
//   carpenter         row 54  modCarpenter: faster window repair, and a C4
//                             charge behind every window the player finishes
//
// Effects last until the player goes down or dies -- the T5 perk lifetime
// (_zombiemode_perks::perk_think) -- because the reward texts above name no
// duration.  The store descriptions (SF_UI_PERKPOP_*_ZM_DESC) say "briefly"
// for the consumable versions; those are marketplace items, not these.
//
// perkpop_disguise_zm (row 61) and perkpop_pandora_zm (row 68) are on no
// roster row, so the streak can never award them; they are not implemented
// here (see the report).

#include maps\mp\gametypes\_zombiemode_utility;

PERKPOP_TABLE = "mp/coKillstreakStatsTable.csv";

// --------------------------------------------------------------------------
// Callback_StartGameType, inside the precache window, after
// _zombiemode_equipment::init() has read the roster.
// --------------------------------------------------------------------------

init()
{
	level.zombie_perkpop_info = [];

	// Every perk pop any roster character carries.  Only those are precached:
	// each precacheItem spends one entry of the finite weapon registry.
	names = [];
	if ( isDefined( level.zombie_roster_order ) )
	{
		for ( i = 0; i < level.zombie_roster_order.size; i++ )
		{
			info = level.zombie_roster[ level.zombie_roster_order[ i ] ];
			if ( !isDefined( info ) || !isDefined( info.perkpops ) )
				continue;
			for ( j = 0; j < info.perkpops.size; j++ )
				names[ names.size ] = info.perkpops[ j ];
		}
	}

	for ( i = 0; i < names.size; i++ )
	{
		name = names[ i ];
		if ( isDefined( level.zombie_perkpop_info[ name ] ) )
			continue;

		pop = spawnStruct();
		pop.name = name;
		pop.id = int( tableLookup( PERKPOP_TABLE, 1, name, 0 ) );
		pop.cost = int( tableLookup( PERKPOP_TABLE, 1, name, 4 ) );
		pop.weapon = tableLookup( PERKPOP_TABLE, 1, name, 13 );
		pop.effect = effect_key( name );
		pop.usable = false;

		if ( isDefined( pop.weapon ) && pop.weapon != "" )
		{
			precacheItem( pop.weapon );
			if ( getWeaponIndexFromName( pop.weapon ) > 0 )
				pop.usable = true;
		}

		if ( pop.id <= 0 || pop.cost <= 0 )
			pop.usable = false;
		if ( !isDefined( pop.effect ) )
			pop.usable = false;

		level.zombie_perkpop_info[ name ] = pop;
		codolprintln( "CoDOL zm perkpop: " + name + " row " + pop.id + " streak " + pop.cost +
			" weapon " + pop.weapon + " effect " + describe( pop.effect ) + " usable " + pop.usable );
	}

	precacheString( &"ZOMBIE_ROUNDSTREAK" );

	level._effect[ "perkpop_divetonuke" ] = loadfx( "maps/zombie/fx_zmb_phdflopper_exp" );
	level._effect[ "perkpop_electro" ] = loadfx( "maps/zombie/fx_zombie_tesla_shock" );

	add_sound( "perkpop_open", "evt_perk_bottle_open" );
	add_sound( "perkpop_swallow", "evt_perk_swallow" );
	add_sound( "perkpop_deny", "evt_perk_deny" );

	// The equipment test hook reaches this module through a pointer so the
	// shared hook never names a zombies-only file.
	level.codolEquipTestPerkpop = ::test_verb;

	level thread on_player_connect();
	level thread round_start_watch();
	level thread round_end_watch();
}

describe( value )
{
	if ( !isDefined( value ) )
		return "-";
	return value;
}

// The effect a perk pop's row stands for.  Rows 61 (disguise) and 68
// (pandora) are on no roster and have no effect here.
effect_key( name )
{
	switch ( name )
	{
	case "perkpop_deadshot_zm":
		return "deadshot";
	case "perkpop_longersprint_zm":
		return "longersprint";
	case "perkpop_flakjacket_zm":
		return "flakjacket";
	case "perkpop_additionalprimary_zm":
		return "additionalprimary";
	case "perkpop_discount_zm":
		return "discount";
	case "perkpop_scavenger_zm":
		return "scavenger";
	case "perkpop_electro_zm":
		return "electro";
	case "perkpop_carpenter_zm":
		return "carpenter";
	}
	return undefined;
}

// --------------------------------------------------------------------------
// Per-player state and watchers.
// --------------------------------------------------------------------------

on_player_connect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player init_player();
	}
}

// Either entry point can come first: _zombiemode's Callback_PlayerConnect
// threads spawnPlayer straight after its "connected" notify.
init_player()
{
	if ( isDefined( self.perkpop_slots ) )
		return;

	self.perkpop_streak = 0;
	self.perkpop_downed_this_round = false;
	self.perkpop_active = [];
	self.perkpop_busy = false;
	self.perkpop_slots = [];

	self thread watch_down();
	self thread watch_death();
	self thread watch_weapon_change();
	self thread watch_kills();
	self thread watch_spent_points();
	self thread watch_action_slots();
}

// self = player.  Called from _zombiemode::giveStartingLoadout after the
// character's kit is armed, i.e. on every spawn.  The native spawn clears the
// killstreak HUD and the action slots, and takeAllWeapons took the items.
on_spawned()
{
	self init_player();

	names = self.zombie_perkpop_names;
	if ( !isDefined( names ) )
		names = [];

	// Rebuild the two slots when the character changed; keep owned charges
	// otherwise (an earned reward survives death, as in multiplayer).
	for ( i = 0; i < 2; i++ )
	{
		name = undefined;
		if ( i < names.size )
			name = names[ i ];

		slot = self.perkpop_slots[ i ];
		if ( isDefined( slot ) && isDefined( name ) && slot.name == name )
			continue;

		slot = undefined;
		if ( isDefined( name ) && isDefined( level.zombie_perkpop_info[ name ] ) &&
			level.zombie_perkpop_info[ name ].usable )
		{
			slot = spawnStruct();
			slot.name = name;
			slot.pop = level.zombie_perkpop_info[ name ];
			slot.count = 0;
		}
		self.perkpop_slots[ i ] = slot;
	}

	self publish_hud();

	for ( i = 0; i < 2; i++ )
		self publish_slot( i );
}

// The largest streak cost among this player's perk pops: the rollover period.
max_cost()
{
	best = 0;
	for ( i = 0; i < 2; i++ )
	{
		slot = self.perkpop_slots[ i ];
		if ( isDefined( slot ) && slot.pop.cost > best )
			best = slot.pop.cost;
	}
	return best;
}

// The streak position inside the current rollover cycle, which is what the
// HUD previews compare against their targets.
display_streak()
{
	s = self.perkpop_streak;
	period = self max_cost();
	if ( period > 0 && s > period )
		s = ( ( s - 1 ) % period ) + 1;
	return s;
}

publish_hud()
{
	self setKillstreak( int( self display_streak() ) );

	// HUD index 0 belongs to action slot 1, which zombies does not use.
	self setKillstreakID( 0, 0 );
	self setKillstreakTarget( 0, 0 );

	for ( i = 0; i < 2; i++ )
	{
		slot = self.perkpop_slots[ i ];
		if ( !isDefined( slot ) )
		{
			self setKillstreakID( 0, i + 1 );
			self setKillstreakTarget( 0, i + 1 );
			continue;
		}
		target = slot.pop.cost;
		if ( target > 25 )
			target = 25;
		self setKillstreakID( int( slot.pop.id ), i + 1 );
		self setKillstreakTarget( int( target ), i + 1 );
	}
}

// Give the item and publish its action slot while a charge is owned.
publish_slot( index )
{
	slot = self.perkpop_slots[ index ];
	action = index + 2;

	if ( !isDefined( slot ) || slot.count <= 0 )
	{
		self setActionSlot( action, "" );
		return;
	}

	weapon = slot.pop.weapon;
	if ( !self hasWeapon( weapon ) )
	{
		self giveWeapon( weapon );
		self setWeaponAmmoClip( weapon, 1 );
	}

	// The registry is finite: a weapon can resolve and still fail to be
	// granted.  Never publish an action for an item the player does not hold.
	if ( !self hasWeapon( weapon ) )
	{
		codolprintln( "CoDOL zm perkpop: " + self.name + " could not be given " + weapon +
			"; action slot " + action + " left empty, charge kept" );
		self setActionSlot( action, "" );
		return;
	}

	self setActionSlot( action, "weapon", weapon );
}

slot_for_weapon( weapon )
{
	if ( !isDefined( weapon ) || !isDefined( self.perkpop_slots ) )
		return -1;
	for ( i = 0; i < 2; i++ )
	{
		slot = self.perkpop_slots[ i ];
		if ( isDefined( slot ) && slot.pop.weapon == weapon )
			return i;
	}
	return -1;
}

// --------------------------------------------------------------------------
// THE STREAK.
// --------------------------------------------------------------------------

round_start_watch()
{
	level endon( "end_game" );
	for ( ;; )
	{
		level waittill( "start_of_round" );
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
			players[ i ].perkpop_downed_this_round = false;
	}
}

round_end_watch()
{
	level endon( "end_game" );
	for ( ;; )
	{
		level waittill( "end_of_round" );
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
			players[ i ] survived_round();
	}
}

// self = player, at the end of a round.
survived_round()
{
	if ( !isDefined( self.perkpop_slots ) )
		return;

	if ( !is_player_valid( self ) || is_true( self.perkpop_downed_this_round ) )
	{
		self.perkpop_downed_this_round = false;
		return;
	}

	self.perkpop_streak++;
	codolprintln( "CoDOL zm perkpop: " + self.name + " survived a round; streak " + self.perkpop_streak );

	period = self max_cost();
	for ( i = 0; i < 2; i++ )
	{
		slot = self.perkpop_slots[ i ];
		if ( !isDefined( slot ) || period <= 0 )
			continue;
		cost = slot.pop.cost;
		if ( self.perkpop_streak < cost || ( self.perkpop_streak - cost ) % period != 0 )
			continue;

		slot.count++;
		self publish_slot( i );
		self iPrintLnBold( &"ZOMBIE_ROUNDSTREAK", self.perkpop_streak );
		codolprintln( "CoDOL zm perkpop: " + self.name + " EARNED " + slot.name + " at streak " +
			self.perkpop_streak + " (action slot " + ( i + 2 ) + ", charges " + slot.count + ")" );
	}

	self publish_hud();
}

// T5's perk lifetime: every drunk effect ends on a down.  The streak ends too,
// unless the player carries C1's streak-keeping perk.
watch_down()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "player_downed" );
		self.perkpop_downed_this_round = true;
		self remove_effects( "downed" );

		if ( self keeps_streak() )
		{
			codolprintln( "CoDOL zm perkpop: " + self.name + " downed; streak " + self.perkpop_streak +
				" kept (modLastStandKeepRoundStreak)" );
			continue;
		}
		self reset_streak( "downed" );
	}
}

watch_death()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "death" );
		self remove_effects( "died" );
		self reset_streak( "died" );
	}
}

// specialty_perkpop_zm is a perk1 row, so the native perk slots can hold it;
// its modifier is read from the native ability state the perk publishes.
keeps_streak()
{
	if ( self hasPerk( "specialty_perkpop_zm" ) )
		return true;
	if ( self hasAbilityModifier( "modLastStandKeepRoundStreak" ) )
		return true;
	return false;
}

reset_streak( reason )
{
	if ( !isDefined( self.perkpop_streak ) || self.perkpop_streak == 0 )
		return;
	codolprintln( "CoDOL zm perkpop: " + self.name + " " + reason + "; streak " + self.perkpop_streak + " reset" );
	self.perkpop_streak = 0;
	if ( isDefined( self.perkpop_slots ) )
		self publish_hud();
}

// --------------------------------------------------------------------------
// USING ONE.  _killstreaks.gsc killstreakUseWaiter / codolRewardActionWaiter,
// reduced to two fixed slots.
// --------------------------------------------------------------------------

watch_weapon_change()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "weapon_change", weapon );
		if ( !isDefined( weapon ) )
			continue;

		index = self slot_for_weapon( weapon );
		if ( index < 0 )
		{
			if ( weapon != "none" )
				self.perkpop_last_weapon = weapon;
			continue;
		}
		self thread drink( index, weapon );
	}
}

// Selecting the weapon already held raises no weapon_change; the action
// slot's own command does (C1 action ids 19/21, codol_script_notify.c).
watch_action_slots()
{
	self endon( "disconnect" );
	self notifyOnPlayerCommand( "perkpop_slot_2", "+actionslot 2" );
	self notifyOnPlayerCommand( "perkpop_slot_3", "+actionslot 3" );
	self thread watch_action_slot( 0, "perkpop_slot_2" );
	self thread watch_action_slot( 1, "perkpop_slot_3" );
}

watch_action_slot( index, event )
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( event );
		slot = self.perkpop_slots[ index ];
		if ( !isDefined( slot ) || slot.count <= 0 )
			continue;
		if ( self getCurrentWeapon() == slot.pop.weapon )
			self thread drink( index, slot.pop.weapon );
	}
}

// forced: test verb only -- skips the held-weapon check, which a test client
// cannot satisfy (it never raises a weapon; see test_verb "drink").
drink( index, weapon, forced )
{
	self endon( "disconnect" );

	if ( is_true( self.perkpop_busy ) )
		return;

	slot = self.perkpop_slots[ index ];
	if ( !isDefined( slot ) || slot.count <= 0 || !is_player_valid( self ) )
	{
		self switch_back( weapon );
		return;
	}

	key = slot.pop.effect;
	if ( is_true( self.perkpop_active[ key ] ) )
	{
		// Already under this effect: refuse without spending the charge.
		self play_sound_on_ent( "perkpop_deny" );
		codolprintln( "CoDOL zm perkpop: " + self.name + " already has " + key + "; " + slot.name + " kept" );
		self switch_back( weapon );
		return;
	}

	self.perkpop_busy = true;
	codolprintln( "CoDOL zm perkpop: " + self.name + " drinks " + slot.name );
	self play_sound_on_ent( "perkpop_open" );

	// The item's fire time (2000 ms, codol_checkweapon) is the drink.
	finished = true;
	for ( t = 0; t < 40; t++ )
	{
		wait( 0.05 );
		if ( !is_player_valid( self ) || ( !is_true( forced ) && self getCurrentWeapon() != weapon ) )
		{
			finished = false;
			break;
		}
	}

	if ( !finished )
	{
		self.perkpop_busy = false;
		codolprintln( "CoDOL zm perkpop: " + self.name + " stopped drinking " + slot.name + "; charge kept" );
		return;
	}

	slot.count--;
	self play_sound_on_ent( "perkpop_swallow" );
	self apply_effect( key );

	self switch_back( weapon );
	if ( slot.count <= 0 )
	{
		self takeWeapon( weapon );
		self setActionSlot( index + 2, "" );
	}

	self.perkpop_busy = false;
}

switch_back( weapon )
{
	if ( self getCurrentWeapon() != weapon )
		return;

	back = self.perkpop_last_weapon;
	if ( isDefined( back ) && back != weapon && self hasWeapon( back ) )
	{
		self switchToWeapon( back );
		return;
	}

	primaries = self getWeaponsListPrimaries();
	for ( i = 0; i < primaries.size; i++ )
	{
		if ( self slot_for_weapon( primaries[ i ] ) >= 0 )
			continue;
		self switchToWeapon( primaries[ i ] );
		return;
	}
}

// --------------------------------------------------------------------------
// THE EFFECTS.
// --------------------------------------------------------------------------

apply_effect( key )
{
	self.perkpop_active[ key ] = true;

	switch ( key )
	{
	case "deadshot":
		self set_modifier( "modBulletSpreadScale", 0.65 );
		self set_modifier( "modADSNoViewBob" );
		break;

	case "longersprint":
		self set_modifier( "modSprintTime", 10 );
		self set_modifier( "modViewBobScale", 0.75 );
		self set_modifier( "modFastMantle" );
		// modMoveSpeedScale 1 + 0.1.  Its native consumer
		// (CODOL_UpdatePerkMoveSpeed) reads the PERK source only, which these
		// no_slot rows cannot enter, so the registered scale is used.
		self setMoveSpeedScale( 1.1 );
		break;

	case "additionalprimary":
		self set_modifier( "modAdditionalPrimary" );
		self.perkpop_extra_primary = true;
		break;

	case "discount":
		self set_modifier( "modDiscount" );
		break;

	case "scavenger":
		self set_modifier( "modScavenger" );
		break;

	case "electro":
		self set_modifier( "modElectro" );
		self thread electro_power_machines();
		break;

	case "carpenter":
		self set_modifier( "modCarpenter" );
		break;

	case "flakjacket":
		// A damage rule, applied in player_damage_override.
		break;
	}

	codolprintln( "CoDOL zm perkpop: " + self.name + " effect " + key + " APPLIED" + effect_detail( key ) );
}

effect_detail( key )
{
	switch ( key )
	{
	case "longersprint":
		return " (sprint modifier set " + self hasAbilityModifier( "modSprintTime" ) + ")";
	case "deadshot":
		return " (spread modifier set " + self hasAbilityModifier( "modBulletSpreadScale" ) + ")";
	case "additionalprimary":
		return " (weapon limit " + self maps\mp\gametypes\_zombiemode_weapons::player_weapon_limit() + ")";
	}
	return "";
}

// A modifier with a value slot takes its value; the others are flags.
set_modifier( name, value )
{
	self setAbilityModifier( name );
	if ( isDefined( value ) )
		self setAbilityModifierValue( name, value );

	if ( !isDefined( self.perkpop_modifiers ) )
		self.perkpop_modifiers = [];
	self.perkpop_modifiers[ self.perkpop_modifiers.size ] = name;
}

remove_effects( reason )
{
	if ( !isDefined( self.perkpop_active ) )
		return;

	keys = getArrayKeys( self.perkpop_active );
	for ( i = 0; i < keys.size; i++ )
	{
		if ( !is_true( self.perkpop_active[ keys[ i ] ] ) )
			continue;
		codolprintln( "CoDOL zm perkpop: " + self.name + " effect " + keys[ i ] + " EXPIRED (" + reason + ")" );
	}

	// T5 take_additionalprimaryweapon (_zombiemode.gsc:2107): losing the
	// perk loses the third primary.  Last stand has already stored the
	// primaries it will hand back on revive, so the third is dropped there.
	if ( is_true( self.perkpop_active[ "additionalprimary" ] ) && isDefined( self.laststand_weapons ) &&
		self.laststand_weapons.size >= 3 )
	{
		kept = [];
		for ( i = 0; i < self.laststand_weapons.size - 1; i++ )
			kept[ kept.size ] = self.laststand_weapons[ i ];
		codolprintln( "CoDOL zm perkpop: " + self.name + " loses third primary " +
			self.laststand_weapons[ self.laststand_weapons.size - 1 ] );
		self.laststand_weapons = kept;
	}

	// Script-set modifiers are not cleared by the native spawn, so they are
	// unset on death as well as on a down.
	if ( isDefined( self.perkpop_modifiers ) )
	{
		for ( i = 0; i < self.perkpop_modifiers.size; i++ )
			self unsetAbilityModifier( self.perkpop_modifiers[ i ] );
	}

	self.perkpop_modifiers = [];
	self.perkpop_active = [];
	self.perkpop_extra_primary = undefined;
}

// Called from _zombiemode::Callback_PlayerDamage.  Returns the damage to
// apply; 0 means none.
player_damage_override( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon )
{
	mod = "";
	if ( isDefined( sMeansOfDeath ) )
		mod = sMeansOfDeath;

	// A perk-pop blast never hurts the owner's teammates.
	if ( isDefined( eAttacker ) && isPlayer( eAttacker ) && eAttacker != self && is_true( eAttacker.perkpop_blasting ) )
		return 0;

	if ( !isDefined( self.perkpop_active ) )
		return iDamage;

	if ( is_true( self.perkpop_active[ "flakjacket" ] ) )
	{
		if ( mod == "MOD_FALLING" )
		{
			// The dive forces prone from launch until 100 ms after landing
			// (codol_dive.c), so a fall hit while prone is a dive landing.
			if ( self getStance() == "prone" )
				self thread divetonuke_explode();
			codolprintln( "CoDOL zm perkpop: " + self.name + " flakjacket absorbed " + iDamage + " " + mod );
			return 0;
		}
		if ( is_explosive_mod( mod ) )
		{
			codolprintln( "CoDOL zm perkpop: " + self.name + " flakjacket absorbed " + iDamage + " " + mod );
			return 0;
		}
	}

	if ( is_true( self.perkpop_active[ "electro" ] ) && isDefined( eAttacker ) && is_true( eAttacker.is_zombie ) )
	{
		// modElectro 30 (coPerkStatsTable row 55): the chance, in percent.
		if ( randomInt( 100 ) < 30 )
			self thread electro_zap( eAttacker );
	}

	return iDamage;
}

is_explosive_mod( mod )
{
	switch ( mod )
	{
	case "MOD_GRENADE":
	case "MOD_GRENADE_SPLASH":
	case "MOD_PROJECTILE":
	case "MOD_PROJECTILE_SPLASH":
	case "MOD_EXPLOSIVE":
		return true;
	}
	return false;
}

// T5 _zombiemode_perks.gsc:963 divetonuke_explode, with its tuning
// (zombie_perk_divetonuke_radius 300, min 1000, max 5000).
divetonuke_explode()
{
	self endon( "disconnect" );
	origin = self.origin;

	self.perkpop_blasting = true;
	radiusDamage( origin, 300, 5000, 1000, self, "MOD_GRENADE_SPLASH" );
	self.perkpop_blasting = undefined;

	playFx( level._effect[ "perkpop_divetonuke" ], origin );
	codolprintln( "CoDOL zm perkpop: " + self.name + " flakjacket dive explosion at " + origin );
}

// KILLSTREAKS_ELECTRO_DESC: "有一定的概率电击击中你的机械僵尸" -- a chance to
// electrocute the robot zombie that hit you.
electro_zap( zombie )
{
	if ( !isDefined( zombie ) || !isDefined( zombie.health ) || zombie.health <= 0 )
		return;

	origin = zombie.origin;
	health = zombie.health;
	playFx( level._effect[ "perkpop_electro" ], origin + ( 0, 0, 40 ) );
	playSoundAtPosition( "wpn_tesla_bounce", origin );
	// The source is the player, never the zombie's own origin: a zero damage
	// direction reaches G_Damage as NULL and crashes the server when an
	// attacker is given (MEASURED 2026-09-24: CODOL_Scr_DoDamage ->
	// G_Damage_60 -> Vec3NormalizeTo, read of 0x0).
	zombie doDamage( health + 666, self.origin, self );
	codolprintln( "CoDOL zm perkpop: " + self.name + " electro shocked a robot zombie at " + origin );
}

// KILLSTREAKS_ELECTRO_DESC: "激活附近的饮料机" -- powers the drink machines
// near the player.  A machine waits for level "<perk>_power_on"
// (_zombiemode_perks::vending_trigger_think); with the power already on
// there is nothing to do.  The 768-unit radius is an interpretation.
electro_power_machines()
{
	if ( flag( "power_on" ) )
		return;

	machines = getEntArray( "zombie_vending", "targetname" );
	for ( i = 0; i < machines.size; i++ )
	{
		m = machines[ i ];
		if ( !isDefined( m ) || !isDefined( m.script_noteworthy ) || is_true( m.perkpop_powered ) )
			continue;
		if ( distanceSquared( m.origin, self.origin ) > 768 * 768 )
			continue;
		m.perkpop_powered = true;
		level notify( m.script_noteworthy + "_power_on" );
		codolprintln( "CoDOL zm perkpop: " + self.name + " electro powered the " + m.script_noteworthy + " machine" );
	}
}

// KILLSTREAKS_SCAVENGER_DESC: ammo from dead robot zombies.  CoDOL's bag
// (scavenger_bag_mp) ships only in common_gametype_mp, so there is no bag to
// drop in zombies; the resupply one bag gives in multiplayer
// (_weapons.gsc handleScavengerBagPickup: one clip of stock per primary) is
// given for each robot zombie the player kills.
watch_kills()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "zom_kill", zombie );
		if ( !isDefined( self.perkpop_active ) || !is_true( self.perkpop_active[ "scavenger" ] ) )
			continue;
		self scavenger_resupply();
	}
}

scavenger_resupply()
{
	primaries = self getWeaponsListPrimaries();
	given = 0;
	for ( i = 0; i < primaries.size; i++ )
	{
		weapon = primaries[ i ];
		if ( self slot_for_weapon( weapon ) >= 0 )
			continue;
		stock = self getWeaponAmmoStock( weapon );
		room = weaponMaxAmmo( weapon ) - stock;
		add = weaponClipSize( weapon );
		if ( add > room )
			add = room;
		if ( add <= 0 )
			continue;
		self setWeaponAmmoStock( weapon, stock + add );
		given += add;
	}

	if ( !isDefined( self.perkpop_scavenges ) )
		self.perkpop_scavenges = 0;
	self.perkpop_scavenges++;
	if ( self.perkpop_scavenges == 1 || self.perkpop_scavenges % 10 == 0 )
		codolprintln( "CoDOL zm perkpop: " + self.name + " scavenger resupply #" + self.perkpop_scavenges +
			" gave " + given + " rounds" );
}

// KILLSTREAKS_DISCOUNT_DESC: "返还10%的购买金额" -- 10% of every purchase back
// (modDiscount 0.1, coPerkStatsTable row 56).  Every
// _zombiemode_score::minus_to_player_score caller is a purchase.  The refund
// does not credit the lifetime total, like T5's magic-box refund.
watch_spent_points()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "zm_spent_points", points );
		if ( !isDefined( points ) || !isDefined( self.perkpop_active ) || !is_true( self.perkpop_active[ "discount" ] ) )
			continue;
		refund = int( points * 0.1 );
		if ( refund <= 0 )
			continue;
		self maps\mp\gametypes\_zombiemode_score::add_to_player_score( refund, false );
		codolprintln( "CoDOL zm perkpop: " + self.name + " discount refunded " + refund + " of " + points );
	}
}

// KILLSTREAKS_CARPENTER_DESC: "大幅提升修理窗户的速度" -- window repair greatly
// faster.  Called by _zombiemode_blockers for each board; the factor is an
// interpretation of "greatly" (大幅).
board_repair_delay( delay )
{
	if ( isDefined( self.perkpop_active ) && is_true( self.perkpop_active[ "carpenter" ] ) )
		return delay * 0.25;
	return delay;
}

// KILLSTREAKS_CARPENTER_DESC: "修理完窗户后自动在窗户掩体后安放一包C4炸药" --
// after the window is repaired, a C4 charge is placed behind its barricade.
// Called by _zombiemode_blockers when this player finishes a window.
barrier_repaired( blocker )
{
	if ( !isDefined( blocker ) || !isDefined( self.perkpop_active ) || !is_true( self.perkpop_active[ "carpenter" ] ) )
		return;
	if ( isDefined( blocker.perkpop_c4 ) )
		return;

	// Behind the barricade is the player's side of it.
	to_player = self.origin - blocker.origin;
	flat = vectorNormalize( ( to_player[ 0 ], to_player[ 1 ], 0 ) );
	spot = groundpos( blocker.origin + vector_scale( flat, 24 ) + ( 0, 0, 32 ) );

	c4 = spawn( "script_model", spot );
	model = getWeaponModel( "semtex_grenade_zm" );
	if ( isDefined( model ) && model != "" )
		c4 setModel( model );
	blocker.perkpop_c4 = c4;
	codolprintln( "CoDOL zm perkpop: " + self.name + " carpenter planted C4 at " + spot );

	c4 thread carpenter_c4_think( self, blocker );
}

carpenter_c4_think( owner, blocker )
{
	level endon( "end_game" );

	for ( ;; )
	{
		wait( 0.25 );
		if ( !isDefined( self ) )
			return;

		hit = false;
		if ( isDefined( level.zombies ) )
		{
			for ( i = 0; i < level.zombies.size; i++ )
			{
				z = level.zombies[ i ];
				if ( !isDefined( z ) )
					continue;
				if ( distanceSquared( z.origin, self.origin ) < 96 * 96 )
				{
					hit = true;
					break;
				}
			}
		}
		if ( !hit )
			continue;

		origin = self.origin;
		if ( isDefined( owner ) && isPlayer( owner ) )
		{
			owner.perkpop_blasting = true;
			radiusDamage( origin, 192, 3000, 500, owner, "MOD_EXPLOSIVE" );
			owner.perkpop_blasting = undefined;
		}
		else
			radiusDamage( origin, 192, 3000, 500 );

		playFx( level._effect[ "perkpop_divetonuke" ], origin );
		codolprintln( "CoDOL zm perkpop: carpenter C4 detonated at " + origin );

		if ( isDefined( blocker ) )
			blocker.perkpop_c4 = undefined;
		self delete();
		return;
	}
}

// --------------------------------------------------------------------------
// Test verbs, reached from _codol_equiptest through level.codolEquipTestPerkpop:
//   set scr_codol_equiptest "perkpop <client> character <zmN>" another roster row's pops
//   set scr_codol_equiptest "perkpop <client> rounds <n>"   end n rounds for him
//   set scr_codol_equiptest "perkpop <client> use <0|1>"    switch to the item (real path)
//   set scr_codol_equiptest "perkpop <client> drink <0|1>"  drink, held-weapon check skipped
//   set scr_codol_equiptest "perkpop <client> select <weapon>" switch + log current weapon
//   set scr_codol_equiptest "perkpop <client> down"         last stand, revive 3 s later
//   set scr_codol_equiptest "perkpop <client> keepperk"     give specialty_perkpop_zm
//   set scr_codol_equiptest "perkpop <client> hit <mod>"    damage him with <mod>
//   set scr_codol_equiptest "perkpop <client> dive"         the dive-landing explosion
//   set scr_codol_equiptest "perkpop <client> zombiehit"    a robot zombie hits him
//   set scr_codol_equiptest "perkpop <client> zkill"        he kills a robot zombie
//   set scr_codol_equiptest "perkpop <client> drain|ammo"   empty / log primary stock
//   set scr_codol_equiptest "perkpop <client> buy <points>" a purchase
//   set scr_codol_equiptest "perkpop <client> repair"       he finishes a window
//   set scr_codol_equiptest "perkpop <client> state"        log his state
// --------------------------------------------------------------------------

test_verb( player, tokens )
{
	if ( tokens.size < 3 || !isDefined( player ) )
		return;
	verb = tokens[ 2 ];

	switch ( verb )
	{
	case "character":
		// A test client has no profile, so it always takes roster row 0;
		// this gives it another character's perk pops.
		if ( tokens.size < 4 || !isDefined( level.zombie_roster[ tokens[ 3 ] ] ) )
		{
			codolprintln( "CoDOL zm perkpop test: no roster character " + tokens[ tokens.size - 1 ] );
			break;
		}
		player.zombie_perkpop_names = level.zombie_roster[ tokens[ 3 ] ].perkpops;
		player on_spawned();
		codolprintln( "CoDOL zm perkpop test: " + player.name + " now carries " + tokens[ 3 ] + "'s perk pops" );
		break;

	case "rounds":
		n = 1;
		if ( tokens.size > 3 )
			n = int( tokens[ 3 ] );
		for ( i = 0; i < n; i++ )
		{
			player survived_round();
			wait( 0.2 );
		}
		break;

	case "use":
		index = 0;
		if ( tokens.size > 3 )
			index = int( tokens[ 3 ] );
		slot = player.perkpop_slots[ index ];
		if ( !isDefined( slot ) )
		{
			codolprintln( "CoDOL zm perkpop test: " + player.name + " has no perk pop in slot " + index );
			break;
		}
		codolprintln( "CoDOL zm perkpop test: " + player.name + " selects " + slot.pop.weapon +
			" (has " + player hasWeapon( slot.pop.weapon ) + ", charges " + slot.count + ", current " +
			player getCurrentWeapon() + ")" );
		player switchToWeapon( slot.pop.weapon );
		// A test client that is not a combat bot builds usercmds only while
		// a scripted hold is active (codol_bot.c CODOL_BotBuildCommand), and
		// the command is what carries the requested weapon into pmove.  Lean
		// (0x40) is held because it changes nothing the drink depends on.
		if ( is_true( player.isbot ) )
			player codol_botPressButtons( 64, 4 );
		player thread test_watch_weapon( 4 );
		break;

	case "drink":
		// The real drink path with the held-weapon check skipped: a test
		// client never raises any weapon (not even its pistol), so the
		// weapon_change that starts a drink cannot happen on a probe.
		index = 0;
		if ( tokens.size > 3 )
			index = int( tokens[ 3 ] );
		slot = player.perkpop_slots[ index ];
		if ( !isDefined( slot ) )
			break;
		player drink( index, slot.pop.weapon, true );
		codolprintln( "CoDOL zm perkpop test: " + player.name + " after drink: " + slot.name + " charges " + slot.count +
			", item held " + player hasWeapon( slot.pop.weapon ) );
		break;

	case "select":
		if ( tokens.size < 4 )
			break;
		codolprintln( "CoDOL zm perkpop test: " + player.name + " select " + tokens[ 3 ] + " (has " +
			player hasWeapon( tokens[ 3 ] ) + ", current " + player getCurrentWeapon() + ")" );
		player switchToWeapon( tokens[ 3 ] );
		if ( is_true( player.isbot ) )
			player codol_botPressButtons( 64, 3 );
		player thread test_watch_weapon( 3 );
		break;

	case "down":
		player maps\mp\gametypes\_zombiemode_laststand::player_laststand();
		wait( 3 );
		if ( isDefined( player ) && player maps\mp\gametypes\_zombiemode_laststand::player_is_in_laststand() )
			player maps\mp\gametypes\_zombiemode_laststand::revive_success();
		break;

	case "hit":
		mod = "MOD_GRENADE_SPLASH";
		if ( tokens.size > 3 )
			mod = tokens[ 3 ];
		if ( mod == "MOD_FALLING" )
		{
			// Prone is what a dive landing leaves; a test client only runs
			// pmove (which applies the stance) while a hold is active.
			player setStance( "prone" );
			if ( is_true( player.isbot ) )
				player codol_botPressButtons( 64, 1 );
			wait( 1.2 );
			codolprintln( "CoDOL zm perkpop test: " + player.name + " stance " + player getStance() );
		}
		result = player player_damage_override( player, player, 50, mod, "none" );
		codolprintln( "CoDOL zm perkpop test: " + player.name + " " + mod + " 50 -> " + result );
		break;

	case "zombiehit":
		z = test_zombie();
		if ( !isDefined( z ) )
			break;
		for ( i = 0; i < 20; i++ )
		{
			if ( !isDefined( z ) || !isDefined( z.health ) || z.health <= 0 )
				break;
			player player_damage_override( z, z, 10, "MOD_MELEE", "none" );
			wait( 0.05 );
		}
		codolprintln( "CoDOL zm perkpop test: zombie hits done (zombie alive " + ( isDefined( z ) && isDefined( z.health ) && z.health > 0 ) + ")" );
		break;

	case "zkill":
		z = test_zombie();
		if ( isDefined( z ) )
			z doDamage( z.health + 100, player.origin, player );
		break;

	case "dive":
		// The dive-landing explosion itself.  A test client's stance never
		// changes (it runs no pmove of its own), so the prone test in
		// player_damage_override cannot be met on a probe.
		player divetonuke_explode();
		break;

	case "drain":
		primaries = player getWeaponsListPrimaries();
		for ( i = 0; i < primaries.size; i++ )
		{
			player setWeaponAmmoStock( primaries[ i ], 0 );
			codolprintln( "CoDOL zm perkpop test: " + player.name + " " + primaries[ i ] + " stock 0 (max " +
				weaponMaxAmmo( primaries[ i ] ) + ", clip " + weaponClipSize( primaries[ i ] ) + ")" );
		}
		break;

	case "ammo":
		primaries = player getWeaponsListPrimaries();
		for ( i = 0; i < primaries.size; i++ )
			codolprintln( "CoDOL zm perkpop test: " + player.name + " " + primaries[ i ] + " stock " +
				player getWeaponAmmoStock( primaries[ i ] ) );
		break;

	case "keepperk":
		// C1's streak-keeping zombies perk (coPerkStatsTable row 63, perk1).
		player setPerk( "specialty_perkpop_zm" );
		codolprintln( "CoDOL zm perkpop test: " + player.name + " specialty_perkpop_zm " + player hasPerk( "specialty_perkpop_zm" ) +
			" modLastStandKeepRoundStreak " + player hasAbilityModifier( "modLastStandKeepRoundStreak" ) );
		break;

	case "buy":
		points = 1000;
		if ( tokens.size > 3 )
			points = int( tokens[ 3 ] );
		before = player.score;
		player maps\mp\gametypes\_zombiemode_score::minus_to_player_score( points );
		codolprintln( "CoDOL zm perkpop test: " + player.name + " bought for " + points + ": score " + before + " -> " + player.score );
		break;

	case "repair":
		barriers = level.exterior_goals;
		if ( !isDefined( barriers ) || barriers.size == 0 )
		{
			codolprintln( "CoDOL zm perkpop test: no exterior_goal on this map" );
			break;
		}
		codolprintln( "CoDOL zm perkpop test: board delay 1 -> " + player board_repair_delay( 1 ) );
		player barrier_repaired( barriers[ 0 ] );
		break;

	case "state":
		keys = getArrayKeys( player.perkpop_active );
		active = "";
		for ( i = 0; i < keys.size; i++ )
			active = active + keys[ i ] + " ";
		charges = "";
		for ( i = 0; i < 2; i++ )
		{
			slot = player.perkpop_slots[ i ];
			if ( isDefined( slot ) )
				charges = charges + slot.name + "=" + slot.count + " ";
		}
		primaries = player getWeaponsListPrimaries();
		codolprintln( "CoDOL zm perkpop test: " + player.name + " streak " + player.perkpop_streak +
			" hud " + player getKillstreak() + " ids " + player getKillstreakID( 1 ) + "/" + player getKillstreakID( 2 ) +
			" targets " + player getKillstreakTarget( 1 ) + "/" + player getKillstreakTarget( 2 ) +
			" charges " + charges + "active " + active + "current " + player getCurrentWeapon() +
			" primaries " + primaries.size + " limit " +
			player maps\mp\gametypes\_zombiemode_weapons::player_weapon_limit() );
		break;
	}
}

test_watch_weapon( seconds )
{
	self endon( "disconnect" );
	last = "";
	for ( t = 0; t < seconds * 10; t++ )
	{
		current = self getCurrentWeapon();
		if ( current != last )
			codolprintln( "CoDOL zm perkpop test: " + self.name + " current weapon " + current + " at +" + ( t * 0.1 ) + "s" );
		last = current;
		wait( 0.1 );
	}
}

test_zombie()
{
	if ( isDefined( level.zombies ) )
	{
		for ( i = 0; i < level.zombies.size; i++ )
		{
			z = level.zombies[ i ];
			if ( isDefined( z ) && isDefined( z.health ) && z.health > 0 )
				return z;
		}
	}
	codolprintln( "CoDOL zm perkpop test: no live robot zombie" );
	return undefined;
}
