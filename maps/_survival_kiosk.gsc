// survival: the kiosks (armories) and what they sell.
//
// Ported from Infinity Ward's Modern Warfare 3 maps\_so_survival_armory.gsc
// (IW5; D:\CoD\COD-GSC-Source\IW5-GSC\scripts\1574.gsc): an armory is a usable
// world object that opens its popup menu, and armory_user_thread buys from the
// menu's responses.  CoDOL replaced IW5's three armories with two kiosks its
// survival addons place (2.6.9.7 coop_survival_<map>, now the level's
// maps/<level>_survival.gsc data): kiosk_locker (coop_evac_case_ammo) and
// kiosk_killstreak (coop_evac_case_support), driven by mp/kiosks.csv:
//   0 index  1 ref  2 kiosk  3 tier (the menus show it as the price)
//   4 name  5 desc  6 icon  7 perks needed  ...  10 amount
// docs/CODOL_SURVIVAL_PLAN.md section 2.4.
//
// The menus are CoDOL 2.6.9.7's kiosk_locker / kiosk_killstreak, converted to
// 1.1.1.13 (Kernel33 feat/survival-menus, zonekit/menus.py) and carried by
// common_gametype_survival.  Their contract, from the conversion:
//   responses  kiosk_locker purchase_0..purchase_4 (ammo, locker, armor,
//              one_perk, two_perks); kiosk_killstreak purchase_10, _11, _13,
//              _14 (predator_missile, mortar_mp, sam_turret, sentry); share;
//              quit (sent from onClose: every close sends it);
//   dvars      surHUD_credits (share needs >= 500), playingCardsCurrentBestSet
//              (a row is enabled when it is >= the row's column 3),
//              perksRemainingForPurchase (>= column 7), and
//              "armory" + <locker|killstreak> + <ref> > 0 = owned (disabled).
// 2.6.9.7 priced the rows in tiers of its card economy; this port keeps IW5's
// credits, prices each row at the survival kiosk's credit price for its ref
// (rows 90..99, CoDOL 3.5.2.6) and hands the menu, as
// playingCardsCurrentBestSet, the highest tier the player can afford -- the
// tiers rise with the price, so exactly the affordable rows are enabled.
//
// Without the menus (scr_survival_menus 0) a kiosk sells its default item on
// use: the locker refills ammo and the support case sells armor.
//
// Names no multiplayer and no coop module.

init()
{
	level.survival_kiosk_table = "mp/kiosks.csv";
	level.survival_menus = ( getDvar( "scr_survival_menus" ) == "" || getDvarInt( "scr_survival_menus" ) != 0 );
	if ( level.survival_menus )
	{
		precacheMenu( "kiosk_locker" );
		precacheMenu( "kiosk_killstreak" );
		precacheMenu( "surhud_display" );
	}
	level.survival_kiosk_models = [];
	level.survival_kiosk_models[ "kiosk_locker" ] = "coop_evac_case_ammo";
	level.survival_kiosk_models[ "kiosk_killstreak" ] = "coop_evac_case_support";
	codescripts\character::precacheMounted( "coop_evac_case_ammo" );
	codescripts\character::precacheMounted( "coop_evac_case_support" );

	// The weapons the locker's upgrade ladder hands out (the _so arsenal,
	// plan section 3.3), cheapest first.
	level.survival_ladder = [];
	level.survival_ladder[ level.survival_ladder.size ] = "usp_silencer_so";
	level.survival_ladder[ level.survival_ladder.size ] = "tmp_silencer_so";
	level.survival_ladder[ level.survival_ladder.size ] = "mp5_silencer_so";
	level.survival_ladder[ level.survival_ladder.size ] = "ump45_so";
	level.survival_ladder[ level.survival_ladder.size ] = "aa12_so";
	level.survival_ladder[ level.survival_ladder.size ] = "m16_basic_so";
	level.survival_ladder[ level.survival_ladder.size ] = "m16_reflex_so";
	level.survival_ladder[ level.survival_ladder.size ] = "m14ebr_thermal_so";
	level.survival_ladder[ level.survival_ladder.size ] = "rpd_so";
	level.survival_ladder[ level.survival_ladder.size ] = "m240_so";
	level.survival_ladder[ level.survival_ladder.size ] = "cheytac_so";
	level.survival_ladder[ level.survival_ladder.size ] = "barrett_so";
	for ( i = 0; i < level.survival_ladder.size; i++ )
		precacheItem( level.survival_ladder[ i ] );
	// The mortar air support's shell.
	precacheItem( "mortar_mp" );

	// IW5 _so_survival_perks: the perk pool (mp/survival_perks.csv column 1).
	level.survival_perk_pool = [];
	for ( i = 0; i < 20; i++ )
	{
		perk = tableLookup( "mp/survival_perks.csv", 0, "" + i, 1 );
		if ( isDefined( perk ) && perk != "" )
			level.survival_perk_pool[ level.survival_perk_pool.size ] = perk;
	}
	level.survival_max_perks = 3;
}

