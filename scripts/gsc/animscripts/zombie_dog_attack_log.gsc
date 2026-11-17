// zm hellhound animscript: animscripts/zombie_dog_attack_log.gsc
//
// Ported from this tree's multiplayer dog animscript
// maps/mp/animscripts/dog_attack_log.gsc (itself adapted from the shipped T5
// maps\mp\animscripts\dog_attack_log.gsc, header kept below).  The zm hellhound
// (species "zombie_dog", codol_actor_scripts.cpp) runs the same native T5 dog
// state machine bound against C1's animtrees/zombie_dog.atr, and every state
// is selected by name through setanimstate(), so the bodies are the MP
// dog's.  Changed for zm, the same way T5 forks animscripts\dog_* into
// animscripts\zombie_dog_*:
//   - module paths maps\mp\animscripts\dog_* -> ...\zombie_dog_*, so the zm
//     category compiles no multiplayer module (see _zombiemode.gsc);
//   - no maps\mp\_utility and no maps\mp\_dogs reference;
//   - nothing else changed.

/*
	maps/mp/animscripts/dog_attack_log.gsc

	CoDOL developer-only attack log for the MP attack dogs.  Nothing here
	changes behaviour: every entry point returns at once unless the server
	runs with developer 1 AND scr_dogAttackLog 1.

	One line per attack attempt ("CoDOL dogattack:"), written when the
	attempt ends, carrying what dog_combat decided and what came of it:
	  dist/dist2d/dz   dog -> target at decision time (prepareAttackPlayer)
	  yaw              dog facing vs. direction to the target (degrees)
	  speed/stance     the target's horizontal speed and stance then
	  trace            the 65/32 jump trace use_low_attack() makes
	  anim             the attack state chosen (run / close_range)
	  tele             the gap attackTeleportThread closes (0 = none)
	  note             ms from setanimstate to stop_tracking / dog_melee
	  mdist/myaw       dog -> target and facing error at the melee() call
	  hit/hp           what melee() returned, target health before/after
	  result           hit, kill, miss, nomelee (no dog_melee notetrack in
	                   the attack window) or interrupted (killanimscript)

	A per-dog watcher (watch) also writes "CoDOL dogapproach:" lines: an
	episode starts when the dog is inside bite range of its enemy (T5
	Actor_Dog_PointNearAttackPoint: meleeAttackDist + 15, 80 high) and ends
	with the latency to the next attempt, or "pass" when the dog leaves
	the range again without attacking.
*/

enabled()
{
	return getDvarInt( "developer" ) > 0 && getDvarInt( "scr_dogAttackLog" ) > 0;
}

yaw_error( target )
{
	toTarget = target - self.origin;
	if ( toTarget[0] == 0 && toTarget[1] == 0 )
		return 0;
	delta = vectortoangles( toTarget )[1] - self.angles[1];
	while ( delta > 180 )
		delta -= 360;
	while ( delta < -180 )
		delta += 360;
	return int( delta );
}

horizontal_speed( ent )
{
	if ( !isPlayer( ent ) )
		return 0;
	v = ent getVelocity();
	return int( length( ( v[0], v[1], 0 ) ) );
}

// Called from prepareAttackPlayer, after the decision values are known.
begin( player, lowAttack, traceClear, teleport )
{
	if ( !enabled() )
		return;

	if ( !isDefined( level.dogAttackLogId ) )
		level.dogAttackLogId = 0;
	level.dogAttackLogId++;

	self finish( "superseded" );

	self.dal = spawnstruct();
	self.dal.id = level.dogAttackLogId;
	self.dal.start = getTime();
	self.dal.target = player;
	self.dal.dogStart = self.origin;
	self.dal.targetStart = player.origin;
	self.dal.text = "dist " + int( distance( self.origin, player.origin ) )
		+ " dist2d " + int( distance2d( self.origin, player.origin ) )
		+ " dz " + int( player.origin[2] - self.origin[2] )
		+ " yaw " + self yaw_error( player.origin )
		+ " speed " + horizontal_speed( player )
		+ " stance " + player getStance()
		+ " trace " + traceClear
		+ " low " + lowAttack
		+ " tele " + int( teleport )
		+ " hp " + player.health;
	self.dal.animName = "?";
	self.dal.notes = "";
	self.dal.melee = "";

	self thread interrupted_watch( self.dal );
}

