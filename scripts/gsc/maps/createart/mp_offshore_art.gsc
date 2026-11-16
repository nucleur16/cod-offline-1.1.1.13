// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_offshore ships no createart. IW5 maps/createart/mp_roughneck_fog.gsc (COD-GSC-Source/IW5-GSC) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 512.098, halfwayDist 4150, red 0.331909, green 0.373677, blue 0.394813, maxOpacity 1,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 512.098, 4150, 0.331909, 0.373677, 0.394813, 1, 0 );
	VisionSetNaked( "mp_offshore", 0 );
}