// --------------------------------------------------------------------------
// The table
// --------------------------------------------------------------------------

item_ref( index )
{
	return tableLookup( level.survival_kiosk_table, 0, "" + index, 1 );
}

item_tier( index )
{
	return int( tableLookup( level.survival_kiosk_table, 0, "" + index, 3 ) );
}

item_amount( index )
{
	return int( tableLookup( level.survival_kiosk_table, 0, "" + index, 10 ) );
}

// The credit price: the survival kiosk's own row for the ref (90..99), else
// the row's column 3 when it is a credit price (the 60.. and 100.. rows).
item_cost( index )
{
	ref = item_ref( index );
	for ( i = 90; i < 100; i++ )
	{
		if ( item_ref( i ) == ref )
			return item_tier( i );
	}
	return item_tier( index );
}

// The rows a kiosk's menu offers.
menu_rows( kind )
{
	rows = [];
	if ( kind == "kiosk_locker" )
	{
		for ( i = 0; i <= 4; i++ )
			rows[ rows.size ] = i;
	}
	else
	{
		rows[ 0 ] = 10;
		rows[ 1 ] = 11;
		rows[ 2 ] = 13;
		rows[ 3 ] = 14;
	}
	return rows;
}

// --------------------------------------------------------------------------
// The kiosks
// --------------------------------------------------------------------------

main()
{
	level.survival_kiosks = [];
	spots = [];
	if ( isDefined( level.survival_kiosk_spots ) )
		spots = level.survival_kiosk_spots;
	for ( i = 0; i < spots.size; i++ )
	{
		spot = spots[ i ];
		e = spawn( "script_model", spot.origin );
		e.angles = spot.angles;
		model = level.survival_kiosk_models[ spot.kind ];
		if ( codescripts\character::modelMounted( model ) )
			e setModel( model );
		e.survival_kind = spot.kind;
		level.survival_kiosks[ level.survival_kiosks.size ] = e;
		// IW5 armory_use_monitor's world icon: the compass marks it.
		objective_add( 7 + level.survival_kiosks.size, "active", e.origin );
		e thread kiosk_think();
	}
	codolprintln( "CoDOL survival: " + level.survival_kiosks.size + " kiosks, menus " + level.survival_menus );
	if ( getDvarInt( "scr_survival_test" ) != 0 )
		level thread test_purchases();
}

// Probe harness (only with +set scr_survival_test 1): scr_survival_buy
// "<player> <kiosks.csv index>" buys as that player would from a menu;
// "<player> credits <n>" pays him.
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
			p maps\_survival::add_credits( int( tokens[ 2 ] ), "test" );
			continue;
		}
		ok = p purchase( int( tokens[ 1 ] ) );
		codolprintln( "CoDOL survival test: buy " + tokens[ 1 ] + " as " + p.name + ": " + ok +
			", credits " + p.survival_credit + ", armor " + p.survival_armor );
	}
}

// IW5 armory_use_monitor: a hint and a use, polled within reach.
kiosk_think()
{
	level endon( "survival_over" );
	if ( self.survival_kind == "kiosk_locker" )
		self.survival_hint = "Press ^3[{+activate}]^7 for the armory";
	else
		self.survival_hint = "Press ^3[{+activate}]^7 for air support";
	self makeUsable();
	self setHintString( self.survival_hint );
	for ( ;; )
	{
		players = maps\_survival_utility::living_players();
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
	while ( self useButtonPressed() )
		wait( 0.05 );
	self.survival_in_kiosk = false;
}

default_purchase( kiosk )
{
	if ( kiosk.survival_kind == "kiosk_locker" )
		self purchase( 0 );
	else
		self purchase( 2 );
}

// The dvars the kiosk menus read (see the header).
menu_state( kind )
{
	if ( !isDefined( self.survival_credit ) )
		self.survival_credit = 0;
	rows = menu_rows( kind );
	tier = 0;
	group = "locker";
	if ( kind == "kiosk_killstreak" )
		group = "killstreak";
	for ( i = 0; i < rows.size; i++ )
	{
		r = rows[ i ];
		if ( item_cost( r ) <= self.survival_credit && item_tier( r ) > tier )
			tier = item_tier( r );
		owned = 0;
		if ( !self can_buy( item_ref( r ), r ) )
			owned = 1;
		self setClientDvar( "armory" + group + item_ref( r ), owned );
	}
	perks = 0;
	if ( isDefined( self.survival_perks ) )
		perks = self.survival_perks.size;
	self setClientDvars( "surHUD_credits", self.survival_credit,
		"playingCardsCurrentBestSet", tier,
		"perksRemainingForPurchase", level.survival_max_perks - perks );
}

// IW5 armory_user_thread: open the menu, freeze, buy until it closes.
menu_session( kiosk )
{
	self endon( "survival_kiosk_closed" );
	self menu_state( kiosk.survival_kind );
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
			self share_credits();
		else if ( isSubStr( response, "purchase_" ) )
			self purchase( int( getSubStr( response, 9, response.size ) ) );
		self menu_state( kiosk.survival_kind );
	}
	self close_menu();
}

