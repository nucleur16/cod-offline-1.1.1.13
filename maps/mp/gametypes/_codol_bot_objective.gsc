// CoDOL combat-training bots: objective play.
//
// PROVENANCE: ADAPTED. The errand pattern is OpenBLOPS / T5 maps/mp/gametypes/
// _bot.gsc (bot_crate_think, bot_is_idle): pick a target, SetScriptGoal on it,
// wait for "goal" / "bad_path", then PressUseButton for the use time. T5
// training bots only played deathmatch modes, so the per-gametype choice of
// target here is new; it reads the stock IW4 gameobjects (_gameobjects.gsc)
// each gametype already builds (level.domFlags, level.bombZones, level.sdBomb,
// level.teamFlags, level.capZones, level.radioObject, ...). Nothing here moves
// a player, scores, or completes an objective by script: the bot walks there
// through the native waypoint AI and the gameobject's own trigger logic does
// the rest, exactly as for a human.
//
// Mixing with combat: the native AI (codol_bot.c) keeps acquiring and shooting
// enemies while it walks to a script goal. Bots share objectives out (each
// claimant makes a target less attractive to teammates, and some targets cap
// their claimants), and only scr_codol_bot_objective percent of bots (default
// 70) make objectives their focus; the rest roam and fight, taking only
// urgent errands (a defuse, a dropped flag, the bomb they carry) or ones close
// by. Planting waits while the bot sees an enemy (T5 bot_is_idle); a defuse
// does not.
//
// Hooked from _codol_bots::watchSpawns. Developer trace: set
// scr_codol_bot_objective_debug 1 (developer > 0).
#include maps\mp\_utility;
#include common_scripts\utility;

objectiveThink()
{
	self endon( "death" );
	self endon( "disconnect" );
	level endon( "game_ended" );
	self notify( "codol_objective_think" );
	self endon( "codol_objective_think" );

	if ( !isObjectiveGametype() )
		return;
	if ( getDvar( "scr_codol_bot_objective" ) == "" )
		setDvar( "scr_codol_bot_objective", 70 );

	self.codolObjKey = undefined;
	self.codolObjGoal = undefined;
	self.codolObjState = undefined;
	self.codolObjBad = [];
	self.codolObjFocus = randomInt( 100 ) < getDvarInt( "scr_codol_bot_objective" );
	if ( !isDefined( level.codolObjStatusStarted ) && getDvarInt( "developer" ) > 0 && getDvarInt( "scr_codol_bot_objective_debug" ) > 0 )
	{
		level.codolObjStatusStarted = true;
		level thread debugStatus();
	}

	// Spread the first decisions of a team that spawned together.
	wait ( 0.5 + randomFloat( 1.5 ) );

	for ( ;; )
	{
		if ( getDvarInt( "scr_codol_bot_objective" ) <= 0 || self objectivePaused() )
		{
			self releaseObjective();
			wait 1.5;
			continue;
		}
		task = self pickTask();
		if ( !isDefined( task ) )
		{
			self releaseObjective();
			wait 1.5;
			continue;
		}
		if ( !isDefined( self.codolObjKey ) || self.codolObjKey != task.key )
			self startTask( task );
		self stepTask( task );
	}
}

isObjectiveGametype()
{
	switch ( level.gametype )
	{
		case "dom":
		case "koth":
		case "sd":
		case "sab":
		case "dd":
		case "ctf":
		case "oneflag":
		case "conf":
		case "vip":
		case "gtnw":
		case "arena":
			return true;
	}
	return false;
}

// Rides, carried rewards and last stand own the bot; so does the end of a
// round. Leave the script goal alone until they are done.
objectivePaused()
{
	if ( isDefined( level.gameEnded ) && level.gameEnded )
		return true;
	if ( isDefined( self.usingRemote ) )
		return true;
	// _codol_bot_killstreaks owns the bot's script goal while it fetches a
	// care package (self.codolBotTask = "crate").
	if ( isDefined( self.codolBotTask ) )
		return true;
	if ( isDefined( self.lastStand ) )
		return true;
	if ( isDefined( self.codolRewardActivationBusy ) && self.codolRewardActivationBusy )
		return true;
	return false;
}

