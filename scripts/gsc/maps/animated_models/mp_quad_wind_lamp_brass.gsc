// PROVENANCE (2026-10-14): CoDOL mp_cha_quad (3.3.3.3 map ported to 1.1.1.13). The map's
// animated_model entities (lantern_iron_off_animated) name this script in their precache_script key
// ("maps animated_models mp_quad_wind_lamp_brass"); 3.3.3.3 ships no source for it, so it follows
// this tree's maps/animated_models/*.gsc (IW4 layout, MP name form: PrecacheMpAnim and
// ScriptModelPlayAnim take the xanim name). mp_quad_wind_lamp_brass ships in the mp_cha_quad zone and
// its animlists/mp_cha_quad.animlist lists it under =animated_props.
#include common_scripts\utility;

main()
{
	if( !isdefined ( level.anim_prop_models ) )
		level.anim_prop_models = [];

	model = "lantern_iron_off_animated";
	level.anim_prop_models[ model ][ "wind" ] = "mp_quad_wind_lamp_brass";
}
