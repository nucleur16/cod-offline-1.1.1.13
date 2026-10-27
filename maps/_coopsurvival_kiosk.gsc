// coop survival: the kiosks (armories) and what they sell.
//
// Ported from Infinity Ward's Modern Warfare 3 maps\_so_survival_armory.gsc
// (IW5; D:\CoD\COD-GSC-Source\IW5-GSC\scripts\1574.gsc): an armory is a usable
// world object that opens its popup menu, and armory_user_thread buys from the
// menu's responses ("purchase_<index>", "quit", "share", "weaponswitch_<n>").
// CoDOL replaced IW5's three armories (weapon / equipment / airsupport) with
// two kiosks that its survival addons place: kiosk_locker (coop_evac_case_ammo:
// ammo, loadout, perks, weapons) and kiosk_killstreak (coop_evac_case_support:
// armor and air support), priced by mp/kiosks.csv:
//   0 index  1 ref  2 group (locker / killstreak / weapon / pistols / smgs ...)
//   3 cost   4 name  5 desc  6 icon  7 perk slots  ...  10 amount
// docs/CODOL_SURVIVAL_PLAN.md section 2.4.
//
// The menus are kiosk_locker and kiosk_killstreak (CoDOL 2.6.9.7
// common_gametype_survival, ported to 1.1.1.13 into codol_survival_postgfx_mp).
// Until that zone is installed in the client they stay off (scr_survival_menus 0)
// and a kiosk sells its default item on use: the locker refills ammo and the
// support case sells armor.
//
// Coop module; names no multiplayer module.

init()
{
	level.survival_kiosk_table = "mp/kiosks.csv";
	level.survival_menus = ( getDvarInt( "scr_survival_menus" ) != 0 );
	if ( level.survival_menus )
	{
		precacheMenu( "kiosk_locker" );
		precacheMenu( "kiosk_killstreak" );
		precacheMenu( "surhud_display" );
	}

	// The coop weapons the weapon rows may sell (plan section 3.3).
	names = [];
	names[ names.size ] = "beretta_so";
	names[ names.size ] = "usp_silencer_so";
	names[ names.size ] = "tmp_silencer_so";
	names[ names.size ] = "ump45_so";
	names[ names.size ] = "mp5_silencer_so";
	names[ names.size ] = "aa12_so";
	names[ names.size ] = "m16_basic_so";
	names[ names.size ] = "m16_reflex_so";
	names[ names.size ] = "m14ebr_thermal_so";
	names[ names.size ] = "cheytac_so";
	names[ names.size ] = "barrett_so";
	names[ names.size ] = "m240_so";
	names[ names.size ] = "rpd_so";
	names[ names.size ] = "rpg_so";
	names[ names.size ] = "at4_so";
	names[ names.size ] = "fraggrenade_so";
	names[ names.size ] = "semtex_grenade_so";
	names[ names.size ] = "throwingknife_so";
	names[ names.size ] = "c4_so";
	names[ names.size ] = "claymore_so";
	names[ names.size ] = "flash_grenade_so";
	names[ names.size ] = "smoke_grenade_american_so";
	for ( i = 0; i < names.size; i++ )
		precacheItem( names[ i ] );
}

// --------------------------------------------------------------------------
// The table
// --------------------------------------------------------------------------

item_ref( index )
{
	return tableLookup( level.survival_kiosk_table, 0, "" + index, 1 );
}

item_group( index )
{
	return tableLookup( level.survival_kiosk_table, 0, "" + index, 2 );
}

item_cost( index )
{
	return int( tableLookup( level.survival_kiosk_table, 0, "" + index, 3 ) );
}

item_amount( index )
{
	return int( tableLookup( level.survival_kiosk_table, 0, "" + index, 10 ) );
}

