/*
	scripts/codol/maps/mp/animscripts/dog_jump.gsc

	Adapted from the shipped Black Ops (T5) script source:
		D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\mp\animscripts\dog_jump.gsc

	Changed for CoDOL:
	  - #include maps\mp\animscripts\utility + ...\shared and the namespaced
	    maps\mp\animscripts\shared::DoNoteTracks call now point at the
	    dog-local maps\mp\animscripts\dog_shared module.
	  - nothing else changed.
*/

#include maps\mp\animscripts\dog_shared;

main()
{
	self endon("killanimscript");

	debug_anim_print("dog_jump::main()" );
	self SetAimAnimWeights( 0, 0 );

	self.safeToChangeScript = false;
	self setanimstate( "traverse_wallhop" );
	maps\mp\animscripts\dog_shared::DoNoteTracks( "done" );
	self.safeToChangeScript = true;
}
