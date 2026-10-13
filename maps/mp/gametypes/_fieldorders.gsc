// sh1x server / Call of Duty Online Closed Alpha Server: C1 field orders
// (战地指令, "Field Order received" / "Field Order complete").
//
// WHAT C1 SHIPS (MEASURED; native evidence in
// src/codol/player/codol_field_orders.h):
//
//   * Four player methods, SetFOGoal / SetFOProgress / SetFOReward /
//     SetFOStatus( <index>, <value> ), store one byte each into the player's
//     playerState at 0x1b4 / 0x1bc / 0x1c4 / 0x1cc + index*4. Two slots.
//   * The field_orders HUD menu (patch_common_mp) draws slot i while
//     getFOGoal(i) > 0 and getFOProgress(i) < target:
//       title    locstring( "@" + tablelookupbyrow( fo_table, goal, 1 ),
//                           int( tablelookupbyrow( fo_table, goal, 5 ) ) )
//       hint     locstring( "@" + column 2 ) while getFOStatus(i) == 0
//       reward   locstring( "@" + column 4, getFOReward(i) )  ("&&1 XP/GP")
//       bar      getFOPercent(i) = progress / column 5
//     and slides a slot in after the fo_display scriptmenu stamps
//     ui_show_fo = milliseconds(), out 500 ms after fo_complete_time stamps
//     ui_fo_complete_time. fo_table is the client dvar naming the table
//     ("mp/cofieldorderbootcamp.csv"); C1.exe reads it with Dvar_FindVar
//     (0x5e326f) and nothing in the client ever sets it.
//   * Splash rows field_order_get (6 s, mp_bonus_start, FO_RECEIVED, column 1
//     and 2 of row dvarInt( "fo_splash_ref" )) and field_order_win (3 s,
//     mp_challenge_complete, FO_COMPLETE and the reward: locstring( "@" +
//     column 4, column 7 )).
//   * mp/cofieldordertable.csv names the sets: (ref, 1, 3) for
//     coFieldOrderBootcamp and zm_FieldOrder, (ref, 1, 9) for
//     coFieldOrderMPOnboarding. Each ref is also a row of
//     mp/allChallengesTable.csv (ids 930 / 932 / 931: "complete 100
//     multiplayer / zombies field orders"), the career counter of completed
//     orders. Column 1 is read here as the concurrent slot count and column 2
//     as the orders per match: onboarding has exactly nine orders.
//     ASSUMPTION - C1's own server scripts are not in any shipped zone.
//   * A set's row: 0 ref, 1 title, 2 hint, 3 icon, 4 reward type (FO_XP /
//     FO_GP), 5 target, 6 (0), 7 reward amount, 8 / 9 flags, 10 required perk,
//     11 required killstreak, 12 C1's notify name for an action order.
//
// WHAT THIS SCRIPT DOES
//
//   A human player gets the MP onboarding orders in table order until the
//   onboarding challenge (931) counts all of them, then three random
//   bootcamp orders per match, one at a time. Progress comes from kills
//   (_damage.gsc calls onEnemyKilled) and from the player notifies C1's
//   column 12 names. A completed order pays its reward (FO_XP through
//   giveRankXP, FO_GP through codol_creditGP into the match report, which
//   the lobby deposits in the GP wallet) and advances the set's challenge
//   in challenges[], which the match report already carries to the lobby.
//
// Not done: zm_FieldOrder (zombies has its own scripts), the act_scores order
// (nothing tells the server the scoreboard is open), and columns 8/9.

#include maps\mp\_utility;
#include common_scripts\utility;

FO_COL_TITLE = 1;
FO_COL_HINT = 2;
FO_COL_REWARD_TYPE = 4;
FO_COL_TARGET = 5;
FO_COL_REWARD = 7;
FO_COL_PERK = 10;
FO_COL_STREAK = 11;


