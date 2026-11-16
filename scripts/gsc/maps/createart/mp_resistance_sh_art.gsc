// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_resistance_sh ships no createart. IW5 maps/createart/mp_paris_fog.gsc (MW3 install zone) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 3255, halfwayDist 5626, red 0.538, green 0.572, blue 0.595, maxOpacity 0.41,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 3255, 5626, 0.538, 0.572, 0.595, 0.41, 0 );
	VisionSetNaked( "mp_resistance_sh", 0 );
}
