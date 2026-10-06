// IW5 GSC SOURCE
// Decompiled by https://github.com/xensik/gsc-tool
//
// Kill Confirmed (engine gametype name "conf"), ported from IW5's
// maps/mp/gametypes/conf.gsc onto this server's IW4 gametype library.
//
// Objective:  Kill enemies, then collect the dog tags they drop to score.
//             Collecting a friendly tag denies the enemy the confirm.
// Map ends:   When one team reaches the score limit, or the time limit.
// Respawning: No wait, near teammates.
//
// The IW5 rules are kept as they are: one tag set per victim, reused on every
// death (the old one vanishes when the victim dies again); a tag is an "any"
// proximity use object on a script spawned trigger_radius; enemies of the
// victim see the foe model and teammates the friend model, both spinning and
// bobbing; only the killer gets the compass and on-screen tag icon; there is
// no tag expiry. A confirm scores one team point; the collector and, when it
// was not the collector, the killer are paid "kill_confirmed". A teammate
// picking it up is paid "kill_denied", the victim picking up their own tag
// "tags_retrieved", and the killer is told the kill was denied.
//
// CoDOL adaptations (C1 1.1.1.13 assets and this VM):
// - Models prop_dogtags_foe / prop_dogtags_friend, shader waypoint_dogtags,
//   sounds mp_killconfirm_tags_drop / _pickup / _deny, fx
//   impacts/small_snowhit and the SPLASHES_* / OBJECTIVES_CONF* strings are
//   the IW5 names, and C1 ships every one of them (docs/zone_assets).
// - IW5's "<voice>kill_confirmed" announcer line is "<voice>killfirm" in C1
//   (uk_1mc_killfirm, ab_1mc_killfirm).
// - IW5 _rank::xpEventPopup has no counterpart in the IW4 _rank.gsc this
//   server runs; xpEventPopup below is IW5's, built on first use.
// - Match rules data (isUsingMatchRulesData) is not registered here; the
//   dvar defaults are IW5's non match rules branch.
// - IW5 writes the "round" confirmed / denied children. C1's playerdata has
//   no such members (codol_match_report.c), so only the career "confirmed" /
//   "denied" counters are kept.
// - A tag's objective id is taken when the tag drops and released when it
//   resets, instead of being held per victim for the whole match: C1 has 32
//   objective slots and getNextObjID clamps at 31.
// - level.codolConfTags lists the live tags' triggers (with codolDropTeam)
//   for the training bots (_codol_bot_objective::addTagTasks).

#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	if ( getdvar( "mapname" ) == "mp_background" )
		return;

	maps\mp\gametypes\_globallogic::init();
	maps\mp\gametypes\_callbacksetup::SetupCallbacks();
	maps\mp\gametypes\_globallogic::SetupCallbacks();

	registerRoundSwitchDvar( level.gameType, 0, 0, 9 );
	registerTimeLimitDvar( level.gameType, 10, 0, 1440 );
	registerScoreLimitDvar( level.gameType, 65, 0, 5000 );
	registerRoundLimitDvar( level.gameType, 1, 0, 10 );
	registerWinLimitDvar( level.gameType, 1, 0, 10 );
	registerNumLivesDvar( level.gameType, 0, 0, 10 );
	registerHalfTimeDvar( level.gameType, 0, 0, 1 );

	level.teamBased = true;
	level.onStartGameType = ::onStartGameType;
	level.getSpawnPoint = ::getSpawnPoint;
	level.onNormalDeath = ::onNormalDeath;
	level.onPrecacheGameType = ::onPrecacheGameType;

	game["dialog"]["gametype"] = "kill_confirmed";

	level.conf_fx["vanish"] = loadfx( "impacts/small_snowhit" );
}


onPrecacheGameType()
{
	precacheModel( "prop_dogtags_friend" );
	precacheModel( "prop_dogtags_foe" );
	precacheShader( "waypoint_dogtags" );

	precacheString( &"SPLASHES_KILL_CONFIRMED" );
	precacheString( &"SPLASHES_KILL_DENIED" );
	precacheString( &"SPLASHES_TAGS_RETRIEVED" );
	precacheString( &"SPLASHES_DENIED_KILL" );
}


