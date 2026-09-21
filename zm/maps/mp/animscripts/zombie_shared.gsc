// zm zombie animscripts: the notetrack pump.
//
// Port of the parts of T5's animscripts\zombie_shared.gsc (704 lines) and
// animscripts\shared.gsc (:1221 DoNoteTracks, :1288 DoNoteTracksForTime) that
// the zombie animscripts in this directory actually call.
//
// WHY THIS EXISTS AT ALL, and why it is pure GSC.  MEASURED 2026-09-20,
// docs/CODOL_SCRIPTED_ANIMATION_20260920.md section 3: the engine already
// produces notetracks.  G_RunFrame ticks every entity's server DObj with
// bNotify = 1, XAnimProcessServerNotify issues Scr_NotifyNum(entnum,
// <the animation's flag>, <notetrack name>) for each notetrack crossed, and
// at the end of the clip it notifies the same flag with "end".  So
// `self waittill( flagName, note )` is a live engine event here exactly as it
// is in T5, and T5's DoNoteTracks pattern ports as written.  Nothing native
// was needed for this file.
//
// ADAPTATIONS, by T5 function:
//
//   DoNoteTracksForTime (shared.gsc:1288).  T5 runs the pump on the CALLING
//   thread and kills it with `ent endon("stop_notetracks")` on a helper
//   struct.  That relies on an endon registered inside a called function
//   unwinding only that function; this VM's endon is thread-scoped, so the
//   T5 shape would kill the caller (MoveMainLoop) instead of returning to it.
//   Ported inverted: the pump is the THREAD and carries the endon, the caller
//   just waits out the interval.  Same observable behaviour, and the endon is
//   now unambiguously scoped to a thread of its own.
//
//   HandleNoteTrack (zombie_shared.gsc:364).  Taken with the cases this
//   server can honour.  Dropped, with the reason:
//     - the self.isdog arm -- HandleDogSoundNoteTracks belongs to
//       maps\mp\animscripts\dog_shared, which is the MULTIPLAYER tree and may
//       not be named from zm (a file::function reference is a compile-time
//       dependency, see _zombiemode.gsc's header).  No zm actor is a dog.
//     - "stop anim" -> anim_stopanimscripted().  MEASURED: stopanimscripted
//       is deliberately NOT a registered builtin
//       (docs/CODOL_SCRIPTED_ANIMATION_20260920.md section 5) and stubbing it
//       is forbidden.  The note is consumed and returned so the caller's loop
//       still terminates on it, which is the half that does not need the
//       builtin; the scripted-alignment half is genuinely absent.
//     - "swap taghelmet to tagleft" -- the zombie bodies carry no hatModel.
//     - the /# showNoteTrack #/ developer block.
//
//   anim.notetracks (T5's global registry).  Rewritten as
//   level.zombie_notetracks.  The dog animscripts already in this server
//   (maps/mp/animscripts/dog_init.gsc and friends, the only animscript set
//   proven to run here) use level. throughout and never touch the anim
//   global object, so this file does the same rather than being the first
//   thing to depend on it.

#include maps\mp\gametypes\_zombiemode_utility;

// --------------------------------------------------------------------------
// The registry.  Level-once; zombie_init::firstInit calls it.
// --------------------------------------------------------------------------

init_notetracks()
{
	if ( isDefined( level.zombie_notetracks_ready ) )
		return;

	level.zombie_notetracks_ready = true;
	level.zombie_notetracks = [];
}

// A level script (or _zombiemode_blockers) may claim a notetrack name.  The
// handler is called as self [[ func ]]( note, flagName ) on the ACTOR and its
// return value, when defined, ends the pump exactly as "end" would.
register_notetrack( note, func )
{
	init_notetracks();
	level.zombie_notetracks[ note ] = func;
}

// --------------------------------------------------------------------------
// T5 zombie_shared.gsc:364 HandleNoteTrack.
// --------------------------------------------------------------------------