releaseObjective()
{
	// Only clear a goal this script set: other errands own theirs.
	if ( isDefined( self.codolObjGoal ) && self hasScriptGoal() )
		self clearScriptGoal();
	self.codolObjGoal = undefined;
	self.codolObjKey = undefined;
	self.codolObjState = undefined;
}

debugTask( text )
{
	if ( getDvarInt( "developer" ) > 0 && getDvarInt( "scr_codol_bot_objective_debug" ) > 0 )
		codolPrintLn( "CoDOL bot objective: " + self.name + " (" + self.pers["team"] + ") " + text );
}

// Developer trace of the objective state every five seconds.
debugStatus()
{
	level endon( "game_ended" );
	for ( ;; )
	{
		wait 5;
		line = level.gametype;
		if ( isDefined( level.domFlags ) )
			foreach ( flag in level.domFlags )
				line = line + " flag" + flag.label + "=" + flag.ownerTeam + "/" + flag.claimTeam;
		if ( isDefined( level.radioObject ) )
			line = line + " hq=" + level.radioObject.ownerTeam + "/" + level.radioObject.interactTeam;
		if ( isDefined( level.bombPlanted ) )
			line = line + " planted=" + level.bombPlanted;
		foreach ( name in strTok( "sdBomb sabBomb", " " ) )
			line = line + debugCarrier( name );
		if ( isDefined( level.teamFlags ) )
			foreach ( team, flag in level.teamFlags )
				line = line + " " + team + "Flag=" + debugCarryState( flag );
		if ( isDefined( level.codolConfTags ) )
			line = line + " tags=" + level.codolConfTags.size;
		line = line + " score " + game["teamScores"]["allies"] + ":" + game["teamScores"]["axis"];
		claims = "";
		foreach ( player in level.players )
			if ( isDefined( player.codolObjKey ) )
				claims = claims + " " + player getEntityNumber() + ":" + player.codolObjKey;
		codolPrintLn( "CoDOL bot objective status: " + line + " |" + claims );
	}
}

debugCarrier( name )
{
	obj = undefined;
	if ( name == "sdBomb" )
		obj = level.sdBomb;
	else if ( name == "sabBomb" )
		obj = level.sabBomb;
	if ( !isDefined( obj ) )
		return "";
	return " " + name + "=" + debugCarryState( obj );
}

debugCarryState( obj )
{
	if ( isDefined( obj.carrier ) )
		return "carried:" + obj.carrier.name;
	if ( obj maps\mp\gametypes\_gameobjects::isHome() )
		return "home";
	return "dropped";
}

// ---------------------------------------------------------------------------
// Choosing
// ---------------------------------------------------------------------------

newTask( key, kind, goal, radius, weight )
{
	task = spawnStruct();
	task.key = key;
	task.kind = kind;       // use | touch | defend | escort | hunt | collect
	task.goal = goal;
	task.radius = radius;
	task.weight = weight;
	task.crowd = 1.0;       // score divisor per teammate already on it
	task.cap = 99;          // most teammates that may share it
	task.urgent = false;    // taken even by bots without objective focus
	task.forceUse = false;  // hold use even with an enemy in sight
	task.useTime = 0;
	return task;
}

pickTask()
{
	team = self.pers["team"];
	if ( !isDefined( team ) || ( team != "allies" && team != "axis" ) )
		return undefined;

	tasks = self collectTasks( team );
	best = undefined;
	bestScore = 0;
	now = getTime();
	foreach ( task in tasks )
	{
		if ( isDefined( self.codolObjBad[task.key] ) && self.codolObjBad[task.key] > now )
			continue;
		dist = distance( self.origin, task.goal );
		if ( !self.codolObjFocus && !task.urgent && dist > 900 )
			continue;
		current = isDefined( self.codolObjKey ) && self.codolObjKey == task.key;
		claims = countClaims( task.key, team );
		if ( claims >= task.cap && !current )
			continue;
		score = task.weight * 1500 / ( 1500 + dist );
		score = score / ( 1 + claims * task.crowd );
		score = score * ( 0.85 + randomFloat( 0.3 ) );
		if ( current )
			score = score * 1.6;
		if ( score > bestScore )
		{
			best = task;
			bestScore = score;
		}
	}
	return best;
}

