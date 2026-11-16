// PROVENANCE (2026-10-14): CoDOL mp_seatown (3.3.3.3 map ported to 1.1.1.13). The map's
// animated_model entities (seatown_mid01_wiregrp) name this script in their precache_script key
// ("maps animated_models seatown_mid01_wiregrp_sway"); 3.3.3.3 ships no source for it, so it follows
// this tree's maps/animated_models/*.gsc (IW4 layout, MP name form: PrecacheMpAnim and
// ScriptModelPlayAnim take the xanim name). seatown_mid01_wiregrp_sway ships in the mp_seatown zone and
// its animlists/mp_seatown.animlist lists it under =animated_props.
#include common_scripts\utility;

main()
{
	if( !isdefined ( level.anim_prop_models ) )
		level.anim_prop_models = [];

	model = "seatown_mid01_wiregrp";
	level.anim_prop_models[ model ][ "sway" ] = "seatown_mid01_wiregrp_sway";
}
