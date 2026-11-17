// PROVENANCE (2026-10-22): mp_strike_sh is a 3.5.6.2 map ported to 1.1.1.13 by CODOL-AssetExplorer
// (port-map). Its createart is in no 3.5.6.2 client zone; this is IW4 maps/createart/mp_strike_art.gsc
// (the same Strike layout and sun) with the vision set pointed at the zone-shipped
// vision/mp_strike_sh.vision.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 1011.69, 5091.8, 0.666667, 0.835294, 1, 0.14427, 0, 0.823529, 0.796079, 0.494118, (-0.758499, -0.295371, 0.580892), 3.54832, 76.386, 1.16812 );
	VisionSetNaked( "mp_strike_sh", 0 );
}
