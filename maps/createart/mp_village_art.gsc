// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_village ships no createart. IW5 maps/createart/mp_village_fog.gsc (MW3 install zone) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 700, halfwayDist 4792, red 0.467, green 0.499, blue 0.478, maxOpacity 0.663,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 700, 4792, 0.467, 0.499, 0.478, 0.663, 0 );
	VisionSetNaked( "mp_village", 0 );
}