init()
{
	level.fieldOrders = getIntProperty( "scr_fieldorders", 1 );
	if ( !level.fieldOrders )
		return;

	precacheMenu( "fo_display" );
	precacheMenu( "fo_complete_time" );

	level.foSets = [];
	summary = "";
	for ( row = 0; ; row++ )
	{
		ref = tableLookupByRow( "mp/cofieldordertable.csv", row, 0 );
		if ( ref == "" )
			break;

		foSet = spawnStruct();
		foSet.ref = ref;
		foSet.table = "mp/" + toLower( ref ) + ".csv";
		foSet.slots = int( tableLookupByRow( "mp/cofieldordertable.csv", row, 1 ) );
		if ( foSet.slots < 1 )
			foSet.slots = 1;
		if ( foSet.slots > 2 )
			foSet.slots = 2;
		foSet.perMatch = int( tableLookupByRow( "mp/cofieldordertable.csv", row, 2 ) );
		foSet.orders = foLoadOrders( foSet.table );
		level.foSets[ toLower( ref ) ] = foSet;
		summary = summary + " " + ref + "(" + foSet.orders.size + " orders, " + foSet.perMatch + "/match)";
	}

	codolPrintLn( "CoDOL field orders:" + summary + "; splash field_order_get sound '"
		+ tableLookup( "mp/splashTable.csv", 0, "field_order_get", 9 ) + "'" );

	if ( !isDefined( level.foSets[ "cofieldorderbootcamp" ] ) &&
	     !isDefined( level.foSets[ "cofieldordermponboarding" ] ) )
		return;

	level thread onPlayerConnect();
}


foLoadOrders( table )
{
	orders = [];
	for ( row = 1; ; row++ )
	{
		ref = tableLookupByRow( table, row, 0 );
		if ( ref == "" )
			break;

		order = spawnStruct();
		order.index = row;
		order.ref = ref;
		title = tableLookupByRow( table, row, FO_COL_TITLE );
		// "fo_kill_pri" -> "kill_pri", "fo_act_reload" -> "act_reload".
		if ( getSubStr( title, 0, 3 ) == "fo_" )
			order.kind = getSubStr( title, 3 );
		else
			order.kind = title;
		order.rewardType = tableLookupByRow( table, row, FO_COL_REWARD_TYPE );
		order.target = int( tableLookupByRow( table, row, FO_COL_TARGET ) );
		if ( order.target < 1 )
			order.target = 1;
		if ( order.target > 255 )
			order.target = 255;
		order.reward = int( tableLookupByRow( table, row, FO_COL_REWARD ) );
		order.perk = tableLookupByRow( table, row, FO_COL_PERK );
		order.streak = tableLookupByRow( table, row, FO_COL_STREAK );
		orders[ orders.size ] = order;
	}
	return orders;
}


// The order kinds this server can observe.
foKindSupported( kind )
{
	switch ( kind )
	{
		case "kill_pri":
		case "kill_sec":
		case "kill_let":
		case "kill_ads":
		case "kill_mel":
		case "kill_cro":
		case "kill_pro":
		case "act_reload":
		case "act_pickupammo":
		case "act_uavearn":
		case "act_uavuse":
		case "act_sprint":
		case "act_grenadethrow":
		case "act_pickupwpn":
			return true;
		default:
			return false;
	}
}


onPlayerConnect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread foPlayer();
	}
}


foPlayer()
{
	self endon( "disconnect" );
	level endon( "game_ended" );

	// Bots have no career. scr_fieldorders_bots 1 lets them run orders anyway,
	// which exercises the whole flow on a server with no human (no reward or
	// challenge is recorded for them).
	if ( isDefined( self.isbot ) && self.isbot && !getDvarInt( "scr_fieldorders_bots" ) )
		return;

	self waittill( "spawned_player" );
	// The class (perks, killstreaks) is applied with the spawn.
	wait ( 1.0 );

	foSet = self foChooseSet();
	if ( !isDefined( foSet ) || !foSet.orders.size || foSet.perMatch < 1 )
		return;

	self.foSet = foSet;
	self.foSlots = [];
	self.foAssigned = 0;
	self.foUsed = [];
	self setClientDvar( "fo_table", foSet.table );

	self thread foWatchNotify( "reload", "act_reload" );
	self thread foWatchNotify( "scavenger_pickup", "act_pickupammo" );
	self thread foWatchNotify( "pickedupweapon", "act_pickupwpn" );
	self thread foWatchStreak( "given_killstreak", "act_uavearn" );
	self thread foWatchStreak( "killstreak_used", "act_uavuse" );
	self thread foWatchGrenades();
	self thread foWatchSprint();

	for ( slot = 0; slot < foSet.slots; slot++ )
		self thread foSlotLoop( slot );
}


