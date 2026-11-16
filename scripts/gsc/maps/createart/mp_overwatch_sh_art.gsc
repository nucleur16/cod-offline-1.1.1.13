// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_overwatch_sh ships no createart. IW5 maps/createart/mp_overwatch_fog.gsc (COD-GSC-Source/IW5-GSC) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 1200, halfwayDist 88053.4, red 0.729014, green 0.702022, blue 0.698223, maxOpacity 0.156545,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 1200, 88053.4, 0.729014, 0.702022, 0.698223, 0.156545, 0 );
	VisionSetNaked( "mp_overwatch_sh", 0 );
}
