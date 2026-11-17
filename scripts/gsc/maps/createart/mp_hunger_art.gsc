// PROVENANCE (2026-10-22): mp_hunger (CODOL 3.5.6.2 port). 3.5.6.2 ships no createart for it; the
// vision set is the zone-shipped vision/mp_hunger.vision. The fog is a light exponential fog for
// its 16000-unit-wide level, in the colour of the mp_hunger vision's light tint.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 2000, 12000, 0.62, 0.6, 0.55, 0.35, 0 );
	VisionSetNaked( "mp_hunger", 0 );
}
