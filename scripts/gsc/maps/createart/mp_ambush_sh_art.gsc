// PROVENANCE (2026-10-14): CoDOL mp_ambush_sh (3.3.3.3 map ported to 1.1.1.13) is CoD4's
// Ambush, mp_convoy (the zone's compass is compass_map_mp_convoy). IW3 maps/mp/mp_convoy.gsc
// sets its fog in the level script, setExpFog( 800, 20000, 0.583, 0.631569, 0.553078, 0 )
// with IW3's six arguments (no opacity); here it is the IW4 MP form with full opacity. The
// zone ships vision/mp_ambush_sh.vision.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 800, 20000, 0.583, 0.631569, 0.553078, 1, 0 );
	VisionSetNaked( "mp_ambush_sh", 0 );
}