// The first row whose ref and group match (the table prices one ref in
// several lists; the 90..99 rows are the survival kiosk's own prices).
find_item( ref, group )
{
	for ( i = 90; i < 100; i++ )
	{
		if ( item_ref( i ) == ref && ( !isDefined( group ) || item_group( i ) == group ) )
			return i;
	}
	for ( i = 0; i < 400; i++ )
	{
		if ( item_ref( i ) == ref && ( !isDefined( group ) || item_group( i ) == group ) )
			return i;
	}
	return undefined;
}

// mp/kiosks.csv names multiplayer weapons, "<base>_mp__<attachments>"; a coop
// match carries the _so arsenal.
coop_weapon( ref )
{
	base = ref;
	parts = strTok( ref, "_" );
	if ( parts.size )
		base = parts[ 0 ];
	switch ( base )
	{
	case "m9":
	case "m93":
		return "beretta_so";
	case "usp":
	case "deserteagle":
		return "usp_silencer_so";
	case "miniuzi":
		return "tmp_silencer_so";
	case "p90":
		return "ump45_so";
	case "vector":
		return "mp5_silencer_so";
	case "model1887":
	case "spas12":
	case "aa12":
		return "aa12_so";
	case "acr":
	case "m16":
		return "m16_basic_so";
	case "m14ebr":
		return "m14ebr_thermal_so";
	case "rsrt3":
	case "cheytac":
		return "cheytac_so";
	case "m21ebr":
		return "barrett_so";
	case "m240":
		return "m240_so";
	case "rpg":
		return "rpg_so";
	case "at4":
		return "at4_so";
	case "frag":
		return "fraggrenade_so";
	case "semtex":
		return "semtex_grenade_so";
	case "throwingknife":
		return "throwingknife_so";
	case "c4":
		return "c4_so";
	case "claymore":
	case "bouncingbetty":
		return "claymore_so";
	case "flash":
	case "concussion":
	case "emp":
		return "flash_grenade_so";
	case "smoke":
	case "scrambler":
		return "smoke_grenade_american_so";
	}
	return undefined;
}

// --------------------------------------------------------------------------
// The kiosks
// --------------------------------------------------------------------------

main()
{
	level.survival_kiosks = [];
	kinds = [];
	kinds[ 0 ] = "kiosk_locker";
	kinds[ 1 ] = "kiosk_killstreak";
	for ( k = 0; k < kinds.size; k++ )
	{
		ents = getEntArray( kinds[ k ], "targetname" );
		for ( i = 0; i < ents.size; i++ )
		{
			e = ents[ i ];
			e.survival_kind = kinds[ k ];
			level.survival_kiosks[ level.survival_kiosks.size ] = e;
			// IW5 armory_use_monitor's world icon: the compass marks it
			// (objective slots 8.., above the ones the coop layers use).
			objective_add( 7 + level.survival_kiosks.size, "active", e.origin );
			e thread kiosk_think();
		}
	}
	// 3.5.2.6's upgraded and mtx kiosks belong to content this port leaves out.
	hide_list( getEntArray( "kiosk_upgraded", "targetname" ) );
	hide_list( getEntArray( "kiosk_mtx", "targetname" ) );
	codolprintln( "CoDOL survival: " + level.survival_kiosks.size + " kiosks, menus " + level.survival_menus );
	if ( getDvarInt( "scr_coop_test" ) != 0 )
		level thread test_purchases();
}

// Probe harness (only with +set scr_coop_test 1): scr_survival_buy
// "<player> <kiosks.csv index | menu response>" buys as that player would
// from the kiosk menu, so the purchase path is testable without a client.
test_purchases()
{
	setDvar( "scr_survival_buy", "" );
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_survival_buy" );
		if ( request == "" )
			continue;
		setDvar( "scr_survival_buy", "" );
		tokens = strTok( request, " " );
		if ( tokens.size < 2 )
			continue;
		players = getPlayers();
		n = int( tokens[ 0 ] );
		if ( n < 0 || n >= players.size )
			continue;
		p = players[ n ];
		if ( tokens[ 1 ] == "credits" && tokens.size >= 3 )
		{
			p maps\_coopsurvival::add_credits( int( tokens[ 2 ] ), "test" );
			continue;
		}
		ok = p purchase( int( tokens[ 1 ] ) );
		codolprintln( "CoDOL survival test: buy " + tokens[ 1 ] + " as " + p.name + ": " + ok +
			", credits " + p.survival_credit + ", armor " + p.survival_armor );
	}
}

