// zm story intel: the secret notebooks, envelopes, papers and photo frames.
//
// WHAT THE MAPS CARRY (MEASURED, the zm_prototype_sh / zm_asylum_sh entity
// strings): twelve trigger_use per map, targetname "intel_collectible", each
// with a "script_index" and a "target" naming the script_model that is the
// item (notebook_zm_sh, envolope_zm_sh, scribbled_papers_zm_sh,
// photo_frame_zm_sh).  script_index is 0..11 on prototype, 12..23 on asylum.
// zm_zombie_training places none.  spawnflags are SP's difficulty exclusions:
//   1536 = NOT_NORMAL|NOT_HARD -> easy   (C1 rows *_E1.._E4)
//   1280 = NOT_EASY|NOT_HARD   -> medium (*_M1.._M4)
//    768 = NOT_EASY|NOT_NORMAL -> hard   (*_H1.._H4)
//
// WHAT C1 DOES WITH THEM (MEASURED in C1.exe; codol_script_collectibles.c has
// the addresses):
//   * mp/coCollectibles.csv row script_index names the item
//     (ZOMBIE_STORY_INTEL_PROTO_E1 ...), its description and its icon.
//   * playerdata collectibles[script_index] (the client's own public profile)
//     says whether this player has found it.
//   * The scoreboard (hold Tab) lists every objective info string (CS 482+i)
//     that carries an "intel" key: the item's name and description once
//     collectibles[intel-1] is set, "Find intel" with an unknown icon before.
//   * splashtable row "intel_found" (4 s, sound intelligence_pickup, menu
//     intel_found) is the pickup splash; the menu draws coCollectibles.csv row
//     dvarInt( "intel_splash_ref" ).
//
// So a pickup sets the player's collectibles bit (codol_collect: session
// profile, C1's 'J' push into the live record the scoreboard reads, and the
// match report the lobby persists), splashes it, and hides the item from
// that player.  There is no button prompt: walking up to the item takes it.
//
// DIFFICULTY.  CoDOL's difficulty selection is not ported (the zombiemode
// table is read at column 1 and _zombiemode_challenges never completes a
// difficulty), so by default every item on the map is collectible.
// scr_zm_intel_difficulty 1/2/3 restricts the map to that difficulty's four,
// as C1 does when a difficulty is chosen; level.zm_difficulty takes
// precedence once a difficulty selection exists.
//
// VISIBILITY.  Collection is per player, so each item is drawn only for the
// players who still need it (hide + showToPlayer per player); one nobody
// present needs is hidden from everyone.

#include maps\_zombiemode_utility;

init()
{
	level.zm_intel = [];

	triggers = getEntArray( "intel_collectible", "targetname" );
	if ( triggers.size == 0 )
		return;

	difficulty = intel_difficulty();
	table = "mp/coCollectibles.csv";
	for ( i = 0; i < triggers.size; i++ )
	{
		trigger = triggers[ i ];
		item = undefined;
		if ( isDefined( trigger.target ) )
			item = getEnt( trigger.target, "targetname" );

		if ( !isDefined( trigger.script_index ) )
		{
			codolprintln( "CoDOL zm intel: trigger at " + trigger.origin + " has no script_index" );
			continue;
		}
		index = int( trigger.script_index );
		name = tableLookupByRow( table, index, 0 );
		if ( name == "" )
		{
			codolprintln( "CoDOL zm intel: script_index " + index + " is not a row of " + table );
			continue;
		}

		itemDifficulty = intel_item_difficulty( trigger );
		if ( difficulty > 0 && itemDifficulty > 0 && itemDifficulty != difficulty )
		{
			if ( isDefined( item ) )
				item delete();
			trigger delete();
			continue;
		}

		// Secret: the trigger_use stays only as the item's volume.  No icon
		// and no text, so nothing on screen gives it away.
		trigger setCursorHint( "HINT_NOICON" );
		trigger setHintString( "" );

		intel = spawnStruct();
		intel.index = index;
		intel.name = name;
		intel.trigger = trigger;
		intel.item = item;
		intel.difficulty = itemDifficulty;
		intel.origin = trigger.origin;
		if ( isDefined( item ) )
			intel.origin = item.origin;
		level.zm_intel[ level.zm_intel.size ] = intel;
	}

	// coCollectibles.csv order: E1..E4, M1..M4, H1..H4.
	for ( i = 1; i < level.zm_intel.size; i++ )
	{
		for ( j = i; j > 0 && level.zm_intel[ j - 1 ].index > level.zm_intel[ j ].index; j-- )
		{
			swap = level.zm_intel[ j ];
			level.zm_intel[ j ] = level.zm_intel[ j - 1 ];
			level.zm_intel[ j - 1 ] = swap;
		}
	}

	// The scoreboard list.  Slots are taken from the top (31 down) so the
	// compass objective ids scripts allocate from 0 stay clear.
	for ( i = 0; i < level.zm_intel.size && i < 32; i++ )
	{
		slot = 31 - i;
		level.zm_intel[ i ].objective = slot;
		codol_objective_setintel( slot, level.zm_intel[ i ].index + 1 );
		codol_objective_setsort( slot, i + 1 );
	}

	codolprintln( "CoDOL zm intel: " + level.zm_intel.size + " of " + triggers.size +
		" intel item(s) on " + getDvar( "mapname" ) + ", difficulty " + difficulty );

	level thread intel_on_connect();
	level thread intel_think();
	level thread intel_test_init();
}

