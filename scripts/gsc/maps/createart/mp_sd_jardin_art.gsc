// PROVENANCE (2026-10-22): CoDOL mp_sd_jardin (3.5.6.2 port). 3.5.6.2 ships no createart for it,
// so no fog is set. The zone ships vision/mp_sd_jardin.vision.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	VisionSetNaked( "mp_sd_jardin", 0 );
}
