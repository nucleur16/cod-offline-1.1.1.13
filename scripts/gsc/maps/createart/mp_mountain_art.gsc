// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_mountain ships no createart. T5 maps/mp/createart/mp_mountain_art.gsc setVolFog (start 900.766, half 5211.24, colour 0.729412 0.862745 0.980392, max opacity 0.705677),
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 900.766, 5211.24, 0.729412, 0.862745, 0.980392, 0.705677, 0 );
	VisionSetNaked( "mp_mountain", 0 );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.2 );
	setdvar( "r_lightGridContrast", 0.2 );
}
