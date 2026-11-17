// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_foundation_sh ships no createart. IW5 maps/createart/mp_cement_fog.gsc (COD-GSC-Source/IW5-GSC) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 541, halfwayDist 5000, red 0.498, green 0.557, blue 0.556, maxOpacity 0.568,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 541, 5000, 0.498, 0.557, 0.556, 0.568, 0 );
	VisionSetNaked( "mp_foundation_sh", 0 );
}
