// CoDOL after-action report: the end-of-match summary C1 shows back in the
// lobby.
//
// WHAT C1 SHOWS AND WHERE IT READS IT (measured in codo.gfx, decompiled with
// JPEXS: codo.controllers.LobbySummaryController and MatchController)
//
//   The lobby movie scaleform/default/codo.gfx (patch_ui_mp) opens the page
//   PAGE_ADV_POPUP_LOBBY_SUMMARY from MatchController.onBackToRoomHandle when
//   the client comes back from a match, if client dvar ui_eog_show is 1 and
//   LobbySummaryController found anything to show. That controller builds
//   every panel from CLIENT DVARS this server sets with setClientDvar (C1 's',
//   CG_SetClientDvarFromServer 0x468A00 -> 0x625330 creates unknown dvars), and
//   clears each one after reading it:
//
//     ui_eog_show        1 = open the summary on the return
//     ui_eog_xp          "<lifetime XP at match start>|<XP gained>"  (progress bar)
//     ui_eog_r1..r4      "<name>|<count>|<xp>|<gp>"; the four token-2 values
//                        sum into the "XP" line (kills, assists, objectives,
//                        medals here)
//     ui_eog_r5          misc XP (token 2), added to the "misc" line
//     ui_eog_r6          challenges: token 2 XP, token 3 GP. Owned by
//                        _missions.gsc (codolNoteChallengeReward).
//     ui_eog_r7          match bonus: token 2 XP (win/loss/tie), token 3 GP
//                        (GP from play, plus the fixed rank-up GP: 200 x
//                        each level reached this match)
//     ui_eog_r10         field orders: token 2 XP (added to misc), token 3 GP
//     ui_eog_ch<i>       "vis|ref|tier|start|gain|target|startPct|gainPct|xp|gp"
//                        one challenge the match advanced (allChallengesTable
//                        ref; read until the first empty one, 6 shown)
//     ui_eog_ev_count    number of medal rows
//     ui_eog_ev<i>       "<splashTable ref>|<count>|<xp>"
//
//   The GP total C1 shows is r6 + r7 + r10 token 3. The scoreboard panel is the
//   client's own last scoreboard (AS_GetScoreBoard_Main); weapon level-ups
//   (AS_GetWeaponLevelup) and rank unlocks are diffed by C1 itself from the
//   inventory and profile it re-reads from the lobby after the return.
//
// The retail server scripts that set these dvars never shipped (MP gametype
// scripts are server-only), so the dvar contract above is C1's reader, and the
// values are this server's own accounting:
//
//   XP  every award passes _rank::giveRankXP, which reports the XP actually
//       committed (noteXP); the gained total is the same lifetime XP the match
//       report measures for the lobby.
//   GP  the match report row (codol_matchGP), so the summary shows exactly what
//       the lobby deposits from progression.gp.
//   Challenges   every write goes through _missions::ch_setProgress
//                (noteChallengeProgress).
//   Medals       every medal splash goes through
//                _hud_message::splashNotifyDelayed (noteMedal).
//
// The dvars are kept current during the match (at most every REFRESH seconds
// after a change), so a player who leaves early returns to an up-to-date
// summary, and are sent complete once more just before the match report is
// written (sendFinal, from _gamelogic::processLobbyData).

REFRESH = 10;
MAX_CHALLENGES = 6;
MAX_TRACKED_CHALLENGES = 64;
MAX_MEDALS = 16;

init()
{
	level thread onPlayerConnect();
}


onPlayerConnect()
{
	for ( ;; )
	{
		level waittill( "connected", player );

		if ( player isAARIgnored() )
			continue;

		// pers survives a round restart but not a new map, so this runs once
		// per match. A rotation (codol_exitToLobby 0) keeps the client
		// connected across matches: clear what the last match left behind so
		// leaving this one early cannot show the old summary.
		if ( !isDefined( player.pers["codolAAR"] ) )
		{
			player.pers["codolAAR"] = true;
			player resetClientSummary();
		}
	}
}


// Bots have no lobby to return to. scr_aar_trace 2 keeps their
// accounting (printed, never sent) for a probe without a C1 client.
isAARIgnored()
{
	if ( !( isDefined( self.isbot ) && self.isbot ) )
		return false;
	return getDvarInt( "scr_aar_trace" ) < 2;
}