// Onboarding until its challenge has counted every onboarding order, the
// bootcamp set after that.
foChooseSet()
{
	onboarding = level.foSets[ "cofieldordermponboarding" ];
	bootcamp = level.foSets[ "cofieldorderbootcamp" ];

	if ( isDefined( onboarding ) && onboarding.orders.size )
	{
		done = self foChallengeProgress( onboarding.ref );
		if ( done < onboarding.orders.size )
		{
			self.foOnboardingDone = done;
			return onboarding;
		}
	}
	return bootcamp;
}


foChallengeProgress( ref )
{
	if ( !self rankingEnabled() )
	{
		if ( isDefined( self.pers[ "foProgress_" + ref ] ) )
			return self.pers[ "foProgress_" + ref ];
		return 0;
	}
	if ( maps\mp\gametypes\_missions::ch_challengeId( ref ) < 0 )
		return 0;
	return int( self maps\mp\gametypes\_missions::ch_getProgress( ref ) );
}


foSlotLoop( slot )
{
	self endon( "disconnect" );
	level endon( "game_ended" );

	for ( ;; )
	{
		if ( self.foAssigned >= self.foSet.perMatch )
			return;

		while ( !isReallyAlive( self ) )
			wait ( 0.5 );

		order = self foPickOrder();
		if ( !isDefined( order ) )
			return;

		self.foAssigned++;
		self.foUsed[ order.ref ] = true;
		self foAssign( slot, order );

		self waittill( "fo_complete_" + slot );
		// field_order_win is 3 s; the completed slot slides out 500 ms after
		// fo_complete_time.
		wait ( 4.0 );
		self foClear( slot );
		wait ( 2.0 );
	}
}


foPickOrder()
{
	foSet = self.foSet;

	if ( foSet.ref == "coFieldOrderMPOnboarding" )
	{
		// In table order from the first one the career has not counted.
		for ( i = self.foOnboardingDone; i < foSet.orders.size; i++ )
		{
			order = foSet.orders[ i ];
			if ( !isDefined( self.foUsed[ order.ref ] ) && self foOrderAvailable( order ) )
				return order;
		}
		return undefined;
	}

	candidates = [];
	foreach ( order in foSet.orders )
	{
		if ( !isDefined( self.foUsed[ order.ref ] ) && self foOrderAvailable( order ) )
			candidates[ candidates.size ] = order;
	}
	if ( !candidates.size )
		return undefined;
	return candidates[ randomInt( candidates.size ) ];
}


foOrderAvailable( order )
{
	if ( !foKindSupported( order.kind ) )
		return false;

	if ( order.perk != "" && !self _hasPerk( order.perk ) )
		return false;

	if ( order.streak != "" )
	{
		if ( !isDefined( self.codolSelectedKillstreaks ) )
			return false;
		found = false;
		foreach ( streak in self.codolSelectedKillstreaks )
		{
			if ( isDefined( streak ) && streak == order.streak )
				found = true;
		}
		if ( !found )
			return false;
	}
	return true;
}


foAssign( slot, order )
{
	state = spawnStruct();
	state.order = order;
	state.progress = 0;
	state.done = false;
	self.foSlots[ slot ] = state;

	self setFOGoal( slot, order.index );
	self setFOProgress( slot, 0 );
	self setFOReward( slot, order.reward );
	self setFOStatus( slot, 0 );

	// fo_display stamps ui_show_fo, which slides the HUD slot in.
	self maps\mp\gametypes\_playerlogic::pulseScriptMenu( "fo_display" );

	self setClientDvar( "fo_splash_ref", order.index );
	self foSplash( "field_order_get", "mp_bonus_start" );

	codolPrintLn( "CoDOL field orders: " + self.name + " slot " + slot + " got " + self.foSet.ref
		+ " row " + order.index + " " + order.ref + " (" + order.kind + " x" + order.target
		+ ", " + order.reward + " " + order.rewardType + ")" );
}


