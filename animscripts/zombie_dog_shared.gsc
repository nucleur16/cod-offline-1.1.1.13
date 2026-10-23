// zm hellhound animscript: animscripts/zombie_dog_shared.gsc
//
// Ported from this tree's multiplayer dog animscript
// maps/mp/animscripts/dog_shared.gsc (itself adapted from the shipped T5
// maps\mp\animscripts\dog_shared.gsc, header kept below).  The zm hellhound
// (species "zombie_dog", codol_actor_scripts.cpp) runs the same native T5 dog
// state machine bound against C1's animtrees/zombie_dog.atr, and every state
// is selected by name through setanimstate(), so the bodies are the MP
// dog's.  Changed for zm, the same way T5 forks animscripts\dog_* into
// animscripts\zombie_dog_*:
//   - module paths maps\mp\animscripts\dog_* -> ...\zombie_dog_*, so the zm
//     category compiles no multiplayer module (see _zombiemode.gsc);
//   - no maps\mp\_utility and no maps\mp\_dogs reference;
//   - HandleNoteTrack and trackLoop treat species "zombie_dog" as a dog.

/*
	maps/mp/animscripts/dog_shared.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\shared.gsc
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\utility.gsc

	This file carries ONLY the helpers the animscripts\zombie_dog_* scripts
	call.  It is deliberately a separate module rather than a port of the two
	T5 files: the CoDOL tree already mounts an IW4-shaped animscripts\shared
	and animscripts\utility (bin/home/codol_rawfiles/animscripts/), and those
	must not be overwritten - their HandleNoteTrack drives the IW4 human AI
	and expects self.a fields the dog init never sets.

	Changed for CoDOL:
	  - shared.gsc + utility.gsc merged into one dog-local module, so the dog
	    scripts #include animscripts\zombie_dog_shared instead of
	    maps\mp\animscripts\shared / ...\utility (neither path exists here).
	  - T5 canonical-hash dvar literals GetDvarInt( #"name" ) rewritten to the
	    plain string form GetDvarInt( "name" ): the #"..." token is a T5
	    compiler feature and does not exist in this VM.
	  - trackLoop dropped its unused "players = get_players();" line:
	    get_players() has no counterpart in this tree (T5 defines it in
	    maps\mp\_utility, the CoDOL maps\mp\_utility does not) and the local
	    was never read.
	  - shared.gsc::growling(), registerNoteTracks() and the whole human
	    weapon/pose notetrack set are not carried: no dog_* script calls them.
	  - utility.gsc::setFootstepEffect() and current_yaw_line_debug() are not
	    carried for the same reason.
*/

#include common_scripts\utility;

// no longer playing sounds on the server but we still want the returns to tell
// the script if a non-sound notify needs to be handled
HandleDogSoundNoteTracks( note )
{
	if ( note == "sound_dogstep_run_default" )
	{
		return true;
	}

	prefix = getsubstr( note, 0, 5 );

	if ( prefix != "sound" )
		return false;

	return true;
}

HandleNoteTrack( note, flagName, customFunction, var1 )
{
/#
	if ( GetDvarInt( "debug_dog_notetracks" ) )
		println("dog notetrack: " + flagName + " " + note + " " + GetTime() );
#/

	if ( isAI( self ) && ( self.type == "dog" || self.type == "zombie_dog" ) )
		if ( HandleDogSoundNoteTracks( note ) )
			return;

	switch ( note )
	{
	case "end":
	case "finish":
	case "undefined":
		return note;
	default:
		if (isDefined(customFunction))
		{
			if (!isdefined(var1))
			{
				return [[customFunction]] (note);
			}
			else
			{
				return [[customFunction]] (note, var1);
			}
		}
		break;
	}
}

// DoNoteTracks waits for and responds to standard noteTracks on the animation, returning when it gets an "end" or a "finish"
// For level scripts, a pointer to a custom function should be passed as the second argument, which handles notetracks not
// already handled by the generic function. This call should take the form DoNoteTracks(flagName, ::customFunction);
// The custom function will be called for each notetrack not recognized, and will pass the notetrack name. Note that this
// function could be called multiple times for a single animation.
DoNoteTracks( flagName, customFunction, var1 )
{
	for (;;)
	{
		self waittill (flagName, note);

		if ( !isDefined( note ) )
			note = "undefined";

		val = self HandleNoteTrack( note, flagName, customFunction, var1 );

		if ( isDefined( val ) )
			return val;
	}
}

DoNoteTracksForeverProc( notetracksFunc, flagName, killString, customFunction, var1 )
{
	if (isdefined (killString))
		self endon (killString);
	self endon ("killanimscript");

	for (;;)
	{
		time = GetTime();
		returnedNote = [[notetracksFunc]](flagName, customFunction, var1);
		timetaken = GetTime() - time;
		if ( timetaken < 0.05)
		{
			time = GetTime();
			returnedNote = [[notetracksFunc]](flagName, customFunction, var1);
			timetaken = GetTime() - time;
			if ( timetaken < 0.05)
			{
/#
				println (GetTime()+" dog_shared::DoNoteTracksForever is trying to cause an infinite loop on anim "+flagName+", returned "+returnedNote+".");
#/
				wait ( 0.05 - timetaken );
			}
		}
	}
}