onStartGameType()
{
	setClientNameMode( "auto_change" );

	if ( !isdefined( game["switchedsides"] ) )
		game["switchedsides"] = false;

	if ( game["switchedsides"] )
	{
		oldAttackers = game["attackers"];
		oldDefenders = game["defenders"];
		game["attackers"] = oldDefenders;
		game["defenders"] = oldAttackers;
	}

	setObjectiveText( "allies", &"OBJECTIVES_CONF" );
	setObjectiveText( "axis", &"OBJECTIVES_CONF" );

	if ( level.splitscreen )
	{
		setObjectiveScoreText( "allies", &"OBJECTIVES_CONF" );
		setObjectiveScoreText( "axis", &"OBJECTIVES_CONF" );
	}
	else
	{
		setObjectiveScoreText( "allies", &"OBJECTIVES_CONF_SCORE" );
		setObjectiveScoreText( "axis", &"OBJECTIVES_CONF_SCORE" );
	}
	setObjectiveHintText( "allies", &"OBJECTIVES_CONF_HINT" );
	setObjectiveHintText( "axis", &"OBJECTIVES_CONF_HINT" );

	level.spawnMins = ( 0, 0, 0 );
	level.spawnMaxs = ( 0, 0, 0 );
	maps\mp\gametypes\_spawnlogic::placeSpawnPoints( "mp_tdm_spawn_allies_start" );
	maps\mp\gametypes\_spawnlogic::placeSpawnPoints( "mp_tdm_spawn_axis_start" );
	maps\mp\gametypes\_spawnlogic::addSpawnPoints( "allies", "mp_tdm_spawn" );
	maps\mp\gametypes\_spawnlogic::addSpawnPoints( "axis", "mp_tdm_spawn" );

	level.mapCenter = maps\mp\gametypes\_spawnlogic::findBoxCenter( level.spawnMins, level.spawnMaxs );
	setMapCenter( level.mapCenter );

	maps\mp\gametypes\_rank::registerScoreInfo( "kill", 50 );
	maps\mp\gametypes\_rank::registerScoreInfo( "kill_confirmed", 50 );
	maps\mp\gametypes\_rank::registerScoreInfo( "kill_denied", 50 );
	maps\mp\gametypes\_rank::registerScoreInfo( "tags_retrieved", 250 );

	level.dogtags = [];
	level.codolConfTags = [];

	allowed[0] = level.gameType;
	maps\mp\gametypes\_gameobjects::main( allowed );
}


getSpawnPoint()
{
	spawnteam = self.pers["team"];
	if ( game["switchedsides"] )
		spawnteam = getOtherTeam( spawnteam );

	if ( level.inGracePeriod )
	{
		spawnPoints = maps\mp\gametypes\_spawnlogic::getSpawnpointArray( "mp_tdm_spawn_" + spawnteam + "_start" );
		spawnPoint = maps\mp\gametypes\_spawnlogic::getSpawnpoint_Random( spawnPoints );
	}
	else
	{
		spawnPoints = maps\mp\gametypes\_spawnlogic::getTeamSpawnPoints( spawnteam );
		spawnPoint = maps\mp\gametypes\_spawnlogic::getSpawnpoint_NearTeam( spawnPoints );
	}

	return spawnPoint;
}


// A kill scores nothing for the team: the tag does. The kill itself is still
// credited to the killer (_damage pays the "kill" score registered above).
onNormalDeath( victim, attacker, lifeId )
{
	level thread spawnDogTags( victim, attacker );

	if ( game["state"] == "postgame" && game["teamScores"][attacker.team] > game["teamScores"][level.otherTeam[attacker.team]] )
		attacker.finalKill = true;
}


// IW5 keys a victim's tags by guid; the entity number is unique among the
// connected players, bots included, and clearOnVictimDisconnect frees it.
tagKey( victim )
{
	return "" + victim getEntityNumber();
}


