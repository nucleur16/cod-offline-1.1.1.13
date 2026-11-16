// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_mission_sh ships no createart. IW5 maps/createart/mp_bravo_fog.gsc (MW3 install zone) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 802, halfwayDist 5000, red 0.513, green 0.436, blue 0.308, maxOpacity 1,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 802, 5000, 0.513, 0.436, 0.308, 1, 0 );
	VisionSetNaked( "mp_mission_sh", 0 );
}
