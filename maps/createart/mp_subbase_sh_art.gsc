// PROVENANCE (2026-10-14): IW4 maps/createart/mp_subbase_art.gsc (COD-GSC-Source IW4-GSC)
// for CoDOL mp_subbase_sh (3.3.3.3 map ported to 1.1.1.13), with the vision set re-pointed
// at the zone-shipped vision/mp_subbase_sh.vision.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 0, 2228.18, 0.894118, 0.8962, 0.929412, 0.776943, 0 );
	VisionSetNaked( "mp_subbase_sh", 0 );
}