// IW5 armory_downed_interrupt: going down closes it.
menu_interrupt()
{
	self endon( "survival_kiosk_closed" );
	self endon( "disconnect" );
	self waittill_any2( "player_downed", "death" );
	self close_menu();
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
		if ( !self maps\_survival::spend_credits( amount ) )
			return;
		players[ i ] maps\_survival::add_credits( amount, "share" );
	}
}

// --------------------------------------------------------------------------
// Buying
// --------------------------------------------------------------------------

purchase( index )
{
	ref = item_ref( index );
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
	if ( !self can_buy( ref, index ) )
		return false;
	if ( !self give_item( ref, index ) )
		return false;
	self maps\_survival::spend_credits( cost );
	codolprintln( "CoDOL survival: " + self.name + " bought " + ref + " (row " + index + ") for " + cost +
		", " + self.survival_credit + " left" );
	return true;
}

// IW5 get_func_can_give: what is full or owned is not for sale.
can_buy( ref, index )
{
	switch ( ref )
	{
	case "armor":
		return !isDefined( self.survival_armor ) || self.survival_armor < 250;
	case "one_perk":
	case "two_perks":
		perks = 0;
		if ( isDefined( self.survival_perks ) )
			perks = self.survival_perks.size;
		need = 1;
		if ( ref == "two_perks" )
			need = 2;
		return level.survival_max_perks - perks >= need;
	case "locker":
		return isDefined( self next_weapon() );
	}
	return true;
}

give_item( ref, index )
{
	switch ( ref )
	{
	case "ammo":
	case "ammo_regen":
		return self give_ammo();
	case "armor":
	case "team_armor":
		amount = item_amount( index );
		if ( amount <= 0 )
			amount = 100;
		return self give_armor( amount );
	case "team_revive":
		return self revive_team();
	case "one_perk":
		return self give_random_perks( 1 );
	case "two_perks":
		return self give_random_perks( 2 );
	case "locker":
		return self give_weapon( self next_weapon() );
	case "mortar_mp":
		return self mortar_strike();
	}
	// The air support and equipment (predator, mortar, SAM, sentry, EMP,
	// airstrike, nuke): phase 5 of the plan.
	self iPrintLnBold( "Not available yet" );
	codolprintln( "CoDOL survival: kiosk item " + ref + " (row " + index + ") is not implemented yet" );
	return false;
}

// --------------------------------------------------------------------------
// Air support: the mortar (IW5 _so_survival_armory's "mortar" air support;
// CoDOL kiosk row 11).  IW5 lets the buyer mark the spot with a strobe; this
// server's survival has no strobe weapon yet, so the barrage falls on the
// enemies nearest him that are clear of every player.  The shells are the
// real mortar_mp projectiles (codol_launchmortarbomb, the multiplayer mortar's
// trajectory: 75 degrees, 6000 units, entering 30% into the flight).
// --------------------------------------------------------------------------

mortar_strike()
{
	if ( !codol_isWeaponMounted( "mortar_mp" ) )
	{
		self iPrintLnBold( "Not available" );
		return false;
	}
	targets = mortar_targets( 3 );
	if ( !targets.size )
	{
		self iPrintLnBold( "No targets clear of the team" );
		return false;
	}
	self iPrintLnBold( "Mortar strike inbound" );
	self thread mortar_barrage( targets );
	return true;
}