countClaims( key, team )
{
	count = 0;
	foreach ( player in level.players )
	{
		if ( player == self || !isDefined( player.codolObjKey ) || player.codolObjKey != key )
			continue;
		if ( !isAlive( player ) || !isDefined( player.pers["team"] ) || player.pers["team"] != team )
			continue;
		count++;
	}
	return count;
}

collectTasks( team )
{
	tasks = [];
	switch ( level.gametype )
	{
		case "dom":
			if ( isDefined( level.domFlags ) )
				foreach ( flag in level.domFlags )
					tasks = self addObjectTasks( tasks, flag, team );
			break;
		case "arena":
			tasks = self addObjectTasks( tasks, level.arenaFlag, team );
			break;
		case "koth":
			tasks = self addObjectTasks( tasks, level.radioObject, team );
			break;
		case "gtnw":
			tasks = self addHoldZoneTasks( tasks, level.nukeSite, team );
			break;
		case "sd":
			tasks = self addSdTasks( tasks, team );
			break;
		case "sab":
		case "dd":
			tasks = self addBombTasks( tasks, team );
			break;
		case "ctf":
		case "oneflag":
			tasks = self addFlagTasks( tasks, team );
			break;
		case "vip":
			tasks = self addVipTasks( tasks, team );
			break;
		case "conf":
			tasks = self addTagTasks( tasks, team );
			break;
	}
	return tasks;
}

addObjectTasks( tasks, obj, team )
{
	if ( !isDefined( obj ) || !isDefined( obj.type ) || !isDefined( obj.trigger ) )
		return tasks;
	if ( obj.type == "carryObject" )
		return self addCarryTasks( tasks, obj, team );
	if ( obj.type == "useObject" )
		return self addUseTasks( tasks, obj, team );
	return tasks;
}

objectOrigin( obj )
{
	origin = obj.trigger.origin;
	if ( origin == ( 0, 0, 0 ) && isDefined( obj.visuals ) && isDefined( obj.visuals[0] ) )
		origin = obj.visuals[0].origin;
	return origin;
}

isDefuseObject( obj )
{
	return isDefined( obj.useWeapon ) && obj.useWeapon == "briefcase_bomb_defuse_mp";
}

addUseTasks( tasks, obj, team )
{
	origin = self objectOrigin( obj );
	if ( obj.interactTeam == "none" )
	{
		// Headquarters between "located" and "online": gather on it.
		if ( level.gametype == "koth" )
		{
			task = newTask( "d" + obj.entNum, "defend", origin, 400, 5 );
			task.anchor = origin;
			task.crowd = 0.6;
			tasks[tasks.size] = task;
		}
		return tasks;
	}

	if ( obj maps\mp\gametypes\_gameobjects::canInteractWith( team ) )
	{
		if ( isDefined( obj.keyObject ) )
		{
			// Plant / capture-the-flag delivery: only the carrier can.
			if ( isDefined( self.carryObject ) && self.carryObject == obj.keyObject )
			{
				task = self newUseTask( "u" + obj.entNum, obj, origin, 60 );
				task.urgent = true;
				tasks[tasks.size] = task;
			}
			return tasks;
		}
		if ( isDefuseObject( obj ) )
		{
			task = self newUseTask( "u" + obj.entNum, obj, origin, 40 );
			task.urgent = true;
			task.forceUse = true;
			task.cap = 2;
			task.crowd = 3;
		}
		else if ( level.gametype == "koth" )
			task = self newUseTask( "u" + obj.entNum, obj, origin, 9 );
		else
			task = self newUseTask( "u" + obj.entNum, obj, origin, 6 );
		tasks[tasks.size] = task;
		return tasks;
	}

	if ( obj.ownerTeam == team || isDefuseObject( obj ) )
	{
		// Our flag / our headquarters / our bomb site, or the bomb we planted.
		// A held headquarters is only safe while we contest it from inside.
		weight = 2.5;
		radius = 450;
		if ( level.gametype == "koth" )
		{
			weight = 6;
			radius = 200;
		}
		else if ( isDefuseObject( obj ) )
			weight = 8;
		task = newTask( "d" + obj.entNum, "defend", origin, radius, weight );
		task.anchor = origin;
		task.crowd = 1.5;
		task.cap = 3;
		tasks[tasks.size] = task;
	}
	return tasks;
}

