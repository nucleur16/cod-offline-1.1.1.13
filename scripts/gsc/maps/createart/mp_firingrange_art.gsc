// PROVENANCE (2026-10-14): IW4 maps/createart/mp_firingrange_art.gsc (COD-GSC-Source IW4-GSC,
// the IW4 DLC port of Black Ops Firing Range) for CoDOL mp_firingrange (3.3.3.3 map ported
// to 1.1.1.13). The zone ships vision/mp_firingrange_tp.vision only; mp_firingrange is the
// name _load sets anyway (VisionSetNaked( mapname )), so the vision call is kept as IW4 has it.
main()
{
	level.tweakfile = true;

	setDvar( "scr_fog_disable", "0" );

	setExpFog( 3015.28, 4009.05, 0.517317, 0.521552, 0.541961, 0.7, 0 );
	VisionSetNaked( "mp_firingrange", 0 );
}