spawnDogTags( victim, attacker )
{
	key = tagKey( victim );

	if ( isDefined( level.dogtags[key] ) )
	{
		// The victim's previous tags are still out: they vanish and move here.
		playFx( level.conf_fx["vanish"], level.dogtags[key].curOrigin );
		level.dogtags[key] notify( "reset" );
	}
	else
	{
		visuals[0] = spawn( "script_model", ( 0, 0, 0 ) );
		visuals[0] setModel( "prop_dogtags_foe" );
		visuals[1] = spawn( "script_model", ( 0, 0, 0 ) );
		visuals[1] setModel( "prop_dogtags_friend" );

		trigger = spawn( "trigger_radius", ( 0, 0, 0 ), 0, 32, 32 );

		tags = maps\mp\gametypes\_gameobjects::createUseObject( "any", trigger, visuals, ( 0, 0, 16 ) );

		// The use object's own team objectives and 2D objpoints are not used:
		// only the killer sees a tag icon.
		_objective_delete( tags.objIDAllies );
		_objective_delete( tags.objIDAxis );
		maps\mp\gametypes\_objpoints::deleteObjPoint( tags.objPoints["allies"] );
		maps\mp\gametypes\_objpoints::deleteObjPoint( tags.objPoints["axis"] );

		tags maps\mp\gametypes\_gameobjects::setUseTime( 0 );
		tags.onUse = ::onUse;
		tags.victim = victim;
		tags.victimTeam = victim.pers["team"];
		tags.key = key;

		level.dogtags[key] = tags;

		level thread clearOnVictimDisconnect( victim );
		victim thread tagTeamUpdater( tags );
	}

	tags = level.dogtags[key];

	pos = victim.origin + ( 0, 0, 14 );
	tags.curOrigin = pos;
	tags.trigger.origin = pos;
	tags.visuals[0].origin = pos;
	tags.visuals[1].origin = pos;
	tags.victimTeam = victim.pers["team"];
	tags.trigger.codolDropTeam = tags.victimTeam;

	tags maps\mp\gametypes\_gameobjects::allowUse( "any" );

	tags.visuals[0] thread showToTeam( tags, getOtherTeam( victim.pers["team"] ) );
	tags.visuals[1] thread showToTeam( tags, victim.pers["team"] );

	tags.attacker = attacker;

	if ( !isDefined( tags.objId ) )
	{
		tags.objId = maps\mp\gametypes\_gameobjects::getNextObjID();
		objective_add( tags.objId, "invisible", ( 0, 0, 0 ) );
		objective_icon( tags.objId, "waypoint_dogtags" );
	}
	objective_position( tags.objId, pos );
	objective_state( tags.objId, "active" );
	objective_player( tags.objId, attacker getEntityNumber() );

	playSoundAtPos( pos, "mp_killconfirm_tags_drop" );

	tags thread bounce();

	updateBotTags();
	debugTags( "drop " + victim.name + " (" + tags.victimTeam + ") killed by " + attacker.name + " at " + pos + " obj " + tags.objId );
}


showToTeam( tags, team )
{
	tags endon( "deleted" );
	tags endon( "reset" );

	self hide();

	foreach ( player in level.players )
	{
		if ( player.team == team )
			self showToPlayer( player );
	}

	for ( ;; )
	{
		level waittill( "joined_team" );

		self hide();

		foreach ( player in level.players )
		{
			if ( player.team == team )
				self showToPlayer( player );

			if ( tags.victimTeam == player.team && isDefined( tags.attacker ) && player == tags.attacker && isDefined( tags.objId ) )
				objective_state( tags.objId, "invisible" );
		}
	}
}


