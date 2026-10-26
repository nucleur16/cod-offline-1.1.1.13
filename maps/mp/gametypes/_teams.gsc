// PROVENANCE: vendor/iw4/maps/mp/gametypes/_teams.gsc adapted for CoDOL. CoDOL arenas
// keep IW4 character-set names (allieschar socom_141_desert, axischar
// opforce_composite) and _teams uses those for player models, but the mounted
// mp/factionTable.csv keys are the alpha's two factions (taskforce_141 first,
// shadow_company second) and carry no character-set rows. The faction-table
// lookups therefore key on the team's faction row (allies = data row 1,
// axis = data row 2) instead of game[teamRef]. CoDOL live balancing below also
// retains classes, prefers training bots and waits for natural human deaths.

#include maps\mp\_utility;

FACTION_REF_COL 					= 0;
FACTION_NAME_COL 					= 1;
FACTION_SHORT_NAME_COL 				= 2;
FACTION_ELIMINATED_COL 				= 3;
FACTION_FORFEITED_COL 				= 4;
FACTION_ICON_COL 					= 5;
FACTION_HUD_ICON_COL 				= 6;
FACTION_VOICE_PREFIX_COL 			= 7;
FACTION_SPAWN_MUSIC_COL 			= 8;
FACTION_WIN_MUSIC_COL 				= 9;
FACTION_FLAG_MODEL_COL 				= 10;
FACTION_FLAG_CARRY_MODEL_COL 		= 11;
FACTION_FLAG_ICON_COL 				= 12;
FACTION_FLAG_FX_COL 				= 13;
FACTION_COLOR_R_COL 				= 14;
FACTION_COLOR_G_COL 				= 15;
FACTION_COLOR_B_COL 				= 16;
FACTION_HEAD_ICON_COL 				= 17;
FACTION_CRATE_MODEL_COL 			= 18;

init()
{
	initScoreBoard();

	// Dedicated C1 playlists do not all publish this IW4 dvar. An absent
	// value must not silently disable balancing; an explicit 0 still does.
	if ( getDvar( "scr_teambalance" ) == "" )
		setDvar( "scr_teambalance", 1 );
	level.teamBalance = getDvarInt("scr_teambalance");
	level.maxClients = getDvarInt( "sv_maxclients" );

	level._effect["thermal_beacon"] = loadFx("misc/thermal_beacon_inverted");
	effect = level._effect["thermal_beacon"];
	PrecacheFxTeamThermal( effect, "J_Spine4" );

	setPlayerModels();

	level.freeplayers = [];

	if( level.teamBased )
	{
		level thread onPlayerConnect();
		// Infection deliberately grows one side by converting survivors.
		// Normal balancing would move infected back and cap survivors at half.
		if ( level.gameType == "infect" )
			level.teamLimit = level.maxClients;
		else
			level thread updateTeamBalance();

		wait .15;
		level thread updatePlayerTimes();
	}
	else
	{
		level thread onFreePlayerConnect();

		wait .15;
		level thread updateFreePlayerTimes();
	}
}


initScoreBoard()
{
	setDvar("g_TeamName_Allies", getTeamShortName( "allies" ));
	setDvar("g_TeamIcon_Allies", getTeamIcon( "allies" ));
	setDvar("g_TeamIcon_MyAllies", getTeamIcon( "allies" ));
	setDvar("g_TeamIcon_EnemyAllies", getTeamIcon( "allies" ));
	scoreColor = getTeamColor( "allies" );
	setDvar("g_ScoresColor_Allies", scoreColor[0] + " " + scoreColor[1] + " " + scoreColor[2] );

	setDvar("g_TeamName_Axis", getTeamShortName( "axis" ));
	setDvar("g_TeamIcon_Axis", getTeamIcon( "axis" ));
	setDvar("g_TeamIcon_MyAxis", getTeamIcon( "axis" ));
	setDvar("g_TeamIcon_EnemyAxis", getTeamIcon( "axis" ));
	scoreColor = getTeamColor( "axis" );
	setDvar("g_ScoresColor_Axis", scoreColor[0] + " " + scoreColor[1] + " " + scoreColor[2] );

	setdvar("g_ScoresColor_Spectator", ".25 .25 .25");
	setdvar("g_ScoresColor_Free", ".76 .78 .10");
	setdvar("g_teamColor_MyTeam", ".6 .8 .6" );
	setdvar("g_teamColor_EnemyTeam", "1 .45 .5" );
	setdvar("g_teamTitleColor_MyTeam", ".6 .8 .6" );
	setdvar("g_teamTitleColor_EnemyTeam", "1 .45 .5" );
}

