// coop soldier animscript: soldier_pain.
//
// IW4 animscripts\pain.gsc reduced to one flinch per hit, rate-limited the way
// IW4 limits it (a soldier does not re-flinch inside 1.5 s), standing or
// running clip by movement.

#include maps\mp\animscripts\soldier_shared;

#using_animtree ( "generic_human" );

main()
{
	self endon( "killanimscript" );
	self endon( "death" );

	self setFlashBanged( false );

	if ( !isDefined( self.allowpain ) || !self.allowpain )
		return;

	now = getTime();
	if ( isDefined( self.a.painTime ) && now - self.a.painTime < 1500 )
		return;
	self.a.painTime = now;

	if ( self.a.movement == "run" )
	{
		if ( randomInt( 2 ) )
			painAnim = %run_pain_stumble;
		else
			painAnim = %run_pain_fallonknee;
	}
	else
	{
		switch ( randomInt( 3 ) )
		{
		case 0:
			painAnim = %exposed_pain_face;
			break;
		case 1:
			painAnim = %exposed_pain_groin;
			break;
		default:
			painAnim = %exposed_pain_right_arm;
			break;
		}
	}

	self animmode( "gravity" );
	self setFlaggedAnimKnobAllRestart( "painanim", painAnim, %body, 1, 0.1, 1 );
	DoNoteTracksForTime( getAnimLength( painAnim ), "painanim" );
	self animmode( "normal" );
}
