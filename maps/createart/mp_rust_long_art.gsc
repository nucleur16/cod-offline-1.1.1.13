// PROVENANCE (2026-10-14): IW4 maps/createart/mp_rust_long_art.gsc (COD-GSC-Source IW4-GSC),
// unchanged in content, for CoDOL mp_rust_long (3.3.3.3 map ported to 1.1.1.13); the zone
// ships vision/mp_rust_long.vision.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 972.376, 1707.07, 0.548461, 0.468579, 0.381201, 0.168073, 0 );
	VisionSetNaked( "mp_rust_long", 0 );
}