hide_list( ents )
{
	for ( i = 0; i < ents.size; i++ )
		ents[ i ] hide();
}

// IW5 armory_use_monitor: a hint and a use, polled within reach so it works
// on the plain script_model the addon authors.
kiosk_think()
{
	level endon( "coop_mission_over" );
	if ( self.survival_kind == "kiosk_locker" )
		self.survival_hint = "Press ^3[{+activate}]^7 for the armory";
	else
		self.survival_hint = "Press ^3[{+activate}]^7 for air support";
	self makeUsable();
	self setHintString( self.survival_hint );
	for ( ;; )
	{
		players = maps\_coopmission::living_players();
		for ( i = 0; i < players.size; i++ )
		{
			p = players[ i ];
			if ( isDefined( p.survival_in_kiosk ) && p.survival_in_kiosk )
				continue;
			if ( isDefined( p.laststand ) && p.laststand )
				continue;
			if ( distanceSquared( p.origin, self.origin ) > 96 * 96 )
				continue;
			if ( !p useButtonPressed() )
				continue;
			p thread use_kiosk( self );
		}
		wait( 0.1 );
	}
}

use_kiosk( kiosk )
{
	self endon( "disconnect" );
	self endon( "death" );
	self.survival_in_kiosk = true;
	if ( level.survival_menus )
		self menu_session( kiosk );
	else
		self default_purchase( kiosk );
	// the use that opened it must be let go first
	while ( self useButtonPressed() )
		wait( 0.05 );
	self.survival_in_kiosk = false;
}

default_purchase( kiosk )
{
	if ( kiosk.survival_kind == "kiosk_locker" )
		index = find_item( "ammo", "locker" );
	else
		index = find_item( "armor", "killstreak" );
	if ( !isDefined( index ) )
		return;
	self purchase( index );
}

// IW5 armory_user_thread: open the menu, freeze, buy until "quit".
menu_session( kiosk )
{
	self endon( "survival_kiosk_closed" );
	self setClientDvar( "surHUD_credits", self.survival_credit );
	self openPopupMenu( kiosk.survival_kind );
	self freezeControls( true );
	self thread menu_interrupt();
	for ( ;; )
	{
		self waittill( "menuresponse", menu, response );
		if ( menu != kiosk.survival_kind )
			continue;
		if ( response == "quit" || response == "back" )
			break;
		if ( response == "share" )
		{
			share_credits();
			continue;
		}
		if ( isSubStr( response, "purchase_" ) )
		{
			index = int( getSubStr( response, 9, response.size ) );
			self purchase( index );
			self setClientDvar( "surHUD_credits", self.survival_credit );
		}
	}
	close_menu();
}

// IW5 armory_downed_interrupt: going down closes it.
menu_interrupt()
{
	self endon( "survival_kiosk_closed" );
	self endon( "disconnect" );
	self waittill_any2( "player_downed", "death" );
	close_menu();
}

waittill_any2( a, b )
{
	self endon( a );
	self waittill( b );
}

close_menu()
{
	self closePopupMenu();
	self freezeControls( false );
	self.survival_in_kiosk = false;
	self notify( "survival_kiosk_closed" );
}

// IW5 "share": 500 (or what he has) to each teammate.
share_credits()
{
	if ( !isDefined( self.survival_credit ) || self.survival_credit <= 0 )
		return;
	amount = 500;
	if ( self.survival_credit < amount )
		amount = self.survival_credit;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] == self )
			continue;
		if ( !self maps\_coopsurvival::spend_credits( amount ) )
			return;
		players[ i ] maps\_coopsurvival::add_credits( amount, "share" );
	}
}

