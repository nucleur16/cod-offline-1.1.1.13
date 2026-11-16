// PROVENANCE (2026-10-14): IW4 maps/createart/mp_shipment_long_art.gsc (COD-GSC-Source
// IW4-GSC), unchanged in content, for CoDOL mp_shipment_long (3.3.3.3 map ported to
// 1.1.1.13); the zone ships vision/mp_shipment_long.vision.
main()
{
	level.tweakfile = true;

	setDvar( "scr_fog_disable", "0" );

	VisionSetNaked( "mp_shipment_long", 0 );
}
