// PROVENANCE (2026-10-14): CoDOL mp_mideast (3.3.3.3 map ported to 1.1.1.13). The map's
// animated_model entities (mid_wires_01_anim) name this script in their precache_script key
// ("maps animated_models mid_wires_01_anim"); 3.3.3.3 ships no source for it, so it follows
// this tree's maps/animated_models/*.gsc (IW4 layout, MP name form: PrecacheMpAnim and
// ScriptModelPlayAnim take the xanim name). mid_wires_01_loop ships in the mp_mideast zone and
// its animlists/mp_mideast.animlist lists it under =animated_props.
#include common_scripts\utility;

main()
{
	if( !isdefined ( level.anim_prop_models ) )
		level.anim_prop_models = [];

	model = "mid_wires_01_anim";
	level.anim_prop_models[ model ][ "loop" ] = "mid_wires_01_loop";
}