foClear( slot )
{
	self.foSlots[ slot ] = undefined;
	self setFOGoal( slot, 0 );
	self setFOProgress( slot, 0 );
	self setFOReward( slot, 0 );
	self setFOStatus( slot, 0 );
}


foSplash( name, defaultSound )
{
	actionData = spawnStruct();
	actionData.name = name;
	actionData.type = "fieldorder";
	actionData.sound = tableLookup( "mp/splashTable.csv", 0, name, 9 );
	if ( actionData.sound == "" )
		actionData.sound = defaultSound;
	actionData.slot = 0;
	self thread maps\mp\gametypes\_hud_message::actionNotify( actionData );
}


// Progress for every active slot holding an order of this kind. `filter` is a
// killstreak name for the two UAV orders.
foAdvance( kind, amount, filter )
{
	if ( !isDefined( self.foSlots ) )
		return;
	if ( !isDefined( amount ) )
		amount = 1;

	for ( slot = 0; slot < 2; slot++ )
	{
		state = self.foSlots[ slot ];
		if ( !isDefined( state ) || state.done || state.order.kind != kind )
			continue;
		if ( isDefined( filter ) && state.order.streak != "" && state.order.streak != filter )
			continue;

		state.progress += amount;
		if ( state.progress > state.order.target )
			state.progress = state.order.target;
		self setFOProgress( slot, state.progress );

		if ( state.progress >= state.order.target )
			self foComplete( slot );
	}
}


foComplete( slot )
{
	state = self.foSlots[ slot ];
	state.done = true;
	order = state.order;

	// The hint line draws only while the status is 0.
	self setFOStatus( slot, 1 );
	// fo_complete_time stamps ui_fo_complete_time: the completed slot shows
	// briefly and slides out.
	self maps\mp\gametypes\_playerlogic::pulseScriptMenu( "fo_complete_time" );

	self setClientDvar( "fo_splash_ref", order.index );
	self foSplash( "field_order_win", "mp_challenge_complete" );

	self foGrantReward( order );
	self thread foCreditChallenge( self.foSet.ref );

	codolPrintLn( "CoDOL field orders: " + self.name + " completed " + self.foSet.ref
		+ " row " + order.index + " " + order.ref + " (" + order.reward + " " + order.rewardType + ")" );

	self notify( "fo_complete_" + slot );
}


foGrantReward( order )
{
	if ( order.reward <= 0 )
		return;

	if ( order.rewardType == "FO_GP" )
	{
		// Recorded for the lobby, which deposits it in the GP wallet.
		self codol_creditGP( order.reward );
	}
	else if ( order.rewardType == "FO_XP" )
	{
		self thread maps\mp\gametypes\_rank::giveRankXP( "challenge", order.reward );
	}
}


// One more completed order on the set's career challenge (allChallengesTable
// ids 930 / 931 / 932), written to challenges[] like every other challenge so
// the match report carries it to the lobby; a crossed tier splashes and pays
// its column-24 XP like _missions::processChallenge.
foCreditChallenge( ref )
{
	self endon( "disconnect" );

	if ( !self rankingEnabled() )
	{
		self.pers[ "foProgress_" + ref ] = self foChallengeProgress( ref ) + 1;
		return;
	}
	if ( maps\mp\gametypes\_missions::ch_challengeId( ref ) < 0 )
		return;

	before = self maps\mp\gametypes\_missions::ch_getProgress( ref );
	after = before + 1;
	self maps\mp\gametypes\_missions::ch_setProgress( ref, after );

	if ( !isDefined( level.challengeInfo[ ref ] ) || !isDefined( level.challengeInfo[ ref ][ "targetval" ] ) )
		return;
	targets = level.challengeInfo[ ref ][ "targetval" ];
	for ( tier = 1; isDefined( targets[ tier ] ); tier++ )
	{
		if ( before < targets[ tier ] && after >= targets[ tier ] )
		{
			self thread maps\mp\gametypes\_hud_message::challengeSplashNotify( ref );
			reward = level.challengeInfo[ ref ][ "reward" ][ tier ];
			if ( isDefined( reward ) && reward > 0 )
				self thread maps\mp\gametypes\_rank::giveRankXP( "challenge", reward );
		}
	}
}


