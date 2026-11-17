// PROVENANCE (2026-10-14): CoDOL mp_terminal_cls (3.3.3.3 map ported to 1.1.1.13). The fog is
// IW4 maps/createart/mp_terminal_art.gsc's (the same layout); IW5's mp_terminal_cls_fog.gsc is a
// Special Ops survival vision-set fog (start 2219, halfway 13307, opacity 0.53), too light for MP.
// The zone ships vision/mp_terminal_cls.vision.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	// IW4 maps/createart/mp_terminal_art.gsc's fog for the same layout: IW5's survival fog (start 2219, halfway
	// 13307, opacity 0.53) barely showed and left the map's edges in plain view.
	setExpFog( 581.8, 15447.9, 0.425054, 0.467361, 0.489628, 0.579678, 0 );
	VisionSetNaked( "mp_terminal_cls", 0 );
}