resetClientSummary()
{
	self setClientDvars( "ui_eog_show", 0,
						 "ui_eog_xp", "",
						 "ui_eog_r1", "",
						 "ui_eog_r2", "",
						 "ui_eog_r3", "",
						 "ui_eog_r4", "",
						 "ui_eog_r5", "",
						 "ui_eog_r6", "" );
	self setClientDvars( "ui_eog_r7", "",
						 "ui_eog_r10", "",
						 "ui_eog_ch0", "",
						 "ui_eog_ev_count", 0 );
}


// ---- accounting -------------------------------------------------------------

// The summary line an XP award type belongs to. The match bonus and challenge
// tiers have their own lines; whatever is not named is misc.
xpRowFor( type )
{
	switch ( type )
	{
		case "kill":
		case "suicide":
		case "teamkill":
		case "shield_damage":
		case "dogkill":
			return "kills";

		case "assist":
			return "assists";

		case "capture":
		case "defend":
		case "return":
		case "pickup":
		case "assault":
		case "plant":
		case "defuse":
		case "destroy":
		case "save":
		case "kill_confirmed":
		case "kill_denied":
			return "objectives";

		// _events.gsc: each of these is a medal splash as well as an award.
		case "headshot":
		case "longshot":
		case "execution":
		case "avenger":
		case "assistedsuicide":
		case "defender":
		case "posthumous":
		case "revenge":
		case "firstblood":
		case "buzzkill":
		case "comeback":
		case "reviver":
		case "hijacker":
			return "medals";

		case "win":
		case "loss":
		case "tie":
			return "bonus";

		// _missions.gsc tiers and _fieldorders.gsc orders both award as
		// "challenge"; sendSummary splits them with _missions' own tally.
		case "challenge":
			return "challenge";

		default:
			return "misc";
	}
}


// _rank::giveRankXP, after incRankXP: oldXP and newXP are the player's
// lifetime XP before and after this award.
noteXP( type, oldXP, newXP )
{
	if ( self isAARIgnored() )
		return;

	if ( !isDefined( self.pers["codolAARStartXP"] ) )
		self.pers["codolAARStartXP"] = oldXP;

	gained = newXP - oldXP;
	if ( gained == 0 )
		return;

	if ( !isDefined( self.pers["codolAARXP"] ) )
		self.pers["codolAARXP"] = [];
	row = xpRowFor( type );
	if ( !isDefined( self.pers["codolAARXP"][row] ) )
		self.pers["codolAARXP"][row] = 0;
	self.pers["codolAARXP"][row] += gained;

	self markDirty();
}


// _missions::ch_setProgress, before the write.
noteChallengeProgress( ref, before, after )
{
	if ( self isAARIgnored() )
		return;
	if ( after <= before )
		return;

	if ( !isDefined( self.pers["codolAARChRefs"] ) )
	{
		self.pers["codolAARChRefs"] = [];
		self.pers["codolAARChStart"] = [];
		self.pers["codolAARChEnd"] = [];
	}

	if ( !isDefined( self.pers["codolAARChStart"][ref] ) )
	{
		if ( self.pers["codolAARChRefs"].size >= MAX_TRACKED_CHALLENGES )
			return;
		self.pers["codolAARChRefs"][self.pers["codolAARChRefs"].size] = ref;
		self.pers["codolAARChStart"][ref] = before;
	}
	self.pers["codolAARChEnd"][ref] = after;

	self markDirty();
}


// _hud_message::splashNotifyDelayed: one medal (a slot-0 splash).
noteMedal( ref, xp )
{
	if ( self isAARIgnored() || level.gameEnded )
		return;

	if ( !isDefined( self.pers["codolAARMedalRefs"] ) )
	{
		self.pers["codolAARMedalRefs"] = [];
		self.pers["codolAARMedalCount"] = [];
		self.pers["codolAARMedalXP"] = [];
	}

	if ( !isDefined( self.pers["codolAARMedalCount"][ref] ) )
	{
		if ( self.pers["codolAARMedalRefs"].size >= MAX_MEDALS )
			return;
		self.pers["codolAARMedalRefs"][self.pers["codolAARMedalRefs"].size] = ref;
		self.pers["codolAARMedalCount"][ref] = 0;
		self.pers["codolAARMedalXP"][ref] = 0;
	}
	self.pers["codolAARMedalCount"][ref]++;
	if ( isDefined( xp ) )
		self.pers["codolAARMedalXP"][ref] += int( xp );

	self markDirty();
}


