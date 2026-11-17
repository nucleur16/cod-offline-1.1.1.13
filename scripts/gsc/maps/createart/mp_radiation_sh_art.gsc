// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_radiation_sh ships no createart. T5 maps/mp/createart/mp_radiation_art.gsc setVolFog (start 400.724, half 2064.4, colour 0.32549 0.501961 0.627451, max opacity 0.460806),
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 400.724, 2064.4, 0.32549, 0.501961, 0.627451, 0.460806, 0 );
	VisionSetNaked( "mp_radiation_sh", 0 );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.4 );
	setdvar( "r_lightGridContrast", .2 );
}
