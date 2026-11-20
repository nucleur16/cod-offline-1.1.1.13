/*
	scripts/codol/maps/mp/animscripts/dog_flashed.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\dog_flashed.gsc

	Changed for CoDOL:
	  - the namespaced maps\mp\animscripts\shared::DoNoteTracks call now points
	    at the dog-local maps\mp\animscripts\dog_shared module.
	  - nothing else changed.  setFlashBanged / getFlashBangedStrength are kept
	    verbatim; they are engine methods this server does not register yet
	    (reported as required builtins).
*/

main()
{
	self endon("killanimscript");
	self endon( "stop_flashbang_effect" );

	wait randomfloatrange( 0, 0.4 );

	duration = self startFlashBanged() * 0.001;

	self setanimstate( "flashed" );
	self maps\mp\animscripts\dog_shared::DoNoteTracks("done");

	self setFlashBanged( false );

	self.flashed = false;
	self notify( "stop_flashbang_effect" );
}

startFlashBanged()
{
	if ( isdefined( self.flashduration ) )
		duration = self.flashduration;
	else
		duration = self getFlashBangedStrength() * 1000;

	self.flashendtime = gettime() + duration;
	self notify("flashed");

	return duration;
}