// --------------------------------------------------------------------------
// Buying
// --------------------------------------------------------------------------

purchase( index )
{
	ref = item_ref( index );
	group = item_group( index );
	cost = item_cost( index );
	if ( !isDefined( ref ) || ref == "" )
		return false;
	if ( !isDefined( self.survival_credit ) )
		self.survival_credit = 0;
	if ( self.survival_credit < cost )
	{
		self iPrintLnBold( "Not enough credits (" + cost + ")" );
		return false;
	}
	if ( !self give_item( ref, group, index ) )
		return false;
	self maps\_coopsurvival::spend_credits( cost );
	codolprintln( "CoDOL survival: " + self.name + " bought " + ref + " (" + group + ") for " + cost +
		", " + self.survival_credit + " left" );
	return true;
}

give_item( ref, group, index )
{
	switch ( ref )
	{
	case "ammo":
	case "ammo_regen":
		return give_ammo();
	case "armor":
	case "team_armor":
		amount = item_amount( index );
		if ( amount <= 0 )
			amount = 100;
		return give_armor( amount );
	case "team_revive":
		return revive_team();
	case "one_perk":
	case "two_perks":
	case "locker":
		self iPrintLnBold( "Choose it in the kiosk menu" );
		return false;
	}
	if ( group == "perks" || getSubStr( ref, 0, 10 ) == "specialty_" )
		return give_perk( ref );
	weapon = coop_weapon( ref );
	if ( isDefined( weapon ) )
		return give_weapon( weapon );
	// The air support and equipment (predator, mortar, sentry, SAM, EMP,
	// airstrike, nuke): phase 5 of the plan.
	self iPrintLnBold( "Not available yet" );
	codolprintln( "CoDOL survival: kiosk item " + ref + " (" + group + ") is not implemented yet" );
	return false;
}

// IW5 give_ammo: every weapon's stock to max.
give_ammo()
{
	weapons = self getWeaponsList();
	for ( i = 0; i < weapons.size; i++ )
		self giveMaxAmmo( weapons[ i ] );
	self iPrintLn( "Ammo refilled" );
	return true;
}

// IW5 give_armor_amount: a pool soaked before health (_coopmode's damage
// callback reads self.survival_armor).
give_armor( amount )
{
	if ( !isDefined( self.survival_armor ) )
		self.survival_armor = 0;
	if ( self.survival_armor >= 250 )
	{
		self iPrintLnBold( "Armor full" );
		return false;
	}
	self.survival_armor += amount;
	if ( self.survival_armor > 250 )
		self.survival_armor = 250;
	self iPrintLn( "Armor " + self.survival_armor );
	return true;
}

revive_team()
{
	n = 0;
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		if ( isAlive( p ) && isDefined( p.laststand ) && p.laststand )
		{
			p common_scripts\_laststand::revive_success( self );
			n++;
		}
	}
	if ( !n )
	{
		self iPrintLnBold( "Nobody is down" );
		return false;
	}
	return true;
}

give_perk( ref )
{
	if ( self hasPerk( ref ) )
	{
		self iPrintLnBold( "Already owned" );
		return false;
	}
	self setPerk( ref );
	self iPrintLn( ref );
	return true;
}

give_weapon( weapon )
{
	if ( !codol_isWeaponMounted( weapon ) )
	{
		self iPrintLnBold( "Not available" );
		return false;
	}
	if ( weaponInventoryType( weapon ) == "offhand" )
	{
		self giveWeapon( weapon );
		self giveMaxAmmo( weapon );
		return true;
	}
	self maps\_coopmode::coop_give_weapon( weapon, true );
	if ( !isDefined( self.survival_bought ) )
		self.survival_bought = [];
	self.survival_bought[ self.survival_bought.size ] = weapon;
	return true;
}