// A use (button) trigger or a proximity (stand in it) trigger.
newUseTask( key, obj, origin, weight )
{
	kind = "touch";
	if ( isDefined( obj.triggerType ) && obj.triggerType == "use" )
		kind = "use";
	task = newTask( key, kind, origin, 24, weight );
	task.obj = obj;
	task.trigger = obj.trigger;
	if ( isDefined( obj.useTime ) )
		task.useTime = obj.useTime;
	if ( kind == "use" )
	{
		task.crowd = 3;
		task.cap = 2;
	}
	return task;
}

addCarryTasks( tasks, obj, team )
{
	if ( isDefined( obj.carrier ) )
	{
		carrier = obj.carrier;
		if ( carrier == self || !isAlive( carrier ) || !isDefined( carrier.pers["team"] ) )
			return tasks;
		if ( carrier.pers["team"] == team )
		{
			task = newTask( "e" + obj.entNum, "escort", carrier.origin, 350, 3 );
			task.anchor = carrier.origin;
			task.crowd = 1.5;
			task.cap = 2;
		}
		else
		{
			task = newTask( "h" + obj.entNum, "hunt", carrier.origin, 150, 9 );
			task.crowd = 0.6;
			if ( obj.ownerTeam == team )
				task.urgent = true;   // they have our flag
		}
		tasks[tasks.size] = task;
		return tasks;
	}

	if ( obj maps\mp\gametypes\_gameobjects::canInteractWith( team ) )
	{
		if ( isDefined( self.carryObject ) || ( isDefined( self.canPickupObject ) && !self.canPickupObject ) )
			return tasks;
		weight = 8;
		if ( obj.ownerTeam == team && level.gametype != "sd" )
			weight = 14;  // return our dropped flag
		task = self newUseTask( "c" + obj.entNum, obj, obj.curOrigin, weight );
		task.cap = 2;
		task.crowd = 2;
		if ( obj.ownerTeam == team || !obj maps\mp\gametypes\_gameobjects::isHome() )
			task.urgent = true;
		tasks[tasks.size] = task;
		return tasks;
	}

	if ( obj.ownerTeam == team && obj maps\mp\gametypes\_gameobjects::isHome() )
	{
		task = newTask( "d" + obj.entNum, "defend", obj.curOrigin, 450, 2.5 );
		task.anchor = obj.curOrigin;
		task.crowd = 1.5;
		task.cap = 3;
		tasks[tasks.size] = task;
	}
	return tasks;
}

// Global Thermonuclear War: a team scores while it has more players inside
// the zone than the other, so both sides stand in it rather than near it.
addHoldZoneTasks( tasks, obj, team )
{
	if ( !isDefined( obj ) || !isDefined( obj.trigger ) || obj.interactTeam == "none" )
		return tasks;
	task = newTask( "u" + obj.entNum, "touch", self objectOrigin( obj ), 24, 8 );
	task.obj = obj;
	task.trigger = obj.trigger;
	task.crowd = 0.4;
	tasks[tasks.size] = task;
	return tasks;
}

// Search and Destroy. The defuse object a plant creates is a local of
// sd::bombPlanted; its trigger is the planted site's bombDefuseTrig, raised
// from -10000 to the bomb model.
addSdTasks( tasks, team )
{
	if ( isDefined( level.bombPlanted ) && level.bombPlanted )
	{
		if ( isDefined( level.bombDefused ) && level.bombDefused )
			return tasks;
		trigger = undefined;
		if ( isDefined( level.bombZones ) )
			foreach ( zone in level.bombZones )
				if ( isDefined( zone.bombDefuseTrig ) && zone.bombDefuseTrig.origin[2] > -5000 )
					trigger = zone.bombDefuseTrig;
		if ( !isDefined( trigger ) )
			return tasks;
		if ( team == game["defenders"] )
		{
			task = newTask( "sdd", "use", trigger.origin, 24, 40 );
			task.trigger = trigger;
			task.useTime = int( level.defuseTime * 1000 );
			task.urgent = true;
			task.forceUse = true;
			task.cap = 2;
			task.crowd = 3;
			task.sdDefuse = true;
			task.useWeapon = "briefcase_bomb_defuse_mp";
		}
		else
		{
			task = newTask( "sdg", "defend", trigger.origin, 500, 8 );
			task.anchor = trigger.origin;
			task.crowd = 0.5;
		}
		tasks[tasks.size] = task;
		return tasks;
	}
	tasks = self addObjectTasks( tasks, level.sdBomb, team );
	if ( isDefined( level.bombZones ) )
		foreach ( zone in level.bombZones )
			tasks = self addObjectTasks( tasks, zone, team );
	return tasks;
}