markDirty()
{
	self.codolAARDirty = true;
	if ( level.gameEnded || isDefined( self.codolAARLoop ) )
		return;
	self.codolAARLoop = true;
	self thread refreshLoop();
}


refreshLoop()
{
	self endon( "disconnect" );
	level endon( "game_ended" );

	for ( ;; )
	{
		wait ( REFRESH );
		if ( !self.codolAARDirty )
			continue;
		self.codolAARDirty = false;
		self sendSummary();
	}
}


// _gamelogic::processLobbyData, before sendClientMatchData() writes the
// "gameended" match report (which carries, and then zeroes, the row's GP).
sendFinal()
{
	foreach ( player in level.players )
	{
		if ( !isDefined( player ) || player isAARIgnored() )
			continue;
		player sendSummary();
	}
}


// ---- the dvars --------------------------------------------------------------

xpRow( row )
{
	if ( !isDefined( self.pers["codolAARXP"] ) || !isDefined( self.pers["codolAARXP"][row] ) )
		return 0;
	return self.pers["codolAARXP"][row];
}


summaryRow( name, xp, gp )
{
	return name + "|0|" + xp + "|" + gp;
}


sendSummary()
{
	// A bot (trace only) has no rank XP at all.
	nowXP = 0;
	if ( isDefined( self.pers["rankxp"] ) )
		nowXP = self.pers["rankxp"];

	startXP = nowXP;
	if ( isDefined( self.pers["codolAARStartXP"] ) )
		startXP = self.pers["codolAARStartXP"];
	gainedXP = nowXP - startXP;
	if ( gainedXP < 0 )
		gainedXP = 0;

	// Challenge tier XP is _missions' tally (ui_eog_r6); the rest of the
	// "challenge" awards are field orders and their career challenge.
	missionXP = 0;
	if ( isDefined( self.pers["codolChallengeXP"] ) )
		missionXP = self.pers["codolChallengeXP"];
	fieldOrderXP = self xpRow( "challenge" ) - missionXP;
	if ( fieldOrderXP < 0 )
		fieldOrderXP = 0;

	// GP is the match report's: what the lobby will deposit.
	totalGP = self codol_matchGP();
	fieldOrderGP = self codol_matchGP( "fieldorder" );
	challengeGP = self codol_matchGP( "challenge" );
	// r7 carries everything else the row will deposit: the scaled play GP and
	// the fixed rank-up GP (codol_matchGP( "rankup" )), so r6 + r7 + r10, the
	// GP C1 shows, is the deposit.
	bonusGP = totalGP - fieldOrderGP - challengeGP;
	if ( bonusGP < 0 )
		bonusGP = 0;

	medalCount = 0;
	if ( isDefined( self.pers["codolAARMedalRefs"] ) )
		medalCount = self.pers["codolAARMedalRefs"].size;
	chLines = self challengeLines();

	show = gainedXP > 0 || totalGP > 0 || medalCount > 0 || chLines.size > 0;

	pairs = [];
	pairs = addPair( pairs, "ui_eog_xp", startXP + "|" + gainedXP );
	pairs = addPair( pairs, "ui_eog_r1", summaryRow( "kills", self xpRow( "kills" ), 0 ) );
	pairs = addPair( pairs, "ui_eog_r2", summaryRow( "assists", self xpRow( "assists" ), 0 ) );
	pairs = addPair( pairs, "ui_eog_r3", summaryRow( "objectives", self xpRow( "objectives" ), 0 ) );
	pairs = addPair( pairs, "ui_eog_r4", summaryRow( "medals", self xpRow( "medals" ), 0 ) );
	pairs = addPair( pairs, "ui_eog_r5", summaryRow( "misc", self xpRow( "misc" ), 0 ) );
	pairs = addPair( pairs, "ui_eog_r7", summaryRow( "bonus", self xpRow( "bonus" ), bonusGP ) );
	pairs = addPair( pairs, "ui_eog_r10", summaryRow( "fieldorders", fieldOrderXP, fieldOrderGP ) );

	// Challenges: the reader stops at the first empty slot, so terminate the
	// list (MatchController clears slots 0..5 itself when it does not open the
	// page).
	for ( i = 0; i < chLines.size; i++ )
		pairs = addPair( pairs, "ui_eog_ch" + i, chLines[i] );
	if ( chLines.size < MAX_CHALLENGES )
		pairs = addPair( pairs, "ui_eog_ch" + chLines.size, "" );

	for ( i = 0; i < medalCount; i++ )
	{
		ref = self.pers["codolAARMedalRefs"][i];
		pairs = addPair( pairs, "ui_eog_ev" + i, ref + "|" + self.pers["codolAARMedalCount"][ref] + "|" + self.pers["codolAARMedalXP"][ref] );
	}
	pairs = addPair( pairs, "ui_eog_ev_count", medalCount );

	// Last, so the page never opens on a half-delivered summary.
	if ( show )
		pairs = addPair( pairs, "ui_eog_show", 1 );

	self flushPairs( pairs );
}