// 0 = every difficulty, else 1 easy, 2 medium, 3 hard.
intel_difficulty()
{
	if ( isDefined( level.zm_difficulty ) && level.zm_difficulty >= 1 && level.zm_difficulty <= 3 )
		return level.zm_difficulty;
	difficulty = getDvarInt( "scr_zm_intel_difficulty" );
	if ( difficulty >= 1 && difficulty <= 3 )
		return difficulty;
	return 0;
}

// The difficulty an item belongs to, from SP's NOT_EASY (256), NOT_NORMAL
// (512) and NOT_HARD (1024) exclusion bits; 0 when it is not exclusive.
intel_item_difficulty( trigger )
{
	if ( !isDefined( trigger.spawnflags ) )
		return 0;
	flags = trigger.spawnflags;
	notEasy = int( flags / 256 ) % 2;
	notNormal = int( flags / 512 ) % 2;
	notHard = int( flags / 1024 ) % 2;
	if ( !notEasy && notNormal && notHard )
		return 1;
	if ( notEasy && !notNormal && notHard )
		return 2;
	if ( notEasy && notNormal && !notHard )
		return 3;
	return 0;
}

// Test clients have no profile; they collect only for a developer test.
intel_player_counts( player )
{
	if ( !isDefined( player ) || !isPlayer( player ) )
		return false;
	if ( is_true( player.isbot ) && !getDvarInt( "scr_zm_intel_testclients" ) )
		return false;
	return true;
}

intel_on_connect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread intel_player_watch();
	}
}

intel_player_watch()
{
	self thread intel_watch_disconnect();
	self endon( "disconnect" );
	self.zm_intel_have = [];
	self intel_refresh_player();
	level thread intel_update_all();

	// The profile upload may complete after "begin"; read it again once the
	// player is in the world.  (Nothing in zm sends "spawned_player".)
	while ( !isDefined( self.sessionstate ) || self.sessionstate != "playing" )
		wait( 0.5 );
	wait( 1 );
	self intel_refresh_player();
	level thread intel_update_all();
}

// A leaving player no longer needs anything: once Callback_PlayerDisconnect
// has dropped it from level.players, recompute who sees what.
intel_watch_disconnect()
{
	self waittill( "disconnect" );
	wait( 0.05 );
	intel_update_all();
}

// What this player already holds, from its profile (earlier matches) and
// this match's pickups.
intel_refresh_player()
{
	if ( !intel_player_counts( self ) )
		return;
	for ( i = 0; i < level.zm_intel.size; i++ )
	{
		index = level.zm_intel[ i ].index;
		if ( is_true( self.zm_intel_have[ index ] ) )
			continue;
		if ( self codol_hasCollectible( index ) )
			self.zm_intel_have[ index ] = true;
	}
}

intel_player_has( player, intel )
{
	return isDefined( player.zm_intel_have ) && is_true( player.zm_intel_have[ intel.index ] );
}

intel_update_all()
{
	for ( i = 0; i < level.zm_intel.size; i++ )
		intel_update_visibility( level.zm_intel[ i ] );
}

intel_update_visibility( intel )
{
	if ( !isDefined( intel.item ) )
		return;

	needers = [];
	for ( i = 0; i < level.players.size; i++ )
	{
		player = level.players[ i ];
		if ( !intel_player_counts( player ) )
			continue;
		if ( !intel_player_has( player, intel ) )
			needers[ needers.size ] = player;
	}

	// With no counted player at all (only bots, or an empty server) leave it
	// as the map placed it.
	if ( intel_counted_players() == 0 )
	{
		intel.item show();
		return;
	}

	// hide() sets every bit of the entity's per-client snapshot mask and
	// showToPlayer() clears one (retained ScrCmd_Hide / ScrCmd_ShowToPlayer,
	// honoured by sv_snapshot.c), so this is exact per-player visibility.
	intel.item hide();
	for ( i = 0; i < needers.size; i++ )
		intel.item showToPlayer( needers[ i ] );
}

intel_counted_players()
{
	count = 0;
	for ( i = 0; i < level.players.size; i++ )
	{
		if ( intel_player_counts( level.players[ i ] ) )
			count++;
	}
	return count;
}