// Sabotage and Demolition: the bomb site turns itself into the defuse object
// (setUpForDefusing), so the generic use rules cover plant and defuse.
addBombTasks( tasks, team )
{
	tasks = self addObjectTasks( tasks, level.sabBomb, team );
	tasks = self addObjectTasks( tasks, level.ddBomb, team );
	if ( isDefined( level.bombZones ) )
		foreach ( zone in level.bombZones )
			tasks = self addObjectTasks( tasks, zone, team );
	return tasks;
}

// Capture the Flag / One Flag. A carrier whose capture zone is closed (our
// own flag is away) waits at home for it to come back.
addFlagTasks( tasks, team )
{
	if ( isDefined( level.teamFlags ) )
		foreach ( flag in level.teamFlags )
			tasks = self addObjectTasks( tasks, flag, team );
	if ( isDefined( level.capZones ) )
		foreach ( zone in level.capZones )
			tasks = self addObjectTasks( tasks, zone, team );
	if ( isDefined( self.carryObject ) && isDefined( level.capZones ) && isDefined( level.capZones[team] ) )
	{
		zone = level.capZones[team];
		if ( isDefined( zone.trigger ) && zone.interactTeam == "none" )
		{
			task = newTask( "hold", "defend", self objectOrigin( zone ), 300, 30 );
			task.anchor = task.goal;
			task.urgent = true;
			tasks[tasks.size] = task;
		}
	}
	return tasks;
}

// VIP: the VIP walks to the extraction zone and teammates escort him; the
// attackers hunt him and watch the zone.
addVipTasks( tasks, team )
{
	zone = level.extractionZone;
	if ( !isDefined( zone ) || !isDefined( zone.type ) || !isDefined( zone.trigger ) )
		return tasks;
	vip = undefined;
	foreach ( player in level.players )
		if ( isAlive( player ) && isDefined( player.isVip ) && player.isVip )
			vip = player;
	origin = self objectOrigin( zone );
	if ( team == game["defenders"] )
	{
		if ( isDefined( self.isVip ) && self.isVip )
		{
			task = self newUseTask( "vip", zone, origin, 60 );
			task.urgent = true;
			tasks[tasks.size] = task;
			return tasks;
		}
		if ( isDefined( vip ) )
		{
			task = newTask( "e-vip", "escort", vip.origin, 350, 6 );
			task.anchor = vip.origin;
			task.crowd = 0.8;
			tasks[tasks.size] = task;
		}
		task = newTask( "d-vip", "defend", origin, 450, 3 );
		task.anchor = origin;
		task.crowd = 1.5;
		tasks[tasks.size] = task;
		return tasks;
	}
	if ( isDefined( vip ) )
	{
		task = newTask( "h-vip", "hunt", vip.origin, 150, 8 );
		task.crowd = 0.5;
		tasks[tasks.size] = task;
	}
	task = newTask( "d-vip", "defend", origin, 500, 3 );
	task.anchor = origin;
	task.crowd = 1.5;
	tasks[tasks.size] = task;
	return tasks;
}

// Kill Confirmed: walk over dropped tags (conf::dropTag records them in
// level.codolConfTags). Enemy tags confirm, ours deny.
addTagTasks( tasks, team )
{
	if ( !isDefined( level.codolConfTags ) )
		return tasks;
	live = [];
	foreach ( tag in level.codolConfTags )
		if ( isDefined( tag ) )
			live[live.size] = tag;
	level.codolConfTags = live;
	foreach ( tag in live )
	{
		if ( distanceSquared( self.origin, tag.origin ) > 2000 * 2000 )
			continue;
		weight = 7;
		if ( isDefined( tag.codolDropTeam ) && tag.codolDropTeam == team )
			weight = 5;
		task = newTask( "t" + tag getEntityNumber(), "collect", tag.origin, 24, weight );
		task.cap = 1;
		task.crowd = 4;
		tasks[tasks.size] = task;
	}
	return tasks;
}

// ---------------------------------------------------------------------------
// Doing
// ---------------------------------------------------------------------------