onUse( player )
{
	// Tags owned by the player's own team: a deny (or the victim's own tags).
	if ( player.pers["team"] == self.victimTeam )
	{
		playSoundAtPos( self.curOrigin, "mp_killconfirm_tags_deny" );

		player initPlayerStat( "killsdenied" );
		player incPlayerStat( "killsdenied", 1 );
		player initPersStat( "denied" );
		player incPersStat( "denied", 1 );

		if ( isDefined( self.victim ) && self.victim == player )
		{
			event = "tags_retrieved";
			splash = &"SPLASHES_TAGS_RETRIEVED";
		}
		else
		{
			event = "kill_denied";
			splash = &"SPLASHES_KILL_DENIED";
		}

		if ( isDefined( self.attacker ) )
			self.attacker thread xpEventPopup( &"SPLASHES_DENIED_KILL", ( 1, 0.5, 0.5 ) );

		debugTags( event + " by " + player.name );
	}
	else
	{
		playSoundAtPos( self.curOrigin, "mp_killconfirm_tags_pickup" );

		event = "kill_confirmed";
		splash = &"SPLASHES_KILL_CONFIRMED";

		player initPlayerStat( "killsconfirmed" );
		player incPlayerStat( "killsconfirmed", 1 );
		player initPersStat( "confirmed" );
		player incPersStat( "confirmed", 1 );

		if ( isDefined( self.attacker ) && self.attacker != player )
			self.attacker onPickup( event, splash );

		if ( isDefined( game["voice"][player.team] ) )
			player playLocalSound( game["voice"][player.team] + "killfirm" );

		debugTags( event + " by " + player.name + " (" + player.pers["team"] + ")" );

		player maps\mp\gametypes\_gamescore::giveTeamScoreForObjective( player.pers["team"], 1 );
	}

	player onPickup( event, splash );

	self resetTags();
}


onPickup( event, splash )
{
	self thread xpEventPopup( splash );
	maps\mp\gametypes\_gamescore::givePlayerScore( event, self, undefined );
	self thread maps\mp\gametypes\_rank::giveRankXP( event );
}


resetTags()
{
	self.attacker = undefined;
	self notify( "reset" );
	self.visuals[0] hide();
	self.visuals[1] hide();
	self.curOrigin = ( 0, 0, 1000 );
	self.trigger.origin = ( 0, 0, 1000 );
	self.visuals[0].origin = ( 0, 0, 1000 );
	self.visuals[1].origin = ( 0, 0, 1000 );
	self maps\mp\gametypes\_gameobjects::allowUse( "none" );

	if ( isDefined( self.objId ) )
	{
		_objective_delete( self.objId );
		self.objId = undefined;
	}

	updateBotTags();
}


bounce()
{
	level endon( "game_ended" );
	self endon( "reset" );
	self endon( "deleted" );

	bottomPos = self.curOrigin;
	topPos = self.curOrigin + ( 0, 0, 12 );

	for ( ;; )
	{
		self.visuals[0] moveTo( topPos, 0.5, 0.15, 0.15 );
		self.visuals[0] rotateYaw( 180, 0.5 );
		self.visuals[1] moveTo( topPos, 0.5, 0.15, 0.15 );
		self.visuals[1] rotateYaw( 180, 0.5 );

		wait 0.5;

		self.visuals[0] moveTo( bottomPos, 0.5, 0.15, 0.15 );
		self.visuals[0] rotateYaw( 180, 0.5 );
		self.visuals[1] moveTo( bottomPos, 0.5, 0.15, 0.15 );
		self.visuals[1] rotateYaw( 180, 0.5 );

		wait 0.5;
	}
}


tagTeamUpdater( tags )
{
	level endon( "game_ended" );
	self endon( "disconnect" );

	for ( ;; )
	{
		self waittill( "joined_team" );

		tags.victimTeam = self.pers["team"];
		tags resetTags();
	}
}


clearOnVictimDisconnect( victim )
{
	level endon( "game_ended" );

	key = tagKey( victim );
	victim waittill( "disconnect" );

	if ( !isDefined( level.dogtags[key] ) )
		return;

	tags = level.dogtags[key];
	tags maps\mp\gametypes\_gameobjects::allowUse( "none" );

	if ( isDefined( tags.attacker ) )
		tags.attacker thread xpEventPopup( &"SPLASHES_DENIED_KILL", ( 1, 0.5, 0.5 ) );

	playFx( level.conf_fx["vanish"], tags.curOrigin );
	tags notify( "reset" );

	wait 0.05;

	if ( !isDefined( level.dogtags[key] ) || level.dogtags[key] != tags )
		return;

	if ( isDefined( tags.objId ) )
	{
		_objective_delete( tags.objId );
		tags.objId = undefined;
	}

	tags.trigger delete();
	for ( i = 0; i < tags.visuals.size; i++ )
		tags.visuals[i] delete();

	tags notify( "deleted" );
	level.dogtags[key] = undefined;

	updateBotTags();
}


