// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_italy_sh ships no createart. IW5 maps/createart/mp_italy_fog.gsc (COD-GSC-Source/IW5-GSC) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 18827, halfwayDist 26018, red 0.51, green 0.55, blue 0.59, maxOpacity 0.91,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 18827, 26018, 0.51, 0.55, 0.59, 0.91, 0 );
	VisionSetNaked( "mp_italy_sh", 0 );
}