// Don't call this function except as a thread you're going to kill - it lasts forever.
DoNoteTracksForever(flagName, killString, customFunction, var1 )
{
	DoNoteTracksForeverProc( ::DoNoteTracks, flagName, killString, customFunction, var1 );
}

DoNoteTracksForTimeProc( doNoteTracksForeverFunc, time, flagName, customFunction , ent, var1)
{
	ent endon ("stop_notetracks");
	[[doNoteTracksForeverFunc]](flagName, undefined, customFunction, var1);
}

// Designed for using DoNoteTracks on looping animations, so you can wait for a time instead of the "end" parameter
DoNoteTracksForTime(time, flagName, customFunction, var1)
{
	ent = spawnstruct();
	ent thread doNoteTracksForTimeEndNotify(time);
	DoNoteTracksForTimeProc( ::DoNoteTracksForever, time, flagName, customFunction, ent, var1);
}

doNoteTracksForTimeEndNotify(time)
{
	wait (time);
	self notify ("stop_notetracks");
}

trackLoop(  )
{
	deltaChangePerFrame = 5;

	aimBlendTime = .05;

	prevYawDelta = 0;
	prevPitchDelta = 0;
	maxYawDeltaChange = 5; // max change in yaw in 1 frame
	maxPitchDeltaChange = 5;

	pitchAdd = 0;
	yawAdd = 0;

	if ( self.type == "dog" || self.type == "zombie_dog" )
	{
		doMaxAngleCheck = false;
		self.shootEnt = self.enemy;
	}
	else
	{
		doMaxAngleCheck = true;
	if ( self.a.script == "cover_crouch" && isdefined( self.a.coverMode ) && self.a.coverMode == "lean" )
		pitchAdd = -1 * anim.coverCrouchLeanPitch;
	if ( (self.a.script == "cover_left" || self.a.script == "cover_right") && isdefined( self.a.cornerMode ) && self.a.cornerMode == "lean" )
		yawAdd = self.coverNode.angles[1] - self.angles[1];
	}

	yawDelta = 0;
	pitchDelta = 0;

	firstFrame = true;

	for(;;)
	{
		incrAnimAimWeight();

		selfShootAtPos = (self.origin[0], self.origin[1], self getEye()[2]);

		shootPos = undefined;
		if ( isdefined( self.enemy ) )
			shootPos = self.enemy getShootAtPos();

		if ( !isdefined( shootPos ) )
		{
			yawDelta = 0;
			pitchDelta = 0;
		}
		else
		{
			vectorToShootPos = shootPos - selfShootAtPos;
			anglesToShootPos = vectorToAngles( vectorToShootPos );

			pitchDelta = 360 - anglesToShootPos[0];
			pitchDelta = AngleClamp180( pitchDelta + pitchAdd );

			yawDelta = self.angles[1] - anglesToShootPos[1];

			yawDelta = AngleClamp180( yawDelta + yawAdd );
		}

		if ( doMaxAngleCheck && ( abs( yawDelta ) > 60 || abs( pitchDelta ) > 60 ) )
		{
			yawDelta = 0;
			pitchDelta = 0;
		}
		else
		{
			if ( yawDelta > self.rightAimLimit )
				yawDelta = self.rightAimLimit;
			else if ( yawDelta < self.leftAimLimit )
				yawDelta = self.leftAimLimit;
			if ( pitchDelta > self.upAimLimit )
				pitchDelta = self.upAimLimit;
			else if ( pitchDelta < self.downAimLimit )
				pitchDelta = self.downAimLimit;
		}

		if ( firstFrame )
		{
			firstFrame = false;
		}
		else
		{
			yawDeltaChange = yawDelta - prevYawDelta;
			if ( abs( yawDeltaChange ) > maxYawDeltaChange )
				yawDelta = prevYawDelta + maxYawDeltaChange * sign( yawDeltaChange );

			pitchDeltaChange = pitchDelta - prevPitchDelta;
			if ( abs( pitchDeltaChange ) > maxPitchDeltaChange )
				pitchDelta = prevPitchDelta + maxPitchDeltaChange * sign( pitchDeltaChange );
		}

		prevYawDelta = yawDelta;
		prevPitchDelta = pitchDelta;

		updown = 0;
		leftright = 0;

		if ( yawDelta > 0 )
		{
			assert( yawDelta <= self.rightAimLimit );
			weight = yawDelta / self.rightAimLimit * self.a.aimweight;
			leftright = weight;
		}
		else if ( yawDelta < 0 )
		{
			assert( yawDelta >= self.leftAimLimit );
			weight = yawDelta / self.leftAimLimit * self.a.aimweight;
			leftright = -1 * weight;
		}

		if ( pitchDelta > 0 )
		{
			assert( pitchDelta <= self.upAimLimit );
			weight = pitchDelta / self.upAimLimit * self.a.aimweight;
			updown = weight;
		}
		else if ( pitchDelta < 0 )
		{
			assert( pitchDelta >= self.downAimLimit );
			weight = pitchDelta / self.downAimLimit * self.a.aimweight;
			updown = -1 * weight;
		}

		self SetAimAnimWeights( updown, leftright );
		wait(0.05);
	}
}

