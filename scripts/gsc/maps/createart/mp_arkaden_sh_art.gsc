// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_arkaden_sh ships no createart. IW5 maps/createart/mp_plaza2_fog.gsc (MW3 install zone) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 1550, halfwayDist 13965, red 0.63, green 0.70, blue 0.72, maxOpacity 0.264091,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 1550, 13965, 0.63, 0.70, 0.72, 0.264091, 0 );
	VisionSetNaked( "mp_arkaden_sh", 0 );
}