onPlayerConnect()
{
	for(;;)
	{
		level waittill( "connected", player );

		player thread onJoinedTeam();
		player thread onJoinedSpectators();
		player thread onPlayerSpawned();

		player thread trackPlayedTime();
	}
}


onFreePlayerConnect()
{
	for(;;)
	{
		level waittill( "connected", player );

		player thread trackFreePlayedTime();
	}
}


onJoinedTeam()
{
	self endon("disconnect");

	for(;;)
	{
		self waittill( "joined_team" );
		//self logString( "joined team: " + self.pers["team"] );
		self updateTeamTime();
	}
}


onJoinedSpectators()
{
	self endon("disconnect");

	for(;;)
	{
		self waittill("joined_spectators");
		self.pers["teamTime"] = undefined;
	}
}


trackPlayedTime()
{
	self endon( "disconnect" );

	self.timePlayed["allies"] = 0;
	self.timePlayed["axis"] = 0;
	self.timePlayed["free"] = 0;
	self.timePlayed["other"] = 0;
	self.timePlayed["total"] = 0;

	gameFlagWait( "prematch_done" );

	for ( ;; )
	{
		if ( game["state"] == "playing" )
		{
			if ( self.sessionteam == "allies" )
			{
				self.timePlayed["allies"]++;
				self.timePlayed["total"]++;
			}
			else if ( self.sessionteam == "axis" )
			{
				self.timePlayed["axis"]++;
				self.timePlayed["total"]++;
			}
			else if ( self.sessionteam == "spectator" )
			{
				self.timePlayed["other"]++;
			}

		}

		wait ( 1.0 );
	}
}


updatePlayerTimes()
{
	if ( !level.rankedmatch )
		return;

	level endon( "game_ended" );

	for ( ;; )
	{
		foreach ( player in level.players )
			player updatePlayedTime();

		wait( 1.0 );
	}
}


updatePlayedTime()
{
	if ( !self rankingEnabled() )
		return;

	if ( self.timePlayed["allies"] )
	{
		self maps\mp\gametypes\_persistence::statAddBuffered( "timePlayedAllies", self.timePlayed["allies"] );
		self maps\mp\gametypes\_persistence::statAddBuffered( "timePlayedTotal", self.timePlayed["allies"] );
		self maps\mp\gametypes\_persistence::statAddChildBuffered( "round", "timePlayed", self.timePlayed["allies"] );
	}

	if ( self.timePlayed["axis"] )
	{
		self maps\mp\gametypes\_persistence::statAddBuffered( "timePlayedOpfor", self.timePlayed["axis"] );
		self maps\mp\gametypes\_persistence::statAddBuffered( "timePlayedTotal", self.timePlayed["axis"] );
		self maps\mp\gametypes\_persistence::statAddChildBuffered( "round", "timePlayed", self.timePlayed["axis"] );
	}

	if ( self.timePlayed["other"] )
	{
		self maps\mp\gametypes\_persistence::statAddBuffered( "timePlayedOther", self.timePlayed["other"] );
		self maps\mp\gametypes\_persistence::statAddBuffered( "timePlayedTotal", self.timePlayed["other"] );
		self maps\mp\gametypes\_persistence::statAddChildBuffered( "round", "timePlayed", self.timePlayed["other"] );
	}

	if ( game["state"] == "postgame" )
		return;

	self.timePlayed["allies"] = 0;
	self.timePlayed["axis"] = 0;
	self.timePlayed["other"] = 0;
}


updateTeamTime()
{
	self.pers["teamTime"] = getTime();
}


updateTeamBalanceDvar()
{
	for(;;)
	{
		teambalance = getdvarInt("scr_teambalance");
		if(level.teambalance != teambalance)
			level.teambalance = getdvarInt("scr_teambalance");

		wait 1;
	}
}


