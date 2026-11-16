// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_mogadishu_sh ships no createart. IW5 maps/createart/mp_mogadishu_fog.gsc (MW3 install zone) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 1000, halfwayDist 1433, red 0.591, green 0.578, blue 0.510, maxOpacity 0.74,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 1000, 1433, 0.591, 0.578, 0.510, 0.74, 0 );
	VisionSetNaked( "mp_mogadishu_sh", 0 );
}