startTask( task )
{
	self.codolObjKey = task.key;
	state = spawnStruct();
	state.tight = 0;
	state.nextPatrol = 0;
	state.patrol = undefined;
	state.tightGoal = undefined;
	state.holdGoal = undefined;
	state.lastGoal = undefined;
	state.lastRadius = 0;
	self.codolObjState = state;
	self debugTask( task.kind + " " + task.key + " at " + task.goal );
}

stepTask( task )
{
	state = self.codolObjState;
	goal = task.goal;
	radius = task.radius;
	if ( isDefined( task.anchor ) && isDefined( state.patrol ) )
	{
		// An escorted carrier moved on: follow him instead of the old spot.
		if ( distanceSquared( state.patrol, task.anchor ) > ( task.radius + 64 ) * ( task.radius + 64 ) )
			state.patrol = undefined;
		else
		{
			goal = state.patrol;
			radius = 48;
		}
	}
	if ( state.tight > 0 && isDefined( state.tightGoal ) )
	{
		goal = state.tightGoal;
		radius = 16;
	}
	// Inside the trigger: stop where we stand (T5 bot_crate_think re-goals on
	// self.origin), rather than shouldering teammates for its exact centre.
	touching = isDefined( task.trigger ) && ( task.kind == "use" || task.kind == "touch" ) && self isTouching( task.trigger );
	if ( touching )
	{
		if ( !isDefined( state.holdGoal ) )
			state.holdGoal = self.origin;
		goal = state.holdGoal;
		radius = 64;
	}
	else
		state.holdGoal = undefined;

	if ( !self hasScriptGoal() || !isDefined( state.lastGoal ) || state.lastRadius != radius ||
		distanceSquared( state.lastGoal, goal ) > 48 * 48 )
	{
		self setScriptGoal( goal, radius );
		self.codolObjGoal = goal;
		state.lastGoal = goal;
		state.lastRadius = radius;
	}

	if ( touching )
	{
		state.tight = 0;
		state.tightGoal = undefined;
		if ( task.kind == "use" )
			self doUse( task );
		else
			wait 0.5;
		return;
	}

	result = self waittill_any_timeout( 1.0, "goal", "bad_path" );
	if ( isDefined( result ) && result == "bad_path" )
	{
		if ( isDefined( state.patrol ) )
		{
			state.patrol = undefined;
			state.nextPatrol = getTime() + 2000;
		}
		else
		{
			self debugTask( "bad path to " + task.key );
			self.codolObjBad[task.key] = getTime() + 12000;
			self.codolObjGoal = undefined;
			self.codolObjKey = undefined;
		}
		return;
	}

	if ( distanceSquared( self.origin, goal ) > ( radius + 32 ) * ( radius + 32 ) )
		return;

	switch ( task.kind )
	{
		case "use":
		case "touch":
			// Arrived, yet not inside the trigger.
			if ( isDefined( task.trigger ) )
				self tighten( task );
			else
				wait 0.25;
			break;
		case "defend":
		case "escort":
			if ( getTime() >= state.nextPatrol )
			{
				state.patrol = codol_botNavPointNear( task.anchor, task.radius );
				state.nextPatrol = getTime() + 4000 + randomInt( 6000 );
			}
			wait 0.5;
			break;
		default:
			wait 0.25;
			break;
	}
}

// At the objective but not inside its trigger: walk onto the trigger origin,
// then onto its model, then give up on it for a while.
tighten( task )
{
	state = self.codolObjState;
	state.tight++;
	if ( state.tight == 1 && isDefined( task.trigger ) )
		state.tightGoal = task.trigger.origin;
	else if ( state.tight == 2 && isDefined( task.obj ) && isDefined( task.obj.visuals ) && isDefined( task.obj.visuals[0] ) )
		state.tightGoal = task.obj.visuals[0].origin;
	else if ( state.tight >= 3 )
	{
		self debugTask( "cannot reach the trigger of " + task.key );
		self.codolObjBad[task.key] = getTime() + 15000;
		self.codolObjKey = undefined;
	}
}