// _damage.gsc, for every enemy kill, in the attacker's context and before any
// wait, so the stance and ADS read are the ones the kill happened with.
onEnemyKilled( victim, sWeapon, sMeansOfDeath )
{
	if ( !isDefined( self.foSlots ) || !isDefined( sWeapon ) || !isDefined( sMeansOfDeath ) )
		return;

	if ( sMeansOfDeath == "MOD_MELEE" )
	{
		self foAdvance( "kill_mel" );
		return;
	}

	bullet = ( sMeansOfDeath == "MOD_RIFLE_BULLET" || sMeansOfDeath == "MOD_PISTOL_BULLET" ||
	           sMeansOfDeath == "MOD_HEAD_SHOT" );
	weaponClass = getWeaponClass( sWeapon );

	if ( foIsLethal( sWeapon ) )
		self foAdvance( "kill_let" );
	else if ( weaponClass == "weapon_pistol" || weaponClass == "weapon_machine_pistol" ||
	          weaponClass == "weapon_projectile" )
		self foAdvance( "kill_sec" );
	else if ( bullet )
		self foAdvance( "kill_pri" );

	if ( !bullet )
		return;

	if ( self playerAds() >= 0.5 )
		self foAdvance( "kill_ads" );

	stance = self getStance();
	if ( stance == "crouch" )
		self foAdvance( "kill_cro" );
	else if ( stance == "prone" )
		self foAdvance( "kill_pro" );
}


// A lethal piece of equipment: an offhand that is not a tactical one.
foIsLethal( weapon )
{
	if ( weapon == "none" || codol_weaponInventoryType( weapon ) != "offhand" )
		return false;

	tactical = [];
	tactical[ tactical.size ] = "flash";
	tactical[ tactical.size ] = "concussion";
	tactical[ tactical.size ] = "smoke";
	tactical[ tactical.size ] = "stun";
	tactical[ tactical.size ] = "emp";
	tactical[ tactical.size ] = "gas";
	tactical[ tactical.size ] = "tabun";
	tactical[ tactical.size ] = "decoy";
	tactical[ tactical.size ] = "light_stick";
	tactical[ tactical.size ] = "willy";
	tactical[ tactical.size ] = "scrambler";
	tactical[ tactical.size ] = "trophy";
	tactical[ tactical.size ] = "tactical";
	tactical[ tactical.size ] = "insertion";
	foreach ( word in tactical )
	{
		if ( isSubStr( weapon, word ) )
			return false;
	}
	return true;
}


foWatchNotify( notifyName, kind )
{
	self endon( "disconnect" );
	level endon( "game_ended" );

	for ( ;; )
	{
		self waittill( notifyName );
		self foAdvance( kind );
	}
}


foWatchStreak( notifyName, kind )
{
	self endon( "disconnect" );
	level endon( "game_ended" );

	for ( ;; )
	{
		self waittill( notifyName, streakName );
		self foAdvance( kind, 1, streakName );
	}
}


foWatchGrenades()
{
	self endon( "disconnect" );
	level endon( "game_ended" );

	for ( ;; )
	{
		self waittill( "grenade_fire", grenade, weaponName );
		if ( isDefined( weaponName ) && foIsLethal( weaponName ) )
			self foAdvance( "act_grenadethrow" );
	}
}


foWatchSprint()
{
	self endon( "disconnect" );
	level endon( "game_ended" );

	carry = 0;
	for ( ;; )
	{
		self waittill( "sprint_begin" );
		last = self.origin;
		for ( ;; )
		{
			result = self waittill_any_timeout( 0.25, "sprint_end", "death" );
			carry += distance2D( last, self.origin );
			last = self.origin;
			// FO_ACT_SPRINT is "sprint one kilometre" with a target of 100:
			// one step per 10 m (393.7 units).
			while ( carry >= 393.7 )
			{
				carry -= 393.7;
				self foAdvance( "act_sprint" );
			}
			if ( result != "timeout" )
				break;
		}
	}
}
