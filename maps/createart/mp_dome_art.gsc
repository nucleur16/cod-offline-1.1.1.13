// PROVENANCE (2026-10-22): CoDOL mp_dome (3.5.6.2 port). 3.5.6.2 ships no createart for it.
// IW5 maps/createart/mp_dome_fog.gsc (COD-GSC-Source/IW5-GSC) sets a vision-set fog
// (create_vision_set_fog, which MP lacks):
//   startDist 1274, halfwayDist 3080, red 0.54, green 0.63, blue 0.68, maxOpacity 0.51,
//   transitionTime 0, sunFogEnabled 0
// written here as the IW4 MP setExpFog call (7 arguments: sun fog off). The zone ships
// vision/mp_dome.vision.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 1274, 3080, 0.54, 0.63, 0.68, 0.51, 0 );
	VisionSetNaked( "mp_dome", 0 );
}