// The training bots walk to every live tag: _codol_bot_objective::addTagTasks
// reads each entry's origin, entity number and codolDropTeam.
updateBotTags()
{
	live = [];
	foreach ( key in getArrayKeys( level.dogtags ) )
	{
		tags = level.dogtags[key];
		if ( !isDefined( tags ) || !isDefined( tags.trigger ) )
			continue;
		if ( tags.interactTeam == "none" )
			continue;
		live[live.size] = tags.trigger;
	}
	level.codolConfTags = live;
}


debugTags( text )
{
	if ( getDvarInt( "scr_conf_debug" ) > 0 )
		codolPrintLn( "CoDOL conf: " + text + " | score " + game["teamScores"]["allies"] + ":" + game["teamScores"]["axis"] );
}


// IW5 maps\mp\gametypes\_rank::xpEventPopup: a short line of text under the
// score popup. The IW4 _rank.gsc here has no such element, so it is created
// the first time a player needs it.
createXpEventPopup()
{
	hudElem = newClientHudElem( self );
	hudElem.children = [];
	hudElem.horzAlign = "center";
	hudElem.vertAlign = "middle";
	hudElem.alignX = "center";
	hudElem.alignY = "middle";
	hudElem.x = 55;

	if ( level.splitscreen )
		hudElem.y = -20;
	else
		hudElem.y = -35;

	hudElem.font = "hudbig";
	hudElem.fontscale = 0.65;
	hudElem.archived = false;
	hudElem.color = ( 0.5, 0.5, 0.5 );
	hudElem.sort = 10000;
	hudElem.elemType = "msgText";
	hudElem maps\mp\gametypes\_hud::fontPulseInit( 3.0 );
	return hudElem;
}


xpEventPopup( event, hudColor, glowAlpha )
{
	if ( !isDefined( self ) || !isPlayer( self ) )
		return;

	if ( !isDefined( self.hud_xpEventPopup ) )
		self.hud_xpEventPopup = self createXpEventPopup();

	self thread xpEventPopupFinalize( event, hudColor, glowAlpha );
	self thread xpEventPopupTerminate();
}


xpEventPopupFinalize( event, hudColor, glowAlpha )
{
	self endon( "disconnect" );
	self endon( "joined_team" );
	self endon( "joined_spectators" );

	self notify( "xpEventPopup" );
	self endon( "xpEventPopup" );

	if ( level.hardcoreMode )
		return;

	wait ( 0.05 );

	if ( !isDefined( hudColor ) )
		hudColor = ( 1, 1, 0.5 );
	if ( !isDefined( glowAlpha ) )
		glowAlpha = 0;

	if ( !isDefined( self ) )
		return;

	self.hud_xpEventPopup.color = hudColor;
	self.hud_xpEventPopup.glowColor = hudColor;
	self.hud_xpEventPopup.glowAlpha = glowAlpha;
	self.hud_xpEventPopup setText( event );
	self.hud_xpEventPopup.alpha = 0.85;

	wait ( 1.0 );

	if ( !isDefined( self ) )
		return;

	self.hud_xpEventPopup fadeOverTime( 0.75 );
	self.hud_xpEventPopup.alpha = 0;
	self notify( "PopComplete" );
}


xpEventPopupTerminate()
{
	self endon( "disconnect" );
	self endon( "PopComplete" );

	self waittill_any( "joined_team", "joined_spectators" );

	self.hud_xpEventPopup fadeOverTime( 0.05 );
	self.hud_xpEventPopup.alpha = 0;
}
