// coop soldier animscripts: shared helpers.
//
// The coop species' counterpart of animscripts/zombie_shared.gsc,
// reduced to the IW4 animscripts\shared.gsc notetrack handling a basic soldier
// needs.  It names no multiplayer or zm module: everything is local or a
// builtin, so the coop script closure stays its own.

#using_animtree ( "generic_human" );

// IW4 animscripts\shared.gsc HandleNoteTrack, the subset whose effect this
// server can produce.
HandleNoteTrack( note, flagName )
{
	if ( isSubStr( note, "sndnt#" ) )
		return undefined;

	switch ( note )
	{
	case "end":
	case "finish":
	case "undefined":
		return note;
	case "start_ragdoll":
	case "ragdoll":
		if ( isDefined( self.coop_allow_ragdoll ) && self.coop_allow_ragdoll )
			self startRagdoll();
		break;
	case "pose_stand":
		self.a.pose = "stand";
		break;
	case "pose_crouch":
		self.a.pose = "crouch";
		break;
	case "no pain":
		self.allowpain = false;
		break;
	case "allow pain":
		self.allowpain = true;
		break;
	}
	return undefined;
}

DoNoteTracks( flagName )
{
	for ( ;; )
	{
		self waittill( flagName, note );
		if ( !isDefined( note ) )
			note = "undefined";
		val = self HandleNoteTrack( note, flagName );
		if ( isDefined( val ) )
			return val;
	}
}

DoNoteTracksForTime( time, flagName )
{
	pump = spawnStruct();
	self thread notetrack_pump( flagName, pump );
	wait( time );
	pump notify( "stop_notetracks" );
}

notetrack_pump( flagName, pump )
{
	self endon( "killanimscript" );
	self endon( "death" );
	pump endon( "stop_notetracks" );
	for ( ;; )
	{
		self waittill( flagName, note );
		if ( !isDefined( note ) )
			note = "undefined";
		self HandleNoteTrack( note, flagName );
	}
}

// The standing pose every non-moving state blends to.  exposed_aim_5 is a
// complete pose (it sits in the generic_human exposed_aiming node, not under
// an additive one); exposed_idle_alert_v1 and the exposed_shoot_* set are
// additive in that tree and cannot pose a body on their own.
stand_pose( blend )
{
	if ( !isDefined( blend ) )
		blend = 0.2;
	self setAnimKnobAll( %exposed_aim_5, %body, 1, blend, 1 );
}

// A soldier with no living enemy and no route stands at ease.
idle_pose( blend )
{
	if ( !isDefined( blend ) )
		blend = 0.3;
	if ( isDefined( self.enemy ) )
		self setFlaggedAnimKnobAll( "idleanim", %exposed_aim_5, %body, 1, blend, 1 );
	else
		self setFlaggedAnimKnobAll( "idleanim", %casual_stand_idle, %body, 1, blend, 1 );
}

// A cast member's pose (_cooptraining.gsc): the gesture in coop_pose_anim
// when one is set, played from its start, else the looping idle in
// coop_idle_anim.  Both come from animlists every coop client loads
// (common_human / common_actor_shared), never from the playlist animlist.
cast_pose( blend )
{
	if ( isDefined( self.coop_pose_anim ) )
	{
		self setFlaggedAnimKnobAll( "idleanim", self.coop_pose_anim, %body, 1, blend, 1 );
		self setAnimTime( self.coop_pose_anim, 0 );
		return;
	}
	idle = %casual_stand_idle;
	if ( isDefined( self.coop_idle_anim ) )
		idle = self.coop_idle_anim;
	self setFlaggedAnimKnobAll( "idleanim", idle, %body, 1, blend, 1 );
}