anim_chosen( name )
{
	if ( !enabled() || !isDefined( self.dal ) )
		return;
	self.dal.animName = name;
	self.dal.animTime = getTime();
}

note( name )
{
	if ( !enabled() || !isDefined( self.dal ) )
		return;
	start = self.dal.start;
	if ( isDefined( self.dal.animTime ) )
		start = self.dal.animTime;
	self.dal.notes = self.dal.notes + " " + name + "@" + ( getTime() - start );
}

// One attackTeleportThread step: applied or refused (seen, > 10 units).
teleport_step( applied )
{
	if ( !enabled() || !isDefined( self.dal ) )
		return;
	if ( applied )
		self.dal.notes = self.dal.notes + " tp";
	else
		self.dal.notes = self.dal.notes + " tp-refused";
}

// Before the melee() call: where the target is, how the dog faces it.
melee_before( player )
{
	if ( !enabled() || !isDefined( self.dal ) || !isDefined( player ) )
		return;
	self.dal.hpBefore = player.health;
	self.dal.melee = " mdist " + int( distance( self.origin, player.origin ) )
		+ " mdist2d " + int( distance2d( self.origin, player.origin ) )
		+ " myaw " + self yaw_error( player.origin )
		+ " mspeed " + horizontal_speed( player )
		+ " dogmoved " + int( distance2d( self.dal.dogStart, self.origin ) )
		+ " tgtmoved " + int( distance2d( self.dal.targetStart, player.origin ) );
	eye = self gettagorigin( "tag_eye" );
	toEye = eye - self.origin;
	self.dal.melee = self.dal.melee + " eyefwd " + int( vectordot( toEye, anglestoforward( self.angles ) ) )
		+ " eyez " + int( toEye[2] )
		+ " eye2tgt " + int( distance2d( eye, player.origin ) );
}

melee_after( player, hitEnt )
{
	if ( !enabled() || !isDefined( self.dal ) )
		return;
	hit = "none";
	if ( isDefined( hitEnt ) )
	{
		if ( isDefined( player ) && hitEnt == player )
			hit = "target";
		else
			hit = "other";
	}
	hpAfter = -1;
	if ( isDefined( player ) )
		hpAfter = player.health;
	self.dal.melee = self.dal.melee + " hit " + hit + " hp " + self.dal.hpBefore + "->" + hpAfter;
	if ( hit == "target" )
	{
		if ( !isDefined( player ) || !isAlive( player ) || hpAfter <= 0 )
			self.dal.result = "kill";
		else
			self.dal.result = "hit";
	}
	else
		self.dal.result = "miss";
}

finish( outcome )
{
	if ( !isDefined( self.dal ) )
		return;
	dal = self.dal;
	self.dal = undefined;
	dal notify( "dal_done" );
	if ( isDefined( dal.result ) )
		outcome = dal.result;
	print( "CoDOL dogattack: id " + dal.id + " dog " + self getEntityNumber() + " t " + dal.start
		+ " " + dal.text + " anim " + dal.animName + " notes" + dal.notes + dal.melee
		+ " ms " + ( getTime() - dal.start ) + " result " + outcome + "\n" );
}

// Called once the attack window closes (DoNoteTracksForTime returned).
end()
{
	if ( !enabled() )
		return;
	self finish( "nomelee" );
}

