// PROVENANCE (2026-10-14): CoDOL mp_seatown (3.3.3.3 map, MW3 Seatown, ported to 1.1.1.13).
// IW5 maps/createart/mp_seatown_art.gsc only sets level.tweakfile and SetSunLight; the fog
// lives in IW5 maps/createart/mp_seatown_fog.gsc as a vision-set fog (create_vision_set_fog,
// which MP lacks):
//   startDist 292, halfwayDist 5000, red 0.537, green 0.549, blue 0.564, maxOpacity 1,
//   transitionTime 0, sunFogEnabled 0
// Source: MW3 raw dump https://github.com/Brentdevent/MW3-GSC-Dump
// (_raw-files/maps/createart/mp_seatown_fog.gsc), same values in the gsc-tool decompile at
// https://github.com/Divity/iw5-gsc-dump. (decompiled and labled/maps/createart/).
// Written here as the IW4 MP setExpFog call (7 arguments: sun fog off). The zone ships
// vision/mp_seatown.vision.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 292, 5000, 0.537, 0.549, 0.564, 1, 0 );
	VisionSetNaked( "mp_seatown", 0 );
}