//setAnimAimWeight works just like setanimlimited on an imaginary anim node that affects the four aiming directions.
setAnimAimWeight(goalweight, goaltime)
{
	if ( !isdefined( goaltime ) || goaltime <= 0 )
	{
		self.a.aimweight = goalweight;
		self.a.aimweight_start = goalweight;
		self.a.aimweight_end = goalweight;
		self.a.aimweight_transframes = 0;
	}
	else
	{
		self.a.aimweight = goalweight;
		self.a.aimweight_start = self.a.aimweight;
		self.a.aimweight_end = goalweight;
		self.a.aimweight_transframes = int(goaltime * 20);
	}
	self.a.aimweight_t = 0;
}

incrAnimAimWeight()
{
	if ( self.a.aimweight_t < self.a.aimweight_transframes )
	{
		self.a.aimweight_t++;
		t = 1.0 * self.a.aimweight_t / self.a.aimweight_transframes;
		self.a.aimweight = self.a.aimweight_start * (1 - t) + self.a.aimweight_end * t;
	}
}

// ================================================================
// from maps\mp\animscripts\utility.gsc
// ================================================================

anim_get_dvar_int( dvar, def )
{
	return int( anim_get_dvar( dvar, def ) );
}

// dvar set/fetch/check
anim_get_dvar( dvar, def )
{
	if ( getdvar( dvar ) != "" )
		return getdvarfloat( dvar );
	else
	{
		setdvar( dvar, def );
		return def;
	}
}

set_orient_mode( mode, val1 )
{
/#
	if ( level.dog_debug_orient == self getentnum() )
	{
		if ( isdefined( val1 ) )
			println( "DOG:  Setting orient mode: " + mode + " " + val1 + " " + getTime() );
		else
			println( "DOG:  Setting orient mode: " + mode + " " + getTime() );
	}
#/

	if ( isdefined( val1 ) )
		self OrientMode( mode, val1 );
	else
		self OrientMode( mode );
}

debug_anim_print( text )
{
/#
	if ( level.dog_debug_anims  )
		println( text+ " " + getTime() );

	if ( level.dog_debug_anims_ent == self getentnum() )
		println( text+ " " + getTime() );
#/
}

debug_turn_print( text, line )
{
/#
	if ( level.dog_debug_turns == self getentnum() )
	{
		duration = 200;
		currentYawColor = (1,1,1);
		lookaheadYawColor = (1,0,0);
		desiredYawColor = (1,1,0);

		currentYaw = AngleClamp180(self.angles[1]);
		desiredYaw = AngleClamp180(self.desiredangle);
		lookaheadDir = self.lookaheaddir;
		lookaheadAngles = vectortoangles(lookaheadDir);
		lookaheadYaw = AngleClamp180(lookaheadAngles[1]);
			println( text+ " " + getTime() + " cur: " + currentYaw + " look: " + lookaheadYaw + " desired: " + desiredYaw );
	}
#/
}

debug_allow_movement()
{
/#
	return ( anim_get_dvar_int( "debug_dog_allow_movement", "1" ) );
#/

	return true;
}

debug_allow_combat()
{
/#
	return ( anim_get_dvar_int( "debug_dog_allow_combat", "1" ) );
#/

	return true;
}

// T5's actor damageyaw is the yaw of the damage DIRECTION (attacker to
// victim) relative to the actor's facing, -180..180: +-180 is a hit from the
// front.  This server's damageyaw is the yaw TOWARDS the attacker, 0..360
// (codol_actor_lifecycle.cpp ActorRecordDamage; animscripts/zombie_pain.gsc
// converts the same way).  MEASURED (t5cap dog clip timeline, 2026-10-22):
// T5 hounds shot from behind play death_hit_back, from their left
// death_hit_left; CoDOL's played death_front and death_hit_right.
t5DamageYaw()
{
	yaw = self.damageyaw + 180;
	while ( yaw > 180 )
		yaw -= 360;
	while ( yaw <= -180 )
		yaw += 360;
	return yaw;
}

getAnimDirection( damageyaw )
{
	if( ( damageyaw > 135 ) ||( damageyaw <= -135 ) )	// Front quadrant
	{
		return "front";
	}
	else if( ( damageyaw > 45 ) &&( damageyaw <= 135 ) )		// Right quadrant
	{
		return "right";
	}
	else if( ( damageyaw > -45 ) &&( damageyaw <= 45 ) )		// Back quadrant
	{
		return "back";
	}
	else
	{															// Left quadrant
		return "left";
	}
	return "front";
}