interrupted_watch( dal )
{
	dal endon( "dal_done" );
	self common_scripts\utility::waittill_any( "killanimscript", "death" );
	if ( isDefined( self ) && isDefined( self.dal ) && self.dal == dal )
		self finish( "interrupted" );
}

// Per-dog approach watcher (threaded from _dogs.gsc on spawn).
watch()
{
	if ( !enabled() )
		return;
	self endon( "death" );

	inRange = false;
	since = 0;
	lastId = 0;
	sampled = 0;
	for ( ;; )
	{
		wait 0.05;
		enemy = self.enemy;
		if ( !isDefined( enemy ) || !isAlive( enemy ) || !isDefined( self.meleeAttackDist ) )
		{
			inRange = false;
			continue;
		}
		reach = self.meleeAttackDist + 15;
		dz = enemy.origin[2] - self.origin[2];
		near = dz * dz <= 6400 && distance2d( self.origin, enemy.origin ) <= reach;
		attempting = isDefined( self.dal );
		if ( attempting )
		{
			if ( inRange && self.dal.id != lastId )
				print( "CoDOL dogapproach: dog " + self getEntityNumber() + " attacked after " + ( getTime() - since ) + " ms in range\n" );
			lastId = self.dal.id;
			inRange = false;
			continue;
		}
		if ( near && !inRange )
		{
			inRange = true;
			since = getTime();
			sampled = since;
		}
		else if ( near && getTime() - sampled >= 500 )
		{
			sampled = getTime();
			print( "CoDOL dogapproach: dog " + self getEntityNumber() + " in range, no attack, " + ( sampled - since ) + " ms: d2d " + int( distance2d( self.origin, enemy.origin ) ) + " dz " + int( dz ) + " yaw " + self yaw_error( enemy.origin ) + " tgtspeed " + horizontal_speed( enemy ) + "\n" );
		}
		else if ( !near && inRange && distance2d( self.origin, enemy.origin ) > reach + 60 )
		{
			inRange = false;
			print( "CoDOL dogapproach: dog " + self getEntityNumber() + " pass after " + ( getTime() - since ) + " ms in range, now " + int( distance2d( self.origin, enemy.origin ) ) + " away\n" );
		}
	}
}

// Developer verbs (developer 1 only), `rcon set scr_dogAttackTest "<verb>"`:
//   "freeze 1" / "freeze 0"   freeze or release every live player who is
//                             not on <team> (the dogs' targets), so a run
//                             can measure lunges on stationary targets
//   "team <axis|allies>"      which team the dogs belong to (default allies)
test_watcher()
{
	if ( getDvarInt( "developer" ) <= 0 )
		return;
	setDvar( "scr_dogAttackTest", "" );
	level.dogAttackTestTeam = "allies";
	level.dogAttackTestFreeze = false;
	for ( ;; )
	{
		wait 0.5;
		request = getDvar( "scr_dogAttackTest" );
		if ( request == "" )
			continue;
		setDvar( "scr_dogAttackTest", "" );
		tokens = strTok( request, " " );
		if ( tokens.size < 2 )
			continue;
		if ( tokens[0] == "team" )
			level.dogAttackTestTeam = tokens[1];
		else if ( tokens[0] == "freeze" )
		{
			wasFrozen = level.dogAttackTestFreeze;
			level.dogAttackTestFreeze = int( tokens[1] ) != 0;
			if ( level.dogAttackTestFreeze && !wasFrozen )
				level thread test_freeze_keeper();
			if ( !level.dogAttackTestFreeze )
				foreach ( player in level.players )
					player freezeControls( false );
		}
		print( "CoDOL dogattack test: " + request + "\n" );
	}
}

test_freeze_keeper()
{
	while ( level.dogAttackTestFreeze )
	{
		wait 0.25;
		foreach ( player in level.players )
			if ( isAlive( player ) && isDefined( player.team ) && player.team != level.dogAttackTestTeam && player.team != "spectator" )
				player freezeControls( true );
	}
}
