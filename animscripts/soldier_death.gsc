// coop soldier animscript: soldier_death.
//
// IW4 animscripts\death.gsc reduced to its choice of a standing or running
// death clip.  The native frame runs this module once health reaches zero and
// turns the actor into a corpse when the thread ends
// (CODOL_ActorRunFrame / ActorBecomeCorpse), so the clip is played to its
// length and the module returns.

#include maps\mp\animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self endon( "killanimscript" );

	if ( isDefined( self.a ) && isDefined( self.a.nodeath ) && self.a.nodeath )
	{
		wait( 0.1 );
		return;
	}

	deathAnim = pick_death_anim();

	self animmode( "gravity" );
	self orientmode( "face current" );
	self setFlaggedAnimKnobAll( "deathanim", deathAnim, %body, 1, 0.05, 1 );

	length = getAnimLength( deathAnim );
	if ( !animHasNotetrack( deathAnim, "start_ragdoll" ) )
		self thread ragdoll_after( length * 0.6 );

	DoNoteTracksForTime( length, "deathanim" );
}

ragdoll_after( delay )
{
	self endon( "killanimscript" );
	wait( delay );
	if ( isDefined( self.coop_allow_ragdoll ) && self.coop_allow_ragdoll )
		self startRagdoll();
}

pick_death_anim()
{
	anims = [];
	if ( isDefined( self.a ) && self.a.movement == "run" )
	{
		anims[ anims.size ] = %run_death_facedown;
		anims[ anims.size ] = %run_death_roll;
		anims[ anims.size ] = %run_death_flop;
	}
	else
	{
		anims[ anims.size ] = %exposed_death;
		anims[ anims.size ] = %exposed_death_02;
		anims[ anims.size ] = %exposed_death_falltoknees;
		anims[ anims.size ] = %exposed_death_twist;
		anims[ anims.size ] = %stand_death_tumbleback;
		anims[ anims.size ] = %stand_death_head_straight_back;
	}
	return anims[ randomInt( anims.size ) ];
}
