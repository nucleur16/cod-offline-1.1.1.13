// PROVENANCE (2026-11-13): the 3.19.26.22 port of mp_lockdown_sh ships no createart. IW5 maps/createart/mp_alpha_fog.gsc (MW3 install zone) sets a vision-set fog (create_vision_set_fog, which MP lacks): startDist 690, halfwayDist 3378, red 0.569, green 0.622, blue 0.676, maxOpacity 0.453,
// written as the IW4 MP setExpFog call (7 arguments: sun fog off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 690, 3378, 0.569, 0.622, 0.676, 0.453, 0 );
	VisionSetNaked( "mp_lockdown_sh", 0 );
}