HandleNoteTrack( note, flagName, customFunction, var1 )
{
	if ( isDefined( level.zombie_notetracks ) )
	{
		notetrackFunc = level.zombie_notetracks[ note ];
		if ( isDefined( notetrackFunc ) )
			return self [[ notetrackFunc ]]( note, flagName );
	}

	// CoDOL/T5 sound notetracks.  MEASURED: T5 writes an animation's audio
	// cues as notetracks named "sndnt#<alias>" (maps\creek_1_anim.gsc:168 and
	// ~200 other call sites), and CoDOL ships the zombie set's aliases under
	// exactly that scheme -- zmb_zombie_nt_vocals_step / _swing / _attack /
	// _crawl / _land / _taunt and zmb_zombie_nt_traverse_v1 / _v2, all in
	// common_gametype_zm (docs/zone_assets/ALL_sound.txt).  The "nt_" in the
	// alias name IS the notetrack marker.  Those are played by the CLIENT
	// from its own copy of the animation (XAnimProcessClientNotify, which a
	// dedicated server never runs -- docs/CODOL_SCRIPTED_ANIMATION_20260920.md
	// section 3), so the server consumes the note and plays nothing.  Playing
	// it here as well would double every footstep.
	if ( isSubStr( note, "sndnt#" ) )
		return;

	switch ( note )
	{
	case "end":
	case "finish":
	case "undefined":
		return note;

	// T5 :199 noteTrackStopAnim.  See the header: the scripted-alignment half
	// of this needs stopanimscripted, which does not exist.  Ending the
	// caller's loop is the half that does.
	case "stop anim":
		return note;

	// T5 :203 noteTrackStartRagdoll.
	case "start_ragdoll":
	case "ragdoll":
		if ( isDefined( self.allow_ragdoll ) && self.allow_ragdoll )
			self startRagdoll();
		break;

	// T5 :214-:236, the movement notes.  self.a.movement is read by
	// zombie_move when it picks the next cycle.
	case "movement_stop":
		self.a.movement = "stop";
		break;
	case "movement_walk":
		self.a.movement = "walk";
		break;
	case "movement_run":
		self.a.movement = "run";
		break;

	// T5 :255-:272.
	case "pose_stand":
		self.a.pose = "stand";
		break;
	case "pose_crouch":
		self.a.pose = "crouch";
		break;
	case "pose_prone":
		self.a.pose = "prone";
		break;

	// T5 :313 / :335.  T5 selects the surface alias from self.groundtype,
	// which no builtin on this server reports; the generic zombie step alias
	// is used for every surface instead.
	case "step":
	case "footstep":
	case "footstep_left":
	case "footstep_right":
		self play_sound_on_ent( "zombie_step" );
		break;
	case "footscrape":
	case "scrape":
		self play_sound_on_ent( "zombie_scrape" );
		break;

	// T5 :292 noteTrackBodyFall.
	case "bodyfall":
	case "bodyfall_large":
		self play_sound_on_ent( "zombie_bodyfall" );
		break;

	case "no pain":
		self.allowpain = false;
		break;
	case "allow pain":
		self.allowpain = true;
		break;
	case "no death":
		self.a.nodeath = true;
		break;

	case "anim_melee = right":
		self.a.meleeState = "right";
		break;
	case "anim_melee = left":
		self.a.meleeState = "left";
		break;

	default:
		if ( isDefined( customFunction ) )
		{
			if ( !isDefined( var1 ) )
				return self [[ customFunction ]]( note );
			return self [[ customFunction ]]( note, var1 );
		}
		break;
	}
}

// --------------------------------------------------------------------------
// T5 shared.gsc:1221 DoNoteTracks.  Runs until the clip ends.
// --------------------------------------------------------------------------

DoNoteTracks( flagName, customFunction, var1 )
{
	for ( ;; )
	{
		self waittill( flagName, note );

		if ( !isDefined( note ) )
			note = "undefined";

		val = self HandleNoteTrack( note, flagName, customFunction, var1 );

		if ( isDefined( val ) )
			return val;
	}
}

// --------------------------------------------------------------------------
// T5 shared.gsc:1288 DoNoteTracksForTime, inverted -- see the header.
// For LOOPING animations: pump the notetracks for `time` seconds, then return
// so the caller can re-evaluate which cycle it wants.
// --------------------------------------------------------------------------

DoNoteTracksForTime( time, flagName, customFunction )
{
	pump = spawnStruct();
	self thread notetrack_pump( flagName, customFunction, pump );

	wait( time );

	pump notify( "stop_notetracks" );
}

notetrack_pump( flagName, customFunction, pump )
{
	self endon( "killanimscript" );
	self endon( "death" );
	pump endon( "stop_notetracks" );

	for ( ;; )
	{
		self waittill( flagName, note );

		if ( !isDefined( note ) )
			note = "undefined";

		self HandleNoteTrack( note, flagName, customFunction );
	}
}