updateTeamBalance()
{
	level.teamLimit = level.maxclients / 2;

	level thread updateTeamBalanceDvar();

	wait .15;

	if ( isRoundBased() )
	{
        if( isDefined( game["BalanceTeamsNextRound"] ) )
                iPrintLnbold( &"MP_AUTOBALANCE_NEXT_ROUND" );

		// TODO: add or change
		level waittill( "restarting" );

		if( level.teamBalance && isDefined( game["BalanceTeamsNextRound"] ) )
		{
			level balanceTeams( true );
			game["BalanceTeamsNextRound"] = undefined;
		}
		else if( level.teamBalance && !getTeamBalance() )
		{
			game["BalanceTeamsNextRound"] = true;
		}
	}
	else
	{
		level endon ( "game_ended" );
		level.teamBalanceStartedAt = undefined;
		level.teamBalanceAnnouncedSeconds = undefined;
		for( ;; )
		{
			updateTeamBalanceCountdown();

			// Keep observing joins/leaves and catch a human's death before their
			// normal respawn. No minute-long blind interval after every check.
			wait 0.25;
		}
	}

}


getTeamBalanceSecondsRemaining( startedAt, now )
{
    if ( !isDefined( startedAt ) )
        return 0;
    remaining = 15000 - ( now - startedAt );
    if ( remaining <= 0 )
        return 0;
    if ( remaining > 15000 )
        remaining = 15000;
    return int( ceil( remaining / 1000.0 ) );
}


updateTeamBalanceCountdown()
{
    // game["state"] is already playing during the loading/countdown phase.
    // Initial human assignment and bot fill must finish before announcing a
    // live imbalance. Otherwise a transient 2-versus-0 starts the warning
    // while the remaining ten clients are still being assigned normally.
    if ( !level.teamBalance || game["state"] != "playing" ||
            !gameFlag( "prematch_done" ) || getTeamBalance() )
    {
        level.teamBalanceStartedAt = undefined;
        level.teamBalanceAnnouncedSeconds = undefined;
        return;
    }

    if ( !isDefined( level.teamBalanceStartedAt ) )
        level.teamBalanceStartedAt = getTime();
    seconds = getTeamBalanceSecondsRemaining( level.teamBalanceStartedAt, getTime() );
    if ( seconds > 0 )
    {
        if ( !isDefined( level.teamBalanceAnnouncedSeconds ) )
        {
            iPrintLnBold( "Teams will be balanced in 15 seconds" );
            level.teamBalanceAnnouncedSeconds = seconds;
        }
        return;
    }

    // Keep the original deadline while waiting for a safe candidate. A
    // human's next natural death must not start another 15-second countdown.
    level balanceTeams( false );
}


getTeamBalance()
{
	level.team["allies"] = 0;
	level.team["axis"] = 0;

	players = level.players;
	for(i = 0; i < players.size; i++)
	{
		if((isdefined(players[i].pers["team"])) && (players[i].pers["team"] == "allies"))
			level.team["allies"]++;
		else if((isdefined(players[i].pers["team"])) && (players[i].pers["team"] == "axis"))
			level.team["axis"]++;
	}

	if((level.team["allies"] > (level.team["axis"] + level.teamBalance)) || (level.team["axis"] > (level.team["allies"] + level.teamBalance)))
		return false;
	else
		return true;
}


// Retain IW4's team membership/notifies and normal death/respawn lifecycle.
// The team-choice menus deliberately clear the class and await UI input;
// using them here stranded training bots and interrupted human respawns.
balanceTeams( roundBoundary )
{
    if ( !isDefined( roundBoundary ) )
        roundBoundary = false;

    moved = false;
    while ( !getTeamBalance() )
    {
        if ( !roundBoundary && ( !level.teamBalance || game["state"] != "playing" ) )
            return;

        fromTeam = "allies";
        toTeam = "axis";
        if ( level.team["axis"] > level.team["allies"] )
        {
            fromTeam = "axis";
            toTeam = "allies";
        }

        player = getBalanceCandidate( fromTeam, roundBoundary );
        // Everyone may be alive, cooling down, remote-controlling or exempt.
        // Leave the match running and retry; never call an undefined entity
        // or spin forever without yielding when no switch is possible.
        if ( !isDefined( player ) )
            return;

        if ( !moved )
            iPrintLnBold( "Balancing teams" );
        moved = true;
        player.codolLastAutoBalance = getTime();
        if ( isAlive( player ) )
        {
            player.switching_teams = true;
            player.joining_team = toTeam;
            player.leaving_team = fromTeam;
            player suicide();
        }
        player maps\mp\gametypes\_menus::addToTeam( toTeam );
        codolPrintLn( "CoDOL autobalance: " + player.name + " " + fromTeam + " -> " + toTeam );
        // Preserve class/pers class and the existing death/killcam/spawn
        // thread. It now selects the destination team's spawn and loadout.
    }
}


