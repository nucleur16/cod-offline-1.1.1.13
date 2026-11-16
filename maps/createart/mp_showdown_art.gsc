// PROVENANCE (2026-11-16): the 3.19.26.22 port of mp_showdown ships no createart. Call of Duty 4's maps/createart/mp_showdown_art.gsc
// (CoD4 Steam install, zone/english/mp_showdown.ff rawfile) sets setExpFog(500, 14000, 0.7, 0.6, .4, 0): CoD4's six-argument exponential fog has no
// opacity argument and draws it at full opacity, so it is written as the IW4 MP call with maxOpacity 1 (7 arguments: sun fog
// off), the way maps/createart/mp_dome_art.gsc does.
main()
{
	level.tweakfile = true;

	setDevDvar( "scr_fog_disable", "0" );

	setExpFog( 500, 14000, 0.7, 0.6, 0.4, 1, 0 );
	VisionSetNaked( "mp_showdown", 0 );
}
