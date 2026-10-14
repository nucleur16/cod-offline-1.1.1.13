// PROVENANCE (2026-10-14): mp_abandon_sh_mk is a 3.3.3.3 map ported to 1.1.1.13 by
// CODOL-AssetExplorer (port-map). Its createart is not in the zone; this is mp_abandon_art.gsc
// (same base map) with the vision set re-pointed at the zone-shipped vision/mp_abandon_sh_mk.vision.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 0, 2465.5, 0.157781, 0.158151, 0.19939, 0.793044, 0, 0.521569, 0.501961, 0.321569, (-0.00219521, 0.000107791, -0.981386), 48.8102, 89.7111, 0 );
	VisionSetNaked( "mp_abandon_sh_mk", 0 );
}