getBalanceCandidate( team, roundBoundary )
{
    best = undefined;
    bestBot = false;
    bestAlive = true;
    bestLastMove = 0;
    bestTeamTime = 0;
    foreach ( player in level.players )
    {
        if ( !isDefined( player.pers["team"] ) || player.pers["team"] != team )
            continue;
        if ( isDefined( player.dont_auto_balance ) || isDefined( player.switching_teams ) )
            continue;
        // Every party-enabled Quick human, including solo cohorts, has an
        // authoritative side. Balance bots around them; never split a group.
        if ( player codol_quickpartyteam() != 0 )
            continue;
        if ( player isUsingRemote() )
            continue;
        // A client still selecting its first class must keep that menu.
        if ( !isValidClass( player.class ) )
            continue;

        bot = maps\mp\gametypes\_codol_bots::isCombatBot( player );
        alive = isAlive( player );
        // Live humans move after dying naturally. Bots can use the standard
        // team-switch death path, so bot fill/removal never waits on a human.
        if ( !roundBoundary && alive && !bot )
            continue;

        lastMove = 0;
        if ( isDefined( player.codolLastAutoBalance ) )
        {
            lastMove = player.codolLastAutoBalance;
            if ( !roundBoundary && getTime() - lastMove < 60000 )
                continue;
        }
        teamTime = 0;
        if ( isDefined( player.pers["teamTime"] ) )
            teamTime = player.pers["teamTime"];

        // Bots first, dead bots first, then the least recently moved player;
        // equal candidates retain IW4's newest-team-member preference.
        if ( isDefined( best ) )
        {
            if ( bestBot && !bot )
                continue;
            if ( bot == bestBot )
            {
                if ( !bestAlive && alive )
                    continue;
                if ( alive == bestAlive )
                {
                    if ( lastMove > bestLastMove )
                        continue;
                    if ( lastMove == bestLastMove && teamTime <= bestTeamTime )
                        continue;
                }
            }
        }
        best = player;
        bestBot = bot;
        bestAlive = alive;
        bestLastMove = lastMove;
        bestTeamTime = teamTime;
    }
    return best;
}


setGhillieModels( env )
{
	// CoDOL composes every player from their own playerdata character record
	// (playerModelForWeapon below), so the IW4 ghillie mptype models are not a
	// model source here. The environment itself is still published: it is the
	// map's camouflage context and callers read level.environment.
	level.environment = env;
}

setTeamModels( team, charSet )
{
	// Deliberately empty. IW4 filled game[team + "_model"][<class>] with
	// the mptype modules' main pointers and precached the whole
	// character/mptype/xmodelalias chain (~130 scripts naming IW4 xmodels that
	// no CoDOL zone contains, which is where the "using void" warnings came
	// from). Nothing reads that table any more: playerModelForWeapon() is the
	// only model producer and it calls codol_applyProfileCharacter /
	// codol_applyBotCharacter. game["allies"]/game["axis"] still carry the
	// faction names used for team name, voice and side data, which is why the
	// arguments survive.
}

setPlayerModels()
{
	setTeamModels( "allies", game["allies"] );
	setTeamModels( "axis", game["axis"] );

	setGhillieModels( getMapCustom( "environment" ) );
}


playerModelForWeapon( weapon, secondary )
{
	// C1 composes the selected faction's characterClasses record at runtime.
	// This also installs the jacket sleeves and gloves as separate viewmodels.
	faction = "tf";
	if ( self.team == "axis" )
		faction = "sc";
    if ( isDefined( self.codolCombatBot ) && self.codolCombatBot )
        self codol_applyBotCharacter( self.codolBotClass, faction, weapon );
    else
	self codol_applyProfileCharacter( faction, weapon );
	return;
}