taskStillValid( task )
{
	if ( isDefined( task.sdDefuse ) )
		return isDefined( level.bombPlanted ) && level.bombPlanted && !( isDefined( level.bombDefused ) && level.bombDefused );
	obj = task.obj;
	if ( !isDefined( obj ) )
		return true;
	if ( obj.interactTeam == "none" || !obj maps\mp\gametypes\_gameobjects::canInteractWith( self.pers["team"] ) )
		return false;
	if ( obj.type == "carryObject" )
		return !isDefined( obj.carrier );
	if ( isDefined( obj.keyObject ) )
		return isDefined( self.carryObject ) && self.carryObject == obj.keyObject;
	return true;
}

// _gameobjects::useHoldThink gives the bot the object's use weapon (the
// briefcase) and switchToWeapon()s it, then counts progress only while it is
// held. A bot has no client naming that weapon in its usercmd and the scripted
// switch does not complete for it, so raise it the way _codol_bots
// pressRewardSlot and _codol_bot_killstreaks do: setSpawnWeapon, once the
// gameobject has claimed this bot (claimTrigger) and handed it the weapon.
raiseUseWeapon( task )
{
	useWeapon = task.useWeapon;
	if ( isDefined( task.obj ) && isDefined( task.obj.useWeapon ) )
		useWeapon = task.obj.useWeapon;
	if ( !isDefined( useWeapon ) || !isDefined( self.claimTrigger ) || !isDefined( task.trigger ) || self.claimTrigger != task.trigger )
		return;
	if ( self getCurrentWeapon() != useWeapon && self hasWeapon( useWeapon ) )
		self setSpawnWeapon( useWeapon );
}

// The gameobject switches back to the weapon held before the use
// (lastNonUseWeapon); for a bot that scripted switch does not complete either,
// and _gameobjects::takeUseWeapon only removes the briefcase once it is no
// longer held. Raise the previous weapon the same way.
lowerUseWeapon()
{
	self endon( "death" );
	self endon( "disconnect" );
	wait 0.5;
	current = self getCurrentWeapon();
	if ( current != "briefcase_bomb_mp" && current != "briefcase_bomb_defuse_mp" )
		return;
	if ( isDefined( self.lastNonUseWeapon ) && self.lastNonUseWeapon != "none" && self hasWeapon( self.lastNonUseWeapon ) )
		self setSpawnWeapon( self.lastNonUseWeapon );
}

// Developer trace of what the gameobject use loop gates on.
debugUse( task )
{
	if ( getDvarInt( "developer" ) <= 0 || getDvarInt( "scr_codol_bot_objective_debug" ) < 2 )
		return;
	progress = "-";
	if ( isDefined( task.obj ) && isDefined( task.obj.curProgress ) )
		progress = "" + task.obj.curProgress;
	self debugTask( "use " + task.key + " claim " + isDefined( self.claimTrigger ) + " usePressed " + self useButtonPressed() +
		" touching " + self isTouching( task.trigger ) + " ground " + self isOnGround() + " weaponEnabled " + self isWeaponEnabled() +
		" progress " + progress + " weapon " + self getCurrentWeapon() );
}

// T5 bot_crate_think: hold use for the use time plus the weapon switch the
// gameobject waits for (useHoldThinkLoop allows 1.5 s), standing still.
doUse( task )
{
	if ( !task.forceUse && isDefined( self getThreat() ) )
	{
		wait 0.5;   // fight first
		return;
	}
	scaler = 1.0;
	if ( isDefined( self.objectiveScaler ) && self.objectiveScaler > 0 )
		scaler = self.objectiveScaler;
	seconds = task.useTime / 1000.0 / scaler + 1.5;
	if ( seconds > 30 )
		seconds = 30;
	self debugTask( "holds use on " + task.key + " for " + seconds + "s" );
	health = self.health;
	self pressUseButton( seconds );
	endTime = getTime() + int( seconds * 1000 );
	while ( getTime() < endTime )
	{
		wait 0.25;
		self raiseUseWeapon( task );
		self debugUse( task );
		if ( !self taskStillValid( task ) )
			break;
		// Under fire with the enemy in sight: half the time stop and fight.
		if ( self.health < health && !task.forceUse && isDefined( self getThreat() ) )
		{
			if ( randomInt( 100 ) < 50 )
				break;
			health = self.health;
		}
	}
	self pressUseButton( 0 );
	self thread lowerUseWeapon();
	self debugTask( "released use on " + task.key + ", still open " + self taskStillValid( task ) + ", weapon " + self getCurrentWeapon() );
	wait 0.3;
}