addPair( pairs, name, value )
{
	pairs[pairs.size] = name;
	pairs[pairs.size] = value;
	return pairs;
}


// Four dvars per setClientDvars (one C1 's' command each, well inside its
// 1024-character limit). scr_aar_trace 1 prints what a player is sent;
// 2 also runs the accounting for bots and prints theirs, sending them nothing.
flushPairs( pairs )
{
	trace = getDvarInt( "scr_aar_trace" );
	botSummary = isDefined( self.isbot ) && self.isbot;
	if ( trace > 0 )
	{
		line = "CoDOL AAR: " + self.name;
		if ( botSummary )
			line += " (bot, not sent)";
		for ( i = 0; i < pairs.size; i += 2 )
			line += " " + pairs[i] + "=\"" + pairs[i + 1] + "\"";
		codolPrintLn( line );
	}
	if ( botSummary )
		return;

	for ( i = 0; i < pairs.size; i += 8 )
	{
		left = pairs.size - i;
		if ( left >= 8 )
			self setClientDvars( pairs[i], pairs[i + 1], pairs[i + 2], pairs[i + 3], pairs[i + 4], pairs[i + 5], pairs[i + 6], pairs[i + 7] );
		else if ( left >= 6 )
			self setClientDvars( pairs[i], pairs[i + 1], pairs[i + 2], pairs[i + 3], pairs[i + 4], pairs[i + 5] );
		else if ( left >= 4 )
			self setClientDvars( pairs[i], pairs[i + 1], pairs[i + 2], pairs[i + 3] );
		else
			self setClientDvar( pairs[i], pairs[i + 1] );
	}
}


// Up to MAX_CHALLENGES summary lines, tiers completed this match first, then
// the most advanced.
challengeLines()
{
	lines = [];
	if ( !isDefined( self.pers["codolAARChRefs"] ) )
		return lines;

	refs = [];
	scores = [];
	texts = [];
	foreach ( ref in self.pers["codolAARChRefs"] )
	{
		if ( !isDefined( level.challengeInfo ) || !isDefined( level.challengeInfo[ref] ) )
			continue;
		targets = level.challengeInfo[ref]["targetval"];
		start = self.pers["codolAARChStart"][ref];
		progress = self.pers["codolAARChEnd"][ref];

		// The tier the match was working on: one plus the thresholds the
		// starting progress had already reached (_missions::ch_getState).
		tier = 1;
		while ( isDefined( targets[tier] ) && start >= targets[tier] )
			tier++;
		if ( !isDefined( targets[tier] ) || targets[tier] <= 0 )
			continue;
		target = targets[tier];

		startPct = start / target;
		endPct = min( progress, target ) / target;
		gainPct = endPct - startPct;

		xp = 0;
		gp = 0;
		if ( progress >= target )
		{
			if ( isDefined( level.challengeInfo[ref]["reward"][tier] ) )
				xp = level.challengeInfo[ref]["reward"][tier];
			if ( isDefined( level.challengeInfo[ref]["gp"] ) && isDefined( level.challengeInfo[ref]["gp"][tier] ) )
				gp = level.challengeInfo[ref]["gp"][tier];
		}

		score = endPct;
		if ( progress >= target )
			score += 1;

		refs[refs.size] = ref;
		scores[scores.size] = score;
		texts[texts.size] = "1|" + ref + "|" + tier + "|" + start + "|" + (progress - start) + "|" + target
			+ "|" + startPct + "|" + gainPct + "|" + xp + "|" + gp;
	}

	// Selection by score, highest first (a handful of entries).
	used = [];
	while ( lines.size < MAX_CHALLENGES )
	{
		best = -1;
		for ( i = 0; i < texts.size; i++ )
		{
			if ( isDefined( used[i] ) )
				continue;
			if ( best < 0 || scores[i] > scores[best] )
				best = i;
		}
		if ( best < 0 )
			break;
		used[best] = true;
		lines[lines.size] = texts[best];
	}
	return lines;
}
