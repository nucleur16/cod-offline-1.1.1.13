// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_carbon_sh ships no createart. IW5 maps/createart/mp_carbon_fog.gsc (MW3 install zone) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 5006, halfwayDist 1748, red 0.360, green 0.371, blue 0.376, maxOpacity 0.715,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 5006, 1748, 0.360, 0.371, 0.376, 0.715, 0 );
	VisionSetNaked( "mp_carbon_sh", 0 );
}