CountPlayers()
{
	//chad
	players = level.players;
	allies = 0;
	axis = 0;
	for(i = 0; i < players.size; i++)
	{
		if ( players[i] == self )
			continue;

		if((isdefined(players[i].pers["team"])) && (players[i].pers["team"] == "allies"))
			allies++;
		else if((isdefined(players[i].pers["team"])) && (players[i].pers["team"] == "axis"))
			axis++;
	}
	players["allies"] = allies;
	players["axis"] = axis;
	return players;
}


trackFreePlayedTime()
{
	self endon( "disconnect" );

	self.timePlayed["allies"] = 0;
	self.timePlayed["axis"] = 0;
	self.timePlayed["other"] = 0;
	self.timePlayed["total"] = 0;

	for ( ;; )
	{
		if ( game["state"] == "playing" )
		{
			if ( isDefined( self.pers["team"] ) && self.pers["team"] == "allies" && self.sessionteam != "spectator" )
			{
				self.timePlayed["allies"]++;
				self.timePlayed["total"]++;
			}
			else if ( isDefined( self.pers["team"] ) && self.pers["team"] == "axis" && self.sessionteam != "spectator" )
			{
				self.timePlayed["axis"]++;
				self.timePlayed["total"]++;
			}
			else
			{
				self.timePlayed["other"]++;
			}
		}

		wait ( 1.0 );
	}
}


/#
playerConnectedTest()
{
	if ( getdvarint( "scr_runlevelandquit" ) == 1 )
		return;

	level endon( "exitLevel_called" );

	// every frame, do a getPlayerData on each player in level.players.
	// this will force a script error if a player in level.players isn't connected.
	for ( ;; )
	{
		foreach ( player in level.players )
		{
			player getPlayerData( "experience" );
		}
		wait .05;
	}
}
#/


updateFreePlayerTimes()
{
	if ( !level.rankedmatch )
		return;

	/#
	thread playerConnectedTest();
	#/

	nextToUpdate = 0;
	for ( ;; )
	{
		nextToUpdate++;
		if ( nextToUpdate >= level.players.size )
			nextToUpdate = 0;

		if ( isDefined( level.players[nextToUpdate] ) )
			level.players[nextToUpdate] updateFreePlayedTime();

		wait ( 1.0 );
	}
}


updateFreePlayedTime()
{
	if ( !self rankingEnabled() )
		return;

	if ( self.timePlayed["allies"] )
	{
		self maps\mp\gametypes\_persistence::statAddBuffered( "timePlayedAllies", self.timePlayed["allies"] );
		self maps\mp\gametypes\_persistence::statAddBuffered( "timePlayedTotal", self.timePlayed["allies"] );
	}

	if ( self.timePlayed["axis"] )
	{
		self maps\mp\gametypes\_persistence::statAddBuffered( "timePlayedOpfor", self.timePlayed["axis"] );
		self maps\mp\gametypes\_persistence::statAddBuffered( "timePlayedTotal", self.timePlayed["axis"] );
	}

	if ( self.timePlayed["other"] )
	{
		self maps\mp\gametypes\_persistence::statAddBuffered( "timePlayedOther", self.timePlayed["other"] );
		self maps\mp\gametypes\_persistence::statAddBuffered( "timePlayedTotal", self.timePlayed["other"] );
	}

	if ( game["state"] == "postgame" )
		return;

	self.timePlayed["allies"] = 0;
	self.timePlayed["axis"] = 0;
	self.timePlayed["other"] = 0;
}


// C1 automatically answers its team popup with the retained side. Refusing a
// full bot-filled side and reopening that popup produces an endless menu echo
// after HotJoin. A real player can replace training AI, never another human.
tryJoinTeam( team )
{
	if ( getJoinTeamPermissions( team ) )
		return true;
	if ( !isPlayer( self ) || self.isbot || maps\mp\gametypes\_codol_bots::isCombatBot( self ) )
		return false;
	if ( level.teamLimit <= 0 || ( team != "allies" && team != "axis" ) )
		return false;

	bots = [];
	teamCount = 0;
	foreach ( player in level.players )
	{
		if ( !isDefined( player.pers["team"] ) || player.pers["team"] != team )
			continue;
		teamCount++;
		if ( maps\mp\gametypes\_codol_bots::isCombatBot( player ) )
			bots[bots.size] = player;
	}
	// GSC division makes teamLimit a float; -- accepts only an integer.
	needed = int( teamCount - level.teamLimit + 1 );
	if ( needed <= 0 )
		return true;
	if ( needed > bots.size )
		return false;
	// Native kick synchronously disconnects and removes the bot from
	// level.players. No wait occurs between reserving its seat and joining.
	for ( i = bots.size - 1; i >= 0 && needed > 0; i-- )
	{
		kick( bots[i] getEntityNumber(), "Kicked by server" );
		needed--;
	}
	return getJoinTeamPermissions( team );
}


