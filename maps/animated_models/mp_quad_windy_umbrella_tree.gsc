// PROVENANCE (2026-10-14): CoDOL mp_cha_quad (3.3.3.3 map ported to 1.1.1.13). The map's
// animated_model entities (quad_tree_umbrellathorn_01a_anim) name this script in their precache_script key
// ("maps animated_models mp_quad_windy_umbrella_tree"); 3.3.3.3 ships no source for it, so it follows
// this tree's maps/animated_models/*.gsc (IW4 layout, MP name form: PrecacheMpAnim and
// ScriptModelPlayAnim take the xanim name). mp_quad_windy_umbrella_tree ships in the mp_cha_quad zone and
// its animlists/mp_cha_quad.animlist lists it under =animated_props.
#include common_scripts\utility;

main()
{
	if( !isdefined ( level.anim_prop_models ) )
		level.anim_prop_models = [];

	model = "quad_tree_umbrellathorn_01a_anim";
	level.anim_prop_models[ model ][ "wind" ] = "mp_quad_windy_umbrella_tree";
}
