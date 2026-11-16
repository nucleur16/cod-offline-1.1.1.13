// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_liberation_sh ships no createart. IW5 maps/createart/mp_park_fog.gsc (COD-GSC-Source/IW5-GSC) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 1053.84, halfwayDist 23677.5, red 0.568627, green 0.701961, blue 0.827451, maxOpacity 0.135068,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 1053.84, 23677.5, 0.568627, 0.701961, 0.827451, 0.135068, 0 );
	VisionSetNaked( "mp_liberation_sh", 0 );
}
