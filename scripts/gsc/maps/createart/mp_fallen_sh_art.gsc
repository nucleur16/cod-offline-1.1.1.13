// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_fallen_sh ships no createart. IW5 maps/createart/mp_lambeth_fog.gsc (MW3 install zone) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 1034.32, halfwayDist 14869, red 0.47451, green 0.537255, blue 0.54902, maxOpacity .313545,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 1034.32, 14869, 0.47451, 0.537255, 0.54902, .313545, 0 );
	VisionSetNaked( "mp_fallen_sh", 0 );
}