// Walking up to the item takes it: inside the level designer's trigger
// volume, or within reach of the model itself.
intel_think()
{
	level endon( "end_game" );
	for ( ;; )
	{
		wait( 0.1 );
		for ( p = 0; p < level.players.size; p++ )
		{
			player = level.players[ p ];
			if ( !intel_player_counts( player ) || !isDefined( player.zm_intel_have ) )
				continue;
			if ( !isAlive( player ) )
				continue;
			if ( isDefined( player.sessionstate ) && player.sessionstate != "playing" )
				continue;
			// Not while a power-up weapon is in hand, like every use trigger.
			if ( is_true( player.has_powerup_weapon ) )
				continue;
			for ( i = 0; i < level.zm_intel.size; i++ )
			{
				intel = level.zm_intel[ i ];
				if ( intel_player_has( player, intel ) )
					continue;
				if ( !intel_in_reach( player, intel ) )
					continue;
				player intel_collect( intel );
			}
		}
	}
}

intel_in_reach( player, intel )
{
	if ( isDefined( intel.trigger ) && player isTouching( intel.trigger ) )
		return true;
	dz = intel.origin[ 2 ] - player.origin[ 2 ];
	if ( dz < -40 || dz > 96 )
		return false;
	dx = intel.origin[ 0 ] - player.origin[ 0 ];
	dy = intel.origin[ 1 ] - player.origin[ 1 ];
	return dx * dx + dy * dy <= 48 * 48;
}

intel_collect( intel )
{
	// The profile is the authority: it may hold the item from an earlier
	// match even when the connect-time read had no upload yet.
	if ( self codol_hasCollectible( intel.index ) )
	{
		self.zm_intel_have[ intel.index ] = true;
		intel_update_visibility( intel );
		return;
	}

	self.zm_intel_have[ intel.index ] = true;
	self codol_collect( intel.index );

	// C1's intel_found splash: plays intelligence_pickup and draws
	// mp/coCollectibles.csv row dvarInt( "intel_splash_ref" ).
	self setClientDvar( "intel_splash_ref", intel.index );
	self ShowHudSplash( "intel_found", 0 );

	codolprintln( "CoDOL zm intel: " + self.name + " found " + intel.name + " (collectibles[" +
		intel.index + "], scoreboard slot " + intel.objective + ")" );
	self notify( "zm_intel_found", intel.index );
	level notify( "zm_intel_found", self, intel.index );

	intel_update_visibility( intel );
}

// --------------------------------------------------------------------------
// Developer-only test verbs (scr_zm_inteltest).  Idle on the live server,
// which runs developer 0.
//
//   list                       every intel item: index, name, origin, slot
//   goto <client> <index>      move the client onto the item; the pickup
//                              loop then takes it (set
//                              scr_zm_intel_testclients 1 for a test client)
//   show <client>              what the client holds, per the profile
// --------------------------------------------------------------------------

intel_test_init()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;
	setDvar( "scr_zm_inteltest", "" );
	for ( ;; )
	{
		wait( 0.5 );
		request = getDvar( "scr_zm_inteltest" );
		if ( request == "" )
			continue;
		setDvar( "scr_zm_inteltest", "" );
		tokens = strTok( request, " " );

		if ( tokens[ 0 ] == "list" )
		{
			for ( i = 0; i < level.zm_intel.size; i++ )
			{
				intel = level.zm_intel[ i ];
				codolprintln( "CoDOL inteltest: [" + intel.index + "] " + intel.name + " difficulty " +
					intel.difficulty + " at " + intel.origin + " slot " + intel.objective );
			}
			continue;
		}

		player = undefined;
		if ( tokens.size > 1 )
		{
			num = int( tokens[ 1 ] );
			for ( i = 0; i < level.players.size; i++ )
			{
				if ( level.players[ i ] getEntityNumber() == num )
					player = level.players[ i ];
			}
		}
		if ( !isDefined( player ) )
		{
			codolprintln( "CoDOL inteltest: no client" );
			continue;
		}

		if ( tokens[ 0 ] == "goto" && tokens.size > 2 )
		{
			target = int( tokens[ 2 ] );
			for ( i = 0; i < level.zm_intel.size; i++ )
			{
				if ( level.zm_intel[ i ].index != target )
					continue;
				player setOrigin( level.zm_intel[ i ].origin );
				codolprintln( "CoDOL inteltest: " + player.name + " moved to intel " + target );
			}
		}
		else if ( tokens[ 0 ] == "show" )
		{
			text = "";
			for ( i = 0; i < level.zm_intel.size; i++ )
			{
				index = level.zm_intel[ i ].index;
				text += " " + index + "=" + player codol_hasCollectible( index );
			}
			codolprintln( "CoDOL inteltest: " + player.name + " holds" + text );
		}
	}
}
