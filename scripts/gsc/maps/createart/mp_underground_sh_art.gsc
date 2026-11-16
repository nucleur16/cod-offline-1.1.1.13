// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_underground_sh ships no createart. IW5 maps/createart/mp_underground_fog.gsc (MW3 install zone) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 1200, halfwayDist 8000, red 0.741176, green 0.780392, blue 0.823529, maxOpacity 0.28871,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 1200, 8000, 0.741176, 0.780392, 0.823529, 0.28871, 0 );
	VisionSetNaked( "mp_underground_sh", 0 );
}