mortar_targets( count )
{
	enemies = maps\_survival_ai::living_enemies();
	players = maps\_survival_utility::living_players();
	picked = [];
	for ( n = 0; n < count; n++ )
	{
		best = undefined;
		bestDist = 0;
		for ( i = 0; i < enemies.size; i++ )
		{
			e = enemies[ i ];
			clear = true;
			for ( j = 0; j < players.size; j++ )
			{
				if ( distanceSquared( players[ j ].origin, e.origin ) < 400 * 400 )
					clear = false;
			}
			for ( j = 0; j < picked.size; j++ )
			{
				if ( distanceSquared( picked[ j ], e.origin ) < 300 * 300 )
					clear = false;
			}
			if ( !clear )
				continue;
			d = distanceSquared( self.origin, e.origin );
			if ( !isDefined( best ) || d < bestDist )
			{
				best = e;
				bestDist = d;
			}
		}
		if ( !isDefined( best ) )
			break;
		picked[ picked.size ] = best.origin;
	}
	return picked;
}

mortar_barrage( targets )
{
	self endon( "disconnect" );
	wait( 3 );
	for ( i = 0; i < targets.size; i++ )
	{
		yaw = randomInt( 360 );
		for ( shell = 0; shell < 2; shell++ )
		{
			jitter = ( randomFloatRange( -96, 96 ), randomFloatRange( -96, 96 ), 0 );
			self mortar_shell( targets[ i ] + jitter, yaw );
			wait( randomFloatRange( 0.6, 1.1 ) );
		}
		wait( 1.5 );
	}
}

mortar_shell( target, yaw )
{
	gravity = codol_getProjectileGravity();
	distance = 6000;
	ratio = 0.3;
	pitch = 75;
	speed = sqrt( gravity * distance / sin( 2 * pitch ) );
	horizontal = cos( pitch ) * speed;
	vertical = sin( pitch ) * speed;
	elapsed = ( distance / horizontal ) * ratio;
	forward = anglesToForward( ( 0, yaw, 0 ) );
	start = target - vectorScale( forward, distance * ( 1 - ratio ) );
	height = vertical * elapsed - 0.5 * gravity * elapsed * elapsed;
	start = start + ( 0, 0, height );
	velocity = vectorScale( forward, horizontal ) + ( 0, 0, vertical - gravity * elapsed );
	self codol_launchmortarbomb( "mortar_mp", start, velocity );
}

give_ammo()
{
	weapons = self getWeaponsList();
	for ( i = 0; i < weapons.size; i++ )
		self giveMaxAmmo( weapons[ i ] );
	self iPrintLn( "Ammo refilled" );
	return true;
}

// IW5 give_armor_amount: a pool soaked before health
// (_survival_mode::Callback_PlayerDamage -> _survival::armor_soak).
give_armor( amount )
{
	if ( !isDefined( self.survival_armor ) )
		self.survival_armor = 0;
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

// IW5 _so_survival_perks give_perk, with CoDOL's "one perk" / "two perks"
// rows: perks the player lacks, at random from the survival perk pool.
give_random_perks( count )
{
	if ( !isDefined( self.survival_perks ) )
		self.survival_perks = [];
	pool = [];
	for ( i = 0; i < level.survival_perk_pool.size; i++ )
	{
		if ( !self hasPerk( level.survival_perk_pool[ i ] ) )
			pool[ pool.size ] = level.survival_perk_pool[ i ];
	}
	if ( pool.size < count )
		return false;
	for ( n = 0; n < count; n++ )
	{
		k = randomInt( pool.size );
		perk = pool[ k ];
		self setPerk( perk );
		self.survival_perks[ self.survival_perks.size ] = perk;
		self iPrintLn( perk );
		rest = [];
		for ( i = 0; i < pool.size; i++ )
		{
			if ( i != k )
				rest[ rest.size ] = pool[ i ];
		}
		pool = rest;
	}
	return true;
}

// The locker's loadout change: the next weapon up the ladder from the best
// one the player holds.
next_weapon()
{
	best = -1;
	weapons = self getWeaponsListPrimaries();
	for ( i = 0; i < level.survival_ladder.size; i++ )
	{
		for ( j = 0; j < weapons.size; j++ )
		{
			if ( weapons[ j ] == level.survival_ladder[ i ] )
				best = i;
		}
	}
	for ( i = best + 1; i < level.survival_ladder.size; i++ )
	{
		if ( codol_isWeaponMounted( level.survival_ladder[ i ] ) )
			return level.survival_ladder[ i ];
	}
	return undefined;
}

give_weapon( weapon )
{
	if ( !isDefined( weapon ) )
		return false;
	self maps\_survival_mode::give_weapon( weapon, true );
	self.survival_bought = [];
	primaries = self getWeaponsListPrimaries();
	for ( i = 0; i < primaries.size; i++ )
		self.survival_bought[ i ] = primaries[ i ];
	return true;
}

vectorScale( v, scale )
{
	return ( v[ 0 ] * scale, v[ 1 ] * scale, v[ 2 ] * scale );
}