getJoinTeamPermissions( team )
{
	teamcount = 0;

	players = level.players;
	for(i = 0; i < players.size; i++)
	{
		player = players[i];

		if((isdefined(player.pers["team"])) && (player.pers["team"] == team))
			teamcount++;
	}

	if( teamCount < level.teamLimit )
		return true;
	else
		return false;
}


onPlayerSpawned()
{
	level endon ( "game_ended" );

	for ( ;; )
	{
		self waittill ( "spawned_player" );
	}
}

getTeamName( teamRef )
{
	return ( tableLookupIString( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_NAME_COL ) );
}

getTeamShortName( teamRef )
{
	return ( tableLookupIString( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_SHORT_NAME_COL ) );
}

getTeamForfeitedString( teamRef )
{
	return ( tableLookupIString( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_FORFEITED_COL ) );
}

getTeamEliminatedString( teamRef )
{
	return ( tableLookupIString( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_ELIMINATED_COL ) );
}

getTeamIcon( teamRef )
{
	return ( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_ICON_COL ) );
}

getTeamHudIcon( teamRef )
{
	return ( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_HUD_ICON_COL ) );
}

getTeamHeadIcon( teamRef )
{
	return ( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_HEAD_ICON_COL ) );
}

getTeamVoicePrefix( teamRef )
{
	return ( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_VOICE_PREFIX_COL ) );
}

// CoDOL: C1's player voice aliases carry the speaker's gender and voice
// number after the faction voice: uk_m1_inform_reloading_generic,
// ab_f1_mp_death. The team prefix alone ("UK_") names the 1mc announcer
// only. codol_getVoicePrefix returns the cg_vo_prefix the server sent this
// player's client for its character ("UK_M1_", "AB_F1_").
getPlayerVoicePrefix( player )
{
	prefix = player codol_getVoicePrefix();
	if ( isDefined( prefix ) )
		return prefix;
	prefix = getTeamVoicePrefix( player.team );
	if ( !isDefined( prefix ) || prefix == "" )
		prefix = "UK_";
	return prefix + "M1_";
}

// "m" or "f": the gender of the player's voice. C1 ships the hurt breathing
// per gender (m_breathing_hurt, f_breathing_better).
getPlayerVoiceGender( player )
{
	prefix = getPlayerVoicePrefix( player );
	if ( prefix.size < 4 )
		return "m";
	return tolower( getSubStr( prefix, 3, 4 ) );
}

getTeamSpawnMusic( teamRef )
{
	return ( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_SPAWN_MUSIC_COL ) );
}

getTeamWinMusic( teamRef )
{
	return ( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_WIN_MUSIC_COL ) );
}

getTeamFlagModel( teamRef )
{
	return ( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_FLAG_MODEL_COL ) );
}

getTeamFlagCarryModel( teamRef )
{
	return ( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_FLAG_CARRY_MODEL_COL ) );
}

getTeamFlagIcon( teamRef )
{
	return ( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_FLAG_ICON_COL ) );
}

getTeamFlagFX( teamRef )
{
	return ( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_FLAG_FX_COL ) );
}

getTeamColor( teamRef )
{
	return ( (stringToFloat( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_COLOR_R_COL ) ),
				stringToFloat( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_COLOR_G_COL ) ),
				stringToFloat( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_COLOR_B_COL ) ))
			);
}

getTeamCrateModel( teamRef )
{
	return ( tableLookup( "mp/factionTable.csv", FACTION_REF_COL, getFactionRef( teamRef ), FACTION_CRATE_MODEL_COL ) );
}


// CoDOL: the faction-table key for a team is the alpha's faction row, not the
// arena character set stored in game[teamRef].
getFactionRef( teamRef )
{
	if ( teamRef == "allies" )
		return tableLookupByRow( "mp/factionTable.csv", 1, 0 );
	return tableLookupByRow( "mp/factionTable.csv", 2, 0 );
}
