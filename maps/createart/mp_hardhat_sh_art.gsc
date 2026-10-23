// PROVENANCE (2026-10-22): CoDOL mp_hardhat_sh (3.5.6.2 port). 3.5.6.2 ships no createart for it
// and COD-GSC-Source has no IW5 mp_hardhat fog, so no fog is set. The zone ships
// vision/mp_hardhat_sh.vision.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	VisionSetNaked( "mp_hardhat_sh", 0 );
}
